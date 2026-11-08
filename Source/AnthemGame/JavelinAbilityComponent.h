#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JavelinAbilityComponent.generated.h"

class APawn;
class UWorld;

/** The javelin's two gear slots, like Anthem's left and right gear. */
UENUM(BlueprintType)
enum class EJavelinGear : uint8
{
	Grenade,
	Missiles
};

/** One piece of gear: its name and color on the HUD, and its cooldown. */
USTRUCT(BlueprintType)
struct FJavelinGearSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	FText Name;

	/** Shown under the HUD icon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	FText ShortName;

	/** Seconds before it can be used again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear", meta = (ClampMin = "0", Units = "s"))
	float Cooldown = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	FLinearColor Color = FLinearColor::White;

	float CooldownRemaining = 0.f;
	float LastUsedTime = -100.f;
	float LastReadyTime = -100.f;
	float LastDeniedTime = -100.f;
};

/**
 * The javelin's gear abilities, Anthem style: a frag grenade and a homing
 * missile launcher, each on its own cooldown.
 *
 *  - Frag grenade: thrown over the left shoulder toward the crosshair. It blows up
 *    on impact and damages every enemy in the blast.
 *  - Homing missiles: a volley fired from the shoulder pod. They lock onto
 *    enemies near the crosshair (the HUD brackets them while the launcher is
 *    ready) and curve in to hit them. With nothing locked they fly at the crosshair.
 *
 * Like the gun, gear can be used on the ground, in the air and while
 * hovering, but not while flying or while the suit is down.
 *
 * This is a plain component for now; the plan moves gear onto the Gameplay
 * Ability System in milestone 2. The owner must be an AJavelinCharacter.
 */
UCLASS(ClassGroup = (Javelin), meta = (BlueprintSpawnableComponent))
class ANTHEMGAME_API UJavelinAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJavelinAbilityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Grenade")
	FJavelinGearSlot GrenadeSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Grenade", meta = (ClampMin = "0"))
	float GrenadeDamage = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Grenade", meta = (ClampMin = "10", Units = "cm"))
	float GrenadeBlastRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Grenade", meta = (ClampMin = "100"))
	float GrenadeThrowSpeed = 2600.f;

	/** It aims to land on the crosshair up to this far away, then carries on past it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Grenade", meta = (ClampMin = "100", Units = "cm"))
	float GrenadeMaxAimDistance = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles")
	FJavelinGearSlot MissileSlot;

	/** Missiles in each volley. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles", meta = (ClampMin = "1"))
	int32 MissileCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles", meta = (ClampMin = "0", Units = "s"))
	float TimeBetweenMissiles = 0.1f;

	/** Damage of each missile's blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles", meta = (ClampMin = "0"))
	float MissileDamage = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles", meta = (ClampMin = "10", Units = "cm"))
	float MissileBlastRadius = 200.f;

	/** Missiles lock onto enemies this close... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles", meta = (ClampMin = "0", Units = "cm"))
	float MissileLockRange = 8000.f;

	/** ...and within this angle of the crosshair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear|Missiles", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float MissileLockAngle = 22.f;

	/** Where grenades and missiles come from. Set by the character. */
	void SetLaunchPoints(USceneComponent* InGrenadePoint, USceneComponent* InMissilePod);

	/** Uses the gear if it's off cooldown and the suit can use it. Returns false (and the HUD flashes) if it can't. */
	UFUNCTION(BlueprintCallable, Category = "Gear")
	bool UseGear(EJavelinGear Gear);

	UFUNCTION(BlueprintPure, Category = "Gear")
	bool IsGearReady(EJavelinGear Gear) const;

	UFUNCTION(BlueprintPure, Category = "Gear")
	float GetCooldownRemaining(EJavelinGear Gear) const;

	/** 0 just after use, 1 when ready again. */
	UFUNCTION(BlueprintPure, Category = "Gear")
	float GetCooldownPercent(EJavelinGear Gear) const;

	/** Makes all gear ready right away, as when the suit reboots. */
	UFUNCTION(BlueprintCallable, Category = "Gear")
	void ResetCooldowns();

	const FJavelinGearSlot& GetSlot(EJavelinGear Gear) const;
	float GetTimeSinceUsed(EJavelinGear Gear) const;
	float GetTimeSinceReady(EJavelinGear Gear) const;
	float GetTimeSinceDenied(EJavelinGear Gear) const;

	/** Enemies the missiles would go after if fired now. Empty unless the launcher is ready. */
	const TArray<TWeakObjectPtr<AActor>>& GetMissileLocks() const { return MissileLocks; }

	/**
	 * Damages every living enemy and target within Radius of Center that the blast can reach, full damage near the
	 * middle and less toward the edge, and shows hit markers and damage numbers for the player. Returns the total dealt.
	 */
	static float ApplyBlastDamage(UWorld* World, const FVector& Center, float Damage, float Radius, AActor* DamageCauser, APawn* InstigatorPawn);

	/** The point to aim at on an enemy or target. */
	static FVector GetTargetCenter(const AActor* Target);

protected:
	virtual void BeginPlay() override;

private:
	FJavelinGearSlot& GetSlotMutable(EJavelinGear Gear);
	bool CanUseGear() const;
	bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
	FVector GetLaunchLocation(const USceneComponent* Point) const;

	void ThrowGrenade();
	void StartMissileVolley();
	void FireMissile();
	void UpdateMissileLocks();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GrenadePoint;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> MissilePod;

	TArray<TWeakObjectPtr<AActor>> MissileLocks;
	TArray<TWeakObjectPtr<AActor>> VolleyTargets;
	FVector VolleyAimPoint = FVector::ZeroVector;
	int32 MissilesLeft = 0;
	int32 MissilesFired = 0;
	float MissileTimer = 0.f;
	float LockUpdateTimer = 0.f;
};
