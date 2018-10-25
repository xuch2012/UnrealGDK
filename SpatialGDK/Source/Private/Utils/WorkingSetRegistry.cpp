// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "WorkingSetRegistry.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorkingSetRegistry, Log, All);
DEFINE_LOG_CATEGORY(LogWorkingSetRegistry);


void UWorkingSetRegistry::Init(USpatialNetDriver* InNetDriver)
{
	NetDriver = InNetDriver;
	PackageMap = InNetDriver->PackageMap;
}

void UWorkingSetRegistry::MergeWorkingSets(TSharedPtr<WorkingSet> Current, TSharedPtr<WorkingSet> Other)
{
	if (Current == Other)
	{
		return;
	}

	for (auto Actor : Other->Actors)
	{
		Current->Actors.Add(Actor);
		WorkingSets.Add(Actor, Current);
	}

	for (auto UnresolvedRef : Other->UnresolvedRefs)
	{
		Current->UnresolvedRefs.Add(UnresolvedRef);
		UnresolvedWorkingSetRefs.Add(UnresolvedRef, Current);
	}
}

void UWorkingSetRegistry::MergeWorkingSets(AActor* Current, AActor* Other)
{
	if (Current == nullptr || Other == nullptr || Current == Other)
	{
		return;
	}

	TSharedPtr<WorkingSet>* CurrentWorkingSet = WorkingSets.Find(Current);
	TSharedPtr<WorkingSet>* OtherWorkingSet = WorkingSets.Find(Other);
		
	if (CurrentWorkingSet && OtherWorkingSet)
	{
		MergeWorkingSets(*CurrentWorkingSet, *OtherWorkingSet);
		WorkingSets.Add(Other, *CurrentWorkingSet);
	}
	else if (CurrentWorkingSet)
	{
		WorkingSets.Add(Other, *CurrentWorkingSet);
	}
	else if (OtherWorkingSet)
	{
		WorkingSets.Add(Current, *OtherWorkingSet);
	}
	else
	{
		TSharedPtr<WorkingSet> NewWorkingSet = WorkingSets.Add(Current, MakeShared<WorkingSet>());
		WorkingSets.Add(Other, NewWorkingSet);
	}
}

void UWorkingSetRegistry::SpawnWorkingSet(TSharedPtr<WorkingSet> ActorWorkingSet)
{
	for (auto Actor : ActorWorkingSet->Actors)
	{
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		Actor->UpdateOverlaps();
		WorkingSets.Remove(Actor);
	}
}

void UWorkingSetRegistry::ReceiveWorkingSet(AActor* Entity, USpatialActorChannel* Channel, TMap<FChannelObjectPair, FObjectReferencesMap>& UnresolvedRefsMap)
{
	// We only want to dispatch play to an Actor if its entire ownership tree has arrived.
	// If the Owner/NetOwner/Children are not null it means the Actor for those have already arrived,
	// so the trees are all merged together.

	AActor* NetOwner = const_cast<AActor*>(Entity->GetNetOwner());
	if (NetOwner && Entity != NetOwner)
	{
		MergeWorkingSets(Entity, NetOwner);
	}

	AActor* Owner = Entity->GetOwner();
	if (Owner && Entity != Owner)
	{
		MergeWorkingSets(Entity, Owner);
	}

	if (!WorkingSets.Contains(Entity))
	{
		WorkingSets.Add(Entity, MakeShared<WorkingSet>());
	}

	TSharedPtr<WorkingSet> EntityWorkingSet = WorkingSets[Entity];

	if (!EntityWorkingSet.IsValid())
	{
		UE_LOG(LogSpatialReceiver, Log, TEXT("Working set is invalid for actor: %s"), *Entity->GetName());
		WorkingSets.Add(Entity, MakeShared<WorkingSet>());
	}

	EntityWorkingSet->Actors.Add(Entity);

	TArray<AActor*> Children;
	Entity->GetAllChildActors(Children, false);
	for (auto Child : Children)
	{
		if (Child)
		{
			MergeWorkingSets(Entity, Child);
		}
	}

	// Any unresolved references in the Parent and Children field of the Actor means the entire tree has
	// not yet arrived.
	FChannelObjectPair ChannelObjectPair(Channel, Channel->Actor);
	auto ObjectRefMap = UnresolvedRefsMap.Find(ChannelObjectPair);
	if (ObjectRefMap)
	{
		auto OwnerOffset = AActor::__PPO__Owner();
		auto ChildrenOffset = STRUCT_OFFSET(AActor, Children);

		auto UnresolvedParentRefs = ObjectRefMap->Find(OwnerOffset);
		auto UnresolvedChildrenRefs = ObjectRefMap->Find(ChildrenOffset);

		if (UnresolvedParentRefs)
		{
			for (FUnrealObjectRef ParentRef : UnresolvedParentRefs->UnresolvedRefs)
			{
				auto UnresolvedWorkingSet = UnresolvedWorkingSetRefs.Find(ParentRef);
				if (UnresolvedWorkingSet)
				{
					MergeWorkingSets(EntityWorkingSet, *UnresolvedWorkingSet);
				}
				else
				{
					EntityWorkingSet->UnresolvedRefs.Add(ParentRef);
					UnresolvedWorkingSetRefs.Add(ParentRef, EntityWorkingSet);
				}
			}
		}

		if (UnresolvedChildrenRefs)
		{
			for (auto ChildRef : UnresolvedChildrenRefs->UnresolvedRefs)
			{
				auto UnresolvedWorkingSet = UnresolvedWorkingSetRefs.Find(ChildRef);
				if (UnresolvedWorkingSet)
				{
					MergeWorkingSets(EntityWorkingSet, *UnresolvedWorkingSet);
				}
				else
				{
					EntityWorkingSet->UnresolvedRefs.Add(ChildRef);
					UnresolvedWorkingSetRefs.Add(ChildRef, EntityWorkingSet);
				}
			}
		}
	}

	// If there are no unresolved references, the actor should be safe to spawn.
	if (WorkingSets[Entity]->UnresolvedRefs.Num() == 0)
	{
		SpawnWorkingSet(WorkingSets[Entity]);
	}
}

void UWorkingSetRegistry::ResolveWorkingSet(UObject* Object, const FUnrealObjectRef& Ref)
{
	TSharedPtr<WorkingSet>* ActorWorkingSet = UnresolvedWorkingSetRefs.Find(Ref);
	if (ActorWorkingSet)
	{
		(*ActorWorkingSet)->UnresolvedRefs.Remove(Ref);
		if (AActor* Actor = Cast<AActor>(Object))
		{
			(*ActorWorkingSet)->Actors.Add(Actor);
			WorkingSets.Add(Actor, (*ActorWorkingSet));
		}

		if ((*ActorWorkingSet)->UnresolvedRefs.Num() == 0)
		{
			SpawnWorkingSet(*ActorWorkingSet);
		}

		UnresolvedWorkingSetRefs.Remove(Ref);
	}
}

void UWorkingSetRegistry::RemoveFromWorkingSet(AActor* Entity)
{
	FUnrealObjectRef Ref = PackageMap->GetUnrealObjectRefFromNetGUID(NetDriver->GetGUIDForActor(Entity));
	TSharedPtr<WorkingSet>* EntityWorkingSet = WorkingSets.Find(Entity);
	if (EntityWorkingSet)
	{
		(*EntityWorkingSet)->Actors.Remove(Entity);
		(*EntityWorkingSet)->UnresolvedRefs.Add(Ref);
		UnresolvedWorkingSetRefs.Add(Ref, *EntityWorkingSet);
		WorkingSets.Remove(Entity);
	}
}
