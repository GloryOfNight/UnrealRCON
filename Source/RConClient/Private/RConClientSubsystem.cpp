// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConClientSubsystem.h"

#include <Engine/Console.h>
#include <EngineUtils.h>

#include "RConClientSettings.h"

DEFINE_LOG_CATEGORY_STATIC(RConClientSubsystem, Log, Log);
#define STRINGIFY(Name) #Name

bool URConClientSubsystem::ShouldCreateSubsystem_StaticCheck()
{
    bool bAllowCreate = false;
    if (WITH_EDITOR)
        bAllowCreate = URConClientSettings::Get()->bAllowInEditorBuild;
    else if (UE_GAME && UE_BUILD_SHIPPING)
        bAllowCreate = URConClientSettings::Get()->bAllowInGameBuild && URConClientSettings::Get()->bAllowInGameShippingBuild;
    else if (UE_GAME)
        bAllowCreate = URConClientSettings::Get()->bAllowInGameBuild;
    return bAllowCreate;
}

bool URConClientSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return ShouldCreateSubsystem_StaticCheck();
}

void URConClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    IConsoleManager& ConsoleManager = IConsoleManager::Get();
    ConsoleManager.RegisterConsoleCommand(TEXT("rcon.client.connect"), TEXT("<hostname> <port> <password>. Invoke connect to rcon server"), FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateUObject(this, &URConClientSubsystem::OnConsoleConnect));
    ConsoleManager.RegisterConsoleCommand(TEXT("rcon.client.exec"), TEXT("<command>. Send command to rcon server"), FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateUObject(this, &URConClientSubsystem::OnConsoleExec));
    ConsoleManager.RegisterConsoleCommand(TEXT("rcon.client.disconnect"), TEXT("Disconnect from rcon server"), FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateUObject(this, &URConClientSubsystem::OnConsoleDisconnect));

    const FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &URConClientSubsystem::Tick);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate);

    RConClient.AssignAuthorizedCallback(FRConClient::FHandleAuthorizedDelegate::CreateUObject(this, &URConClientSubsystem::OnRConAuthorized));
    RConClient.AssignDisconnectedCallback(FRConClient::FHandleAuthorizedDelegate::CreateUObject(this, &URConClientSubsystem::OnRConDisconnected));
    RConClient.AssignHandleResponseCallback(FRConClient::FHandleResponseDelegate::CreateUObject(this, &URConClientSubsystem::OnRConResponse));
}

void URConClientSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

bool URConClientSubsystem::Connect(FString HostAddr, uint16 Port, FString Password)
{
    FRConClient::FSettings RConSettings{};
    RConSettings.Host = HostAddr;
    RConSettings.Port = Port;
    RConSettings.Password = Password;

    return RConClient.Connect(RConSettings);
}

bool URConClientSubsystem::IsConnected() const
{
    const auto& ConnState = RConClient.GetConnectionState();
    return ConnState.bAuthSucceed;
}

void URConClientSubsystem::Disconnect()
{
    RConClient.Disconnect();
}

int32 URConClientSubsystem::SendCommand(FString InCommand)
{
    return RConClient.SendCommand(MoveTemp(InCommand));
}

void URConClientSubsystem::AddRequestCallback(int32 RequestId, FRConClientResponseCallback Callback)
{
    auto& CallbacksList = RequestResponseCallbacks.FindOrAdd(RequestId);
    CallbacksList.Emplace(Callback);
}

void URConClientSubsystem::OnRConAuthorized()
{
    WriteToConsole(TEXT("Connected"));
    OnConnected.ExecuteIfBound();
}

void URConClientSubsystem::OnRConDisconnected()
{
    WriteToConsole(TEXT("Connection lost"));
    OnDisconnected.ExecuteIfBound();
}

void URConClientSubsystem::OnRConResponse(int32 RequestId, const FString& Response)
{
    auto CallbacksList = RequestResponseCallbacks.Find(RequestId);
    if (CallbacksList)
    {
        for (auto& Callback : *CallbacksList)
        {
            Callback.ExecuteIfBound(RequestId, Response);
        }
        RequestResponseCallbacks.Remove(RequestId);
    }

    OnAnyResponse.ExecuteIfBound(RequestId, Response);
}

bool URConClientSubsystem::Tick(float DeltaTime)
{
    RConClient.Tick();
    return true;
}

void URConClientSubsystem::OnConsoleConnect(const TArray<FString>& Args, FOutputDevice& OutputDevice)
{
    const auto* Settings = URConClientSettings::Get();

    FRConClient::FSettings RConSettings{};
    RConSettings.Host = Settings->DefaultHost;
    RConSettings.Port = Settings->DefaultPort;
    RConSettings.Password = Settings->DefaultPassword;

    if (Args.IsValidIndex(0))
        RConSettings.Host = Args[0];
    if (Args.IsValidIndex(1))
        RConSettings.Port = FCString::Atoi(*Args[1]);
    if (Args.IsValidIndex(2))
        RConSettings.Password = Args[2];

    if (!RConClient.Connect(RConSettings))
    {
        WriteToConsole(TEXT("Failed to invoke connect"), ELogVerbosity::Error);
        return;
    }

    WriteToConsole(TEXT("Begin connecting"));
}

void URConClientSubsystem::OnConsoleExec(const TArray<FString>& Args, FOutputDevice& OutputDevice)
{
    if (Args.Num() == 0)
    {
        WriteToConsole(TEXT("Expected <command> argument"), ELogVerbosity::Error);
        return;
    }

    FString FullCommand{};
    for (int32 i = 0; i < Args.Num(); ++i)
    {
        FullCommand.Append(Args[i]);
        if (i < Args.Num())
            FullCommand.AppendChar(TEXT(' '));
    }
    const int32 RequestId = RConClient.SendCommand(FullCommand);
    if (RequestId != INDEX_NONE)
    {
        WriteToConsole(*FString::Printf(TEXT("Execute command \'%s\'. Id: %d"), *FullCommand, RequestId));

        const auto OutputResponse = [](int32, const FString& Response)
        {
            URConClientSubsystem::WriteToConsole(Response);
        };

        AddRequestCallback(RequestId, FRConClientResponseCallback::CreateWeakLambda(this, OutputResponse));
    }
    else
    {
        WriteToConsole(TEXT("Failed send command"), ELogVerbosity::Error);
    }
}

void URConClientSubsystem::OnConsoleDisconnect(const TArray<FString>& Args, FOutputDevice& OutputDevice)
{
    RConClient.Disconnect();
}

void URConClientSubsystem::WriteToConsole(const FString& Value, ELogVerbosity::Type Verbosity)
{
    UConsole* ViewportConsole = GEngine->GameViewport ? GEngine->GameViewport->ViewportConsole : nullptr;
    if (ViewportConsole)
    {
        FConsoleOutputDevice OutputDevice(ViewportConsole);
        OutputDevice.Serialize(*FString::Printf(TEXT("%s"), *Value), Verbosity, STRINGIFY(RConClientSubsystem));
    }
}
