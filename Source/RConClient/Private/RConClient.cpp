// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConClient.h"

IMPLEMENT_MODULE(FRConClientModule, RConClient)

DEFINE_LOG_CATEGORY_STATIC(RConClient, Log, Log);

bool FRConClient::Connect(const FSettings& InSettings)
{
    if (Socket)
    {
        UE_LOG(RConClient, Warning, TEXT("New connect invoked while being connected, huh? Disconnect client first"));
        return false;
    }

    ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

    TSharedPtr<FInternetAddr> RemoteAddr = SocketSubsystem->CreateInternetAddr();

    const auto AddressInfoResult = SocketSubsystem->GetAddressInfo(*InSettings.Host, nullptr, EAddressInfoFlags::Default, NAME_None);
    if (AddressInfoResult.ReturnCode == SE_NO_ERROR && AddressInfoResult.Results.Num() > 0)
    {
        RemoteAddr->SetRawIp(AddressInfoResult.Results[0].Address->GetRawIp());
    }
    RemoteAddr->SetPort(InSettings.Port);

    if (!RemoteAddr->IsValid())
    {
        UE_LOG(RConClient, Error, TEXT("Failed to connect, invalid (or failed to resolve) remote address %s:%d"), *InSettings.Host, InSettings.Port);
        return false;
    }

    TSharedPtr<FSocket> NewSocket = TSharedPtr<FSocket>(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("RConClient"), RemoteAddr->GetProtocolType()));

    if (UNLIKELY(!NewSocket->SetNonBlocking()))
    {
        UE_LOG(RConClient, Error, TEXT("Failed to set socket as non-blocking"));
        return false;
    }

    TSharedPtr<FInternetAddr> BindAddr = SocketSubsystem->CreateInternetAddr();
    BindAddr->SetAnyAddress();
    BindAddr->SetPort(0);

    if (UNLIKELY(!NewSocket->Bind(*BindAddr)))
    {
        UE_LOG(RConClient, Error, TEXT("Failed properly bind socket"));
        return false;
    }

    TFunction<bool()> Task = [NewSocket, RemoteAddr]() -> bool
    {
        return NewSocket->Connect(*RemoteAddr);
    };

    FutureConnect.Reset();
    FutureConnect = AsyncThread(Task);

    const bool bConnectStarted = FutureConnect.IsValid();
    if (bConnectStarted)
    {
        Settings = InSettings;
        Socket = NewSocket;

        ConnectionState = FConnectionState();
        SendQueue.Empty();

        UE_LOG(RConClient, Log, TEXT("Connection began"));
    }
    else
    {
        UE_LOG(RConClient, Error, TEXT("Failed to start connect task"));
    }

    return bConnectStarted;
}

void FRConClient::WaitConnectBlocked()
{
    if (FutureConnect.IsValid())
    {
        FutureConnect.Wait();
    }
}

bool FRConClient::IsStarted()
{
    return Socket.IsValid();
}

const FRConClient::FConnectionState& FRConClient::GetConnectionState() const
{
    return ConnectionState;
}

int32 FRConClient::SendCommand(FString InCommand)
{
    if (!Socket)
    {
        UE_LOG(RConClient, Warning, TEXT("Cannot send command \'%s\', not initialized"), *InCommand);
        return INDEX_NONE;
    }

    if (!ConnectionState.bAuthSucceed)
    {
        UE_LOG(RConClient, Warning, TEXT("Cannot send command \'%s\', not yet authorized"), *InCommand);
        return INDEX_NONE;
    }

    return EnqueuePacket(ERConPacketType::ExecCommand, MoveTemp(InCommand));
}

void FRConClient::Disconnect()
{
    if (Socket)
    {
        Socket->Shutdown(ESocketShutdownMode::ReadWrite);
        Socket->Close();

        Socket.Reset();

        UE_LOG(RConClient, Log, TEXT("Socket closed"));

        OnDisconnected.ExecuteIfBound();
    }
}

void FRConClient::Tick()
{
    if (!Socket)
        return;

    if (FutureConnect.IsValid() && FutureConnect.IsReady())
    {
        ConnectionState.bConnected = FutureConnect.Consume();
        if (!ConnectionState.bConnected)
        {
            UE_LOG(RConClient, Log, TEXT("Failed to establish connection with a remote"))
            Disconnect();
            return;
        }
    }

    if (Socket->GetConnectionState() == SCS_NotConnected)
        return;

    if (Socket->GetConnectionState() == SCS_ConnectionError)
    {
        UE_LOG(RConClient, Log, TEXT("Socket encountered connection error"))
        Disconnect();
        return;
    }

    if (!CheckConnection())
    {
        UE_LOG(RConClient, Log, TEXT("Connection no longer active"))
        Disconnect();
        return;
    }

    if (!ConnectionState.bAuthSent)
    {
        EnqueuePacket(ERConPacketType::Auth, Settings.Password);
        ConnectionState.bAuthSent = true;
        UE_LOG(RConClient, Log, TEXT("Waiting auth request to be processed"))
    }

    ProcessOutcoming();
    ProcessIncoming();
}

void FRConClient::AssignDisconnectedCallback(FHandleDisconnectedDelegate InCallback)
{
    OnDisconnected = InCallback;
}

void FRConClient::AssignAuthorizedCallback(FHandleAuthorizedDelegate InCallback)
{
    OnAuthorized = InCallback;
}

void FRConClient::AssignHandleResponseCallback(FHandleResponseDelegate InCallback)
{
    OnResponse = InCallback;
}

ISocketSubsystem* FRConClient::GetSocketSubsystem()
{
    return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}

bool FRConClient::SendPacket(const FRConPacket& Packet)
{
    if (!Socket)
        return false;

    const TArray<uint8> Data = Packet.Serialize();
    int32 BytesSent{};

    const bool bSendOk = Socket->Send(Data.GetData(), Data.Num(), BytesSent);

    UE_CLOG(bSendOk, RConClient, VeryVerbose, TEXT("Send request: Id: %d, Type: %d, Body: %s"), Packet.Id, (int32)Packet.Type, *Packet.Body);
    UE_CLOG(!bSendOk, RConClient, Error, TEXT("Failed to send response, error code %i"), static_cast<int32>(GetSocketSubsystem()->GetLastErrorCode()));

    return bSendOk;
}
bool FRConClient::CheckConnection()
{
    if (!Socket)
        return false;

    uint32 PendingDataSize{};
    const bool bHasPendingData = Socket->HasPendingData(PendingDataSize);

    int32 BytesRead{};
    const bool bRecvOk = Socket->Recv(nullptr, 0, BytesRead);

    return bHasPendingData || bRecvOk;
}

void FRConClient::ProcessOutcoming()
{
    while (!SendQueue.IsEmpty())
    {
        FRConPacket Packet;
        SendQueue.Peek(Packet);
        if (SendPacket(Packet))
        {
            SendQueue.Pop();
        }
        else
        {
            break;
        }
    }
}

void FRConClient::ProcessIncoming()
{
    TArray<uint8> RecvBuffer{};

    uint32 PendingDataSize{};
    while (Socket && Socket->HasPendingData(PendingDataSize))
    {
        RecvBuffer.SetNumUninitialized(PendingDataSize);

        int32 BytesRead{};
        const bool bRecvOk = Socket->Recv(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead);
        if (!bRecvOk)
            return;

        if (BytesRead < CRConBasePacketSize)
            return;

        const auto [bPacketOk, Packet] = FRConPacket::DeserializePacket(RecvBuffer.GetData(), RecvBuffer.Num());
        if (!bPacketOk)
            continue;

        if (Packet.Type == ERConPacketType::AuthResponse)
        {
            if (Packet.Id > 0)
            {
                ConnectionState.bAuthSucceed = true;
                UE_LOG(RConClient, Log, TEXT("Auth success"));

                OnAuthorized.ExecuteIfBound();
            }
            else
            {
                Disconnect();
                UE_LOG(RConClient, Error, TEXT("Auth failed"));
            }
        }
        else if (Packet.Type == ERConPacketType::ResponseValue)
        {
            UE_LOG(RConClient, Log, TEXT("Exec response: Id: %d, Body:\n%s"), Packet.Id, *Packet.Body);

            OnResponse.ExecuteIfBound(Packet.Id, Packet.Body);
        }
    }
}

int32 FRConClient::EnqueuePacket(ERConPacketType Type, const FString& Body)
{
    // clang-format off
    FRConPacket Packet
    {
        .Id = ++LastSentId,
        .Type = Type,
        .Body = Body
    };
    // clang-format on
    SendQueue.Enqueue(MoveTemp(Packet));
    return LastSentId;
}
