// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkingSetManager.h"

void UWorkingSetManager::CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const FString& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged)
{
	WorkingSetData Data{ Channels, Location, PlayerWorkerId, RepChanged, HandoverChanged };
	PendingWorkingSetCreationRequests.Add(Sender->SendReserveEntityIdsRequest(Channels.Num()), Data);
}

