// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SpatialGameState.generated.h"

SPATIALGDK_API DECLARE_LOG_CATEGORY_EXTERN(LogSpatialGDKGameState, Log, All);

// Extension of GameState also containing information about time synchronized across managed workers.

UCLASS()
class SPATIALGDK_API ASpatialGameState : public AGameState
{
	GENERATED_BODY()

public:

	ASpatialGameState(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > &OutLifetimeProps) const;

	uint64 GetSpatialStartTime() const;

	virtual void SetSpatialStartTime(const uint64& StartTime);

	/** Returns the current game time based on the system time and the SpatialStartTime. */
	virtual uint64 GetSpatialTime() const;

protected:

	uint64 CalculateTime() const;

	/** Timestamp of the game start. */
	UPROPERTY(Replicated)
	uint64 SpatialStartTime;
};
