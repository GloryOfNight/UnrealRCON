// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <Containers/Ticker.h>
#include <CoreMinimal.h>
#include <Logging/LogVerbosity.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "RConClient.h"

#include "RConClientSubsystem.generated.h"

using FRConClientConnectedCallback = FRConClient::FHandleAuthorizedDelegate;
using FRConClientDisconnectedCallback = FRConClient::FHandleDisconnectedDelegate;
using FRConClientResponseCallback = FRConClient::FHandleResponseDelegate;

UCLASS()
class RCONCLIENT_API URConClientSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // Called when RConClient connected and authorized (aka ready to accept incoming commands)
    FRConClientConnectedCallback OnConnected{};

    // Called when initial connection failed or closed
    FRConClientDisconnectedCallback OnDisconnected{};

    // Called when received response from RConClient
    FRConClientResponseCallback OnAnyResponse{};

    // @return true subsystem should be be created
    static bool ShouldCreateSubsystem_StaticCheck();

    bool ShouldCreateSubsystem(UObject* Outer) const override;

    void Initialize(FSubsystemCollectionBase& Collection) override;

    void Deinitialize() override;

    // Connect RConClient to remote host using password
    bool Connect(FString HostAddr, uint16 Port, FString Password);

    // Check is RConClient ready to accept commands
    bool IsConnected() const;

    // Disconnect RConClient from remote host
    void Disconnect();

    /*
    * Send execute command to connected host
    * Commands will get enqueued internally if connection is in progress
    * Response (OnResponse) is NOT guaranteed in any way, expect responses with timeout
    *  @return request_id or INDEX_NONE if fail
    */
    int32 SendCommand(FString InCommand);

    void AddRequestCallback(int32 RequestId, FRConClientResponseCallback Callback);

private:
    void OnRConAuthorized();

    void OnRConDisconnected();

    void OnRConResponse(int32 RequestId, const FString& Response);

    void OnConsoleConnect(const TArray<FString>& Args, FOutputDevice& OutputDevice);

    void OnConsoleExec(const TArray<FString>& Args, FOutputDevice& OutputDevice);

    void OnConsoleDisconnect(const TArray<FString>& Args, FOutputDevice& OutputDevice);

    static void WriteToConsole(const FString& Value, ELogVerbosity::Type Verbosity = ELogVerbosity::Log);

    bool Tick(float DeltaTime);

    FRConClient RConClient{};

    FTSTicker::FDelegateHandle TickHandle{};

    TMap<int32, TArray<FRConClientResponseCallback>> RequestResponseCallbacks{};
};