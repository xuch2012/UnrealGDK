// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKEditorModule.h"

#include "ModuleManager.h"
#include "Paths.h"
#include "PlatformProcess.h"
#include "UObjectBase.h"

#define LOCTEXT_NAMESPACE "FSpatialGDKEditorModule"

DEFINE_LOG_CATEGORY(LogSpatialGDKEditorModule);

IMPLEMENT_MODULE(FSpatialGDKEditorModule, SpatialGDKEditor)

void FSpatialGDKEditorModule::StartupModule()
{
}

void FSpatialGDKEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
