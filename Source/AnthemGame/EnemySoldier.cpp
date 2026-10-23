#include "EnemySoldier.h"
#include "Components/CapsuleComponent.h"
#include "JavelinCharacter.h"

AEnemySoldier::AEnemySoldier()
{
	// The capsule's feet are at Z = -95. Built from basic shapes, so no art is needed.
	Collision->InitCapsuleSize(40.f, 95.f);
	HealthBarHeight = 130.f;

	// Hips and legs. Each leg swings from its hip pivot.
	AddPart(TEXT("Pelvis"), Body, CubeMesh, FVector(0.f, 0.f, -8.f), FVector(30.f, 46.f, 20.f), PaintArmor);
	LeftHip = AddPivot(TEXT("LeftHip"), Body, FVector(0.f, -14.f, -15.f));
	RightHip = AddPivot(TEXT("RightHip"), Body, FVector(0.f, 14.f, -15.f));
	for (USceneComponent* Hip : { LeftHip.Get(), RightHip.Get() })
	{
		const FString Side = Hip == LeftHip ? TEXT("Left") : TEXT("Right");
		AddPart(*(Side + TEXT("Thigh")), Hip, CubeMesh, FVector(0.f, 0.f, -20.f), FVector(20.f, 18.f, 42.f), PaintBody);
		AddPart(*(Side + TEXT("Shin")), Hip, CubeMesh, FVector(2.f, 0.f, -56.f), FVector(18.f, 16.f, 34.f), PaintArmor);
		AddPart(*(Side + TEXT("Foot")), Hip, CubeMesh, FVector(7.f, 0.f, -76.f), FVector(32.f, 18.f, 9.f), PaintArmor);
	}

	// Torso with a chest plate, a power pack on the back, and shoulder pads.
	AddPart(TEXT("Torso"), Body, CubeMesh, FVector(0.f, 0.f, 25.f), FVector(36.f, 56.f, 58.f), PaintBody);
	AddPart(TEXT("ChestPlate"), Body, CubeMesh, FVector(20.f, 0.f, 32.f), FVector(8.f, 46.f, 36.f), PaintArmor, FRotator(-8.f, 0.f, 0.f));
	AddPart(TEXT("PowerPack"), Body, CubeMesh, FVector(-24.f, 0.f, 28.f), FVector(16.f, 38.f, 44.f), PaintArmor);
	AddPart(TEXT("PowerCell"), Body, CylinderMesh, FVector(-33.f, 0.f, 30.f), FVector(12.f, 12.f, 30.f), PaintWeakSpot);

	// Head: a glowing visor face (the weak spot, so headshots do double damage) inside an armored shell.
	AddPart(TEXT("Visor"), Body, CubeMesh, FVector(0.f, 0.f, 70.f), FVector(30.f, 28.f, 30.f), PaintWeakSpot);
	AddPart(TEXT("HelmetShell"), Body, CubeMesh, FVector(-3.f, 0.f, 74.f), FVector(28.f, 32.f, 26.f), PaintArmor);
	AddPart(TEXT("Crest"), Body, CubeMesh, FVector(-2.f, 0.f, 90.f), FVector(26.f, 6.f, 8.f), PaintBody);

	// Arms. The left one steadies the rifle; the right one holds it.
	LeftShoulder = AddPivot(TEXT("LeftShoulder"), Body, FVector(0.f, -36.f, 45.f));
	RightShoulder = AddPivot(TEXT("RightShoulder"), Body, FVector(0.f, 36.f, 45.f));
	AddPart(TEXT("LeftPad"), LeftShoulder, CubeMesh, FVector(0.f, -4.f, 6.f), FVector(26.f, 22.f, 14.f), PaintArmor);
	AddPart(TEXT("RightPad"), RightShoulder, CubeMesh, FVector(0.f, 4.f, 6.f), FVector(26.f, 22.f, 14.f), PaintArmor);
	AddPart(TEXT("LeftArm"), LeftShoulder, CubeMesh, FVector(18.f, 10.f, -14.f), FVector(40.f, 13.f, 13.f), PaintBody, FRotator(-30.f, 25.f, 0.f));
	AddPart(TEXT("RightArm"), RightShoulder, CubeMesh, FVector(14.f, 0.f, -10.f), FVector(34.f, 14.f, 14.f), PaintBody, FRotator(-20.f, 0.f, 0.f));

	// Rifle held out in front, pointing along +X from the shoulder.
	AddPart(TEXT("Rifle"), RightShoulder, CubeMesh, FVector(42.f, -6.f, -14.f), FVector(64.f, 9.f, 13.f), PaintArmor);
	AddPart(TEXT("RifleBarrel"), RightShoulder, CylinderMesh, FVector(82.f, -6.f, -12.f), FVector(5.f, 5.f, 24.f), PaintBody, FRotator(90.f, 0.f, 0.f));
	AddPart(TEXT("RifleGlow"), RightShoulder, CubeMesh, FVector(36.f, -6.f, -6.f), FVector(22.f, 10.f, 3.f), PaintBody);
	Muzzle->SetupAttachment(RightShoulder);
	Muzzle->SetRelativeLocation(FVector(96.f, -6.f, -12.f));

	MaxHealth = 220.f;
	SightRange = 5000.f;
	MoveSpeed = 380.f;
	MinCombatRange = 900.f;
	MaxCombatRange = 2500.f;
	AttackRange = 4500.f;
	ProjectileDamage = 10.f;
	ProjectileSpeed = 5500.f;
	BurstCount = 3;
	TimeBetweenBurstShots = 0.15f;
	TimeBetweenBursts = 2.2f;
	WindUpTime = 0.35f;
	AimSpread = 2.5f;
	HealthDropChance = 0.35f;
	AmmoDropChance = 0.6f;
}

FText AEnemySoldier::GetEnemyName() const
{
	return NSLOCTEXT("Javelin", "EnemySoldier", "SOLDIER");
}

void AEnemySoldier::UpdateMovement(float DeltaSeconds, const AJavelinCharacter* Target)
{
	AimTarget = Target;
	const FVector Location = GetActorLocation();

	FVector Desired = FVector::ZeroVector;
	if (Target)
	{
		FVector ToTarget = Target->GetActorLocation() - Location;
		ToTarget.Z = 0.f;
		const float Distance = ToTarget.Size();
		const FVector Forward = ToTarget.GetSafeNormal();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

		// Close in until it has a clear shot within range, back off if you get too close, and strafe in between.
		if (Distance > MaxCombatRange || !CanSeePlayer())
		{
			Desired = Forward;
			if (bBlocked)
			{
				// Walk around whatever is in the way.
				Desired += Right * GetStrafeSign() * 1.5f;
			}
		}
		else if (Distance < MinCombatRange)
		{
			Desired = -Forward + Right * GetStrafeSign() * 0.5f;
		}
		else
		{
			Desired = Right * GetStrafeSign() * 0.7f;
		}
	}
	else
	{
		// Nothing to fight: walk back to where it started.
		FVector ToHome = HomeLocation - Location;
		ToHome.Z = 0.f;
		if (ToHome.Size() > 150.f)
		{
			Desired = ToHome.GetSafeNormal() * 0.6f;
		}
	}

	const FVector DesiredVelocity = Desired.GetClampedToMaxSize(1.f) * MoveSpeed;
	MoveVelocity = FMath::VInterpTo(MoveVelocity, DesiredVelocity, DeltaSeconds, 6.f);
	bBlocked = MoveWithSliding(MoveVelocity * DeltaSeconds);

	// Stick to the ground, or fall if there's none under its feet.
	const float HalfHeight = Collision->GetScaledCapsuleHalfHeight();
	const FVector NewLocation = GetActorLocation();
	FVector Ground;
	if (FallSpeed <= 0.f && FindGround(NewLocation + FVector(0.f, 0.f, 40.f), HalfHeight + 80.f, Ground))
	{
		// A small gap keeps the capsule from scraping the floor, which would block sideways moves.
		SetActorLocation(FVector(NewLocation.X, NewLocation.Y, Ground.Z + HalfHeight + 2.f));
	}
	else
	{
		FallSpeed = FMath::Min(FallSpeed + 980.f * DeltaSeconds, 4000.f);
		FHitResult Hit;
		AddActorWorldOffset(FVector(0.f, 0.f, -FallSpeed * DeltaSeconds), true, &Hit);
		if (Hit.IsValidBlockingHit())
		{
			FallSpeed = 0.f;
		}
	}
}

void AEnemySoldier::UpdateAnimation(float DeltaSeconds)
{
	// Legs swing with ground speed so the feet roughly match the distance covered.
	const float Speed = FVector(MoveVelocity.X, MoveVelocity.Y, 0.f).Size();
	const float Stride = FMath::Clamp(Speed / FMath::Max(MoveSpeed, 1.f), 0.f, 1.f);
	WalkPhase = FMath::Fmod(WalkPhase + DeltaSeconds * UE_TWO_PI * Speed / FMath::Max(StrideLength, 1.f), UE_TWO_PI);

	// Walking sideways swings the legs out to the side instead of forward.
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(MoveVelocity);
	const float ForwardShare = Speed > 1.f ? LocalVelocity.X / Speed : 1.f;
	const float SideShare = Speed > 1.f ? LocalVelocity.Y / Speed : 0.f;
	const float Swing = 30.f * Stride * FMath::Sin(WalkPhase);
	LeftHip->SetRelativeRotation(FRotator(Swing * ForwardShare, 0.f, -Swing * SideShare));
	RightHip->SetRelativeRotation(FRotator(-Swing * ForwardShare, 0.f, Swing * SideShare));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -3.f * Stride * FMath::Abs(FMath::Cos(WalkPhase))));

	// Point the rifle at the player, or hold it low when there's no one to shoot.
	float TargetPitch = -25.f;
	if (const AJavelinCharacter* Target = AimTarget.Get())
	{
		const FVector ToTarget = Target->GetActorLocation() - RightShoulder->GetComponentLocation();
		TargetPitch = FMath::Clamp(ToTarget.Rotation().Pitch, -60.f, 75.f);
	}
	AimPitch = FMath::FInterpTo(AimPitch, TargetPitch, DeltaSeconds, 8.f);
	RightShoulder->SetRelativeRotation(FRotator(AimPitch, 0.f, 0.f));
	LeftShoulder->SetRelativeRotation(FRotator(AimPitch * 0.8f, 0.f, 0.f));
}
