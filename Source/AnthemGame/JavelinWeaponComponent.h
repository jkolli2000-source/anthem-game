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

/**
 * The javelin's gun: an automatic rifle with a magazine, reserve ammo,
 * reloading, aim-down-sights, spread that blooms while you fire, and recoil.
 *
 * Shots are instant line traces from the camera, so they go where the
 * crosshair points, then checked from the muzzle so you can't shoot through
 * a wall next to you. Hitting a part tagged "WeakSpot" does extra damage.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage", meta = (ClampMin = "100", Units = "cm"))
	float Range = 15000.f;

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

	/** How fast the extra spread from firing goes away, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0"))
	float BloomRecovery = 6.f;

	/** How far each shot kicks the view up. Halved while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0", Units = "deg"))
	float RecoilPitch = 0.45f;

	/** Random sideways kick per shot, up to this much either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil", meta = (ClampMin = "0", Units = "deg"))
	float RecoilYaw = 0.2f;

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
};
