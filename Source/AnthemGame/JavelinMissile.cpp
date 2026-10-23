#include "JavelinMissile.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "JavelinAbilityComponent.h"
#include "JavelinWeaponComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float MissileCollisionRadius = 12.f;

	/** With no target left, it blows up when it gets this close to its aim point. */
	constexpr float AimPointArrivalDistance = 60.f;
}

AJavelinMissile::AJavelinMissile()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	InitialLifeSpan = 10.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	auto MakePart = [this](FName Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Body);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocationAndRotation(Location, Rotation);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		return Part;
	};

	// The actor faces along its velocity. Basic shapes stand along Z, so they're pitched to lie along X.
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Tube = MakePart(TEXT("Tube"), CylinderMesh.Object, FVector::ZeroVector, FVector(0.08f, 0.08f, 0.4f), FRotator(90.f, 0.f, 0.f));
	Nose = MakePart(TEXT("Nose"), ConeMesh.Object, FVector(25.f, 0.f, 0.f), FVector(0.08f, 0.08f, 0.1f), FRotator(-90.f, 0.f, 0.f));
	Flame = MakePart(TEXT("Flame"), ConeMesh.Object, FVector(-32.f, 0.f, 0.f), FVector(0.07f, 0.07f, 0.25f), FRotator(90.f, 0.f, 0.f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(-35.f, 0.f, 0.f));
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->Intensity = 200.f;
	Glow->AttenuationRadius = 400.f;
	Glow->CastShadows = false;
}

void AJavelinMissile::Launch(const FVector& InVelocity, AActor* InTarget, const FVector& InAimPoint, float InDamage, float InRadius, const FLinearColor& Color)
{
	Velocity = InVelocity;
	Target = InTarget;
	AimPoint = InAimPoint;
	Damage = InDamage;
	Radius = InRadius;
	TrailColor = Color;

	SetActorRotation(Velocity.Rotation());
	auto Paint = [](UStaticMeshComponent* Part, const FLinearColor& PartColor)
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), PartColor);
		}
	};
	Paint(Tube, FLinearColor(0.5f, 0.52f, 0.55f));
	Paint(Nose, Color);
	Paint(Flame, FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.4f));
	Glow->SetLightColor(Color);
}

void AJavelinMissile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded)
	{
		return;
	}

	Age += DeltaSeconds;
	const FVector Location = GetActorLocation();
	if (Age >= MaxLifetime)
	{
		Explode(Location);
		return;
	}

	const float Speed = FMath::Min(MaxSpeed, static_cast<float>(Velocity.Size()) + Acceleration * DeltaSeconds);
	FVector Direction = Velocity.GetSafeNormal();

	if (Age > HomingDelay)
	{
		// Follow the target while it lives. Afterwards, finish the trip to where it was last seen.
		const AActor* Goal = Target.Get();
		if (UJavelinWeaponComponent::IsLivingTarget(Goal))
		{
			AimPoint = UJavelinAbilityComponent::GetTargetCenter(Goal);
		}
		else
		{
			Target.Reset();
		}

		const FVector ToGoal = AimPoint - Location;
		if (!Target.IsValid() && ToGoal.Size() < AimPointArrivalDistance)
		{
			Explode(Location);
			return;
		}

		const float TurnRate = StartTurnRate + TurnRateGrowth * (Age - HomingDelay);
		Direction = FMath::VInterpNormalRotationTo(Direction, ToGoal.GetSafeNormal(), DeltaSeconds, TurnRate);
	}
	Velocity = Direction * Speed;

	const FVector End = Location + Velocity * DeltaSeconds;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinMissile), false, GetOwner());
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Location, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(MissileCollisionRadius), Params))
	{
		Explode(FVector(Hit.Location) + FVector(Hit.ImpactNormal) * 10.f);
		return;
	}
	SetActorLocationAndRotation(End, Direction.Rotation());

	// The exhaust flickers.
	const float Flicker = 1.f + 0.3f * FMath::Sin(Age * 60.f);
	Flame->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.25f * Flicker));
}

void AJavelinMissile::Explode(const FVector& Location)
{
	bExploded = true;
	UJavelinAbilityComponent::ApplyBlastDamage(GetWorld(), Location, Damage, Radius, this, GetInstigator());
	if (AShotEffect* Effect = GetWorld()->SpawnActor<AShotEffect>(Location, FRotator::ZeroRotator))
	{
		Effect->InitBurst(Location, TrailColor, Radius / 80.f);
	}
	Destroy();
}
