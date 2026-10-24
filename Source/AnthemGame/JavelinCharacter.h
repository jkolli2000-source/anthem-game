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
class UJavelinAbilityComponent;
class UJavelinHeatComponent;
class UJavelinMovementComponent;
class UJavelinSuitHealthComponent;
class UJavelinWeaponComponent;
class UPointLightComponent;
struct FInputActionValue;

/** Camera framing for one movement state. The camera blends between these. */
USTRUCT(BlueprintType)
struct FJavelinCameraProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ArmLength = 300.f;

	/** Where the camera sits relative to the shoulder pivot. Positive Y puts it over the right shoulder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector SocketOffset = FVector(0.f, 100.f, 30.f);

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
 *  - Left mouse / right trigger: fire. Right mouse / left trigger: hold to aim down sights.
 *  - R / X: reload. You can shoot on the ground, in the air and while hovering, but not while flying.
 *  - Q / left bumper: frag grenade. E / right bumper: homing missiles. Each has its own cooldown,
 *    and like the gun they can't be used while flying.
 *
 * The suit has shields and armor (UJavelinSuitHealthComponent). When the armor
 * runs out the suit goes down, and after a few seconds it reboots back at
 * the start with everything restored.
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
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinMovementComponent* GetJavelinMovement() const;

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinHeatComponent* GetHeatComponent() const { return Heat; }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinWeaponComponent* GetWeapon() const { return Weapon; }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinSuitHealthComponent* GetSuitHealth() const { return SuitHealth; }

	UFUNCTION(BlueprintPure, Category = "Javelin")
	UJavelinAbilityComponent* GetAbilities() const { return Abilities; }

	/** True while the suit is down and rebooting. It can't move or shoot. */
	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool IsSuitDown() const;

	/** Seconds until a downed suit reboots. */
	float GetTimeUntilReboot() const;

	/** Guns are stowed while flying, like in Anthem. Hover to shoot from the air. */
	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool CanFireWeapon() const;

	/** Aiming down sights also needs you not to be sprinting. */
	UFUNCTION(BlueprintPure, Category = "Javelin")
	bool CanAimWeapon() const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UJavelinWeaponComponent> Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UJavelinSuitHealthComponent> SuitHealth;

	/** Gear: the frag grenade and homing missile launcher. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UJavelinAbilityComponent> Abilities;

	/** How long the suit stays down before it reboots. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Health", meta = (ClampMin = "0", Units = "s"))
	float RebootTime = 5.f;

	/** Reboot back where you started instead of where you went down. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Health")
	bool bRebootAtStart = true;

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

	/** End of the gun barrel, held in the right hand. Shots and tracers start here. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	/** Front of the missile pod on the left shoulder. Missiles come out here. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> MissileLauncher;

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

	UPROPERTY(EditAnywhere, Category = "Visuals|Paint")
	FLinearColor MuzzleFlashColor = FLinearColor(1.f, 0.8f, 0.35f);

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

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> GrenadeAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MissileAction;

	/** Mouse look speed multiplier while aiming down sights, for finer aim. */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.05", ClampMax = "1"))
	float AimLookScale = 0.6f;

	/** Right stick turn speed (yaw, pitch) in degrees per second at full tilt, before the hold speed-up. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad")
	FVector2D StickLookRate = FVector2D(130.f, 85.f);

	/** Right stick turn speed (yaw, pitch) at full tilt while aiming down sights. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad")
	FVector2D StickAimLookRate = FVector2D(60.f, 40.f);

	/** Shapes the stick response: higher keeps small nudges slower and more precise. 1 is linear. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad", meta = (ClampMin = "1", ClampMax = "4"))
	float StickResponseExponent = 2.2f;

	/** How far the stick must be pushed sideways (0 to 1) before holding it speeds up turning. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad", meta = (ClampMin = "0.5", ClampMax = "1"))
	float StickRampThreshold = 0.85f;

	/** Seconds the stick must stay pushed before turning starts to speed up. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad")
	float StickRampDelay = 0.2f;

	/** Seconds from the start of the speed-up to the top turn speed. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad")
	float StickRampTime = 0.5f;

	/** Extra yaw speed in degrees per second once the speed-up is complete. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad")
	float StickRampYawBonus = 120.f;

	/** Extra yaw speed once the speed-up is complete while aiming down sights. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad")
	float StickAimRampYawBonus = 50.f;

	/** Gamepad turning slows to this fraction while the crosshair is over an enemy and you are aiming. */
	UPROPERTY(EditAnywhere, Category = "Input|Gamepad", meta = (ClampMin = "0.1", ClampMax = "1"))
	float StickAimFriction = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile GroundCamera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile SprintCamera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile HoverCamera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile FlightCamera;

	/** Over-the-shoulder zoom while aiming down sights. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	FJavelinCameraProfile AimCamera;

	/** Aiming zooms in faster than the other camera changes. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float AimCameraBlendSpeed = 14.f;

	/** How quickly the camera blends to a new profile. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraBlendSpeed = 4.f;

	/** Height of the camera's pivot above the middle of the capsule, about shoulder height, so looking up and down turns around the shoulders. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraPivotHeight = 60.f;

	/** Hide the suit when a wall pushes the camera closer than this, so it never fills the screen. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float HideSuitCameraDistance = 90.f;

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
	void LookStickCompleted();
	void OnJumpPressed();
	void OnJumpReleased();
	void OnThrustPressed();
	void OnThrustReleased();
	void OnLaunchPressed();
	void OnFirePressed();
	void OnFireReleased();
	void OnAimPressed();
	void OnAimReleased();
	void OnReloadPressed();
	void OnGrenadePressed();
	void OnMissilePressed();

	/** True while aiming or shortly after firing: the suit turns to face where the camera looks. */
	bool IsInCombatStance() const;
	void UpdateCombatFacing();
	void UpdateMuzzleFlash();

	UFUNCTION()
	void HandleOverheated();

	UFUNCTION()
	void HandleSuitDown();

	void Reboot();

	FTimerHandle RebootTimer;
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;

	void UpdateCamera(float DeltaSeconds);
	void UpdateVisuals(float DeltaSeconds);

	/** Shared by every thruster glow and flame, so they all change color together. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThrusterMaterial;

	/** Builds the rifle in the right hand. Called from BuildSuit. */
	void BuildGun(USceneComponent* Hand, UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere);

	/** Builds the missile pod over the left shoulder. Called from BuildSuit. */
	void BuildMissilePod(USceneComponent* Torso, UStaticMesh* Cube, UStaticMesh* Cylinder);

	FJavelinSuitPose CurrentPose;
	float WalkPhase = 0.f;
	float AnimTime = 0.f;

	float VisualPitch = 0.f;
	float VisualRoll = 0.f;
	float LastYaw = 0.f;

	/** How long the right stick has been held far enough sideways to speed up turning. */
	float StickHoldTime = 0.f;
};
