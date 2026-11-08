#include "JavelinCharacter.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"
#include "JavelinWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
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

	void SetPartColor(UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
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

	GroundCamera = MakeCameraProfile(350.f, FVector(0.f, 60.f, 60.f), 90.f, 15.f);
	SprintCamera = MakeCameraProfile(390.f, FVector(0.f, 50.f, 50.f), 98.f, 12.f);
	HoverCamera = MakeCameraProfile(300.f, FVector(0.f, 80.f, 70.f), 85.f, 15.f);
	FlightCamera = MakeCameraProfile(650.f, FVector(0.f, 0.f, 110.f), 100.f, 8.f);
	AimCamera = MakeCameraProfile(170.f, FVector(0.f, 75.f, 65.f), 60.f, 30.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagMaxDistance = 400.f;
	CameraBoom->TargetArmLength = GroundCamera.ArmLength;
	CameraBoom->SocketOffset = GroundCamera.SocketOffset;
	CameraBoom->CameraLagSpeed = GroundCamera.LagSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = GroundCamera.FieldOfView;

	Heat = CreateDefaultSubobject<UJavelinHeatComponent>(TEXT("Heat"));
	Weapon = CreateDefaultSubobject<UJavelinWeaponComponent>(TEXT("Weapon"));

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
	CreatePart(TEXT("ChestCore"), TorsoPivot, Cylinder, FVector(29.f, 0.f, 46.f), FVector(0.14f, 0.14f, 0.04f), PaintAccent, FRotator(90.f, 0.f, 0.f));
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

	// Helmet
	HeadPivot = CreatePivot(TEXT("HeadPivot"), TorsoPivot, FVector(4.f, 0.f, 70.f));
	CreatePart(TEXT("Neck"), HeadPivot, Cylinder, FVector(0.f, 0.f, 2.f), FVector(0.18f, 0.18f, 0.1f), PaintFrame);
	CreatePart(TEXT("Helmet"), HeadPivot, Sphere, FVector(0.f, 0.f, 16.f), FVector(0.34f, 0.3f, 0.36f), PaintPrimary);
	CreatePart(TEXT("HelmetCrest"), HeadPivot, Cube, FVector(-2.f, 0.f, 32.f), FVector(0.3f, 0.06f, 0.06f), PaintTrim);
	CreatePart(TEXT("Faceplate"), HeadPivot, Cube, FVector(13.f, 0.f, 12.f), FVector(0.1f, 0.24f, 0.16f), PaintSecondary);
	CreatePart(TEXT("Visor"), HeadPivot, Cube, FVector(16.f, 0.f, 19.f), FVector(0.06f, 0.24f, 0.06f), PaintAccent);

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
		USceneComponent* Hip = CreatePivot(PartName(TEXT("Hip")), VisualRoot, FVector(0.f, 19.f * S, -14.f));
		CreatePart(PartName(TEXT("Thigh")), Hip, Cube, FVector(0.f, 0.f, -20.f), FVector(0.26f, 0.24f, 0.4f), PaintPrimary);
		CreatePart(PartName(TEXT("ThighPlate")), Hip, Cube, FVector(0.f, 13.f * S, -16.f), FVector(0.2f, 0.04f, 0.28f), PaintTrim);

		USceneComponent* Knee = CreatePivot(PartName(TEXT("Knee")), Hip, FVector(0.f, 0.f, -40.f));
		CreatePart(PartName(TEXT("KneePad")), Knee, Sphere, FVector(12.f, 0.f, 1.f), FVector(0.17f, 0.18f, 0.18f), PaintSecondary);
		CreatePart(PartName(TEXT("Shin")), Knee, Cube, FVector(0.f, 0.f, -19.f), FVector(0.24f, 0.22f, 0.36f), PaintSecondary);
		CreatePart(PartName(TEXT("CalfJet")), Knee, Cylinder, FVector(-13.f, 0.f, -22.f), FVector(0.12f, 0.12f, 0.2f), PaintFrame);
		CreatePart(PartName(TEXT("Foot")), Knee, Cube, FVector(7.f, 0.f, -37.f), FVector(0.34f, 0.2f, 0.1f), PaintFrame);
		CreatePart(PartName(TEXT("SoleGlow")), Knee, Cylinder, FVector(-2.f, 0.f, -42.f), FVector(0.12f, 0.12f, 0.01f), PaintGlow);
		FootFlames.Add(CreateFlame(PartName(TEXT("FootFlame")), Knee, Cone, FVector(-2.f, 0.f, -42.5f), 0.11f));

		(Side == 0 ? LeftShoulder : RightShoulder) = Shoulder;
		(Side == 0 ? LeftElbow : RightElbow) = Elbow;
		(Side == 0 ? LeftHip : RightHip) = Hip;
		(Side == 0 ? LeftKnee : RightKnee) = Knee;
	}
}

void AJavelinCharacter::BuildGun(USceneComponent* Hand, UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere)
{
	// An assault rifle gripped in the right hand, its barrel running on down the line of the forearm.
	// In the elbow's space -Z runs down the forearm and +X is the top of the gun when the arm is raised to aim.
	CreatePart(TEXT("GunReceiver"), Hand, Cube, FVector(3.f, 0.f, -58.f), FVector(0.16f, 0.09f, 0.4f), PaintFrame);
	CreatePart(TEXT("GunTrim"), Hand, Cube, FVector(3.f, 4.8f, -58.f), FVector(0.1f, 0.01f, 0.3f), PaintTrim);
	CreatePart(TEXT("GunMagazine"), Hand, Cube, FVector(-6.f, 0.f, -64.f), FVector(0.1f, 0.06f, 0.14f), PaintSecondary, FRotator(-10.f, 0.f, 0.f));
	CreatePart(TEXT("GunSight"), Hand, Cube, FVector(12.f, 0.f, -52.f), FVector(0.04f, 0.04f, 0.1f), PaintAccent);
	CreatePart(TEXT("GunShroud"), Hand, Cylinder, FVector(6.f, 0.f, -84.f), FVector(0.08f, 0.08f, 0.12f), PaintSecondary);
	CreatePart(TEXT("GunBarrel"), Hand, Cylinder, FVector(6.f, 0.f, -95.f), FVector(0.05f, 0.05f, 0.24f), PaintFrame);

	Muzzle = CreatePivot(TEXT("Muzzle"), Hand, FVector(6.f, 0.f, -108.f));

	MuzzleFlash = CreatePart(TEXT("MuzzleFlash"), Muzzle, Sphere, FVector(0.f, 0.f, -8.f), FVector(0.14f, 0.14f, 0.24f), PaintMuzzle);
	MuzzleFlash->SetCastShadow(false);
	MuzzleFlash->SetVisibility(false);

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

UJavelinMovementComponent* AJavelinCharacter::GetJavelinMovement() const
{
	return CastChecked<UJavelinMovementComponent>(GetCharacterMovement());
}

void AJavelinCharacter::BeginPlay()
{
	Super::BeginPlay();

	struct FPaintSlot
	{
		FName Tag;
		FLinearColor Color;
	};
	const FPaintSlot Paints[] = {
		{ PaintPrimary, PrimaryColor },
		{ PaintSecondary, SecondaryColor },
		{ PaintTrim, TrimColor },
		{ PaintFrame, FrameColor },
		{ PaintAccent, AccentColor },
		{ PaintMuzzle, MuzzleFlashColor },
	};

	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part->ComponentHasTag(PaintGlow))
		{
			// One shared material so every thruster glow and flame changes color together.
			if (!ThrusterMaterial)
			{
				ThrusterMaterial = UMaterialInstanceDynamic::Create(Part->GetMaterial(0), this);
			}
			Part->SetMaterial(0, ThrusterMaterial);
			continue;
		}
		for (const FPaintSlot& Paint : Paints)
		{
			if (Part->ComponentHasTag(Paint.Tag))
			{
				SetPartColor(Part, Paint.Color);
				break;
			}
		}
	}

	Heat->OnOverheated.AddDynamic(this, &AJavelinCharacter::HandleOverheated);
	Weapon->SetMuzzle(Muzzle);
	MuzzleLight->SetLightColor(MuzzleFlashColor);
	LastYaw = GetActorRotation().Yaw;
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
	LaunchAction = MakeAction(TEXT("IA_Launch"), EInputActionValueType::Boolean);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimAction = MakeAction(TEXT("IA_Aim"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);

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
	Map(LookStickAction, EKeys::Gamepad_Right2D, false, false, true, true);

	Map(JumpAction, EKeys::SpaceBar);
	Map(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	Map(ThrustAction, EKeys::LeftShift);
	Map(ThrustAction, EKeys::Gamepad_LeftThumbstick);
	Map(LaunchAction, EKeys::F);
	Map(LaunchAction, EKeys::Gamepad_FaceButton_Top);
	Map(FireAction, EKeys::LeftMouseButton);
	Map(FireAction, EKeys::Gamepad_RightTrigger);
	Map(AimAction, EKeys::RightMouseButton);
	Map(AimAction, EKeys::Gamepad_LeftTrigger);
	Map(ReloadAction, EKeys::R);
	Map(ReloadAction, EKeys::Gamepad_FaceButton_Left);

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
}

void AJavelinCharacter::Move(const FInputActionValue& Value)
{
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
}

void AJavelinCharacter::MoveCompleted()
{
	GetJavelinMovement()->SetFlightInput(FVector2D::ZeroVector);
}

void AJavelinCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>() * (Weapon->IsAiming() ? AimLookScale : 1.f);
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AJavelinCharacter::LookStick(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>() * StickLookRate * GetWorld()->GetDeltaSeconds() * (Weapon->IsAiming() ? AimLookScale : 1.f);
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

bool AJavelinCharacter::TryStartFlight()
{
	return !Heat->IsOverheated() && GetJavelinMovement()->StartFlight();
}

bool AJavelinCharacter::TryStartHover()
{
	return !Heat->IsOverheated() && GetJavelinMovement()->StartHover();
}

void AJavelinCharacter::OnJumpPressed()
{
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
	GetJavelinMovement()->SetSprinting(false);
}

void AJavelinCharacter::OnLaunchPressed()
{
	if (!GetJavelinMovement()->IsInFlight())
	{
		TryStartFlight();
	}
}

void AJavelinCharacter::OnFirePressed()
{
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
	GetJavelinMovement()->SetSprinting(false);
	Weapon->SetWantsToAim(true);
}

void AJavelinCharacter::OnAimReleased()
{
	Weapon->SetWantsToAim(false);
}

void AJavelinCharacter::OnReloadPressed()
{
	Weapon->StartReload();
}

bool AJavelinCharacter::CanFireWeapon() const
{
	return !GetJavelinMovement()->IsInFlight();
}

bool AJavelinCharacter::CanAimWeapon() const
{
	return CanFireWeapon() && !GetJavelinMovement()->IsSprinting();
}

bool AJavelinCharacter::IsInCombatStance() const
{
	return CanFireWeapon() && (Weapon->IsAiming() || Weapon->GetTimeSinceLastShot() < CombatStanceTime);
}

void AJavelinCharacter::UpdateCombatFacing()
{
	// On foot and falling the suit normally turns toward where it moves. In combat it faces the crosshair instead,
	// so you can strafe while shooting. Hover already faces the camera, and flight steers itself.
	UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	const bool bFaceAim = IsInCombatStance();
	JavelinMovement->bOrientRotationToMovement = !bFaceAim;
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
		MuzzleFlash->SetRelativeScale3D(FVector(0.14f, 0.14f, 0.24f) * Size);
	}
}

void AJavelinCharacter::HandleOverheated()
{
	GetJavelinMovement()->StopFlightOrHover();
}

void AJavelinCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCombatFacing();
	UpdateMuzzleFlash();
	UpdateCamera(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
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
}

void AJavelinCharacter::UpdateVisuals(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();

	const float Yaw = GetActorRotation().Yaw;
	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
	LastYaw = Yaw;

	// In flight the suit lies along its velocity (back up, head forward) and banks into turns.
	float TargetPitch = 0.f;
	float TargetRoll = 0.f;
	if (JavelinMovement->IsInFlight())
	{
		TargetPitch = -90.f + GetVelocity().Rotation().Pitch;
		TargetRoll = FMath::Clamp(YawRate * FlightBankFactor, -MaxFlightBank, MaxFlightBank);
	}

	VisualPitch = FMath::FInterpTo(VisualPitch, TargetPitch, DeltaSeconds, VisualTiltSpeed);
	VisualRoll = FMath::FInterpTo(VisualRoll, TargetRoll, DeltaSeconds, VisualTiltSpeed);

	// Pitch first, then roll around the flight direction.
	const FQuat Tilt = FRotator(0.f, 0.f, VisualRoll).Quaternion() * FRotator(VisualPitch, 0.f, 0.f).Quaternion();
	VisualRoot->SetRelativeRotation(Tilt);

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
	}
	ApplyPose(CurrentPose);

	// Thrusters glow blue when active and shift to red as heat builds.
	if (ThrusterMaterial)
	{
		FLinearColor Color = ThrusterIdleColor;
		if (JavelinMovement->GetJavelinMode() != EJavelinMoveMode::None)
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

FJavelinSuitPose AJavelinCharacter::ComputeTargetPose(float DeltaSeconds)
{
	const UJavelinMovementComponent* JavelinMovement = GetJavelinMovement();
	AnimTime += DeltaSeconds;

	// Angles in degrees. Positive pitch swings a limb forward; negative knee bend folds the shin back.
	FJavelinSuitPose Pose;
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
			// Walk cycle driven by ground speed, so feet roughly match the distance covered.
			const float Speed = GetVelocity().Size2D();
			const float Stride = FMath::Clamp(Speed / FMath::Max(JavelinMovement->MaxWalkSpeed, 1.f), 0.f, 1.4f);
			WalkPhase = FMath::Fmod(WalkPhase + DeltaSeconds * UE_TWO_PI * Speed / FMath::Max(WalkStrideLength, 1.f), UE_TWO_PI);
			const float Swing = FMath::Sin(WalkPhase);
			const float Breath = FMath::Sin(AnimTime * 1.6f);

			Pose.TorsoPitch = -8.f * Stride;
			Pose.HeadPitch = 6.f * Stride;
			Pose.ShoulderSpread = 8.f + Breath;
			Pose.BodyBob = -3.f * Stride * FMath::Abs(FMath::Cos(WalkPhase));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float SideSwing = Side == 0 ? Swing : -Swing;
				const float Lift = FMath::Max(0.f, FMath::Cos(WalkPhase + (Side == 0 ? 0.f : UE_PI)));
				Pose.HipPitch[Side] = 28.f * Stride * SideSwing;
				Pose.KneeBend[Side] = -4.f - 45.f * Stride * Lift;
				Pose.ShoulderPitch[Side] = -22.f * Stride * SideSwing;
				Pose.ElbowBend[Side] = 15.f + 20.f * Stride + 2.f * Breath;
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
	return Pose;
}

void AJavelinCharacter::ApplyPose(const FJavelinSuitPose& Pose)
{
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, Pose.BodyBob));
	TorsoPivot->SetRelativeRotation(FRotator(Pose.TorsoPitch, 0.f, 0.f));
	HeadPivot->SetRelativeRotation(FRotator(Pose.HeadPitch, 0.f, 0.f));

	USceneComponent* const Shoulders[2] = { LeftShoulder, RightShoulder };
	USceneComponent* const Elbows[2] = { LeftElbow, RightElbow };
	USceneComponent* const Hips[2] = { LeftHip, RightHip };
	USceneComponent* const Knees[2] = { LeftKnee, RightKnee };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// Negative roll swings a hanging limb toward +Y (the right side), so mirror it per side to spread outward.
		const float Outward = Side == 0 ? 1.f : -1.f;
		Shoulders[Side]->SetRelativeRotation(FRotator(Pose.ShoulderPitch[Side], 0.f, Pose.ShoulderSpread * Outward));
		Elbows[Side]->SetRelativeRotation(FRotator(Pose.ElbowBend[Side], 0.f, 0.f));
		Hips[Side]->SetRelativeRotation(FRotator(Pose.HipPitch[Side], 0.f, Pose.LegSpread * Outward));
		Knees[Side]->SetRelativeRotation(FRotator(Pose.KneeBend[Side], 0.f, 0.f));
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
