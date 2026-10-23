#include "FlightTestArena.h"
#include "AmmoCrate.h"
#include "CoolingVolume.h"
#include "EnemyEncounter.h"
#include "ShootingTarget.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float Meter = 100.f;

	// Lake and waterfall layout, in meters from the arena center.
	const FVector LakeCenter(200.f, 0.f, 0.f);
	const FVector2D LakeSize(80.f, 60.f);
	const float CliffX = 260.f;
	const FVector CliffSize(20.f, 60.f, 90.f);
	const float WaterfallWidth = 20.f;

	// Shooting range in front of the start, beside the landing tower. Meters from the arena center.
	const FVector2D RangeMin(15.f, -45.f);
	const FVector2D RangeMax(100.f, 25.f);

	// Enemy camp behind and to the left of the start, between two arches. Meters from the arena center.
	const FVector2D CampCenter(-55.f, -95.f);
	const float CampRadius = 30.f;
}

AFlightTestArena::AFlightTestArena()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = ShapeMaterial.Object;

	auto MakeInstances = [this](FName Name, bool bCollides)
	{
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(RootComponent);
		Component->SetStaticMesh(CubeMesh.Object);
		if (bCollides)
		{
			Component->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCastShadow(false);
		}
		return Component;
	};

	Ground = MakeInstances(TEXT("Ground"), true);
	Rocks = MakeInstances(TEXT("Rocks"), true);
	Water = MakeInstances(TEXT("Water"), false);

	LakeCooling = CreateDefaultSubobject<UCoolingZoneComponent>(TEXT("LakeCooling"));
	LakeCooling->SetupAttachment(RootComponent);
	LakeCooling->CoolingPerSecond = 50.f;

	WaterfallCooling = CreateDefaultSubobject<UCoolingZoneComponent>(TEXT("WaterfallCooling"));
	WaterfallCooling->SetupAttachment(RootComponent);
	WaterfallCooling->CoolingPerSecond = 80.f;
}

void AFlightTestArena::SetColor(UInstancedStaticMeshComponent* Component, const FLinearColor& Color)
{
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Component->SetMaterial(0, Material);
}

void AFlightTestArena::AddBox(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, float Yaw)
{
	// The engine cube is 1 m on each side with its pivot in the middle.
	Component->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Center, Size / Meter));
}

void AFlightTestArena::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Ground->ClearInstances();
	Rocks->ClearInstances();
	Water->ClearInstances();

	SetColor(Ground, GroundColor);
	SetColor(Rocks, RockColor);
	SetColor(Water, WaterColor);

	// 1 km square ground. Its top sits just under zero so it doesn't flicker against a template level's floor.
	AddBox(Ground, FVector(0.f, 0.f, -52.f), FVector(1000.f, 1000.f, 1.f) * Meter);

	// Landing tower near the start: a 15 m pad 60 m up.
	AddBox(Rocks, FVector(40.f, 40.f, 30.f) * Meter, FVector(15.f, 15.f, 60.f) * Meter);

	// Lake: shallow enough to wade in, with a cooling zone a few meters deep so skimming the surface cools you.
	AddBox(Water, FVector(LakeCenter.X, LakeCenter.Y, 0.25f) * Meter, FVector(LakeSize.X, LakeSize.Y, 0.5f) * Meter);
	LakeCooling->SetRelativeLocation(FVector(LakeCenter.X, LakeCenter.Y, 2.f) * Meter);
	LakeCooling->SetBoxExtent(FVector(LakeSize.X * 0.5f, LakeSize.Y * 0.5f, 2.5f) * Meter);

	// Cliff behind the lake with a waterfall sheet down its face.
	AddBox(Rocks, FVector(CliffX, 0.f, CliffSize.Z * 0.5f) * Meter, CliffSize * Meter);
	const float FallX = CliffX - CliffSize.X * 0.5f - 0.5f;
	AddBox(Water, FVector(FallX, 0.f, CliffSize.Z * 0.5f) * Meter, FVector(1.f, WaterfallWidth, CliffSize.Z) * Meter);
	WaterfallCooling->SetRelativeLocation(FVector(FallX - 3.f, 0.f, CliffSize.Z * 0.5f) * Meter);
	WaterfallCooling->SetBoxExtent(FVector(4.f, WaterfallWidth * 0.5f + 1.f, CliffSize.Z * 0.5f) * Meter);

	// Arches to fly through, on a ring around the start.
	for (const float Angle : { 90.f, 150.f, 210.f, 270.f })
	{
		const FRotator Facing(0.f, Angle, 0.f);
		const FVector Center = Facing.Vector() * 140.f * Meter;
		const FVector Side = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y) * 15.f * Meter;
		const FVector PostSize = FVector(5.f, 5.f, 40.f) * Meter;
		AddBox(Rocks, Center + Side + FVector(0.f, 0.f, 20.f * Meter), PostSize, Angle);
		AddBox(Rocks, Center - Side + FVector(0.f, 0.f, 20.f * Meter), PostSize, Angle);
		AddBox(Rocks, Center + FVector(0.f, 0.f, 42.5f * Meter), FVector(5.f, 35.f, 5.f) * Meter, Angle);
	}

	// Low cover walls at the front of the shooting range.
	AddBox(Rocks, FVector(22.f, -20.f, 0.75f) * Meter, FVector(0.8f, 5.f, 1.5f) * Meter);
	AddBox(Rocks, FVector(22.f, 2.f, 0.75f) * Meter, FVector(0.8f, 5.f, 1.5f) * Meter);
	AddBox(Rocks, FVector(55.f, -30.f, 1.f) * Meter, FVector(4.f, 4.f, 2.f) * Meter, 30.f);

	// Enemy camp: a ring of broken walls and crates to fight around, with a watchtower in the middle.
	{
		const FVector Camp(CampCenter.X, CampCenter.Y, 0.f);
		for (int32 Index = 0; Index < 6; ++Index)
		{
			// Wall segments on a ring, with gaps between them to walk through.
			const float Angle = Index * 60.f + 15.f;
			const FVector Spot = Camp + FRotator(0.f, Angle, 0.f).Vector() * CampRadius * 0.85f;
			const float Height = Index % 2 == 0 ? 2.5f : 1.5f;
			AddBox(Rocks, FVector(Spot.X, Spot.Y, Height * 0.5f) * Meter, FVector(1.2f, 9.f, Height) * Meter, Angle);
		}
		const FVector Crates[] = { FVector(8.f, 6.f, 1.5f), FVector(-10.f, 4.f, 1.f), FVector(3.f, -11.f, 2.f), FVector(-6.f, -8.f, 1.2f), FVector(14.f, -4.f, 1.f) };
		for (const FVector& Crate : Crates)
		{
			AddBox(Rocks, FVector(Camp.X + Crate.X, Camp.Y + Crate.Y, Crate.Z * 0.5f) * Meter, FVector(Crate.Z * 1.6f, Crate.Z * 1.6f, Crate.Z) * Meter, Crate.X * 7.f);
		}
		AddBox(Rocks, FVector(Camp.X, Camp.Y, 6.f) * Meter, FVector(4.f, 4.f, 12.f) * Meter, 45.f);
	}

	// Rock pillars scattered around, kept clear of the start area, the lake, the shooting range and the enemy camp.
	FRandomStream Random(RandomSeed);
	int32 Placed = 0;
	for (int32 Attempt = 0; Placed < PillarCount && Attempt < PillarCount * 20; ++Attempt)
	{
		const FVector2D Position = FVector2D(Random.FRandRange(-450.f, 450.f), Random.FRandRange(-450.f, 450.f));
		const bool bNearStart = Position.Size() < 70.f;
		const bool bNearLake = Position.X > LakeCenter.X - LakeSize.X * 0.5f - 15.f && Position.X < CliffX + 20.f && FMath::Abs(Position.Y) < 50.f;
		const bool bNearArches = FMath::Abs(Position.Size() - 140.f) < 25.f;
		const bool bNearRange = Position.X > RangeMin.X - 15.f && Position.X < RangeMax.X + 15.f && Position.Y > RangeMin.Y - 15.f && Position.Y < RangeMax.Y + 15.f;
		const bool bNearCamp = FVector2D::Distance(Position, CampCenter) < CampRadius + 20.f;
		if (bNearStart || bNearLake || bNearArches || bNearRange || bNearCamp)
		{
			continue;
		}

		const float Width = Random.FRandRange(8.f, 25.f);
		const float Height = Random.FRandRange(20.f, 150.f);
		AddBox(Rocks, FVector(Position.X, Position.Y, Height * 0.5f) * Meter, FVector(Width, Width * Random.FRandRange(0.6f, 1.4f), Height) * Meter,
			Random.FRandRange(0.f, 90.f));
		++Placed;
	}
}

void AFlightTestArena::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnShootingRange)
	{
		SpawnShootingRange();
	}
	if (bSpawnEnemies)
	{
		SpawnEnemyCamp();
	}
}

void AFlightTestArena::SpawnEnemyCamp()
{
	const FTransform CampTransform = FTransform(FVector(CampCenter.X, CampCenter.Y, 0.f) * Meter) * GetActorTransform();
	if (AEnemyEncounter* Encounter = GetWorld()->SpawnActorDeferred<AEnemyEncounter>(AEnemyEncounter::StaticClass(), CampTransform, this))
	{
		Encounter->CampRadius = CampRadius * 0.8f * Meter;
		Encounter->FinishSpawning(CampTransform);
	}
}

void AFlightTestArena::SpawnShootingRange()
{
	UWorld* World = GetWorld();
	const FTransform& ArenaTransform = GetActorTransform();

	struct FTargetSpot
	{
		EShootingTargetStyle Style;
		FVector Location;	// meters
		float Strafe;		// meters to each side
		float Bob;			// meters
	};
	const FTargetSpot Spots[] = {
		// A row of standing dummies to warm up on.
		{ EShootingTargetStyle::Dummy, FVector(32.f, -28.f, 0.f), 0.f, 0.f },
		{ EShootingTargetStyle::Dummy, FVector(36.f, -14.f, 0.f), 0.f, 0.f },
		{ EShootingTargetStyle::Dummy, FVector(40.f, 0.f, 0.f), 0.f, 0.f },
		{ EShootingTargetStyle::Dummy, FVector(44.f, 14.f, 0.f), 0.f, 0.f },
		// Dummies that strafe, further out.
		{ EShootingTargetStyle::Dummy, FVector(65.f, -18.f, 0.f), 8.f, 0.f },
		{ EShootingTargetStyle::Dummy, FVector(78.f, 6.f, 0.f), 10.f, 0.f },
		// Drones up high. Hover (Space in the air) to shoot them.
		{ EShootingTargetStyle::Drone, FVector(60.f, -38.f, 15.f), 0.f, 2.f },
		{ EShootingTargetStyle::Drone, FVector(85.f, -12.f, 25.f), 6.f, 3.f },
		{ EShootingTargetStyle::Drone, FVector(92.f, 18.f, 35.f), 0.f, 4.f },
	};

	for (const FTargetSpot& Spot : Spots)
	{
		// Deferred so the style is set before the target builds its shape.
		const FTransform SpotTransform = FTransform(FRotator(0.f, 180.f, 0.f), Spot.Location * Meter) * ArenaTransform;
		AShootingTarget* Target = World->SpawnActorDeferred<AShootingTarget>(AShootingTarget::StaticClass(), SpotTransform, this);
		if (!Target)
		{
			continue;
		}
		Target->Style = Spot.Style;
		Target->MaxHealth = Spot.Style == EShootingTargetStyle::Drone ? 180.f : 300.f;
		Target->StrafeDistance = Spot.Strafe * Meter;
		Target->BobHeight = Spot.Bob * Meter;
		Target->FinishSpawning(SpotTransform);
	}

	const FTransform CrateTransform = FTransform(FRotator(0.f, 20.f, 0.f), FVector(14.f, -9.f, 0.f) * Meter) * ArenaTransform;
	World->SpawnActor<AAmmoCrate>(AAmmoCrate::StaticClass(), CrateTransform);
}
