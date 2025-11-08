// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "RConServerSettings.h"

URConServerSettings::URConServerSettings()
    : Super()
{
    CategoryName = "Plugins";
    SectionName = "RCon Server";
}

URConServerSettings* URConServerSettings::Get()
{
    return GetMutableDefault<URConServerSettings>();
}