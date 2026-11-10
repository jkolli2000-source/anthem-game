#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "JavelinSuitTypes.h"
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
class UPrimitiveComponent;
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
	/** Torso twist and side tilt on top of the hips, for counter-rotating the shoulders against the stride. */
	float TorsoYaw = 0.f;
	float TorsoRoll = 0.f;
	float HeadPitch = 0.f;
	/** The head turns and tilts against the body so it stays steady while the suit stomps around. */
	float HeadYaw = 0.f;
	float HeadRoll = 0.f;
	float ShoulderPitch[2] = {};
	float ShoulderSpread = 0.f;
	float ElbowBend[2] = {};
	float HipPitch[2] = {};
	float LegSpread = 0.f;
	float KneeBend[2] = {};
	/** Foot tilt when the legs aren't planted on the ground (falling, kneeling). Positive lifts the toes. */
	float AnklePitch[2] = {};
	float BodyBob = 0.f;
	/** Hips shifting sideways over the planted foot, in cm. Positive is to the suit's right. */
	float BodySway = 0.f;
	float BackFlame = 0.f;
	float FootFlame = 0.f;
};

/** A simple damped spring, for bounces that overshoot a little and settle: landings, camera dips, flinches. */
struct FJavelinSpring
{
	float Value = 0.f;
	float Speed = 0.f;

	void Update(float DeltaSeconds, float Stiffness, float Damping)
	{
		// Small steps keep stiff springs stable at low frame rates.
		float Remaining = FMath::Min(DeltaSeconds, 0.1f);
		while (Remaining > 0.f)
		{
			const float Step = FMath::Min(Remaining, 1.f / 120.f);
			Speed += (-Stiffness * Value - Damping * Speed) * Step;
			Value += Speed * Step;
			Remaining -= Step;
		}
	}
};

/** One foot of the walking gait. Positions are in world space so planted feet stay exactly where they landed. */
struct FJavelinFoot
{
	/** Ankle position. */
	FVector Location = FVector::ZeroVector;
	/** Ground normal under the foot, so it sits flat on slopes. */
	FVector Normal = FVector::UpVector;
	/** Which way the foot points (world yaw). */
	float Yaw = 0.f;

	bool bSwinging = false;
	FVector SwingStart = FVector::ZeroVector;
	float SwingStartYaw = 0.f;
	/** 0 at lift-off to 1 at touch-down. */
	float SwingProgress = 0.f;
	float SwingHeight = 0.f;
	/** Where the swinging foot will land, refreshed every frame. */
	FVector Target = FVector::ZeroVector;
	FVector TargetNormal = FVector::UpVector;

	/** 1 while the foot carries weight, 0 while it swings. Smoothed; drives the hip sway. */
	float Support = 1.f;
	/** How far the foot is ahead (+1) or behind (-1) its rest spot. Drives the arm swing and hip twist. */
	float Stride = 0.f;
	/** Toe tilt during the swing: down as it pushes off, up just before it lands. */
	float ToePitch = 0.f;
};

/**
 * The player's javelin (Ranger for now).
 *
 * Controls (keyboard and mouse / gamepad):
 *  - Move: WASD / left stick. Look: mouse / right stick.
 *  - Space / A: jump on the ground, hover in the air, stop hovering, or switch flight to hover.
 *  - Left Shift: hold to sprint on the ground. Left stick click: sprint until you stop moving or click again.
 *    Either one starts flying in the air and stops flying in flight.
 *  - Left Ctrl or C / B: dodge the way you're moving (backward with no input), on the ground or hovering.
 *  - F / Y: take off into flight from anywhere.
 *  - Left mouse / right trigger: fire. Right mouse / left trigger: hold to aim down sights.
 *  - R / X: reload. You can shoot on the ground, in the air and while hovering, but not while flying.
 *  - Q / left bumper: frag grenade. E / right bumper: homing missiles. Each has its own cooldown,
 *    and like the gun they can't be used while flying.
 *
 * Walking is procedural: each foot plants on the ground and stays put while
 * the body moves over it, then lifts and swings to where it will land next
 * (two-bone leg IK). The hips sway over the planted foot, twist with the
 * stride while the shoulders counter-twist, the arms swing opposite the legs,
 * the head stays steady, and each footfall drops the body a little. Turning
 * on the spot shuffles the feet, and stopping takes a settling step.
 *
 * The suit is meant to feel heavy: footsteps thump the camera, landings
 * compress the legs and dip the camera (hard ones kick up dust and slow you
 * for a moment), the gun kicks the arms and camera, hits make the suit
 * flinch, and the camera shakes with impacts, explosions and fast flight.
 * Gamepads rumble along with all of it.
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

	/** Shakes the camera. 0.1 is a nudge, 0.5 a big hit, 1 the most it shakes. Shakes add up and fade out. */
	UFUNCTION(BlueprintCallable, Category = "Javelin|Feel")
	void AddCameraTrauma(float Amount);

	/** Rumbles a gamepad, if one is being used. Heavy uses the big motors too. */
	void PlayRumble(float Intensity, float Duration, bool bHeavy);

	// ---- Suits: Ranger, USMC EXO-07 and SEMPER-03 ----

	/**
	 * Switches to another suit: its look, gun, stats, gear tuning and visor modes. Only works on the ground
	 * with the suit up. Shields and armor keep the same share of the new suit's maximum. Returns false if
	 * the suit can't be changed right now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Javelin|Suit")
	bool SetSuitClass(EJavelinSuitClass NewClass);

	UFUNCTION(BlueprintPure, Category = "Javelin|Suit")
	EJavelinSuitClass GetSuitClass() const { return SuitClass; }

	/** Name, role and numbers of the suit being worn. */
	const FJavelinSuitStats& GetSuitStats() const { return ActiveStats; }

	/** True when the suit can be changed: standing on the ground and not down. */
	bool CanChangeSuit() const;
	float GetTimeSinceSuitChanged() const;
	float GetTimeSinceSuitChangeDenied() const;

	/** The visor mode showing right now, including Combat and Alert, which switch on by themselves. */
	const FJavelinVisorMode& GetVisorMode() const;
	/** The mode picked with V. Its effect stays on even while Combat or Alert is showing. */
	const FJavelinVisorMode& GetSelectedVisorMode() const;
	float GetTimeSinceVisorChanged() const;

	/** Enemies spot the suit from this share of their usual sight range. 0.5 in EXO-07 stealth, otherwise 1. */
	float GetDetectionRangeScale() const;

	/** True while SEMPER-03's Scan visor is on: the HUD shows every enemy nearby. */
	bool IsScanVisorActive() const;

	/** SEMPER-03 carries a ballistic shield. */
	UFUNCTION(BlueprintPure, Category = "Javelin|Suit")
	bool HasBallisticShield() const { return SuitClass == EJavelinSuitClass::Semper03; }

	/** True while the ballistic shield is up far enough to block hits from the front. */
	UFUNCTION(BlueprintPure, Category = "Javelin|Suit")
	bool IsShieldRaised() const;

	/** 0 to 1. The shield breaks at 0 and has to recover before it can be raised again. */
	float GetShieldIntegrityPercent() const;
	bool IsShieldBroken() const;
	float GetTimeSinceShieldBlock() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

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

	/** The suit you start in. Change suits in game with 1, 2 and 3 (D-pad left and right on a gamepad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Javelin|Suit")
	EJavelinSuitClass SuitClass = EJavelinSuitClass::Exo07;

	/** How much damage SEMPER-03's ballistic shield soaks up before it breaks. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "1"))
	float ShieldMaxIntegrity = 650.f;

	/** The shield blocks hits coming from within this angle of straight ahead. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
	float ShieldBlockAngle = 70.f;

	/** Seconds without a hit before the shield starts to recover. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "0", Units = "s"))
	float ShieldRecoverDelay = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "0"))
	float ShieldRecoverPerSecond = 160.f;

	/** How long a broken shield can't be raised. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "0", Units = "s"))
	float ShieldBrokenTime = 4.f;

	/** Walking speed with the shield up, as a share of normal. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ShieldMoveSpeedScale = 0.55f;

	/** How fast the shield swings up and down, in full swings per second. */
	UPROPERTY(EditAnywhere, Category = "Javelin|Suit|Ballistic Shield", meta = (ClampMin = "0.5"))
	float ShieldRaiseSpeed = 5.f;

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

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> LeftAnkle;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> RightAnkle;

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

	// The two guns. Muzzle and MuzzleFlash above point at whichever one the current suit carries.
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> RifleMuzzle;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<UStaticMeshComponent> RifleMuzzleFlash;

	/** End of EXO-07's HMG-88. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> HmgMuzzle;

	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<UStaticMeshComponent> HmgMuzzleFlash;

	/** The HMG-88's rotary barrels, spun as it fires. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> HmgBarrels;

	/** SEMPER-03's ballistic shield. Carried at the left side, swung in front of the suit when raised. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<USceneComponent> ShieldPivot;

	/** A small light in front of the visor, tinted to the visor mode so the helmet glows. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Suit")
	TObjectPtr<UPointLightComponent> VisorLight;

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

	/** Gamepad version of thrust: clicking the stick toggles sprint instead of having to hold it. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ThrustToggleAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

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

	/** 1, 2 and 3: switch straight to the Ranger, EXO-07 or SEMPER-03. */
	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> SuitRangerAction;

	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> SuitExoAction;

	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> SuitSemperAction;

	/** D-pad right and left: next and previous suit. */
	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> SuitNextAction;

	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> SuitPreviousAction;

	/** V or D-pad up: next visor mode. */
	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> VisorAction;

	/** Hold to raise SEMPER-03's ballistic shield (G or middle mouse). */
	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> ShieldAction;

	/** Gamepad: click the right stick to raise or lower the shield. */
	UPROPERTY(EditAnywhere, Category = "Input|Suit")
	TObjectPtr<UInputAction> ShieldToggleAction;

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

	/** How far the suit leans into speeding up, braking and turning on foot, in degrees per m/s². */
	UPROPERTY(EditAnywhere, Category = "Visuals")
	float GroundLeanFactor = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Visuals", meta = (Units = "deg"))
	float MaxGroundLean = 7.f;

	/** Biggest camera shake angles, at full shake. */
	UPROPERTY(EditAnywhere, Category = "Feel|Camera Shake")
	FRotator MaxShakeAngles = FRotator(2.5f, 2.f, 3.f);

	/** How fast the camera jitters while shaking. */
	UPROPERTY(EditAnywhere, Category = "Feel|Camera Shake", meta = (ClampMin = "1"))
	float ShakeFrequency = 22.f;

	/** How quickly a shake fades out, in shake per second. */
	UPROPERTY(EditAnywhere, Category = "Feel|Camera Shake", meta = (ClampMin = "0.1"))
	float ShakeDecay = 1.4f;

	/** Steady camera buffeting at full flight boost. */
	UPROPERTY(EditAnywhere, Category = "Feel|Camera Shake", meta = (ClampMin = "0", ClampMax = "1"))
	float FlightBuffet = 0.06f;

	/** Scales every camera shake. 0 turns shaking off. */
	UPROPERTY(EditAnywhere, Category = "Feel|Camera Shake", meta = (ClampMin = "0", ClampMax = "2"))
	float CameraShakeScale = 1.f;

	/** Falling speed for the softest landing that thumps. */
	UPROPERTY(EditAnywhere, Category = "Feel|Landing", meta = (ClampMin = "0", Units = "cm/s"))
	float SoftLandingSpeed = 450.f;

	/** Falling speed for the heaviest landing: legs fully crouched, big dust ring, strong shake. */
	UPROPERTY(EditAnywhere, Category = "Feel|Landing", meta = (ClampMin = "0", Units = "cm/s"))
	float HeavyLandingSpeed = 2500.f;

	/** Color of the dust kicked up by landings, footsteps and the hover jets. */
	UPROPERTY(EditAnywhere, Category = "Feel|Dust")
	FLinearColor DustColor = FLinearColor(0.3f, 0.26f, 0.19f);

	/** Hover jets blow up dust when the ground is closer than this. */
	UPROPERTY(EditAnywhere, Category = "Feel|Dust", meta = (ClampMin = "0", Units = "cm"))
	float JetDustHeight = 600.f;

	/** How far a hit shoves the suit, in cm/s per point of damage. */
	UPROPERTY(EditAnywhere, Category = "Feel|Hits", meta = (ClampMin = "0"))
	float HitKnockbackPerDamage = 12.f;

	UPROPERTY(EditAnywhere, Category = "Feel|Rumble")
	bool bGamepadRumble = true;

	/** Scales every rumble. */
	UPROPERTY(EditAnywhere, Category = "Feel|Rumble", meta = (ClampMin = "0", ClampMax = "1"))
	float RumbleScale = 1.f;

	/** Steps per second at full walking speed. Lower is slower and heavier, but each step has to reach further. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0.5"))
	float StepRateWalk = 3.f;

	/** Steps per second at full sprint. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0.5"))
	float StepRateSprint = 3.8f;

	/** Steps per second when barely moving, turning on the spot or taking a settling step. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0.5"))
	float StepRateSlow = 1.8f;

	/** Furthest a foot lands ahead of (or pushes off behind) the spot under its hip. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "10", ClampMax = "65", Units = "cm"))
	float MaxFootReach = 48.f;

	/** How high a foot lifts at walking speed. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "cm"))
	float StepHeight = 20.f;

	/** How high a foot lifts at full sprint. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "cm"))
	float SprintStepHeight = 30.f;

	/** How far the hips sit below standing straight, so the knees stay bent like a heavy mech. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", ClampMax = "30", Units = "cm"))
	float StanceCrouch = 7.f;

	/** How far the body drops as a foot slams down, at walking speed. Sprinting drops more. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "cm"))
	float StepStompDrop = 5.f;

	/** How far the hips shift over the planted foot. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "cm"))
	float HipSway = 4.f;

	/** How much the hips twist with the stride; the shoulders twist back the other way. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "deg"))
	float HipTwist = 7.f;

	/** How much the hips tilt down on the side of the swinging leg. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "deg"))
	float HipTilt = 3.f;

	/** Arm swing at walking speed. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "deg"))
	float ArmSwing = 26.f;

	/** Arm swing at full sprint. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "deg"))
	float SprintArmSwing = 44.f;

	/** How far the suit leans into a dodge. */
	UPROPERTY(EditAnywhere, Category = "Visuals|Walking", meta = (ClampMin = "0", Units = "deg"))
	float DodgeLean = 14.f;

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
	void OnThrustTogglePressed();
	void OnDodgePressed();
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

	void HandleLanded(const FVector& ImpactVelocity, bool bFromFlight);
	void HandleCrashed(const FVector& ImpactVelocity, const FVector& ImpactPoint);
	void HandleWeaponFired();
	void HandleFootstep(int32 Side, float Weight);
	void CheckGearUsed();
	void UpdateJetDust(float DeltaSeconds);
	void UpdateFeelSprings(float DeltaSeconds);
	void UpdateDodge(float DeltaSeconds);

	/** Moves the feet: plants, lifts and swings them, and works out the hip height, sway and twist that go with it. */
	void UpdateGait(float DeltaSeconds);
	/** Puts both feet down under the hips, for landings and teleports. */
	void ResetFeet();
	/** Where the ground is under a point, for placing a foot. Falls back to the capsule's feet if nothing is hit. */
	void FindFootGround(const FVector& Around, FVector& OutAnkle, FVector& OutNormal) const;
	/** Where a foot rests under its hip, in the actor's space. Side 0 is left, 1 right. */
	FVector GetFootRest(int32 Side) const;
	/** Bends the hips, knees and ankles so the feet reach where the gait put them. Runs after ApplyPose. */
	void ApplyLegIK();

	/** Where the bottom of the capsule is: the suit's feet when standing. */
	FVector GetFeetLocation() const;

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

	// ---- Suits (JavelinCharacterSuits.cpp) ----

	/** Adds the armor, helmets and gear of the two Marine suits on top of the base frame. Called from BuildSuit. */
	void BuildMarineSuits(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere, UStaticMesh* Cone);
	void BuildExoArmor(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere, UStaticMesh* Cone);
	void BuildHeavyMachineGun(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere);
	void BuildSemperArmor(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere);
	void BuildBallisticShield(UStaticMesh* Cube, UStaticMesh* Cylinder);
	/** Scatters blocky digital camo patches over the Marine suits' armor plates. */
	void BuildCamo(UStaticMesh* Cube);

	/** Shows the parts that belong to the current suit and hides the rest. */
	void UpdateSuitVisibility(const TArray<UPrimitiveComponent*>& Parts) const;
	void UpdateSuitVisibility();

	/** Paints every part for the current suit. */
	void ApplyPaint();
	void CaptureRangerStats();
	FJavelinSuitStats MakeSuitStats(EJavelinSuitClass Class) const;
	FJavelinSuitPaint MakeSuitPaint(EJavelinSuitClass Class) const;
	void BuildVisorModes();
	/** Applies the current suit to every part and component. */
	void ApplySuitClass();

	void UpdateVisor();
	void UpdateShield(float DeltaSeconds);
	void UpdateSuitExtras(float DeltaSeconds);

	/** Takes what it can of a hit from the front on the ballistic shield. Returns the damage that gets through. */
	float AbsorbWithShield(float Damage, const FVector& HitDirection);

	void OnSuitRangerPressed();
	void OnSuitExoPressed();
	void OnSuitSemperPressed();
	void OnSuitNextPressed();
	void OnSuitPreviousPressed();
	void OnVisorPressed();
	void OnShieldPressed();
	void OnShieldReleased();
	void OnShieldTogglePressed();

	/** Parts made in the constructor, kept just long enough to set them up. Not used after construction. */
	TArray<UPrimitiveComponent*> ConstructionParts;
	TArray<UStaticMeshComponent*> CamoCandidates;

	FJavelinSuitStats RangerStats;
	FJavelinSuitStats ActiveStats;
	bool bCapturedRangerStats = false;
	float SuitChangedTime = -100.f;
	float SuitChangeDeniedTime = -100.f;

	TArray<FJavelinVisorMode> VisorModes;
	FJavelinVisorMode CombatVisor;
	FJavelinVisorMode AlertVisor;
	bool bHasAutoVisors = false;
	int32 VisorIndex = 0;
	/** Which mode is showing: the selected index, or -1 for Combat and -2 for Alert. */
	int32 ShownVisorKey = -100;
	float VisorChangedTime = -100.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> VisorMaterials;

	bool bWantsShield = false;
	/** 0 carried at the side, 1 fully raised. */
	float ShieldRaise = 0.f;
	float ShieldIntegrity = 0.f;
	float LastShieldHitTime = -100.f;
	float ShieldBrokenUntil = -100.f;

	FVector MuzzleFlashScale = FVector(0.14f, 0.14f, 0.24f);
	float BarrelAngle = 0.f;

	FJavelinSuitPose CurrentPose;
	float AnimTime = 0.f;

	// Walking gait state.
	FJavelinFoot Feet[2];
	bool bFeetPlanted = false;
	/** False while the gait clock is paused with both feet settled. */
	bool bGaitRunning = false;
	/** Runs 0 to 1 over two steps. The left foot lifts at Duty, the right half a cycle later. */
	float GaitPhase = 0.f;
	/** Fraction of the cycle each foot is on the ground. Lower at speed, when both feet leave the ground between steps. */
	float GaitDuty = 0.6f;
	/** Blends the legs between planted-foot IK (1) and the animated poses (0) for flying, falling and so on. */
	float LegIKWeight = 0.f;
	float GaitBodyOffset = 0.f;
	float GaitSway = 0.f;
	float GaitHipYaw = 0.f;
	float GaitHipRoll = 0.f;
	float GaitLastYaw = 0.f;
	FVector GaitLastLocation = FVector::ZeroVector;
	/** How fast the feet are being carried along, including turning on the spot. */
	float GaitSpeed = 0.f;

	/** Body dropping as each foot slams down, in cm. */
	FJavelinSpring StepStomp;

	/** The last move input, as a flat world direction, for picking the dodge direction. */
	FVector LastMoveInput = FVector::ZeroVector;
	/** True when sprint was switched on by clicking the gamepad stick, so it turns off when you stop. */
	bool bSprintToggled = false;
	bool bWasDodging = false;
	float DodgeDustTimer = 0.f;

	float VisualPitch = 0.f;
	float VisualRoll = 0.f;
	float LastYaw = 0.f;

	/** How long the right stick has been held far enough sideways to speed up turning. */
	float StickHoldTime = 0.f;

	/** How much the camera is shaking, 0 to 1. Fades over time. */
	float CameraTrauma = 0.f;
	float ShakeTime = 0.f;

	/** Legs crouching under a landing: 0 standing, 1 fully crouched. */
	FJavelinSpring LandingCrouch;

	/** Camera height bounce from landings and footsteps, in cm. */
	FJavelinSpring CameraDip;

	/** Quick camera-only kick from each shot, in degrees. */
	FJavelinSpring CameraKick;

	/** Torso snapping back from a hit, in degrees. */
	FJavelinSpring HitFlinch;

	FVector LastVelocity = FVector::ZeroVector;
	FVector SmoothedAcceleration = FVector::ZeroVector;
	float PrevGrenadeSinceUsed = 1000.f;
	float PrevMissileSinceUsed = 1000.f;
	float JetDustTimer = 0.f;
};
