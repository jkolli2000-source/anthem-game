#include "JavelinMovementComponent.h"
#include "GameFramework/Character.h"

UJavelinMovementComponent::UJavelinMovementComponent()
{
	// A heavy suit: it takes a moment to get up to speed and to stop, and turns with some weight.
	MaxWalkSpeed = 600.f;
	MaxAcceleration = 1600.f;
	BrakingDecelerationWalking = 1500.f;
	GroundFriction = 6.f;
	// Heavier gravity with a stronger jump: about the same height as before, but it falls like it weighs a ton.
	JumpZVelocity = 820.f;
	AirControl = 0.6f;
	GravityScale = 1.8f;
	bOrientRotationToMovement = true;
	RotationRate = FRotator(0.f, 450.f, 0.f);
}

void UJavelinMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	LandingRecoveryRemaining = FMath::Max(0.f, LandingRecoveryRemaining - DeltaTime);
	LandingSkidRemaining = FMath::Max(0.f, LandingSkidRemaining - DeltaTime);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UJavelinMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Landing fast, the feet skid along the ground before they grip.
	if (LandingSkidRemaining > 0.f && IsMovingOnGround())
	{
		Friction *= LandingSkidFriction;
		BrakingDeceleration *= 0.3f;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UJavelinMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// Walking zeroes the vertical speed, so grab the impact velocity first.
	const FVector ImpactVelocity = Velocity;
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const bool bFromFlight = PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(EJavelinMoveMode::Flight);
	if (!IsMovingOnGround() || (PreviousMovementMode != MOVE_Falling && !bFromFlight))
	{
		return;
	}

	const float FallSpeed = FMath::Max(0.f, static_cast<float>(-ImpactVelocity.Z));
	if (FallSpeed >= HardLandingSpeed)
	{
		LandingRecoveryRemaining = HardLandingRecoveryTime * (bFromFlight ? 1.5f : 1.f);
	}

	const float GroundSpeed = static_cast<float>(ImpactVelocity.Size2D());
	if (GroundSpeed > MaxWalkSpeed * 1.5f)
	{
		LandingSkidRemaining = MaxLandingSkidTime * FMath::Clamp(GroundSpeed / FMath::Max(FlightBoostSpeed, 1.f), 0.3f, 1.f);
	}

	OnLanded.Broadcast(ImpactVelocity, bFromFlight);
}

EJavelinMoveMode UJavelinMovementComponent::GetJavelinMode() const
{
	return MovementMode == MOVE_Custom ? static_cast<EJavelinMoveMode>(CustomMovementMode) : EJavelinMoveMode::None;
}

bool UJavelinMovementComponent::IsDiving() const
{
	if (!IsInFlight() || Velocity.Size() < DiveMinSpeed)
	{
		return false;
	}
	return Velocity.GetSafeNormal().Z <= -FMath::Sin(FMath::DegreesToRadians(DiveAngle));
}

bool UJavelinMovementComponent::StartFlight()
{
	if (!CharacterOwner || !UpdatedComponent || IsInFlight())
	{
		return false;
	}

	if (IsMovingOnGround())
	{
		// Launch straight up, keeping a little of the run-up; PhysFlight then curves the path toward the aim.
		Velocity = FVector::UpVector * FlightTakeoffSpeed + FVector(Velocity.X, Velocity.Y, 0.f) * 0.5f;
		FlightHeading = FVector::UpVector;
	}
	else
	{
		// Keep the momentum you had (a fall, a jump), and fire the thrusters along the aim on top of it.
		FlightHeading = GetAimDirection();
		const float AlongAim = static_cast<float>(FVector::DotProduct(Velocity, FlightHeading));
		if (AlongAim < FlightEntryMinSpeed)
		{
			Velocity += FlightHeading * (FlightEntryMinSpeed - AlongAim);
		}
	}

	SetMovementMode(MOVE_Custom, static_cast<uint8>(EJavelinMoveMode::Flight));
	return true;
}

bool UJavelinMovementComponent::StartHover()
{
	if (!CharacterOwner || !UpdatedComponent || IsMovingOnGround() || IsHovering())
	{
		return false;
	}

	SetMovementMode(MOVE_Custom, static_cast<uint8>(EJavelinMoveMode::Hover));
	return true;
}

void UJavelinMovementComponent::StopFlightOrHover()
{
	if (MovementMode == MOVE_Custom)
	{
		SetMovementMode(MOVE_Falling);
	}
}

float UJavelinMovementComponent::GetMaxSpeed() const
{
	switch (GetJavelinMode())
	{
	case EJavelinMoveMode::Flight:
		return FlightBoostSpeed + DiveSpeedBonus;
	case EJavelinMoveMode::Hover:
		return HoverSpeed;
	default:
		break;
	}

	float Speed = Super::GetMaxSpeed();
	if (IsSprinting() && !IsCrouching())
	{
		Speed = SprintSpeed;
	}
	if (IsMovingOnGround() && IsRecoveringFromLanding())
	{
		Speed *= HardLandingSpeedScale;
	}
	return Speed;
}

FVector UJavelinMovementComponent::GetAimDirection() const
{
	if (CharacterOwner && CharacterOwner->GetController())
	{
		return CharacterOwner->GetControlRotation().Vector();
	}
	return UpdatedComponent->GetForwardVector();
}

void UJavelinMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (GetJavelinMode())
	{
	case EJavelinMoveMode::Flight:
		PhysFlight(DeltaTime, Iterations);
		break;
	case EJavelinMoveMode::Hover:
		PhysHover(DeltaTime, Iterations);
		break;
	default:
		SetMovementMode(MOVE_Falling);
		break;
	}
}

void UJavelinMovementComponent::PhysFlight(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// The camera steers. Left/right input bends the path off the aim a little.
	const FRotator ControlRotation = CharacterOwner->GetControlRotation();
	const FVector Right = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::Y);
	const FVector AimDirection = (GetAimDirection() + Right * FlightInput.X * FlightStrafeInfluence).GetSafeNormal();

	// The heading swings toward the aim. The faster you go, the slower it turns, so fast turns are wide arcs.
	const float SpeedAlpha = FMath::GetMappedRangeValueClamped(FVector2D(FlightSlowSpeed, FlightBoostSpeed), FVector2D(0.f, 1.f), Velocity.Size());
	const float TurnRate = FMath::Lerp(FlightTurnRate, FlightTurnRateAtSpeed, SpeedAlpha);
	if (FlightHeading.IsNearlyZero())
	{
		FlightHeading = Velocity.IsNearlyZero() ? AimDirection : Velocity.GetSafeNormal();
	}
	FlightHeading = FMath::VInterpNormalRotationTo(FlightHeading, AimDirection, DeltaTime, TurnRate).GetSafeNormal();

	// Split the velocity into speed along the heading and sideways drift. Drift bleeds off as the suit grips the air.
	float ForwardSpeed = static_cast<float>(FVector::DotProduct(Velocity, FlightHeading));
	FVector Drift = Velocity - FlightHeading * ForwardSpeed;
	Drift *= FMath::Exp(-FlightGrip * DeltaTime);

	float TargetSpeed = FlightCruiseSpeed;
	if (FlightInput.Y > KINDA_SMALL_NUMBER)
	{
		TargetSpeed = FMath::Lerp(FlightCruiseSpeed, FlightBoostSpeed, FlightInput.Y);
	}
	else if (FlightInput.Y < -KINDA_SMALL_NUMBER)
	{
		TargetSpeed = FMath::Lerp(FlightCruiseSpeed, FlightSlowSpeed, -FlightInput.Y);
	}
	TargetSpeed += FMath::Max(0.f, -FlightHeading.Z) * DiveSpeedBonus;
	TargetSpeed -= FMath::Max(0.f, FlightHeading.Z) * ClimbSpeedPenalty;

	// Thrusters push up toward the target speed quickly; drag pulls a faster suit back slowly, so momentum carries.
	if (ForwardSpeed < TargetSpeed)
	{
		ForwardSpeed = FMath::Min(TargetSpeed, ForwardSpeed + FlightThrustAcceleration * DeltaTime);
	}
	else
	{
		ForwardSpeed = FMath::Max(TargetSpeed, ForwardSpeed - FlightDragDeceleration * DeltaTime);
	}
	// Gravity along the path: diving builds speed, climbing costs it.
	ForwardSpeed -= static_cast<float>(FlightHeading.Z) * FlightGravity * DeltaTime;
	ForwardSpeed = FMath::Clamp(ForwardSpeed, FlightMinSpeed, FlightBoostSpeed + DiveSpeedBonus);

	Velocity = FlightHeading * ForwardSpeed + Drift;
	const FVector Direction = Velocity.GetSafeNormal();
	const float Speed = static_cast<float>(Velocity.Size());

	Iterations++;
	bJustTeleported = false;

	// The capsule only yaws, along the heading. The character tilts its visual mesh for pitch and bank.
	const FVector Delta = Velocity * DeltaTime;
	const FQuat NewRotation = FRotator(0.f, FlightHeading.Rotation().Yaw, 0.f).Quaternion();
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, NewRotation, true, Hit);

	if (!Hit.IsValidBlockingHit())
	{
		return;
	}

	// Touching down on walkable ground ends the flight with a landing.
	if (IsWalkable(Hit) && Direction.Z < 0.1f)
	{
		SetMovementMode(MOVE_Walking);
		return;
	}

	// Slamming head-on into a wall knocks you out of the sky.
	if (Speed >= CrashMinSpeed && FVector::DotProduct(Direction, -Hit.Normal) >= CrashImpactDot)
	{
		const FVector ImpactVelocity = Velocity;
		Velocity = Hit.Normal * 400.f + FVector(0.f, 0.f, 150.f);
		SetMovementMode(MOVE_Falling);
		OnCrashed.Broadcast(ImpactVelocity, Hit.ImpactPoint);
		return;
	}

	// Glancing hits slide along the surface and keep flying, losing a bit of speed to the scrape.
	HandleImpact(Hit, DeltaTime, Delta);
	SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal) * 0.97f;
}

void UJavelinMovementComponent::PhysHover(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	CalcVelocity(DeltaTime, HoverFriction, false, HoverBrakingDeceleration);

	// Hold altitude: bleed off vertical speed carried in from a jump, fall or flight.
	Velocity.Z = FMath::FInterpTo(Velocity.Z, 0.f, DeltaTime, HoverVerticalDamping);

	Iterations++;
	bJustTeleported = false;

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

void UJavelinMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (MovementMode != MOVE_Custom)
	{
		Super::PhysicsRotation(DeltaTime);
		return;
	}

	// Flight sets rotation in PhysFlight. Hover turns to face where the camera looks.
	if (IsHovering() && CharacterOwner && CharacterOwner->GetController())
	{
		const FRotator Current = UpdatedComponent->GetComponentRotation();
		const FRotator Target(0.f, CharacterOwner->GetControlRotation().Yaw, 0.f);
		MoveUpdatedComponent(FVector::ZeroVector, FMath::RInterpTo(Current, Target, DeltaTime, HoverTurnSpeed), false);
	}
}
