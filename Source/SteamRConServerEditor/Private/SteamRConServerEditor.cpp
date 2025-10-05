// Copyright (c) 2025 Siarhei Dziki aka "GloryOfNight"

#include "SteamRConServerEditor.h"

#include <ISettingsModule.h>

#include "SteamRConSettings.h"

IMPLEMENT_MODULE(FSteamRConServerEditorModule, SteamRConServerEditor)

DEFINE_LOG_CATEGORY_STATIC(SteamRConServerEditor, Log, All);

#define LOCTEXT_NAMESPACE "FSteamRconServerEditorModule"

void FSteamRConServerEditorModule::StartupModule()
{
    IModuleInterface::StartupModule();

    ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
    if (SettingsModule != nullptr)
    {
        SettingsModule->RegisterSettings("Project", "RCon", "RCon Server Settings",
            LOCTEXT("SteamRConServerSettingsName", "RCon Server Settings"),
            LOCTEXT("SteamRConServerSettingsDescription", "Configure RCon server settings"),
            GetMutableDefault<USteamRConSettings>());
    }
}

void FSteamRConServerEditorModule::ShutdownModule()
{
    IModuleInterface::ShutdownModule();
}
