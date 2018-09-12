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

UCLASS()
class SPATIALGDK_API UWorkingSetManager : public UObject
{
	//useful for retrieving tuple elements
	typedef TArray<TArray<uint16>> HandoverData;
	typedef TArray<TArray<uint16>> RepChangedData;

	typedef std::tuple<TArray<USpatialActorChannel*>, FVector, FString, RepChangedData, HandoverData> WorkingSetData;

	GENERATED_BODY()

public:

	// Working set initialization
	void CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const FString& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged);
	bool IsRelevantRequest(const Worker_RequestId& RequestId);
	void ProcessWorkingSet(const Worker_EntityId& FirstId, const uint32& NumOfEntities, const Worker_RequestId& RequestId);

private:


private:
	 
	USpatialSender* Sender;
	TMap<UActorChannel*, FWorkingSet> CurrentWorkingSets;
	TMap<Worker_RequestId, WorkingSetData> PendingWorkingSetCreationRequests;
};
