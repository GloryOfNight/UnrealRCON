# UnrealRCON
Unreal Engine plugin with Steam RCON server implementation

# How to install
1. Download source from latest [releases](https://github.com/GloryOfNight/UnrealRCON/releases)
2. Unpack source code to Plugins/SteamRCon folder
3. Regenerate and launch project, make sure SteamRCon enabled in Plugins
> [!NOTE]
> DO NOT download source code from main branch since it could be unstable at times. Only do that from tags or releases page!
# How to use

### RCON client
You can use any Steam RCON compatible client, for example [rcon](https://github.com/n0la/rcon) or [rcon-cli](https://github.com/gorcon/rcon-cli)

### Startup options
`-RConEnable` to enable rcon server

`-RConPort=8000` to set rcon server port. In case of forked server, port + fork id would be used for that fork

`-RConPassoword=changeme` to set rcon server password

### Default commands
`help` List all available commands

`exec` Execute unreal engine console command

### Adding custom commands
> [!NOTE]
> By default `USteamRConServerSubsystem` would only be created on Server only builds! If you want to override that befavior take a look in `USteamRConServerSubsystem::ShouldCreateSubsystem`

> [!WARNING]
> Make sure to add `SteamRConServer` to your **project**.build.cs file!

```
PublicDependencyModuleNames.AddRange(new string[] { "SteamRConServer" });
```

Binding commands
```
#include "SteamRConServerSubsystem.h"

void USomeObject::AnyDesiredBindFunction()
{
	const auto SteamRConSubsystem = USteamRConServerSubsystem::Get(this);
	if (SteamRConSubsystem)
	{
		const auto CommandCallbackLam = [](const FString& Command) -> FString
			{
				// Execute code and write back to connected rcon client
				FString Output{};
				Output = TEXT("Player list is . . .");
				return Output;
			};
		SteamRConSubsystem->AddCommandCallback(TEXT("list_players"), FSteamRConServerCommandCallback::CreateLambda(CommandCallbackLam), TEXT("List current players"));
	}
}
```
if your command uses argument, use string space separation to separate command arguments from command itself

```
#include "SteamRConServerSubsystem.h"

void USomeObject::AnyDesiredBindFunction()
{
	const auto CommandCallbackLam = [](const FString& Command) -> FString
	{
		FString CommandArguments{};
		int32 SpaceIndex;
		if (Command.FindChar(TEXT(' '), SpaceIndex))
		{
			CommandArguments = Command.Mid(SpaceIndex + 1);
		}
		// Sample values:
		// Command = kick Player1
		// CommandArguments = Player1
		// Handle arguments separately from kick command
		return FString(TEXT("Player kicked"));
	};
	SteamRConSubsystem->AddCommandCallback(TEXT("kick"), FSteamRConServerCommandCallback::CreateLambda(CommandCallbackLam), TEXT("kick [PlayerName] from match"));
}
```
# See also
[Steam RCON Specification](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol)
