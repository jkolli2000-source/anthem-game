#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShotEffect.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Short-lived visual for one shot, built from basic shapes so it needs no
 * particle assets: a tracer line from the muzzle, and where it lands a flash
 * of light, a puff that grows and fades, and sparks that fly off the surface.
 *
 * InitBurst plays just the impact part, bigger, for things blowing up.
 */
UCLASS(NotPlaceable)
class ANTHEMGAME_API AShotEffect : public AActor
{
	GENERATED_BODY()

public:
	AShotEffect();

	virtual void Tick(float DeltaSeconds) override;

	/** Tracer from Start to End. If bHit, also an impact at End facing along Normal. */
	void InitShot(const FVector& Start, const FVector& End, bool bHit, const FVector& Normal, const FLinearColor& Color);

	/** A big impact with no tracer, for explosions. Size 1 is a normal bullet impact. */
	void InitBurst(const FVector& Location, const FLinearColor& Color, float Size);

	static constexpr int32 SparkCount = 6;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Tracer;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Puff;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Flash;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Sparks;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	void StartImpact(const FVector& Location, const FVector& Normal, const FLinearColor& Color, float Size);
	UMaterialInstanceDynamic* Paint(UStaticMeshComponent* Part, const FLinearColor& Color);

	TArray<FVector> SparkVelocities;
	float Age = 0.f;
	float ImpactSize = 1.f;
	float TracerLength = 0.f;
	bool bHasTracer = false;
	bool bHasImpact = false;
};
