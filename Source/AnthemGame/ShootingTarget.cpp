#include "ShootingTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName WeakSpotTag(TEXT("WeakSpot"));
}

AShootingTarget::AShootingTarget()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	SphereMesh = SphereFinder.Object;
	BaseMaterial = ShapeMaterial.Object;

	for (int32 Index = 0; Index < MaxParts; ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), Index));
		Part->SetupAttachment(RootComponent);
		Part->SetCollisionProfileName(TEXT("BlockAll"));
		Parts.Add(Part);
	}
}

void AShootingTarget::SetPart(int32 Index, UStaticMesh* Mesh, const FVector& Location, const FVector& Size, bool bWeakSpot)
{
	// Basic shapes are 100 units across, so Size is in centimeters.
	UStaticMeshComponent* Part = Parts[Index];
	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Size / 100.f);
	Part->SetVisibility(true);
	Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Part->ComponentTags.Remove(WeakSpotTag);
	if (bWeakSpot)
	{
		Part->ComponentTags.Add(WeakSpotTag);
	}

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	const FLinearColor Color = bWeakSpot ? WeakSpotColor : BodyColor;
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Part->SetMaterial(0, Material);
	PartMaterials[Index] = Material;
	PartColors[Index] = Color;
}

void AShootingTarget::BuildParts()
{
	PartMaterials.SetNum(MaxParts);
	PartColors.SetNum(MaxParts);
	for (UStaticMeshComponent* Part : Parts)
	{
		Part->SetVisibility(false);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	switch (Style)
	{
	case EShootingTargetStyle::Drone:
		// Round body with a wide ring, an antenna, and a glowing eye on the front. The actor sits at its center.
		SetPart(0, SphereMesh, FVector::ZeroVector, FVector(110.f, 110.f, 90.f), false);
		SetPart(1, CylinderMesh, FVector::ZeroVector, FVector(160.f, 160.f, 14.f), false);
		SetPart(2, CubeMesh, FVector(-10.f, 0.f, 65.f), FVector(8.f, 8.f, 50.f), false);
		SetPart(3, SphereMesh, FVector(50.f, 0.f, 8.f), FVector(40.f), true);
		TopHeight = 90.f;
		break;

	default:
		// Training dummy on a post. The actor sits at its feet.
		SetPart(0, CylinderMesh, FVector(0.f, 0.f, 40.f), FVector(22.f, 22.f, 80.f), false);
		SetPart(1, CubeMesh, FVector(0.f, 0.f, 88.f), FVector(40.f, 60.f, 20.f), false);
		SetPart(2, CubeMesh, FVector(0.f, 0.f, 145.f), FVector(50.f, 85.f, 95.f), false);
		SetPart(3, SphereMesh, FVector(0.f, 0.f, 218.f), FVector(48.f), true);
		TopHeight = 245.f;
		break;
	}
}

void AShootingTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildParts();
}

void AShootingTarget::BeginPlay()
{
	Super::BeginPlay();

	// Construction doesn't run again for targets saved in a level, so rebuild here to get fresh flash materials.
	BuildParts();
	Health = MaxHealth;
	Origin = GetActorLocation();
	StrafeAxis = GetActorRightVector();

	// Spread out the motion so neighboring targets don't move in step.
	MoveTime = FMath::FRandRange(0.f, 10.f);
}

void AShootingTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	MoveTime += DeltaSeconds;

	const float Strafe = StrafeDistance * FMath::Sin(MoveTime * StrafeFrequency * UE_TWO_PI);
	const float Bob = BobHeight * FMath::Sin(MoveTime * 0.4f * UE_TWO_PI);
	SetActorLocation(Origin + StrafeAxis * Strafe + FVector(0.f, 0.f, Bob));

	// Turn to face the player.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		if (!ToPlayer.IsNearlyZero())
		{
			const FRotator Facing(0.f, ToPlayer.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Facing, DeltaSeconds, 3.f));
		}
	}

	// Flash white when hit.
	if (HitFlash > 0.f)
	{
		HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds * 8.f);
		for (int32 Index = 0; Index < PartMaterials.Num(); ++Index)
		{
			if (PartMaterials[Index])
			{
				PartMaterials[Index]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(PartColors[Index], FLinearColor::White, HitFlash));
			}
		}
	}
}

float AShootingTarget::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsAlive() || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - DamageAmount);
	HitFlash = 1.f;
	LastHitTime = GetWorld()->GetTimeSeconds();

	if (!IsAlive())
	{
		Die();
	}
	return DamageAmount;
}

void AShootingTarget::Die()
{
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, Style == EShootingTargetStyle::Drone ? 0.f : TopHeight * 0.6f);
	if (AShotEffect* Effect = GetWorld()->SpawnActor<AShotEffect>(Center, FRotator::ZeroRotator))
	{
		Effect->InitBurst(Center, WeakSpotColor, 4.f);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AShootingTarget::Respawn, FMath::Max(RespawnDelay, 0.1f));
}

void AShootingTarget::Respawn()
{
	Health = MaxHealth;
	HitFlash = 0.f;
	for (int32 Index = 0; Index < PartMaterials.Num(); ++Index)
	{
		if (PartMaterials[Index])
		{
			PartMaterials[Index]->SetVectorParameterValue(TEXT("Color"), PartColors[Index]);
		}
	}
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

float AShootingTarget::GetTimeSinceHit() const
{
	return GetWorld()->GetTimeSeconds() - LastHitTime;
}

FVector AShootingTarget::GetHealthBarLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, TopHeight + 40.f);
}
