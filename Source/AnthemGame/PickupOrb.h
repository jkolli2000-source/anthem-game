#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupOrb.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EPickupOrbType : uint8
{
	/** Green. Repairs armor. */
	Health,
	/** White. Adds reserve ammo. */
	Ammo
};

/**
 * A glowing orb dropped by enemies, like Anthem's health and ammo pickups.
 * It floats and bobs, gets pulled toward you when you come close, and is
 * picked up on touch. If you don't need it (full armor or full ammo) it stays
 * put for later. It fades away after a while.
 */
UCLASS()
class ANTHEMGAME_API APickupOrb : public AActor
{
	GENERATED_BODY()

public:
	APickupOrb();

	virtual void Tick(float DeltaSeconds) override;

	/** Set before spawning finishes, or on a placed orb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	EPickupOrbType Type = EPickupOrbType::Health;

	/** Armor repaired by a health orb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0"))
	float HealAmount = 80.f;

	/** Reserve rounds added by an ammo orb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0"))
	int32 AmmoAmount = 45;

	/** Within this distance the orb flies to you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0", Units = "cm"))
	float MagnetRange = 700.f;

	/** Seconds before an orb nobody picked up disappears. 0 keeps it forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0", Units = "s"))
	float Lifetime = 40.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

private:
	/** Gives the orb to the javelin. Returns false if it didn't need it. */
	bool TryGiveTo(class AJavelinCharacter* Javelin) const;

	FVector RestLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
};
