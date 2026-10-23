#include "JavelinGrenade.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "JavelinAbilityComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float GrenadeCollisionRadius = 10.f;
}

AJavelinGrenade::AJavelinGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	InitialLifeSpan = 10.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// A dark ball with a glowing band around its middle, about the size of a fist.
	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	Shell->SetupAttachment(RootComponent);
	Shell->SetStaticMesh(SphereMesh.Object);
	Shell->SetRelativeScale3D(FVector(0.2f));
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shell->SetGenerateOverlapEvents(false);

	Band = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Band"));
	Band->SetupAttachment(Shell);
	Band->SetStaticMesh(CylinderMesh.Object);
	Band->SetRelativeScale3D(FVector(1.08f, 1.08f, 0.25f));
	Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Band->SetGenerateOverlapEvents(false);
	Band->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->Intensity = 120.f;
	Glow->AttenuationRadius = 300.f;
	Glow->CastShadows = false;
}

void AJavelinGrenade::Throw(const FVector& InVelocity, float InDamage, float InRadius, const FLinearColor& Color)
{
	Velocity = InVelocity;
	Damage = InDamage;
	Radius = InRadius;
	GlowColor = Color;

	if (UMaterialInstanceDynamic* Material = Shell->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.06f, 0.06f, 0.07f));
	}
	if (UMaterialInstanceDynamic* Material = Band->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	Glow->SetLightColor(Color);
}

void AJavelinGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded)
	{
		return;
	}

	Age += DeltaSeconds;
	if (Age >= FuseTime)
	{
		Explode(GetActorLocation());
		return;
	}

	// Gravity, then sweep along this frame's path. Visibility hits the ground, walls and enemy bodies,
	// but not the player's capsule (it's ignored anyway) or pickup triggers.
	Velocity.Z += GetWorld()->GetGravityZ() * DeltaSeconds;
	const FVector Start = GetActorLocation();
	const FVector End = Start + Velocity * DeltaSeconds;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinGrenade), false, GetOwner());
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(GrenadeCollisionRadius), Params))
	{
		// Go off just off the surface, so the wall or floor it hit doesn't block its own blast.
		Explode(FVector(Hit.Location) + FVector(Hit.ImpactNormal) * 20.f);
		return;
	}
	SetActorLocation(End);

	// Tumble end over end, and blink faster as the fuse runs down.
	Shell->AddLocalRotation(FRotator(-720.f * DeltaSeconds, 0.f, 200.f * DeltaSeconds));
	const float BlinkRate = 8.f + 20.f * Age / FuseTime;
	Glow->SetIntensity(FMath::Sin(Age * BlinkRate) > 0.f ? 160.f : 30.f);
}

void AJavelinGrenade::Explode(const FVector& Location)
{
	bExploded = true;
	UJavelinAbilityComponent::ApplyBlastDamage(GetWorld(), Location, Damage, Radius, this, GetInstigator());

	// A big fireball sized to roughly match the blast.
	if (AShotEffect* Effect = GetWorld()->SpawnActor<AShotEffect>(Location, FRotator::ZeroRotator))
	{
		Effect->InitBurst(Location, GlowColor, Radius / 80.f);
	}
	if (AShotEffect* Core = GetWorld()->SpawnActor<AShotEffect>(Location, FRotator::ZeroRotator))
	{
		Core->InitBurst(Location, FLinearColor(1.f, 0.95f, 0.7f), Radius / 200.f);
	}
	Destroy();
}
