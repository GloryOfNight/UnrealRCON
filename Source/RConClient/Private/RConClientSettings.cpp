// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConClientSettings.h"

URConClientSettings::URConClientSettings()
    : Super()
{
    CategoryName = "Plugins";
    SectionName = "RCon Client";
}

URConClientSettings* URConClientSettings::Get()
{
    return GetMutableDefault<URConClientSettings>();
}