#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"

UJavelinHeatComponent::UJavelinHeatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UJavelinHeatComponent::BeginPlay()
{
	Super::BeginPlay();
	Movement = GetOwner() ? GetOwner()->FindComponentByClass<UJavelinMovementComponent>() : nullptr;
}

void UJavelinHeatComponent::AddCoolingSource(const UObject* Source, float CoolingPerSecond)
{
	if (Source)
	{
		CoolingSources.Add(Source, CoolingPerSecond);
	}
}

void UJavelinHeatComponent::RemoveCoolingSource(const UObject* Source)
{
	CoolingSources.Remove(Source);
}

void UJavelinHeatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const EJavelinMoveMode Mode = Movement ? Movement->GetJavelinMode() : EJavelinMoveMode::None;
	float Rate = 0.f;
	bool bHeating = false;
	bBeingCooled = false;

	if (Mode == EJavelinMoveMode::Flight)
	{
		if (Movement->IsDiving())
		{
			Rate -= DiveCoolingPerSecond;
			bBeingCooled = true;
		}
		else
		{
			Rate += FlightHeatPerSecond;
			bHeating = true;
		}
	}
	else if (Mode == EJavelinMoveMode::Hover)
	{
		Rate += HoverHeatPerSecond;
		bHeating = true;
	}

	TimeSinceHeating = bHeating ? 0.f : TimeSinceHeating + DeltaTime;
	if (Mode == EJavelinMoveMode::None && TimeSinceHeating >= CoolingDelay)
	{
		Rate -= bOverheated ? OverheatedCoolingPerSecond : PassiveCoolingPerSecond;
	}

	float ZoneCooling = 0.f;
	for (auto It = CoolingSources.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		ZoneCooling += It->Value;
	}
	if (ZoneCooling > 0.f)
	{
		Rate -= ZoneCooling;
		bBeingCooled = true;
	}

	Heat = FMath::Clamp(Heat + Rate * DeltaTime, 0.f, MaxHeat);

	if (!bOverheated && Heat >= MaxHeat)
	{
		bOverheated = true;
		OnOverheated.Broadcast();
	}
	else if (bOverheated && Heat <= 0.f)
	{
		bOverheated = false;
		OnCooledDown.Broadcast();
	}
}
