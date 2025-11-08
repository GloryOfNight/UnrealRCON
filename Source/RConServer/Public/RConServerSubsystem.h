// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <Containers/Ticker.h>
#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "RConServer.h"

#include "RConServerSubsystem.generated.h"

using FRConServerCommandCallback = FRConServer::FHandleReceivedCommandDelegate;

UCLASS()
class RCONSERVER_API URConServerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    struct FCommandProperties
    {
        // Extra information for 'help <command>'
        FString Help;
    };

    struct FCommandHandle
    {
        FString Command{};
        FRConServer::FHandleReceivedCommandDelegate Callback{};
        // Short description for command displayed after command name in 'help'
        FString Tooltip{};
        FCommandProperties Properties{};
    };

    static URConServerSubsystem* Get(const UObject* Context);

    static uint16 GetRConPort();

    static FString GetRConPassword();

    static uint16 GetRConMaxActiveConnections();

    // @return true subsystem should be created
    static bool ShouldCreateSubsystem_StaticCheck();

    bool ShouldCreateSubsystem(UObject* Outer) const override;

    void Initialize(FSubsystemCollectionBase& Collection) override;

    void Deinitialize() override;

    void StartServer();
    bool IsStarted() const { return RConServer.IsStarted(); };
    void SendCommandResponse(int32 RequestId, const FString& Response);
    void StopServer();

    void AddCommand(FString InCommand, FRConServerCommandCallback InCallback, FString InTooltip = TEXT(""), FCommandProperties InProperties = FCommandProperties());

    void AddCommand(FCommandHandle InCommandHandle);

    FCommandHandle* FindCommandHandle(const FString& Command);

private:
    void TryAutoStart();

    bool TickServer(float DeltaTime);

    void HandleClientConnected();

    void HandleRConCommand(int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse);

    void OnConsoleStartServer(const TArray<FString>& Args, FOutputDevice& OutputDevice);

    void OnConsoleStopServer(const TArray<FString>& Args, FOutputDevice& OutputDevice);

    void OnHelpCommand(int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse);

    void OnExecCommand(int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse);

    void OnPostFork(EForkProcessRole);

    FRConServer RConServer{};

    FTSTicker::FDelegateHandle TickHandle{};

    TMap<FString, FCommandHandle> CommandHandles{};
};