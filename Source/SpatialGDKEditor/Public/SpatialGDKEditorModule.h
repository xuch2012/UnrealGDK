// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Developer/Settings/Public/ISettingsContainer.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "Developer/Settings/Public/ISettingsSection.h"
#include "ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGDKEditorModule, Log, All);

class SPATIALGDKEDITOR_API FSpatialGDKEditorModule : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
};
