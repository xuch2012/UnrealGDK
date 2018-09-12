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
		TArray<USpatialActorChannel*> ActorChannels = std::get<0>(*WorkingSetInitParams);
		FWorkingSet WorkingSet = { FirstId, ActorChannels };
		FVector Location = std::get<FVector>(*WorkingSetInitParams);

		Sender->SendCreateWorkingSetParentEntity(FirstId, Location, NumOfEntities);

		for (uint32 i = 0; i < NumOfEntities; i++) {
			USpatialActorChannel* ActorChannel = ActorChannels[i];
			ActorChannel->SetEntityId(FirstId+i+1);
			CurrentWorkingSets.Add(ActorChannel, WorkingSet);
			Sender->SendCreateEntityRequest(ActorChannel,
				Location,
				std::get<FString>(*WorkingSetInitParams),
				std::get<3>(*WorkingSetInitParams)[i],
				std::get<4>(*WorkingSetInitParams)[i],
				&FirstId);
		}

		PendingWorkingSetCreationRequests.Remove(RequestId);
	}
}
