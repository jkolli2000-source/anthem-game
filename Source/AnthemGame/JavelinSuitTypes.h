#pragma once

#include "CoreMinimal.h"
#include "JavelinSuitTypes.generated.h"

/**
 * The javelin suits you can pilot. Each one has its own look, weapon, stats and visor modes.
 * Switch between them in game with 1, 2 and 3 (or D-pad left and right on a gamepad).
 */
UENUM(BlueprintType)
enum class EJavelinSuitClass : uint8
{
	/** The original all-rounder: assault rifle, blue and orange paint. */
	Ranger UMETA(DisplayName = "Ranger"),
	/** USMC EXO-07: Marine heavy exosuit. Heavy machine gun, woodland camo, horned helmet. Tough but slow. */
	Exo07 UMETA(DisplayName = "USMC EXO-07 (Heavy Assault)"),
	/** SEMPER-03: Marine combat exosuit. Assault rifle and ballistic shield, desert camo, thruster pack. */
	Semper03 UMETA(DisplayName = "SEMPER-03 (Assault / Defense)"),
};

/** What the visor is showing. Combat and Alert switch on by themselves; the rest you pick with V (D-pad up). */
UENUM(BlueprintType)
enum class EJavelinVisorEffect : uint8
{
	None,
	/** Red: aiming or locked on. */
	Combat,
	/** Amber: just took a hit. */
	Alert,
	/** EXO-07 Support: shields start recharging sooner and recharge faster. */
	Support,
	/** EXO-07 Stealth: enemies only spot you from half as far, until you start shooting. */
	LowProfile,
	/** SEMPER-03 Tactical: night vision, brightens and greens the view. */
	NightVision,
	/** SEMPER-03 Scan: shows every enemy nearby on the HUD, even ones that haven't seen you. */
	Scan,
};

/** One visor mode: its name on the HUD, its color, and what it does. */
struct FJavelinVisorMode
{
	FString Name;
	/** Shown in brackets after the name, like the character sheets. */
	FString Detail;
	FLinearColor Color = FLinearColor::White;
	EJavelinVisorEffect Effect = EJavelinVisorEffect::None;
};

/** Paint for one suit. Marine suits add digital camo blotches in the three camo colors. */
struct FJavelinSuitPaint
{
	FLinearColor Primary = FLinearColor::Gray;
	FLinearColor Secondary = FLinearColor::Gray;
	FLinearColor Trim = FLinearColor::Gray;
	FLinearColor Frame = FLinearColor::Black;
	FLinearColor Accent = FLinearColor::White;
	FLinearColor Camo[3] = { FLinearColor::Gray, FLinearColor::Gray, FLinearColor::Gray };
	FLinearColor Emblem = FLinearColor(0.75f, 0.6f, 0.25f);
	FLinearColor MuzzleFlash = FLinearColor(1.f, 0.8f, 0.35f);
};

/**
 * Everything that changes between suits apart from which parts are shown.
 * Values here are the suit's own; the Ranger uses whatever the components were set to in the editor.
 */
struct FJavelinSuitStats
{
	FText Name;
	FText Role;

	// Weapon
	FText WeaponName;
	float Damage = 30.f;
	float FireRate = 600.f;
	int32 MagazineSize = 30;
	int32 MaxReserveAmmo = 270;
	float ReloadTime = 1.8f;
	float HipSpread = 2.5f;
	float AimSpread = 0.6f;
	float SpreadPerShot = 0.35f;
	float MaxBloom = 3.f;
	float RecoilPitch = 0.45f;
	float RecoilYaw = 0.2f;
	float HitImpulse = 45.f;
	FLinearColor TracerColor = FLinearColor(1.f, 0.75f, 0.3f);
	/** Seconds the trigger has to be held before the gun starts firing. The HMG's barrels spin up first. */
	float SpinUpTime = 0.f;

	// Suit health
	float MaxShields = 200.f;
	float MaxArmor = 300.f;
	float ShieldRegenDelay = 4.f;
	float ShieldRegenPerSecond = 60.f;

	// Movement
	float WalkSpeed = 520.f;
	float SprintSpeed = 1000.f;
	float DodgeSpeed = 2300.f;
	float DodgeCooldown = 0.75f;
	float FlightCruiseSpeed = 3000.f;
	float FlightBoostSpeed = 4500.f;
	float FlightTurnRate = 170.f;
	float HoverSpeed = 600.f;

	// Heat
	float FlightHeatPerSecond = 9.f;
	float HoverHeatPerSecond = 4.f;
	float PassiveCoolingPerSecond = 20.f;

	// Gear
	float GrenadeCooldown = 8.f;
	float GrenadeDamage = 180.f;
	float GrenadeBlastRadius = 500.f;
	float MissileCooldown = 12.f;
	float MissileDamage = 70.f;
};

/** Component tags saying which suits a part belongs to. Parts with none of these are shared by every suit. */
namespace JavelinSuitTags
{
	inline const FName Ranger(TEXT("Suit.Ranger"));
	inline const FName Exo07(TEXT("Suit.Exo07"));
	inline const FName Semper03(TEXT("Suit.Semper03"));

	inline FName ForClass(EJavelinSuitClass Class)
	{
		switch (Class)
		{
		case EJavelinSuitClass::Exo07: return Exo07;
		case EJavelinSuitClass::Semper03: return Semper03;
		default: return Ranger;
		}
	}
}

/** Component tags saying how each suit part is painted. */
namespace JavelinPaintTags
{
	inline const FName Primary(TEXT("Paint.Primary"));
	inline const FName Secondary(TEXT("Paint.Secondary"));
	inline const FName Trim(TEXT("Paint.Trim"));
	inline const FName Frame(TEXT("Paint.Frame"));
	inline const FName Accent(TEXT("Paint.Accent"));
	inline const FName Glow(TEXT("Paint.Glow"));
	inline const FName Muzzle(TEXT("Paint.Muzzle"));
	/** Visor lenses: colored by the visor mode. */
	inline const FName Visor(TEXT("Paint.Visor"));
	/** Eagle, globe and anchor insignia. */
	inline const FName Emblem(TEXT("Paint.Emblem"));
	inline const FName Camo0(TEXT("Paint.Camo0"));
	inline const FName Camo1(TEXT("Paint.Camo1"));
	inline const FName Camo2(TEXT("Paint.Camo2"));
	inline const FName FlagRed(TEXT("Paint.FlagRed"));
	inline const FName FlagWhite(TEXT("Paint.FlagWhite"));
	inline const FName FlagBlue(TEXT("Paint.FlagBlue"));
}
