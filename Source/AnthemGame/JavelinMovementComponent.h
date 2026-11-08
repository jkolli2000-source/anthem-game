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

/**
 * Character movement with javelin flight and hover.
 *
 * Ground, sprint, jump and falling use the stock modes. Flight and hover are
 * custom modes: flight thrusts along the camera aim with gravity off, hover
 * holds altitude and lets the pilot strafe and aim.
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

	/** How quickly speed moves toward its target. Higher is snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0"))
	float FlightSpeedInterp = 1.5f;

	/** How fast the flight path turns toward the camera aim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Flight", meta = (ClampMin = "0", Units = "deg"))
	float FlightTurnRate = 130.f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverFriction = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverBrakingDeceleration = 2500.f;

	/** How quickly vertical speed bleeds off when you start hovering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverVerticalDamping = 5.f;

	/** How quickly the suit turns to face the camera while hovering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Javelin|Hover", meta = (ClampMin = "0"))
	float HoverTurnSpeed = 12.f;

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

	virtual float GetMaxSpeed() const override;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void PhysicsRotation(float DeltaTime) override;

private:
	void PhysFlight(float DeltaTime, int32 Iterations);
	void PhysHover(float DeltaTime, int32 Iterations);
	FVector GetAimDirection() const;

	bool bWantsToSprint = false;
	FVector2D FlightInput = FVector2D::ZeroVector;
};
