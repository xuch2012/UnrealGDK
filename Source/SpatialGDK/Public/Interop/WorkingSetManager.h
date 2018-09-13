// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialActorChannel.h"
#include "SpatialSender.h"
#include <tuple>
#include "WorkingSetManager.generated.h"

class USpatialSender;

USTRUCT()
struct FWorkingSet
{
	GENERATED_BODY()

	Worker_EntityId ParentId;
	TArray<USpatialActorChannel*> ActorChannels;
};


// Assumes same location and player worker id for now
USTRUCT()
struct FWorkingSetData
{
	GENERATED_BODY()

	TArray<USpatialActorChannel*> ActorChannels;
	FVector Location;
	FString PlayerWorkerId;
	TArray<TArray<uint16>> RepChangedData;
	TArray<TArray<uint16>> HandoverData;
};
UCLASS()
class SPATIALGDK_API UWorkingSetManager : public UObject
{

	GENERATED_BODY()

public:
	UWorkingSetManager();

	// Working set initialization
	void CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const FString& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged);
	void CreateWorkingSet(const uint32& WorkingSetId);
	bool IsRelevantRequest(const Worker_RequestId& RequestId);
	void ProcessWorkingSet(const Worker_EntityId& FirstId, const uint32& NumOfEntities, const Worker_RequestId& RequestId);
	uint32 RegisterNewWorkingSet();
	void EnqueueForWorkingSet(USpatialActorChannel* Channel, const FVector& Location, const FString& PlayerWorkerId, const TArray<uint16>& RepChanged, const TArray<uint16>& HandoverChanged, const uint32& WorkingSetId);

private:


private:
	 
	USpatialSender* Sender;
	TMap<UActorChannel*, FWorkingSet> CurrentWorkingSets;
	TMap<Worker_RequestId, FWorkingSetData> PendingWorkingSetCreationRequests;
	TMap<uint32, FWorkingSetData> PendingWorkingSets;
	uint32 CurrentWorkingSetId;
};
