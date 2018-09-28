// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialActorChannel.h"
#include "SpatialSender.h"
#include "SpatialReceiver.h"
#include "Schema/StandardLibrary.h"
#include "WorkingSetManager.generated.h"

class USpatialSender;
class USpatialNetDriver;
class USpatialReceiver;

USTRUCT()
struct FWorkingSet
{
	GENERATED_BODY()

	Worker_EntityId ParentId;
	TArray<USpatialActorChannel*> ActorChannels;

	FWorkingSet()
	{}

	FWorkingSet(const Worker_EntityId& EntityId, const TArray<USpatialActorChannel*>& Channels) : ParentId(EntityId), ActorChannels(Channels)
	{}
};

USTRUCT()
struct FWorkingSetSpawnData
{
	GENERATED_BODY()

	Worker_EntityId EntityId;
	improbable::WorkingSet WorkingSetData;

	bool operator==(const FWorkingSetSpawnData& Other) const
	{
		return EntityId == Other.EntityId;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWorkingSetSpawnData& data)
{
	return FCrc::MemCrc32(&data, sizeof(FWorkingSetSpawnData));
}


// Assumes same location and player worker id for now
USTRUCT()
struct FWorkingSetData
{
	GENERATED_BODY()

	TArray<USpatialActorChannel*> ActorChannels;
	TArray<FString> PlayerWorkerId;
};

UCLASS()
class SPATIALGDK_API UWorkingSetManager : public UObject
{

	GENERATED_BODY()

public:

	void Init(USpatialNetDriver* NetDriver);

	// Working set initialization
	void CreateWorkingSet(const uint32& WorkingSetId);

	// updates
	void SendPositionUpdate(const USpatialActorChannel* ActorChannel, const FVector& Loction);
	bool IsQueuedEntity(const Worker_EntityId& EntityId);


	bool IsRelevantRequest(const Worker_RequestId& RequestId);
	void ProcessWorkingSet(const Worker_EntityId& FirstId, const uint32& NumOfEntities, const Worker_RequestId& RequestId);
	uint32 GetWorkingSetSize(const uint32& WorkingSetId);
	uint32 RegisterNewWorkingSet();
	void EnqueueForWorkingSet(USpatialActorChannel* Channel, const FString& PlayerWorkerId, const uint32& WorkingSetId);

	void AddParent(const Worker_EntityId& EntityId, const improbable::WorkingSet& ParentData);
	void QueueActorSpawn(const Worker_EntityId& EntityId, const improbable::WorkingSet& WorkingSetData);

	bool IsCurrentWorkingSetActor(const Worker_EntityId& EntityId);
	void AddCurrentWorkingSetChannel(const Worker_EntityId& EntityId, USpatialActorChannel* Channel);
private:

	USpatialSender* Sender;
	USpatialReceiver* Receiver;
	USpatialNetDriver* NetDriver;
	TMap<USpatialActorChannel*, FWorkingSet*> CurrentWorkingSets;
	TMap<Worker_EntityId, FWorkingSet*> CurrentPendingWorkingSetCreations;
	TMap<Worker_RequestId, FWorkingSetData> PendingWorkingSetCreationRequests;
	TMap<uint32, FWorkingSetData> PendingWorkingSets;

	//working set spawning
	TMap<FWorkingSetSpawnData, TArray<FWorkingSetSpawnData>> PendingSpawningSets;
	TQueue<FWorkingSetSpawnData> ActorSpawnQueue;

	bool IsReadyForReplication(const FWorkingSetSpawnData& EntityId);
	const FWorkingSetSpawnData* GetWorkingSetParentByEntityId(const Worker_EntityId& EntityId);
	void SpawnAndCleanActors(const FWorkingSetSpawnData& EntityId);

	uint32 CurrentWorkingSetId;

};
