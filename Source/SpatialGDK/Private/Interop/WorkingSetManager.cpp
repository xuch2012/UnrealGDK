// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkingSetManager.h"

void UWorkingSetManager::Init(USpatialNetDriver* NetDriver)
{
	// local working set id initialization
	CurrentWorkingSetId = 1;
	this->NetDriver = NetDriver;
	Sender = NetDriver->Sender;
	Receiver = NetDriver->Receiver;
}

void UWorkingSetManager::CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const TArray<FString>& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged)
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

void UWorkingSetManager::SendPositionUpdate(const USpatialActorChannel* ActorChannel, const FVector& Loction)
{
	if (FWorkingSet** Data = CurrentWorkingSets.Find(ActorChannel))
	{
		// Update parent
		Sender->SendPositionUpdate((*Data)->ParentId, Loction);

		// update children
		for (USpatialActorChannel* Channel : (*Data)->ActorChannels)
		{
			Sender->SendPositionUpdate(Channel->GetEntityId(), Loction);
		}
	}
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
		FWorkingSet* WorkingSet = new FWorkingSet(FirstId, ActorChannels);
		TArray<Schema_EntityId> ChildEntityIds;

		for (uint32 i = 0; i < NumOfEntities-1; i++) {
			USpatialActorChannel* ActorChannel = ActorChannels[i];
			CurrentWorkingSets.Add(ActorChannel, WorkingSet);
			ChildEntityIds.Add(ActorChannel->GetEntityId());
			Sender->SendCreateEntityRequest(ActorChannel,
				Location,
				WorkingSetInitParams->PlayerWorkerId[i],
				WorkingSetInitParams->RepChangedData[i],
				WorkingSetInitParams->HandoverData[i],
				&FirstId);
		}

		//Todo: async exec
		Sender->SendCreateWorkingSetParentEntity(ChildEntityIds.GetData(), Location, ChildEntityIds.Num(), &FirstId);

		PendingWorkingSetCreationRequests.Remove(RequestId);
	}
}

// Todo: be able to have automatic working set creations
uint32 UWorkingSetManager::GetWorkingSetSize(const uint32 & WorkingSetId)
{
	if (PendingWorkingSets.Contains(WorkingSetId))
	{
		return PendingWorkingSets.Find(WorkingSetId)->ActorChannels.Num();
	}

	return 0;
}

// Register working set
uint32 UWorkingSetManager::RegisterNewWorkingSet()
{
	// hacky workaround for testing
	if (PendingWorkingSets.Contains(CurrentWorkingSetId))
	{
		return CurrentWorkingSetId;
	}

	TArray<USpatialActorChannel*> ActorChannels;
	FVector Location;
	TArray<FString> PlayerWorkerId;
	TArray<TArray<uint16>> RepChangedData;
	TArray<TArray<uint16>> HandoverData;

	//CurrentWorkingSetId++;
	PendingWorkingSets.Add(CurrentWorkingSetId, { ActorChannels, Location, PlayerWorkerId, RepChangedData, HandoverData });
	return CurrentWorkingSetId;
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
	//overrides location, change with support of multiple locations
	WorkingSetData->Location = Location;

	WorkingSetData->ActorChannels.Add(Channel);
	WorkingSetData->PlayerWorkerId.Add(PlayerWorkerId);
	WorkingSetData->RepChangedData.Add(RepChanged);
	WorkingSetData->HandoverData.Add(HandoverChanged);
}

void UWorkingSetManager::AddParent(const Worker_EntityId& EntityId, const WorkingSet & ParentData)
{
	FWorkingSetSpawnData SpawnData = { EntityId, ParentData };

	if (PendingSpawningSets.Contains(SpawnData))
	{
		//already contains key
		return;
	}

	// Collect existing entities that are waiting for parent
	TArray<FWorkingSetSpawnData> StoredChildren;
	FWorkingSetSpawnData* FirstRequeuedData = nullptr;
	FWorkingSetSpawnData CurrentData;

	while (ActorSpawnQueue.Dequeue(CurrentData) && !(FirstRequeuedData && (CurrentData == *FirstRequeuedData)))
	{
		if (ParentData.ChildReferences.Contains(CurrentData.EntityId))
		{
			StoredChildren.Add(CurrentData);
		}
		else
		{
			ActorSpawnQueue.Enqueue(CurrentData);

			//uninitialized struct
			if (!FirstRequeuedData)
			{
				FirstRequeuedData = &CurrentData;
			}
		}
	}

	PendingSpawningSets.Add(SpawnData, StoredChildren);
	if (IsReadyForReplication(SpawnData))
	{
		SpawnAndCleanActors(SpawnData);
	}
}

void UWorkingSetManager::QueueActorSpawn(const Worker_EntityId & EntityId, const WorkingSet& WorkingSetData)
{
	FWorkingSetSpawnData SpawnData = { EntityId, WorkingSetData };
	if (const FWorkingSetSpawnData* SpawningActorsParentId = GetWorkingSetDataByEntityId(EntityId))
	{
		PendingSpawningSets.Find(*SpawningActorsParentId)->Add(SpawnData);
		if (IsReadyForReplication(*SpawningActorsParentId))
		{
			SpawnAndCleanActors(*SpawningActorsParentId);
		}
	}
	else
	{
		ActorSpawnQueue.Enqueue(SpawnData);
	}
}

bool UWorkingSetManager::IsCurrentWorkingSetActor(const Worker_EntityId & EntityId)
{
	return CurrentPendingWorkingSetCreations.Contains(EntityId);
}

void UWorkingSetManager::AddCurrentWorkingSetChannel(const Worker_EntityId & EntityId, USpatialActorChannel * Channel)
{
	if (CurrentWorkingSets.Contains(Channel))
	{
		return;
	}

	FWorkingSet* WorkingSet = *CurrentPendingWorkingSetCreations.Find(EntityId);
	WorkingSet->ActorChannels.Add(Channel);
	CurrentWorkingSets.Add(Channel, WorkingSet);
}


bool UWorkingSetManager::IsReadyForReplication(const FWorkingSetSpawnData & ParentSpawnData)
{
	TArray<FWorkingSetSpawnData>* ChildSpawnData = PendingSpawningSets.Find(ParentSpawnData);
	if (ParentSpawnData.WorkingSetData.ChildReferences.Num() != ChildSpawnData->Num())
	{
		return false;
	}

	for (const Worker_EntityId& ParentChildReference : ParentSpawnData.WorkingSetData.ChildReferences)
	{
		if (!ChildSpawnData->FindByPredicate([&ParentChildReference](FWorkingSetSpawnData InData)
		{
			return InData.EntityId == ParentChildReference;
		}))
		{
			return false;
		}
	}

	return true;
}

const FWorkingSetSpawnData* UWorkingSetManager::GetWorkingSetDataByEntityId(const Worker_EntityId & EntityId)
{
	for (const auto& Entry : PendingSpawningSets)
	{
		if (Entry.Key.EntityId == EntityId)
		{
			return &Entry.Key;
		}
	}
	return nullptr;
}

void UWorkingSetManager::SpawnAndCleanActors(const FWorkingSetSpawnData & ParentSpawnData)
{
	TArray<FWorkingSetSpawnData>* ChildSpawnData = PendingSpawningSets.Find(ParentSpawnData);
	TArray<USpatialActorChannel*>* PendingChildren = new TArray<USpatialActorChannel *>();

	FWorkingSet* WorkingSet = new FWorkingSet(ParentSpawnData.EntityId, *PendingChildren);

	for (const FWorkingSetSpawnData& SpawningChild : *ChildSpawnData)
	{
		CurrentPendingWorkingSetCreations.Add(SpawningChild.EntityId, WorkingSet);
		Receiver->CreateActor(SpawningChild.EntityId);
		Receiver->CleanWorkingSetAddComponents(SpawningChild.EntityId);
	}

	CurrentPendingWorkingSetCreations.Empty();
	PendingSpawningSets.Remove(ParentSpawnData);
}
