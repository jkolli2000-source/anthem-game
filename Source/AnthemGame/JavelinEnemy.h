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
 * Hits have weight: every shot makes the body flinch away from it and
 * shoves it back a little, explosions throw it, and enough damage in a
 * short time (or a shot to the weak spot while it's flaring) staggers it,
 * cancelling its volley. When it dies a soldier topples over and a drone
 * loses power and falls, trailing smoke, until it hits the ground.
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

	/** Heavier enemies get pushed less by shots and blasts. 1 is normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hit Reactions", meta = (ClampMin = "0.1"))
	float Mass = 1.f;

	/** Taking this fraction of its max health within a couple of seconds staggers it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hit Reactions", meta = (ClampMin = "0", ClampMax = "1"))
	float StaggerDamageFraction = 0.3f;

	/** How long a stagger stops it shooting and slows it down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hit Reactions", meta = (ClampMin = "0", Units = "s"))
	float StaggerTime = 0.9f;

	/** How far a hit tips the body, in degrees per second of kick per point of damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hit Reactions", meta = (ClampMin = "0"))
	float FlinchPerDamage = 9.f;

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

	/** True while it's reeling from heavy damage: it can't shoot and barely moves. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsStaggered() const { return StaggerRemaining > 0.f; }

	/** Shoves it, in cm/s. Shots push a little, explosions a lot. A falling wreck gets knocked along too. */
	void ApplyKnockback(const FVector& Impulse);

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

	/** Plays the death: by default it topples over away from the killing blow, lies there, then sinks away. */
	virtual void UpdateDeath(float DeltaSeconds);

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

	/** Push from hits and blasts, fading out. Added on top of its own movement. */
	FVector KnockbackVelocity = FVector::ZeroVector;

	/** Flying enemies can be knocked up and down; walkers only get pushed along the ground. */
	bool bCanBeKnockedUp = false;

	/** True once killed, while the death plays out. */
	bool bDying = false;
	float DeathTime = 0.f;

	/** Which way the killing blow was going. */
	FVector DeathDirection = FVector::ForwardVector;

	/** How fast a falling wreck is moving. */
	FVector DeathVelocity = FVector::ZeroVector;

	static const FName PaintBody;
	static const FName PaintArmor;
	static const FName PaintWeakSpot;

private:
	void UpdateSenses(float DeltaSeconds, AJavelinCharacter* Player);
	void UpdateAttack(float DeltaSeconds, const AJavelinCharacter* Target);
	void FireProjectile(const AJavelinCharacter* Target);
	bool CanSee(const AJavelinCharacter* Player) const;
	void UpdatePaint(float DeltaSeconds);
	void UpdateHitReactions(float DeltaSeconds);
	void Stagger();
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

	/** Body tipping from hits, in degrees, and how fast. A spring pulls it back upright. */
	float FlinchPitch = 0.f;
	float FlinchRoll = 0.f;
	float FlinchPitchSpeed = 0.f;
	float FlinchRollSpeed = 0.f;

	/** The body's rotation from its own animation, before flinching is added on top. */
	FRotator BodyRestRotation = FRotator::ZeroRotator;

	/** Recent damage, draining over time. Too much at once staggers it. */
	float RecentDamage = 0.f;
	float StaggerRemaining = 0.f;
	bool bSankDust = false;
};
