// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Editor/UnrealEdEngine.h"
#include "SpatialEditorEngine.generated.h"


UCLASS()
class SPATIALGDKEDITOR_API USpatialEditorEngine : public UUnrealEdEngine
{
	GENERATED_BODY()

public:
	virtual void TickWorldTravel(FWorldContext& WorldContext, float DeltaSeconds) override;

	virtual EBrowseReturnVal::Type Browse(FWorldContext& WorldContext, FURL URL, FString& Error) override;
};

#endif  // WITH_EDITOR
