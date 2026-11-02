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
 * The suit is heavy on the ground: it builds up speed and stops over a short
 * distance, turns with some weight, skids when it lands fast, and a hard
 * landing slows it down for a moment.
 */
UCLASS()
class ANTHEMGAME_API UJavelinMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UJavelinMovementComponent();

	/** Ground speed while sprint is held. Walk speed is MaxWalkSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Ground", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 1100.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s2"))
	float FlightThrustAcceleration = 2200.f;

	/** How quickly air drag slows you when you're faster than the target speed, say after a dive. Lower carries momentum longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s2"))
	float FlightDragDeceleration = 900.f;

	/** How much gravity speeds up dives and slows climbs, along the flight path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "cm/s2"))
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
};
