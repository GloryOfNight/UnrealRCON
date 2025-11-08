// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConServerSubsystem.h"

#include <HAL/ConsoleManager.h>

#include "RConServerSettings.h"

DEFINE_LOG_CATEGORY_STATIC(RConServerSubsystem, Log, Log);
#define STRINGIFY(Name) #Name

class FExecOutputDevice : public FOutputDevice
{
public:
    void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (!Output.IsEmpty())
            Output.AppendChar(TEXT('\n'));
        Output.Append(V);
    }

    FString Output{};
};

enum
{
    COMMAND_EXEC = 1,
    COMMAND_RESPONSE = 2
};

URConServerSubsystem* URConServerSubsystem::Get(const UObject* Context)
{
    UGameInstance* GameInstance{nullptr};
    auto World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull);
    if (World)
        GameInstance = World->GetGameInstance();
    return GameInstance ? GameInstance->GetSubsystem<URConServerSubsystem>() : nullptr;
}

uint16 URConServerSubsystem::GetRConPort()
{
    FString ClPort{};
    FParse::Value(FCommandLine::Get(), TEXT("-RConPort="), ClPort);
    return !ClPort.IsEmpty() ? FCString::Atoi(*ClPort) : URConServerSettings::Get()->Port;
}

FString URConServerSubsystem::GetRConPassword()
{
    FString ClPassword{};
    FParse::Value(FCommandLine::Get(), TEXT("-RConPassword="), ClPassword);
    return !ClPassword.IsEmpty() ? ClPassword : URConServerSettings::Get()->Password;
}

uint16 URConServerSubsystem::GetRConMaxActiveConnections()
{
    FString ClMaxActiveConnections{};
    FParse::Value(FCommandLine::Get(), TEXT("-RConMaxActiveConnections="), ClMaxActiveConnections);
    return !ClMaxActiveConnections.IsEmpty() ? FCString::Atoi(*ClMaxActiveConnections) : URConServerSettings::Get()->MaxActiveConnections;
}

bool URConServerSubsystem::ShouldCreateSubsystem_StaticCheck()
{
    bool bAllowCreate = false;
    if (WITH_EDITOR)
        bAllowCreate = URConServerSettings::Get()->bAllowInEditorBuild;
    else if (UE_SERVER && UE_BUILD_SHIPPING)
        bAllowCreate = URConServerSettings::Get()->bAllowInServerBuild && URConServerSettings::Get()->bAllowInServerShippingBuild;
    else if (UE_SERVER)
        bAllowCreate = URConServerSettings::Get()->bAllowInServerBuild;
    else if (UE_GAME && UE_BUILD_SHIPPING && RCON_SERVER_ALLOW_IN_GAME_SHIPPING)
        bAllowCreate = URConServerSettings::Get()->bAllowInGameBuild;
    else if (UE_GAME)
        bAllowCreate = URConServerSettings::Get()->bAllowInGameBuild;
    return bAllowCreate;
}

bool URConServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const bool bAllowCreate = ShouldCreateSubsystem_StaticCheck();
    UE_LOG(RConServerSubsystem, Verbose, TEXT("ShouldCreateSubsystem %d"), bAllowCreate);
    return bAllowCreate;
}

void URConServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    IConsoleManager& ConsoleManager = IConsoleManager::Get();
    ConsoleManager.RegisterConsoleCommand(TEXT("rcon.server.start"), TEXT("Start rcon server"), FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateUObject(this, &URConServerSubsystem::OnConsoleStartServer));
    ConsoleManager.RegisterConsoleCommand(TEXT("rcon.server.stop"), TEXT("Stop rcon server"), FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateUObject(this, &URConServerSubsystem::OnConsoleStopServer));

    const FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &URConServerSubsystem::TickServer);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate);

    FCommandProperties Properties{};

    AddCommand(TEXT("help"), FRConServer::FHandleReceivedCommandDelegate::CreateUObject(this, &URConServerSubsystem::OnHelpCommand), TEXT("<command> - list all available command or print specific command help"), Properties);

    Properties.Help = TEXT("exec <command> \nRedirects input command to unreal GEngine->Exec function and responds with unreal output to that command");
    AddCommand(TEXT("exec"), FRConServer::FHandleReceivedCommandDelegate::CreateUObject(this, &URConServerSubsystem::OnExecCommand), TEXT("<command> - execute unreal engine console command"), Properties);

    FCoreDelegates::OnPostFork.AddUObject(this, &URConServerSubsystem::OnPostFork);

    TryAutoStart();
}

void URConServerSubsystem::Deinitialize()
{
    Super::Deinitialize();

    FCoreDelegates::OnPostFork.RemoveAll(this);
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle = FTSTicker::FDelegateHandle();
    }

    if (IsStarted())
    {
        StopServer();
    }
}

void URConServerSubsystem::StartServer()
{
    if (RConServer.IsStarted())
    {
        UE_LOG(RConServerSubsystem, Warning, TEXT("Attempt to start RCon server, when it already started"));
        return;
    }

    FRConServer::FSettings Settings{};
    Settings.Port = GetRConPort() + FForkProcessHelper::GetForkedChildProcessIndex();
    Settings.Password = GetRConPassword();
    Settings.bAllowPortReuse = FForkProcessHelper::IsForkedChildProcess();
    Settings.MaxActiveConnections = GetRConMaxActiveConnections();

    if (RConServer.Start(Settings))
    {
        UE_LOG(RConServerSubsystem, Log, TEXT("RCon server started. Using port: %d"), RConServer.GetBoundPort());

        RConServer.AssignClientConnectedCallback(FRConServer::FHandleClientConnectedDelegate::CreateUObject(this, &URConServerSubsystem::HandleClientConnected));
        RConServer.AssignCommandCallback(FRConServer::FHandleReceivedCommandDelegate::CreateUObject(this, &URConServerSubsystem::HandleRConCommand));
    }
    else
    {
        UE_LOG(RConServerSubsystem, Error, TEXT("Failed to start RCon server"));
    }
}

void URConServerSubsystem::SendCommandResponse(int32 RequestId, const FString& Response)
{
    RConServer.SendResponse(RequestId, Response);
}

void URConServerSubsystem::StopServer()
{
    if (RConServer.IsStarted())
    {
        RConServer.Stop();
        UE_LOG(RConServerSubsystem, Log, TEXT("RCon server stopped"));
    }
}

void URConServerSubsystem::AddCommand(FString InCommand, FRConServerCommandCallback InCallback, FString InTooltip, FCommandProperties InProperties)
{
    FCommandHandle CommandHandle{};
    CommandHandle.Command = MoveTemp(InCommand);
    CommandHandle.Callback = MoveTemp(InCallback);
    CommandHandle.Tooltip = MoveTemp(InTooltip);
    CommandHandle.Properties = MoveTemp(InProperties);

    AddCommand(MoveTemp(CommandHandle));
}

void URConServerSubsystem::AddCommand(FCommandHandle InCommandHandle)
{
    UE_LOG(RConServerSubsystem, Verbose, TEXT("Registered command: %s"), *InCommandHandle.Command);
    CommandHandles.Emplace(InCommandHandle.Command, InCommandHandle);
}

void URConServerSubsystem::TryAutoStart()
{
    const bool bAutoEnable = URConServerSettings::Get()->bAutoStart;
    const bool bClEnable = FParse::Param(FCommandLine::Get(), TEXT("RConEnable"));
    if (bAutoEnable || bClEnable)
    {
        UE_LOG(RConServerSubsystem, Log, TEXT("Subsystem initialized. Starting RCon server now."));
        StartServer();
    }
    else
    {
        UE_LOG(RConServerSubsystem, Log, TEXT("Subsystem initialized. Waiting for StartServer() function."));
    }
}

bool URConServerSubsystem::TickServer(float DeltaTime)
{
    if (RConServer.IsStarted())
    {
        RConServer.Tick();
    }
    return true;
}

void URConServerSubsystem::HandleClientConnected()
{
}

void URConServerSubsystem::HandleRConCommand(int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse)
{
    auto* CommandHandle = FindCommandHandle(Command);
    if (CommandHandle)
    {
        if (CommandHandle->Callback.IsBound())
        {
            CommandHandle->Callback.Execute(RequestId, Command, Response, bDelayResponse);
        }
        else
        {
            Response = FString::Printf(TEXT("Recognized command \'%s\', but has no bound callback. Huh."), *CommandHandle->Command);
        }
    }
    else
    {
        Response = FString::Printf(TEXT("Command \'%s\' not recognized (use \'help\' to list all available commands)"), *Command);
    }
}

void URConServerSubsystem::OnConsoleStartServer(const TArray<FString>& Args, FOutputDevice& OutputDevice)
{
    StartServer();
    if (RConServer.IsStarted())
    {
        OutputDevice.Serialize(*FString::Printf(TEXT("RCon server ready on port %d"), RConServer.GetBoundPort()), ELogVerbosity::Display, STRINGIFY(RConServerSubsystem));
    }
}

void URConServerSubsystem::OnConsoleStopServer(const TArray<FString>& Args, FOutputDevice& OutputDevice)
{
    StopServer();
    OutputDevice.Serialize(TEXT("RCon server stopped"), ELogVerbosity::Display, STRINGIFY(RConServerSubsystem));
}

URConServerSubsystem::FCommandHandle* URConServerSubsystem::FindCommandHandle(const FString& Command)
{
    // Chop parts of a original command, until found match
    FString InputCommand = Command;

    for (uint8 i = 0; i < /* MaxDepth */ UINT8_MAX; ++i)
    {
        auto* Handle = CommandHandles.Find(InputCommand);
        if (Handle)
        {
            return Handle;
        }
        else
        {
            int32 SpaceIndex{0};
            if (InputCommand.FindLastChar(TEXT(' '), SpaceIndex))
            {
                InputCommand = InputCommand.Left(SpaceIndex);
            }
            else
            {
                break;
            }
        }
    }
    return nullptr;
}

void URConServerSubsystem::OnHelpCommand(int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse)
{
    FString CommandArg{};
    int32 SpaceIndex;
    if (Command.FindChar(TEXT(' '), SpaceIndex) && Command.IsValidIndex(SpaceIndex + 1))
    {
        CommandArg = Command.Mid(SpaceIndex + 1);
    }

    if (CommandArg.Len())
    {
        auto* CommandHandle = FindCommandHandle(CommandArg);
        if (CommandHandle)
        {
            Response.Append(FString::Printf(TEXT("%s %s \n - - - \n"), *CommandHandle->Command, *CommandHandle->Tooltip));
            if (CommandHandle->Properties.Help.IsEmpty())
            {
                Response.Append(TEXT("No extra help has been provided for that command."));
            }
            else
            {
                Response.Append(CommandHandle->Properties.Help);
            }
        }
        else
        {
            Response.Append(FString::Printf(TEXT("Help. Command \"%s\" not recognized!"), *CommandArg));
        }
    }
    else
    {
#if UE_SERVER
        Response.Append(FString::Printf(TEXT("Current Process Id - %d\n"), FPlatformProcess::GetCurrentProcessId()));
        Response.Append(FString::Printf(TEXT("Current Fork Id - %d\n"), FForkProcessHelper::GetForkedChildProcessIndex()));
#endif
        Response.Append(TEXT("Listing all available commands:\n"));
        for (const auto& [CommandKey, CommandHandle] : CommandHandles)
        {
            if (CommandHandle.Callback.IsBound())
            {
                Response.Append(FString::Printf(TEXT("%s %s \n"), *CommandHandle.Command, *CommandHandle.Tooltip));
            }
            else
            {
                Response.Append(FString::Printf(TEXT("%s %s (no bound)\n"), *CommandHandle.Command, *CommandHandle.Tooltip));
            }
        }
    }
}

void URConServerSubsystem::OnExecCommand(int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse)
{
    FString CommandArg{};
    int32 SpaceIndex;
    if (Command.FindChar(TEXT(' '), SpaceIndex) && Command.IsValidIndex(SpaceIndex + 1))
    {
        CommandArg = Command.Mid(SpaceIndex + 1);
    }

    FExecOutputDevice OutputDevice{};
    const bool bExec = GEngine->Exec(GetWorld(), *CommandArg, OutputDevice);

    if (bExec)
        Response = FString::Printf(TEXT("exec: %s; \n %s"), *Command, *OutputDevice.Output);
    else
        Response = FString::Printf(TEXT("exec: %s; \n Failed to execute"), *Command);
}

void URConServerSubsystem::OnPostFork(EForkProcessRole Role)
{
    if (IsStarted())
    {
        StopServer();
        StartServer();
    }
    else
    {
        TryAutoStart();
    }
}