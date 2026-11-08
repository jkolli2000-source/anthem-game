#include "JungleWorld.h"
#include "CoolingVolume.h"
#include "EnemyEncounter.h"
#include "EngineUtils.h"
#include "FlightTestArena.h"
#include "JavelinHeatComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr float Meter = 100.f;

	// Start clearing, where the shooting range and first enemy camp are. Meters.
	constexpr float ClearingRadius = 170.f;
	constexpr float ClearingBlend = 110.f;

	// Terrain sections, each with its own color.
	enum ETerrainSection : int32 { Section_Grass, Section_Mud, Section_Rock, Section_Count };

	/** Fractal noise, roughly -1 to 1. */
	float Fbm(float X, float Y, int32 Octaves, const FVector2D& Offset)
	{
		float Sum = 0.f;
		float Amplitude = 1.f;
		float Frequency = 1.f;
		float Total = 0.f;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += FMath::PerlinNoise2D(FVector2D(X * Frequency, Y * Frequency) + Offset * (Octave + 1)) * Amplitude;
			Total += Amplitude;
			Amplitude *= 0.5f;
			Frequency *= 2.03f;
		}
		return Sum / Total;
	}

	/** Sharp-crested noise for mountain ridges, 0 to 1. */
	float Ridged(float X, float Y, int32 Octaves, const FVector2D& Offset)
	{
		float Sum = 0.f;
		float Amplitude = 1.f;
		float Frequency = 1.f;
		float Total = 0.f;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			const float Crest = 1.f - FMath::Abs(FMath::PerlinNoise2D(FVector2D(X * Frequency, Y * Frequency) + Offset * (Octave + 1)));
			Sum += Crest * Crest * Amplitude;
			Total += Amplitude;
			Amplitude *= 0.5f;
			Frequency *= 2.1f;
		}
		return Sum / Total;
	}

	/** Like FMath::Min, but rounds off the corner where the two meet. */
	float SmoothMin(float A, float B, float Smoothing)
	{
		const float H = FMath::Clamp(0.5f + 0.5f * (B - A) / Smoothing, 0.f, 1.f);
		return FMath::Lerp(B, A, H) - Smoothing * H * (1.f - H);
	}
}

AJungleWorld::AJungleWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = ShapeMaterial.Object;

	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	Terrain->SetupAttachment(RootComponent);
	Terrain->SetCollisionProfileName(TEXT("BlockAll"));
	Terrain->bUseAsyncCooking = false;

	Water = MakeInstances(TEXT("Water"), CubeMesh.Object, false, false);
	Waterfalls = MakeInstances(TEXT("Waterfalls"), CubeMesh.Object, false, false);
	Rocks = MakeInstances(TEXT("Rocks"), CubeMesh.Object, true, true);
	Ruins = MakeInstances(TEXT("Ruins"), CubeMesh.Object, true, true);
	Trunks = MakeInstances(TEXT("Trunks"), CylinderMesh.Object, true, true);
	GiantWood = MakeInstances(TEXT("GiantWood"), CubeMesh.Object, true, true);
	GiantPlatforms = MakeInstances(TEXT("GiantPlatforms"), CylinderMesh.Object, true, true);
	LeavesDark = MakeInstances(TEXT("LeavesDark"), SphereMesh.Object, false, true);
	Leaves = MakeInstances(TEXT("Leaves"), SphereMesh.Object, false, true);
	LeavesLight = MakeInstances(TEXT("LeavesLight"), SphereMesh.Object, false, true);
	PalmFronds = MakeInstances(TEXT("PalmFronds"), CubeMesh.Object, false, true);
	Vines = MakeInstances(TEXT("Vines"), CylinderMesh.Object, false, false);
	Ferns = MakeInstances(TEXT("Ferns"), CubeMesh.Object, false, false);
	Bushes = MakeInstances(TEXT("Bushes"), SphereMesh.Object, false, false);
	Flowers = MakeInstances(TEXT("Flowers"), ConeMesh.Object, false, false);

	for (int32 Index = 0; Index < WaterfallCount; ++Index)
	{
		UCoolingZoneComponent* Cooling = CreateDefaultSubobject<UCoolingZoneComponent>(*FString::Printf(TEXT("WaterfallCooling%d"), Index));
		Cooling->SetupAttachment(RootComponent);
		Cooling->CoolingPerSecond = 80.f;
		WaterfallCooling.Add(Cooling);
	}
}

UHierarchicalInstancedStaticMeshComponent* AJungleWorld::MakeInstances(FName Name, UStaticMesh* Mesh, bool bCollides, bool bCastShadow)
{
	UHierarchicalInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
	Component->SetupAttachment(RootComponent);
	Component->SetStaticMesh(Mesh);
	Component->SetCastShadow(bCastShadow);
	if (bCollides)
	{
		Component->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	return Component;
}

void AJungleWorld::SetColor(UHierarchicalInstancedStaticMeshComponent* Component, const FLinearColor& Color)
{
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Component->SetMaterial(0, Material);
}

void AJungleWorld::AddShape(UHierarchicalInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	// The engine shapes are 1 m across, so the scale is the size in meters.
	QueuedShapes.FindOrAdd(Component).Add(FTransform(Rotation, Center * Meter, Size));
}

void AJungleWorld::AddQueuedShapes()
{
	for (TPair<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>>& Pair : QueuedShapes)
	{
		Pair.Key->AddInstances(Pair.Value, false);
	}
	QueuedShapes.Empty();
}

void AJungleWorld::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TArray<UHierarchicalInstancedStaticMeshComponent*> AllInstances = { Water, Waterfalls, Rocks, Ruins, Trunks, GiantWood, GiantPlatforms,
		LeavesDark, Leaves, LeavesLight, PalmFronds, Vines, Ferns, Bushes, Flowers };
	for (UHierarchicalInstancedStaticMeshComponent* Component : AllInstances)
	{
		Component->ClearInstances();
	}
	QueuedShapes.Empty();
	GiantTreeSpots.Empty();

	SetColor(Water, WaterColor);
	SetColor(Waterfalls, WaterfallColor);
	SetColor(Rocks, RockColor);
	SetColor(Ruins, RuinColor);
	SetColor(Trunks, BarkColor);
	SetColor(GiantWood, BarkColor * 1.15f);
	SetColor(GiantPlatforms, FLinearColor::LerpUsingHSV(BarkColor, LeafColor, 0.6f));
	SetColor(LeavesDark, LeafDarkColor);
	SetColor(Leaves, LeafColor);
	SetColor(LeavesLight, LeafLightColor);
	SetColor(PalmFronds, FLinearColor::LerpUsingHSV(LeafColor, LeafLightColor, 0.5f));
	SetColor(Vines, FLinearColor::LerpUsingHSV(BarkColor, LeafDarkColor, 0.5f));
	SetColor(Ferns, LeafColor * 1.25f);
	SetColor(Bushes, LeafDarkColor * 1.4f);
	SetColor(Flowers, FlowerColor);

	// Small plants pop out of view up close; trees much farther away, in the fog.
	const int32 TreeCull = FMath::RoundToInt(TreeDrawDistance * Meter);
	const int32 PlantCull = FMath::RoundToInt(PlantDrawDistance * Meter);
	for (UHierarchicalInstancedStaticMeshComponent* Component : { Trunks.Get(), LeavesDark.Get(), Leaves.Get(), LeavesLight.Get(), PalmFronds.Get(), Vines.Get() })
	{
		Component->SetCullDistances(FMath::RoundToInt(TreeCull * 0.85f), TreeCull);
	}
	Ferns->SetCullDistances(FMath::RoundToInt(PlantCull * 0.8f), PlantCull);
	Flowers->SetCullDistances(FMath::RoundToInt(PlantCull * 0.8f), PlantCull);
	Bushes->SetCullDistances(FMath::RoundToInt(PlantCull * 1.3f), FMath::RoundToInt(PlantCull * 1.6f));

	ComputeGrid();
	BuildTerrain();
	BuildWater();
	BuildWaterfalls();
	BuildRuins();
	BuildCaves();

	FRandomStream Random(RandomSeed);
	BuildRocks(Random);
	BuildGiantTrees(Random);
	BuildForest(Random);
	BuildUndergrowth(Random);

	AddQueuedShapes();
}

// ---------------------------------------------------------------------------------------------------------------------
// Terrain shape

void AJungleWorld::ComputeGrid()
{
	GridSpacing = TerrainSpacing;
	GridPoints = FMath::Max(2, FMath::FloorToInt(WorldSize / GridSpacing) + 1);
	HalfSize = (GridPoints - 1) * GridSpacing * 0.5f;

	PlanSites();

	Heights.SetNumUninitialized(GridPoints * GridPoints);
	for (int32 XIndex = 0; XIndex < GridPoints; ++XIndex)
	{
		for (int32 YIndex = 0; YIndex < GridPoints; ++YIndex)
		{
			Heights[XIndex * GridPoints + YIndex] = ComputeHeight(XIndex * GridSpacing - HalfSize, YIndex * GridSpacing - HalfSize);
		}
	}
}

void AJungleWorld::PlanSites()
{
	FRandomStream Random(RandomSeed);
	NoiseOffset = FVector2D(Random.FRandRange(-500.f, 500.f), Random.FRandRange(-500.f, 500.f));

	FlatSites.Reset();
	WaterfallSites.Reset();
	RuinSites.Reset();
	CaveSites.Reset();
	Rivers.Reset();

	// Two winding rivers: one west to east north of the start, one south to north west of it. They're
	// kept out of the start clearing.
	auto AddRiver = [&](bool bAlongX, float Offset)
	{
		FRiver& River = Rivers.AddDefaulted_GetRef();
		River.bAlongX = bAlongX;
		River.Offset = Offset;
		const float Amplitudes[] = { 110.f, 45.f, 14.f };
		const float Wavelengths[] = { 900.f, 370.f, 140.f };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			River.Amplitudes[Index] = Amplitudes[Index] * Random.FRandRange(0.8f, 1.1f);
			River.Wavelengths[Index] = Wavelengths[Index] * Random.FRandRange(0.85f, 1.15f);
			River.Phases[Index] = Random.FRandRange(0.f, 2.f * UE_PI);
		}
	};
	AddRiver(true, 430.f);
	AddRiver(false, -540.f);

	// Snaps a point onto the terrain grid, so a cliff edge lines up with a row of points and drops straight down.
	auto SnapToGrid = [this](float Value)
	{
		return FMath::RoundToFloat((Value + HalfSize) / GridSpacing) * GridSpacing - HalfSize;
	};

	// Waterfalls: the first one is in view of the start.
	WaterfallSites.Add({ FVector2f(SnapToGrid(330.f), SnapToGrid(-170.f)), FVector2f(-1.f, 0.f), 55.f, 24.f });
	WaterfallSites.Add({ FVector2f(SnapToGrid(-900.f), SnapToGrid(950.f)), FVector2f(0.f, 1.f), 80.f, 30.f });
	WaterfallSites.Add({ FVector2f(SnapToGrid(1200.f), SnapToGrid(-320.f)), FVector2f(0.f, -1.f), 65.f, 26.f });

	// Ruins with enemy camps, each a bit tougher than the last.
	RuinSites.Add({ FVector2f(950.f, -720.f), 42.f, 4, 2 });
	RuinSites.Add({ FVector2f(-1200.f, -500.f), 45.f, 5, 2 });
	RuinSites.Add({ FVector2f(350.f, 1250.f), 48.f, 5, 3 });
	RuinSites.Add({ FVector2f(100.f, -1350.f), 55.f, 6, 4 });

	// Rock tunnels: one just south of the start, one far to the north-west.
	CaveSites.Add({ FVector2f(40.f, -340.f), 0.f, 110.f });
	CaveSites.Add({ FVector2f(-1300.f, 1300.f), 45.f, 140.f });

	// Drop anything that doesn't fit a smaller world.
	const float Limit = HalfSize - 150.f;
	auto Fits = [Limit](const FVector2f& Point) { return FMath::Abs(Point.X) < Limit && FMath::Abs(Point.Y) < Limit; };
	WaterfallSites.RemoveAll([&](const FWaterfallSite& Site) { return !Fits(Site.Edge); });
	RuinSites.RemoveAll([&](const FRuinSite& Site) { return !Fits(Site.Center); });
	CaveSites.RemoveAll([&](const FCaveSite& Site) { return !Fits(Site.Center); });

	// Flatten the start clearing, the ruins and the caves. Their heights come from the land before flattening.
	FlatSites.Add({ FVector2f(0.f, 0.f), ClearingRadius, ClearingBlend, 0.f });
	for (const FRuinSite& Site : RuinSites)
	{
		const float Height = FMath::Max(ComputeHeight(Site.Center.X, Site.Center.Y), WaterLevel + 3.f);
		FlatSites.Add({ Site.Center, Site.Radius, 60.f, Height });
	}
	for (const FCaveSite& Site : CaveSites)
	{
		const float Height = FMath::Max(ComputeHeight(Site.Center.X, Site.Center.Y), WaterLevel + 3.f);
		FlatSites.Add({ Site.Center, Site.Length * 0.5f + 25.f, 70.f, Height });
	}
}

float AJungleWorld::DistanceToRiver(float X, float Y) const
{
	float Nearest = UE_BIG_NUMBER;
	for (const FRiver& River : Rivers)
	{
		const float Along = River.bAlongX ? X : Y;
		const float Across = River.bAlongX ? Y : X;
		float Center = River.Offset;
		float Slope = 0.f;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float Wave = 2.f * UE_PI / River.Wavelengths[Index];
			Center += River.Amplitudes[Index] * FMath::Sin(Along * Wave + River.Phases[Index]);
			Slope += River.Amplitudes[Index] * Wave * FMath::Cos(Along * Wave + River.Phases[Index]);
		}
		// Distance across the river, corrected for how much it bends here.
		Nearest = FMath::Min(Nearest, FMath::Abs(Across - Center) / FMath::Sqrt(1.f + Slope * Slope));
	}
	return Nearest;
}

float AJungleWorld::ComputeNaturalHeight(float X, float Y) const
{
	// Rolling jungle hills.
	float Height = 6.f + Fbm(X / 380.f, Y / 380.f, 5, NoiseOffset) * HillHeight;

	// Mountain ranges in some areas, and all the way around the edge to box the world in.
	const float Edge = FMath::Max(FMath::Abs(X), FMath::Abs(Y)) / HalfSize;
	const float EdgeMask = FMath::SmoothStep(0.72f, 0.97f, Edge);
	const float RangeMask = FMath::SmoothStep(-0.1f, 0.45f, FMath::PerlinNoise2D(FVector2D(X / 1600.f, Y / 1600.f) + NoiseOffset * 0.37));
	Height += Ridged(X / 700.f, Y / 700.f, 4, NoiseOffset * 1.7) * MountainHeight * FMath::Max(RangeMask, EdgeMask);
	Height += EdgeMask * MountainHeight * 0.9f;

	// In some areas the land steps up in terraces, with cliffs between them.
	const float TerraceMask = FMath::SmoothStep(0.05f, 0.35f, FMath::PerlinNoise2D(FVector2D(X / 900.f, Y / 900.f) + NoiseOffset * 0.71));
	if (TerraceMask > 0.f)
	{
		const float Step = 22.f;
		const float Steps = Height / Step;
		const float Terraced = (FMath::FloorToFloat(Steps) + FMath::SmoothStep(0.4f, 0.6f, FMath::Frac(Steps))) * Step;
		Height = FMath::Lerp(Height, Terraced, TerraceMask);
	}

	// Rivers carve their beds below the water line, with banks that rise into canyon walls through the hills.
	const float RiverDistance = DistanceToRiver(X, Y);
	const float Bank = FMath::Square(FMath::Max(0.f, RiverDistance - RiverHalfWidth) / 55.f) * 50.f;
	Height = SmoothMin(Height, WaterLevel - 3.5f + Bank, 6.f);

	return Height;
}

float AJungleWorld::ComputeHeight(float X, float Y) const
{
	float Height = ComputeNaturalHeight(X, Y);

	// Waterfalls: a plateau behind a sheer cliff edge, a stream across the top, and a pool at the bottom.
	for (const FWaterfallSite& Site : WaterfallSites)
	{
		const FVector2f Offset = FVector2f(X, Y) - Site.Edge;
		const float Out = FVector2f::DotProduct(Offset, Site.Facing);
		const float Side = FMath::Abs(Offset.X * Site.Facing.Y - Offset.Y * Site.Facing.X);
		const float Top = WaterLevel + Site.Top;
		if (Out <= 0.01f)
		{
			const float Mask = (1.f - FMath::SmoothStep(Site.Width * 0.5f + 30.f, Site.Width * 0.5f + 70.f, Side))
				* (1.f - FMath::SmoothStep(100.f, 180.f, -Out));
			Height = FMath::Lerp(Height, Top, Mask);

			// The stream bed, deepest in the middle.
			const float Stream = (1.f - FMath::SmoothStep(Site.Width * 0.25f, Site.Width * 0.45f, Side)) * (1.f - FMath::SmoothStep(90.f, 110.f, -Out));
			Height -= Stream * 1.5f;
		}
		else
		{
			// Low ground in front of the cliff so it stands tall over its pool.
			const float Basin = (1.f - FMath::SmoothStep(Site.Width * 0.5f + 40.f, Site.Width * 0.5f + 90.f, Side)) * (1.f - FMath::SmoothStep(60.f, 140.f, Out));
			Height = FMath::Lerp(Height, WaterLevel + 2.f, Basin);

			const FVector2f Pool((Out - GridSpacing - 25.f) / 35.f, Side / (Site.Width * 0.5f + 20.f));
			const float Mask = 1.f - FMath::SmoothStep(0.7f, 1.f, Pool.Size());
			Height = FMath::Lerp(Height, WaterLevel - 5.f, Mask);
		}
	}

	// Flat ground for the start clearing, the ruins and the caves.
	for (const FFlatSite& Site : FlatSites)
	{
		const float Distance = FVector2f::Distance(FVector2f(X, Y), Site.Center);
		Height = FMath::Lerp(Site.Height, Height, FMath::SmoothStep(Site.Radius, Site.Radius + Site.Blend, Distance));
	}

	return Height;
}

float AJungleWorld::SampleHeight(float X, float Y) const
{
	if (GridPoints < 2 || Heights.Num() != GridPoints * GridPoints)
	{
		return ComputeHeight(X, Y);
	}

	// Find the grid cell, then the triangle in it, so this matches the terrain mesh exactly.
	const float GridX = FMath::Clamp((X + HalfSize) / GridSpacing, 0.f, GridPoints - 1.001f);
	const float GridY = FMath::Clamp((Y + HalfSize) / GridSpacing, 0.f, GridPoints - 1.001f);
	const int32 CellX = FMath::FloorToInt(GridX);
	const int32 CellY = FMath::FloorToInt(GridY);
	const float FracX = GridX - CellX;
	const float FracY = GridY - CellY;

	const float H00 = Heights[CellX * GridPoints + CellY];
	const float H10 = Heights[(CellX + 1) * GridPoints + CellY];
	const float H01 = Heights[CellX * GridPoints + CellY + 1];
	const float H11 = Heights[(CellX + 1) * GridPoints + CellY + 1];
	if (FracX + FracY <= 1.f)
	{
		return H00 + FracX * (H10 - H00) + FracY * (H01 - H00);
	}
	return H11 + (1.f - FracX) * (H01 - H11) + (1.f - FracY) * (H10 - H11);
}

float AJungleWorld::SampleFlatness(float X, float Y) const
{
	const float Step = GridSpacing;
	const float SlopeX = (SampleHeight(X + Step, Y) - SampleHeight(X - Step, Y)) / (2.f * Step);
	const float SlopeY = (SampleHeight(X, Y + Step) - SampleHeight(X, Y - Step)) / (2.f * Step);
	return 1.f / FMath::Sqrt(1.f + SlopeX * SlopeX + SlopeY * SlopeY);
}

float AJungleWorld::GetGroundHeight(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation) / Meter;
	const FVector Ground = GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, SampleHeight(Local.X, Local.Y)) * Meter);
	return Ground.Z;
}

bool AJungleWorld::IsWaterAt(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation) / Meter;
	return SampleHeight(Local.X, Local.Y) < WaterLevel - 0.2f;
}

bool AJungleWorld::CanGrowAt(float X, float Y, float MinFlatness, float Clearance) const
{
	if (FMath::Abs(X) > HalfSize - 20.f || FMath::Abs(Y) > HalfSize - 20.f)
	{
		return false;
	}
	if (SampleHeight(X, Y) < WaterLevel + 0.6f || SampleFlatness(X, Y) < MinFlatness)
	{
		return false;
	}

	const FVector2f Point(X, Y);
	for (const FFlatSite& Site : FlatSites)
	{
		if (FVector2f::Distance(Point, Site.Center) < Site.Radius + Clearance)
		{
			return false;
		}
	}
	for (const FWaterfallSite& Site : WaterfallSites)
	{
		// Keep the stream, the waterfall and the pool clear.
		const FVector2f Offset = Point - Site.Edge;
		const float Out = FVector2f::DotProduct(Offset, Site.Facing);
		const float Side = FMath::Abs(Offset.X * Site.Facing.Y - Offset.Y * Site.Facing.X);
		if (Side < Site.Width * 0.5f + 8.f + Clearance && Out > -115.f && Out < 75.f)
		{
			return false;
		}
	}
	for (const FVector2f& Spot : GiantTreeSpots)
	{
		if (FVector2f::Distance(Point, Spot) < 22.f + Clearance)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Building

void AJungleWorld::BuildTerrain()
{
	Terrain->ClearAllMeshSections();

	const int32 Count = GridPoints * GridPoints;
	TArray<FVector> Positions;
	TArray<FVector> Normals;
	Positions.SetNumUninitialized(Count);
	Normals.SetNumUninitialized(Count);
	for (int32 XIndex = 0; XIndex < GridPoints; ++XIndex)
	{
		for (int32 YIndex = 0; YIndex < GridPoints; ++YIndex)
		{
			const int32 Index = XIndex * GridPoints + YIndex;
			Positions[Index] = FVector(XIndex * GridSpacing - HalfSize, YIndex * GridSpacing - HalfSize, Heights[Index]) * Meter;

			const float Left = Heights[FMath::Max(XIndex - 1, 0) * GridPoints + YIndex];
			const float Right = Heights[FMath::Min(XIndex + 1, GridPoints - 1) * GridPoints + YIndex];
			const float Back = Heights[XIndex * GridPoints + FMath::Max(YIndex - 1, 0)];
			const float Front = Heights[XIndex * GridPoints + FMath::Min(YIndex + 1, GridPoints - 1)];
			Normals[Index] = FVector(Left - Right, Back - Front, 2.f * GridSpacing).GetSafeNormal();
		}
	}

	// Split the triangles into grass, mud and rock so each can have its own color.
	struct FSection
	{
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<int32> Triangles;
		TArray<int32> Remap;
	};
	FSection Sections[Section_Count];
	for (FSection& Section : Sections)
	{
		Section.Remap.Init(INDEX_NONE, Count);
	}

	const float RockHeight = WaterLevel + 6.f + MountainHeight * 0.7f;
	auto AddTriangle = [&](int32 A, int32 B, int32 C)
	{
		const float Average = (Heights[A] + Heights[B] + Heights[C]) / 3.f;
		const float Flatness = ((Normals[A] + Normals[B] + Normals[C]) / 3.f).Z;
		int32 Type = Section_Grass;
		if (Average < WaterLevel + 1.2f)
		{
			Type = Section_Mud;
		}
		else if (Flatness < 0.72f || Average > RockHeight)
		{
			Type = Section_Rock;
		}

		FSection& Section = Sections[Type];
		for (const int32 Corner : { A, B, C })
		{
			int32& Local = Section.Remap[Corner];
			if (Local == INDEX_NONE)
			{
				Local = Section.Vertices.Add(Positions[Corner]);
				Section.Normals.Add(Normals[Corner]);
				Section.UVs.Add(FVector2D(Positions[Corner].X, Positions[Corner].Y) / 1000.f);
			}
			Section.Triangles.Add(Local);
		}
	};

	for (int32 XIndex = 0; XIndex < GridPoints - 1; ++XIndex)
	{
		for (int32 YIndex = 0; YIndex < GridPoints - 1; ++YIndex)
		{
			// Corners of the cell. Triangles are wound so they face up.
			const int32 I00 = XIndex * GridPoints + YIndex;
			const int32 I10 = I00 + GridPoints;
			const int32 I01 = I00 + 1;
			const int32 I11 = I10 + 1;
			AddTriangle(I00, I01, I10);
			AddTriangle(I10, I01, I11);
		}
	}

	const FLinearColor Colors[Section_Count] = { GrassColor, MudColor, RockColor };
	for (int32 Type = 0; Type < Section_Count; ++Type)
	{
		FSection& Section = Sections[Type];
		Terrain->CreateMeshSection_LinearColor(Type, Section.Vertices, Section.Triangles, Section.Normals, Section.UVs, TArray<FLinearColor>(),
			TArray<FProcMeshTangent>(), true);

		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), Colors[Type]);
		Terrain->SetMaterial(Type, Material);
	}
}

void AJungleWorld::BuildWater()
{
	// One big sheet at the water line fills the rivers, lakes and swamps, and the sea past the mountains.
	const float Size = HalfSize * 2.f + 4000.f;
	AddShape(Water, FVector(0.f, 0.f, WaterLevel - 0.5f), FVector(Size, Size, 1.f));
}

void AJungleWorld::BuildWaterfalls()
{
	for (int32 Index = 0; Index < WaterfallCooling.Num(); ++Index)
	{
		UCoolingZoneComponent* Cooling = WaterfallCooling[Index];
		if (!WaterfallSites.IsValidIndex(Index))
		{
			Cooling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			continue;
		}
		Cooling->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		const FWaterfallSite& Site = WaterfallSites[Index];
		const FVector Facing(Site.Facing.X, Site.Facing.Y, 0.f);
		const FVector Lateral(-Site.Facing.Y, Site.Facing.X, 0.f);
		const FVector Edge(Site.Edge.X, Site.Edge.Y, 0.f);
		const float Top = WaterLevel + Site.Top;

		// The falling sheet runs from the cliff edge down to the pool, just off the rock face.
		const FVector Upper = Edge - Facing * 1.f + FVector(0.f, 0.f, Top - 0.6f);
		const FVector Lower = Edge + Facing * (GridSpacing + 2.f) + FVector(0.f, 0.f, WaterLevel);
		const FVector Down = (Lower - Upper).GetSafeNormal();
		const FVector Out = FVector::CrossProduct(Down, Lateral).GetSafeNormal();
		const FRotator SheetRotation = FRotationMatrix::MakeFromZY(-Down, Lateral).Rotator();
		AddShape(Waterfalls, (Upper + Lower) * 0.5f + Out * 1.5f, FVector(1.2f, Site.Width, FVector::Dist(Upper, Lower)), SheetRotation);

		// White water churning at the bottom.
		for (int32 Splash = 0; Splash < 5; ++Splash)
		{
			const float Along = (Splash - 2) * Site.Width * 0.2f;
			AddShape(Waterfalls, Lower + Lateral * Along + Facing * 4.f + FVector(0.f, 0.f, 0.3f), FVector(6.f, Site.Width * 0.3f, 0.8f),
				FRotator(0.f, Splash * 23.f, 0.f));
		}

		// The stream across the top of the plateau, feeding the fall.
		const float StreamLength = 100.f;
		AddShape(Water, Edge - Facing * (StreamLength * 0.5f) + FVector(0.f, 0.f, Top - 0.6f), FVector(StreamLength, Site.Width * 0.8f, 0.4f),
			Facing.Rotation());

		const float Yaw = Facing.Rotation().Yaw;
		FVector CoolingCenter = Edge + Facing * (GridSpacing * 0.5f);
		CoolingCenter.Z = (Top + WaterLevel) * 0.5f;
		Cooling->SetRelativeLocationAndRotation(CoolingCenter * Meter, FRotator(0.f, Yaw, 0.f));
		Cooling->SetBoxExtent(FVector(GridSpacing * 0.5f + 5.f, Site.Width * 0.5f + 2.f, Site.Top * 0.5f + 3.f) * Meter);
	}
}

void AJungleWorld::BuildRuins()
{
	FRandomStream Random(RandomSeed + 11);
	for (const FRuinSite& Site : RuinSites)
	{
		const float Ground = SampleHeight(Site.Center.X, Site.Center.Y);
		const FVector Center(Site.Center.X, Site.Center.Y, Ground);

		// Broken outer walls on a ring, with gaps to walk through.
		const int32 Segments = 9;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			if (Random.FRand() < 0.25f)
			{
				continue;
			}
			const float Angle = Index * 360.f / Segments + Random.FRandRange(-6.f, 6.f);
			const FVector Spot = Center + FRotator(0.f, Angle, 0.f).Vector() * Site.Radius * 0.85f;
			const float Height = Random.FRandRange(2.f, 7.f);
			AddShape(Ruins, Spot + FVector(0.f, 0.f, Height * 0.5f - 1.f), FVector(2.f, Site.Radius * 0.45f, Height + 2.f),
				FRotator(0.f, Angle, Random.FRandRange(-4.f, 4.f)));
		}

		// Columns, some still standing and some toppled.
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = Index * 45.f + 22.f;
			const FVector Spot = Center + FRotator(0.f, Angle, 0.f).Vector() * Site.Radius * 0.55f;
			const float Height = Random.FRandRange(6.f, 14.f);
			if (Random.FRand() < 0.35f)
			{
				AddShape(Ruins, Spot + FVector(0.f, 0.f, 1.f), FVector(2.f, 2.f, Height), FRotator(88.f, Random.FRandRange(0.f, 360.f), 0.f));
			}
			else
			{
				AddShape(Ruins, Spot + FVector(0.f, 0.f, Height * 0.5f - 0.5f), FVector(2.2f, 2.2f, Height + 1.f), FRotator(0.f, Angle, 0.f));
			}
		}

		// A stepped altar in the middle with an archway on top.
		for (int32 Step = 0; Step < 3; ++Step)
		{
			const float Width = 18.f - Step * 5.f;
			AddShape(Ruins, Center + FVector(0.f, 0.f, Step * 1.5f + 0.25f), FVector(Width, Width, 2.f), FRotator(0.f, 45.f, 0.f));
		}
		const FVector AltarTop = Center + FVector(0.f, 0.f, 4.25f);
		const FVector ArchSide = FRotator(0.f, 45.f, 0.f).Vector() * 3.5f;
		AddShape(Ruins, AltarTop + ArchSide + FVector(0.f, 0.f, 3.f), FVector(1.5f, 1.5f, 6.f), FRotator(0.f, 45.f, 0.f));
		AddShape(Ruins, AltarTop - ArchSide + FVector(0.f, 0.f, 3.f), FVector(1.5f, 1.5f, 6.f), FRotator(0.f, 45.f, 0.f));
		AddShape(Ruins, AltarTop + FVector(0.f, 0.f, 6.6f), FVector(9.f, 1.8f, 1.2f), FRotator(0.f, 45.f, 0.f));
	}
}

void AJungleWorld::BuildCaves()
{
	FRandomStream Random(RandomSeed + 23);
	for (const FCaveSite& Site : CaveSites)
	{
		const FRotator Facing(0.f, Site.Yaw, 0.f);
		const FVector Along = Facing.Vector();
		const FVector Side = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
		const FVector Center(Site.Center.X, Site.Center.Y, SampleHeight(Site.Center.X, Site.Center.Y));

		const float InnerHalfWidth = 12.f;
		const float InnerHeight = 16.f;
		const float SegmentLength = 10.f;
		const int32 Segments = FMath::Max(2, FMath::RoundToInt(Site.Length / SegmentLength));
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const FVector Spot = Center + Along * ((Index + 0.5f) * SegmentLength - Site.Length * 0.5f);
			const float Jitter = Random.FRandRange(-3.f, 3.f);

			// Thick walls on both sides, sunk into the ground.
			for (const float Sign : { -1.f, 1.f })
			{
				const float Thickness = Random.FRandRange(7.f, 11.f);
				const float Height = InnerHeight + 6.f + Random.FRandRange(0.f, 4.f);
				AddShape(Rocks, Spot + Side * Sign * (InnerHalfWidth + Thickness * 0.5f) + FVector(0.f, 0.f, Height * 0.5f - 4.f),
					FVector(SegmentLength + 2.f, Thickness, Height), Facing + FRotator(Random.FRandRange(-3.f, 3.f), Jitter, 0.f));
			}

			// The roof, with a lumpy hill of boulders piled on top.
			AddShape(Rocks, Spot + FVector(0.f, 0.f, InnerHeight + 3.f + Random.FRandRange(-1.f, 1.f)),
				FVector(SegmentLength + 2.f, InnerHalfWidth * 2.f + 16.f, 6.f), Facing + FRotator(0.f, Jitter, Random.FRandRange(-3.f, 3.f)));
			for (int32 Pile = 0; Pile < 2; ++Pile)
			{
				const float Size = Random.FRandRange(10.f, 18.f);
				AddShape(Rocks, Spot + Side * Random.FRandRange(-12.f, 12.f) + FVector(0.f, 0.f, InnerHeight + 5.f + Size * 0.25f), FVector(Size, Size, Size * 0.7f),
					FRotator(Random.FRandRange(-20.f, 20.f), Random.FRandRange(0.f, 90.f), Random.FRandRange(-20.f, 20.f)));
			}

			// Rubble on the floor.
			if (Random.FRand() < 0.5f)
			{
				const float Size = Random.FRandRange(1.f, 2.5f);
				AddShape(Rocks, Spot + Side * Random.FRandRange(-8.f, 8.f) + FVector(0.f, 0.f, Size * 0.3f), FVector(Size, Size, Size),
					FRotator(Random.FRandRange(0.f, 45.f), Random.FRandRange(0.f, 90.f), 0.f));
			}
		}
	}
}

void AJungleWorld::BuildRocks(FRandomStream& Random)
{
	// Tall moss-covered stone spires to fly around.
	int32 Placed = 0;
	for (int32 Attempt = 0; Placed < 40 && Attempt < 800; ++Attempt)
	{
		const float X = Random.FRandRange(-HalfSize, HalfSize) * 0.9f;
		const float Y = Random.FRandRange(-HalfSize, HalfSize) * 0.9f;
		if (FMath::Sqrt(X * X + Y * Y) < ClearingRadius + ClearingBlend + 40.f || !CanGrowAt(X, Y, 0.6f, 40.f))
		{
			continue;
		}
		const float Ground = SampleHeight(X, Y);
		const float Width = Random.FRandRange(12.f, 28.f);
		const float Height = Random.FRandRange(50.f, 150.f);
		const FRotator Tilt(Random.FRandRange(-4.f, 4.f), Random.FRandRange(0.f, 90.f), Random.FRandRange(-4.f, 4.f));
		AddShape(Rocks, FVector(X, Y, Ground + Height * 0.5f - 6.f), FVector(Width, Width * Random.FRandRange(0.6f, 1.3f), Height), Tilt);
		// A cap that juts out a little, like Anthem's mesas.
		AddShape(Rocks, FVector(X, Y, Ground + Height - 6.f), FVector(Width * 1.3f, Width * 1.2f, 5.f), Tilt + FRotator(0.f, 15.f, 0.f));
		++Placed;
	}

	// Boulders, mostly on slopes and along riverbanks.
	const int32 Boulders = FMath::RoundToInt(1500 * VegetationDensity);
	for (int32 Index = 0; Index < Boulders; ++Index)
	{
		const float X = Random.FRandRange(-HalfSize, HalfSize);
		const float Y = Random.FRandRange(-HalfSize, HalfSize);
		const float Flatness = SampleFlatness(X, Y);
		const bool bRiverbank = DistanceToRiver(X, Y) < RiverHalfWidth + 25.f;
		if (Flatness > 0.9f && !bRiverbank && Random.FRand() < 0.8f)
		{
			continue;
		}
		if (!CanGrowAt(X, Y, 0.f, 5.f) && !bRiverbank)
		{
			continue;
		}
		const float Size = Random.FRandRange(2.f, 11.f);
		AddShape(Rocks, FVector(X, Y, SampleHeight(X, Y) + Size * 0.15f), FVector(Size, Size * Random.FRandRange(0.6f, 1.2f), Size * Random.FRandRange(0.5f, 0.9f)),
			FRotator(Random.FRandRange(-25.f, 25.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-25.f, 25.f)));
	}
}

void AJungleWorld::BuildGiantTrees(FRandomStream& Random)
{
	// A few go in a ring around the start so you see them straight away; the rest anywhere.
	int32 Placed = 0;
	for (int32 Attempt = 0; Placed < GiantTreeCount && Attempt < GiantTreeCount * 60; ++Attempt)
	{
		float X;
		float Y;
		if (Placed < 3)
		{
			const FVector2D Spot = FVector2D(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f).Vector()) * Random.FRandRange(300.f, 520.f);
			X = Spot.X;
			Y = Spot.Y;
		}
		else
		{
			X = Random.FRandRange(-HalfSize, HalfSize) * 0.85f;
			Y = Random.FRandRange(-HalfSize, HalfSize) * 0.85f;
		}
		if (!CanGrowAt(X, Y, 0.85f, 30.f))
		{
			continue;
		}
		bool bTooClose = false;
		for (const FVector2f& Other : GiantTreeSpots)
		{
			bTooClose |= FVector2f::Distance(Other, FVector2f(X, Y)) < 160.f;
		}
		if (bTooClose)
		{
			continue;
		}
		GiantTreeSpots.Add(FVector2f(X, Y));
		++Placed;

		const float Ground = SampleHeight(X, Y);
		const FVector Base(X, Y, Ground);
		const float Height = Random.FRandRange(90.f, 150.f);
		const float Radius = Random.FRandRange(5.f, 8.f);

		// The trunk narrows on the way up.
		AddShape(Trunks, Base + FVector(0.f, 0.f, Height * 0.3f - 3.f), FVector(Radius * 2.f, Radius * 2.f, Height * 0.6f + 6.f));
		AddShape(Trunks, Base + FVector(0.f, 0.f, Height * 0.77f), FVector(Radius * 1.4f, Radius * 1.4f, Height * 0.46f));

		// Buttress roots fanning out at the base.
		const float RootStart = Random.FRandRange(0.f, 60.f);
		for (int32 Root = 0; Root < 6; ++Root)
		{
			const FRotator Facing(0.f, RootStart + Root * 60.f + Random.FRandRange(-10.f, 10.f), 0.f);
			AddShape(GiantWood, Base + Facing.Vector() * Radius * 1.8f + FVector(0.f, 0.f, Radius * 0.7f),
				FVector(Radius * 3.f, 1.6f, Radius * 2.4f), FRotator(-30.f, Facing.Yaw, 0.f));
		}

		// Branch platforms you can land on, the highest one right on top of the trunk.
		const int32 Platforms = Random.RandRange(3, 4);
		const float FirstYaw = Random.FRandRange(0.f, 360.f);
		for (int32 Level = 0; Level < Platforms; ++Level)
		{
			const bool bTop = Level == Platforms - 1;
			const float PlatformHeight = Ground + Height * (0.5f + 0.5f * Level / (Platforms - 1));
			const FRotator Facing(0.f, FirstYaw + Level * 137.f, 0.f);
			const float Reach = bTop ? 0.f : Random.FRandRange(18.f, 28.f);
			const float PlatformRadius = bTop ? Random.FRandRange(24.f, 32.f) : Random.FRandRange(13.f, 20.f);
			const FVector Center = Base + Facing.Vector() * Reach + FVector(0.f, 0.f, PlatformHeight - Ground);

			AddShape(GiantPlatforms, Center, FVector(PlatformRadius * 2.f, PlatformRadius * 2.f, 2.5f));
			if (!bTop)
			{
				// The branch from the trunk out to the platform.
				const FVector Start = Base + FVector(0.f, 0.f, PlatformHeight - Ground - 8.f);
				const FVector Branch = Center - FVector(0.f, 0.f, 1.f) - Start;
				AddShape(GiantWood, Start + Branch * 0.5f, FVector(Branch.Size(), 3.5f, 3.5f), Branch.Rotation());
			}

			// Leaves around the rim, leaving the middle open to land on.
			const int32 Clumps = bTop ? 9 : 6;
			for (int32 Clump = 0; Clump < Clumps; ++Clump)
			{
				const FRotator Around(0.f, Clump * 360.f / Clumps + Random.FRandRange(-15.f, 15.f), 0.f);
				const float Size = PlatformRadius * Random.FRandRange(0.8f, 1.1f);
				UHierarchicalInstancedStaticMeshComponent* Color = Random.FRand() < 0.5f ? Leaves.Get() : LeavesDark.Get();
				AddShape(Color, Center + Around.Vector() * PlatformRadius * 0.9f + FVector(0.f, 0.f, Size * 0.15f), FVector(Size, Size, Size * 0.45f));
			}

			// Vines hanging from the edge.
			for (int32 Vine = 0; Vine < 5; ++Vine)
			{
				const FRotator Around(0.f, Random.FRandRange(0.f, 360.f), 0.f);
				const float Length = Random.FRandRange(15.f, 45.f);
				AddShape(Vines, Center + Around.Vector() * PlatformRadius * Random.FRandRange(0.6f, 0.95f) - FVector(0.f, 0.f, Length * 0.5f),
					FVector(0.5f, 0.5f, Length));
			}
		}
	}
}

void AJungleWorld::BuildForest(FRandomStream& Random)
{
	// Trees on a jittered grid, thick in some places and thin in others.
	const float Cell = 22.f / FMath::Sqrt(FMath::Max(VegetationDensity, 0.05f));
	const int32 Cells = FMath::FloorToInt(HalfSize * 2.f / Cell);
	UHierarchicalInstancedStaticMeshComponent* CanopyColors[] = { LeavesDark.Get(), Leaves.Get(), Leaves.Get(), LeavesLight.Get() };
	auto PointInCircle = [&Random](float Radius)
	{
		return FVector2D(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f).Vector()) * Radius * FMath::Sqrt(Random.FRand());
	};

	for (int32 CellX = 0; CellX < Cells; ++CellX)
	{
		for (int32 CellY = 0; CellY < Cells; ++CellY)
		{
			const float X = -HalfSize + (CellX + Random.FRand()) * Cell;
			const float Y = -HalfSize + (CellY + Random.FRand()) * Cell;
			const float Density = 0.55f + 0.4f * FMath::PerlinNoise2D(FVector2D(X / 230.f, Y / 230.f) + NoiseOffset * 0.13);
			if (VegetationDensity <= 0.f || Random.FRand() > Density || !CanGrowAt(X, Y, 0.78f, 8.f))
			{
				continue;
			}

			const float Ground = SampleHeight(X, Y);
			// Fewer trees up on the mountains.
			if (Ground > WaterLevel + MountainHeight * 0.6f && Random.FRand() < 0.7f)
			{
				continue;
			}

			const bool bNearWater = DistanceToRiver(X, Y) < RiverHalfWidth + 45.f || Ground < WaterLevel + 3.f;
			const bool bPalm = Random.FRand() < (bNearWater ? 0.7f : 0.15f);
			const FVector Base(X, Y, Ground);

			if (bPalm)
			{
				// A thin leaning trunk with drooping fronds.
				const float Height = Random.FRandRange(9.f, 18.f);
				const FRotator Lean(Random.FRandRange(-10.f, 10.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-10.f, 10.f));
				const FVector Top = Base + Lean.RotateVector(FVector(0.f, 0.f, Height));
				AddShape(Trunks, (Base + Top) * 0.5f - FVector(0.f, 0.f, 0.5f), FVector(0.7f, 0.7f, Height + 1.f), Lean);
				const int32 Fronds = 7;
				const float FrondLength = Random.FRandRange(6.f, 8.5f);
				const float StartYaw = Random.FRandRange(0.f, 360.f);
				for (int32 Frond = 0; Frond < Fronds; ++Frond)
				{
					const FRotator Droop(-28.f + Random.FRandRange(-8.f, 8.f), StartYaw + Frond * 360.f / Fronds, 0.f);
					AddShape(PalmFronds, Top + Droop.Vector() * FrondLength * 0.45f, FVector(FrondLength, 1.4f, 0.15f), Droop);
				}
				continue;
			}

			// A broadleaf rainforest tree: a tall bare trunk with a wide, flat crown.
			const float Height = Random.FRandRange(16.f, 38.f);
			const float Scale = Height / 27.f;
			const float TrunkWidth = Random.FRandRange(0.9f, 2.f) * Scale;
			AddShape(Trunks, Base + FVector(0.f, 0.f, Height * 0.5f - 1.f), FVector(TrunkWidth, TrunkWidth, Height + 2.f));

			UHierarchicalInstancedStaticMeshComponent* Canopy = CanopyColors[Random.RandRange(0, 3)];
			const int32 Clumps = Random.RandRange(2, 3);
			const float ClumpSize = Random.FRandRange(7.f, 13.f) * Scale;
			for (int32 Clump = 0; Clump < Clumps; ++Clump)
			{
				const FVector2D Offset = PointInCircle(ClumpSize * 0.4f);
				const float Size = ClumpSize * Random.FRandRange(0.85f, 1.25f);
				AddShape(Canopy, Base + FVector(Offset.X, Offset.Y, Height - ClumpSize * 0.15f + Random.FRandRange(-2.f, 2.f)),
					FVector(Size, Size * Random.FRandRange(0.85f, 1.15f), Size * Random.FRandRange(0.45f, 0.7f)));
			}

			// Some trees trail vines.
			if (Random.FRand() < 0.3f)
			{
				const FVector2D Offset = PointInCircle(ClumpSize * 0.45f);
				const float Length = Random.FRandRange(6.f, Height * 0.7f);
				AddShape(Vines, Base + FVector(Offset.X, Offset.Y, Height - Length * 0.5f), FVector(0.3f, 0.3f, Length));
			}

			// And a fallen log here and there.
			if (Random.FRand() < 0.04f)
			{
				const float Length = Random.FRandRange(8.f, 20.f);
				const float Width = Random.FRandRange(1.f, 1.8f);
				const FVector Spot = Base + FVector(Random.FRandRange(-8.f, 8.f), Random.FRandRange(-8.f, 8.f), 0.f);
				if (CanGrowAt(Spot.X, Spot.Y, 0.85f, 4.f))
				{
					AddShape(Trunks, FVector(Spot.X, Spot.Y, SampleHeight(Spot.X, Spot.Y) + Width * 0.3f), FVector(Width, Width, Length),
						FRotator(90.f, Random.FRandRange(0.f, 360.f), 0.f));
				}
			}
		}
	}
}

void AJungleWorld::BuildUndergrowth(FRandomStream& Random)
{
	// Ferns, bushes and flowers on a fine jittered grid. They're only drawn up close, so there can be lots.
	const float Cell = 6.5f / FMath::Sqrt(FMath::Max(VegetationDensity, 0.05f));
	const int32 Cells = FMath::FloorToInt(HalfSize * 2.f / Cell);
	for (int32 CellX = 0; CellX < Cells; ++CellX)
	{
		for (int32 CellY = 0; CellY < Cells; ++CellY)
		{
			if (VegetationDensity <= 0.f || Random.FRand() > 0.45f)
			{
				continue;
			}
			const float X = -HalfSize + (CellX + Random.FRand()) * Cell;
			const float Y = -HalfSize + (CellY + Random.FRand()) * Cell;
			if (!CanGrowAt(X, Y, 0.7f, 2.f))
			{
				continue;
			}
			const FVector Base(X, Y, SampleHeight(X, Y));
			const float Pick = Random.FRand();
			if (Pick < 0.5f)
			{
				// A fern: a few long, flat fronds fanning out from the ground.
				const float Size = Random.FRandRange(1.5f, 3.2f);
				const float StartYaw = Random.FRandRange(0.f, 360.f);
				for (int32 Frond = 0; Frond < 3; ++Frond)
				{
					const FRotator Arch(Random.FRandRange(15.f, 35.f), StartYaw + Frond * 120.f, 0.f);
					AddShape(Ferns, Base + Arch.Vector() * Size * 0.45f + FVector(0.f, 0.f, 0.2f), FVector(Size, Size * 0.3f, 0.06f), Arch);
				}
			}
			else if (Pick < 0.88f)
			{
				const float Size = Random.FRandRange(1.8f, 5.f);
				AddShape(Bushes, Base + FVector(0.f, 0.f, Size * 0.2f), FVector(Size, Size * Random.FRandRange(0.8f, 1.2f), Size * Random.FRandRange(0.5f, 0.8f)));
			}
			else
			{
				// Bright flowers, upside-down cones like trumpets.
				const float Size = Random.FRandRange(0.4f, 0.8f);
				AddShape(Flowers, Base + FVector(0.f, 0.f, Size * 0.8f), FVector(Size, Size, Size), FRotator(180.f, 0.f, Random.FRandRange(-20.f, 20.f)));
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Play

void AJungleWorld::BeginPlay()
{
	Super::BeginPlay();

	// A jungle placed in the editor keeps its meshes, but the height grid isn't saved; work it out again.
	if (Heights.Num() != GridPoints * GridPoints || GridPoints < 2)
	{
		ComputeGrid();
	}

	HideTemplateFloor();
	if (bSetUpJungleAtmosphere)
	{
		SetUpAtmosphere();
	}
	if (bSpawnStartArea)
	{
		SpawnStartArea();
	}
	if (bSpawnRuinCamps)
	{
		SpawnRuinCamps();
	}
}

void AJungleWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CooledHeat.IsValid())
	{
		CooledHeat->RemoveCoolingSource(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AJungleWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWaterCooling();
}

void AJungleWorld::UpdateWaterCooling()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	UJavelinHeatComponent* Heat = Player ? Player->FindComponentByClass<UJavelinHeatComponent>() : nullptr;
	if (CooledHeat.IsValid() && CooledHeat.Get() != Heat)
	{
		CooledHeat->RemoveCoolingSource(this);
		CooledHeat.Reset();
	}
	if (!Heat)
	{
		return;
	}

	// Cool the suit when its feet are in the water or skimming just above it.
	FVector Feet = Player->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Player))
	{
		Feet.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(Feet) / Meter;
	const bool bInWater = SampleHeight(Local.X, Local.Y) < WaterLevel - 0.2f && Local.Z < WaterLevel + 2.5f;
	if (bInWater)
	{
		Heat->AddCoolingSource(this, WaterCoolingPerSecond);
		CooledHeat = Heat;
	}
	else if (CooledHeat.IsValid())
	{
		Heat->RemoveCoolingSource(this);
		CooledHeat.Reset();
	}
}

void AJungleWorld::SpawnStartArea()
{
	// The flight test arena without its course: just the shooting range, the ammo crate and the first enemy camp.
	const FTransform Transform = GetActorTransform();
	if (AFlightTestArena* Arena = GetWorld()->SpawnActorDeferred<AFlightTestArena>(AFlightTestArena::StaticClass(), Transform, this))
	{
		Arena->bBuildFlightCourse = false;
		Arena->RockColor = RuinColor;
		Arena->FinishSpawning(Transform);
	}
}

void AJungleWorld::SpawnRuinCamps()
{
	for (const FRuinSite& Site : RuinSites)
	{
		const FVector Local = FVector(Site.Center.X, Site.Center.Y, SampleHeight(Site.Center.X, Site.Center.Y)) * Meter;
		const FTransform CampTransform = FTransform(Local) * GetActorTransform();
		if (AEnemyEncounter* Encounter = GetWorld()->SpawnActorDeferred<AEnemyEncounter>(AEnemyEncounter::StaticClass(), CampTransform, this))
		{
			Encounter->CampRadius = Site.Radius * 0.7f * Meter;
			Encounter->FirstWaveSoldiers = Site.Soldiers;
			Encounter->FirstWaveDrones = Site.Drones;
			Encounter->MaxSoldiers = Site.Soldiers + 4;
			Encounter->MaxDrones = Site.Drones + 3;
			Encounter->FinishSpawning(CampTransform);
		}
	}
}

void AJungleWorld::HideTemplateFloor()
{
	// The Basic level template comes with a big gray floor at the start. The jungle has its own ground.
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		if (Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName().Contains(TEXT("Template_Map_Floor")))
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
		}
	}
}

void AJungleWorld::SetUpAtmosphere()
{
	UWorld* World = GetWorld();

	// Thick, low-lying haze with light shafts through it.
	auto SetUpFog = [](UExponentialHeightFogComponent* Fog)
	{
		Fog->SetFogDensity(0.03f);
		Fog->SetFogHeightFalloff(0.15f);
		Fog->SetFogMaxOpacity(0.97f);
		Fog->SetStartDistance(1500.f);
		Fog->SetVolumetricFog(true);
		Fog->SetVolumetricFogScatteringDistribution(0.7f);
		Fog->SetVolumetricFogAlbedo(FColor(205, 235, 215));
		Fog->SetVolumetricFogExtinctionScale(1.5f);
		Fog->SetVolumetricFogDistance(15000.f);
	};
	bool bHasFog = false;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* Fog = It->GetComponent())
		{
			SetUpFog(Fog);
			bHasFog = true;
		}
	}
	if (!bHasFog)
	{
		UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this, TEXT("JungleFog"));
		Fog->SetupAttachment(RootComponent);
		Fog->RegisterComponent();
		SetUpFog(Fog);
	}

	// Warm, slightly golden sunlight that scatters strongly in the fog.
	bool bHasSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (ULightComponent* Sun = It->GetLightComponent())
		{
			Sun->SetLightColor(FLinearColor(1.f, 0.9f, 0.74f));
			Sun->SetVolumetricScatteringIntensity(2.f);
			bHasSun = true;
		}
	}

	// An empty level gets its own sun, sky and sky light.
	if (!bHasSun)
	{
		UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("JungleSun"));
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->SetupAttachment(RootComponent);
		Sun->SetWorldRotation(FRotator(-40.f, 35.f, 0.f));
		Sun->RegisterComponent();
		Sun->SetIntensity(10.f);
		Sun->SetAtmosphereSunLight(true);
		Sun->SetLightColor(FLinearColor(1.f, 0.9f, 0.74f));
		Sun->SetVolumetricScatteringIntensity(2.f);
	}

	bool bHasSky = false;
	bool bHasSkyLight = false;
	for (TObjectIterator<USkyAtmosphereComponent> It; It; ++It)
	{
		bHasSky |= It->GetWorld() == World;
	}
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		bHasSkyLight |= It->GetWorld() == World;
	}
	if (!bHasSky)
	{
		USkyAtmosphereComponent* Sky = NewObject<USkyAtmosphereComponent>(this, TEXT("JungleSky"));
		Sky->SetupAttachment(RootComponent);
		Sky->RegisterComponent();
	}
	if (!bHasSkyLight)
	{
		USkyLightComponent* SkyLight = NewObject<USkyLightComponent>(this, TEXT("JungleSkyLight"));
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLight->SetupAttachment(RootComponent);
		SkyLight->bRealTimeCapture = true;
		SkyLight->RegisterComponent();
	}
}
