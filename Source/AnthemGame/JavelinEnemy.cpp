#include "JavelinEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DustEffect.h"
#include "EnemyProjectile.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "JavelinCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PickupOrb.h"
#include "ShotEffect.h"
#include "UObject/ConstructorHelpers.h"

const FName AJavelinEnemy::PaintBody(TEXT("Paint.Body"));
const FName AJavelinEnemy::PaintArmor(TEXT("Paint.Armor"));
const FName AJavelinEnemy::PaintWeakSpot(TEXT("Paint.WeakSpot"));

namespace
{
	// The player's gun deals extra damage to parts with this tag.
	const FName WeakSpotTag(TEXT("WeakSpot"));

	/** How long a toppled body lies there before it sinks away, and how long sinking takes. */
	constexpr float CorpseTime = 2.5f;
	constexpr float SinkTime = 1.5f;
	constexpr float ToppleTime = 0.6f;

	const FLinearColor EnemyDustColor(0.3f, 0.26f, 0.19f);
}

AJavelinEnemy::AJavelinEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// Enemies drive themselves, so they don't need an AI controller.
	AutoPossessAI = EAutoPossessAI::Disabled;

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	Collision->InitCapsuleSize(40.f, 95.f);
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = Collision;

	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Body);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	SphereMesh = SphereFinder.Object;
	ConeMesh = ConeFinder.Object;
}

USceneComponent* AJavelinEnemy::AddPivot(FName Name, USceneComponent* Parent, const FVector& Location)
{
	USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(Name);
	Pivot->SetupAttachment(Parent);
	Pivot->SetRelativeLocation(Location);
	return Pivot;
}

UStaticMeshComponent* AJavelinEnemy::AddPart(FName Name, USceneComponent* Parent, UStaticMesh* PartMesh, const FVector& Location, const FVector& Size,
	FName Paint, const FRotator& Rotation)
{
	// Basic shapes are 100 units across, so Size is in centimeters.
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(PartMesh);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Size / 100.f);

	// Parts stop bullets and bolts, but they're WorldDynamic so ground checks look straight through them,
	// and they don't push the player's camera in.
	Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Part->SetCollisionObjectType(ECC_WorldDynamic);
	Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Part->SetGenerateOverlapEvents(false);
	Part->ComponentTags.Add(Paint);
	if (Paint == PaintWeakSpot)
	{
		Part->ComponentTags.Add(WeakSpotTag);
	}
	return Part;
}

void AJavelinEnemy::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	HomeLocation = GetActorLocation();
	StrafeSign = FMath::RandBool() ? 1.f : -1.f;
	SightCheckTimer = FMath::FRandRange(0.f, 0.25f);

	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (UStaticMeshComponent* Part : Parts)
	{
		FLinearColor Color;
		if (Part->ComponentHasTag(PaintWeakSpot))
		{
			Color = WeakSpotColor;
		}
		else if (Part->ComponentHasTag(PaintArmor))
		{
			Color = ArmorColor;
		}
		else if (Part->ComponentHasTag(PaintBody))
		{
			Color = BodyColor;
		}
		else
		{
			continue;
		}

		UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0);
		if (!Material)
		{
			continue;
		}
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		PartMaterials.Add(Material);

		FJavelinEnemyPaintedPart& Painted = PaintedParts.AddDefaulted_GetRef();
		Painted.Material = Material;
		Painted.Color = Color;
		Painted.bWeakSpot = Part->ComponentHasTag(PaintWeakSpot);
	}
}

void AJavelinEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDying)
	{
		DeathTime += DeltaSeconds;
		UpdateDeath(DeltaSeconds);
		return;
	}
	if (!IsAlive())
	{
		return;
	}

	// Take last frame's flinch off, so the animation below starts from its own pose.
	Body->SetRelativeRotation(BodyRestRotation);

	AJavelinCharacter* Player = Cast<AJavelinCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UpdateSenses(DeltaSeconds, Player);
	const AJavelinCharacter* Target = bEngaged ? Player : nullptr;

	// Switch strafing direction every few seconds so it's harder to track.
	StrafeTimer -= DeltaSeconds;
	if (StrafeTimer <= 0.f)
	{
		StrafeTimer = FMath::FRandRange(1.5f, 3.5f);
		StrafeSign = FMath::RandBool() ? 1.f : -1.f;
	}

	// Get shoved by hits and blasts, sliding along whatever it's pushed into.
	if (!KnockbackVelocity.IsNearlyZero(1.f))
	{
		MoveWithSliding(KnockbackVelocity * DeltaSeconds);
		KnockbackVelocity *= FMath::Exp(-5.f * DeltaSeconds);
	}

	// Staggered, it stumbles instead of walking and can't shoot.
	StaggerRemaining = FMath::Max(0.f, StaggerRemaining - DeltaSeconds);
	const float NormalMoveSpeed = MoveSpeed;
	if (IsStaggered())
	{
		MoveSpeed *= 0.15f;
	}
	UpdateMovement(DeltaSeconds, Target);
	MoveSpeed = NormalMoveSpeed;

	UpdateFacing(DeltaSeconds, Target);
	if (!IsStaggered())
	{
		UpdateAttack(DeltaSeconds, Target);
	}
	UpdateAnimation(DeltaSeconds);
	BodyRestRotation = Body->GetRelativeRotation();
	UpdateHitReactions(DeltaSeconds);
	UpdatePaint(DeltaSeconds);
}

void AJavelinEnemy::UpdateHitReactions(float DeltaSeconds)
{
	RecentDamage = FMath::Max(0.f, RecentDamage - MaxHealth * 0.15f * DeltaSeconds);

	// A stiff spring pulls the body back upright after a hit, overshooting a little like something heavy rocking.
	const float Step = FMath::Min(DeltaSeconds, 0.05f);
	FlinchPitchSpeed += (-160.f * FlinchPitch - 14.f * FlinchPitchSpeed) * Step;
	FlinchRollSpeed += (-160.f * FlinchRoll - 14.f * FlinchRollSpeed) * Step;
	FlinchPitch = FMath::Clamp(FlinchPitch + FlinchPitchSpeed * Step, -35.f, 35.f);
	FlinchRoll = FMath::Clamp(FlinchRoll + FlinchRollSpeed * Step, -35.f, 35.f);

	const FQuat Tip = FRotator(FlinchPitch, 0.f, FlinchRoll).Quaternion();
	Body->SetRelativeRotation((Tip * BodyRestRotation.Quaternion()).Rotator());
}

void AJavelinEnemy::ApplyKnockback(const FVector& Impulse)
{
	FVector Push = Impulse / FMath::Max(Mass, 0.1f);
	if (bDying)
	{
		// A wreck that's already going down gets knocked along as it falls.
		DeathVelocity += Push;
		return;
	}
	if (!bCanBeKnockedUp)
	{
		Push.Z = 0.f;
	}
	KnockbackVelocity = (KnockbackVelocity + Push).GetClampedToMaxSize(2500.f);
}

void AJavelinEnemy::Stagger()
{
	// Reeling: the volley it was lining up is cancelled, and it needs a moment before it can shoot again.
	StaggerRemaining = StaggerTime;
	RecentDamage = 0.f;
	bWindingUp = false;
	ShotsLeftInBurst = 0;
	AttackTimer = FMath::Max(AttackTimer, StaggerTime + 0.4f);
	// Reel further the way it was already rocking.
	FlinchPitchSpeed += FMath::Sign(FlinchPitchSpeed) * 120.f;
	FlinchRollSpeed += FMath::Sign(FlinchRollSpeed) * 60.f;
}

bool AJavelinEnemy::CanSee(const AJavelinCharacter* Player) const
{
	const FVector Eye = Muzzle->GetComponentLocation();
	const FVector PlayerLocation = Player->GetActorLocation();
	if (FVector::Dist(Eye, PlayerLocation) > SightRange || FVector::Dist(HomeLocation, PlayerLocation) > LeashRange)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight), false, this);
	Params.AddIgnoredActor(Player);
	return !GetWorld()->LineTraceTestByChannel(Eye, PlayerLocation, ECC_Visibility, Params);
}

void AJavelinEnemy::UpdateSenses(float DeltaSeconds, AJavelinCharacter* Player)
{
	if (!Player || Player->IsSuitDown())
	{
		bEngaged = false;
		bCanSeePlayer = false;
		return;
	}

	// Line of sight checks a few times a second, not every frame.
	SightCheckTimer -= DeltaSeconds;
	if (SightCheckTimer <= 0.f)
	{
		SightCheckTimer = 0.25f;
		bCanSeePlayer = CanSee(Player);
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (bCanSeePlayer)
	{
		if (!bEngaged)
		{
			Alert();
		}
		LastSawPlayerTime = Now;
	}

	if (bEngaged)
	{
		const bool bLeashed = FVector::Dist(HomeLocation, Player->GetActorLocation()) > LeashRange;
		if (bLeashed || Now - LastSawPlayerTime > ForgetTime)
		{
			bEngaged = false;
		}
	}
}

void AJavelinEnemy::Alert()
{
	const AJavelinCharacter* Player = Cast<AJavelinCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!IsAlive() || !Player || Player->IsSuitDown())
	{
		return;
	}
	if (!bEngaged)
	{
		// A short pause before the first volley, so you have a moment to react.
		AttackTimer = FMath::FRandRange(0.6f, 1.4f);
		bWindingUp = false;
		ShotsLeftInBurst = 0;
	}
	bEngaged = true;
	LastSawPlayerTime = GetWorld()->GetTimeSeconds();
}

void AJavelinEnemy::UpdateFacing(float DeltaSeconds, const AJavelinCharacter* Target)
{
	FVector Direction = Target ? Target->GetActorLocation() - GetActorLocation() : MoveVelocity;
	Direction.Z = 0.f;
	if (Direction.SizeSquared() < 100.f)
	{
		return;
	}
	const FRotator Facing(0.f, Direction.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Facing, DeltaSeconds, 6.f));
}

void AJavelinEnemy::UpdateAttack(float DeltaSeconds, const AJavelinCharacter* Target)
{
	const bool bInRange = Target && bCanSeePlayer && FVector::Dist(Muzzle->GetComponentLocation(), Target->GetActorLocation()) <= AttackRange;
	if (!bInRange)
	{
		// Lost the shot: cancel the volley, and wait a moment after getting it back.
		bWindingUp = false;
		ShotsLeftInBurst = 0;
		AttackTimer = FMath::Max(AttackTimer, 0.5f);
		return;
	}

	AttackTimer -= DeltaSeconds;
	if (AttackTimer > 0.f)
	{
		return;
	}

	if (ShotsLeftInBurst > 0)
	{
		FireProjectile(Target);
		--ShotsLeftInBurst;
		AttackTimer = ShotsLeftInBurst > 0 ? TimeBetweenBurstShots : TimeBetweenBursts * FMath::FRandRange(0.8f, 1.3f);
	}
	else if (!bWindingUp)
	{
		// Flare the weak spot as a warning, then fire.
		bWindingUp = true;
		AttackTimer = WindUpTime;
	}
	else
	{
		bWindingUp = false;
		ShotsLeftInBurst = BurstCount;
		AttackTimer = 0.f;
	}
}

void AJavelinEnemy::FireProjectile(const AJavelinCharacter* Target)
{
	UWorld* World = GetWorld();
	const FVector From = Muzzle->GetComponentLocation();

	// Lead a moving target by how long the bolt takes to get there.
	FVector AimPoint = Target->GetActorLocation();
	const float FlightTime = FVector::Dist(From, AimPoint) / ProjectileSpeed;
	AimPoint += Target->GetVelocity() * FlightTime * AimLead;
	const FVector Direction = FMath::VRandCone((AimPoint - From).GetSafeNormal(), FMath::DegreesToRadians(AimSpread * 0.5f));

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	if (AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(From, Direction.Rotation(), SpawnParams))
	{
		Projectile->Launch(Direction * ProjectileSpeed, ProjectileDamage, ProjectileColor, ProjectileSize);
	}
	if (AShotEffect* Flash = World->SpawnActor<AShotEffect>(From, FRotator::ZeroRotator))
	{
		Flash->InitBurst(From, ProjectileColor, 0.4f * ProjectileSize);
	}
}

void AJavelinEnemy::UpdatePaint(float DeltaSeconds)
{
	HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds * 8.f);

	// Weak spots pulse bright while winding up a volley.
	const float WindUpGlow = bWindingUp ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 30.f) : 0.f;
	for (const FJavelinEnemyPaintedPart& Part : PaintedParts)
	{
		FLinearColor Color = Part.Color;
		if (Part.bWeakSpot)
		{
			Color = FMath::Lerp(Color, FLinearColor(1.f, 1.f, 0.6f), WindUpGlow);
		}
		Part.Material->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Color, FLinearColor::White, HitFlash));
	}
}

float AJavelinEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsAlive() || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - DamageAmount);
	HitFlash = 1.f;
	LastHitTime = GetWorld()->GetTimeSeconds();

	// Which way the hit was going, and whether it struck the weak spot.
	FVector HitDirection = DamageCauser ? (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal() : -GetActorForwardVector();
	bool bWeakSpotHit = false;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitDirection = PointDamage.ShotDirection.GetSafeNormal();
		const UPrimitiveComponent* HitPart = PointDamage.HitInfo.GetComponent();
		bWeakSpotHit = HitPart && HitPart->ComponentHasTag(WeakSpotTag);
	}

	if (!IsAlive())
	{
		DeathDirection = HitDirection;
		Die();
		return DamageAmount;
	}

	// Flinch away from the hit: shot from the front it rocks back, from the side it tips over sideways.
	const FVector LocalHit = GetActorRotation().UnrotateVector(HitDirection);
	const float Kick = FMath::Min(DamageAmount, 60.f) * FlinchPerDamage / FMath::Max(Mass, 0.1f);
	FlinchPitchSpeed += -static_cast<float>(LocalHit.X) * Kick;
	FlinchRollSpeed += static_cast<float>(LocalHit.Y) * Kick;

	// Too much damage at once, or shooting the weak spot while it flares, staggers it and cancels its volley.
	RecentDamage += DamageAmount;
	if (RecentDamage >= StaggerDamageFraction * MaxHealth || (bWeakSpotHit && bWindingUp))
	{
		Stagger();
	}

	// Getting shot gives your position away, to this enemy and to its friends nearby.
	Alert();
	for (TActorIterator<AJavelinEnemy> It(GetWorld()); It; ++It)
	{
		if (*It != this && FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= AlertAlliesRange)
		{
			It->Alert();
		}
	}
	return DamageAmount;
}

void AJavelinEnemy::Die()
{
	UWorld* World = GetWorld();
	const FVector Center = Body->GetComponentLocation();
	if (AShotEffect* Effect = World->SpawnActor<AShotEffect>(Center, FRotator::ZeroRotator))
	{
		Effect->InitBurst(Center, WeakSpotColor, 1.5f);
	}

	// The body stays and plays out its death (see UpdateDeath), but nothing can hit it or bump into it any more.
	bDying = true;
	DeathTime = 0.f;
	DeathVelocity = MoveVelocity * 0.5f + KnockbackVelocity + DeathDirection * 250.f;
	BodyRestRotation = Body->GetRelativeRotation();
	SetActorEnableCollision(false);
	SetLifeSpan(10.f);

	// Its power dies: the glow goes out of the weak spots.
	for (const FJavelinEnemyPaintedPart& Part : PaintedParts)
	{
		Part.Material->SetVectorParameterValue(TEXT("Color"), Part.bWeakSpot ? Part.Color * 0.08f : Part.Color * 0.6f);
	}

	auto Drop = [this, World, &Center](EPickupOrbType Type)
	{
		const FTransform SpawnTransform(FRotator::ZeroRotator, Center);
		if (APickupOrb* Orb = World->SpawnActorDeferred<APickupOrb>(APickupOrb::StaticClass(), SpawnTransform))
		{
			Orb->Type = Type;
			Orb->FinishSpawning(SpawnTransform);
		}
	};
	if (FMath::FRand() < HealthDropChance)
	{
		Drop(EPickupOrbType::Health);
	}
	if (FMath::FRand() < AmmoDropChance)
	{
		Drop(EPickupOrbType::Ammo);
	}
}

void AJavelinEnemy::UpdateDeath(float DeltaSeconds)
{
	// Topple over the way the killing blow pushed, pivoting on the feet, falling faster as it goes like a dead weight.
	const float HalfHeight = Collision->GetScaledCapsuleHalfHeight();
	FVector LocalDirection = GetActorRotation().UnrotateVector(DeathDirection);
	LocalDirection.Z = 0.f;
	LocalDirection = LocalDirection.IsNearlyZero() ? FVector(-1.f, 0.f, 0.f) : LocalDirection.GetSafeNormal();
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, LocalDirection).GetSafeNormal();

	const float Alpha = FMath::Clamp(DeathTime / ToppleTime, 0.f, 1.f);
	const float Eased = Alpha * Alpha;
	const FQuat Fall(Axis, FMath::DegreesToRadians(85.f * Eased));
	const FVector Feet(0.f, 0.f, -HalfHeight);
	FVector Offset = Feet - Fall.RotateVector(Feet) + FVector(0.f, 0.f, 22.f * Eased);

	// It hits the ground with a thud of dust.
	if (Alpha >= 1.f && !bSankDust)
	{
		bSankDust = true;
		const FVector WorldDirection = GetActorRotation().RotateVector(LocalDirection);
		ADustEffect::Spawn(GetWorld(), GetActorLocation() + WorldDirection * HalfHeight - FVector(0.f, 0.f, HalfHeight), 110.f, 0.7f, 6, EnemyDustColor, 0.9f);
	}

	// After lying there a while it sinks into the ground and is gone.
	if (DeathTime > CorpseTime)
	{
		Offset.Z -= 80.f * (DeathTime - CorpseTime) / SinkTime;
		if (DeathTime > CorpseTime + SinkTime)
		{
			Destroy();
			return;
		}
	}

	Body->SetRelativeLocationAndRotation(Offset, Fall * BodyRestRotation.Quaternion());
}

bool AJavelinEnemy::MoveWithSliding(const FVector& Delta)
{
	if (Delta.IsNearlyZero())
	{
		return false;
	}

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (!Hit.IsValidBlockingHit())
	{
		return false;
	}

	if (Hit.bStartPenetrating)
	{
		// Stuck inside something (another enemy spawned on top of it, say): push straight out.
		AddActorWorldOffset(Hit.Normal * (Hit.PenetrationDepth + 1.f), false);
		return true;
	}

	// Slide along whatever it bumped into with the rest of the move.
	const FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
	AddActorWorldOffset(Slide, true);
	return true;
}

bool AJavelinEnemy::FindGround(const FVector& Location, float MaxDrop, FVector& OutGround) const
{
	// Only world geometry counts as ground, not the player or other enemies.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyGround), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0.f, 0.f, MaxDrop), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}

FText AJavelinEnemy::GetEnemyName() const
{
	return NSLOCTEXT("Javelin", "Enemy", "HOSTILE");
}

FVector AJavelinEnemy::GetHealthBarLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, HealthBarHeight);
}

float AJavelinEnemy::GetTimeSinceHit() const
{
	return GetWorld()->GetTimeSeconds() - LastHitTime;
}
