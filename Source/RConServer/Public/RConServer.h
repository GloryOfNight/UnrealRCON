// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>
#include <SocketSubsystem.h>
#include <Sockets.h>

#include "RConCommon.h"

class FRConServerModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override {};
    virtual void ShutdownModule() override {};
};

class RCONSERVER_API FRConServer final
{
public:
    FRConServer() = default;
    FRConServer(const FRConServer&) = delete;
    FRConServer(FRConServer&&) = delete;
    ~FRConServer() = default;

    DECLARE_DELEGATE(FHandleClientConnectedDelegate);
    DECLARE_DELEGATE_FourParams(FHandleReceivedCommandDelegate, int32 /*RequestId*/, const FString& /*Command*/, FString& /*Response*/, bool& /*bDelayResponse*/);

    struct FSettings
    {
        FSettings()
            : Password{TEXT("1111")}
            , Port{27015}
            , bAllowPortReuse{false}
        {
        }

        FString Password;
        uint16 Port;
        bool bAllowPortReuse;

        uint16 MaxActiveConnections{3};
    };

    struct FClientConnection
    {
        uint32 Id;
        TSharedPtr<FSocket> Socket;
        TQueue<FRConPacket> SendQueue;
        bool bAuthorized;
        // map local requests ids to received ones, needed to avoid collision in received Ids
        TArray<TPair<int32, int32>> RequestIdMapping;
    };

    bool Start(const FSettings& InSettings = FSettings());
    void Tick();
    void Stop();

    void AssignClientConnectedCallback(FHandleClientConnectedDelegate InCallback);

    void AssignCommandCallback(FHandleReceivedCommandDelegate InCallback);

    int32 GetBoundPort() const { return ListenSocket ? BoundPort : -1; };

    bool IsStarted() const { return bStarted; }

    void SendResponse(const int32 RequestId, const FString& Response);

private:
    static ISocketSubsystem* GetSocketSubsystem();

    void ProcessNewConnections();
    void TryPurgeOldConnections();

    bool CheckConnection(FClientConnection& Connection);
    void ProcessIncoming(FClientConnection& Connection);
    void ProcessOutcoming(FClientConnection& Connection);
    void CloseConnection(FClientConnection& Connection);

    void EnqueueResponse(FClientConnection& Connection, int32 RequestId, ERConPacketType Type, const FString& Payload);

    bool SendPacket(TSharedPtr<FSocket>& Socket, const struct FRConPacket& Packet);

    FSettings Settings{};

    FUniqueSocket ListenSocket{};
    int32 BoundPort{};

    FHandleClientConnectedDelegate ClientConnectedCallback{};

    FHandleReceivedCommandDelegate ExecCommandCallback{};

    uint32 LastId{};
    uint32 LastRequestId{};
    uint16 ActiveConnections{};

    TArray<TUniquePtr<FClientConnection>> ClientConnections{};

    bool bStarted{};
};
