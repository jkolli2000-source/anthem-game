#include "PickupOrb.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "JavelinCharacter.h"
#include "JavelinSuitHealthComponent.h"
#include "JavelinWeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor HealthOrbColor(0.15f, 1.f, 0.3f);
	const FLinearColor AmmoOrbColor(1.f, 0.95f, 0.8f);
	constexpr float PickupDistance = 120.f;
}

APickupOrb::APickupOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Orb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Orb"));
	Orb->SetupAttachment(RootComponent);
	Orb->SetStaticMesh(SphereMesh.Object);
	Orb->SetRelativeScale3D(FVector(0.3f));
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb->SetGenerateOverlapEvents(false);
	Orb->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->Intensity = 120.f;
	Glow->AttenuationRadius = 400.f;
	Glow->CastShadows = false;
}

void APickupOrb::BeginPlay()
{
	Super::BeginPlay();

	const FLinearColor Color = Type == EPickupOrbType::Health ? HealthOrbColor : AmmoOrbColor;
	if (UMaterialInstanceDynamic* Material = Orb->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	Glow->SetLightColor(Color);

	// Float a little above the ground under where it dropped.
	RestLocation = GetActorLocation();
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PickupOrb), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Ground, RestLocation, RestLocation - FVector(0.f, 0.f, 5000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		RestLocation = Ground.ImpactPoint + FVector(0.f, 0.f, 70.f);
	}

	// Pop out with a little hop in a random direction, so drops from one enemy spread out.
	Velocity = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 250.f + FVector(0.f, 0.f, 300.f);
	if (Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);
	}
}

bool APickupOrb::TryGiveTo(AJavelinCharacter* Javelin) const
{
	if (!Javelin || Javelin->GetSuitHealth()->IsDown())
	{
		return false;
	}
	if (Type == EPickupOrbType::Health)
	{
		return Javelin->GetSuitHealth()->RepairArmor(HealAmount);
	}
	return Javelin->GetWeapon()->AddReserveAmmo(AmmoAmount);
}

void APickupOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector Location = GetActorLocation();

	// Only orbs you can use get pulled in, so a full suit doesn't vacuum up health it can't take.
	bool bWanted = false;
	if (Javelin && !Javelin->GetSuitHealth()->IsDown())
	{
		bWanted = Type == EPickupOrbType::Health
			? Javelin->GetSuitHealth()->GetArmor() < Javelin->GetSuitHealth()->MaxArmor
			: Javelin->GetWeapon()->GetReserveAmmo() < Javelin->GetWeapon()->MaxReserveAmmo;
	}

	const FVector ToPlayer = Javelin ? Javelin->GetActorLocation() - Location : FVector::ZeroVector;
	if (bWanted && ToPlayer.Size() < MagnetRange)
	{
		// Speed up as it closes in, so it can catch a sprinting suit.
		const float Speed = FMath::Lerp(2500.f, 900.f, ToPlayer.Size() / FMath::Max(MagnetRange, 1.f));
		SetActorLocation(Location + ToPlayer.GetSafeNormal() * FMath::Min(Speed * DeltaSeconds, ToPlayer.Size()));
		if (ToPlayer.Size() < PickupDistance && TryGiveTo(Javelin))
		{
			const FLinearColor Color = Type == EPickupOrbType::Health ? HealthOrbColor : AmmoOrbColor;
			if (AShotEffect* Effect = GetWorld()->SpawnActor<AShotEffect>(GetActorLocation(), FRotator::ZeroRotator))
			{
				Effect->InitBurst(GetActorLocation(), Color, 1.2f);
			}
			Destroy();
		}
		return;
	}

	// Settle toward the resting spot with a gentle bob.
	const FVector Rest = RestLocation + FVector(0.f, 0.f, 15.f * FMath::Sin(Age * 3.f));
	if (Age < 0.6f)
	{
		Velocity.Z -= 980.f * DeltaSeconds;
		SetActorLocation(Location + Velocity * DeltaSeconds);
	}
	else
	{
		SetActorLocation(FMath::VInterpTo(Location, FVector(Location.X, Location.Y, Rest.Z), DeltaSeconds, 4.f));
	}

	// Blink out over the last few seconds.
	const float Remaining = GetLifeSpan();
	if (Lifetime > 0.f && Remaining > 0.f && Remaining < 4.f)
	{
		Orb->SetVisibility(FMath::Fmod(Remaining, 0.4f) > 0.15f);
	}
}
