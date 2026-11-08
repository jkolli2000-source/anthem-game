#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JavelinGrenade.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The javelin's frag grenade. It arcs under gravity and blows up the moment
 * it hits anything (or after a few seconds in the air), damaging every enemy
 * in the blast. The player who threw it is never hurt.
 */
UCLASS(NotPlaceable)
class ANTHEMGAME_API AJavelinGrenade : public AActor
{
	GENERATED_BODY()

public:
	AJavelinGrenade();

	virtual void Tick(float DeltaSeconds) override;

	void Throw(const FVector& InVelocity, float InDamage, float InRadius, const FLinearColor& Color);

	/** It goes off in the air if it hasn't hit anything after this long. */
	UPROPERTY(EditAnywhere, Category = "Grenade", meta = (ClampMin = "0.1", Units = "s"))
	float FuseTime = 3.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Band;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

private:
	void Explode(const FVector& Location);

	FVector Velocity = FVector::ZeroVector;
	FLinearColor GlowColor = FLinearColor::White;
	float Damage = 0.f;
	float Radius = 500.f;
	float Age = 0.f;
	bool bExploded = false;
};
