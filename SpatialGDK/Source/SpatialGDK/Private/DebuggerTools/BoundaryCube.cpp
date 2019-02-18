// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "BoundaryCube.h"
#include "PackageName.h"
#include "ConstructorHelpers.h"
#include "UnrealNetwork.h"

// Sets default values
ABoundaryCube::ABoundaryCube()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	SceneComponent->SetupAttachment(RootComponent);

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(TEXT("StaticMesh'/Game/Geometry/Meshes/1M_Cube.1M_Cube'"));
	StaticMeshComponent->SetStaticMesh(BoxMesh.Object);
	StaticMeshComponent->SetupAttachment(SceneComponent);

	bReplicates = true;
	bIsVisible = true;
}

// Called when the game starts or when spawned
void ABoundaryCube::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ABoundaryCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABoundaryCube::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoundaryCube, bIsVisible);
}

void ABoundaryCube::OnAuthorityGained()
{
	Server_OnAuthorityGained();
}
bool Server_OnAuthorityGained_Validate()
{
	return true;
}
void Server_OnAuthorityGained_Implementation()
{
	BoundaryCubeOnAuthorityGained.Broadcast(GridIndex);
}

void ABoundaryCube::SetGridIndex(const int& InGridIndex)
{
	GridIndex = InGridIndex;
}

void ABoundaryCube::OnRep_IsVisible()
{
	StaticMeshComponent->SetVisibility(bIsVisible);
}

bool ABoundaryCube::Server_SetVisibility_Validate(bool bInIsVisible)
{
	return true;
}

void ABoundaryCube::Server_SetVisibility_Implementation(bool bInIsVisible)
{
	CrossServer_SetVisibility(bInIsVisible);
}

bool ABoundaryCube::CrossServer_SetVisibility_Validate(bool bInIsVisible)
{
	return true;
}

void ABoundaryCube::CrossServer_SetVisibility_Implementation(bool bInIsVisible)
{
	Destroy();
	//bIsVisible = bInIsVisible;
}
