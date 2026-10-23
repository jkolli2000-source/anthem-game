#include "EnemyProjectile.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "JavelinCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "UObject/ConstructorHelpers.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	InitialLifeSpan = 4.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// A stretched sphere pointing along X, the direction of travel.
	Bolt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bolt"));
	Bolt->SetupAttachment(RootComponent);
	Bolt->SetStaticMesh(SphereMesh.Object);
	Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bolt->SetGenerateOverlapEvents(false);
	Bolt->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->Intensity = 150.f;
	Glow->AttenuationRadius = 350.f;
	Glow->CastShadows = false;
}

void AEnemyProjectile::Launch(const FVector& InVelocity, float InDamage, const FLinearColor& Color, float Size)
{
	Velocity = InVelocity;
	Damage = InDamage;
	BoltColor = Color;
	Radius = 15.f * Size;

	SetActorRotation(Velocity.Rotation());
	Bolt->SetRelativeScale3D(FVector(0.7f, 0.25f, 0.25f) * Size);
	if (UMaterialInstanceDynamic* Material = Bolt->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	Glow->SetLightColor(Color);
}

void AEnemyProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Sweep a small ball along this frame's path. The Pawn channel hits the player, enemies and the world,
	// but not triggers like the ammo crate's pickup zone.
	const FVector Start = GetActorLocation();
	const FVector End = Start + Velocity * DeltaSeconds;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyProjectile), false, GetOwner());
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params))
	{
		Explode(Hit);
		return;
	}
	SetActorLocation(End);
}

void AEnemyProjectile::Explode(const FHitResult& Hit)
{
	// Only the player takes damage. Hitting another enemy or a wall just pops the bolt.
	AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(Hit.GetActor());
	if (Javelin)
	{
		UGameplayStatics::ApplyPointDamage(Javelin, Damage, Velocity.GetSafeNormal(), Hit, nullptr, this, UDamageType::StaticClass());
	}

	if (AShotEffect* Effect = GetWorld()->SpawnActor<AShotEffect>(Hit.ImpactPoint, FRotator::ZeroRotator))
	{
		Effect->InitShot(Hit.ImpactPoint, Hit.ImpactPoint, true, Hit.ImpactNormal, BoltColor);
	}
	Destroy();
}
