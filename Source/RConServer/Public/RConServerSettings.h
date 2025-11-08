// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>
#include <Modules/ModuleManager.h>

#include "RConServerSettings.generated.h"

UCLASS(Config = Game)
class RCONSERVER_API URConServerSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    URConServerSettings();

    static URConServerSettings* Get();

    // RCon server default port
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    int32 Port{27015};

    // RCon server default password
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    FString Password{TEXT("1111")};

    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    uint16 MaxActiveConnections{5};

    // Allow launching RCon server subsystem in editor build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInEditorBuild{true};

    // Allow launching RCon server subsystem in game build. Make sure module to enable build for that target first!
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInGameBuild{false};

    // Allow launching RCon server subsystem in server build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInServerBuild{true};

    // Allow launching RCon server subsystem in server shipping build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInServerShippingBuild{true};

    // Allow RCon server subsystem to start automatically without -RConEnable argument
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAutoStart{false};
};