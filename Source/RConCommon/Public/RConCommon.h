// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>

class FRConCommonModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override {};
    virtual void ShutdownModule() override {};
};

const int32 CRConBasePacketSize = 12;

enum class ERConPacketType : int32
{
    ResponseValue = 0,
    ExecCommand = 2,
    AuthResponse = 2, // repeat of 2 is not a accident!
    Auth = 3
};

struct RCONCOMMON_API FRConPacket
{
    // int32 Size; <unused>
    int32 Id;
    ERConPacketType Type;
    FString Body;

    TArray<uint8> Serialize() const;

    static TArray<uint8> SerializePacket(const FRConPacket& Packet);

    static TPair<bool, FRConPacket> DeserializePacket(uint8* Data, int32 Size);
};