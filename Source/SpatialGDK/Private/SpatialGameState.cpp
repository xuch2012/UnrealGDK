// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include <chrono>
#include "SpatialGameState.h"
#include "UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogSpatialGDKGameState);

ASpatialGameState::ASpatialGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SpatialStartTime = 0;
}

void ASpatialGameState::SetSpatialStartTime(const uint64& StartTime)
{
	if (SpatialStartTime != 0)
	{
		UE_LOG(LogSpatialGDKGameState, Display, TEXT("Reseting SpatialStartTime based on the authorititave worker's start time to: %ld"), StartTime)
	}

	SpatialStartTime = StartTime;
}

uint64 ASpatialGameState::GetSpatialStartTime() const
{
	return SpatialStartTime;
}

uint64 ASpatialGameState::GetSpatialTime() const
{
	if (SpatialStartTime == 0)
	{
		UE_LOG(LogSpatialGDKGameState, Warning, TEXT("Spatial game start time has not been set yet."))
		return 0;
	}

	return CalculateTime();
}

// We use std::chrono to enable a high precision game clock. It enables
// fetching the current time in milliseconds via a single call to time_since_epoch().
// This functionality is not present in FDateTime and thus a potential context
// switch could result in a significant time difference between the times at which we
// get the milliseconds and the seconds.
uint64 ASpatialGameState::CalculateTime() const
{
	uint64 MillisSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	return MillisSinceEpoch - SpatialStartTime;
}

void ASpatialGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASpatialGameState, SpatialStartTime);
}
