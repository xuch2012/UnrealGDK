// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoundaryCube.h"
#include "VisualizeWorkerBoundaries.generated.h"

USTRUCT(BlueprintType)
struct FDebugDataS
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite)
	FColor ObjectColor;

	UPROPERTY(BlueprintReadWrite)
	FVector Position;

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<ABoundaryCube> DebugCube;

	UPROPERTY(BlueprintReadWrite)
	bool bDeleteAfterProcessed;
};

UCLASS(SpatialType=(Singleton))
class AVisualizeWorkerBoundaries : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVisualizeWorkerBoundaries();
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);
	virtual void OnAuthorityGained();
	UPROPERTY(BlueprintReadWrite, Replicated)
	TArray<FDebugDataS> Grid2D;

	void InitGrid2D();
	void UpdateGridVisibilityData(int InGridIndex);
	void UpdateGridData(FColor InColor, int InIndex);
	
	void AssignBoundaryCubePointer(int InIndex, ABoundaryCube* InCube);
	
	void SetDeleteAfterProcessed(int InIndex, bool bToDelete);
	
	void SwitchOffUnusedBoundaryCubes();
	
	void CompareChuncks(const uint32& CenterCell, TArray< uint32> CompareTo);

	UFUNCTION(Server, Reliable, WithValidation)
	void SpawnBoundaryCubes();
};
