#include "EnemyDrone.h"
#include "Components/CapsuleComponent.h"
#include "DustEffect.h"
#include "Engine/World.h"
#include "JavelinCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "ShotEffect.h"

AEnemyDrone::AEnemyDrone()
{
	// A ball-shaped collision. The actor sits at the drone's center.
	Collision->InitCapsuleSize(70.f, 70.f);
	HealthBarHeight = 110.f;

	AddPart(TEXT("Shell"), Body, SphereMesh, FVector::ZeroVector, FVector(110.f, 110.f, 90.f), PaintBody);
	AddPart(TEXT("Visor"), Body, CubeMesh, FVector(38.f, 0.f, 10.f), FVector(30.f, 70.f, 34.f), PaintArmor);
	AddPart(TEXT("Eye"), Body, SphereMesh, FVector(52.f, 0.f, 8.f), FVector(36.f), PaintWeakSpot);
	AddPart(TEXT("Antenna"), Body, CubeMesh, FVector(-15.f, 0.f, 62.f), FVector(6.f, 6.f, 46.f), PaintArmor, FRotator(-15.f, 0.f, 0.f));
	AddPart(TEXT("Thruster"), Body, ConeMesh, FVector(0.f, 0.f, -55.f), FVector(40.f, 40.f, 30.f), PaintArmor, FRotator(180.f, 0.f, 0.f));
	AddPart(TEXT("ThrusterGlow"), Body, CylinderMesh, FVector(0.f, 0.f, -72.f), FVector(26.f, 26.f, 4.f), PaintWeakSpot);

	Ring = AddPivot(TEXT("Ring"), Body, FVector::ZeroVector);
	AddPart(TEXT("RingBand"), Ring, CylinderMesh, FVector::ZeroVector, FVector(170.f, 170.f, 12.f), PaintArmor);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		// Four blocky fins around the ring so its spin shows.
		const FRotator Angle(0.f, Index * 90.f, 0.f);
		AddPart(*FString::Printf(TEXT("RingFin%d"), Index), Ring, CubeMesh, Angle.RotateVector(FVector(85.f, 0.f, 0.f)), FVector(22.f, 12.f, 22.f), PaintBody, Angle);
	}

	Muzzle->SetRelativeLocation(FVector(75.f, 0.f, 8.f));

	// Light and airborne: hits and blasts knock it around in every direction.
	bCanBeKnockedUp = true;
	Mass = 0.7f;
	FlinchPerDamage = 12.f;

	MaxHealth = 160.f;
	SightRange = 7000.f;
	MoveSpeed = 1100.f;
	MinCombatRange = 1500.f;
	MaxCombatRange = 3000.f;
	AttackRange = 6000.f;
	ProjectileDamage = 28.f;
	ProjectileSpeed = 3200.f;
	ProjectileSize = 2.2f;
	BurstCount = 1;
	TimeBetweenBursts = 2.8f;
	WindUpTime = 0.8f;
	AimSpread = 1.f;
	AimLead = 0.3f;
	HealthDropChance = 0.5f;
	AmmoDropChance = 0.4f;
	BodyColor = FLinearColor(0.18f, 0.08f, 0.22f);
	WeakSpotColor = FLinearColor(0.85f, 0.2f, 1.f);
	ProjectileColor = FLinearColor(0.75f, 0.2f, 1.f);
}

FText AEnemyDrone::GetEnemyName() const
{
	return NSLOCTEXT("Javelin", "EnemyDrone", "DRONE");
}

void AEnemyDrone::UpdateMovement(float DeltaSeconds, const AJavelinCharacter* Target)
{
	const FVector Location = GetActorLocation();
	AnimTime += DeltaSeconds;

	FVector Goal;
	if (Target)
	{
		// Head for where it would be a second from now if circling the player at a comfortable range, up high.
		const FVector TargetLocation = Target->GetActorLocation();
		FVector Away = Location - TargetLocation;
		Away.Z = 0.f;
		const float Distance = FMath::Clamp(Away.Size(), MinCombatRange, MaxCombatRange);
		const FVector Around = FRotator(0.f, OrbitSpeed * GetStrafeSign(), 0.f).RotateVector(Away.GetSafeNormal());
		Goal = TargetLocation + Around * Distance + FVector(0.f, 0.f, CombatHeight);
	}
	else
	{
		// Idle: drift lazily around where it started.
		Goal = HomeLocation + FVector(300.f * FMath::Sin(AnimTime * 0.3f), 300.f * FMath::Cos(AnimTime * 0.23f), 0.f);
	}

	// Stay clear of the ground.
	FVector Ground;
	if (FindGround(FVector(Goal.X, Goal.Y, Goal.Z + 5000.f), 20000.f, Ground))
	{
		Goal.Z = FMath::Max(Goal.Z, Ground.Z + MinAltitude);
	}

	// Ease toward the goal, slowing down as it arrives so it doesn't overshoot.
	const FVector ToGoal = Goal - Location;
	const FVector DesiredVelocity = ToGoal.GetSafeNormal() * FMath::Min(MoveSpeed, ToGoal.Size() * 1.5f);
	MoveVelocity = FMath::VInterpTo(MoveVelocity, DesiredVelocity, DeltaSeconds, 2.f);
	MoveWithSliding(MoveVelocity * DeltaSeconds);
}

void AEnemyDrone::UpdateFacing(float DeltaSeconds, const AJavelinCharacter* Target)
{
	Super::UpdateFacing(DeltaSeconds, Target);

	// Tip forward when speeding up and look down at the player, like a real hovering drone.
	const FVector LocalVelocity = GetActorRotation().UnrotateVector(MoveVelocity);
	float TargetPitch = FMath::Clamp(-LocalVelocity.X * 0.015f, -20.f, 20.f);
	if (Target)
	{
		const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		TargetPitch = FMath::Clamp(ToTarget.Rotation().Pitch, -60.f, 30.f);
	}
	BodyPitch = FMath::FInterpTo(BodyPitch, TargetPitch, DeltaSeconds, 4.f);
	Body->SetRelativeRotation(FRotator(BodyPitch, 0.f, FMath::Clamp(LocalVelocity.Y * 0.02f, -25.f, 25.f)));
}

void AEnemyDrone::UpdateAnimation(float DeltaSeconds)
{
	Ring->AddLocalRotation(FRotator(0.f, (IsEngaged() ? 220.f : 60.f) * DeltaSeconds, 0.f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 12.f * FMath::Sin(AnimTime * 2.f)));
}

void AEnemyDrone::UpdateDeath(float DeltaSeconds)
{
	UWorld* World = GetWorld();

	// Power's gone: it drops like a stone, keeps some of the push from what killed it, and spins out.
	DeathVelocity.Z -= 1400.f * DeltaSeconds;
	DeathVelocity *= FMath::Exp(-0.3f * DeltaSeconds);
	Body->AddLocalRotation(FRotator(140.f * DeltaSeconds, 320.f * DeltaSeconds, 210.f * DeltaSeconds));
	Ring->AddLocalRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f));

	// A trail of black smoke behind it.
	SmokeTimer -= DeltaSeconds;
	if (SmokeTimer <= 0.f)
	{
		SmokeTimer = 0.07f;
		ADustEffect::Spawn(World, GetActorLocation(), 0.f, 0.5f, 1, FLinearColor(0.04f, 0.04f, 0.045f), 1.2f);
	}

	// Blow up on hitting the ground (or give up after a while if it fell off the world).
	const FVector Step = DeathVelocity * DeltaSeconds;
	const float Radius = Collision->GetScaledCapsuleRadius();
	FVector Ground;
	const bool bHitGround = FindGround(GetActorLocation() + FVector(0.f, 0.f, 50.f), 50.f + Radius + FMath::Max(0.f, static_cast<float>(-Step.Z)), Ground);
	if (bHitGround || DeathTime > 6.f)
	{
		const FVector Blast = bHitGround ? Ground + FVector(0.f, 0.f, 30.f) : GetActorLocation();
		if (AShotEffect* Effect = World->SpawnActor<AShotEffect>(Blast, FRotator::ZeroRotator))
		{
			Effect->InitBurst(Blast, WeakSpotColor, 3.5f);
		}
		ADustEffect::Spawn(World, Blast, 260.f, 1.2f, 9, FLinearColor(0.08f, 0.07f, 0.06f), 1.3f);
		if (AJavelinCharacter* Player = Cast<AJavelinCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			// Feel it if you're close.
			const float Closeness = 1.f - FVector::Dist(Player->GetActorLocation(), Blast) / 3000.f;
			if (Closeness > 0.f)
			{
				Player->AddCameraTrauma(0.35f * Closeness);
				Player->PlayRumble(0.6f * Closeness, 0.25f, true);
			}
		}
		Destroy();
		return;
	}

	AddActorWorldOffset(Step, false);
}
