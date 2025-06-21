# UnrealRCON
UnrealRCON is a **RCON (Remote Console)** plugin for **Unreal Engine**, designed for servers to remotely receive and handle commands.

## 🔧 Features

- ✅ RCON server protocol implementation
- 🎮 Built to integrate directly into Unreal Engine project as a plugin
- 🔁 Easy integration and use
- 🧪 Simple enought to tailor for your specific needs

## 🚀 Getting Started

### Installation
1. Download source from latest [releases](https://github.com/GloryOfNight/UnrealRCON/releases)
2. Unpack source code to Plugins/SteamRCon folder
3. Regenerate and launch project, make sure SteamRCon enabled in Plugins
> [!WARNING]
> DO NOT download source code from main branch since it could be unstable at times. Only do that from tags or releases page!

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
(C++ only)

> [!TIP]
> Create new [Game Instance Subsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine) class to handle custom rcon commands code.

1. Open your project .build.cs file
   and apply `SteamRConServer` to module dependecies list, just like that:
```
PublicDependencyModuleNames.AddRange(new string[] { "SteamRConServer" });
```
2. In your codebase, add include
```
#include "SteamRConServerSubsystem.h"
```
3. Start implementing 
```
const auto SteamRConSubsystem = USteamRConServerSubsystem::Get(this);
if (SteamRConSubsystem)
{
	// Preferably you want bind calls to UObjects not lambda's
	const auto CommandCallbackLam = [](const FString& Command) -> FString
		{
			// When it's called, you receive command that you bind for in Command variable
			// Command could have some additional arguments that you need to handle here
			// After you done processing command, do not forget write back to RCon client the result of operation
			FString Output{};
			Output = TEXT("Player list is . . .");
			return Output;
		};
	// Adding commands handles is easy
	SteamRConSubsystem->AddCommandCallback(TEXT("list_players"), FSteamRConServerCommandCallback::CreateLambda(CommandCallbackLam), TEXT("List current players"));
}
```
