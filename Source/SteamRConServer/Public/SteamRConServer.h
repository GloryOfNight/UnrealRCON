// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>
#include <SocketSubsystem.h>
#include <Sockets.h>

class FSteamRConServerModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override {};
    virtual void ShutdownModule() override {};
};

class STEAMRCONSERVER_API FSteamRConServer
{
public:
    DECLARE_DELEGATE_RetVal_OneParam(FString, FHandleReceivedCommandDelegate, const FString&);

    struct FSettings
    {
        FSettings()
            : Password{TEXT("default")}
            , ConnectionTimeoutMs{5000}
            , HeartbeatTimeMs{500}
            , Port{8000}
            , bAllowPortReuse{false}
        {
        }

        FString Password;
        int32 ConnectionTimeoutMs;
        int32 HeartbeatTimeMs;
        uint16 Port;
        bool bAllowPortReuse;
    };

    bool Start(const FSettings& InSettings = FSettings());
    void Tick();
    void Stop();

    void AssignCommandCallback(FHandleReceivedCommandDelegate InCallback);

private:
    static ISocketSubsystem* GetSocketSubsystem();

    void ProcessClientConnection();
    void SendClientResponse(const int32 RespId, const int32 RespType, const FString& Payload);

    void ResetClientConnection();

    FSettings Settings{};

    FUniqueSocket ListenSocket{};
    FUniqueSocket ClientSocket{};

    FHandleReceivedCommandDelegate Callback{};

    int64 LastRecvTime{};

    int64 LastHeartbeatTime{};

    bool bClientAuth{};
};
