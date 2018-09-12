// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "SpatialSender.h"
#include "SpatialActorChannel.h"
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
	typedef std::tuple<TArray<USpatialActorChannel*>, FVector, FString, TArray<TArray<uint16>>, TArray<TArray<uint16>>> WorkingSetData;

	GENERATED_BODY()

public:

	// Working set initialization
	void CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const FString& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged);

private:
	

private:
	 
	USpatialSender* Sender;
	TArray<FWorkingSet> CurrentWorkingSets;
	TMap<Worker_RequestId, WorkingSetData> PendingWorkingSetCreationRequests;
};
