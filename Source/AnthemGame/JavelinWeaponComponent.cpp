#include "JavelinWeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "JavelinCharacter.h"
#include "JavelinEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "ShootingTarget.h"
#include "ShotEffect.h"

namespace
{
	const FName WeakSpotTag(TEXT("WeakSpot"));
	const FName ArmorPlateTag(TEXT("Paint.Armor"));

	/** A pause this long between shots counts as letting go of the trigger, for recoil climb and recovery. */
	constexpr float RecoilResetTime = 0.15f;
}

bool UJavelinWeaponComponent::IsLivingTarget(const AActor* Actor)
{
	// Also how a hit that drops something to zero shows as a kill.
	if (const AShootingTarget* Target = Cast<AShootingTarget>(Actor))
	{
		return Target->IsAlive();
	}
	if (const AJavelinEnemy* Enemy = Cast<AJavelinEnemy>(Actor))
	{
		return Enemy->IsAlive();
	}
	return false;
}

UJavelinWeaponComponent::UJavelinWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UJavelinWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	Ammo = MagazineSize;
	ReserveAmmo = MaxReserveAmmo;
}

bool UJavelinWeaponComponent::CanFire() const
{
	const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(GetOwner());
	return Javelin && Javelin->CanFireWeapon() && !bReloading && Ammo > 0;
}

bool UJavelinWeaponComponent::IsAiming() const
{
	const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(GetOwner());
	return bWantsToAim && Javelin && Javelin->CanAimWeapon();
}

void UJavelinWeaponComponent::SetTriggerHeld(bool bHeld)
{
	bTriggerHeld = bHeld;

	// Fire the first shot right away so taps feel responsive.
	if (bHeld && ShotCooldown <= 0.f && CanFire())
	{
		FireShot();
		ShotCooldown = 60.f / FireRate;
	}
}

void UJavelinWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReloading)
	{
		ReloadRemaining -= DeltaTime;
		if (ReloadRemaining <= 0.f)
		{
			FinishReload();
		}
	}

	// Pulling the trigger on an empty magazine reloads.
	if (bTriggerHeld && Ammo == 0 && !bReloading)
	{
		StartReload();
	}

	// Leftover time carries into the next shot so the fire rate holds steady at any frame rate.
	const float TimeBetweenShots = 60.f / FireRate;
	ShotCooldown -= DeltaTime;
	if (bTriggerHeld && ShotCooldown <= 0.f && CanFire())
	{
		FireShot();
		ShotCooldown += TimeBetweenShots;
	}
	ShotCooldown = FMath::Max(ShotCooldown, bTriggerHeld ? -TimeBetweenShots : 0.f);

	Bloom = FMath::Max(0.f, Bloom - BloomRecovery * DeltaTime);
	RecoilKick = FMath::Max(0.f, RecoilKick - DeltaTime * 10.f);

	// Once you stop firing, the view settles back down part of the way the recoil pushed it.
	if (GetTimeSinceLastShot() > RecoilResetTime)
	{
		SustainedShots = 0;
		if (RecoilToRecover > 0.f)
		{
			const APawn* Pawn = Cast<APawn>(GetOwner());
			if (AController* Controller = Pawn ? Pawn->GetController() : nullptr)
			{
				// Ease out: fast at first, gentler as it arrives.
				const float Step = FMath::Min(RecoilToRecover, FMath::Max(RecoilRecoverySpeed, RecoilToRecover * 5.f) * DeltaTime);
				Controller->SetControlRotation(Controller->GetControlRotation() - FRotator(Step, 0.f, 0.f));
				RecoilToRecover -= Step;
			}
			else
			{
				RecoilToRecover = 0.f;
			}
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	DamageNumbers.RemoveAll([Now](const FJavelinDamageNumber& Number) { return Now - Number.SpawnTime > DamageNumberLifetime; });
}

void UJavelinWeaponComponent::FireShot()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return;
	}

	--Ammo;
	LastShotTime = World->GetTimeSeconds();

	// Aim from the camera so shots go where the crosshair points.
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ShotDirection = FMath::VRandCone(ViewRotation.Vector(), FMath::DegreesToRadians(GetCurrentSpread() * 0.5f));

	FHitResult Hit;
	bool bViewHit = false;
	const FVector AimPoint = TraceFromView(ViewLocation, ShotDirection, Hit, bViewHit);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinWeapon), false, Pawn);

	// Then check from the gun itself, so a wall right next to you blocks the shot.
	const FVector MuzzleLocation = Muzzle ? Muzzle->GetComponentLocation() : Pawn->GetActorLocation();
	FHitResult MuzzleHit;
	const FVector ToAim = AimPoint - MuzzleLocation;
	const bool bHit = World->LineTraceSingleByChannel(MuzzleHit, MuzzleLocation, AimPoint + ToAim.GetSafeNormal() * 10.f, ECC_Visibility, Params);
	const FVector EndPoint = bHit ? FVector(MuzzleHit.ImpactPoint) : AimPoint;

	if (AShotEffect* Effect = World->SpawnActor<AShotEffect>(MuzzleLocation, FRotator::ZeroRotator))
	{
		Effect->InitShot(MuzzleLocation, EndPoint, bHit, bHit ? FVector(MuzzleHit.ImpactNormal) : FVector::ZeroVector, TracerColor);
	}

	AActor* HitActor = MuzzleHit.GetActor();
	if (bHit && HitActor && HitActor->CanBeDamaged())
	{
		const UPrimitiveComponent* HitPart = MuzzleHit.GetComponent();
		const bool bWeakSpot = HitPart && HitPart->ComponentHasTag(WeakSpotTag);
		const bool bArmorPlate = HitPart && HitPart->ComponentHasTag(ArmorPlateTag);

		// Long shots lose some punch.
		const double Distance = FVector::Dist(MuzzleLocation, MuzzleHit.ImpactPoint);
		const float Falloff = FMath::GetMappedRangeValueClamped(FVector2D(FalloffStartRange, FMath::Max(FalloffEndRange, FalloffStartRange + 1.f)),
			FVector2D(1.f, FalloffMinDamage), Distance);
		const float ShotDamage = Damage * Falloff * (bWeakSpot ? WeakSpotMultiplier : (bArmorPlate ? ArmorPlateMultiplier : 1.f));

		const bool bWasAlive = IsLivingTarget(HitActor);
		const float Dealt = UGameplayStatics::ApplyPointDamage(HitActor, ShotDamage, ShotDirection, MuzzleHit, Controller, Pawn, UDamageType::StaticClass());
		if (Dealt > 0.f)
		{
			AddHitFeedback(MuzzleHit.ImpactPoint, Dealt, bWeakSpot, bWasAlive && !IsLivingTarget(HitActor));
			if (AJavelinEnemy* Enemy = Cast<AJavelinEnemy>(HitActor))
			{
				Enemy->ApplyKnockback(ShotDirection * HitImpulse * (bWeakSpot ? 1.5f : 1.f));
			}
		}
	}

	Bloom = FMath::Min(Bloom + SpreadPerShot, MaxBloom);

	// Kick the view up. Holding the trigger the kick builds, and it drifts a little to one side.
	// When you let go, most of the climb settles back (see TickComponent); the rest you pull down yourself.
	const float RecoilScale = IsAiming() ? 0.5f : 1.f;
	const float Climb = 1.f + RecoilClimb * FMath::Min(1.f, static_cast<float>(SustainedShots) / FMath::Max(RecoilClimbShots, 1));
	const float KickPitch = RecoilPitch * RecoilScale * Climb;
	const float KickYaw = (RecoilYawDrift + FMath::FRandRange(-RecoilYaw, RecoilYaw)) * RecoilScale;
	Controller->SetControlRotation(Controller->GetControlRotation() + FRotator(KickPitch, KickYaw, 0.f));
	RecoilToRecover += KickPitch * RecoilRecovery;
	++SustainedShots;
	RecoilKick = 1.f;
	OnFired.Broadcast();

	// Anthem-style auto reload when the magazine runs dry.
	if (Ammo == 0)
	{
		StartReload();
	}
}

FVector UJavelinWeaponComponent::TraceFromView(const FVector& ViewLocation, const FVector& Direction, FHitResult& OutHit, bool& bOutHit) const
{
	const AActor* Owner = GetOwner();
	// Start level with the suit so nothing between the camera and the suit gets hit.
	const float SkipDistance = FMath::Max(0.f, FVector::DotProduct(Owner->GetActorLocation() - ViewLocation, Direction));
	const FVector TraceStart = ViewLocation + Direction * SkipDistance;
	const FVector TraceEnd = TraceStart + Direction * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinWeapon), false, Owner);
	bOutHit = GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, Params);
	return bOutHit ? FVector(OutHit.ImpactPoint) : TraceEnd;
}

bool UJavelinWeaponComponent::IsCrosshairOnTarget() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	bool bHit = false;
	TraceFromView(ViewLocation, ViewRotation.Vector(), Hit, bHit);
	return bHit && IsLivingTarget(Hit.GetActor());
}

FVector UJavelinWeaponComponent::GetAimPoint() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * Range;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	bool bHit = false;
	return TraceFromView(ViewLocation, ViewRotation.Vector(), Hit, bHit);
}

void UJavelinWeaponComponent::AddHitFeedback(const FVector& Location, float Amount, bool bWeakSpot, bool bKill)
{
	// A blast can hit several enemies at once; a kill among them should still show as a kill.
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bSameMoment = LastHitTime == Now;
	bLastHitWeakSpot = bWeakSpot || (bSameMoment && bLastHitWeakSpot);
	bLastHitKill = bKill || (bSameMoment && bLastHitKill);
	LastHitTime = Now;

	FJavelinDamageNumber& Number = DamageNumbers.AddDefaulted_GetRef();
	Number.Location = Location;
	Number.Amount = Amount;
	Number.bWeakSpot = bWeakSpot;
	Number.SpawnTime = LastHitTime;
}

bool UJavelinWeaponComponent::StartReload()
{
	if (bReloading || Ammo >= MagazineSize || ReserveAmmo <= 0)
	{
		return false;
	}
	bReloading = true;
	ReloadRemaining = ReloadTime;
	return true;
}

void UJavelinWeaponComponent::FinishReload()
{
	const int32 Loaded = FMath::Min(MagazineSize - Ammo, ReserveAmmo);
	Ammo += Loaded;
	ReserveAmmo -= Loaded;
	bReloading = false;
	ReloadRemaining = 0.f;
}

bool UJavelinWeaponComponent::RefillAmmo()
{
	if (Ammo >= MagazineSize && ReserveAmmo >= MaxReserveAmmo)
	{
		return false;
	}
	Ammo = MagazineSize;
	ReserveAmmo = MaxReserveAmmo;
	bReloading = false;
	return true;
}

bool UJavelinWeaponComponent::AddReserveAmmo(int32 Rounds)
{
	if (ReserveAmmo >= MaxReserveAmmo || Rounds <= 0)
	{
		return false;
	}
	ReserveAmmo = FMath::Min(MaxReserveAmmo, ReserveAmmo + Rounds);
	return true;
}

float UJavelinWeaponComponent::GetReloadProgress() const
{
	return bReloading && ReloadTime > 0.f ? 1.f - ReloadRemaining / ReloadTime : 0.f;
}

float UJavelinWeaponComponent::GetCurrentSpread() const
{
	float Spread = (IsAiming() ? AimSpread : HipSpread) + Bloom;

	// Shooting on the move or in mid-air is less accurate than standing or hovering still.
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const float MoveAlpha = FMath::Clamp(static_cast<float>(Character->GetVelocity().Size2D()) / 1100.f, 0.f, 1.f);
		float Extra = MovingSpread * MoveAlpha;
		if (Movement && Movement->IsFalling())
		{
			Extra += AirborneSpread;
		}
		Spread += Extra * (IsAiming() ? 0.33f : 1.f);
	}
	return Spread;
}

float UJavelinWeaponComponent::GetTimeSinceLastShot() const
{
	return GetWorld()->GetTimeSeconds() - LastShotTime;
}

float UJavelinWeaponComponent::GetTimeSinceLastHit() const
{
	return GetWorld()->GetTimeSeconds() - LastHitTime;
}
