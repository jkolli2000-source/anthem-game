#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JavelinSuitHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FJavelinSuitEvent);

/** Where a hit came from, so the HUD can point at it. */
USTRUCT()
struct FJavelinDamageIndicator
{
	GENERATED_BODY()

	FVector SourceLocation = FVector::ZeroVector;
	float Time = 0.f;
};

/**
 * The suit's shields and armor, like in Anthem.
 *
 * Damage hits the shields first. Shields come back on their own a few
 * seconds after you stop taking damage; armor doesn't, so pick up the green
 * health orbs enemies drop. When armor runs out the suit goes down.
 */
UCLASS(ClassGroup = (Javelin), meta = (BlueprintSpawnableComponent))
class ANTHEMGAME_API UJavelinSuitHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJavelinSuitHealthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suit Health", meta = (ClampMin = "0"))
	float MaxShields = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suit Health", meta = (ClampMin = "1"))
	float MaxArmor = 300.f;

	/** Seconds without taking damage before shields start coming back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suit Health", meta = (ClampMin = "0", Units = "s"))
	float ShieldRegenDelay = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suit Health", meta = (ClampMin = "0"))
	float ShieldRegenPerSecond = 60.f;

	/** Takes damage from a hit coming from SourceLocation. Returns how much was taken. */
	float ApplyDamage(float Amount, const FVector& SourceLocation);

	/** Repairs armor. Returns false if it was already full. */
	UFUNCTION(BlueprintCallable, Category = "Suit Health")
	bool RepairArmor(float Amount);

	/** Brings a downed suit back with full shields and armor. */
	UFUNCTION(BlueprintCallable, Category = "Suit Health")
	void Restore();

	UFUNCTION(BlueprintPure, Category = "Suit Health")
	float GetShields() const { return Shields; }

	UFUNCTION(BlueprintPure, Category = "Suit Health")
	float GetArmor() const { return Armor; }

	UFUNCTION(BlueprintPure, Category = "Suit Health")
	float GetShieldPercent() const { return MaxShields > 0.f ? Shields / MaxShields : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Suit Health")
	float GetArmorPercent() const { return Armor / MaxArmor; }

	UFUNCTION(BlueprintPure, Category = "Suit Health")
	bool IsDown() const { return bDown; }

	UFUNCTION(BlueprintPure, Category = "Suit Health")
	bool IsRegeneratingShields() const;

	float GetTimeSinceDamage() const;
	float GetTimeSinceShieldsBroke() const;
	float GetTimeSinceArmorRepaired() const;
	const TArray<FJavelinDamageIndicator>& GetDamageIndicators() const { return DamageIndicators; }

	/** How long a damage direction arrow stays on screen. */
	static constexpr float DamageIndicatorLifetime = 1.5f;

	UPROPERTY(BlueprintAssignable, Category = "Suit Health")
	FJavelinSuitEvent OnSuitDown;

	UPROPERTY(BlueprintAssignable, Category = "Suit Health")
	FJavelinSuitEvent OnShieldsBroken;

protected:
	virtual void BeginPlay() override;

private:
	float Now() const;

	TArray<FJavelinDamageIndicator> DamageIndicators;

	float Shields = 0.f;
	float Armor = 0.f;
	float LastDamageTime = -100.f;
	float ShieldsBrokeTime = -100.f;
	float ArmorRepairedTime = -100.f;
	bool bDown = false;
};
