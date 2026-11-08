#include "AmmoCrate.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "JavelinWeaponComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AAmmoCrate::AAmmoCrate()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// The actor sits at the crate's base.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Crate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crate"));
	Crate->SetupAttachment(RootComponent);
	Crate->SetStaticMesh(CubeMesh.Object);
	Crate->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	Crate->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.9f));
	Crate->SetCollisionProfileName(TEXT("BlockAll"));

	Stripe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stripe"));
	Stripe->SetupAttachment(RootComponent);
	Stripe->SetStaticMesh(CubeMesh.Object);
	Stripe->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Stripe->SetRelativeScale3D(FVector(1.22f, 0.82f, 0.12f));
	Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupZone = CreateDefaultSubobject<UBoxComponent>(TEXT("PickupZone"));
	PickupZone->SetupAttachment(RootComponent);
	PickupZone->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	PickupZone->InitBoxExtent(FVector(200.f, 180.f, 150.f));
	PickupZone->SetCollisionProfileName(TEXT("Trigger"));
	PickupZone->SetGenerateOverlapEvents(true);
	PickupZone->ShapeColor = FColor(255, 180, 30);
}

void AAmmoCrate::BeginPlay()
{
	Super::BeginPlay();

	auto Paint = [this](UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};
	Paint(Crate, CrateColor);
	Paint(Stripe, StripeColor);

	PickupZone->OnComponentBeginOverlap.AddDynamic(this, &AAmmoCrate::HandleBeginOverlap);
	GetWorldTimerManager().SetTimer(RefillTimer, this, &AAmmoCrate::RefillOverlapping, 1.f, true);
}

void AAmmoCrate::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Refill(OtherActor);
}

void AAmmoCrate::RefillOverlapping()
{
	TArray<AActor*> Overlapping;
	PickupZone->GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		Refill(Actor);
	}
}

void AAmmoCrate::Refill(AActor* Actor)
{
	UJavelinWeaponComponent* Weapon = Actor ? Actor->FindComponentByClass<UJavelinWeaponComponent>() : nullptr;
	if (Weapon && Weapon->RefillAmmo())
	{
		const FVector Top = GetActorLocation() + FVector(0.f, 0.f, 100.f);
		if (AShotEffect* Effect = GetWorld()->SpawnActor<AShotEffect>(Top, FRotator::ZeroRotator))
		{
			Effect->InitBurst(Top, StripeColor, 1.5f);
		}
	}
}
