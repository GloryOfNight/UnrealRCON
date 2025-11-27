// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>

#include "RConClientSettings.generated.h"

UCLASS(Config = Game)
class RCONCLIENT_API URConClientSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    URConClientSettings();

    static URConClientSettings* Get();

    // Connection host used by default by connect command
    UPROPERTY(Config, EditAnywhere, Category = "RCon Client Settings")
    FString DefaultHost{TEXT("localhost")};

    // Connection port used by default by connect command
    UPROPERTY(Config, EditAnywhere, Category = "RCon Client Settings")
    uint16 DefaultPort{27015};

    // Password used by default by connect command
    UPROPERTY(Config, EditAnywhere, Category = "RCon Client Settings")
    FString DefaultPassword{TEXT("1111")};

    // Create RCon client subsystem in editor
    UPROPERTY(Config, EditAnywhere, Category = "RCon Client Settings")
    bool bAllowInEditorBuild{true};

    // Create RCon client subsystem in game build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Client Settings")
    bool bAllowInGameBuild{true};

    // Create RCon client subsystem in game shipping build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Client Settings")
    bool bAllowInGameShippingBuild{false};
};