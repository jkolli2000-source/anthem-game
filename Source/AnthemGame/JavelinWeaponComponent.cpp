#include "JavelinWeaponComponent.h"
#include "Engine/World.h"
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

	/** Whether a hit actor counts as a living target, so a hit that drops it to zero shows as a kill. */
	bool IsLivingTarget(const AActor* Actor)
	{
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

	// Start level with the suit so nothing between the camera and the suit gets hit.
	const float SkipDistance = FMath::Max(0.f, FVector::DotProduct(Pawn->GetActorLocation() - ViewLocation, ShotDirection));
	const FVector TraceStart = ViewLocation + ShotDirection * SkipDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinWeapon), false, Pawn);
	FHitResult Hit;
	FVector AimPoint = TraceStart + ShotDirection * Range;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, AimPoint, ECC_Visibility, Params))
	{
		AimPoint = Hit.ImpactPoint;
	}

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
		const bool bWeakSpot = MuzzleHit.GetComponent() && MuzzleHit.GetComponent()->ComponentHasTag(WeakSpotTag);
		const float ShotDamage = Damage * (bWeakSpot ? WeakSpotMultiplier : 1.f);

		const bool bWasAlive = IsLivingTarget(HitActor);
		const float Dealt = UGameplayStatics::ApplyPointDamage(HitActor, ShotDamage, ShotDirection, MuzzleHit, Controller, Pawn, UDamageType::StaticClass());
		if (Dealt > 0.f)
		{
			LastHitTime = LastShotTime;
			bLastHitWeakSpot = bWeakSpot;
			bLastHitKill = bWasAlive && !IsLivingTarget(HitActor);

			FJavelinDamageNumber& Number = DamageNumbers.AddDefaulted_GetRef();
			Number.Location = MuzzleHit.ImpactPoint;
			Number.Amount = Dealt;
			Number.bWeakSpot = bWeakSpot;
			Number.SpawnTime = LastShotTime;
		}
	}

	Bloom = FMath::Min(Bloom + SpreadPerShot, MaxBloom);

	// Kick the view up a little. The player pulls it back down.
	const float RecoilScale = IsAiming() ? 0.5f : 1.f;
	Controller->SetControlRotation(Controller->GetControlRotation()
		+ FRotator(RecoilPitch * RecoilScale, FMath::FRandRange(-RecoilYaw, RecoilYaw) * RecoilScale, 0.f));

	// Anthem-style auto reload when the magazine runs dry.
	if (Ammo == 0)
	{
		StartReload();
	}
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
	return (IsAiming() ? AimSpread : HipSpread) + Bloom;
}

float UJavelinWeaponComponent::GetTimeSinceLastShot() const
{
	return GetWorld()->GetTimeSeconds() - LastShotTime;
}

float UJavelinWeaponComponent::GetTimeSinceLastHit() const
{
	return GetWorld()->GetTimeSeconds() - LastHitTime;
}
