// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "WorkingSetRegistry.generated.h"

using FChannelObjectPair = TPair<TWeakObjectPtr<USpatialActorChannel>, TWeakObjectPtr<UObject>>;
using FUnresolvedObjectsMap = TMap<Schema_FieldId, TSet<const UObject*>>;
struct FObjectReferences;
using FObjectReferencesMap = TMap<int32, FObjectReferences>;

struct WorkingSet
{
	TSet<AActor*> Actors;
	TSet<FUnrealObjectRef> UnresolvedRefs;
};

UCLASS()
class SPATIALGDK_API UWorkingSetRegistry : public UObject
{
	GENERATED_BODY()

public:

	void Init(USpatialNetDriver* NetDriver);
    void ReceiveWorkingSet(AActor* Entity, USpatialActorChannel* Channel, TMap<FChannelObjectPair, FObjectReferencesMap>& UnresolvedRefsMap);

    void SpawnWorkingSet(TSharedPtr<WorkingSet> ActorWorkingSet);
	void ResolveWorkingSet(UObject* Object, const FUnrealObjectRef& Ref);
	void RemoveFromWorkingSet(AActor* Entity);

private:
	void MergeWorkingSets(AActor* Current, AActor* Other);
	void MergeWorkingSets(TSharedPtr<WorkingSet> Current, TSharedPtr<WorkingSet> Other);

private:
	UPROPERTY()
	USpatialNetDriver* NetDriver;

	UPROPERTY()
	USpatialPackageMapClient* PackageMap;

	TMap<AActor*, TSharedPtr<WorkingSet>> WorkingSets;
	TMap<FUnrealObjectRef, TSharedPtr<WorkingSet>> UnresolvedWorkingSetRefs;
};
