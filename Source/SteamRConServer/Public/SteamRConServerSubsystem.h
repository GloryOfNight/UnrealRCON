// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <Containers/Ticker.h>
#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "SteamRConServer.h"

#include "SteamRConServerSubsystem.generated.h"

using FSteamRConServerCommandCallback = FSteamRConServer::FHandleReceivedCommandDelegate;

UCLASS()
class STEAMRCONSERVER_API USteamRConServerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    struct FCommandHandle
    {
        FString Tooltip{};
        FSteamRConServer::FHandleReceivedCommandDelegate Callback{};
    };

    static USteamRConServerSubsystem* Get(const UObject* Context);

    static uint16 GetRConPort();

    static FString GetRConPassword();

    bool ShouldCreateSubsystem(UObject* Outer) const override;

    void Initialize(FSubsystemCollectionBase& Collection) override;

    void Deinitialize() override;
    
    void StartServer();
    bool IsStarted() const { return RConServer.IsStarted(); };
    void StopServer();

    // A command is single word string without spaces. Everything after space in a command is considered a agrument and ignored
    void AddCommandCallback(FString Command, FSteamRConServerCommandCallback InCallback, const FString& InTooltip);

private:
    bool TickServer(float DeltaTime);

    FString HandleRConCommand(const FString& Command);

    FString OnHelpCommand(const FString& Command);

    FString OnExecCommand(const FString& Command);

    void OnPostFork(EForkProcessRole);

    FSteamRConServer RConServer{};

    FTSTicker::FDelegateHandle TickHandle{};

    TMap<FString, FCommandHandle> CommandHandles{};
};