#include "EnemyEncounter.h"
#include "EnemyDrone.h"
#include "EnemySoldier.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ShotEffect.h"

AEnemyEncounter::AEnemyEncounter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AEnemyEncounter::BeginPlay()
{
	Super::BeginPlay();
	SpawnWave();
}

void AEnemyEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Enemies.RemoveAll([](const TWeakObjectPtr<AJavelinEnemy>& Enemy) { return !Enemy.IsValid() || !Enemy->IsAlive(); });

	const float Now = GetWorld()->GetTimeSeconds();
	if (Enemies.Num() == 0 && NextWaveTime < 0.f)
	{
		NextWaveTime = Now + TimeBetweenWaves;
	}
	else if (NextWaveTime >= 0.f && Now >= NextWaveTime)
	{
		SpawnWave();
	}
}

FVector AEnemyEncounter::PickSpawnPoint(float Height) const
{
	const FVector Center = GetActorLocation();
	const FVector2D Offset = FMath::RandPointInCircle(CampRadius);
	FVector Point = Center + FVector(Offset.X, Offset.Y, 0.f);

	// Find the ground (or the top of a crate) under the spot.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EncounterSpawn), false, this);
	const FVector Above = Point + FVector(0.f, 0.f, 5000.f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Above, Point - FVector(0.f, 0.f, 5000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Point = Hit.ImpactPoint;
	}
	return Point + FVector(0.f, 0.f, Height);
}

void AEnemyEncounter::SpawnWave()
{
	UWorld* World = GetWorld();
	++Wave;
	NextWaveTime = -1.f;

	const int32 Soldiers = FMath::Min(FirstWaveSoldiers + Wave - 1, MaxSoldiers);
	const int32 Drones = FMath::Min(FirstWaveDrones + (Wave - 1) / 2, MaxDrones);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	auto Spawn = [&](UClass* EnemyClass, float Height)
	{
		const FVector Location = PickSpawnPoint(Height);
		const FRotator Facing(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
		if (AJavelinEnemy* Enemy = World->SpawnActor<AJavelinEnemy>(EnemyClass, Location, Facing, SpawnParams))
		{
			Enemies.Add(Enemy);

			// A flash where each one arrives.
			if (AShotEffect* Effect = World->SpawnActor<AShotEffect>(Location, FRotator::ZeroRotator))
			{
				Effect->InitBurst(Location, Enemy->WeakSpotColor, 2.5f);
			}
		}
	};

	for (int32 Index = 0; Index < Soldiers; ++Index)
	{
		// Spawn a little above the ground; soldiers drop onto it.
		Spawn(AEnemySoldier::StaticClass(), 120.f);
	}
	for (int32 Index = 0; Index < Drones; ++Index)
	{
		Spawn(AEnemyDrone::StaticClass(), DroneSpawnHeight * FMath::FRandRange(0.8f, 1.3f));
	}

	// From the second wave on, they already know where you are if you're close by.
	if (Wave > 1)
	{
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < CampRadius * 2.5f)
		{
			for (const TWeakObjectPtr<AJavelinEnemy>& Enemy : Enemies)
			{
				if (Enemy.IsValid())
				{
					Enemy->Alert();
				}
			}
		}
	}
}

float AEnemyEncounter::GetTimeUntilNextWave() const
{
	return NextWaveTime >= 0.f ? FMath::Max(0.f, NextWaveTime - GetWorld()->GetTimeSeconds()) : -1.f;
}

bool AEnemyEncounter::IsInCombat() const
{
	for (const TWeakObjectPtr<AJavelinEnemy>& Enemy : Enemies)
	{
		if (Enemy.IsValid() && Enemy->IsEngaged())
		{
			return true;
		}
	}
	return false;
}
