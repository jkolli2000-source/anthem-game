#include "JavelinCharacter.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
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

	// Placeholder suit built from the engine's basic shapes, so the project needs no art assets yet.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	TorsoMesh = CreateVisualPart(TEXT("Torso"), CubeMesh.Object, FVector(0.f, 0.f, 25.f), FVector(0.55f, 0.75f, 0.7f));
	LegsMesh = CreateVisualPart(TEXT("Legs"), CylinderMesh.Object, FVector(0.f, 0.f, -50.f), FVector(0.4f, 0.6f, 0.85f));
	HeadMesh = CreateVisualPart(TEXT("Head"), SphereMesh.Object, FVector(0.f, 0.f, 78.f), FVector(0.35f));
	VisorMesh = CreateVisualPart(TEXT("Visor"), CubeMesh.Object, FVector(15.f, 0.f, 80.f), FVector(0.1f, 0.25f, 0.08f));
	ThrusterMesh = CreateVisualPart(TEXT("Thruster"), CubeMesh.Object, FVector(-42.f, 0.f, 30.f), FVector(0.3f, 0.6f, 0.55f));
}

UStaticMeshComponent* AJavelinCharacter::CreateVisualPart(FName Name, UStaticMesh* PartMesh, const FVector& Location, const FVector& Scale)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(VisualRoot);
	Part->SetStaticMesh(PartMesh);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	return Part;
}

UJavelinMovementComponent* AJavelinCharacter::GetJavelinMovement() const
{
	return CastChecked<UJavelinMovementComponent>(GetCharacterMovement());
}

void AJavelinCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetPartColor(TorsoMesh, FLinearColor(0.12f, 0.16f, 0.22f));
	SetPartColor(LegsMesh, FLinearColor(0.08f, 0.08f, 0.1f));
	SetPartColor(HeadMesh, FLinearColor(0.18f, 0.2f, 0.24f));
	SetPartColor(VisorMesh, FLinearColor(0.1f, 0.8f, 1.f));
	ThrusterMaterial = ThrusterMesh->CreateDynamicMaterialInstance(0);

	Heat->OnOverheated.AddDynamic(this, &AJavelinCharacter::HandleOverheated);
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
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AJavelinCharacter::LookStick(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>() * StickLookRate * GetWorld()->GetDeltaSeconds();
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

void AJavelinCharacter::HandleOverheated()
{
	GetJavelinMovement()->StopFlightOrHover();
}

void AJavelinCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, Target->ArmLength, DeltaSeconds, CameraBlendSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, Target->SocketOffset, DeltaSeconds, CameraBlendSpeed);
	CameraBoom->CameraLagSpeed = FMath::FInterpTo(CameraBoom->CameraLagSpeed, Target->LagSpeed, DeltaSeconds, CameraBlendSpeed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, Target->FieldOfView + ExtraFOV, DeltaSeconds, CameraBlendSpeed));
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

	// Thruster glows blue when active and shifts to red as heat builds.
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
