#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JavelinMissile.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * One homing missile from the javelin's shoulder launcher. It pops out of the
 * pod, then speeds up and curves in toward its target, turning harder the
 * longer it flies so it can't circle forever. With no target (or once the
 * target dies) it heads for the point the crosshair was on when it was fired.
 * It blows up on contact, damaging enemies in a small blast.
 */
UCLASS(NotPlaceable)
class ANTHEMGAME_API AJavelinMissile : public AActor
{
	GENERATED_BODY()

public:
	AJavelinMissile();

	virtual void Tick(float DeltaSeconds) override;

	void Launch(const FVector& InVelocity, AActor* InTarget, const FVector& InAimPoint, float InDamage, float InRadius, const FLinearColor& Color);

	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "100"))
	float MaxSpeed = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float Acceleration = 6000.f;

	/** How long it flies straight out of the pod before it starts homing. */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0", Units = "s"))
	float HomingDelay = 0.15f;

	/** Turn rate when homing starts, in degrees per second. It grows by TurnRateGrowth every second. */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float StartTurnRate = 180.f;

	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0"))
	float TurnRateGrowth = 600.f;

	/** It blows up on its own after this long. */
	UPROPERTY(EditAnywhere, Category = "Missile", meta = (ClampMin = "0.1", Units = "s"))
	float MaxLifetime = 5.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Tube;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Nose;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Flame;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

private:
	void Explode(const FVector& Location);

	TWeakObjectPtr<AActor> Target;
	FVector AimPoint = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FLinearColor TrailColor = FLinearColor::White;
	float Damage = 0.f;
	float Radius = 200.f;
	float Age = 0.f;
	bool bExploded = false;
};
