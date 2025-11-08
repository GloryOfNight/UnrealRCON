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
2. Unpack source code to Plugins/UnrealRCON folder
3. Regenerate and launch project, make sure UnrealRCON enabled in Plugins
> [!WARNING]
> DO NOT download source code from main branch since it could be unstable at times. Only do that from tags or releases page!

### RCON client
You can use any Source RCON compatible client, for example [ARRCON]([https://github.com/n0la/rcon](https://github.com/radj307/ARRCON)) or [rcon-cli](https://github.com/gorcon/rcon-cli)

### Startup options
`-RConEnable` auto-start rcon server on startup if allowed

`-RConPort=27015` set rcon server port. In case of forked server, port + fork id would be used for that fork

`-RConPassoword=1111` set rcon server password

`-RConMaxActiveConnections=5` set maximum amount of active connections

### Config
`DefaultGame.ini`
```
[/Script/RConServer.RConSettings]
Port=27015 # Note: Commandline argument has a priority over config
Password=1111 # Note: Commandline argument has a priority over config
MaxActiveConnections=5 # Note: Commandline argument has a priority over config
bAllowInEditorBuild=True
bAllowInGameBuild=False
bAllowInGameShippingBuild=False
bAllowInServerBuild=True
bAllowInServerShippingBuild=True
bAutoStart=False # Note: if true, -RConEnable not required to auto-start rcon server
```

### Default commands
`help` List all available commands

`exec` Execute unreal engine console command

### Adding custom commands
(C++ only)

> [!TIP]
> Create new [Game Instance Subsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine) class to handle custom rcon commands code.

1. Open your project .build.cs file
   and apply `RConServer` to module dependecies list, just like that:
```
PublicDependencyModuleNames.AddRange(new string[] { "RConServer" });
```
2. In your codebase, add include
```
#include "RConServerSubsystem.h"
```
3. Start implementing 
```
URConServerSubsystem* RConServerSubsystem = URConServerSubsystem::Get(this);
if (RConServerSubsystem)
{
	// Preferably you want bind calls to UObjects not lambda's (FDelegate::CreateUObject)
	const auto CommandCallbackLam = [this](int32 RequestId, const FString& Command, FString& Response, bool& bDelayResponse)
		{
			Reponse.Append(TEXT("Listing players:"));
			for (const auto& Player : PlayerList}
				Reponse.Append(FString::Printf(TEXT(\n%s (%s)), *Player->Nickname, *Player->Id));
		};
	// Adding commands handles is easy
	RConServerSubsystem->AddCommand(TEXT("list players"), FRConServerCommandCallback::CreateWeakLambda(this, CommandCallbackLam), TEXT("List current players"));
}
```
