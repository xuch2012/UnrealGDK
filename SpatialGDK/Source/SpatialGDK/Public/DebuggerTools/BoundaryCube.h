// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoundaryCube.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FBoundaryCubeOnAuthorityGained, int, InGridIndex);

namespace BoundaryCubeGlobals {
	static FBoundaryCubeOnAuthorityGained BoundaryCubeOnAuthorityGained;
}

UCLASS()
class ABoundaryCube : public AActor
{
	GENERATED_BODY()

public:
	// Sets defauvalues for this actor's properties
	ABoundaryCube();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void SetGridIndex(const int& InGridIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetVisibility(bool bInIsVisible);

	UFUNCTION()
	void OnRep_IsVisible();

	UPROPERTY(ReplicatedUsing = OnRep_IsVisible)
	bool bIsVisible;

	virtual void OnAuthorityGained();
protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnVisibilityUpdated(bool bInIsVisible);

private:
	UFUNCTION(CrossServer, Reliable, WithValidation)
	void CrossServer_SetVisibility(bool bInIsVisible);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_OnAuthorityGained();
private:
	UPROPERTY()
	int GridIndex;

	UPROPERTY(EditAnywhere)
	USceneComponent* SceneComponent;

	UPROPERTY(EditAnywhere)
		UStaticMeshComponent* StaticMeshComponent;
};
