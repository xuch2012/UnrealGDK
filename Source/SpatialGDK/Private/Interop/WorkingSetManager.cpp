// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkingSetManager.h"

void UWorkingSetManager::CreateWorkingSet(TArray<USpatialActorChannel*> Channels, const FVector& Location, const FString& PlayerWorkerId, const TArray<TArray<uint16>>& RepChanged, const TArray<TArray<uint16>>& HandoverChanged)
{
	WorkingSetData Data{ Channels, Location, PlayerWorkerId, RepChanged, HandoverChanged };

	//Reserve ids for working set and the parent
	PendingWorkingSetCreationRequests.Add(Sender->SendReserveEntityIdsRequest(Channels.Num()+1), Data);
}

bool UWorkingSetManager::IsRelevantRequest(const Worker_RequestId & RequestId)
{
	return PendingWorkingSetCreationRequests.Contains(RequestId);
}

void UWorkingSetManager::ProcessWorkingSet(const Worker_EntityId& FirstId, const uint32 & NumOfEntities, const Worker_RequestId & RequestId)
{
	if (WorkingSetData* WorkingSetInitParams = PendingWorkingSetCreationRequests.Find(RequestId))
	{
		//link the sets together on the server
		TArray<USpatialActorChannel*> ActorChannels = std::get<0>(WorkingSetInitParams);
		FWorkingSet WorkingSet = { FirstId, ActorChannels };

		Sender->SendCreateWorkingSetParentEntity(FirstId, FVector Location = std::get<FVector>(WorkingSetInitParams), NumOfEntities);

		for (int i = 0; i < Array.Num(); i++) {
			USpatialActorChannel* ActorChannel = ActorChannels[i];
			ActorChannel->SetEntityId(CurrentId);
			CurrentWorkingSets.Add(ActorChannel, WorkingSet);
			Sender->SendCreateEntityRequest(ActorChannel,
				Location,
				std::get<FString>(WorkingSetInitParams),
				std::get<RepChangedData>(WorkingSetInitParams)[i],
				std::get<HandoverData>(WorkingSetInitParams)[i],
				FirstId);
		}

		PendingWorkingSetCreationRequests.Remove(RequestId);
	}
}
