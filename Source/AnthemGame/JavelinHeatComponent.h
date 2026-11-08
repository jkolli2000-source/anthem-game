#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JavelinHeatComponent.generated.h"

class UJavelinMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FJavelinHeatEvent);

/**
 * Suit heat. Flying and hovering build heat; the ground, dives and cooling
 * zones (water, waterfalls) remove it. At max heat the suit overheats and
 * can't fly or hover until it has cooled all the way down.
 *
 * Milestone 2 moves this into GAS attributes and effects.
 */
UCLASS(ClassGroup = (Javelin), meta = (BlueprintSpawnableComponent))
class ANTHEMGAME_API UJavelinHeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJavelinHeatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "1"))
	float MaxHeat = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "0"))
	float FlightHeatPerSecond = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "0"))
	float HoverHeatPerSecond = 4.f;

	/** Cooling on the ground or while falling, after CoolingDelay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "0"))
	float PassiveCoolingPerSecond = 20.f;

	/** Cooling while overheated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "0"))
	float OverheatedCoolingPerSecond = 25.f;

	/** Seconds after you stop flying or hovering before passive cooling starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "0", Units = "s"))
	float CoolingDelay = 0.75f;

	/** Cooling while diving (replaces flight heat gain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heat", meta = (ClampMin = "0"))
	float DiveCoolingPerSecond = 15.f;

	UFUNCTION(BlueprintPure, Category = "Heat")
	float GetHeat() const { return Heat; }

	UFUNCTION(BlueprintPure, Category = "Heat")
	float GetHeatPercent() const { return Heat / MaxHeat; }

	UFUNCTION(BlueprintPure, Category = "Heat")
	bool IsOverheated() const { return bOverheated; }

	/** True while a dive or a cooling zone is pulling heat out. */
	UFUNCTION(BlueprintPure, Category = "Heat")
	bool IsBeingCooled() const { return bBeingCooled; }

	/** Adds a cooling zone the suit is inside. Calling again with the same source replaces its rate. */
	void AddCoolingSource(const UObject* Source, float CoolingPerSecond);
	void RemoveCoolingSource(const UObject* Source);

	UPROPERTY(BlueprintAssignable, Category = "Heat")
	FJavelinHeatEvent OnOverheated;

	UPROPERTY(BlueprintAssignable, Category = "Heat")
	FJavelinHeatEvent OnCooledDown;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UJavelinMovementComponent> Movement;

	TMap<TWeakObjectPtr<const UObject>, float> CoolingSources;

	float Heat = 0.f;
	float TimeSinceHeating = 0.f;
	bool bOverheated = false;
	bool bBeingCooled = false;
};
