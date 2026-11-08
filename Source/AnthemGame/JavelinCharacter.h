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

	/** Placeholder suit made of basic shapes. Tilted for flight pitch and banking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TorsoMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LegsMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VisorMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ThrusterMesh;

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

private:
	UStaticMeshComponent* CreateVisualPart(FName Name, UStaticMesh* PartMesh, const FVector& Location, const FVector& Scale);
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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThrusterMaterial;

	float VisualPitch = 0.f;
	float VisualRoll = 0.f;
	float LastYaw = 0.f;
};
