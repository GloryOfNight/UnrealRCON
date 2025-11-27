// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>
#include <SocketSubsystem.h>
#include <Sockets.h>

#include "RConCommon.h"

class FRConClientModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override {};
    virtual void ShutdownModule() override {};
};

class RCONCLIENT_API FRConClient final
{
public:
    FRConClient() = default;
    FRConClient(const FRConClient&) = delete;
    FRConClient(FRConClient&&) = delete;
    ~FRConClient() = default;

    struct FSettings
    {
        FSettings()
            : Host{TEXT("127.0.0.1")}
            , Port{27015}
            , Password{TEXT("1111")}
        {
        }

        FString Host;
        uint16 Port;
        FString Password;
    };

    struct FConnectionState
    {
        bool bConnected : 1;
        bool bAuthSent : 1;
        bool bAuthSucceed : 1;
    };

    // @return true if connect process started
    bool Connect(const FSettings& InSettings);

    bool IsStarted();

    const FConnectionState& GetConnectionState() const;

    // @return when waited
    void WaitConnectBlocked();

    // @return RequestId or INDEX_NONE if failed
    int32 SendCommand(FString InCommand);

    void Disconnect();

    void Tick();

    DECLARE_DELEGATE(FHandleAuthorizedDelegate);
    void AssignAuthorizedCallback(FHandleAuthorizedDelegate InCallback);

    DECLARE_DELEGATE_TwoParams(FHandleResponseDelegate, int32 /*RequestId*/, const FString& /*Response*/);
    void AssignHandleResponseCallback(FHandleResponseDelegate InCallback);

    DECLARE_DELEGATE(FHandleDisconnectedDelegate);
    void AssignDisconnectedCallback(FHandleDisconnectedDelegate InCallback);

private:
    static ISocketSubsystem* GetSocketSubsystem();

    bool SendPacket(const struct FRConPacket& Packet);

    bool CheckConnection();

    void ProcessOutcoming();

    void ProcessIncoming();

    int32 EnqueuePacket(ERConPacketType Type, const FString& Body);

    FHandleAuthorizedDelegate OnAuthorized{};

    FHandleResponseDelegate OnResponse{};

    FHandleDisconnectedDelegate OnDisconnected{};

    FSettings Settings{};

    TSharedPtr<FSocket> Socket{};

    TFuture<bool> FutureConnect{};

    FConnectionState ConnectionState{};

    TQueue<FRConPacket> SendQueue{};
    int32 LastSentId{};
};