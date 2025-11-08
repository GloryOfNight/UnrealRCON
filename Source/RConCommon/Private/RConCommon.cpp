// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConCommon.h"

IMPLEMENT_MODULE(FRConCommonModule, RConCommon)

TArray<uint8> FRConPacket::Serialize() const
{
    return SerializePacket(*this);
}

TArray<uint8> FRConPacket::SerializePacket(const FRConPacket& Packet)
{
    TArray<uint8> OutData{};

    FTCHARToUTF8 Utf8(*Packet.Body);
    const int32 Size = 4 + 4 + Utf8.Length() + 2;

#if PLATFORM_LITTLE_ENDIAN
    OutData.Append((uint8*)&Size, 4);
    OutData.Append((uint8*)&Packet.Id, 4);
    OutData.Append((uint8*)&Packet.Type, 4);
#else
    const int32 LeRespSize = NETWORK_ORDER32(Size);
    const int32 LeRespId = NETWORK_ORDER32(Packet.Id);
    const int32 LeRespType = NETWORK_ORDER32((int32)Packet.Type);
    Response.Append((uint8*)&LeRespSize, 4);
    Response.Append((uint8*)&LeRespId, 4);
    Response.Append((uint8*)&LeRespType, 4);
#endif

    OutData.Append((uint8*)Utf8.Get(), Utf8.Length());
    OutData.Add(0);
    OutData.Add(0);

    return OutData;
}

TPair<bool, FRConPacket> FRConPacket::DeserializePacket(uint8* Data, int32 Size)
{
    if (Size < CRConBasePacketSize)
        return TPair<bool, FRConPacket>();

    FRConPacket OutPacket{};

    FMemory::Memcpy(&OutPacket.Id, Data + 4, 4);
    FMemory::Memcpy(&OutPacket.Type, Data + 8, 4);

#if PLATFORM_LITTLE_ENDIAN == 0
    OutPacket.Id = NETWORK_ORDER32(OutPacket.Id);
    OutPacket.Type = static_cast<ERConPacketType>(NETWORK_ORDER32((int32)OutPacket.Type));
#endif

    OutPacket.Body = FString(UTF8_TO_TCHAR(reinterpret_cast<char*>(Data + CRConBasePacketSize)));

    return TPair<bool, FRConPacket>(true, OutPacket);
}