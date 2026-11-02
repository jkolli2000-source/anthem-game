#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "JavelinMovementComponent.generated.h"

/** Sub-modes of MOVE_Custom used by the javelin. */
UENUM(BlueprintType)
enum class EJavelinMoveMode : uint8
{
	None	UMETA(Hidden),
	Flight,
	Hover
};

/** Called when the suit touches down, with its velocity just before impact. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnJavelinLanded, const FVector& /*ImpactVelocity*/, bool /*bFromFlight*/);

/** Called when flight slams into a wall, with the velocity before the hit and where it hit. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnJavelinCrashed, const FVector& /*ImpactVelocity*/, const FVector& /*ImpactPoint*/);

/**
 * Character movement with javelin flight and hover.
 *
 * Ground, sprint, jump and falling use the stock modes. Flight and hover are
 * custom modes: flight thrusts along the camera aim, hover holds altitude and
 * lets the pilot strafe and aim.
 *
 * Flight has momentum: the suit has a heading that swings toward the aim
 * (slower at high speed, so fast turns are wide), and any speed that isn't
 * along the heading bleeds off over a moment, so hard turns drift outward.
 * Thrusters push speed toward a target, air drag pulls it back more slowly,
 * and gravity speeds up dives and slows climbs, so a dive carries its speed
 * into the pull-up.
 *
 * The suit is heavy on the ground: the first push-off is strong so it still
 * answers the sticks right away, but it builds the rest of its speed over a
 * moment, turns slower the faster it goes, has to plant and brake to reverse
 * at speed, skids when it lands fast, and a hard landing slows it down for a
 * moment.
 *
 * Dodge is a short jet-boosted dash on the ground or while hovering. It goes
 * the way you push the stick (or back, with no input), keeps the suit facing
 * the same way, and bleeds off into a skid at the end.
 */
UCLASS()
class ANTHEMGAME_API UJavelinMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UJavelinMovementComponent();

	/** Ground speed while sprint is held. Walk speed is MaxWalkSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 1000.f;

	/** Acceleration from a standstill. High so the first step answers the input right away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0"))
	float GroundStartAcceleration = 2800.f;

	/** Acceleration near top speed. Lower makes the suit take longer to wind up to a full sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0"))
	float GroundTopAcceleration = 1200.f;

	/** How quickly the suit's path bends toward a new direction when walking slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0"))
	float GroundTurnGripSlow = 10.f;

	/** How quickly the path bends at full sprint. Lower makes fast turns wider and heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0"))
	float GroundTurnGripFast = 4.f;

	/** Extra braking when you push against the way you're moving, so reversing at speed plants and stops first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0"))
	float GroundReverseBraking = 2600.f;

	/** How fast the suit turns to face where it's going while standing or walking slowly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0", Units = "deg"))
	float GroundTurnRateSlow = 480.f;

	/** How fast it turns at full sprint. Lower feels heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0", Units = "deg"))
	float GroundTurnRateFast = 240.f;

	/** How fast the suit turns to face the crosshair while aiming or shooting. Kept quick so aiming stays responsive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0", Units = "deg"))
	float CombatTurnRate = 620.f;

	/** Speed at the start of a dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Dodge", meta = (ClampMin = "0", Units = "cm/s"))
	float DodgeSpeed = 2300.f;

	/** How long the dodge's jet burst lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float DodgeDuration = 0.32f;

	/** Fraction of the dodge speed left when the burst ends; the suit then skids to a stop or walks on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Dodge", meta = (ClampMin = "0", ClampMax = "1"))
	float DodgeEndSpeedFraction = 0.3f;

	/** Seconds from the start of one dodge until the next is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Dodge", meta = (ClampMin = "0", Units = "s"))
	float DodgeCooldown = 0.75f;

	/** Allow dodging while hovering, as an air dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Dodge")
	bool bCanDodgeWhileHovering = true;

	/** Speed when flying with no forward or back input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float FlightCruiseSpeed = 3000.f;

	/** Speed with full forward input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float FlightBoostSpeed = 4500.f;

	/** Speed with full back input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float FlightSlowSpeed = 1600.f;

	/** How hard the thrusters push speed up toward the target speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0"))
	float FlightThrustAcceleration = 2200.f;

	/** How quickly air drag slows you when you're faster than the target speed, say after a dive. Lower carries momentum longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0"))
	float FlightDragDeceleration = 900.f;

	/** How much gravity speeds up dives and slows climbs, along the flight path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0"))
	float FlightGravity = 900.f;

	/** The suit never flies slower than this, even climbing straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float FlightMinSpeed = 900.f;

	/** How fast the heading turns toward the camera aim at low speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "deg"))
	float FlightTurnRate = 170.f;

	/** How fast the heading turns at full boost. Lower makes fast turns wider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "deg"))
	float FlightTurnRateAtSpeed = 85.f;

	/** How quickly sideways drift bleeds off after a turn. Higher grips the air harder; lower slides more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0"))
	float FlightGrip = 2.5f;

	/** How much left and right input bends the flight path away from the aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", ClampMax = "1"))
	float FlightStrafeInfluence = 0.35f;

	/** Upward speed when taking off from the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float FlightTakeoffSpeed = 1800.f;

	/** Minimum speed when entering flight from the air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float FlightEntryMinSpeed = 1500.f;

	/** Extra target speed when pointing straight down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float DiveSpeedBonus = 2000.f;

	/** Target speed lost when pointing straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float ClimbSpeedPenalty = 1000.f;

	/** A dive is any flight path at least this steep below the horizon... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float DiveAngle = 30.f;

	/** ...at or above this speed. Dives cool the suit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float DiveMinSpeed = 2000.f;

	/** Hitting a wall at or above this speed knocks you out of flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s"))
	float CrashMinSpeed = 2000.f;

	/** How head-on a hit must be to count as a crash (1 = straight into the wall). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", ClampMax = "1"))
	float CrashImpactDot = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0", Units = "cm/s"))
	float HoverSpeed = 600.f;

	/** Lower lets the suit drift more before the hover jets catch it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverFriction = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverBrakingDeceleration = 1800.f;

	/** How quickly vertical speed bleeds off when you start hovering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverVerticalDamping = 5.f;

	/** How quickly the suit turns to face the camera while hovering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverTurnSpeed = 12.f;

	/** Landing faster than this (falling speed) is a hard landing: the suit slows down for a moment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Landing", meta = (ClampMin = "0", Units = "cm/s"))
	float HardLandingSpeed = 1500.f;

	/** How long a hard landing slows you down. Landing out of flight at speed takes a little longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Landing", meta = (ClampMin = "0", Units = "s"))
	float HardLandingRecoveryTime = 0.3f;

	/** Ground speed multiplier while recovering from a hard landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Landing", meta = (ClampMin = "0", ClampMax = "1"))
	float HardLandingSpeedScale = 0.35f;

	/** Ground friction while skidding after landing fast. Lower slides further. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Landing", meta = (ClampMin = "0", ClampMax = "1"))
	float LandingSkidFriction = 0.12f;

	/** Longest skid after landing at full speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Landing", meta = (ClampMin = "0", Units = "s"))
	float MaxLandingSkidTime = 0.6f;

	/** Called when the suit touches down. */
	FOnJavelinLanded OnLanded;

	/** Called when flight slams into a wall. */
	FOnJavelinCrashed OnCrashed;

	/** True for a moment after a hard landing, while the suit gets its footing back. */
	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsRecoveringFromLanding() const { return LandingRecoveryRemaining > 0.f; }

	/** Takes off from the ground, or starts flying from the air. */
	UFUNCTION(BlueprintCallable, Category = "Javelin")
	bool StartFlight();

	/** Starts hovering. Only works in the air. */
	UFUNCTION(BlueprintCallable, Category = "Javelin")
	bool StartHover();

	/** Leaves flight or hover and falls. */
	UFUNCTION(BlueprintCallable, Category = "Javelin")
	void StopFlightOrHover();

	UFUNCTION(BlueprintCallable, Category = "Javelin")
	void SetSprinting(bool bSprint) { bWantsToSprint = bSprint; }

	/** True if a dodge can start now: on the ground (or hovering), not mid-dodge and off cooldown. */
	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool CanDodge() const;

	/** Dashes along Direction (flattened). Returns false if a dodge can't start now. */
	UFUNCTION(BlueprintCallable, Category = "Javelin")
	bool StartDodge(const FVector& Direction);

	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsDodging() const { return DodgeRemaining > 0.f; }

	/** How far through the dodge burst, 0 at the start to 1 at the end. */
	UFUNCTION(BlueprintPure, Category = "Javelin")
	float GetDodgeProgress() const { return DodgeDuration > 0.f ? 1.f - DodgeRemaining / DodgeDuration : 1.f; }

	/** The way the current (or last) dodge went, flat and normalized. */
	FVector GetDodgeDirection() const { return DodgeDirection; }

	/** True while sprint is wanted, even in the air. */
	bool WantsToSprint() const { return bWantsToSprint; }

	/** Move input while flying: X is left/right, Y is forward/back. */
	void SetFlightInput(const FVector2D& Input) { FlightInput = Input; }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	EJavelinMoveMode GetJavelinMode() const;

	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsInFlight() const { return GetJavelinMode() == EJavelinMoveMode::Flight; }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsHovering() const { return GetJavelinMode() == EJavelinMoveMode::Hover; }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsSprinting() const { return bWantsToSprint && IsMovingOnGround(); }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsDiving() const;

	/** The way the suit is pointed in flight. Velocity can drift off it in hard turns. */
	FVector GetFlightHeading() const { return FlightHeading; }

	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void PhysicsRotation(float DeltaTime) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void PhysFlight(float DeltaTime, int32 Iterations);
	void PhysHover(float DeltaTime, int32 Iterations);
	FVector GetAimDirection() const;

	bool bWantsToSprint = false;
	FVector2D FlightInput = FVector2D::ZeroVector;
	FVector FlightHeading = FVector::ForwardVector;
	float LandingRecoveryRemaining = 0.f;
	float LandingSkidRemaining = 0.f;
	float DodgeRemaining = 0.f;
	float DodgeCooldownRemaining = 0.f;
	FVector DodgeDirection = FVector::ForwardVector;
};
