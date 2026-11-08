#include "CoolingVolume.h"
#include "JavelinHeatComponent.h"

UCoolingZoneComponent::UCoolingZoneComponent()
{
	SetCollisionProfileName(TEXT("Trigger"));
	SetGenerateOverlapEvents(true);
	ShapeColor = FColor(80, 200, 255);
	InitBoxExtent(FVector(500.f, 500.f, 200.f));
}

void UCoolingZoneComponent::BeginPlay()
{
	Super::BeginPlay();
	OnComponentBeginOverlap.AddDynamic(this, &UCoolingZoneComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UCoolingZoneComponent::HandleEndOverlap);
}

void UCoolingZoneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		if (UJavelinHeatComponent* Heat = Actor->FindComponentByClass<UJavelinHeatComponent>())
		{
			Heat->RemoveCoolingSource(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UCoolingZoneComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only count the actor's root (the character's capsule), so extra colliders don't double up.
	if (OtherActor && OtherComp == OtherActor->GetRootComponent())
	{
		if (UJavelinHeatComponent* Heat = OtherActor->FindComponentByClass<UJavelinHeatComponent>())
		{
			Heat->AddCoolingSource(this, CoolingPerSecond);
		}
	}
}

void UCoolingZoneComponent::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherComp == OtherActor->GetRootComponent())
	{
		if (UJavelinHeatComponent* Heat = OtherActor->FindComponentByClass<UJavelinHeatComponent>())
		{
			Heat->RemoveCoolingSource(this);
		}
	}
}

ACoolingVolume::ACoolingVolume()
{
	CoolingZone = CreateDefaultSubobject<UCoolingZoneComponent>(TEXT("CoolingZone"));
	RootComponent = CoolingZone;
}
