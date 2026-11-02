#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JavelinWeaponComponent.generated.h"

/** A damage number floating over something you hit. */
USTRUCT()
struct FJavelinDamageNumber
{
	GENERATED_BODY()

	FVector Location = FVector::ZeroVector;
	float Amount = 0.f;
	bool bWeakSpot = false;
	float SpawnTime = 0.f;
};

/** Called each time the gun fires a round. */
DECLARE_MULTICAST_DELEGATE(FOnJavelinWeaponFired);

/**
 * The javelin's gun: an automatic rifle with a magazine, reserve ammo,
 * reloading, aim-down-sights, spread that blooms while you fire, and recoil.
 *
 * Shots are instant line traces from the camera, so they go where the
 * crosshair points, then checked from the muzzle so you can't shoot through
 * a wall next to you. Hitting a part tagged "WeakSpot" does extra damage,
 * armor plates (parts tagged "Paint.Armor") soak some of it, and damage
 * drops off at long range.
 *
 * Recoil climbs the longer you hold the trigger, drifts a little to one
 * side, and when you let go the view settles most of the way back down.
 * Moving and being in the air widen the spread.
 * It damages anything that takes damage: shooting range targets and enemies.
 *
 * The owner must be an AJavelinCharacter; it decides when the gun can be used
 * (not while flying) and gives the component its muzzle point.
 */
UCLASS(ClassGroup = (Javelin), meta = (BlueprintSpawnableComponent))
class ANTHEMGAME_API UJavelinWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJavelinWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FText WeaponName = NSLOCTEXT("Javelin", "AssaultRifle", "ASSAULT RIFLE");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "0"))
	float Damage = 30.f;

	/** Damage multiplier for hitting a weak spot (the glowing parts on targets). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "1"))
	float WeakSpotMultiplier = 2.f;

	/** Damage multiplier for hitting enemy armor plates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float ArmorPlateMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "100", Units = "cm"))
	float Range = 15000.f;

	/** Full damage out to this distance... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "0", Units = "cm"))
	float FalloffStartRange = 4000.f;

	/** ...dropping to FalloffMinDamage at this distance and beyond. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "0", Units = "cm"))
	float FalloffEndRange = 10000.f;

	/** Fraction of damage left at long range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float FalloffMinDamage = 0.6f;

	/** How hard each round shoves what it hits, in cm/s. Enemies get pushed back and flinch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "0", Units = "cm/s"))
	float HitImpulse = 45.f;

	/** Rounds per minute while the trigger is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Firing", meta = (ClampMin = "1"))
	float FireRate = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 270;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo", meta = (ClampMin = "0", Units = "s"))
	float ReloadTime = 1.8f;

	/** Width of the cone shots land in when firing from the hip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0", Units = "deg"))
	float HipSpread = 2.5f;

	/** Width of the cone while aiming down sights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0", Units = "deg"))
	float AimSpread = 0.6f;

	/** Spread added by each shot, so long bursts get less accurate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0", Units = "deg"))
	float SpreadPerShot = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0", Units = "deg"))
	float MaxBloom = 3.f;

	/** Extra spread from the hip while moving at full sprint. A third as much while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0", Units = "deg"))
	float MovingSpread = 1.6f;

	/** Extra spread while jumping or falling. Hovering is a stable platform and doesn't add any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0", Units = "deg"))
	float AirborneSpread = 1.f;

	/** How fast the extra spread from firing goes away, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0"))
	float BloomRecovery = 6.f;

	/** How far each shot kicks the view up. Halved while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0", Units = "deg"))
	float RecoilPitch = 0.45f;

	/** Random sideways kick per shot, up to this much either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0", Units = "deg"))
	float RecoilYaw = 0.2f;

	/** Steady sideways drift per shot. Positive pulls right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (Units = "deg"))
	float RecoilYawDrift = 0.06f;

	/** How much harder each shot kicks the longer you hold the trigger. 0.5 means the kick grows up to 1.5 times. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0"))
	float RecoilClimb = 0.6f;

	/** Shots it takes for the kick to reach its strongest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "1"))
	int32 RecoilClimbShots = 10;

	/** How much of the upward kick the view settles back down after you stop firing. 0 none, 1 all of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0", ClampMax = "1"))
	float RecoilRecovery = 0.6f;

	/** How fast the view settles back, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0", Units = "deg"))
	float RecoilRecoverySpeed = 14.f;

	/** Called each time the gun fires a round. */
	FOnJavelinWeaponFired OnFired;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effects")
	FLinearColor TracerColor = FLinearColor(1.f, 0.75f, 0.3f);

	/** The point shots and tracers come out of. Set by the character. */
	void SetMuzzle(USceneComponent* InMuzzle) { Muzzle = InMuzzle; }

	void SetTriggerHeld(bool bHeld);
	void SetWantsToAim(bool bWants) { bWantsToAim = bWants; }

	/** Starts reloading if the magazine isn't full and there is reserve ammo. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool StartReload();

	/** Fills the magazine and reserve, as at an ammo crate. Returns false if they were already full. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool RefillAmmo();

	/** Adds rounds to the reserve, as from an ammo orb. Returns false if the reserve was already full. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool AddReserveAmmo(int32 Rounds);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const;

	/** True when the center of the crosshair is on a living enemy or target, so the HUD can turn it red. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsCrosshairOnTarget() const;

	/** The point in the world under the center of the crosshair. Gear aims here too. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetAimPoint() const;

	/** Shows a hit marker and damage number, for damage done by something else the player fired, like a grenade. */
	void AddHitFeedback(const FVector& Location, float Amount, bool bWeakSpot, bool bKill);

	/** Whether an actor is an enemy or shooting target that's still alive. */
	static bool IsLivingTarget(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bReloading; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetAmmo() const { return Ammo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	/** 0 when a reload starts, 1 when it finishes. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetReloadProgress() const;

	/** Current width of the cone shots land in, in degrees. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetCurrentSpread() const;

	float GetTimeSinceLastShot() const;

	/** 1 the moment a round fires, fading to 0 quickly. Drives the arms kicking back. */
	float GetRecoilKick() const { return RecoilKick; }
	float GetTimeSinceLastHit() const;
	bool WasLastHitKill() const { return bLastHitKill; }
	bool WasLastHitWeakSpot() const { return bLastHitWeakSpot; }
	const TArray<FJavelinDamageNumber>& GetDamageNumbers() const { return DamageNumbers; }

	/** How long damage numbers stay on screen. */
	static constexpr float DamageNumberLifetime = 1.f;

protected:
	virtual void BeginPlay() override;

private:
	bool CanFire() const;
	void FireShot();

	/** Traces from the camera along Direction, starting level with the suit. Returns the point the crosshair is on. */
	FVector TraceFromView(const FVector& ViewLocation, const FVector& Direction, FHitResult& OutHit, bool& bOutHit) const;
	void FinishReload();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Muzzle;

	TArray<FJavelinDamageNumber> DamageNumbers;

	int32 Ammo = 0;
	int32 ReserveAmmo = 0;
	float ShotCooldown = 0.f;
	float ReloadRemaining = 0.f;
	float Bloom = 0.f;
	float LastShotTime = -100.f;
	float LastHitTime = -100.f;
	bool bTriggerHeld = false;
	bool bWantsToAim = false;
	bool bReloading = false;
	bool bLastHitKill = false;
	bool bLastHitWeakSpot = false;

	/** Rounds fired since the trigger was last let go for a moment. */
	int32 SustainedShots = 0;

	/** Upward kick still waiting to settle back down. */
	float RecoilToRecover = 0.f;
	float RecoilKick = 0.f;
};
