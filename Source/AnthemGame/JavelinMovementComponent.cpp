#include "JavelinMovementComponent.h"
#include "GameFramework/Character.h"

UJavelinMovementComponent::UJavelinMovementComponent()
{
	MaxWalkSpeed = 600.f;
	MaxAcceleration = 2400.f;
	JumpZVelocity = 750.f;
	AirControl = 0.6f;
	GravityScale = 1.5f;
	bOrientRotationToMovement = true;
	RotationRate = FRotator(0.f, 720.f, 0.f);
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
		// Launch straight up; PhysFlight then curves the path toward the aim.
		Velocity = FVector::UpVector * FlightTakeoffSpeed;
	}
	else
	{
		Velocity = GetAimDirection() * FMath::Max(Velocity.Size(), FlightEntryMinSpeed);
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

	if (IsSprinting() && !IsCrouching())
	{
		return SprintSpeed;
	}
	return Super::GetMaxSpeed();
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

	FVector Direction = Velocity.IsNearlyZero() ? AimDirection : Velocity.GetSafeNormal();
	Direction = FMath::VInterpNormalRotationTo(Direction, AimDirection, DeltaTime, FlightTurnRate);

	float TargetSpeed = FlightCruiseSpeed;
	if (FlightInput.Y > KINDA_SMALL_NUMBER)
	{
		TargetSpeed = FMath::Lerp(FlightCruiseSpeed, FlightBoostSpeed, FlightInput.Y);
	}
	else if (FlightInput.Y < -KINDA_SMALL_NUMBER)
	{
		TargetSpeed = FMath::Lerp(FlightCruiseSpeed, FlightSlowSpeed, -FlightInput.Y);
	}
	TargetSpeed += FMath::Max(0.f, -Direction.Z) * DiveSpeedBonus;
	TargetSpeed -= FMath::Max(0.f, Direction.Z) * ClimbSpeedPenalty;

	const float Speed = FMath::FInterpTo(Velocity.Size(), TargetSpeed, DeltaTime, FlightSpeedInterp);
	Velocity = Direction * Speed;

	Iterations++;
	bJustTeleported = false;

	// The capsule only yaws. The character tilts its visual mesh for pitch and bank.
	const FVector Delta = Velocity * DeltaTime;
	const FQuat NewRotation = FRotator(0.f, Direction.Rotation().Yaw, 0.f).Quaternion();
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
		Velocity = Hit.Normal * 400.f;
		SetMovementMode(MOVE_Falling);
		return;
	}

	// Glancing hits slide along the surface and keep flying.
	HandleImpact(Hit, DeltaTime, Delta);
	SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
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
