#include "JavelinCharacter.h"
#include "DustEffect.h"
#include "JavelinAbilityComponent.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"
#include "JavelinSuitHealthComponent.h"
#include "JavelinWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShotEffect.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FJavelinCameraProfile MakeCameraProfile(float ArmLength, const FVector& SocketOffset, float FieldOfView, float LagSpeed)
	{
		FJavelinCameraProfile Profile;
		Profile.ArmLength = ArmLength;
		Profile.SocketOffset = SocketOffset;
		Profile.FieldOfView = FieldOfView;
		Profile.LagSpeed = LagSpeed;
		return Profile;
	}

	// Component tags that say how BeginPlay paints each suit part.
	const FName PaintPrimary(TEXT("Paint.Primary"));
	const FName PaintSecondary(TEXT("Paint.Secondary"));
	const FName PaintTrim(TEXT("Paint.Trim"));
	const FName PaintFrame(TEXT("Paint.Frame"));
	const FName PaintAccent(TEXT("Paint.Accent"));
	const FName PaintGlow(TEXT("Paint.Glow"));
	const FName PaintMuzzle(TEXT("Paint.Muzzle"));

	/** How long the suit keeps facing the aim direction after the last shot. */
	constexpr float CombatStanceTime = 1.f;
	constexpr float MuzzleFlashTime = 0.05f;

	/** How long the left arm takes to follow through after throwing a grenade. */
	constexpr float GrenadeThrowPoseTime = 0.35f;

	/** Ground speed where footsteps start to kick up dust and rumble harder. */
	constexpr float HeavyStepSpeed = 800.f;

	// Leg measurements, in the suit's local space. Hips hang off VisualRoot; the capsule's feet are at Z = -96.
	constexpr float HipWidth = 19.f;
	constexpr float HipHeight = -14.f;
	constexpr float ThighLength = 40.f;
	constexpr float ShinLength = 34.f;
	/** Ankle pivot height above the sole. */
	constexpr float AnkleHeight = 8.f;
	/** Feet rest a little wider than the hips, toes turned out a touch. */
	constexpr float FootRestWidth = 23.f;
	constexpr float FootRestForward = 2.f;
	constexpr float FootToeOut = 6.f;
	/** Planted feet closer than this to their rest spot don't need a settling step. */
	constexpr float SettleDistance = 9.f;
	constexpr float SettleYaw = 14.f;

	/**
	 * Two-bone leg IK. Delta goes from the hip to where the ankle should be, in the hip's parent space.
	 * The hip rolls the leg sideways toward the foot, then pitches it forward or back; the knee only bends backward.
	 */
	void SolveLeg(const FVector& Delta, FQuat& OutHip, FQuat& OutKnee)
	{
		const float Y = static_cast<float>(Delta.Y);
		const float Down = FMath::Max(static_cast<float>(-Delta.Z), 1.f);
		// Negative roll swings a hanging limb toward +Y.
		const float Roll = FMath::RadiansToDegrees(FMath::Atan2(-Y, Down));
		const float InPlaneDown = FMath::Sqrt(Y * Y + Down * Down);
		const float Forward = static_cast<float>(Delta.X);

		const float Reach = FMath::Clamp(FMath::Sqrt(Forward * Forward + InPlaneDown * InPlaneDown),
			FMath::Abs(ThighLength - ShinLength) + 2.f, ThighLength + ShinLength - 0.1f);
		const float ToFoot = FMath::Atan2(Forward, InPlaneDown);
		const float HipAngle = FMath::Acos(FMath::Clamp(
			(ThighLength * ThighLength + Reach * Reach - ShinLength * ShinLength) / (2.f * ThighLength * Reach), -1.f, 1.f));
		const float KneeAngle = FMath::Acos(FMath::Clamp(
			(ThighLength * ThighLength + ShinLength * ShinLength - Reach * Reach) / (2.f * ThighLength * ShinLength), -1.f, 1.f));

		// Positive pitch swings a hanging limb forward. The thigh points ahead of the foot so the knee bends forward.
		const float HipPitch = FMath::RadiansToDegrees(ToFoot + HipAngle);
		const float KneeBend = -FMath::RadiansToDegrees(UE_PI - KneeAngle);
		OutHip = FRotator(0.f, 0.f, Roll).Quaternion() * FRotator(HipPitch, 0.f, 0.f).Quaternion();
		OutKnee = FRotator(KneeBend, 0.f, 0.f).Quaternion();
	}

	/** True if a phase running from Old forward by Advance (less than a full cycle) passed Mark. */
	bool PhaseCrossed(float Old, float Advance, float Mark)
	{
		const float Ahead = FMath::Frac(Mark - Old);
		return Advance > 0.f && Ahead < Advance;
	}

	const FLinearColor ThrusterIdleColor(0.2f, 0.2f, 0.22f);
	const FLinearColor ThrusterCoolColor(0.1f, 0.7f, 1.f);
	const FLinearColor ThrusterHotColor(1.f, 0.15f, 0.05f);
}

AJavelinCharacter::AJavelinCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UJavelinMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// The movement component turns the suit; the camera turns freely.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Anthem-style framing: on foot the camera sits over the right shoulder, so the suit stands left of center and
	// the crosshair has a clear view past it. Aiming pulls in tight over the shoulder. Flight drops the shoulder
	// offset and pulls far back so the suit is centered and you can see where you're going.
	GroundCamera = MakeCameraProfile(300.f, FVector(0.f, 100.f, 30.f), 80.f, 15.f);
	SprintCamera = MakeCameraProfile(340.f, FVector(0.f, 85.f, 30.f), 90.f, 12.f);
	HoverCamera = MakeCameraProfile(320.f, FVector(0.f, 100.f, 35.f), 82.f, 15.f);
	FlightCamera = MakeCameraProfile(850.f, FVector(0.f, 0.f, 90.f), 100.f, 6.f);
	AimCamera = MakeCameraProfile(170.f, FVector(0.f, 85.f, 20.f), 58.f, 30.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraPivotHeight));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagMaxDistance = 600.f;
	CameraBoom->TargetArmLength = GroundCamera.ArmLength;
	CameraBoom->SocketOffset = GroundCamera.SocketOffset;
	CameraBoom->CameraLagSpeed = GroundCamera.LagSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = GroundCamera.FieldOfView;

	Heat = CreateDefaultSubobject<UJavelinHeatComponent>(TEXT("Heat"));
	Weapon = CreateDefaultSubobject<UJavelinWeaponComponent>(TEXT("Weapon"));
	SuitHealth = CreateDefaultSubobject<UJavelinSuitHealthComponent>(TEXT("SuitHealth"));
	Abilities = CreateDefaultSubobject<UJavelinAbilityComponent>(TEXT("Abilities"));

	BuildSuit();
}

USceneComponent* AJavelinCharacter::CreatePivot(FName Name, USceneComponent* Parent, const FVector& Location)
{
	USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(Name);
	Pivot->SetupAttachment(Parent);
	Pivot->SetRelativeLocation(Location);
	return Pivot;
}

UStaticMeshComponent* AJavelinCharacter::CreatePart(FName Name, USceneComponent* Parent, UStaticMesh* PartMesh, const FVector& Location, const FVector& Scale, FName Paint, const FRotator& Rotation)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(PartMesh);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->ComponentTags.Add(Paint);

	// Remembered for the rest of construction: every part gets shown or hidden for the starting suit, and the
	// big painted boxes get camo patches on the Marine suits.
	ConstructionParts.Add(Part);
	if ((Paint == PaintPrimary || Paint == PaintSecondary) && PartMesh && PartMesh->GetFName() == TEXT("Cube"))
	{
		CamoCandidates.Add(Part);
	}
	return Part;
}

USceneComponent* AJavelinCharacter::CreateFlame(FName Name, USceneComponent* Parent, UStaticMesh* ConeMesh, const FVector& Location, float Width)
{
	// The pivot sits at the nozzle; scaling its Z stretches the flame away from the nozzle.
	USceneComponent* Pivot = CreatePivot(Name, Parent, Location);
	UStaticMeshComponent* Cone = CreatePart(FName(*(Name.ToString() + TEXT("Cone"))), Pivot, ConeMesh,
		FVector(0.f, 0.f, -50.f), FVector(Width, Width, 1.f), PaintGlow, FRotator(180.f, 0.f, 0.f));
	Cone->SetCastShadow(false);
	Pivot->SetVisibility(false, true);
	return Pivot;
}

void AJavelinCharacter::BuildSuit()
{
	// A Ranger-style exosuit built from the engine's basic shapes, so the project needs no art assets yet.
	// Basic shapes are 100 units across and centered on their pivot. The capsule's feet are at Z = -96.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Cube = CubeFinder.Object;
	UStaticMesh* Cylinder = CylinderFinder.Object;
	UStaticMesh* Sphere = SphereFinder.Object;
	UStaticMesh* Cone = ConeFinder.Object;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	// Pelvis
	CreatePart(TEXT("Pelvis"), VisualRoot, Cube, FVector(0.f, 0.f, -10.f), FVector(0.34f, 0.5f, 0.22f), PaintFrame);
	CreatePart(TEXT("Codpiece"), VisualRoot, Cube, FVector(14.f, 0.f, -14.f), FVector(0.1f, 0.24f, 0.2f), PaintPrimary);

	// Torso, leaning around the waist
	TorsoPivot = CreatePivot(TEXT("TorsoPivot"), VisualRoot, FVector(0.f, 0.f, 2.f));
	CreatePart(TEXT("Abdomen"), TorsoPivot, Cube, FVector(0.f, 0.f, 10.f), FVector(0.32f, 0.44f, 0.26f), PaintFrame);
	CreatePart(TEXT("Chest"), TorsoPivot, Cube, FVector(2.f, 0.f, 42.f), FVector(0.5f, 0.78f, 0.46f), PaintPrimary);
	CreatePart(TEXT("ChestPlate"), TorsoPivot, Cube, FVector(24.f, 0.f, 44.f), FVector(0.08f, 0.62f, 0.32f), PaintSecondary, FRotator(-8.f, 0.f, 0.f));
	// The Marine suits wear their insignia on the chest instead of the Ranger's glowing core.
	CreatePart(TEXT("ChestCore"), TorsoPivot, Cylinder, FVector(29.f, 0.f, 46.f), FVector(0.14f, 0.14f, 0.04f), PaintAccent, FRotator(90.f, 0.f, 0.f))
		->ComponentTags.Add(JavelinSuitTags::Ranger);
	CreatePart(TEXT("Collar"), TorsoPivot, Cube, FVector(-2.f, 0.f, 66.f), FVector(0.42f, 0.5f, 0.08f), PaintTrim);

	// Jet pack with two nozzles pointing down the back
	CreatePart(TEXT("JetPack"), TorsoPivot, Cube, FVector(-30.f, 0.f, 44.f), FVector(0.24f, 0.62f, 0.5f), PaintSecondary);
	CreatePart(TEXT("JetPackSpine"), TorsoPivot, Cube, FVector(-43.f, 0.f, 46.f), FVector(0.06f, 0.14f, 0.42f), PaintTrim);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Y = Side == 0 ? -20.f : 20.f;
		const FString Prefix = Side == 0 ? TEXT("Left") : TEXT("Right");
		CreatePart(*(Prefix + TEXT("Nozzle")), TorsoPivot, Cylinder, FVector(-36.f, Y, 16.f), FVector(0.17f, 0.17f, 0.18f), PaintFrame);
		CreatePart(*(Prefix + TEXT("NozzleGlow")), TorsoPivot, Cylinder, FVector(-36.f, Y, 6.5f), FVector(0.13f, 0.13f, 0.02f), PaintGlow);
		BackFlames.Add(CreateFlame(*(Prefix + TEXT("BackFlame")), TorsoPivot, Cone, FVector(-36.f, Y, 6.f), 0.13f));
	}
	BuildMissilePod(TorsoPivot, Cube, Cylinder);

	// Helmet
	HeadPivot = CreatePivot(TEXT("HeadPivot"), TorsoPivot, FVector(4.f, 0.f, 70.f));
	CreatePart(TEXT("Neck"), HeadPivot, Cylinder, FVector(0.f, 0.f, 2.f), FVector(0.18f, 0.18f, 0.1f), PaintFrame);
	// The round helmet and faceplate are shared by the Ranger and EXO-07; SEMPER-03 wears a boxy one instead.
	UStaticMeshComponent* Helmet = CreatePart(TEXT("Helmet"), HeadPivot, Sphere, FVector(0.f, 0.f, 16.f), FVector(0.34f, 0.3f, 0.36f), PaintPrimary);
	Helmet->ComponentTags.Add(JavelinSuitTags::Ranger);
	Helmet->ComponentTags.Add(JavelinSuitTags::Exo07);
	CreatePart(TEXT("HelmetCrest"), HeadPivot, Cube, FVector(-2.f, 0.f, 32.f), FVector(0.3f, 0.06f, 0.06f), PaintTrim)
		->ComponentTags.Add(JavelinSuitTags::Ranger);
	UStaticMeshComponent* Faceplate = CreatePart(TEXT("Faceplate"), HeadPivot, Cube, FVector(13.f, 0.f, 12.f), FVector(0.1f, 0.24f, 0.16f), PaintSecondary);
	Faceplate->ComponentTags.Add(JavelinSuitTags::Ranger);
	Faceplate->ComponentTags.Add(JavelinSuitTags::Exo07);
	CreatePart(TEXT("Visor"), HeadPivot, Cube, FVector(16.f, 0.f, 19.f), FVector(0.06f, 0.24f, 0.06f), JavelinPaintTags::Visor)
		->ComponentTags.Add(JavelinSuitTags::Ranger);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? -1.f : 1.f;
		const FString Prefix = Side == 0 ? TEXT("Left") : TEXT("Right");
		auto PartName = [&Prefix](const TCHAR* Part) { return FName(*(Prefix + Part)); };

		// Arm: shoulder > elbow > hand
		USceneComponent* Shoulder = CreatePivot(PartName(TEXT("Shoulder")), TorsoPivot, FVector(0.f, 46.f * S, 56.f));
		CreatePart(PartName(TEXT("Pauldron")), Shoulder, Sphere, FVector(0.f, 6.f * S, 6.f), FVector(0.42f, 0.36f, 0.32f), PaintPrimary);
		CreatePart(PartName(TEXT("PauldronTrim")), Shoulder, Cube, FVector(0.f, 12.f * S, 15.f), FVector(0.32f, 0.16f, 0.05f), PaintTrim);
		CreatePart(PartName(TEXT("UpperArm")), Shoulder, Cylinder, FVector(0.f, 0.f, -22.f), FVector(0.17f, 0.17f, 0.36f), PaintFrame);

		USceneComponent* Elbow = CreatePivot(PartName(TEXT("Elbow")), Shoulder, FVector(0.f, 0.f, -40.f));
		CreatePart(PartName(TEXT("Forearm")), Elbow, Cube, FVector(2.f, 0.f, -17.f), FVector(0.24f, 0.22f, 0.34f), PaintSecondary);
		CreatePart(PartName(TEXT("Gauntlet")), Elbow, Cube, FVector(3.f, 9.f * S, -14.f), FVector(0.2f, 0.06f, 0.22f), PaintPrimary);
		CreatePart(PartName(TEXT("WristGlow")), Elbow, Cylinder, FVector(2.f, 0.f, -35.f), FVector(0.19f, 0.19f, 0.03f), PaintGlow);
		CreatePart(PartName(TEXT("Hand")), Elbow, Cube, FVector(3.f, 0.f, -43.f), FVector(0.15f, 0.12f, 0.14f), PaintFrame);
		if (Side == 1)
		{
			BuildGun(Elbow, Cube, Cylinder, Sphere);
		}

		// Leg: hip > knee > foot
		USceneComponent* Hip = CreatePivot(PartName(TEXT("Hip")), VisualRoot, FVector(0.f, HipWidth * S, HipHeight));
		CreatePart(PartName(TEXT("Thigh")), Hip, Cube, FVector(0.f, 0.f, -20.f), FVector(0.26f, 0.24f, 0.4f), PaintPrimary);
		CreatePart(PartName(TEXT("ThighPlate")), Hip, Cube, FVector(0.f, 13.f * S, -16.f), FVector(0.2f, 0.04f, 0.28f), PaintTrim);

		USceneComponent* Knee = CreatePivot(PartName(TEXT("Knee")), Hip, FVector(0.f, 0.f, -ThighLength));
		CreatePart(PartName(TEXT("KneePad")), Knee, Sphere, FVector(12.f, 0.f, 1.f), FVector(0.17f, 0.18f, 0.18f), PaintSecondary);
		CreatePart(PartName(TEXT("Shin")), Knee, Cube, FVector(0.f, 0.f, -19.f), FVector(0.24f, 0.22f, 0.36f), PaintSecondary);
		CreatePart(PartName(TEXT("CalfJet")), Knee, Cylinder, FVector(-13.f, 0.f, -22.f), FVector(0.12f, 0.12f, 0.2f), PaintFrame);
		// The ankle lets the foot sit flat on the ground while the leg swings over it. The sole is AnkleHeight below it.
		USceneComponent* Ankle = CreatePivot(PartName(TEXT("Ankle")), Knee, FVector(0.f, 0.f, -ShinLength));
		CreatePart(PartName(TEXT("Foot")), Ankle, Cube, FVector(7.f, 0.f, -3.f), FVector(0.34f, 0.2f, 0.1f), PaintFrame);
		CreatePart(PartName(TEXT("SoleGlow")), Ankle, Cylinder, FVector(-2.f, 0.f, -8.f), FVector(0.12f, 0.12f, 0.01f), PaintGlow);
		FootFlames.Add(CreateFlame(PartName(TEXT("FootFlame")), Ankle, Cone, FVector(-2.f, 0.f, -8.5f), 0.11f));

		(Side == 0 ? LeftShoulder : RightShoulder) = Shoulder;
		(Side == 0 ? LeftElbow : RightElbow) = Elbow;
		(Side == 0 ? LeftHip : RightHip) = Hip;
		(Side == 0 ? LeftKnee : RightKnee) = Knee;
		(Side == 0 ? LeftAnkle : RightAnkle) = Ankle;
	}

	// The two Marine suits' armor and gear go on the same frame, and only the current suit's parts are shown.
	BuildMarineSuits(Cube, Cylinder, Sphere, Cone);
	BuildCamo(Cube);
	UpdateSuitVisibility(ConstructionParts);
	ConstructionParts.Empty();
	CamoCandidates.Empty();
}

void AJavelinCharacter::BuildGun(USceneComponent* Hand, UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere)
{
	// An assault rifle gripped in the right hand, its barrel running on down the line of the forearm.
	// In the elbow's space -Z runs down the forearm and +X is the top of the gun when the arm is raised to aim.
	// Carried by the Ranger and SEMPER-03. EXO-07 carries the HMG-88 instead (BuildHeavyMachineGun).
	UStaticMeshComponent* const RifleParts[] = {
		CreatePart(TEXT("GunReceiver"), Hand, Cube, FVector(3.f, 0.f, -58.f), FVector(0.16f, 0.09f, 0.4f), PaintFrame),
		CreatePart(TEXT("GunTrim"), Hand, Cube, FVector(3.f, 4.8f, -58.f), FVector(0.1f, 0.01f, 0.3f), PaintTrim),
		CreatePart(TEXT("GunMagazine"), Hand, Cube, FVector(-6.f, 0.f, -64.f), FVector(0.1f, 0.06f, 0.14f), PaintSecondary, FRotator(-10.f, 0.f, 0.f)),
		CreatePart(TEXT("GunSight"), Hand, Cube, FVector(12.f, 0.f, -52.f), FVector(0.04f, 0.04f, 0.1f), PaintAccent),
		CreatePart(TEXT("GunShroud"), Hand, Cylinder, FVector(6.f, 0.f, -84.f), FVector(0.08f, 0.08f, 0.12f), PaintSecondary),
		CreatePart(TEXT("GunBarrel"), Hand, Cylinder, FVector(6.f, 0.f, -95.f), FVector(0.05f, 0.05f, 0.24f), PaintFrame),
	};
	for (UStaticMeshComponent* Part : RifleParts)
	{
		Part->ComponentTags.Add(JavelinSuitTags::Ranger);
		Part->ComponentTags.Add(JavelinSuitTags::Semper03);
	}

	Muzzle = CreatePivot(TEXT("Muzzle"), Hand, FVector(6.f, 0.f, -108.f));
	RifleMuzzle = Muzzle;

	MuzzleFlash = CreatePart(TEXT("MuzzleFlash"), Muzzle, Sphere, FVector(0.f, 0.f, -8.f), FVector(0.14f, 0.14f, 0.24f), PaintMuzzle);
	MuzzleFlash->SetCastShadow(false);
	MuzzleFlash->SetVisibility(false);
	RifleMuzzleFlash = MuzzleFlash;

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Muzzle);
	MuzzleLight->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
	MuzzleLight->SetMobility(EComponentMobility::Movable);
	MuzzleLight->IntensityUnits = ELightUnits::Candelas;
	MuzzleLight->Intensity = 600.f;
	MuzzleLight->AttenuationRadius = 600.f;
	MuzzleLight->LightColor = MuzzleFlashColor.ToFColor(true);
	MuzzleLight->CastShadows = false;
	MuzzleLight->SetVisibility(false);
}

void AJavelinCharacter::BuildMissilePod(USceneComponent* Torso, UStaticMesh* Cube, UStaticMesh* Cylinder)
{
	// A Ranger-style launcher box riding above the left pauldron, its four tubes facing forward.
	// It sits on the left so it doesn't block the over-the-right-shoulder camera.
	CreatePart(TEXT("MissilePodMount"), Torso, Cube, FVector(-8.f, -40.f, 79.f), FVector(0.12f, 0.08f, 0.06f), PaintFrame);
	CreatePart(TEXT("MissilePod"), Torso, Cube, FVector(-6.f, -40.f, 91.f), FVector(0.36f, 0.2f, 0.2f), PaintSecondary);
	CreatePart(TEXT("MissilePodStripe"), Torso, Cube, FVector(-6.f, -40.f, 101.5f), FVector(0.3f, 0.12f, 0.01f), PaintTrim);
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Column = 0; Column < 2; ++Column)
		{
			const FVector Location(12.5f, -40.f + (Column == 0 ? -4.5f : 4.5f), 91.f + (Row == 0 ? -4.5f : 4.5f));
			CreatePart(*FString::Printf(TEXT("MissileTube%d"), Row * 2 + Column), Torso, Cylinder, Location,
				FVector(0.06f, 0.06f, 0.02f), PaintFrame, FRotator(90.f, 0.f, 0.f));
		}
	}
	MissileLauncher = CreatePivot(TEXT("MissileLauncher"), Torso, FVector(18.f, -40.f, 93.f));
}

UJavelinMovementComponent* AJavelinCharacter::GetJavelinMovement() const
{
	return CastChecked<UJavelinMovementComponent>(GetCharacterMovement());
}

void AJavelinCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Picks up a pivot height changed in the editor.
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraPivotHeight));

	// Remember the Ranger's numbers as set on the components, then dress the suit and set up its gun, stats and visor.
	CaptureRangerStats();
	ApplySuitClass();
	// Show the suit's name banner as the game starts.
	SuitChangedTime = GetWorld()->GetTimeSeconds();

	Heat->OnOverheated.AddDynamic(this, &AJavelinCharacter::HandleOverheated);
	SuitHealth->OnSuitDown.AddDynamic(this, &AJavelinCharacter::HandleSuitDown);
	StartLocation = GetActorLocation();
	StartRotation = GetActorRotation();
	Abilities->SetLaunchPoints(LeftShoulder, MissileLauncher);
	LastYaw = GetActorRotation().Yaw;
	LastVelocity = GetVelocity();

	GetJavelinMovement()->OnLanded.AddUObject(this, &AJavelinCharacter::HandleLanded);
	GetJavelinMovement()->OnCrashed.AddUObject(this, &AJavelinCharacter::HandleCrashed);
	Weapon->OnFired.AddUObject(this, &AJavelinCharacter::HandleWeaponFired);
}

void AJavelinCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	CreateDefaultInput();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AJavelinCharacter::CreateDefaultInput()
{
	if (DefaultMappingContext)
	{
		return;
	}

	// Built in code so the prototype runs without any input assets. Swap in assets later via the Input properties.
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	ThrustAction = MakeAction(TEXT("IA_Thrust"), EInputActionValueType::Boolean);
	ThrustToggleAction = MakeAction(TEXT("IA_ThrustToggle"), EInputActionValueType::Boolean);
	DodgeAction = MakeAction(TEXT("IA_Dodge"), EInputActionValueType::Boolean);
	LaunchAction = MakeAction(TEXT("IA_Launch"), EInputActionValueType::Boolean);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimAction = MakeAction(TEXT("IA_Aim"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	GrenadeAction = MakeAction(TEXT("IA_Grenade"), EInputActionValueType::Boolean);
	MissileAction = MakeAction(TEXT("IA_Missiles"), EInputActionValueType::Boolean);
	SuitRangerAction = MakeAction(TEXT("IA_SuitRanger"), EInputActionValueType::Boolean);
	SuitExoAction = MakeAction(TEXT("IA_SuitExo07"), EInputActionValueType::Boolean);
	SuitSemperAction = MakeAction(TEXT("IA_SuitSemper03"), EInputActionValueType::Boolean);
	SuitNextAction = MakeAction(TEXT("IA_SuitNext"), EInputActionValueType::Boolean);
	SuitPreviousAction = MakeAction(TEXT("IA_SuitPrevious"), EInputActionValueType::Boolean);
	VisorAction = MakeAction(TEXT("IA_Visor"), EInputActionValueType::Boolean);
	ShieldAction = MakeAction(TEXT("IA_Shield"), EInputActionValueType::Boolean);
	ShieldToggleAction = MakeAction(TEXT("IA_ShieldToggle"), EInputActionValueType::Boolean);

	UInputMappingContext* Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Javelin"));

	auto Map = [Context](UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegateX = false, bool bNegateY = false, bool bDeadZone = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bDeadZone)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
		}
		if (bSwizzle)
		{
			// Turns a 1D key press into the Y (forward) axis.
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
		}
		if (bNegateX || bNegateY)
		{
			UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Context);
			Negate->bX = bNegateX;
			Negate->bY = bNegateY;
			Negate->bZ = false;
			Mapping.Modifiers.Add(Negate);
		}
	};

	Map(MoveAction, EKeys::W, true);
	Map(MoveAction, EKeys::S, true, false, true);
	Map(MoveAction, EKeys::D);
	Map(MoveAction, EKeys::A, false, true);
	Map(MoveAction, EKeys::Gamepad_Left2D, false, false, false, true);

	Map(LookAction, EKeys::Mouse2D, false, false, true);
	// Not negated: LookStick turns the camera itself, with stick up looking up.
	Map(LookStickAction, EKeys::Gamepad_Right2D, false, false, false, true);

	Map(JumpAction, EKeys::SpaceBar);
	Map(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	Map(ThrustAction, EKeys::LeftShift);
	Map(ThrustToggleAction, EKeys::Gamepad_LeftThumbstick);
	Map(DodgeAction, EKeys::LeftControl);
	Map(DodgeAction, EKeys::C);
	Map(DodgeAction, EKeys::Gamepad_FaceButton_Right);
	Map(LaunchAction, EKeys::F);
	Map(LaunchAction, EKeys::Gamepad_FaceButton_Top);
	Map(FireAction, EKeys::LeftMouseButton);
	Map(FireAction, EKeys::Gamepad_RightTrigger);
	Map(AimAction, EKeys::RightMouseButton);
	Map(AimAction, EKeys::Gamepad_LeftTrigger);
	Map(ReloadAction, EKeys::R);
	Map(ReloadAction, EKeys::Gamepad_FaceButton_Left);
	Map(GrenadeAction, EKeys::Q);
	Map(GrenadeAction, EKeys::Gamepad_LeftShoulder);
	Map(MissileAction, EKeys::E);
	Map(MissileAction, EKeys::Gamepad_RightShoulder);
	Map(SuitRangerAction, EKeys::One);
	Map(SuitExoAction, EKeys::Two);
	Map(SuitSemperAction, EKeys::Three);
	Map(SuitNextAction, EKeys::Gamepad_DPad_Right);
	Map(SuitPreviousAction, EKeys::Gamepad_DPad_Left);
	Map(VisorAction, EKeys::V);
	Map(VisorAction, EKeys::Gamepad_DPad_Up);
	Map(ShieldAction, EKeys::G);
	Map(ShieldAction, EKeys::MiddleMouseButton);
	Map(ShieldToggleAction, EKeys::Gamepad_RightThumbstick);

	DefaultMappingContext = Context;
}

void AJavelinCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateDefaultInput();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("JavelinCharacter needs Enhanced Input. Check Config/DefaultInput.ini."));
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AJavelinCharacter::Move);
		Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AJavelinCharacter::MoveCompleted);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AJavelinCharacter::Look);
	}
	if (LookStickAction)
	{
		Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AJavelinCharacter::LookStick);
		Input->BindAction(LookStickAction, ETriggerEvent::Completed, this, &AJavelinCharacter::LookStickCompleted);
	}
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnJumpPressed);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AJavelinCharacter::OnJumpReleased);
	}
	if (ThrustAction)
	{
		Input->BindAction(ThrustAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnThrustPressed);
		Input->BindAction(ThrustAction, ETriggerEvent::Completed, this, &AJavelinCharacter::OnThrustReleased);
	}
	if (ThrustToggleAction)
	{
		Input->BindAction(ThrustToggleAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnThrustTogglePressed);
	}
	if (DodgeAction)
	{
		Input->BindAction(DodgeAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnDodgePressed);
	}
	if (LaunchAction)
	{
		Input->BindAction(LaunchAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnLaunchPressed);
	}
	if (FireAction)
	{
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnFirePressed);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AJavelinCharacter::OnFireReleased);
	}
	if (AimAction)
	{
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnAimPressed);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &AJavelinCharacter::OnAimReleased);
	}
	if (ReloadAction)
	{
		Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnReloadPressed);
	}
	if (GrenadeAction)
	{
		Input->BindAction(GrenadeAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnGrenadePressed);
	}
	if (MissileAction)
	{
		Input->BindAction(MissileAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnMissilePressed);
	}
	if (SuitRangerAction)
	{
		Input->BindAction(SuitRangerAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnSuitRangerPressed);
	}
	if (SuitExoAction)
	{
		Input->BindAction(SuitExoAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnSuitExoPressed);
	}
	if (SuitSemperAction)
	{
		Input->BindAction(SuitSemperAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnSuitSemperPressed);
	}
	if (SuitNextAction)
	{
		Input->BindAction(SuitNextAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnSuitNextPressed);
	}
	if (SuitPreviousAction)
	{
		Input->BindAction(SuitPreviousAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnSuitPreviousPressed);
	}
	if (VisorAction)
	{
		Input->BindAction(VisorAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnVisorPressed);
	}
	if (ShieldAction)
	{
		Input->BindAction(ShieldAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnShieldPressed);
		Input->BindAction(ShieldAction, ETriggerEvent::Completed, this, &AJavelinCharacter::OnShieldReleased);
	}
	if (ShieldToggleAction)
	{
		Input->BindAction(ShieldToggleAction, ETriggerEvent::Started, this, &AJavelinCharacter::OnShieldTogglePressed);
	}
}

void AJavelinCharacter::Move(const FInputActionValue& Value)
{
	if (IsSuitDown())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();

	JavelinMovement->SetFlightInput(Axis);
	if (JavelinMovement->IsInFlight() || !Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Basis(YawRotation);
	AddMovementInput(Basis.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Axis.X);
	LastMoveInput = Basis.GetUnitAxis(EAxis::X) * Axis.Y + Basis.GetUnitAxis(EAxis::Y) * Axis.X;
}

void AJavelinCharacter::MoveCompleted()
{
	GetJavelinMovement()->SetFlightInput(FVector2D::ZeroVector);
	LastMoveInput = FVector::ZeroVector;

	// A sprint switched on with the gamepad stick ends when you let go of the stick, like in Anthem.
	if (bSprintToggled)
	{
		bSprintToggled = false;
		GetJavelinMovement()->SetSprinting(false);
	}
}

void AJavelinCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>() * (Weapon->IsAiming() ? AimLookScale : 1.f);
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AJavelinCharacter::LookStick(const FInputActionValue& Value)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	const bool bAiming = Weapon->IsAiming();

	// The dead zone modifier already rescales the stick to 0..1 past the dead zone.
	// Curve the magnitude (not each axis) so small nudges are slow and precise and diagonals stay smooth.
	const FVector2D Raw = Value.Get<FVector2D>();
	const float Magnitude = FMath::Min(Raw.Size(), 1.f);
	if (Magnitude <= KINDA_SMALL_NUMBER)
	{
		StickHoldTime = 0.f;
		return;
	}
	const FVector2D Stick = Raw / Raw.Size() * FMath::Pow(Magnitude, StickResponseExponent);

	// Like Anthem on console: holding the stick pushed sideways speeds up turning after a short delay.
	if (FMath::Abs(Raw.X) >= StickRampThreshold)
	{
		StickHoldTime += DeltaSeconds;
	}
	else
	{
		StickHoldTime = 0.f;
	}
	const float Ramp = StickRampTime > 0.f
		? FMath::Clamp((StickHoldTime - StickRampDelay) / StickRampTime, 0.f, 1.f)
		: (StickHoldTime >= StickRampDelay ? 1.f : 0.f);
	const float RampEased = Ramp * Ramp * (3.f - 2.f * Ramp);

	FVector2D Rate = bAiming ? StickAimLookRate : StickLookRate;
	Rate.X += (bAiming ? StickAimRampYawBonus : StickRampYawBonus) * RampEased;
	if (bAiming && Weapon->IsCrosshairOnTarget())
	{
		Rate *= StickAimFriction;
	}

	// Rotate the controller directly instead of AddControllerYawInput, which UE5's legacy input
	// scales multiply by 2.5 and made the stick far too fast. The camera manager still clamps pitch.
	FRotator Rotation = PC->GetControlRotation();
	Rotation.Yaw += Stick.X * Rate.X * DeltaSeconds;
	Rotation.Pitch += Stick.Y * Rate.Y * DeltaSeconds;
	PC->SetControlRotation(Rotation);
}

void AJavelinCharacter::LookStickCompleted()
{
	StickHoldTime = 0.f;
}

bool AJavelinCharacter::TryStartFlight()
{
	const bool bFromGround = GetJavelinMovement()->IsMovingOnGround();
	if (Heat->IsOverheated() || !GetJavelinMovement()->StartFlight())
	{
		return false;
	}

	if (bFromGround)
	{
		// Blasting off the ground: the jets kick up a ring of dust, the legs push off and everything shakes.
		ADustEffect::Spawn(GetWorld(), GetFeetLocation(), 220.f, 1.1f, 9, DustColor, 1.1f);
		AddCameraTrauma(0.3f);
		PlayRumble(0.6f, 0.25f, true);
		CameraDip.Speed -= 120.f;
	}
	return true;
}

bool AJavelinCharacter::TryStartHover()
{
	return !Heat->IsOverheated() && GetJavelinMovement()->StartHover();
}

void AJavelinCharacter::OnJumpPressed()
{
	if (IsSuitDown())
	{
		return;
	}
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	switch (JavelinMovement->GetJavelinMode())
	{
	case EJavelinMoveMode::Flight:
		TryStartHover();
		break;
	case EJavelinMoveMode::Hover:
		JavelinMovement->StopFlightOrHover();
		break;
	default:
		if (JavelinMovement->IsFalling())
		{
			TryStartHover();
		}
		else
		{
			Jump();
		}
		break;
	}
}

void AJavelinCharacter::OnJumpReleased()
{
	StopJumping();
}

void AJavelinCharacter::OnThrustPressed()
{
	if (IsSuitDown())
	{
		return;
	}
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	if (JavelinMovement->IsInFlight())
	{
		JavelinMovement->StopFlightOrHover();
	}
	else if (JavelinMovement->IsMovingOnGround())
	{
		JavelinMovement->SetSprinting(true);
	}
	else
	{
		TryStartFlight();
	}
}

void AJavelinCharacter::OnThrustReleased()
{
	bSprintToggled = false;
	GetJavelinMovement()->SetSprinting(false);
}

void AJavelinCharacter::OnThrustTogglePressed()
{
	if (IsSuitDown())
	{
		return;
	}
	// Holding a stick click down is tiring, so on a gamepad a click switches sprint on and another click (or
	// letting go of the stick) switches it off. In the air it works like Shift: fly, or stop flying.
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	if (JavelinMovement->IsMovingOnGround())
	{
		bSprintToggled = !JavelinMovement->WantsToSprint();
		JavelinMovement->SetSprinting(bSprintToggled);
	}
	else
	{
		OnThrustPressed();
	}
}

void AJavelinCharacter::OnDodgePressed()
{
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	if (IsSuitDown() || !JavelinMovement->CanDodge())
	{
		return;
	}

	// Dodge the way you're pushing. With no input, hop backward away from whatever you're facing down.
	FVector Direction = LastMoveInput;
	Direction.Z = 0.f;
	if (Direction.SizeSquared() < 0.04f)
	{
		const float Yaw = Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
		Direction = -FRotator(0.f, Yaw, 0.f).Vector();
	}
	if (!JavelinMovement->StartDodge(Direction))
	{
		return;
	}

	// The jets fire hard: dust blasts off the ground, the legs crouch into it, and the pad thumps.
	const bool bOnGround = JavelinMovement->IsMovingOnGround();
	if (bOnGround)
	{
		ADustEffect::Spawn(GetWorld(), GetFeetLocation(), 110.f, 0.55f, 6, DustColor, 0.8f);
	}
	LandingCrouch.Speed += 3.5f;
	CameraDip.Speed -= 45.f;
	AddCameraTrauma(0.14f);
	PlayRumble(0.45f, 0.15f, true);
	bWasDodging = true;
	DodgeDustTimer = 0.f;
}

void AJavelinCharacter::UpdateDodge(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	if (JavelinMovement->IsDodging())
	{
		// A trail of dust off the skidding feet.
		DodgeDustTimer -= DeltaSeconds;
		if (DodgeDustTimer <= 0.f && JavelinMovement->IsMovingOnGround() && bFeetPlanted)
		{
			DodgeDustTimer = 0.05f;
			for (const FJavelinFoot& Foot : Feet)
			{
				ADustEffect::Spawn(GetWorld(), Foot.Location - FVector(0.f, 0.f, AnkleHeight), 25.f, 0.3f, 2, DustColor, 0.5f);
			}
		}
		return;
	}

	if (bWasDodging)
	{
		// The burst ends and the suit digs its feet in.
		bWasDodging = false;
		if (JavelinMovement->IsMovingOnGround())
		{
			LandingCrouch.Speed += 2.5f;
			CameraDip.Speed -= 30.f;
			PlayRumble(0.25f, 0.08f, true);
			ADustEffect::Spawn(GetWorld(), GetFeetLocation(), 70.f, 0.4f, 4, DustColor, 0.6f);
		}
	}
}

void AJavelinCharacter::OnLaunchPressed()
{
	if (IsSuitDown())
	{
		return;
	}
	if (!GetJavelinMovement()->IsInFlight())
	{
		TryStartFlight();
	}
}

void AJavelinCharacter::OnFirePressed()
{
	if (IsSuitDown())
	{
		return;
	}
	// Shooting stops a sprint. Holding the trigger while flying starts firing as soon as you hover.
	GetJavelinMovement()->SetSprinting(false);
	Weapon->SetTriggerHeld(true);
}

void AJavelinCharacter::OnFireReleased()
{
	Weapon->SetTriggerHeld(false);
}

void AJavelinCharacter::OnAimPressed()
{
	if (IsSuitDown())
	{
		return;
	}
	GetJavelinMovement()->SetSprinting(false);
	Weapon->SetWantsToAim(true);
}

void AJavelinCharacter::OnAimReleased()
{
	Weapon->SetWantsToAim(false);
}

void AJavelinCharacter::OnReloadPressed()
{
	if (IsSuitDown())
	{
		return;
	}
	Weapon->StartReload();
}

void AJavelinCharacter::OnGrenadePressed()
{
	if (IsSuitDown())
	{
		return;
	}
	// Using gear stops a sprint, like shooting.
	GetJavelinMovement()->SetSprinting(false);
	Abilities->UseGear(EJavelinGear::Grenade);
}

void AJavelinCharacter::OnMissilePressed()
{
	if (IsSuitDown())
	{
		return;
	}
	GetJavelinMovement()->SetSprinting(false);
	Abilities->UseGear(EJavelinGear::Missiles);
}

bool AJavelinCharacter::CanFireWeapon() const
{
	return !GetJavelinMovement()->IsInFlight() && !IsSuitDown();
}

bool AJavelinCharacter::CanAimWeapon() const
{
	return CanFireWeapon() && !GetJavelinMovement()->IsSprinting();
}

bool AJavelinCharacter::IsInCombatStance() const
{
	const float SinceGear = FMath::Min(Abilities->GetTimeSinceUsed(EJavelinGear::Grenade), Abilities->GetTimeSinceUsed(EJavelinGear::Missiles));
	return CanFireWeapon() && (Weapon->IsAiming() || Weapon->GetTimeSinceLastShot() < CombatStanceTime || SinceGear < CombatStanceTime
		|| ShieldRaise > 0.5f);
}

void AJavelinCharacter::UpdateCombatFacing()
{
	// On foot and falling the suit normally turns toward where it moves. In combat it faces the crosshair instead,
	// so you can strafe while shooting. Hover already faces the camera, and flight steers itself.
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	// A dodge keeps the suit facing the way it was, so a sidestep stays a sidestep.
	const bool bHoldFacing = JavelinMovement->IsDodging();
	const bool bFaceAim = IsInCombatStance() && !bHoldFacing;
	JavelinMovement->bOrientRotationToMovement = !bFaceAim && !bHoldFacing;
	JavelinMovement->bUseControllerDesiredRotation = bFaceAim;
}

void AJavelinCharacter::UpdateMuzzleFlash()
{
	const bool bFlash = Weapon->GetTimeSinceLastShot() < MuzzleFlashTime;
	MuzzleFlash->SetVisibility(bFlash);
	MuzzleLight->SetVisibility(bFlash);
	if (bFlash)
	{
		// A different size and spin each frame so rapid fire flickers.
		const float Size = FMath::FRandRange(0.8f, 1.3f);
		MuzzleFlash->SetRelativeRotation(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
		MuzzleFlash->SetRelativeScale3D(MuzzleFlashScale * Size);
	}
}

void AJavelinCharacter::HandleOverheated()
{
	GetJavelinMovement()->StopFlightOrHover();
}

bool AJavelinCharacter::IsSuitDown() const
{
	return SuitHealth->IsDown();
}

float AJavelinCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Incoming = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Incoming <= 0.f)
	{
		return 0.f;
	}

	// Remember where the hit came from so the HUD can point at it.
	FVector Source = DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation();
	FVector HitDirection = (GetActorLocation() - Source).GetSafeNormal();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		Source = GetActorLocation() - PointDamage.ShotDirection * 1000.f;
		HitDirection = PointDamage.ShotDirection.GetSafeNormal();
	}

	// SEMPER-03's ballistic shield soaks up hits from the front while it's raised.
	const float GetsThrough = AbsorbWithShield(Incoming, HitDirection);
	if (GetsThrough <= 0.f)
	{
		return 0.f;
	}

	const bool bHadShields = SuitHealth->GetShields() > 0.f;
	const float Dealt = SuitHealth->ApplyDamage(GetsThrough, Source);
	if (Dealt <= 0.f)
	{
		return 0.f;
	}

	// Feel the hit: the camera jolts, the torso snaps away from it, and the suit gets shoved a little.
	// Hits that get through to armor land harder than ones the shields soak.
	const bool bArmorHit = SuitHealth->GetShields() <= 0.f;
	const float Weight = FMath::Clamp(Dealt / 40.f, 0.15f, 1.f) * (bArmorHit ? 1.3f : 1.f);
	AddCameraTrauma(0.12f + 0.25f * Weight);
	PlayRumble(0.3f + 0.4f * Weight, 0.15f, bArmorHit);

	// Hit from the front leans the torso back, hit from behind pitches it forward.
	const float FrontBack = static_cast<float>(FVector::DotProduct(HitDirection, GetActorForwardVector()));
	HitFlinch.Speed += -FrontBack * 260.f * Weight;

	FVector Push = HitDirection * FMath::Min(Dealt * HitKnockbackPerDamage, 450.f);
	Push.Z = 0.f;
	if (!IsSuitDown())
	{
		GetJavelinMovement()->AddImpulse(Push, true);
	}

	if (bHadShields && SuitHealth->GetShields() <= 0.f)
	{
		// Shields breaking is a jolt of its own.
		AddCameraTrauma(0.3f);
		PlayRumble(0.8f, 0.3f, true);
	}
	return Dealt;
}

void AJavelinCharacter::HandleSuitDown()
{
	// Drop out of the air and let go of everything until the suit reboots.
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	JavelinMovement->StopFlightOrHover();
	JavelinMovement->SetSprinting(false);
	JavelinMovement->SetFlightInput(FVector2D::ZeroVector);
	Weapon->SetTriggerHeld(false);
	Weapon->SetWantsToAim(false);
	bWantsShield = false;
	GetWorldTimerManager().SetTimer(RebootTimer, this, &AJavelinCharacter::Reboot, FMath::Max(RebootTime, 0.1f));
}

void AJavelinCharacter::Reboot()
{
	if (bRebootAtStart)
	{
		SetActorLocationAndRotation(StartLocation, StartRotation, false, nullptr, ETeleportType::TeleportPhysics);
		GetJavelinMovement()->StopMovementImmediately();
		if (Controller)
		{
			Controller->SetControlRotation(StartRotation);
		}
	}
	SuitHealth->Restore();
	Weapon->RefillAmmo();
	Abilities->ResetCooldowns();
	ShieldIntegrity = ShieldMaxIntegrity;
	ShieldBrokenUntil = -100.f;
}

float AJavelinCharacter::GetTimeUntilReboot() const
{
	return GetWorldTimerManager().IsTimerActive(RebootTimer) ? GetWorldTimerManager().GetTimerRemaining(RebootTimer) : 0.f;
}

void AJavelinCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateShield(DeltaSeconds);
	UpdateVisor();
	UpdateCombatFacing();
	UpdateMuzzleFlash();
	CheckGearUsed();
	UpdateJetDust(DeltaSeconds);
	UpdateFeelSprings(DeltaSeconds);
	UpdateDodge(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
	UpdateSuitExtras(DeltaSeconds);
}

FVector AJavelinCharacter::GetFeetLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void AJavelinCharacter::AddCameraTrauma(float Amount)
{
	CameraTrauma = FMath::Clamp(CameraTrauma + Amount, 0.f, 1.f);
}

void AJavelinCharacter::PlayRumble(float Intensity, float Duration, bool bHeavy)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!bGamepadRumble || !PlayerController || Intensity <= 0.f || Duration <= 0.f)
	{
		return;
	}
	// The big motors give the deep thump, the small ones the buzz.
	PlayerController->PlayDynamicForceFeedback(FMath::Clamp(Intensity * RumbleScale, 0.f, 1.f), Duration, bHeavy, true, bHeavy, true);
}

void AJavelinCharacter::UpdateFeelSprings(float DeltaSeconds)
{
	LandingCrouch.Update(DeltaSeconds, 70.f, 11.f);
	CameraDip.Update(DeltaSeconds, 90.f, 12.f);
	CameraKick.Update(DeltaSeconds, 250.f, 30.f);
	HitFlinch.Update(DeltaSeconds, 220.f, 18.f);
	StepStomp.Update(DeltaSeconds, 170.f, 15.f);
}

void AJavelinCharacter::HandleLanded(const FVector& ImpactVelocity, bool bFromFlight)
{
	// Landing out of flight at a shallow angle still slams down, so forward speed counts for some of the impact.
	const float FallSpeed = FMath::Max(0.f, static_cast<float>(-ImpactVelocity.Z));
	const float ImpactSpeed = bFromFlight ? FMath::Max(FallSpeed, static_cast<float>(ImpactVelocity.Size()) * 0.5f) : FallSpeed;
	const float Strength = FMath::GetMappedRangeValueClamped(FVector2D(SoftLandingSpeed, HeavyLandingSpeed), FVector2D(0.f, 1.f), static_cast<double>(ImpactSpeed));
	if (ImpactSpeed < SoftLandingSpeed)
	{
		// A hop: just a little give in the knees.
		LandingCrouch.Speed += 1.5f;
		CameraDip.Speed -= 30.f;
		return;
	}

	// Knees absorb the weight, the camera dips and bounces back, and a hard one shakes the ground.
	LandingCrouch.Speed += 4.f + 11.f * Strength;
	CameraDip.Speed -= 80.f + 280.f * Strength;
	AddCameraTrauma(0.12f + 0.5f * Strength);
	PlayRumble(0.25f + 0.75f * Strength, 0.12f + 0.25f * Strength, Strength > 0.35f);

	if (Strength > 0.2f)
	{
		ADustEffect::Spawn(GetWorld(), GetFeetLocation(), 120.f + 260.f * Strength, 0.6f + 0.9f * Strength,
			FMath::RoundToInt(5.f + 5.f * Strength), DustColor, 0.7f + 0.6f * Strength);
	}
}

void AJavelinCharacter::HandleCrashed(const FVector& ImpactVelocity, const FVector& ImpactPoint)
{
	// Slamming into a wall at flight speed: sparks and rock dust where you hit, and a big jolt.
	if (AShotEffect* Sparks = GetWorld()->SpawnActor<AShotEffect>(ImpactPoint, FRotator::ZeroRotator))
	{
		Sparks->InitBurst(ImpactPoint, FLinearColor(1.f, 0.7f, 0.35f), 2.f);
	}
	ADustEffect::Spawn(GetWorld(), ImpactPoint, 150.f, 1.f, 8, DustColor * 1.2f, 1.f);
	AddCameraTrauma(0.9f);
	PlayRumble(1.f, 0.45f, true);
	HitFlinch.Speed += 450.f;
}

void AJavelinCharacter::HandleWeaponFired()
{
	// Each shot punches the camera up a touch (on top of the real recoil the gun adds to your aim) and buzzes the pad.
	const bool bAiming = Weapon->IsAiming();
	CameraKick.Speed += bAiming ? 22.f : 38.f;
	AddCameraTrauma(bAiming ? 0.03f : 0.05f);
	PlayRumble(0.2f, 0.05f, false);
}

void AJavelinCharacter::HandleFootstep(int32 Side, float Weight)
{
	// Every step is a stomp: the body drops onto the foot, the camera thumps and the pad gives a small knock.
	// Sprinting stomps harder and kicks up dust.
	StepStomp.Speed += 12.f * StepStompDrop * Weight;
	CameraDip.Speed -= 24.f * Weight;
	LandingCrouch.Speed += 0.2f * Weight;
	AddCameraTrauma(0.012f * Weight);
	PlayRumble(0.1f * Weight, 0.05f, true);

	if (GetVelocity().Size2D() >= HeavyStepSpeed)
	{
		AddCameraTrauma(0.02f);
		PlayRumble(0.18f, 0.07f, true);
		const FVector Sole = Feet[FMath::Clamp(Side, 0, 1)].Location - FVector(0.f, 0.f, AnkleHeight);
		ADustEffect::Spawn(GetWorld(), Sole, 30.f, 0.35f, 3, DustColor, 0.6f);
	}
}

void AJavelinCharacter::CheckGearUsed()
{
	// Gear going off has some kick to it: a grenade throw is light, the missile pod firing is a heavier thud.
	const float GrenadeSince = Abilities->GetTimeSinceUsed(EJavelinGear::Grenade);
	const float MissileSince = Abilities->GetTimeSinceUsed(EJavelinGear::Missiles);
	if (GrenadeSince < PrevGrenadeSinceUsed)
	{
		AddCameraTrauma(0.1f);
		PlayRumble(0.3f, 0.1f, false);
	}
	if (MissileSince < PrevMissileSinceUsed)
	{
		AddCameraTrauma(0.22f);
		PlayRumble(0.55f, 0.2f, true);
		CameraKick.Speed += 40.f;
	}
	PrevGrenadeSinceUsed = GrenadeSince;
	PrevMissileSinceUsed = MissileSince;
}

void AJavelinCharacter::UpdateJetDust(float DeltaSeconds)
{
	// Hovering low, or flying fast and low, the jets blow dust off the ground under you.
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	const bool bFastLowFlight = JavelinMovement->IsInFlight() && GetVelocity().Size() > JavelinMovement->FlightCruiseSpeed * 0.8f;
	if (!JavelinMovement->IsHovering() && !bFastLowFlight)
	{
		JetDustTimer = 0.f;
		return;
	}

	JetDustTimer -= DeltaSeconds;
	if (JetDustTimer > 0.f)
	{
		return;
	}
	JetDustTimer = JavelinMovement->IsHovering() ? 0.12f : 0.06f;

	const FVector Start = GetActorLocation();
	const float MaxHeight = JavelinMovement->IsHovering() ? JetDustHeight : JetDustHeight * 0.6f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JetDust), false, this);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start - FVector(0.f, 0.f, MaxHeight), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return;
	}

	const float Closeness = 1.f - Hit.Distance / MaxHeight;
	if (JavelinMovement->IsHovering())
	{
		ADustEffect::Spawn(GetWorld(), Hit.ImpactPoint, 60.f + 140.f * Closeness, 0.3f + 0.45f * Closeness, 5, DustColor, 0.7f);
	}
	else
	{
		// A wake of dust streaming out behind you.
		ADustEffect::Spawn(GetWorld(), Hit.ImpactPoint, 80.f * Closeness, 0.4f + 0.6f * Closeness, 3, DustColor, 0.9f);
	}
}

void AJavelinCharacter::UpdateCamera(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();

	const FJavelinCameraProfile* Target = &GroundCamera;
	float ExtraFOV = 0.f;
	switch (JavelinMovement->GetJavelinMode())
	{
	case EJavelinMoveMode::Flight:
		Target = &FlightCamera;
		ExtraFOV = FlightBoostExtraFOV * FMath::GetMappedRangeValueClamped(
			FVector2D(JavelinMovement->FlightCruiseSpeed, JavelinMovement->FlightBoostSpeed), FVector2D(0.f, 1.f), GetVelocity().Size());
		break;
	case EJavelinMoveMode::Hover:
		Target = &HoverCamera;
		break;
	default:
		if (JavelinMovement->IsSprinting() && GetVelocity().Size2D() > JavelinMovement->MaxWalkSpeed)
		{
			Target = &SprintCamera;
		}
		break;
	}

	float BlendSpeed = CameraBlendSpeed;
	if (Weapon->IsAiming())
	{
		Target = &AimCamera;
		ExtraFOV = 0.f;
		BlendSpeed = AimCameraBlendSpeed;
	}

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Target->ArmLength, DeltaSeconds, BlendSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, Target->SocketOffset, DeltaSeconds, BlendSpeed);
	CameraBoom->CameraLagSpeed = FMath::FInterpTo(CameraBoom->CameraLagSpeed, Target->LagSpeed, DeltaSeconds, BlendSpeed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, Target->FieldOfView + ExtraFOV, DeltaSeconds, BlendSpeed));

	// Backed up against a wall the camera ends up inside the suit; hide it rather than show its insides.
	const float CameraDistance = FVector::Dist(FollowCamera->GetComponentLocation(), CameraBoom->GetComponentLocation());
	VisualRoot->SetHiddenInGame(CameraDistance < HideSuitCameraDistance, true);

	// Camera shake. Trauma fades over time; squaring it keeps small bumps subtle and big hits violent.
	// Fast flight adds a steady buffet on top.
	CameraTrauma = FMath::Max(0.f, CameraTrauma - ShakeDecay * DeltaSeconds);
	float Buffet = 0.f;
	if (JavelinMovement->IsInFlight())
	{
		Buffet = FlightBuffet * FMath::GetMappedRangeValueClamped(
			FVector2D(JavelinMovement->FlightCruiseSpeed, JavelinMovement->FlightBoostSpeed + JavelinMovement->DiveSpeedBonus), FVector2D(0.3f, 1.f), GetVelocity().Size());
	}
	const float Shake = FMath::Min(1.f, FMath::Square(CameraTrauma) + Buffet) * CameraShakeScale;
	ShakeTime += DeltaSeconds * ShakeFrequency;
	const FRotator ShakeRotation(
		MaxShakeAngles.Pitch * Shake * FMath::PerlinNoise1D(ShakeTime) + CameraKick.Value * CameraShakeScale,
		MaxShakeAngles.Yaw * Shake * FMath::PerlinNoise1D(ShakeTime + 31.7f),
		MaxShakeAngles.Roll * Shake * FMath::PerlinNoise1D(ShakeTime + 73.1f));
	FollowCamera->SetRelativeRotation(ShakeRotation);

	// Landings and footsteps bounce the camera's pivot.
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraPivotHeight + CameraDip.Value * CameraShakeScale));
}

void AJavelinCharacter::UpdateVisuals(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();

	const float Yaw = GetActorRotation().Yaw;
	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	LastYaw = Yaw;

	// How the suit is speeding up or slowing down, smoothed so it reads as weight shifting rather than jitter.
	const FVector SuitVelocity = GetVelocity();
	const FVector SuitAcceleration = (SuitVelocity - LastVelocity) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	LastVelocity = SuitVelocity;
	SmoothedAcceleration = FMath::VInterpTo(SmoothedAcceleration, SuitAcceleration.GetClampedToMaxSize(5000.f), DeltaSeconds, 6.f);

	// Feet first: where they are decides how high the hips sit and how they sway and twist.
	UpdateGait(DeltaSeconds);

	float TargetPitch = 0.f;
	float TargetRoll = 0.f;
	float TiltSpeed = VisualTiltSpeed;
	if (JavelinMovement->IsInFlight())
	{
		// Lie along the heading (back up, head forward) and bank into turns. In a hard turn the velocity
		// drifts off the heading, so you see the suit slide.
		TargetPitch = -90.f + JavelinMovement->GetFlightHeading().Rotation().Pitch;
		TargetRoll = FMath::Clamp(YawRate * FlightBankFactor, -MaxFlightBank, MaxFlightBank);
	}
	else if (JavelinMovement->IsDodging())
	{
		// Throw the weight into the dodge.
		const FVector LocalDodge = GetActorRotation().UnrotateVector(JavelinMovement->GetDodgeDirection());
		const float Fade = 1.f - 0.5f * JavelinMovement->GetDodgeProgress();
		TargetPitch = static_cast<float>(-LocalDodge.X) * DodgeLean * Fade;
		TargetRoll = static_cast<float>(LocalDodge.Y) * DodgeLean * Fade;
		TiltSpeed = VisualTiltSpeed * 2.f;
	}
	else if (JavelinMovement->IsMovingOnGround() && !IsSuitDown())
	{
		// A heavy suit leans into speeding up and turning, and rocks back when it brakes.
		const FVector LocalAccel = GetActorRotation().UnrotateVector(SmoothedAcceleration) / 100.f;
		TargetPitch = FMath::Clamp(static_cast<float>(-LocalAccel.X) * GroundLeanFactor, -MaxGroundLean, MaxGroundLean);
		TargetRoll = FMath::Clamp(static_cast<float>(LocalAccel.Y) * GroundLeanFactor, -MaxGroundLean, MaxGroundLean);
	}

	VisualPitch = FMath::FInterpTo(VisualPitch, TargetPitch, DeltaSeconds, TiltSpeed);
	VisualRoll = FMath::FInterpTo(VisualRoll, TargetRoll, DeltaSeconds, TiltSpeed);

	// Pitch first, then roll around the flight direction. The hips twist and tilt with the stride on top.
	const FQuat Tilt = FRotator(0.f, 0.f, VisualRoll).Quaternion() * FRotator(VisualPitch, 0.f, 0.f).Quaternion();
	const FQuat HipMotion = FRotator(0.f, GaitHipYaw * LegIKWeight, GaitHipRoll * LegIKWeight).Quaternion();
	VisualRoot->SetRelativeRotation(Tilt * HipMotion);

	// Limbs blend toward the pose for the current movement state.
	const FJavelinSuitPose Target = ComputeTargetPose(DeltaSeconds);
	const float Alpha = FMath::Clamp(DeltaSeconds * PoseBlendSpeed, 0.f, 1.f);
	auto Blend = [Alpha](float& Current, float Goal) { Current = FMath::Lerp(Current, Goal, Alpha); };
	Blend(CurrentPose.TorsoPitch, Target.TorsoPitch);
	Blend(CurrentPose.HeadPitch, Target.HeadPitch);
	Blend(CurrentPose.ShoulderSpread, Target.ShoulderSpread);
	Blend(CurrentPose.LegSpread, Target.LegSpread);
	Blend(CurrentPose.BodyBob, Target.BodyBob);
	Blend(CurrentPose.BackFlame, Target.BackFlame);
	Blend(CurrentPose.FootFlame, Target.FootFlame);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Blend(CurrentPose.ShoulderPitch[Side], Target.ShoulderPitch[Side]);
		Blend(CurrentPose.ElbowBend[Side], Target.ElbowBend[Side]);
		Blend(CurrentPose.HipPitch[Side], Target.HipPitch[Side]);
		Blend(CurrentPose.KneeBend[Side], Target.KneeBend[Side]);
		Blend(CurrentPose.AnklePitch[Side], Target.AnklePitch[Side]);
	}

	// Quick reactions go on top of the blended pose so they snap instead of easing in.
	FJavelinSuitPose FinalPose = CurrentPose;
	if (!JavelinMovement->IsInFlight())
	{
		// Landing crouch: hips drop, knees fold, torso tips forward and arms come out for balance.
		const float Crouch = FMath::Clamp(LandingCrouch.Value, -0.1f, 1.2f);
		FinalPose.BodyBob -= 32.f * Crouch;
		FinalPose.TorsoPitch -= 18.f * Crouch;
		FinalPose.HeadPitch += 12.f * Crouch;
		FinalPose.ShoulderSpread += 12.f * Crouch;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FinalPose.HipPitch[Side] += 50.f * Crouch;
			FinalPose.KneeBend[Side] -= 85.f * Crouch;
		}
	}

	// The walking gait: hips ride up and down over the planted feet, drop as each foot stomps down, sway over the
	// foot carrying the weight and twist with the stride. The shoulders twist back against the hips, and the head
	// turns against both so it stays steady. Less twist while aiming so the gun stays on target.
	if (LegIKWeight > 0.f)
	{
		const float W = LegIKWeight;
		const float Stomp = FMath::Clamp(StepStomp.Value, -4.f, 15.f);
		const float TwistScale = IsInCombatStance() ? 0.4f : 1.f;
		FinalPose.BodyBob += (GaitBodyOffset - Stomp) * W;
		FinalPose.BodySway += GaitSway * W;
		FinalPose.TorsoPitch -= 0.6f * Stomp * W;
		FinalPose.TorsoYaw -= GaitHipYaw * (1.f + 0.6f * TwistScale) * W;
		FinalPose.TorsoRoll -= GaitHipRoll * 1.3f * W;
		FinalPose.HeadYaw -= GaitHipYaw * W + FinalPose.TorsoYaw;
		FinalPose.HeadRoll -= (GaitHipRoll * W + FinalPose.TorsoRoll) * 0.8f;
	}

	// Taking a hit snaps the torso and head.
	FinalPose.TorsoPitch += HitFlinch.Value;
	FinalPose.HeadPitch += HitFlinch.Value * 0.6f;

	// Recoil kicks the gun arm up and back, and the support arm with it.
	if (IsInCombatStance())
	{
		const float Kick = Weapon->GetRecoilKick();
		FinalPose.ShoulderPitch[1] += 7.f * Kick;
		FinalPose.ElbowBend[1] += 10.f * Kick;
		FinalPose.ShoulderPitch[0] += 5.f * Kick;
		FinalPose.ElbowBend[0] += 6.f * Kick;
		FinalPose.TorsoPitch += 1.5f * Kick;
	}

	// Dodge jets: a hard burst from the feet and back that dies off through the dash.
	if (JavelinMovement->IsDodging())
	{
		const float Burst = 1.3f * FMath::Sqrt(1.f - JavelinMovement->GetDodgeProgress());
		FinalPose.FootFlame = FMath::Max(FinalPose.FootFlame, Burst);
		FinalPose.BackFlame = FMath::Max(FinalPose.BackFlame, 0.8f * Burst);
	}

	ApplyPose(FinalPose);
	ApplyLegIK();

	// Thrusters glow blue when active and shift to red as heat builds.
	if (ThrusterMaterial)
	{
		FLinearColor Color = ThrusterIdleColor;
		if (JavelinMovement->GetJavelinMode() != EJavelinMoveMode::None || JavelinMovement->IsDodging())
		{
			Color = FLinearColor::LerpUsingHSV(ThrusterCoolColor, ThrusterHotColor, Heat->GetHeatPercent());
		}
		else if (Heat->IsOverheated())
		{
			Color = ThrusterHotColor;
		}
		ThrusterMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

FVector AJavelinCharacter::GetFootRest(int32 Side) const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return FVector(FootRestForward, Side == 0 ? -FootRestWidth : FootRestWidth, -HalfHeight + AnkleHeight);
}

void AJavelinCharacter::FindFootGround(const FVector& Around, FVector& OutAnkle, FVector& OutNormal) const
{
	const FVector Up = FVector::UpVector;
	const FVector Start = Around + Up * 60.f;
	const FVector End = Around - Up * 80.f;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(JavelinFoot), false, this);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
	{
		OutAnkle = Hit.ImpactPoint + Up * AnkleHeight;
		// Feet tilt to follow a slope, but not so far they look broken on a rock edge.
		FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		if (FVector::DotProduct(Normal, Up) < 0.9f)
		{
			Normal = (Normal + Up).GetSafeNormal();
		}
		OutNormal = Normal.IsNearlyZero() ? Up : Normal;
		return;
	}
	OutAnkle = FVector(Around.X, Around.Y, GetFeetLocation().Z + AnkleHeight);
	OutNormal = Up;
}

void AJavelinCharacter::ResetFeet()
{
	const FTransform ActorTransform = GetActorTransform();
	const float Yaw = GetActorRotation().Yaw;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FJavelinFoot& Foot = Feet[Side];
		FindFootGround(ActorTransform.TransformPosition(GetFootRest(Side)), Foot.Location, Foot.Normal);
		Foot.Yaw = Yaw + (Side == 0 ? -FootToeOut : FootToeOut);
		Foot.bSwinging = false;
		Foot.SwingProgress = 0.f;
		Foot.Support = 1.f;
		Foot.Stride = 0.f;
		Foot.ToePitch = 0.f;
		Foot.Target = Foot.Location;
		Foot.TargetNormal = Foot.Normal;
	}
	bFeetPlanted = true;
	GaitLastYaw = Yaw;
	GaitLastLocation = GetActorLocation();
}

void AJavelinCharacter::UpdateGait(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	const bool bOnGround = JavelinMovement->IsMovingOnGround() && !IsSuitDown();

	// Planted-foot IK only on the ground. In the air the legs go back to their animated poses.
	LegIKWeight = FMath::FInterpConstantTo(LegIKWeight, bOnGround ? 1.f : 0.f, DeltaSeconds, 8.f);
	if (!bOnGround)
	{
		bFeetPlanted = false;
		GaitBodyOffset = FMath::FInterpTo(GaitBodyOffset, 0.f, DeltaSeconds, 8.f);
		GaitSway = FMath::FInterpTo(GaitSway, 0.f, DeltaSeconds, 8.f);
		GaitHipYaw = FMath::FInterpTo(GaitHipYaw, 0.f, DeltaSeconds, 8.f);
		GaitHipRoll = FMath::FInterpTo(GaitHipRoll, 0.f, DeltaSeconds, 8.f);
		GaitSpeed = 0.f;
		bGaitRunning = false;
		return;
	}

	// Touching down, or teleported (a reboot): put both feet under the hips.
	if (!bFeetPlanted || FVector::DistSquared(GetActorLocation(), GaitLastLocation) > FMath::Square(400.f))
	{
		ResetFeet();
	}
	GaitLastLocation = GetActorLocation();

	const FTransform ActorTransform = GetActorTransform();
	const float ActorYaw = GetActorRotation().Yaw;
	const float YawRate = FMath::FindDeltaAngleDegrees(GaitLastYaw, ActorYaw) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	GaitLastYaw = ActorYaw;
	const float Omega = FMath::DegreesToRadians(FMath::Clamp(YawRate, -720.f, 720.f));

	FVector LocalVelocity = ActorTransform.InverseTransformVectorNoScale(GetVelocity());
	LocalVelocity.Z = 0.f;

	// How fast a planted foot slides back relative to the hips: the body's speed, plus turning swinging it around.
	auto FootRelativeVelocity = [&LocalVelocity, Omega](const FVector& Rest)
	{
		return LocalVelocity + FVector(-Omega * Rest.Y, Omega * Rest.X, 0.f);
	};
	GaitSpeed = 0.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		GaitSpeed = FMath::Max(GaitSpeed, static_cast<float>(FootRelativeVelocity(GetFootRest(Side)).Size2D()));
	}

	const float WalkSpeed = FMath::Max(JavelinMovement->MaxWalkSpeed, 1.f);
	const float SprintSpeed = FMath::Max(JavelinMovement->SprintSpeed, WalkSpeed + 1.f);
	const float MoveAlpha = FMath::Clamp(GaitSpeed / WalkSpeed, 0.f, 1.f);
	const float SprintAlpha = FMath::Clamp((GaitSpeed - WalkSpeed) / (SprintSpeed - WalkSpeed), 0.f, 1.f);

	// Cadence: slow heavy steps when walking slowly, quicker at a sprint. Each foot stays down for as long as it
	// can still reach, so at speed both feet leave the ground between steps, like a heavy run.
	const float StepRate = GaitSpeed <= WalkSpeed
		? FMath::Lerp(StepRateSlow, StepRateWalk, MoveAlpha)
		: FMath::Lerp(StepRateWalk, StepRateSprint, SprintAlpha);
	const float CycleRate = 0.5f * StepRate;
	const float TargetDuty = FMath::Clamp(2.f * MaxFootReach * CycleRate / FMath::Max(GaitSpeed, 1.f), 0.18f, 0.62f);
	GaitDuty = FMath::FInterpTo(GaitDuty, TargetDuty, DeltaSeconds, 6.f);
	const float StanceTime = GaitDuty / CycleRate;
	const bool bMoving = GaitSpeed > 30.f;
	const bool bDodging = JavelinMovement->IsDodging();

	auto OffRest = [this, &ActorTransform, ActorYaw](int32 Side)
	{
		const FJavelinFoot& Foot = Feet[Side];
		const FVector Local = ActorTransform.InverseTransformPosition(Foot.Location);
		const float RestYaw = ActorYaw + (Side == 0 ? -FootToeOut : FootToeOut);
		return FVector::Dist2D(Local, GetFootRest(Side)) > SettleDistance
			|| FMath::Abs(FMath::FindDeltaAngleDegrees(Foot.Yaw, RestYaw)) > SettleYaw;
	};

	if (bDodging)
	{
		// Dodging, the feet skid along the ground braced against the jets, set wide and trailing the dash.
		const FVector LocalDodge = ActorTransform.InverseTransformVectorNoScale(JavelinMovement->GetDodgeDirection());
		const float Hop = JavelinMovement->GetDodgeProgress() < 0.25f ? 6.f : 0.f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FJavelinFoot& Foot = Feet[Side];
			const FVector Rest = GetFootRest(Side) + FVector(0.f, Side == 0 ? -6.f : 6.f, 0.f);
			const FVector Brace = Rest - LocalDodge * 22.f + FVector(Side == 0 ? 6.f : -6.f, 0.f, 0.f);
			FindFootGround(ActorTransform.TransformPosition(Brace), Foot.Location, Foot.Normal);
			Foot.Location.Z += Hop;
			Foot.Yaw = ActorYaw + (Side == 0 ? -FootToeOut : FootToeOut);
			Foot.bSwinging = false;
			Foot.ToePitch = 0.f;
		}
		bGaitRunning = false;
	}
	else
	{
		// Run the clock while moving, while a foot is in the air, or until both feet have settled under the hips.
		const bool bNeedsStep = bMoving || Feet[0].bSwinging || Feet[1].bSwinging || OffRest(0) || OffRest(1);
		if (bNeedsStep && !bGaitRunning)
		{
			// Starting off from a standstill: step right away instead of waiting for the clock to come round.
			// Strafing leads with the foot on that side; otherwise the foot that's further back (or out of place) goes.
			int32 Lead = Feet[0].Stride <= Feet[1].Stride ? 0 : 1;
			if (!bMoving)
			{
				Lead = OffRest(0) ? 0 : 1;
			}
			else if (FMath::Abs(LocalVelocity.Y) > FMath::Abs(LocalVelocity.X))
			{
				Lead = LocalVelocity.Y > 0.f ? 1 : 0;
			}
			GaitPhase = FMath::Frac(GaitDuty + (Lead == 0 ? 0.f : 0.5f) - 0.001f);
		}
		bGaitRunning = bNeedsStep;
		const float Advance = bNeedsStep ? FMath::Min(CycleRate * DeltaSeconds, 0.5f) : 0.f;
		const float OldPhase = GaitPhase;
		GaitPhase = FMath::Frac(GaitPhase + Advance);

		const float SwingHeight = !bMoving ? StepHeight * 0.5f : FMath::Lerp(StepHeight * FMath::Max(MoveAlpha, 0.5f), SprintStepHeight, SprintAlpha);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FJavelinFoot& Foot = Feet[Side];
			const FVector Rest = GetFootRest(Side);
			const float RestYaw = ActorYaw + (Side == 0 ? -FootToeOut : FootToeOut);

			// Lift off when this foot's part of the cycle comes round. Standing still, a foot that's already in
			// place stays put instead of tapping.
			const float LiftMark = FMath::Frac(GaitDuty + (Side == 0 ? 0.f : 0.5f));
			if (!Foot.bSwinging && PhaseCrossed(OldPhase, Advance, LiftMark) && (bMoving || OffRest(Side)))
			{
				Foot.bSwinging = true;
				Foot.SwingStart = Foot.Location;
				Foot.SwingStartYaw = Foot.Yaw;
				Foot.SwingProgress = 0.f;
				Foot.SwingHeight = SwingHeight;
			}

			if (Foot.bSwinging)
			{
				// Land ahead of the hip by half the distance the body will cover while this foot is down, so the
				// body passes over it. Turning on the spot swings the landing spot around too.
				FVector TargetLocal = Rest + FootRelativeVelocity(Rest) * (0.5f * StanceTime);
				FVector Offset = TargetLocal - Rest;
				Offset.Z = 0.f;
				TargetLocal = Rest + Offset.GetClampedToMaxSize(MaxFootReach);
				FindFootGround(ActorTransform.TransformPosition(TargetLocal), Foot.Target, Foot.TargetNormal);

				const float SwingRate = CycleRate / FMath::Max(1.f - GaitDuty, 0.25f);
				Foot.SwingProgress = FMath::Min(1.f, Foot.SwingProgress + SwingRate * DeltaSeconds);
				const float S = Foot.SwingProgress;
				const float Ease = S * S * (3.f - 2.f * S);

				// The foot rises quickly, hangs, then comes down hard.
				const float Early = FMath::Pow(S, 0.75f);
				const float Lift = Foot.SwingHeight * 4.f * Early * (1.f - Early);
				Foot.Location = FMath::Lerp(Foot.SwingStart, Foot.Target, Ease) + FVector(0.f, 0.f, Lift);
				Foot.Yaw = Foot.SwingStartYaw + FMath::FindDeltaAngleDegrees(Foot.SwingStartYaw, RestYaw) * Ease;
				Foot.Normal = FMath::Lerp(FVector::UpVector, Foot.TargetNormal, Ease).GetSafeNormal();
				// Toes push off pointing down, then lift just before the heel comes down.
				Foot.ToePitch = S < 0.5f ? -18.f * FMath::Sin(UE_PI * S / 0.5f) : 14.f * FMath::Sin(UE_PI * (S - 0.5f) / 0.5f);

				if (S >= 1.f)
				{
					Foot.bSwinging = false;
					Foot.Location = Foot.Target;
					Foot.Normal = Foot.TargetNormal;
					Foot.Yaw = RestYaw;
					Foot.ToePitch = 0.f;
					HandleFootstep(Side, bMoving ? FMath::Clamp(GaitSpeed / WalkSpeed, 0.35f, 1.4f) : 0.3f);
				}
			}
			else
			{
				// A planted foot stays where it is in the world. If something shoves the suit further than the leg
				// can reach (a hit, a hard stop), the foot gets dragged along.
				FVector Local = ActorTransform.InverseTransformPosition(Foot.Location);
				FVector Offset = Local - Rest;
				const float MaxDrag = MaxFootReach * 1.35f;
				const float Vertical = FMath::Clamp(static_cast<float>(Offset.Z), -40.f, 40.f);
				Offset.Z = 0.f;
				if (Offset.SizeSquared() > FMath::Square(MaxDrag) || !FMath::IsNearlyEqual(Vertical, static_cast<float>(Local.Z - Rest.Z)))
				{
					Local = Rest + Offset.GetClampedToMaxSize(MaxDrag) + FVector(0.f, 0.f, Vertical);
					Foot.Location = ActorTransform.TransformPosition(Local);
				}
			}
		}
	}

	// Hip height: as low as it has to be for each planted leg to reach its foot, plus the bent-knee stance.
	// That makes the body ride highest over a foot and dip as the stride opens up, like a real heavy gait.
	float BodyTarget = -StanceCrouch - (bDodging ? 10.f : 0.f);
	bool bAnyPlanted = false;
	const float MaxLeg = (ThighLength + ShinLength) * 0.98f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FJavelinFoot& Foot = Feet[Side];
		const FVector Local = ActorTransform.InverseTransformPosition(Foot.Location);
		const FVector Rest = GetFootRest(Side);

		Foot.Support = FMath::FInterpTo(Foot.Support, Foot.bSwinging ? 0.f : 1.f, DeltaSeconds, 12.f);
		Foot.Stride = FMath::Clamp(static_cast<float>(Local.X - Rest.X) / FMath::Max(MaxFootReach, 1.f), -1.f, 1.f);

		if (Foot.bSwinging)
		{
			continue;
		}
		bAnyPlanted = true;
		const FVector Hip(0.f, (Side == 0 ? -HipWidth : HipWidth) + GaitSway, HipHeight);
		const float Across = FVector::Dist2D(Hip, Local);
		const float MaxDrop = FMath::Sqrt(FMath::Max(MaxLeg * MaxLeg - Across * Across, 1.f));
		BodyTarget = FMath::Min(BodyTarget, MaxDrop - (HipHeight - static_cast<float>(Local.Z)));
	}
	if (!bAnyPlanted)
	{
		// Both feet off the ground between sprint strides: the body floats up a touch before the next stomp.
		BodyTarget += 2.f + 3.f * SprintAlpha;
	}
	BodyTarget = FMath::Max(BodyTarget, -40.f);
	// Drops quickly, rises slower: the suit's weight falls onto its legs and has to be pushed back up.
	GaitBodyOffset = FMath::FInterpTo(GaitBodyOffset, BodyTarget, DeltaSeconds, BodyTarget < GaitBodyOffset ? 22.f : 9.f);

	// Hips shift over the foot carrying the weight (less at a sprint), tilt down on the side of the swinging leg,
	// and twist so the hip of the leading leg is forward. Standing still they settle with a slow idle shift.
	const float WeightShift = Feet[1].Support - Feet[0].Support;
	const float Idle = bMoving ? 0.f : 0.6f * FMath::Sin(AnimTime * 0.9f);
	const float SwayTarget = HipSway * (1.f - 0.6f * SprintAlpha) * WeightShift + Idle;
	const float TwistTarget = -HipTwist * FMath::Max(MoveAlpha, 0.3f) * 0.5f * (Feet[1].Stride - Feet[0].Stride);
	const float TiltTarget = -HipTilt * WeightShift;
	GaitSway = FMath::FInterpTo(GaitSway, SwayTarget, DeltaSeconds, 8.f);
	GaitHipYaw = FMath::FInterpTo(GaitHipYaw, bDodging ? 0.f : TwistTarget, DeltaSeconds, 10.f);
	GaitHipRoll = FMath::FInterpTo(GaitHipRoll, bDodging ? 0.f : TiltTarget, DeltaSeconds, 10.f);
}

void AJavelinCharacter::ApplyLegIK()
{
	if (LegIKWeight <= 0.f)
	{
		return;
	}

	const FTransform RootTransform = VisualRoot->GetComponentTransform();
	USceneComponent* const Hips[2] = { LeftHip, RightHip };
	USceneComponent* const Knees[2] = { LeftKnee, RightKnee };
	USceneComponent* const Ankles[2] = { LeftAnkle, RightAnkle };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FJavelinFoot& Foot = Feet[Side];
		const FVector Delta = RootTransform.InverseTransformVectorNoScale(Foot.Location - Hips[Side]->GetComponentLocation());
		FQuat HipRotation;
		FQuat KneeRotation;
		SolveLeg(Delta, HipRotation, KneeRotation);

		// ApplyPose already set the animated rotations; blend from those for smooth take-offs and landings.
		Hips[Side]->SetRelativeRotation(FQuat::Slerp(Hips[Side]->GetRelativeRotation().Quaternion(), HipRotation, LegIKWeight));
		Knees[Side]->SetRelativeRotation(FQuat::Slerp(Knees[Side]->GetRelativeRotation().Quaternion(), KneeRotation, LegIKWeight));

		// The foot lies flat on the ground (tilted to the slope) pointing its own way, whatever the leg above is doing.
		const FVector Forward = FRotator(0.f, Foot.Yaw, 0.f).Vector();
		const FQuat FootWorld = FRotationMatrix::MakeFromZX(Foot.Normal, Forward).ToQuat() * FRotator(Foot.ToePitch, 0.f, 0.f).Quaternion();
		const FQuat AnkleRotation = Knees[Side]->GetComponentQuat().Inverse() * FootWorld;
		Ankles[Side]->SetRelativeRotation(FQuat::Slerp(Ankles[Side]->GetRelativeRotation().Quaternion(), AnkleRotation, LegIKWeight));
	}
}

FJavelinSuitPose AJavelinCharacter::ComputeTargetPose(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	AnimTime += DeltaSeconds;

	// Angles in degrees. Positive pitch swings a limb forward; negative knee bend folds the shin back.
	FJavelinSuitPose Pose;
	if (IsSuitDown())
	{
		// Slumped on its knees, head down and arms hanging, jets dead.
		Pose.TorsoPitch = -30.f;
		Pose.HeadPitch = -25.f;
		Pose.ShoulderSpread = 10.f;
		Pose.LegSpread = 8.f;
		Pose.BodyBob = -40.f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Pose.ShoulderPitch[Side] = 15.f;
			Pose.ElbowBend[Side] = 20.f;
			Pose.HipPitch[Side] = 70.f;
			Pose.KneeBend[Side] = -110.f;
		}
		return Pose;
	}

	switch (JavelinMovement->GetJavelinMode())
	{
	case EJavelinMoveMode::Flight:
	{
		// Superhero pose: arms swept back along the body, legs straight, back jets roaring.
		const float Boost = FMath::GetMappedRangeValueClamped(
			FVector2D(JavelinMovement->FlightCruiseSpeed, JavelinMovement->FlightBoostSpeed), FVector2D(0.f, 1.f), GetVelocity().Size());
		Pose.HeadPitch = 35.f;
		Pose.ShoulderSpread = 14.f - 8.f * Boost;
		Pose.LegSpread = 4.f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Pose.ShoulderPitch[Side] = -12.f;
			Pose.ElbowBend[Side] = 8.f;
			Pose.HipPitch[Side] = -4.f;
			Pose.KneeBend[Side] = -8.f;
		}
		Pose.BackFlame = 1.f + 0.8f * Boost;
		Pose.FootFlame = 0.4f;
		break;
	}
	case EJavelinMoveMode::Hover:
		// Legs dangle unevenly, arms out for balance, and the suit bobs on its foot jets.
		Pose.TorsoPitch = -4.f;
		Pose.ShoulderSpread = 22.f;
		Pose.LegSpread = 6.f;
		Pose.ShoulderPitch[0] = 10.f;
		Pose.ShoulderPitch[1] = 4.f;
		Pose.ElbowBend[0] = 35.f;
		Pose.ElbowBend[1] = 25.f;
		Pose.HipPitch[0] = 18.f;
		Pose.HipPitch[1] = -4.f;
		Pose.KneeBend[0] = -40.f;
		Pose.KneeBend[1] = -18.f;
		Pose.BodyBob = 3.f * FMath::Sin(AnimTime * 2.5f);
		Pose.BackFlame = 0.35f;
		Pose.FootFlame = 0.9f;
		break;
	default:
		if (JavelinMovement->IsMovingOnGround())
		{
			// The legs are placed by the gait (UpdateGait and ApplyLegIK). This pose is the upper body: leaning into
			// the stride, arms swinging against the legs, elbows pumping harder at a sprint, and a slow breath at rest.
			const float WalkSpeed = FMath::Max(JavelinMovement->MaxWalkSpeed, 1.f);
			const float MoveAlpha = FMath::Clamp(GaitSpeed / WalkSpeed, 0.f, 1.f);
			const float SprintAlpha = FMath::Clamp((GaitSpeed - WalkSpeed) / FMath::Max(JavelinMovement->SprintSpeed - WalkSpeed, 1.f), 0.f, 1.f);
			const float Breath = FMath::Sin(AnimTime * 1.6f);
			const float Swing = FMath::Lerp(ArmSwing * MoveAlpha, SprintArmSwing, SprintAlpha);

			Pose.TorsoPitch = -5.f * MoveAlpha - 9.f * SprintAlpha + 0.8f * Breath * (1.f - MoveAlpha);
			Pose.HeadPitch = -0.7f * Pose.TorsoPitch;
			Pose.ShoulderSpread = 9.f + Breath * (1.f - MoveAlpha) + 3.f * SprintAlpha;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				// The arm comes forward as the leg on the same side goes back.
				const float Stride = Feet[Side].Stride;
				Pose.ShoulderPitch[Side] = 4.f - Swing * Stride;
				Pose.ElbowBend[Side] = 18.f + 10.f * MoveAlpha + 40.f * SprintAlpha + 14.f * FMath::Max(0.f, -Stride) * MoveAlpha + 2.f * Breath;
				// Used only while blending in or out of the planted-foot legs: a bent-knee stance.
				Pose.HipPitch[Side] = 12.f;
				Pose.KneeBend[Side] = -22.f;
			}
		}
		else
		{
			// Falling: arms flung out, legs braced for landing.
			Pose.ShoulderSpread = 35.f;
			Pose.LegSpread = 8.f;
			Pose.HipPitch[0] = 20.f;
			Pose.HipPitch[1] = -8.f;
			Pose.KneeBend[0] = -30.f;
			Pose.KneeBend[1] = -20.f;
			Pose.AnklePitch[0] = -15.f;
			Pose.AnklePitch[1] = -10.f;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				Pose.ShoulderPitch[Side] = 15.f;
				Pose.ElbowBend[Side] = 30.f;
			}
		}
		break;
	}

	if (IsInCombatStance())
	{
		// Gun arm points where the camera looks, undoing the torso lean; the left hand comes up to steady it.
		const float AimPitch = FMath::Clamp(FRotator::NormalizeAxis(GetControlRotation().Pitch), -70.f, 70.f) - Pose.TorsoPitch;
		Pose.ShoulderSpread = 4.f;
		Pose.ShoulderPitch[1] = 90.f + AimPitch;
		Pose.ElbowBend[1] = 2.f;
		Pose.ShoulderPitch[0] = 60.f + AimPitch;
		Pose.ElbowBend[0] = 45.f;
	}
	else if (Weapon->IsReloading() && !JavelinMovement->IsInFlight())
	{
		// Gun tipped up against the chest while the left hand swaps the magazine.
		const float Fumble = 6.f * FMath::Sin(AnimTime * 14.f);
		Pose.ShoulderPitch[1] = 30.f;
		Pose.ElbowBend[1] = 85.f;
		Pose.ShoulderPitch[0] = 45.f + Fumble;
		Pose.ElbowBend[0] = 75.f;
	}

	// SEMPER-03's ballistic shield: the left arm swings it up in front of the suit.
	if (ShieldRaise > 0.f)
	{
		const float Raise = FMath::SmoothStep(0.f, 1.f, ShieldRaise);
		Pose.ShoulderPitch[0] = FMath::Lerp(Pose.ShoulderPitch[0], 62.f, Raise);
		Pose.ElbowBend[0] = FMath::Lerp(Pose.ElbowBend[0], 58.f, Raise);
	}

	// Grenade throw: the left arm follows through from overhead down toward the target.
	const float SinceThrow = Abilities->GetTimeSinceUsed(EJavelinGear::Grenade);
	if (SinceThrow < GrenadeThrowPoseTime && !JavelinMovement->IsInFlight())
	{
		const float Alpha = SinceThrow / GrenadeThrowPoseTime;
		Pose.ShoulderPitch[0] = FMath::Lerp(160.f, 70.f, Alpha);
		Pose.ElbowBend[0] = FMath::Lerp(40.f, 5.f, Alpha);
	}
	return Pose;
}

void AJavelinCharacter::ApplyPose(const FJavelinSuitPose& Pose)
{
	VisualRoot->SetRelativeLocation(FVector(0.f, Pose.BodySway, Pose.BodyBob));
	TorsoPivot->SetRelativeRotation(FRotator(Pose.TorsoPitch, Pose.TorsoYaw, Pose.TorsoRoll));
	HeadPivot->SetRelativeRotation(FRotator(Pose.HeadPitch, Pose.HeadYaw, Pose.HeadRoll));

	USceneComponent* const Shoulders[2] = { LeftShoulder, RightShoulder };
	USceneComponent* const Elbows[2] = { LeftElbow, RightElbow };
	USceneComponent* const Hips[2] = { LeftHip, RightHip };
	USceneComponent* const Knees[2] = { LeftKnee, RightKnee };
	USceneComponent* const Ankles[2] = { LeftAnkle, RightAnkle };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// Negative roll swings a hanging limb toward +Y (the right side), so mirror it per side to spread outward.
		const float Outward = Side == 0 ? 1.f : -1.f;
		Shoulders[Side]->SetRelativeRotation(FRotator(Pose.ShoulderPitch[Side], 0.f, Pose.ShoulderSpread * Outward));
		Elbows[Side]->SetRelativeRotation(FRotator(Pose.ElbowBend[Side], 0.f, 0.f));
		Hips[Side]->SetRelativeRotation(FRotator(Pose.HipPitch[Side], 0.f, Pose.LegSpread * Outward));
		Knees[Side]->SetRelativeRotation(FRotator(Pose.KneeBend[Side], 0.f, 0.f));
		Ankles[Side]->SetRelativeRotation(FRotator(Pose.AnklePitch[Side], 0.f, 0.f));
	}

	// Flames flicker and stretch with thrust, and hide when the jets are off.
	auto UpdateFlames = [this](const TArray<TObjectPtr<USceneComponent>>& Flames, float Length)
	{
		for (int32 Index = 0; Index < Flames.Num(); ++Index)
		{
			const bool bLit = Length > 0.05f;
			Flames[Index]->SetVisibility(bLit, true);
			if (bLit)
			{
				const float Flicker = 1.f + 0.15f * FMath::Sin(AnimTime * 38.f + Index * 1.7f);
				Flames[Index]->SetRelativeScale3D(FVector(1.f, 1.f, 0.6f * Length * Flicker));
			}
		}
	};
	UpdateFlames(BackFlames, Pose.BackFlame);
	UpdateFlames(FootFlames, Pose.FootFlame);
}
