#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DustEffect.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * A short puff of dust or smoke built from basic shapes, so it needs no
 * particle assets: a few soft balls that billow outward, rise a little,
 * swell and then shrink away.
 *
 * Used for landings, footsteps, hover jets blowing on the ground, flight
 * crashes, and smoke trails from downed drones.
 */
UCLASS(NotPlaceable)
class ANTHEMGAME_API ADustEffect : public AActor
{
	GENERATED_BODY()

public:
	ADustEffect();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Spawns a puff. Radius is how far the dust spreads out (0 for a single puff that just rises),
	 * Size is how big each ball gets (1 is about a meter across), Count is how many balls (up to MaxPuffs).
	 */
	static ADustEffect* Spawn(UWorld* World, const FVector& Location, float Radius, float Size, int32 Count, const FLinearColor& Color, float Lifetime = 0.8f);

	static constexpr int32 MaxPuffs = 10;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Puffs;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

private:
	void Start(float Radius, float Size, int32 Count, const FLinearColor& Color, float Lifetime);

	TArray<FVector> Velocities;
	TArray<float> Sizes;
	int32 ActiveCount = 0;
	float Age = 0.f;
	float Duration = 0.8f;
};
