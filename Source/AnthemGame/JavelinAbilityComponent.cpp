#include "JavelinAbilityComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "JavelinCharacter.h"
#include "JavelinEnemy.h"
#include "JavelinGrenade.h"
#include "JavelinMissile.h"
#include "JavelinWeaponComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ShootingTarget.h"
#include "ShotEffect.h"

namespace
{
	/** How hard a blast throws enemies at its center, in cm/s. Lighter enemies fly further. */
	constexpr float BlastKnockback = 1300.f;

	/** Missiles leave the pod at this speed, then speed up as they home in. */
	constexpr float MissileLaunchSpeed = 1400.f;

	/** Blasts do full damage out to this fraction of their radius, then fall off to EdgeDamage at the edge. */
	constexpr float FullDamageFraction = 0.35f;
	constexpr float EdgeDamage = 0.3f;
}

UJavelinAbilityComponent::UJavelinAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	GrenadeSlot.Name = NSLOCTEXT("Javelin", "FragGrenade", "FRAG GRENADE");
	GrenadeSlot.ShortName = NSLOCTEXT("Javelin", "FragGrenadeShort", "FRAG");
	GrenadeSlot.Cooldown = 8.f;
	GrenadeSlot.Color = FLinearColor(1.f, 0.55f, 0.1f);

	MissileSlot.Name = NSLOCTEXT("Javelin", "HomingMissiles", "HOMING MISSILES");
	MissileSlot.ShortName = NSLOCTEXT("Javelin", "HomingMissilesShort", "MISSILES");
	MissileSlot.Cooldown = 12.f;
	MissileSlot.Color = FLinearColor(0.3f, 0.85f, 1.f);
}

void UJavelinAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetCooldowns();
}

void UJavelinAbilityComponent::SetLaunchPoints(USceneComponent* InGrenadePoint, USceneComponent* InMissilePod)
{
	GrenadePoint = InGrenadePoint;
	MissilePod = InMissilePod;
}

FJavelinGearSlot& UJavelinAbilityComponent::GetSlotMutable(EJavelinGear Gear)
{
	return Gear == EJavelinGear::Grenade ? GrenadeSlot : MissileSlot;
}

const FJavelinGearSlot& UJavelinAbilityComponent::GetSlot(EJavelinGear Gear) const
{
	return Gear == EJavelinGear::Grenade ? GrenadeSlot : MissileSlot;
}

bool UJavelinAbilityComponent::CanUseGear() const
{
	// Same rule as the gun: not while flying, not while the suit is down.
	const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(GetOwner());
	return Javelin && Javelin->CanFireWeapon();
}

bool UJavelinAbilityComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

FVector UJavelinAbilityComponent::GetLaunchLocation(const USceneComponent* Point) const
{
	return Point ? Point->GetComponentLocation() : GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
}

bool UJavelinAbilityComponent::UseGear(EJavelinGear Gear)
{
	FJavelinGearSlot& Slot = GetSlotMutable(Gear);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Slot.CooldownRemaining > 0.f || !CanUseGear() || (Gear == EJavelinGear::Missiles && MissilesLeft > 0))
	{
		Slot.LastDeniedTime = Now;
		return false;
	}

	if (Gear == EJavelinGear::Grenade)
	{
		ThrowGrenade();
	}
	else
	{
		StartMissileVolley();
	}

	Slot.CooldownRemaining = Slot.Cooldown;
	Slot.LastUsedTime = Now;
	if (Slot.Cooldown <= 0.f)
	{
		Slot.LastReadyTime = Now;
	}
	return true;
}

bool UJavelinAbilityComponent::IsGearReady(EJavelinGear Gear) const
{
	return GetSlot(Gear).CooldownRemaining <= 0.f;
}

float UJavelinAbilityComponent::GetCooldownRemaining(EJavelinGear Gear) const
{
	return FMath::Max(0.f, GetSlot(Gear).CooldownRemaining);
}

float UJavelinAbilityComponent::GetCooldownPercent(EJavelinGear Gear) const
{
	const FJavelinGearSlot& Slot = GetSlot(Gear);
	return Slot.Cooldown > 0.f ? 1.f - FMath::Clamp(Slot.CooldownRemaining / Slot.Cooldown, 0.f, 1.f) : 1.f;
}

void UJavelinAbilityComponent::ResetCooldowns()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (FJavelinGearSlot* Slot : { &GrenadeSlot, &MissileSlot })
	{
		if (Slot->CooldownRemaining > 0.f)
		{
			Slot->LastReadyTime = Now;
		}
		Slot->CooldownRemaining = 0.f;
	}
	MissilesLeft = 0;
}

float UJavelinAbilityComponent::GetTimeSinceUsed(EJavelinGear Gear) const
{
	return GetWorld()->GetTimeSeconds() - GetSlot(Gear).LastUsedTime;
}

float UJavelinAbilityComponent::GetTimeSinceReady(EJavelinGear Gear) const
{
	return GetWorld()->GetTimeSeconds() - GetSlot(Gear).LastReadyTime;
}

float UJavelinAbilityComponent::GetTimeSinceDenied(EJavelinGear Gear) const
{
	return GetWorld()->GetTimeSeconds() - GetSlot(Gear).LastDeniedTime;
}

void UJavelinAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Now = GetWorld()->GetTimeSeconds();
	for (FJavelinGearSlot* Slot : { &GrenadeSlot, &MissileSlot })
	{
		if (Slot->CooldownRemaining > 0.f)
		{
			Slot->CooldownRemaining -= DeltaTime;
			if (Slot->CooldownRemaining <= 0.f)
			{
				Slot->CooldownRemaining = 0.f;
				Slot->LastReadyTime = Now;
			}
		}
	}

	// The volley fires one missile at a time. Taking off or going down stops it.
	if (MissilesLeft > 0)
	{
		if (!CanUseGear())
		{
			MissilesLeft = 0;
		}
		MissileTimer -= DeltaTime;
		while (MissilesLeft > 0 && MissileTimer <= 0.f)
		{
			FireMissile();
			--MissilesLeft;
			MissileTimer += TimeBetweenMissiles;
		}
	}

	// Looking for lock-on targets a few times a second is plenty for the HUD brackets.
	LockUpdateTimer -= DeltaTime;
	if (LockUpdateTimer <= 0.f)
	{
		LockUpdateTimer = 0.1f;
		if (IsGearReady(EJavelinGear::Missiles) && MissilesLeft == 0 && CanUseGear())
		{
			UpdateMissileLocks();
		}
		else
		{
			MissileLocks.Reset();
		}
	}
}

void UJavelinAbilityComponent::ThrowGrenade()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(Pawn);
	if (!World || !Javelin)
	{
		return;
	}

	// Aim for the spot under the crosshair, lobbing it up just enough that gravity brings it down there.
	const FVector From = GetLaunchLocation(GrenadePoint);
	const FVector AimPoint = Javelin->GetWeapon()->GetAimPoint();
	const FVector ToAim = AimPoint - From;
	const FVector Direction = ToAim.IsNearlyZero() ? Pawn->GetActorForwardVector() : ToAim.GetSafeNormal();
	const float Distance = FMath::Clamp(static_cast<float>(ToAim.Size()), 300.f, GrenadeMaxAimDistance);
	const float FlightTime = Distance / GrenadeThrowSpeed;
	const FVector Velocity = Direction * GrenadeThrowSpeed + FVector(0.f, 0.f, -World->GetGravityZ() * 0.5f * FlightTime);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Pawn;
	SpawnParams.Instigator = Pawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJavelinGrenade* Grenade = World->SpawnActor<AJavelinGrenade>(From, Velocity.Rotation(), SpawnParams))
	{
		Grenade->Throw(Velocity, GrenadeDamage, GrenadeBlastRadius, GrenadeSlot.Color);
	}
}

void UJavelinAbilityComponent::StartMissileVolley()
{
	const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(GetOwner());
	UpdateMissileLocks();
	VolleyTargets = MissileLocks;
	VolleyAimPoint = Javelin ? Javelin->GetWeapon()->GetAimPoint() : GetOwner()->GetActorLocation();
	MissileLocks.Reset();

	MissilesLeft = MissileCount;
	MissilesFired = 0;
	MissileTimer = 0.f;
}

void UJavelinAbilityComponent::FireMissile()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!World || !Pawn)
	{
		return;
	}

	// Locked targets are shared out in turn. If one has died already, try the next.
	AActor* Target = nullptr;
	for (int32 Offset = 0; Offset < VolleyTargets.Num() && !Target; ++Offset)
	{
		AActor* Candidate = VolleyTargets[(MissilesFired + Offset) % VolleyTargets.Num()].Get();
		if (UJavelinWeaponComponent::IsLivingTarget(Candidate))
		{
			Target = Candidate;
		}
	}

	// Each missile pops up out of the pod and fans out to one side before it turns toward its target.
	FVector ViewLocation;
	FRotator ViewRotation = Pawn->GetActorRotation();
	GetView(ViewLocation, ViewRotation);
	const FRotationMatrix Basis(FRotator(0.f, ViewRotation.Yaw, 0.f));
	const float Side = MissilesFired % 2 == 0 ? -1.f : 1.f;
	const float Spread = 0.25f + 0.25f * ((MissilesFired / 2) % 2);
	const FVector Direction = (Basis.GetUnitAxis(EAxis::Z) * 0.9f + ViewRotation.Vector() * 0.6f
		+ Basis.GetUnitAxis(EAxis::Y) * Side * Spread + FMath::VRand() * 0.1f).GetSafeNormal();

	const FVector From = GetLaunchLocation(MissilePod);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Pawn;
	SpawnParams.Instigator = Pawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJavelinMissile* Missile = World->SpawnActor<AJavelinMissile>(From, Direction.Rotation(), SpawnParams))
	{
		Missile->Launch(Direction * MissileLaunchSpeed, Target, VolleyAimPoint, MissileDamage, MissileBlastRadius, MissileSlot.Color);
	}
	if (AShotEffect* Flash = World->SpawnActor<AShotEffect>(From, FRotator::ZeroRotator))
	{
		Flash->InitBurst(From, MissileSlot.Color, 0.4f);
	}
	++MissilesFired;
}

void UJavelinAbilityComponent::UpdateMissileLocks()
{
	MissileLocks.Reset();
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!World || !Owner || !GetView(ViewLocation, ViewRotation))
	{
		return;
	}

	const FVector ViewDirection = ViewRotation.Vector();
	const FVector From = GetLaunchLocation(MissilePod);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinMissileLock), false, Owner);

	// Anything living, in range, near the crosshair and in clear view of the launcher. Closest to the crosshair first.
	struct FLockCandidate
	{
		AActor* Actor;
		float Angle;
	};
	TArray<FLockCandidate> Candidates;
	auto Consider = [&](AActor* Actor)
	{
		if (!UJavelinWeaponComponent::IsLivingTarget(Actor))
		{
			return;
		}
		const FVector Center = GetTargetCenter(Actor);
		if (FVector::Dist(Center, Owner->GetActorLocation()) > MissileLockRange)
		{
			return;
		}
		const float Cosine = FVector::DotProduct((Center - ViewLocation).GetSafeNormal(), ViewDirection);
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cosine, -1.f, 1.f)));
		if (Angle > MissileLockAngle)
		{
			return;
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, Center, ECC_Visibility, Params) && Hit.GetActor() != Actor)
		{
			return;
		}
		Candidates.Add(FLockCandidate{ Actor, Angle });
	};
	for (TActorIterator<AJavelinEnemy> It(World); It; ++It)
	{
		Consider(*It);
	}
	for (TActorIterator<AShootingTarget> It(World); It; ++It)
	{
		Consider(*It);
	}

	Candidates.Sort([](const FLockCandidate& A, const FLockCandidate& B) { return A.Angle < B.Angle; });
	for (int32 Index = 0; Index < FMath::Min(Candidates.Num(), MissileCount); ++Index)
	{
		MissileLocks.Add(Candidates[Index].Actor);
	}
}

FVector UJavelinAbilityComponent::GetTargetCenter(const AActor* Target)
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	if (Cast<AJavelinEnemy>(Target))
	{
		// Enemies are centered on their collision capsule.
		return Target->GetActorLocation();
	}
	return Target->GetComponentsBoundingBox().GetCenter();
}

float UJavelinAbilityComponent::ApplyBlastDamage(UWorld* World, const FVector& Center, float Damage, float Radius, AActor* DamageCauser, APawn* InstigatorPawn)
{
	if (!World || Damage <= 0.f || Radius <= 0.f)
	{
		return 0.f;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinBlast), false, InstigatorPawn);
	Params.AddIgnoredActor(DamageCauser);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(Radius), Params);

	// Find the nearest point of each living enemy or target that got caught in the blast.
	struct FBlastVictim
	{
		float Distance;
		FVector Point;
	};
	TMap<AActor*, FBlastVictim> Victims;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Component || !UJavelinWeaponComponent::IsLivingTarget(Actor))
		{
			continue;
		}
		FVector Point;
		float Distance = Component->GetClosestPointOnCollision(Center, Point);
		if (Distance < 0.f)
		{
			Point = Component->Bounds.Origin;
			Distance = FVector::Dist(Center, Point);
		}
		FBlastVictim* Existing = Victims.Find(Actor);
		if (!Existing || Distance < Existing->Distance)
		{
			Victims.Add(Actor, FBlastVictim{ Distance, Point });
		}
	}

	AController* Controller = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;
	UJavelinWeaponComponent* Feedback = nullptr;
	if (const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(InstigatorPawn))
	{
		Feedback = Javelin->GetWeapon();
	}

	float Total = 0.f;
	for (const TPair<AActor*, FBlastVictim>& Victim : Victims)
	{
		AActor* Actor = Victim.Key;

		// Walls protect what's behind them.
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Center, Victim.Value.Point, ECC_Visibility, Params) && Hit.GetActor() != Actor)
		{
			continue;
		}

		const float FullRadius = Radius * FullDamageFraction;
		const float Falloff = Victim.Value.Distance <= FullRadius ? 1.f
			: FMath::GetMappedRangeValueClamped(FVector2D(FullRadius, Radius), FVector2D(1.f, EdgeDamage), Victim.Value.Distance);
		const float Dealt = UGameplayStatics::ApplyDamage(Actor, Damage * Falloff, Controller, DamageCauser, UDamageType::StaticClass());
		if (Dealt > 0.f)
		{
			Total += Dealt;
			if (Feedback)
			{
				Feedback->AddHitFeedback(Victim.Value.Point, Dealt, false, !UJavelinWeaponComponent::IsLivingTarget(Actor));
			}

			// The blast throws enemies away from it, a little upward, hardest near the middle.
			if (AJavelinEnemy* Enemy = Cast<AJavelinEnemy>(Actor))
			{
				FVector Away = Victim.Value.Point - Center;
				Away = Away.IsNearlyZero() ? FVector::UpVector : Away.GetSafeNormal();
				Away.Z += 0.35f;
				Enemy->ApplyKnockback(Away.GetSafeNormal() * BlastKnockback * Falloff);
			}
		}
	}

	// Explosions shake the camera, more the closer you are.
	if (AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(InstigatorPawn))
	{
		const float Closeness = 1.f - FVector::Dist(Javelin->GetActorLocation(), Center) / (Radius * 5.f);
		if (Closeness > 0.f)
		{
			Javelin->AddCameraTrauma(0.45f * Closeness);
			Javelin->PlayRumble(0.7f * Closeness, 0.3f, true);
		}
	}
	return Total;
}
