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
};

USTRUCT()
struct FWorkingSetSpawnData
{
	GENERATED_BODY()

	Worker_EntityId EntityId;
	WorkingSet WorkingSetData;

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
	FVector Location;
	TArray<FString> PlayerWorkerId;
	TArray<TArray<uint16>> RepChangedData;
	TArray<TArray<uint16>> HandoverData;
};

UCLASS()
class SPATIALGDK_API UWorkingSetManager : public UObject
{

	GENERATED_BODY()

public:

	void Init(USpatialNetDriver* NetDriver);

	// Working set initialization
	void CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const TArray<FString>& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged);
	void CreateWorkingSet(const uint32& WorkingSetId);

	// updates
	void SendPositionUpdate(const USpatialActorChannel* ActorChannel, const FVector& Loction);

	bool IsRelevantRequest(const Worker_RequestId& RequestId);
	void ProcessWorkingSet(const Worker_EntityId& FirstId, const uint32& NumOfEntities, const Worker_RequestId& RequestId);
	uint32 GetWorkingSetSize(const uint32& WorkingSetId);
	uint32 RegisterNewWorkingSet();
	void EnqueueForWorkingSet(USpatialActorChannel* Channel, const FVector& Location, const FString& PlayerWorkerId, const TArray<uint16>& RepChanged, const TArray<uint16>& HandoverChanged, const uint32& WorkingSetId);

	void AddParent(const Worker_EntityId& EntityId, const WorkingSet& ParentData);
	void QueueActorSpawn(const Worker_EntityId& EntityId, const WorkingSet& WorkingSetData);

private:

	USpatialSender* Sender;
	USpatialReceiver* Receiver;
	USpatialNetDriver* NetDriver;
	TMap<USpatialActorChannel*, FWorkingSet> CurrentWorkingSets;
	TMap<Worker_RequestId, FWorkingSetData> PendingWorkingSetCreationRequests;
	TMap<uint32, FWorkingSetData> PendingWorkingSets;

	//working set spawning
	TMap<FWorkingSetSpawnData, TArray<FWorkingSetSpawnData>> PendingSpawningSets;
	TQueue<FWorkingSetSpawnData> ActorSpawnQueue;

	bool IsReadyForReplication(const FWorkingSetSpawnData& EntityId);
	const FWorkingSetSpawnData* GetWorkingSetDataByEntityId(const Worker_EntityId& EntityId);
	void SpawnAndCleanActors(const FWorkingSetSpawnData& EntityId);

	uint32 CurrentWorkingSetId;

};
