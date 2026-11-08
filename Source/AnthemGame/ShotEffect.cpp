#include "ShotEffect.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float TracerDuration = 0.06f;
	constexpr float PuffDuration = 0.18f;
	constexpr float SparkDuration = 0.35f;
	constexpr float FlashDuration = 0.08f;
	constexpr float TracerWidth = 0.035f;
}

AShotEffect::AShotEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	InitialLifeSpan = 2.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = ShapeMaterial.Object;

	auto MakePart = [this](FName Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		Part->SetHiddenInGame(true);
		return Part;
	};

	// Parts are placed in world space when the effect starts.
	Tracer = MakePart(TEXT("Tracer"), CylinderMesh.Object);
	Tracer->SetUsingAbsoluteLocation(true);
	Tracer->SetUsingAbsoluteRotation(true);
	Tracer->SetUsingAbsoluteScale(true);

	Puff = MakePart(TEXT("Puff"), SphereMesh.Object);
	for (int32 Index = 0; Index < SparkCount; ++Index)
	{
		UStaticMeshComponent* Spark = MakePart(*FString::Printf(TEXT("Spark%d"), Index), CubeMesh.Object);
		Spark->SetUsingAbsoluteLocation(true);
		Sparks.Add(Spark);
	}

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(RootComponent);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->IntensityUnits = ELightUnits::Candelas;
	Flash->Intensity = 0.f;
	Flash->AttenuationRadius = 500.f;
	Flash->CastShadows = false;
}

UMaterialInstanceDynamic* AShotEffect::Paint(UStaticMeshComponent* Part, const FLinearColor& Color)
{
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Part->SetMaterial(0, Material);
	return Material;
}

void AShotEffect::InitShot(const FVector& Start, const FVector& End, bool bHit, const FVector& Normal, const FLinearColor& Color)
{
	// The cylinder is 100 units tall along Z with its pivot in the middle.
	const FVector Delta = End - Start;
	TracerLength = Delta.Size();
	if (TracerLength > 1.f)
	{
		bHasTracer = true;
		Tracer->SetWorldLocationAndRotation((Start + End) * 0.5f, FRotationMatrix::MakeFromZ(Delta / TracerLength).Rotator());
		Tracer->SetWorldScale3D(FVector(TracerWidth, TracerWidth, TracerLength / 100.f));
		Tracer->SetHiddenInGame(false);
		Paint(Tracer, Color);
	}

	if (bHit)
	{
		StartImpact(End, Normal, Color, 1.f);
	}
}

void AShotEffect::InitBurst(const FVector& Location, const FLinearColor& Color, float Size)
{
	StartImpact(Location, FVector::UpVector, Color, Size);
}

void AShotEffect::StartImpact(const FVector& Location, const FVector& Normal, const FLinearColor& Color, float Size)
{
	bHasImpact = true;
	ImpactSize = Size;

	// Pull the puff out of the surface a little so it isn't half buried.
	SetActorLocation(Location + Normal * 5.f * Size);
	Puff->SetHiddenInGame(false);
	Puff->SetRelativeScale3D(FVector(0.1f * Size));
	Paint(Puff, Color);

	Flash->SetLightColor(Color);
	Flash->SetAttenuationRadius(500.f * Size);
	Flash->SetIntensity(800.f * Size);

	// Sparks spray off the surface in a rough cone around its normal.
	const FVector Facing = Normal.IsNearlyZero() ? FVector::UpVector : Normal;
	const FLinearColor SparkColor = FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.5f);
	for (UStaticMeshComponent* Spark : Sparks)
	{
		const FVector Direction = FMath::VRandCone(Facing, FMath::DegreesToRadians(70.f));
		SparkVelocities.Add(Direction * FMath::FRandRange(600.f, 1400.f) * FMath::Sqrt(Size));
		Spark->SetWorldLocation(GetActorLocation());
		Spark->SetRelativeScale3D(FVector(0.04f * Size));
		Spark->SetHiddenInGame(false);
		Paint(Spark, SparkColor);
	}
}

void AShotEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (bHasTracer)
	{
		// The tracer thins out and vanishes almost immediately.
		const float Alpha = Age / TracerDuration;
		if (Alpha >= 1.f)
		{
			Tracer->SetHiddenInGame(true);
			bHasTracer = false;
		}
		else
		{
			const float Width = TracerWidth * (1.f - Alpha);
			Tracer->SetWorldScale3D(FVector(Width, Width, TracerLength / 100.f));
		}
	}

	if (bHasImpact)
	{
		// The puff swells, then shrinks away.
		const float PuffAlpha = FMath::Clamp(Age / PuffDuration, 0.f, 1.f);
		const float PuffScale = FMath::Sin(PuffAlpha * UE_PI) * 0.35f + 0.1f * (1.f - PuffAlpha);
		Puff->SetRelativeScale3D(FVector(PuffScale * ImpactSize));
		Puff->SetHiddenInGame(PuffAlpha >= 1.f);

		Flash->SetIntensity(800.f * ImpactSize * FMath::Max(0.f, 1.f - Age / (FlashDuration * ImpactSize)));

		const float SparkAlpha = FMath::Clamp(Age / SparkDuration, 0.f, 1.f);
		for (int32 Index = 0; Index < Sparks.Num(); ++Index)
		{
			SparkVelocities[Index].Z -= 980.f * DeltaSeconds;
			Sparks[Index]->SetWorldLocation(Sparks[Index]->GetComponentLocation() + SparkVelocities[Index] * DeltaSeconds);
			Sparks[Index]->SetRelativeScale3D(FVector(0.04f * ImpactSize * (1.f - SparkAlpha)));
			Sparks[Index]->SetHiddenInGame(SparkAlpha >= 1.f);
		}
	}

	if (Age > SparkDuration * FMath::Max(1.f, ImpactSize) && !bHasTracer)
	{
		Destroy();
	}
}
