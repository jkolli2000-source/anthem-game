#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmmoCrate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Walk up to it to refill your gun's magazine and reserve ammo. */
UCLASS()
class ANTHEMGAME_API AAmmoCrate : public AActor
{
	GENERATED_BODY()

public:
	AAmmoCrate();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Crate;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Stripe;

	/** Standing inside this box refills ammo. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> PickupZone;

	UPROPERTY(EditAnywhere, Category = "Ammo Crate")
	FLinearColor CrateColor = FLinearColor(0.15f, 0.2f, 0.12f);

	UPROPERTY(EditAnywhere, Category = "Ammo Crate")
	FLinearColor StripeColor = FLinearColor(1.f, 0.7f, 0.1f);

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Also checks every second, so standing next to the crate keeps you topped up. */
	void RefillOverlapping();
	void Refill(AActor* Actor);

	FTimerHandle RefillTimer;
};
