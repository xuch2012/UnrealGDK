// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkingSetManager.h"

UWorkingSetManager::UWorkingSetManager()
{
	// local working set id initialization
	CurrentWorkingSetId = 0;
}

void UWorkingSetManager::CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const FString& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged)
{
	FWorkingSetData Data = { Channels, Location, PlayerWorkerId, RepChanged, HandoverChanged };

	//Reserve ids for working set and the parent
	PendingWorkingSetCreationRequests.Add(Sender->SendReserveEntityIdsRequest(Channels.Num()+1), Data);
}

//todo
void UWorkingSetManager::CreateWorkingSet(const uint32 & WorkingSetId)
{
	if (!PendingWorkingSets.Contains(WorkingSetId))
	{
		// haven't registered working set
		return;
	}

	FWorkingSetData* WorkingSetData = PendingWorkingSets.Find(WorkingSetId);
	PendingWorkingSetCreationRequests.Add(Sender->SendReserveEntityIdsRequest(WorkingSetData->ActorChannels.Num() + 1), *WorkingSetData);
}

bool UWorkingSetManager::IsRelevantRequest(const Worker_RequestId & RequestId)
{
	return PendingWorkingSetCreationRequests.Contains(RequestId);
}

void UWorkingSetManager::ProcessWorkingSet(const Worker_EntityId& FirstId, const uint32 & NumOfEntities, const Worker_RequestId & RequestId)
{
	if (FWorkingSetData* WorkingSetInitParams = PendingWorkingSetCreationRequests.Find(RequestId))
	{
		TArray<USpatialActorChannel*> ActorChannels = WorkingSetInitParams->ActorChannels;
		FVector Location = WorkingSetInitParams->Location;
		FWorkingSet WorkingSet = { FirstId, ActorChannels };

		//Todo: async exec
		Sender->SendCreateWorkingSetParentEntity(FirstId, Location, NumOfEntities);

		for (uint32 i = 0; i < NumOfEntities; i++) {
			USpatialActorChannel* ActorChannel = ActorChannels[i];
			ActorChannel->SetEntityId(FirstId+i+1);
			CurrentWorkingSets.Add(ActorChannel, WorkingSet);
			Sender->SendCreateEntityRequest(ActorChannel,
				Location,
				WorkingSetInitParams->PlayerWorkerId,
				WorkingSetInitParams->RepChangedData[i],
				WorkingSetInitParams->HandoverData[i],
				&FirstId);
		}

		PendingWorkingSetCreationRequests.Remove(RequestId);
	}
}

// Register working set
uint32 UWorkingSetManager::RegisterNewWorkingSet()
{
	TArray<USpatialActorChannel*> ActorChannels;
	FVector Location;
	FString PlayerWorkerId;
	TArray<TArray<uint16>> RepChangedData;
	TArray<TArray<uint16>> HandoverData;

	PendingWorkingSets.Add(CurrentWorkingSetId, { ActorChannels, Location, PlayerWorkerId, RepChangedData, HandoverData });
	return CurrentWorkingSetId++;
}

//todo
void UWorkingSetManager::EnqueueForWorkingSet(USpatialActorChannel* Channel, const FVector& Location, const FString& PlayerWorkerId, const TArray<uint16>& RepChanged, const TArray<uint16>& HandoverChanged, const uint32& WorkingSetId)
{
	if (!PendingWorkingSets.Contains(WorkingSetId))
	{
		// haven't registered working set
		return;
	}

	FWorkingSetData* WorkingSetData = PendingWorkingSets.Find(WorkingSetId);
	WorkingSetData->ActorChannels.Add(Channel);

	//overrides location, change with support of multiple locations
	WorkingSetData->Location = Location;
	WorkingSetData->PlayerWorkerId = PlayerWorkerId;

	WorkingSetData->RepChangedData.Add(RepChanged);
	WorkingSetData->HandoverData.Add(HandoverChanged);
}
