// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "SteamRConServer.h"

IMPLEMENT_MODULE(FSteamRConServerModule, SteamRConServer)

DEFINE_LOG_CATEGORY_STATIC(SteamRConServer, Log, All);

// Note that the repetition in the above table is not an error: SERVERDATA_AUTH_RESPONSE and SERVERDATA_EXECCOMMAND both have a numeric value of 2.
enum
{
    SERVERDATA_RESPONSE_VALUE = 0,
    SERVERDATA_EXECCOMMAND = 2,
    SERVERDATA_AUTH_RESPONSE = 2,
    SERVERDATA_AUTH = 3
};

bool FSteamRConServer::Start(const FSettings& InSettings)
{
    if (bStarted)
    {
        UE_LOG(SteamRConServer, Warning, TEXT("Attempt to start server, while already started"))
        return false;
    }

    auto SocketSubsystem = GetSocketSubsystem();

    FUniqueSocket NewSocket = SocketSubsystem->CreateUniqueSocket(NAME_Stream, TEXT("SteamRConServer"));

    TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
    RemoteAddress->SetAnyAddress();
    RemoteAddress->SetPort(InSettings.Port);

    const bool bBlocking = NewSocket->SetNonBlocking();
    if (!bBlocking)
        UE_LOG(SteamRConServer, Warning, TEXT("Failed SetNonBlocking for listen socket"))

    const bool bNoDelay = NewSocket->SetNoDelay();
    if (!bNoDelay)
        UE_LOG(SteamRConServer, Warning, TEXT("Failed SetNoDelay for listen socket"))

    const bool bReuse = NewSocket->SetReuseAddr(InSettings.bAllowPortReuse);
    if (!bReuse)
        UE_LOG(SteamRConServer, Warning, TEXT("Failed SetReuseAddr for listen socket"))

    BoundPort = SocketSubsystem->BindNextPort(NewSocket.Get(), *RemoteAddress, 10, 1);
    if (BoundPort == 0)
    {
        UE_LOG(SteamRConServer, Error, TEXT("Failed bind to address: %s"), *RemoteAddress->ToString(true))
        return false;
    }

    const bool bListen = NewSocket->Listen(1);
    if (!bListen)
    {
        UE_LOG(SteamRConServer, Error, TEXT("Failed start listen with bind address: %s"), *RemoteAddress->ToString(true))
        return false;
    }

    Settings = InSettings;
    ListenSocket = MoveTemp(NewSocket);
    bStarted = true;

    UE_LOG(SteamRConServer, Display, TEXT("RCon started listening on port %d (requested: %d)"), BoundPort, Settings.Port);

    return true;
}

void FSteamRConServer::Tick()
{
    if (UNLIKELY(!ListenSocket))
        return;

    bool bPending{};
    if (ListenSocket->HasPendingConnection(bPending) && bPending)
    {
        auto NewClientSocket = FUniqueSocket(ListenSocket->Accept(TEXT("SteamRConClient")));
        auto IncomingAddr = GetSocketSubsystem()->CreateInternetAddr();
        NewClientSocket->GetPeerAddress(*IncomingAddr);

        if (ClientSocket && bClientAuth)
        {
            NewClientSocket.Reset();
            UE_LOG(SteamRConServer, Warning, TEXT("Refusing connection for \'%s\' since other client already connected"), *IncomingAddr->ToString(true));
        }
        else
        {
            bClientAuth = false;
            ClientSocket = MoveTemp(NewClientSocket);
            UE_LOG(SteamRConServer, Display, TEXT("Accepting new client connection from \'%s\'"), *IncomingAddr->ToString(true));

            const bool bBlocking = ClientSocket->SetNonBlocking();
            if (!bBlocking)
                UE_LOG(SteamRConServer, Warning, TEXT("Failed SetNonBlocking for client socket"))

            const bool bNoDelay = ClientSocket->SetNoDelay();
            if (!bNoDelay)
                UE_LOG(SteamRConServer, Warning, TEXT("Failed SetNoDelay for client socket"))
        }
    }

    if (ClientSocket)
    {
        ProcessClientConnection();

        const double Now = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());

        const double TimeSinceLastHeartbeat = Now - FPlatformTime::ToMilliseconds64(LastHeartbeatTime);
        if (static_cast<int32>(TimeSinceLastHeartbeat) > Settings.HeartbeatTimeMs)
        {
            int32 BytesSent{};
            ClientSocket->Send(nullptr, 0, BytesSent);
            LastHeartbeatTime = Now;
        }

        const double TimeSinceLastRecvMs = Now - FPlatformTime::ToMilliseconds64(LastRecvTime);
        if (static_cast<int32>(TimeSinceLastRecvMs) > Settings.ConnectionTimeoutMs)
        {
            UE_LOG(SteamRConServer, Display, TEXT("RCON client connection timeout"));
            ResetClientConnection();
        }
    }
}

void FSteamRConServer::Stop()
{
    ListenSocket.Reset();
    ResetClientConnection();
    bStarted = false;
}

void FSteamRConServer::AssignCommandCallback(FHandleReceivedCommandDelegate InCallback)
{
    Callback = InCallback;
}

ISocketSubsystem* FSteamRConServer::GetSocketSubsystem()
{
    return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}

void FSteamRConServer::ProcessClientConnection()
{
    if (UNLIKELY(!ClientSocket))
        return;

    uint8 Buffer[4096];
    int32 BytesRead = 0;

    while (ClientSocket->Recv(Buffer, sizeof(Buffer), BytesRead, ESocketReceiveFlags::None))
    {
        LastRecvTime = FPlatformTime::Cycles64();

        if (BytesRead < 12)
            return;

        int32 PacketSize, RequestId, Type;
        FMemory::Memcpy(&PacketSize, Buffer, 4);
        FMemory::Memcpy(&RequestId, Buffer + 4, 4);
        FMemory::Memcpy(&Type, Buffer + 8, 4);

        const FString Payload = FString(UTF8_TO_TCHAR(reinterpret_cast<char*>(Buffer + 12)));

        if (Type == SERVERDATA_AUTH)
        {
            const bool bAuthSuccess = Settings.Password.Equals(Payload, ESearchCase::CaseSensitive);
            const int32 RespId = bAuthSuccess ? RequestId : -1;
            SendClientResponse(RespId, SERVERDATA_AUTH_RESPONSE, FString());

            if (bAuthSuccess)
            {
                UE_LOG(SteamRConServer, Display, TEXT("RCON client authenticated"));
                bClientAuth = true;
            }
            else
            {
                UE_LOG(SteamRConServer, Warning, TEXT("RCON client auth failed"));
                ResetClientConnection();
            }
        }
        else if (Type == SERVERDATA_EXECCOMMAND)
        {
            if (!bClientAuth)
            {
                UE_LOG(SteamRConServer, Warning, TEXT("Attempt to execute command without auth"));
                ClientSocket.Reset();
                return;
            }

            UE_LOG(SteamRConServer, Display, TEXT("RCON client command received: %s"), *Payload);

            FString Output;
            if (Callback.IsBound())
                Output = Callback.Execute(Payload);
            else
                Output = FString::Printf(TEXT("Failed to execute: %s (no command callback is bound)"), *Payload);

            SendClientResponse(RequestId, SERVERDATA_RESPONSE_VALUE, Output);
        }
    }
}

void FSteamRConServer::SendClientResponse(const int32 RespId, const int32 RespType, const FString& Payload)
{
    FTCHARToUTF8 Utf8(*Payload);
    const int32 RespSize = 4 + 4 + Utf8.Length() + 2;

    TArray<uint8> Response;
    Response.Reserve(4 + RespSize);

#if PLATFORM_LITTLE_ENDIAN
    Response.Append((uint8*)&RespSize, 4);
    Response.Append((uint8*)&RespId, 4);
    Response.Append((uint8*)&RespType, 4);
#else
    const int32 LeRespSize = NETWORK_ORDER32(RespSize);
    const int32 LeRespId = NETWORK_ORDER32(RespId);
    const int32 LeRespType = NETWORK_ORDER32(RespType);
    Response.Append((uint8*)&LeRespSize, 4);
    Response.Append((uint8*)&LeRespId, 4);
    Response.Append((uint8*)&LeRespType, 4);
#endif

    Response.Append((uint8*)Utf8.Get(), Utf8.Length());
    Response.Add(0);
    Response.Add(0);

    int32 BytesSent{};
    const bool bSend = ClientSocket->Send(Response.GetData(), Response.Num(), BytesSent);
    if (!bSend)
        UE_LOG(SteamRConServer, Error, TEXT("Failed to send response, error code %i"), static_cast<int32>(GetSocketSubsystem()->GetLastErrorCode()));
}

void FSteamRConServer::ResetClientConnection()
{
    bClientAuth = false;
    ClientSocket.Reset();
}
