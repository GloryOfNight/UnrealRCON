#pragma once

#include <CoreMinimal.h>

#include "SteamRConSettings.generated.h"

UCLASS(Config = Game)
class STEAMRCONSERVER_API USteamRConSettings : public UObject
{
    GENERATED_BODY()
public:
    static inline USteamRConSettings* Get()
    {
        return GetMutableDefault<USteamRConSettings>();
    }

    // RCon server default port
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    int32 Port{8000};

    // RCon server default password
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    FString Password{TEXT("changeme")};

    // Allow launching RCon server subsystem in editor build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInEditorBuild{true};

    // Allow launching RCon server subsystem in game build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInGameBuild{false};

    // Allow launching RCon server subsystem in game shipping build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInGameShippingBuild{false};

    // Allow launching RCon server subsystem in server build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInServerBuild{true};

    // Allow launching RCon server subsystem in server shipping build
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAllowInServerShippingBuild{true};

    // Allow RCon server subsystem to start automaticly without -RConEnable argument
    UPROPERTY(Config, EditAnywhere, Category = "RCon Server Settings")
    bool bAutoStart{false};
};