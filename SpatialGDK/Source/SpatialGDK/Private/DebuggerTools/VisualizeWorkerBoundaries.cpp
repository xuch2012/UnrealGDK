#include "VisualizeWorkerBoundaries.h"
#include "UnrealNetwork.h"

#define CHUNCK_EDGE_LENGTH 5.0f
#define WORLD_DIMENSION_X  200
#define WORLD_DIMENSION_Z  200

AVisualizeWorkerBoundaries::AVisualizeWorkerBoundaries()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	InitGrid2D();

	BoundaryCubeGlobals::BoundaryCubeOnAuthorityGained.AddUObject(this, &AVisualizeWorkerBoundaries::UpdateGridVisibilityData);
}

void AVisualizeWorkerBoundaries::BeginPlay()
{
	Super::BeginPlay();
	
}

void AVisualizeWorkerBoundaries::OnAuthorityGained()
{
	SpawnBoundaryCubes();
}

void AVisualizeWorkerBoundaries::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVisualizeWorkerBoundaries::UpdateGridVisibilityData(int InGridIndex)
{

	uint32 centerIndex = Width * i + j;

	uint32 leftUpperIndex = Width * (i - 1) + (j - 1);
	uint32 upperIndex = Width * (i - 1) + j;
	uint32 rightUpperIndex = Width * (i - 1) + (j + 1);

	uint32 leftIndex = Width * (i)+(j - 1);
	uint32 rightIndex = Width * (i)+(j + 1);

	uint32 leftLowerIndex = Width * (i + 1) + (j - 1);
	uint32 lowerIndex = Width * (i + 1) + j;
	uint32 rightLowerIndex = Width * (i + 1) + (j + 1);

	TArray< uint32> CompareTo{ leftUpperIndex, upperIndex, rightUpperIndex, leftIndex, rightIndex, leftLowerIndex, lowerIndex, rightLowerIndex };

	CompareChuncks(centerIndex, CompareTo);


	Grid2D[InGridIndex].DebugCube.Get()->Server_SetVisibility()
}

void AVisualizeWorkerBoundaries::InitGrid2D()
{
	const int Width	   = WORLD_DIMENSION_X / CHUNCK_EDGE_LENGTH;
	const int Height   = WORLD_DIMENSION_Z / CHUNCK_EDGE_LENGTH;
	const float Offset = CHUNCK_EDGE_LENGTH / 2 * 100;
	const float Scalar = CHUNCK_EDGE_LENGTH * 100; // meters to centimeters

	Grid2D.SetNum(Width * Height);

	int index = 0;
	for (int i = Width / 2 * -1; i < Width / 2; ++i)
	{
		for (int j = Height / 2 * -1; j < Height / 2; ++j)
		{
			Grid2D[index].ObjectColor = FColor::Black;
			Grid2D[index].Position = FVector(i * Scalar + Offset, j * Scalar + Offset, 50);
			Grid2D[index].DebugCube = nullptr;
			Grid2D[index].bDeleteAfterProcessed = true;
			++index;
		}
	}
}

void AVisualizeWorkerBoundaries::UpdateGridData(FColor InColor, int InIndex)
{
	Grid2D[InIndex].ObjectColor = InColor;
}

void AVisualizeWorkerBoundaries::SetDeleteAfterProcessed(int InIndex, bool bToDelete)
{
	Grid2D[InIndex].bDeleteAfterProcessed = bToDelete;
}

void AVisualizeWorkerBoundaries::SwitchOffUnusedBoundaryCubes()
{
	const int Width = WORLD_DIMENSION_X / CHUNCK_EDGE_LENGTH;
	const int Height = WORLD_DIMENSION_Z / CHUNCK_EDGE_LENGTH;
	// top row
	for (int i = 0; i < Width; ++i)
	{
		Grid2D[i].bDeleteAfterProcessed = false;
	}

	// bottom row
	for (int i = Width * Height - Width; i < Width * Height; ++i)
	{
		Grid2D[i].bDeleteAfterProcessed = false;
	}

	// first column
	for (int i = 0; i < Height; ++i)
	{
		Grid2D[Width * i].bDeleteAfterProcessed = false;
	}

	// last column
	for (int i = Height - 1; i < Width * Height; i += Height)
	{
		Grid2D[i].bDeleteAfterProcessed = false;
	}

	for (int i = 1; i < Width - 1; ++i)
	{
		for (int j = 1; j < Height - 1; ++j)
		{
			uint32 centerIndex = Width * i + j;

			uint32 leftUpperIndex = Width * (i - 1) + (j - 1);
			uint32 upperIndex = Width * (i - 1) + j;
			uint32 rightUpperIndex = Width * (i - 1) + (j + 1);

			uint32 leftIndex = Width * (i)+(j - 1);
			uint32 rightIndex = Width * (i)+(j + 1);

			uint32 leftLowerIndex = Width * (i + 1) + (j - 1);
			uint32 lowerIndex = Width * (i + 1) + j;
			uint32 rightLowerIndex = Width * (i + 1) + (j + 1);

			TArray< uint32> CompareTo{ leftUpperIndex, upperIndex, rightUpperIndex, leftIndex, rightIndex, leftLowerIndex, lowerIndex, rightLowerIndex };

			CompareChuncks(centerIndex, CompareTo);
		}
	}

	for (FDebugDataS& EntryIn : Grid2D)
	{
		if (EntryIn.bDeleteAfterProcessed && EntryIn.DebugCube.IsValid())
		{
			EntryIn.DebugCube->Server_SetVisibility(false);
		}
	}
}

void AVisualizeWorkerBoundaries::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVisualizeWorkerBoundaries, Grid2D);
}
void AVisualizeWorkerBoundaries::CompareChuncks(const uint32& CenterCell, TArray<uint32> CompareTo)
{
	for (auto& EntryIn : CompareTo)
	{
		if (Grid2D[CenterCell].ObjectColor != Grid2D[EntryIn].ObjectColor)
		{
			Grid2D[CenterCell].bDeleteAfterProcessed = false;
			Grid2D[EntryIn].bDeleteAfterProcessed = false;
		}
	}
}

bool AVisualizeWorkerBoundaries::SpawnBoundaryCubes_Validate()
{
	return true;
}

void AVisualizeWorkerBoundaries::SpawnBoundaryCubes_Implementation()
{
	for (int i = 0; i < Grid2D.Num(); ++i)
	{
		Grid2D[i].DebugCube = Cast<ABoundaryCube>(GetWorld()->SpawnActor(ABoundaryCube::StaticClass(), &(Grid2D[i].Position)));
		Grid2D[i].DebugCube->SetGridIndex(i);
	}
}
