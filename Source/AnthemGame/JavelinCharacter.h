#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JavelinCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UInputAction;
class UInputMappingContext;
class UJavelinHeatComponent;
class UJavelinMovementComponent;
struct FInputActionValue;

/** Camera framing for one movement state. The camera blends between these. */
USTRUCT(BlueprintType)
struct FJavelinCameraProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ArmLength = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector SocketOffset = FVector(0.f, 60.f, 60.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FieldOfView = 90.f;

	/** Lower values let the camera trail further behind at speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float LagSpeed = 15.f;
};

/** Joint angles for the suit, in degrees. Index 0 is the left side, 1 the right. */
struct FJavelinSuitPose
{
	float TorsoPitch = 0.f;
	float HeadPitch = 0.f;
	float ShoulderPitch[2] = {};
	float ShoulderSpread = 0.f;
	float ElbowBend[2] = {};
	float HipPitch[2] = {};
	float LegSpread = 0.f;
	float KneeBend[2] = {};
	float BodyBob = 0.f;
	float BackFlame = 0.f;
	float FootFlame = 0.f;
};

/**
 * The player's javelin (Ranger for now).
 *
 * Controls (keyboard and mouse / gamepad):
 *  - Move: WASD / left stick. Look: mouse / right stick.
 *  - Space / A: jump on the ground, hover in the air, stop hovering, or switch flight to hover.
 *  - Left Shift / left stick click: hold to sprint on the ground, start flying in the air, stop flying in flight.
 *  - F / Y: take off into flight from anywhere.
 */
UCLASS()
class ANTHEMGAME_API AJavelinCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AJavelinCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinMovementComponent* GetJavelinMovement() const;

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinHeatComponent* GetHeatComponent() const { return Heat; }

	UFUNCTION(BlueprintCallable, Category = "Javelin")
	bool TryStartFlight();

	UFUNCTION(BlueprintCallable, Category = "Javelin")
	bool TryStartHover();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UJavelinHeatComponent> Heat;

	/** Root of the mech suit, built from basic shapes. Tilted for flight pitch and banking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	// Joints of the suit. Each one is a pivot that the procedural animation rotates.
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> TorsoPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> HeadPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> LeftShoulder;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> RightShoulder;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> LeftElbow;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> RightElbow;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> LeftHip;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> RightHip;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> LeftKnee;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> RightKnee;

	/** Jet flames on the back pack. Scaled along their length by thrust. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TArray<TObjectPtr<USceneComponent>> BackFlames;

	/** Jet flames under the feet, used mostly for hovering. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TArray<TObjectPtr<USceneComponent>> FootFlames;

	UPROPERTY(EditAnywhere, Category = "Visuals|Paint")
	FLinearColor PrimaryColor = FLinearColor(0.16f, 0.22f, 0.32f);

	UPROPERTY(EditAnywhere, Category = "Visuals|Paint")
	FLinearColor SecondaryColor = FLinearColor(0.42f, 0.44f, 0.48f);

	UPROPERTY(EditAnywhere, Category = "Visuals|Paint")
	FLinearColor TrimColor = FLinearColor(0.85f, 0.55f, 0.12f);

	UPROPERTY(EditAnywhere, Category = "Visuals|Paint")
	FLinearColor FrameColor = FLinearColor(0.05f, 0.05f, 0.06f);

	/** Visor and chest core. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Paint")
	FLinearColor AccentColor = FLinearColor(0.1f, 0.8f, 1.f);

	/** Leave empty to use the built-in key bindings. Set these to use your own input assets instead. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ThrustAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LaunchAction;

	/** Gamepad look speed in degrees per second. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float StickLookRate = 160.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile GroundCamera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile SprintCamera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile HoverCamera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile FlightCamera;

	/** How quickly the camera blends to a new profile. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraBlendSpeed = 4.f;

	/** Extra field of view at full flight boost, for a sense of speed. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float FlightBoostExtraFOV = 12.f;

	/** Degrees of bank per degree-per-second of turning. */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	float FlightBankFactor = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Visuals", meta = (Units = "deg"))
	float MaxFlightBank = 60.f;

	UPROPERTY(EditAnywhere, Category = "Visuals")
	float VisualTiltSpeed = 8.f;

	/** Distance covered by one full walk cycle (two steps). */
	UPROPERTY(EditAnywhere, Category = "Visuals|Animation", meta = (Units = "cm"))
	float WalkStrideLength = 170.f;

	/** How quickly limbs blend between poses. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Animation")
	float PoseBlendSpeed = 10.f;

private:
	USceneComponent* CreatePivot(FName Name, USceneComponent* Parent, const FVector& Location);
	UStaticMeshComponent* CreatePart(FName Name, USceneComponent* Parent, UStaticMesh* PartMesh, const FVector& Location, const FVector& Scale, FName Paint, const FRotator& Rotation = FRotator::ZeroRotator);
	USceneComponent* CreateFlame(FName Name, USceneComponent* Parent, UStaticMesh* ConeMesh, const FVector& Location, float Width);
	void BuildSuit();
	FJavelinSuitPose ComputeTargetPose(float DeltaSeconds);
	void ApplyPose(const FJavelinSuitPose& Pose);
	void CreateDefaultInput();

	void Move(const FInputActionValue& Value);
	void MoveCompleted();
	void Look(const FInputActionValue& Value);
	void LookStick(const FInputActionValue& Value);
	void OnJumpPressed();
	void OnJumpReleased();
	void OnThrustPressed();
	void OnThrustReleased();
	void OnLaunchPressed();

	UFUNCTION()
	void HandleOverheated();

	void UpdateCamera(float DeltaSeconds);
	void UpdateVisuals(float DeltaSeconds);

	/** Shared by every thruster glow and flame, so they all change color together. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThrusterMaterial;

	FJavelinSuitPose CurrentPose;
	float WalkPhase = 0.f;
	float AnimTime = 0.f;

	float VisualPitch = 0.f;
	float VisualRoll = 0.f;
	float LastYaw = 0.f;
};
