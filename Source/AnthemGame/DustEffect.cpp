#include "DustEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADustEffect::ADustEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	InitialLifeSpan = 5.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = ShapeMaterial.Object;

	for (int32 Index = 0; Index < MaxPuffs; ++Index)
	{
		UStaticMeshComponent* Puff = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Puff%d"), Index));
		Puff->SetupAttachment(RootComponent);
		Puff->SetStaticMesh(SphereMesh.Object);
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetGenerateOverlapEvents(false);
		Puff->SetCastShadow(false);
		Puff->SetHiddenInGame(true);
		Puffs.Add(Puff);
	}
}

ADustEffect* ADustEffect::Spawn(UWorld* World, const FVector& Location, float Radius, float Size, int32 Count, const FLinearColor& Color, float Lifetime)
{
	if (!World || Count <= 0 || Size <= 0.f)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADustEffect* Effect = World->SpawnActor<ADustEffect>(Location, FRotator::ZeroRotator, Params);
	if (Effect)
	{
		Effect->Start(Radius, Size, Count, Color, Lifetime);
	}
	return Effect;
}

void ADustEffect::Start(float Radius, float Size, int32 Count, const FLinearColor& Color, float Lifetime)
{
	ActiveCount = FMath::Clamp(Count, 1, MaxPuffs);
	Duration = FMath::Max(Lifetime, 0.1f);
	Age = 0.f;

	// One material for every ball, slightly varied in shade per effect so dust doesn't look stamped out.
	Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color * FMath::FRandRange(0.85f, 1.15f));

	const float StartAngle = FMath::FRandRange(0.f, 360.f);
	for (int32 Index = 0; Index < ActiveCount; ++Index)
	{
		// Spread evenly around a ring with some jitter, billowing outward and a little up.
		const float Angle = StartAngle + 360.f * Index / ActiveCount + FMath::FRandRange(-15.f, 15.f);
		const FVector Out = FRotator(0.f, Angle, 0.f).Vector();
		const float Spread = Radius * FMath::FRandRange(0.7f, 1.3f);
		Velocities.Add(Out * Spread * 2.5f / Duration + FVector(0.f, 0.f, FMath::FRandRange(40.f, 120.f) * Size));
		Sizes.Add(Size * FMath::FRandRange(0.75f, 1.25f));

		UStaticMeshComponent* Puff = Puffs[Index];
		Puff->SetMaterial(0, Material);
		Puff->SetRelativeLocation(Out * Radius * 0.15f);
		Puff->SetRelativeScale3D(FVector(0.15f * Sizes[Index]));
		Puff->SetHiddenInGame(false);
	}
}

void ADustEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	const float Alpha = Age / Duration;
	if (Alpha >= 1.f)
	{
		Destroy();
		return;
	}

	// Dust shoots out fast and slows as the air catches it. Balls swell quickly, then shrink away.
	const float Drag = FMath::Exp(-4.f * DeltaSeconds);
	const float Swell = FMath::Sin(FMath::Min(Alpha * 3.f, 1.f) * UE_HALF_PI) * (1.f - Alpha * Alpha);
	for (int32 Index = 0; Index < ActiveCount; ++Index)
	{
		Velocities[Index] *= Drag;
		UStaticMeshComponent* Puff = Puffs[Index];
		Puff->SetRelativeLocation(Puff->GetRelativeLocation() + Velocities[Index] * DeltaSeconds);
		Puff->SetRelativeScale3D(FVector(1.f, 1.f, 0.75f) * FMath::Max(0.01f, Sizes[Index] * Swell));
	}
}
