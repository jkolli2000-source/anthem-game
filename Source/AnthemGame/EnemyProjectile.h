#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A glowing bolt fired by an enemy. It flies in a straight line at a speed
 * you can see and dodge, and damages the player if it hits them. Other
 * enemies just absorb it, so there's no friendly fire.
 */
UCLASS(NotPlaceable)
class ANTHEMGAME_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/** Starts it flying. The owner (the enemy that fired it) is never hit. */
	void Launch(const FVector& InVelocity, float InDamage, const FLinearColor& Color, float Size);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Bolt;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

private:
	void Explode(const FHitResult& Hit);

	FVector Velocity = FVector::ZeroVector;
	FLinearColor BoltColor = FLinearColor::Red;
	float Damage = 0.f;
	float Radius = 15.f;
};
