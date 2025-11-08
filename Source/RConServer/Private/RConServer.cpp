// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConServer.h"

IMPLEMENT_MODULE(FRConServerModule, RConServer)

DEFINE_LOG_CATEGORY_STATIC(RConServer, Log, Log);

bool FRConServer::Start(const FSettings& InSettings)
{
    // prevent ability to start in game shipping build unless allowed
    if (UE_GAME && UE_BUILD_SHIPPING && RCON_SERVER_ALLOW_IN_GAME_SHIPPING == 0)
        return false;

    if (bStarted)
    {
        UE_LOG(RConServer, Warning, TEXT("Attempt to start server, while already started"))
        return false;
    }

    auto SocketSubsystem = GetSocketSubsystem();

    FUniqueSocket NewSocket = SocketSubsystem->CreateUniqueSocket(NAME_Stream, TEXT("RConServer"));

    TSharedRef<FInternetAddr> SocketAddress = SocketSubsystem->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
    SocketAddress->SetAnyAddress();
    SocketAddress->SetPort(InSettings.Port);

    const bool bBlocking = NewSocket->SetNonBlocking();
    if (!bBlocking)
        UE_LOG(RConServer, Warning, TEXT("Failed SetNonBlocking for listen socket"))

    const bool bReuse = NewSocket->SetReuseAddr(InSettings.bAllowPortReuse);
    if (!bReuse)
        UE_LOG(RConServer, Warning, TEXT("Failed SetReuseAddr for listen socket"))

    BoundPort = SocketSubsystem->BindNextPort(NewSocket.Get(), *SocketAddress, 10, 1);
    if (BoundPort == 0)
    {
        UE_LOG(RConServer, Error, TEXT("Failed bind to address: %s"), *SocketAddress->ToString(true))
        return false;
    }

    const bool bListen = NewSocket->Listen(3);
    if (!bListen)
    {
        UE_LOG(RConServer, Error, TEXT("Failed start listen with bind address: %s"), *SocketAddress->ToString(true))
        return false;
    }

    Settings = InSettings;
    ListenSocket = MoveTemp(NewSocket);
    bStarted = true;

    ListenSocket->GetAddress(*SocketAddress);
    UE_LOG(RConServer, Log, TEXT("RCon started using %s port (requested port: %d)"), *SocketAddress->ToString(true), Settings.Port);

    return true;
}

void FRConServer::Tick()
{
    if (!ListenSocket)
        return;

    if (!ActiveConnections && ClientConnections.Num())
        TryPurgeOldConnections();

    ProcessNewConnections();

    for (auto& Connection : ClientConnections)
    {
        if (!Connection->Socket)
            continue;

        if (!CheckConnection(*Connection))
        {
            UE_LOG(RConServer, Log, TEXT("Client %d disconnected"), Connection->Id);
            CloseConnection(*Connection);
            continue;
        }

        ProcessIncoming(*Connection);
        ProcessOutcoming(*Connection);
    }
}

void FRConServer::Stop()
{
    UE_LOG(RConServer, Verbose, TEXT("RCon stop invoked. Is started: %d"), bStarted);

    ListenSocket.Reset();

    for (auto& Connection : ClientConnections)
    {
        CloseConnection(*Connection);
    }
    ClientConnections.Reset();

    bStarted = false;
}

void FRConServer::AssignClientConnectedCallback(FHandleClientConnectedDelegate InCallback)
{
    ClientConnectedCallback = InCallback;
}

void FRConServer::AssignCommandCallback(FHandleReceivedCommandDelegate InCallback)
{
    ExecCommandCallback = InCallback;
}

void FRConServer::SendResponse(const int32 RequestId, const FString& Response)
{
    for (auto& Connection : ClientConnections)
        for (const auto [LocalRequestId, RealRequestId] : Connection->RequestIdMapping)
            if (LocalRequestId == RequestId)
                EnqueueResponse(*Connection, RealRequestId, ERConPacketType::ResponseValue, Response);
}

ISocketSubsystem* FRConServer::GetSocketSubsystem()
{
    return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}

bool FRConServer::CheckConnection(FClientConnection& Connection)
{
    if (!Connection.Socket)
        return false;

    uint32 PendingDataSize{};
    const bool bHasPendingData = Connection.Socket->HasPendingData(PendingDataSize);

    int32 BytesRead{};
    const bool bRecvOk = Connection.Socket->Recv(nullptr, 0, BytesRead);

    return bHasPendingData || bRecvOk;
}

void FRConServer::ProcessNewConnections()
{
    bool bPending{};
    if (ListenSocket->HasPendingConnection(bPending) && bPending)
    {
        TSharedPtr<FSocket> NewClientSocket = TSharedPtr<FSocket>(ListenSocket->Accept(TEXT("RConClient")));
        if (!NewClientSocket.IsValid())
            return;

        auto IncomingAddr = GetSocketSubsystem()->CreateInternetAddr();
        NewClientSocket->GetPeerAddress(*IncomingAddr);

        if (ActiveConnections >= Settings.MaxActiveConnections)
        {
            NewClientSocket->Shutdown(ESocketShutdownMode::ReadWrite);
            NewClientSocket->Close();

            UE_LOG(RConServer, Warning, TEXT("Refusing \'%s\' connection, max active connection limit of %d reached"), *IncomingAddr->ToString(true), Settings.MaxActiveConnections)
            return;
        }

        const bool bBlocking = NewClientSocket->SetNonBlocking();
        if (!bBlocking)
            UE_LOG(RConServer, Warning, TEXT("Failed SetNonBlocking for client socket"))

        auto& NewConnection = ClientConnections.Emplace_GetRef(MakeUnique<FClientConnection>());
        NewConnection->Id = ++LastId;
        NewConnection->Socket = MoveTemp(NewClientSocket);

        ActiveConnections++;

        UE_LOG(RConServer, Log, TEXT("Accepting new client connection from \'%s\'. Assigned id: %d. Connection: %d out of %d"), *IncomingAddr->ToString(true), LastId, ActiveConnections, Settings.MaxActiveConnections);
    }
}

void FRConServer::TryPurgeOldConnections()
{
    for (int32 i = ClientConnections.Num() - 1; i > -1; --i)
    {
        const auto& Connection = ClientConnections[i];
        if (!Connection->Socket.IsValid())
        {
            UE_LOG(RConServer, Verbose, TEXT("Client %d connection purged"), Connection->Id);
            ClientConnections.RemoveAt(i, EAllowShrinking::No);
        }
    }
}

void FRConServer::ProcessIncoming(FClientConnection& Connection)
{
    TArray<uint8> RecvBuffer{};

    uint32 PendingDataSize{};
    while (Connection.Socket && Connection.Socket->HasPendingData(PendingDataSize))
    {
        RecvBuffer.SetNumUninitialized(PendingDataSize);

        int32 BytesRead{};
        const bool bRecvOk = Connection.Socket->Recv(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead);
        if (!bRecvOk)
            return;

        if (BytesRead < CRConBasePacketSize)
            return;

        const auto [bPacketOk, Packet] = FRConPacket::DeserializePacket(RecvBuffer.GetData(), RecvBuffer.Num());
        if (!bPacketOk)
            continue;

        ++LastRequestId;

        if (Packet.Type == ERConPacketType::Auth)
        {
            const bool bAuthSuccess = Settings.Password.Equals(Packet.Body, ESearchCase::CaseSensitive);

            const int32 AuthId = bAuthSuccess ? Packet.Id : -1;
            EnqueueResponse(Connection, AuthId, ERConPacketType::AuthResponse, FString());

            if (bAuthSuccess)
            {
                UE_LOG(RConServer, Log, TEXT("Client %d authenticated"), Connection.Id);
                Connection.bAuthorized = true;
                ClientConnectedCallback.ExecuteIfBound();
            }
            else
            {
                UE_LOG(RConServer, Warning, TEXT("Client %d authentication failure"), Connection.Id);
                CloseConnection(Connection);
            }
        }
        else if (Packet.Type == ERConPacketType::ExecCommand)
        {
            if (!Connection.Socket.IsValid())
            {
                UE_LOG(RConServer, Warning, TEXT("Client %d attempt to execute command without authentication"), Connection.Id);
                continue;
            }

            UE_LOG(RConServer, Log, TEXT("Client %d received command: %s"), Connection.Id, *Packet.Body);

            bool bDelayResponse{};
            FString Response{};

            if (!ExecCommandCallback.ExecuteIfBound(LastRequestId, Packet.Body, Response, bDelayResponse))
            {
                Response = FString::Printf(TEXT("Failed to execute: %s (no command callback is bound)"), *Packet.Body);
            }

            if (bDelayResponse)
            {
                // map only delayed responses, since it will require figure out real packet id
                Connection.RequestIdMapping.Emplace(TPair<int32, int32>(LastRequestId, Packet.Id));
            }
            else
            {
                EnqueueResponse(Connection, Packet.Id, ERConPacketType::ResponseValue, Response);
            }
        }
    }
}

void FRConServer::ProcessOutcoming(FClientConnection& Connection)
{
    if (!Connection.Socket)
        return;

    while (!Connection.SendQueue.IsEmpty())
    {
        FRConPacket Packet;
        Connection.SendQueue.Peek(Packet);
        if (SendPacket(Connection.Socket, Packet))
        {
            Connection.SendQueue.Pop();
        }
        else
        {
            break;
        }
    }
}

bool FRConServer::SendPacket(TSharedPtr<FSocket>& Socket, const FRConPacket& Packet)
{
    if (!Socket)
        return false;

    const TArray<uint8> Data = Packet.Serialize();
    int32 BytesSent{};

    const bool bSendOk = Socket->Send(Data.GetData(), Data.Num(), BytesSent);

    UE_CLOG(bSendOk, RConServer, Verbose, TEXT("Send request: Type %d, Id %d, Body %s"), (int32)Packet.Type, Packet.Id, *Packet.Body);
    UE_CLOG(!bSendOk, RConServer, Error, TEXT("Failed to send response, error code %i"), static_cast<int32>(GetSocketSubsystem()->GetLastErrorCode()));

    return bSendOk;
}

void FRConServer::CloseConnection(FClientConnection& Connection)
{
    if (Connection.Socket.IsValid())
    {
        Connection.Socket->Shutdown(ESocketShutdownMode::ReadWrite);
        Connection.Socket->Close();
        Connection.Socket.Reset();

        --ActiveConnections;
    }
}

void FRConServer::EnqueueResponse(FClientConnection& Connection, int32 RequestId, ERConPacketType Type, const FString& Payload)
{
    // clang-format off
    FRConPacket Packet
    {
        .Id = RequestId,
        .Type = Type,
        .Body = Payload
    };
    // clang-format on
    Connection.SendQueue.Enqueue(MoveTemp(Packet));
}