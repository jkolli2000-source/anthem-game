#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "JavelinEnemy.generated.h"

class AJavelinCharacter;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/** One body part's dynamic material and normal color, so it can flash and glow. */
struct FJavelinEnemyPaintedPart
{
	UMaterialInstanceDynamic* Material = nullptr;
	FLinearColor Color = FLinearColor::White;
	bool bWeakSpot = false;
};

/**
 * Base for enemies that fight back. Each one has health and a glowing weak
 * spot, watches for the player, and once it spots you (or you shoot it) it
 * chases you and fires glowing bolts you can see coming and dodge.
 *
 * Before each volley the weak spot flares bright, so you get a moment of
 * warning. When it loses track of you for a while, or you lead it too far
 * from where it started, it gives up and goes home.
 *
 * Enemies don't use the navigation system, so they need no nav mesh in the
 * level: soldiers walk straight at you and slide along walls, drones fly.
 * Subclasses build the body and decide how to move.
 */
UCLASS(Abstract)
class ANTHEMGAME_API AJavelinEnemy : public APawn
{
	GENERATED_BODY()

public:
	AJavelinEnemy();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "1"))
	float MaxHealth = 250.f;

	/** How far away it can spot you, if nothing is in the way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Senses", meta = (ClampMin = "0", Units = "cm"))
	float SightRange = 6000.f;

	/** It gives up the chase after not seeing you for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Senses", meta = (ClampMin = "0", Units = "s"))
	float ForgetTime = 8.f;

	/** It won't chase you further than this from where it spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Senses", meta = (ClampMin = "0", Units = "cm"))
	float LeashRange = 12000.f;

	/** When shot, it warns other enemies within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Senses", meta = (ClampMin = "0", Units = "cm"))
	float AlertAlliesRange = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0"))
	float MoveSpeed = 400.f;

	/** It tries to stay between these distances from you while fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0", Units = "cm"))
	float MinCombatRange = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0", Units = "cm"))
	float MaxCombatRange = 3000.f;

	/** It only shoots when you're this close. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0", Units = "cm"))
	float AttackRange = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0"))
	float ProjectileDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "100"))
	float ProjectileSpeed = 6000.f;

	/** 1 is a normal bolt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.1"))
	float ProjectileSize = 1.f;

	/** Shots fired in each volley. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0", Units = "s"))
	float TimeBetweenBurstShots = 0.15f;

	/** Rest between volleys. A little randomness is added so enemies don't fire in sync. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0", Units = "s"))
	float TimeBetweenBursts = 2.2f;

	/** The weak spot flares for this long before each volley. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0", Units = "s"))
	float WindUpTime = 0.4f;

	/** Width of the cone its shots land in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0", Units = "deg"))
	float AimSpread = 2.f;

	/** How much it aims ahead of a moving target. 0 shoots where you are, 1 where you'll be. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0", ClampMax = "1"))
	float AimLead = 0.5f;

	/** Chance of dropping a health orb, and of dropping an ammo orb, when killed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Drops", meta = (ClampMin = "0", ClampMax = "1"))
	float HealthDropChance = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Drops", meta = (ClampMin = "0", ClampMax = "1"))
	float AmmoDropChance = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Paint")
	FLinearColor BodyColor = FLinearColor(0.3f, 0.07f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Paint")
	FLinearColor ArmorColor = FLinearColor(0.12f, 0.11f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Paint")
	FLinearColor WeakSpotColor = FLinearColor(1.f, 0.35f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Paint")
	FLinearColor ProjectileColor = FLinearColor(1.f, 0.25f, 0.1f);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsAlive() const { return Health > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetHealthPercent() const { return Health / MaxHealth; }

	/** True while it knows where you are and is fighting you. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsEngaged() const { return bEngaged; }

	/** Makes it start fighting the player right away, as if it had spotted them. */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void Alert();

	/** Shown over the health bar on the HUD. */
	virtual FText GetEnemyName() const;

	/** Where the HUD draws its health bar. */
	FVector GetHealthBarLocation() const;

	float GetTimeSinceHit() const;

protected:
	virtual void BeginPlay() override;

	/** Moves toward where it wants to be. Target is null when it isn't fighting. */
	virtual void UpdateMovement(float DeltaSeconds, const AJavelinCharacter* Target) {}

	/** Legs, spinning parts and so on. */
	virtual void UpdateAnimation(float DeltaSeconds) {}

	/** Turns to face Target, or its direction of travel when Target is null. */
	virtual void UpdateFacing(float DeltaSeconds, const AJavelinCharacter* Target);

	/** Adds a body part made from a basic shape. Size is in centimeters. Parts tagged as weak spots take extra damage. */
	UStaticMeshComponent* AddPart(FName Name, USceneComponent* Parent, UStaticMesh* PartMesh, const FVector& Location, const FVector& Size,
		FName Paint, const FRotator& Rotation = FRotator::ZeroRotator);

	USceneComponent* AddPivot(FName Name, USceneComponent* Parent, const FVector& Location);

	/** Moves by Delta, sliding along anything in the way. Returns true if it bumped into something. */
	bool MoveWithSliding(const FVector& Delta);

	/** Point on the ground under Location, if there is ground within MaxDrop. */
	bool FindGround(const FVector& Location, float MaxDrop, FVector& OutGround) const;

	/** Moving left or right around the player while fighting, flipped now and then. */
	float GetStrafeSign() const { return StrafeSign; }

	/** Whether it had a clear line of sight to the player at the last check. */
	bool CanSeePlayer() const { return bCanSeePlayer; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> Collision;

	/** Everything visible hangs off this, so subclasses can tilt the body without turning the collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Body;

	/** Bolts are fired from here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	/** Height of the health bar above the actor's location. */
	float HealthBarHeight = 150.f;

	FVector HomeLocation = FVector::ZeroVector;
	FVector MoveVelocity = FVector::ZeroVector;

	static const FName PaintBody;
	static const FName PaintArmor;
	static const FName PaintWeakSpot;

private:
	void UpdateSenses(float DeltaSeconds, AJavelinCharacter* Player);
	void UpdateAttack(float DeltaSeconds, const AJavelinCharacter* Target);
	void FireProjectile(const AJavelinCharacter* Target);
	bool CanSee(const AJavelinCharacter* Player) const;
	void UpdatePaint(float DeltaSeconds);
	void Die();

	TArray<FJavelinEnemyPaintedPart> PaintedParts;

	/** Keeps the dynamic materials alive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PartMaterials;

	float Health = 0.f;
	float HitFlash = 0.f;
	float LastHitTime = -100.f;
	float LastSawPlayerTime = -100.f;
	float SightCheckTimer = 0.f;
	float AttackTimer = 0.f;
	float StrafeTimer = 0.f;
	float StrafeSign = 1.f;
	int32 ShotsLeftInBurst = 0;
	bool bEngaged = false;
	bool bWindingUp = false;
	bool bCanSeePlayer = false;
};
