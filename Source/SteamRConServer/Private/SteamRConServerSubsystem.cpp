// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "SteamRConServerSubsystem.h"

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

DEFINE_LOG_CATEGORY_STATIC(SteamRConServerSubsystem, Log, All);

USteamRConServerSubsystem* USteamRConServerSubsystem::Get(const UObject* Context)
{
    UGameInstance* GameInstance{nullptr};

    auto World = GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::LogAndReturnNull);

    if (World)
        GameInstance = World->GetGameInstance();

    return GameInstance ? GameInstance->GetSubsystem<USteamRConServerSubsystem>() : nullptr;
}

uint16 USteamRConServerSubsystem::GetRConPort()
{
    FString ClPort{};
    FParse::Value(FCommandLine::Get(), TEXT("-RConPort="), ClPort);
    return !ClPort.IsEmpty() ? FCString::Atoi(*ClPort) : 8000;
}

FString USteamRConServerSubsystem::GetRConPassword()
{
    FString ClPassword{};
    FParse::Value(FCommandLine::Get(), TEXT("-RConPassword="), ClPassword);
    return !ClPassword.IsEmpty() ? ClPassword : FString(TEXT("changeme"));
}

bool USteamRConServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const bool bForceEnable = false; // for debug and testing
    const bool bRealEnable = FParse::Param(FCommandLine::Get(), TEXT("RConEnable")) && UE_SERVER;
    return bForceEnable || bRealEnable;
}

void USteamRConServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    const FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &USteamRConServerSubsystem::Tick);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate);

    FSteamRConServer::FSettings Settings{};
    Settings.Port = GetRConPort();
    Settings.Password = GetRConPassword();

    if (RConServer.Start(Settings))
    {
        RConServer.AssignCommandCallback(FSteamRConServer::FHandleReceivedCommandDelegate::CreateUObject(this, &USteamRConServerSubsystem::HandleRConCommand));

        AddCommandCallback(TEXT("help"), FSteamRConServer::FHandleReceivedCommandDelegate::CreateUObject(this, &USteamRConServerSubsystem::OnHelpCommand), TEXT("List all available command and some help info"));
        AddCommandCallback(TEXT("exec"), FSteamRConServer::FHandleReceivedCommandDelegate::CreateUObject(this, &USteamRConServerSubsystem::OnExecCommand), TEXT("Execute unreal engine console command"));

        FCoreDelegates::OnPostFork.AddUObject(this, &USteamRConServerSubsystem::OnPostFork);

        UE_LOG(SteamRConServerSubsystem, Display, TEXT("RCon server ready"));
    }
    else
    {
        UE_LOG(SteamRConServerSubsystem, Error, TEXT("Failed to start RCon server"));
    }
}

void USteamRConServerSubsystem::Deinitialize()
{
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle = FTSTicker::FDelegateHandle();
    }
}

bool USteamRConServerSubsystem::Tick(float DeltaTime)
{
    RConServer.Tick();
    return true;
}

void USteamRConServerSubsystem::AddCommandCallback(FString Command, FSteamRConServerCommandCallback InCallback, const FString& InTooltip)
{
    if (!InCallback.IsBound())
    {
        UE_LOG(SteamRConServerSubsystem, Warning, TEXT("Failed to register command \'%s\', since delegate not bound"), *Command);
        return;
    }

    int32 SpaceIndex{};
    if (Command.FindChar(TEXT(' '), SpaceIndex))
    {
        Command = Command.Left(SpaceIndex);
    }

    UE_LOG(SteamRConServerSubsystem, Verbose, TEXT("Registered command: %s"), *Command);

    FCommandHandle CommandHandle{};
    CommandHandle.Tooltip = InTooltip;
    CommandHandle.Callback = InCallback;

    CommandHandles.Add(Command, CommandHandle);
}

FString USteamRConServerSubsystem::HandleRConCommand(const FString& Command)
{
    FString InputCommand{Command};

    int32 SpaceIndex{};
    if (InputCommand.FindChar(TEXT(' '), SpaceIndex))
    {
        InputCommand = InputCommand.Left(SpaceIndex);
    }

    if (CommandHandles.Contains(InputCommand))
    {
        const auto& CommandHandle = CommandHandles[InputCommand];
        if (CommandHandle.Callback.IsBound())
            return CommandHandle.Callback.Execute(Command);
    }

    return FString::Printf(TEXT("Command \'%s\' not recognized (use \'help\' to list all available commands)"), *Command);
}

FString USteamRConServerSubsystem::OnHelpCommand(const FString& Command)
{
    FString Output{};
    Output.Append(TEXT("Listing all available commands:\n"));
    for (const auto& CommandHandle : CommandHandles)
    {
        Output.Append(FString::Printf(TEXT("%s - %s \n"), *CommandHandle.Key, *CommandHandle.Value.Tooltip));
    }
    return Output;
}

FString USteamRConServerSubsystem::OnExecCommand(const FString& Command)
{
    FString CommandArguments{};
    int32 SpaceIndex;
    if (Command.FindChar(TEXT(' '), SpaceIndex))
    {
        CommandArguments = Command.Mid(SpaceIndex + 1);
    }

    FExecOutputDevice OutputDevice{};
    const bool bExec = GEngine->Exec(GetWorld(), *CommandArguments, OutputDevice);

    return bExec ? FString::Printf(TEXT("Executed: %s; \n %s"), *Command, *OutputDevice.Output)
                 : FString::Printf(TEXT("Failed to execute: %s"), *Command);
}

void USteamRConServerSubsystem::OnPostFork(EForkProcessRole Role)
{
    if (Role == EForkProcessRole::Child)
    {
        RConServer.Stop();

        FSteamRConServer::FSettings Settings{};
        Settings.Port = GetRConPort() + FForkProcessHelper::GetForkedChildProcessIndex();
        Settings.Password = GetRConPassword();
        Settings.bAllowPortReuse = true; // for forked processes reuse is a must

        RConServer.Start(Settings);
    }
}
