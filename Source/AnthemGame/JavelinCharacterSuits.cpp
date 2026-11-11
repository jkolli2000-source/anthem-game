// The javelin suits: the original Ranger and the two Marine exosuits, USMC EXO-07 and SEMPER-03.
//
// Every suit shares one frame (pivots, base parts and animation, built in JavelinCharacter.cpp). Each Marine
// suit adds its own armor, helmet and gear on top, tagged with the suit it belongs to, and only the current
// suit's parts are shown. Switching suits shows and hides parts, repaints them and swaps the gun, stats,
// gear tuning and visor modes. All of it is built from the engine's basic shapes, so it needs no art yet.

#include "JavelinCharacter.h"
#include "DustEffect.h"
#include "JavelinAbilityComponent.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"
#include "JavelinSuitHealthComponent.h"
#include "JavelinWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	namespace SuitPaint = JavelinPaintTags;

	void SetSuitPartColor(UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	/** A color picked in sRGB (0 to 255, as in a paint program), converted for the renderer. */
	FLinearColor SuitColor(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor(FColor(R, G, B));
	}

	bool HasSuitTag(const UActorComponent* Part)
	{
		return Part->ComponentHasTag(JavelinSuitTags::Ranger) || Part->ComponentHasTag(JavelinSuitTags::Exo07)
			|| Part->ComponentHasTag(JavelinSuitTags::Semper03);
	}

	/** Shows a part if it belongs to the suit (or to every suit), hides it otherwise. */
	void ShowPartForSuit(UPrimitiveComponent* Part, FName SuitTag)
	{
		if (Part && HasSuitTag(Part))
		{
			Part->SetVisibility(Part->ComponentHasTag(SuitTag));
		}
	}

	// Where SEMPER-03's shield sits, relative to the torso: hanging at the left side, or held up in front.
	const FVector ShieldCarriedLocation(-2.f, -70.f, 22.f);
	const FRotator ShieldCarriedRotation(0.f, -90.f, 0.f);
	const FVector ShieldRaisedLocation(72.f, -18.f, 34.f);
	const FRotator ShieldRaisedRotation(0.f, 6.f, 0.f);

	/** How long the visor stays on Combat after the last shot, and on Alert after the last hit. */
	constexpr float VisorCombatHoldTime = 1.5f;
	constexpr float VisorAlertHoldTime = 2.5f;
}

// ---------------------------------------------------------------------------------------------------------
// Building the Marine suits (constructor)
// ---------------------------------------------------------------------------------------------------------

void AJavelinCharacter::BuildMarineSuits(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere, UStaticMesh* Cone)
{
	BuildExoArmor(Cube, Cylinder, Sphere, Cone);
	BuildHeavyMachineGun(Cube, Cylinder, Sphere);
	BuildSemperArmor(Cube, Cylinder, Sphere);
	BuildBallisticShield(Cube, Cylinder);

	// A faint light just in front of the visor so the helmet picks up the visor's color.
	VisorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("VisorLight"));
	VisorLight->SetupAttachment(HeadPivot);
	VisorLight->SetRelativeLocation(FVector(28.f, 0.f, 17.f));
	VisorLight->SetMobility(EComponentMobility::Movable);
	VisorLight->IntensityUnits = ELightUnits::Candelas;
	VisorLight->Intensity = 6.f;
	VisorLight->AttenuationRadius = 70.f;
	VisorLight->CastShadows = false;
}

void AJavelinCharacter::BuildExoArmor(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere, UStaticMesh* Cone)
{
	// USMC EXO-07, the Marine heavy exosuit: bulky woodland-camo plates, a horned helmet with angry slit eyes,
	// the Marine Corps insignia on the chest, a flag on the left shoulder and a big assault pack on the back.
	auto Add = [this](FName Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, FName Paint, const FRotator& Rotation)
	{
		UStaticMeshComponent* Part = CreatePart(Name, Parent, Mesh, Location, Scale, Paint, Rotation);
		Part->ComponentTags.Add(JavelinSuitTags::Exo07);
		return Part;
	};
	const FRotator Flat = FRotator::ZeroRotator;
	const FRotator FacingForward(90.f, 0.f, 0.f);

	// Chest armor with the eagle, globe and anchor
	Add(TEXT("ExoChestArmor"), TorsoPivot, Cube, FVector(28.f, 0.f, 44.f), FVector(0.12f, 0.72f, 0.42f), SuitPaint::Primary, FRotator(-6.f, 0.f, 0.f));
	Add(TEXT("ExoChestEmblem"), TorsoPivot, Cylinder, FVector(34.5f, 0.f, 46.f), FVector(0.17f, 0.17f, 0.02f), SuitPaint::Emblem, FacingForward);
	Add(TEXT("ExoEmblemGlobe"), TorsoPivot, Sphere, FVector(35.f, 0.f, 46.f), FVector(0.05f, 0.09f, 0.09f), SuitPaint::Trim, Flat);
	Add(TEXT("ExoEmblemEagle"), TorsoPivot, Cube, FVector(35.5f, 0.f, 52.5f), FVector(0.02f, 0.16f, 0.03f), SuitPaint::Emblem, Flat);
	Add(TEXT("ExoEmblemAnchor"), TorsoPivot, Cube, FVector(35.5f, 0.f, 40.f), FVector(0.02f, 0.03f, 0.1f), SuitPaint::Emblem, Flat);
	Add(TEXT("ExoAbdomenPlate"), TorsoPivot, Cube, FVector(17.f, 0.f, 13.f), FVector(0.1f, 0.4f, 0.18f), SuitPaint::Secondary, Flat);
	Add(TEXT("ExoNeckGuard"), TorsoPivot, Cube, FVector(-2.f, 0.f, 72.f), FVector(0.42f, 0.58f, 0.12f), SuitPaint::Secondary, Flat);

	// Assault pack: power, ammo and comms
	Add(TEXT("ExoAssaultPack"), TorsoPivot, Cube, FVector(-54.f, 0.f, 50.f), FVector(0.22f, 0.66f, 0.5f), SuitPaint::Primary, Flat);
	Add(TEXT("ExoPackLid"), TorsoPivot, Cube, FVector(-54.f, 0.f, 76.f), FVector(0.24f, 0.62f, 0.06f), SuitPaint::Trim, Flat);
	Add(TEXT("ExoPackPouchBack"), TorsoPivot, Cube, FVector(-66.f, 0.f, 44.f), FVector(0.04f, 0.4f, 0.28f), SuitPaint::Secondary, Flat);
	Add(TEXT("ExoPackLights"), TorsoPivot, Cube, FVector(-68.5f, 14.f, 58.f), FVector(0.01f, 0.08f, 0.03f), SuitPaint::Visor, Flat);
	Add(TEXT("ExoAntenna"), TorsoPivot, Cylinder, FVector(-58.f, 24.f, 100.f), FVector(0.015f, 0.015f, 0.5f), SuitPaint::Frame, Flat);
	Add(TEXT("ExoBelt"), VisualRoot, Cube, FVector(0.f, 0.f, 0.f), FVector(0.4f, 0.56f, 0.08f), SuitPaint::Frame, Flat);

	// Helmet: brow and jaw guards, cheek plates, two horns and V-shaped eye slits
	Add(TEXT("ExoBrow"), HeadPivot, Cube, FVector(12.f, 0.f, 27.f), FVector(0.16f, 0.32f, 0.07f), SuitPaint::Primary, FRotator(-12.f, 0.f, 0.f));
	Add(TEXT("ExoJaw"), HeadPivot, Cube, FVector(14.f, 0.f, 5.f), FVector(0.14f, 0.28f, 0.1f), SuitPaint::Secondary, Flat);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? -1.f : 1.f;
		const FString Prefix = Side == 0 ? TEXT("Left") : TEXT("Right");
		auto PartName = [&Prefix](const TCHAR* Part) { return FName(*(Prefix + Part)); };
		USceneComponent* Shoulder = Side == 0 ? LeftShoulder : RightShoulder;
		USceneComponent* Elbow = Side == 0 ? LeftElbow : RightElbow;
		USceneComponent* Hip = Side == 0 ? LeftHip : RightHip;
		USceneComponent* Knee = Side == 0 ? LeftKnee : RightKnee;
		USceneComponent* Ankle = Side == 0 ? LeftAnkle : RightAnkle;

		Add(PartName(TEXT("ExoCheek")), HeadPivot, Cube, FVector(8.f, 15.f * S, 12.f), FVector(0.18f, 0.05f, 0.16f), SuitPaint::Primary, Flat);
		// The inner end of each eye sits lower, for a scowl.
		Add(PartName(TEXT("ExoEye")), HeadPivot, Cube, FVector(17.5f, 6.f * S, 18.f), FVector(0.03f, 0.1f, 0.035f), SuitPaint::Visor, FRotator(0.f, 0.f, 12.f * S));
		// Horns sweep back and out.
		Add(PartName(TEXT("ExoHornBase")), HeadPivot, Cube, FVector(-1.f, 9.f * S, 33.f), FVector(0.1f, 0.06f, 0.06f), SuitPaint::Frame, Flat);
		Add(PartName(TEXT("ExoHorn")), HeadPivot, Cone, FVector(-1.f, 9.f * S, 46.f), FVector(0.08f, 0.08f, 0.28f), SuitPaint::Secondary, FRotator(15.f, 0.f, -10.f * S));

		// Big square shoulder shells
		Add(PartName(TEXT("ExoPauldronShell")), Shoulder, Cube, FVector(0.f, 10.f * S, 10.f), FVector(0.5f, 0.3f, 0.36f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("ExoPauldronRim")), Shoulder, Cube, FVector(0.f, 12.f * S, 29.f), FVector(0.46f, 0.26f, 0.04f), SuitPaint::Trim, Flat);

		// Arms
		Add(PartName(TEXT("ExoForearmGuard")), Elbow, Cube, FVector(4.f, 0.f, -18.f), FVector(0.3f, 0.28f, 0.3f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("ExoElbowPad")), Elbow, Sphere, FVector(-6.f, 0.f, -1.f), FVector(0.16f, 0.16f, 0.16f), SuitPaint::Secondary, Flat);

		// Legs and boots
		Add(PartName(TEXT("ExoThighArmor")), Hip, Cube, FVector(4.f, 0.f, -20.f), FVector(0.32f, 0.3f, 0.36f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("ExoKneeGuard")), Knee, Cube, FVector(14.f, 0.f, 0.f), FVector(0.12f, 0.22f, 0.2f), SuitPaint::Secondary, Flat);
		Add(PartName(TEXT("ExoShinArmor")), Knee, Cube, FVector(5.f, 0.f, -20.f), FVector(0.3f, 0.28f, 0.3f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("ExoBoot")), Ankle, Cube, FVector(8.f, 0.f, -1.f), FVector(0.42f, 0.26f, 0.14f), SuitPaint::Frame, Flat);

		Add(PartName(TEXT("ExoPackPouch")), TorsoPivot, Cube, FVector(-52.f, 37.f * S, 42.f), FVector(0.18f, 0.1f, 0.3f), SuitPaint::Secondary, Flat);
		Add(PartName(TEXT("ExoBeltPouch")), VisualRoot, Cube, FVector(15.f, 17.f * S, -4.f), FVector(0.08f, 0.12f, 0.12f), SuitPaint::Secondary, Flat);
	}

	// U.S. flag patch on the outside of the left shoulder shell: red and white stripes with a blue canton toward the front.
	const float FlagY = -25.6f;
	Add(TEXT("ExoFlag"), LeftShoulder, Cube, FVector(2.f, FlagY, 10.f), FVector(0.22f, 0.01f, 0.14f), SuitPaint::FlagWhite, Flat);
	for (int32 Stripe = 0; Stripe < 4; ++Stripe)
	{
		Add(*FString::Printf(TEXT("ExoFlagStripe%d"), Stripe), LeftShoulder, Cube, FVector(2.f, FlagY - 0.1f, 16.f - Stripe * 4.f),
			FVector(0.22f, 0.012f, 0.02f), SuitPaint::FlagRed, Flat);
	}
	Add(TEXT("ExoFlagCanton"), LeftShoulder, Cube, FVector(8.f, FlagY - 0.2f, 13.5f), FVector(0.1f, 0.014f, 0.07f), SuitPaint::FlagBlue, Flat);
}

void AJavelinCharacter::BuildHeavyMachineGun(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere)
{
	// The HMG-88: a boxy receiver with a side ammo drum, a carry handle, a vented barrel shroud and three rotary
	// barrels that spin up before it fires. Held in the right hand like the rifle, in the elbow's space.
	USceneComponent* Hand = RightElbow;
	auto Add = [this](FName Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, FName Paint, const FRotator& Rotation)
	{
		UStaticMeshComponent* Part = CreatePart(Name, Parent, Mesh, Location, Scale, Paint, Rotation);
		Part->ComponentTags.Add(JavelinSuitTags::Exo07);
		return Part;
	};
	const FRotator Flat = FRotator::ZeroRotator;
	const FRotator SideOn(0.f, 0.f, 90.f);

	Add(TEXT("HmgReceiver"), Hand, Cube, FVector(4.f, 0.f, -64.f), FVector(0.24f, 0.16f, 0.56f), SuitPaint::Frame, Flat);
	Add(TEXT("HmgCover"), Hand, Cube, FVector(17.f, 0.f, -60.f), FVector(0.04f, 0.14f, 0.4f), SuitPaint::Trim, Flat);
	Add(TEXT("HmgHandle"), Hand, Cube, FVector(24.f, 0.f, -64.f), FVector(0.04f, 0.04f, 0.18f), SuitPaint::Frame, Flat);
	Add(TEXT("HmgHandlePost"), Hand, Cube, FVector(21.f, 0.f, -72.f), FVector(0.06f, 0.03f, 0.03f), SuitPaint::Frame, Flat);
	Add(TEXT("HmgDrum"), Hand, Cylinder, FVector(-14.f, -2.f, -58.f), FVector(0.28f, 0.28f, 0.16f), SuitPaint::Secondary, SideOn);
	Add(TEXT("HmgDrumCap"), Hand, Cylinder, FVector(-14.f, -10.5f, -58.f), FVector(0.2f, 0.2f, 0.01f), SuitPaint::Trim, SideOn);
	Add(TEXT("HmgFeed"), Hand, Cube, FVector(-4.f, 0.f, -62.f), FVector(0.06f, 0.1f, 0.1f), SuitPaint::Frame, Flat);
	Add(TEXT("HmgFrontGrip"), Hand, Cube, FVector(-10.f, 0.f, -92.f), FVector(0.1f, 0.06f, 0.14f), SuitPaint::Frame, Flat);
	Add(TEXT("HmgShroud"), Hand, Cylinder, FVector(5.f, 0.f, -104.f), FVector(0.15f, 0.15f, 0.3f), SuitPaint::Frame, Flat);
	for (int32 Vent = 0; Vent < 3; ++Vent)
	{
		Add(*FString::Printf(TEXT("HmgShroudBand%d"), Vent), Hand, Cylinder, FVector(5.f, 0.f, -96.f - Vent * 8.f),
			FVector(0.16f, 0.16f, 0.012f), SuitPaint::Trim, Flat);
	}

	// The barrels turn around the gun's long axis, which is the pivot's Z.
	HmgBarrels = CreatePivot(TEXT("HmgBarrels"), Hand, FVector(5.f, 0.f, -119.f));
	for (int32 Barrel = 0; Barrel < 3; ++Barrel)
	{
		const float Angle = Barrel * 2.f * UE_PI / 3.f;
		Add(*FString::Printf(TEXT("HmgBarrel%d"), Barrel), HmgBarrels, Cylinder, FVector(3.2f * FMath::Cos(Angle), 3.2f * FMath::Sin(Angle), -10.f),
			FVector(0.035f, 0.035f, 0.24f), SuitPaint::Frame, Flat);
	}
	Add(TEXT("HmgMuzzleBrake"), HmgBarrels, Cylinder, FVector(0.f, 0.f, -23.f), FVector(0.1f, 0.1f, 0.04f), SuitPaint::Frame, Flat);

	HmgMuzzle = CreatePivot(TEXT("HmgMuzzle"), Hand, FVector(5.f, 0.f, -146.f));
	// Not tagged: the flash is shown only for a moment after each shot, by UpdateMuzzleFlash.
	HmgMuzzleFlash = CreatePart(TEXT("HmgMuzzleFlash"), HmgMuzzle, Sphere, FVector(0.f, 0.f, -12.f), FVector(0.24f, 0.24f, 0.4f), SuitPaint::Muzzle);
	HmgMuzzleFlash->SetCastShadow(false);
	HmgMuzzleFlash->SetVisibility(false);
}

void AJavelinCharacter::BuildSemperArmor(UStaticMesh* Cube, UStaticMesh* Cylinder, UStaticMesh* Sphere)
{
	// SEMPER-03, the Marine combat exosuit: segmented desert-camo plates, a boxy helmet with a cluster of four
	// optic lenses, and a thruster pack of four big jets on the back. It carries the rifle with a scope added.
	auto Add = [this](FName Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, FName Paint, const FRotator& Rotation)
	{
		UStaticMeshComponent* Part = CreatePart(Name, Parent, Mesh, Location, Scale, Paint, Rotation);
		Part->ComponentTags.Add(JavelinSuitTags::Semper03);
		return Part;
	};
	const FRotator Flat = FRotator::ZeroRotator;
	const FRotator FacingForward(90.f, 0.f, 0.f);

	// Helmet with the four-lens optics cluster
	Add(TEXT("SemperHelmet"), HeadPivot, Cube, FVector(2.f, 0.f, 17.f), FVector(0.37f, 0.33f, 0.34f), SuitPaint::Primary, Flat);
	Add(TEXT("SemperHelmetTop"), HeadPivot, Cube, FVector(0.f, 0.f, 34.f), FVector(0.33f, 0.29f, 0.06f), SuitPaint::Secondary, FRotator(-6.f, 0.f, 0.f));
	Add(TEXT("SemperBrow"), HeadPivot, Cube, FVector(17.f, 0.f, 27.f), FVector(0.08f, 0.3f, 0.06f), SuitPaint::Primary, FRotator(-20.f, 0.f, 0.f));
	Add(TEXT("SemperFaceplate"), HeadPivot, Cube, FVector(19.5f, 0.f, 13.f), FVector(0.06f, 0.28f, 0.24f), SuitPaint::Frame, Flat);
	Add(TEXT("SemperChin"), HeadPivot, Cube, FVector(17.f, 0.f, 0.f), FVector(0.1f, 0.2f, 0.06f), SuitPaint::Primary, Flat);
	for (int32 Lens = 0; Lens < 4; ++Lens)
	{
		const float Y = (Lens % 2 == 0) ? -5.5f : 5.5f;
		const float Z = (Lens < 2) ? 18.f : 9.5f;
		Add(*FString::Printf(TEXT("SemperLensRim%d"), Lens), HeadPivot, Cylinder, FVector(21.5f, Y, Z), FVector(0.09f, 0.09f, 0.03f), SuitPaint::Frame, FacingForward);
		Add(*FString::Printf(TEXT("SemperLens%d"), Lens), HeadPivot, Cylinder, FVector(22.8f, Y, Z), FVector(0.065f, 0.065f, 0.02f), SuitPaint::Visor, FacingForward);
	}

	// Chest plates and collar
	Add(TEXT("SemperChestPlate"), TorsoPivot, Cube, FVector(27.f, 0.f, 47.f), FVector(0.08f, 0.7f, 0.34f), SuitPaint::Primary, FRotator(-8.f, 0.f, 0.f));
	Add(TEXT("SemperChestPlateLow"), TorsoPivot, Cube, FVector(24.f, 0.f, 25.f), FVector(0.08f, 0.52f, 0.16f), SuitPaint::Secondary, FRotator(8.f, 0.f, 0.f));
	Add(TEXT("SemperCollar"), TorsoPivot, Cube, FVector(-2.f, 0.f, 70.f), FVector(0.38f, 0.54f, 0.08f), SuitPaint::Primary, Flat);
	Add(TEXT("SemperBelt"), VisualRoot, Cube, FVector(0.f, 0.f, 0.f), FVector(0.38f, 0.54f, 0.08f), SuitPaint::Frame, Flat);
	Add(TEXT("SemperBeltPouch"), VisualRoot, Cube, FVector(15.f, 15.f, -4.f), FVector(0.08f, 0.12f, 0.12f), SuitPaint::Secondary, Flat);

	// Thruster pack: four big jets on a rack behind the jet pack
	Add(TEXT("SemperThrusterRack"), TorsoPivot, Cube, FVector(-48.f, 0.f, 56.f), FVector(0.12f, 0.52f, 0.34f), SuitPaint::Frame, Flat);
	for (int32 Jet = 0; Jet < 4; ++Jet)
	{
		const float Y = (Jet % 2 == 0) ? -17.f : 17.f;
		const float Z = (Jet < 2) ? 72.f : 38.f;
		Add(*FString::Printf(TEXT("SemperThruster%d"), Jet), TorsoPivot, Cylinder, FVector(-56.f, Y, Z), FVector(0.15f, 0.15f, 0.28f), SuitPaint::Secondary, Flat);
		Add(*FString::Printf(TEXT("SemperThrusterCap%d"), Jet), TorsoPivot, Cylinder, FVector(-56.f, Y, Z + 15.f), FVector(0.12f, 0.12f, 0.03f), SuitPaint::Frame, Flat);
		Add(*FString::Printf(TEXT("SemperThrusterGlow%d"), Jet), TorsoPivot, Cylinder, FVector(-56.f, Y, Z - 14.5f), FVector(0.12f, 0.12f, 0.01f), SuitPaint::Glow, Flat);
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? -1.f : 1.f;
		const FString Prefix = Side == 0 ? TEXT("Left") : TEXT("Right");
		auto PartName = [&Prefix](const TCHAR* Part) { return FName(*(Prefix + Part)); };
		USceneComponent* Shoulder = Side == 0 ? LeftShoulder : RightShoulder;
		USceneComponent* Elbow = Side == 0 ? LeftElbow : RightElbow;
		USceneComponent* Hip = Side == 0 ? LeftHip : RightHip;
		USceneComponent* Knee = Side == 0 ? LeftKnee : RightKnee;
		USceneComponent* Ankle = Side == 0 ? LeftAnkle : RightAnkle;

		Add(PartName(TEXT("SemperCheek")), HeadPivot, Cube, FVector(10.f, 16.5f * S, 10.f), FVector(0.2f, 0.04f, 0.2f), SuitPaint::Secondary, Flat);
		Add(PartName(TEXT("SemperPauldron")), Shoulder, Cube, FVector(0.f, 9.f * S, 8.f), FVector(0.44f, 0.3f, 0.3f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("SemperPauldronPlate")), Shoulder, Cube, FVector(0.f, 24.5f * S, -2.f), FVector(0.32f, 0.03f, 0.24f), SuitPaint::Secondary, Flat);
		Add(PartName(TEXT("SemperForearm")), Elbow, Cube, FVector(5.f, 0.f, -18.f), FVector(0.28f, 0.26f, 0.28f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("SemperElbowPad")), Elbow, Sphere, FVector(-6.f, 0.f, -1.f), FVector(0.14f, 0.14f, 0.14f), SuitPaint::Secondary, Flat);
		Add(PartName(TEXT("SemperThigh")), Hip, Cube, FVector(5.f, 0.f, -20.f), FVector(0.3f, 0.28f, 0.34f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("SemperKnee")), Knee, Cube, FVector(14.f, 0.f, 0.f), FVector(0.1f, 0.22f, 0.22f), SuitPaint::Secondary, FRotator(-10.f, 0.f, 0.f));
		Add(PartName(TEXT("SemperShin")), Knee, Cube, FVector(6.f, 0.f, -18.f), FVector(0.28f, 0.27f, 0.3f), SuitPaint::Primary, Flat);
		Add(PartName(TEXT("SemperBoot")), Ankle, Cube, FVector(8.f, 0.f, -1.f), FVector(0.4f, 0.25f, 0.14f), SuitPaint::Frame, Flat);
	}

	// A scope and handguard on the rifle
	Add(TEXT("SemperScope"), RightElbow, Cylinder, FVector(14.5f, 0.f, -58.f), FVector(0.05f, 0.05f, 0.14f), SuitPaint::Frame, Flat);
	Add(TEXT("SemperScopeLens"), RightElbow, Cylinder, FVector(14.5f, 0.f, -65.2f), FVector(0.042f, 0.042f, 0.01f), SuitPaint::Visor, Flat);
	Add(TEXT("SemperHandguard"), RightElbow, Cube, FVector(6.f, 0.f, -82.f), FVector(0.12f, 0.11f, 0.16f), SuitPaint::Secondary, Flat);
}

void AJavelinCharacter::BuildBallisticShield(UStaticMesh* Cube, UStaticMesh* Cylinder)
{
	// A tall, coffin-shaped ballistic shield marked SEMPER FI and 03, with the insignia in the middle.
	// In the pivot's space the shield faces +X, its width runs along Y and its height along Z.
	ShieldPivot = CreatePivot(TEXT("ShieldPivot"), TorsoPivot, ShieldCarriedLocation);
	ShieldPivot->SetRelativeRotation(ShieldCarriedRotation);

	auto Add = [this](FName Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, FName Paint, const FRotator& Rotation)
	{
		UStaticMeshComponent* Part = CreatePart(Name, ShieldPivot, Mesh, Location, Scale, Paint, Rotation);
		Part->ComponentTags.Add(JavelinSuitTags::Semper03);
		return Part;
	};
	const FRotator Flat = FRotator::ZeroRotator;

	Add(TEXT("ShieldPlate"), Cube, FVector(0.f, 0.f, 0.f), FVector(0.05f, 0.62f, 0.62f), SuitPaint::Primary, Flat);
	Add(TEXT("ShieldTop"), Cube, FVector(0.f, 0.f, 42.f), FVector(0.05f, 0.44f, 0.24f), SuitPaint::Primary, Flat);
	Add(TEXT("ShieldBottom"), Cube, FVector(0.f, 0.f, -42.f), FVector(0.05f, 0.4f, 0.24f), SuitPaint::Primary, Flat);
	// Slanted corners give it the coffin outline.
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const float Y = (Corner % 2 == 0) ? -1.f : 1.f;
		const float Z = (Corner < 2) ? 1.f : -1.f;
		Add(*FString::Printf(TEXT("ShieldCorner%d"), Corner), Cube, FVector(0.f, 26.f * Y, 38.f * Z), FVector(0.05f, 0.12f, 0.2f),
			SuitPaint::Primary, FRotator(0.f, 0.f, 35.f * Y * Z));
	}
	Add(TEXT("ShieldBacking"), Cube, FVector(-3.f, 0.f, 0.f), FVector(0.02f, 0.66f, 1.1f), SuitPaint::Frame, Flat);
	Add(TEXT("ShieldHandle"), Cube, FVector(-5.f, 0.f, 0.f), FVector(0.05f, 0.06f, 0.22f), SuitPaint::Frame, Flat);
	Add(TEXT("ShieldViewSlit"), Cube, FVector(2.8f, 0.f, 47.f), FVector(0.01f, 0.22f, 0.035f), SuitPaint::Frame, Flat);
	Add(TEXT("ShieldEmblem"), Cylinder, FVector(3.f, 0.f, 6.f), FVector(0.26f, 0.26f, 0.01f), SuitPaint::Emblem, FRotator(90.f, 0.f, 0.f));
	Add(TEXT("ShieldEmblemGlobe"), Cube, FVector(3.6f, 0.f, 6.f), FVector(0.01f, 0.12f, 0.12f), SuitPaint::Trim, FRotator(0.f, 0.f, 45.f));

	// Stenciled lettering. Text faces +X, the shield's front.
	auto AddText = [this](FName Name, const TCHAR* Text, const FVector& Location, float Size)
	{
		UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(Name);
		Label->SetupAttachment(ShieldPivot);
		Label->SetRelativeLocation(Location);
		Label->SetText(FText::FromString(Text));
		Label->SetTextRenderColor(FColor(52, 44, 32));
		Label->SetWorldSize(Size);
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCastShadow(false);
		Label->ComponentTags.Add(JavelinSuitTags::Semper03);
		ConstructionParts.Add(Label);
	};
	AddText(TEXT("ShieldMotto"), TEXT("SEMPER FI"), FVector(3.4f, 0.f, 31.f), 9.f);
	AddText(TEXT("ShieldNumber"), TEXT("03"), FVector(3.4f, 0.f, -24.f), 30.f);
}

void AJavelinCharacter::BuildCamo(UStaticMesh* Cube)
{
	// Digital camo: small square patches in the three camo colors, scattered over the faces of the painted
	// armor boxes. They're shared by both Marine suits (woodland on EXO-07, desert on SEMPER-03) and hidden on
	// the Ranger. A patch is a very thin box sitting just proud of the face it's on.
	const TArray<UStaticMeshComponent*> Candidates = CamoCandidates;
	const FName CamoPaints[3] = { SuitPaint::Camo0, SuitPaint::Camo1, SuitPaint::Camo2 };

	for (UStaticMeshComponent* Part : Candidates)
	{
		// Which Marine suits show this part? Untagged parts are on every suit.
		const bool bShared = !HasSuitTag(Part);
		const bool bOnExo = bShared || Part->ComponentHasTag(JavelinSuitTags::Exo07);
		const bool bOnSemper = bShared || Part->ComponentHasTag(JavelinSuitTags::Semper03);
		if (!bOnExo && !bOnSemper)
		{
			continue;
		}

		// Most of the shared frame's limbs end up inside the Marine armor, so only the big shared boxes that stay in
		// view get patches. Tiny parts like pouches are left plain.
		static const FName VisibleSharedParts[] = { TEXT("Chest"), TEXT("Codpiece"), TEXT("JetPack"), TEXT("MissilePod") };
		bool bVisibleShared = false;
		for (const FName& Shared : VisibleSharedParts)
		{
			bVisibleShared |= Part->GetFName() == Shared;
		}
		const FVector3f SizeCm(Part->GetRelativeScale3D() * 100.f);
		if ((bShared && !bVisibleShared) || SizeCm.GetMax() < 15.f)
		{
			continue;
		}
		FRandomStream Random(static_cast<int32>(GetTypeHash(Part->GetFName())));

		// Bigger plates get more patches. The bottom face is skipped; nobody sees it.
		const float Area = SizeCm.Y * SizeCm.Z + SizeCm.X * SizeCm.Z + SizeCm.X * SizeCm.Y;
		const int32 Clusters = FMath::Clamp(FMath::RoundToInt(Area / 900.f), 1, 4);
		const float FaceAreas[5] = { SizeCm.Y * SizeCm.Z, SizeCm.Y * SizeCm.Z, SizeCm.X * SizeCm.Z, SizeCm.X * SizeCm.Z, SizeCm.X * SizeCm.Y };
		float TotalArea = 0.f;
		for (float FaceArea : FaceAreas)
		{
			TotalArea += FaceArea;
		}

		int32 PatchIndex = 0;
		for (int32 Cluster = 0; Cluster < Clusters; ++Cluster)
		{
			// Pick a face, weighted by its size: +X, -X, +Y, -Y, +Z.
			float Pick = Random.FRandRange(0.f, TotalArea);
			int32 Face = 0;
			while (Face < 4 && Pick > FaceAreas[Face])
			{
				Pick -= FaceAreas[Face];
				++Face;
			}
			const int32 Axis = Face / 2;
			const float Sign = (Face == 4 || Face % 2 == 0) ? 1.f : -1.f;
			const int32 AxisU = (Axis + 1) % 3;
			const int32 AxisV = (Axis + 2) % 3;
			const FName Paint = CamoPaints[Random.RandRange(0, 2)];

			// One or two blocks per cluster, the second stepped off the first, for the pixelated look.
			const int32 Blocks = Random.FRand() < 0.55f ? 2 : 1;
			float U = 0.f;
			float V = 0.f;
			for (int32 Block = 0; Block < Blocks; ++Block)
			{
				const float WidthU = FMath::Min(Random.FRandRange(5.f, 12.f), SizeCm[AxisU] * 0.6f);
				const float WidthV = FMath::Min(Random.FRandRange(4.f, 10.f), SizeCm[AxisV] * 0.6f);

				// In the part's own units the box runs from -50 to 50 on each axis.
				FVector3f PatchScale;
				PatchScale[Axis] = 0.4f / FMath::Max(SizeCm[Axis], 0.1f);
				PatchScale[AxisU] = WidthU / FMath::Max(SizeCm[AxisU], 0.1f);
				PatchScale[AxisV] = WidthV / FMath::Max(SizeCm[AxisV], 0.1f);

				const float RoomU = 50.f - PatchScale[AxisU] * 50.f;
				const float RoomV = 50.f - PatchScale[AxisV] * 50.f;
				if (Block == 0)
				{
					U = Random.FRandRange(-RoomU, RoomU);
					V = Random.FRandRange(-RoomV, RoomV);
				}
				else
				{
					U = FMath::Clamp(U + PatchScale[AxisU] * 50.f * (Random.FRand() < 0.5f ? -1.f : 1.f), -RoomU, RoomU);
					V = FMath::Clamp(V + PatchScale[AxisV] * 60.f, -RoomV, RoomV);
				}

				FVector3f Location;
				// 0.35 cm off the face so it never flickers against it.
				Location[Axis] = Sign * (50.f + 35.f / FMath::Max(SizeCm[Axis], 0.1f));
				Location[AxisU] = U;
				Location[AxisV] = V;

				const FName Name(*FString::Printf(TEXT("%s_Camo%d"), *Part->GetName(), PatchIndex++));
				UStaticMeshComponent* Patch = CreatePart(Name, Part, Cube, FVector(Location), FVector(PatchScale), Paint);
				Patch->SetCastShadow(false);
				if (bOnExo)
				{
					Patch->ComponentTags.Add(JavelinSuitTags::Exo07);
				}
				if (bOnSemper)
				{
					Patch->ComponentTags.Add(JavelinSuitTags::Semper03);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------
// Showing, painting and tuning the current suit
// ---------------------------------------------------------------------------------------------------------

void AJavelinCharacter::UpdateSuitVisibility(const TArray<UPrimitiveComponent*>& Parts) const
{
	const FName SuitTag = JavelinSuitTags::ForClass(SuitClass);
	for (UPrimitiveComponent* Part : Parts)
	{
		ShowPartForSuit(Part, SuitTag);
	}
}

void AJavelinCharacter::UpdateSuitVisibility()
{
	const FName SuitTag = JavelinSuitTags::ForClass(SuitClass);
	TInlineComponentArray<UPrimitiveComponent*> Parts(this);
	for (UPrimitiveComponent* Part : Parts)
	{
		ShowPartForSuit(Part, SuitTag);
	}
}

void AJavelinCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// So a suit picked in the editor shows right away in the viewport.
	UpdateSuitVisibility();
}

FJavelinSuitPaint AJavelinCharacter::MakeSuitPaint(EJavelinSuitClass Class) const
{
	FJavelinSuitPaint Paint;
	switch (Class)
	{
	case EJavelinSuitClass::Exo07:
		// Woodland MARPAT: olive drab plates with brown, green and black blocks, gunmetal frame, worn metal edges.
		Paint.Primary = SuitColor(92, 94, 62);
		Paint.Secondary = SuitColor(80, 82, 57);
		Paint.Trim = SuitColor(128, 126, 116);
		Paint.Frame = SuitColor(42, 42, 41);
		Paint.Accent = SuitColor(26, 90, 255);
		Paint.Camo[0] = SuitColor(72, 56, 38);
		Paint.Camo[1] = SuitColor(46, 62, 36);
		Paint.Camo[2] = SuitColor(26, 27, 23);
		Paint.Emblem = SuitColor(158, 152, 138);
		Paint.MuzzleFlash = FLinearColor(1.f, 0.62f, 0.22f);
		break;
	case EJavelinSuitClass::Semper03:
		// Desert digital: sand plates with tan, brown and pale blocks, dark reinforced metal frame.
		Paint.Primary = SuitColor(194, 168, 120);
		Paint.Secondary = SuitColor(172, 146, 102);
		Paint.Trim = SuitColor(84, 78, 68);
		Paint.Frame = SuitColor(36, 35, 33);
		Paint.Accent = SuitColor(150, 160, 166);
		Paint.Camo[0] = SuitColor(146, 116, 78);
		Paint.Camo[1] = SuitColor(104, 82, 56);
		Paint.Camo[2] = SuitColor(218, 198, 154);
		Paint.Emblem = SuitColor(98, 86, 68);
		Paint.MuzzleFlash = FLinearColor(1.f, 0.8f, 0.35f);
		break;
	default:
		Paint.Primary = PrimaryColor;
		Paint.Secondary = SecondaryColor;
		Paint.Trim = TrimColor;
		Paint.Frame = FrameColor;
		Paint.Accent = AccentColor;
		Paint.Camo[0] = Paint.Camo[1] = Paint.Camo[2] = PrimaryColor;
		Paint.Emblem = TrimColor;
		Paint.MuzzleFlash = MuzzleFlashColor;
		break;
	}
	return Paint;
}

void AJavelinCharacter::ApplyPaint()
{
	const FJavelinSuitPaint Paint = MakeSuitPaint(SuitClass);
	const bool bMarine = SuitClass != EJavelinSuitClass::Ranger;

	struct FPaintSlot
	{
		FName Tag;
		FLinearColor Color;
	};
	const FPaintSlot Slots[] = {
		{ SuitPaint::Primary, Paint.Primary },
		{ SuitPaint::Secondary, Paint.Secondary },
		{ SuitPaint::Trim, Paint.Trim },
		{ SuitPaint::Frame, Paint.Frame },
		{ SuitPaint::Accent, Paint.Accent },
		{ SuitPaint::Muzzle, Paint.MuzzleFlash },
		{ SuitPaint::Emblem, Paint.Emblem },
		{ SuitPaint::Camo0, Paint.Camo[0] },
		{ SuitPaint::Camo1, Paint.Camo[1] },
		{ SuitPaint::Camo2, Paint.Camo[2] },
		{ SuitPaint::FlagRed, SuitColor(150, 28, 36) },
		{ SuitPaint::FlagWhite, SuitColor(220, 216, 205) },
		{ SuitPaint::FlagBlue, SuitColor(30, 40, 92) },
	};

	VisorMaterials.Reset();
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part->ComponentHasTag(SuitPaint::Glow))
		{
			// One shared material so every thruster glow and flame changes color together.
			if (!ThrusterMaterial)
			{
				ThrusterMaterial = UMaterialInstanceDynamic::Create(Part->GetMaterial(0), this);
			}
			Part->SetMaterial(0, ThrusterMaterial);
			continue;
		}
		if (Part->ComponentHasTag(SuitPaint::Visor))
		{
			// Colored by UpdateVisor, to match the visor mode.
			if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
			{
				VisorMaterials.AddUnique(Material);
			}
			continue;
		}
		for (const FPaintSlot& Slot : Slots)
		{
			if (Part->ComponentHasTag(Slot.Tag))
			{
				FLinearColor Color = Slot.Color;
				if (bMarine && (Slot.Tag == SuitPaint::Primary || Slot.Tag == SuitPaint::Secondary))
				{
					// Plates that have taken a beating don't all match: each is a touch lighter or darker.
					const float Wear = 0.9f + 0.18f * static_cast<float>(GetTypeHash(Part->GetFName()) % 100) / 100.f;
					Color = FLinearColor(Color.R * Wear, Color.G * Wear, Color.B * Wear, Color.A);
				}
				SetSuitPartColor(Part, Color);
				break;
			}
		}
	}
	MuzzleLight->SetLightColor(Paint.MuzzleFlash);
}

void AJavelinCharacter::CaptureRangerStats()
{
	if (bCapturedRangerStats)
	{
		return;
	}
	bCapturedRangerStats = true;

	// The Ranger plays exactly as before: its numbers are whatever the components are set to.
	FJavelinSuitStats& S = RangerStats;
	S.Name = NSLOCTEXT("Javelin", "RangerName", "RANGER");
	S.Role = NSLOCTEXT("Javelin", "RangerRole", "All-Rounder");

	S.WeaponName = Weapon->WeaponName;
	S.Damage = Weapon->Damage;
	S.FireRate = Weapon->FireRate;
	S.MagazineSize = Weapon->MagazineSize;
	S.MaxReserveAmmo = Weapon->MaxReserveAmmo;
	S.ReloadTime = Weapon->ReloadTime;
	S.HipSpread = Weapon->HipSpread;
	S.AimSpread = Weapon->AimSpread;
	S.SpreadPerShot = Weapon->SpreadPerShot;
	S.MaxBloom = Weapon->MaxBloom;
	S.RecoilPitch = Weapon->RecoilPitch;
	S.RecoilYaw = Weapon->RecoilYaw;
	S.HitImpulse = Weapon->HitImpulse;
	S.TracerColor = Weapon->TracerColor;
	S.SpinUpTime = Weapon->SpinUpTime;

	S.MaxShields = SuitHealth->MaxShields;
	S.MaxArmor = SuitHealth->MaxArmor;
	S.ShieldRegenDelay = SuitHealth->ShieldRegenDelay;
	S.ShieldRegenPerSecond = SuitHealth->ShieldRegenPerSecond;

	const UJavelinMovementComponent* Movement = GetJavelinMovement();
	S.WalkSpeed = Movement->MaxWalkSpeed;
	S.SprintSpeed = Movement->SprintSpeed;
	S.DodgeSpeed = Movement->DodgeSpeed;
	S.DodgeCooldown = Movement->DodgeCooldown;
	S.FlightCruiseSpeed = Movement->FlightCruiseSpeed;
	S.FlightBoostSpeed = Movement->FlightBoostSpeed;
	S.FlightTurnRate = Movement->FlightTurnRate;
	S.HoverSpeed = Movement->HoverSpeed;

	S.FlightHeatPerSecond = Heat->FlightHeatPerSecond;
	S.HoverHeatPerSecond = Heat->HoverHeatPerSecond;
	S.PassiveCoolingPerSecond = Heat->PassiveCoolingPerSecond;

	S.GrenadeCooldown = Abilities->GrenadeSlot.Cooldown;
	S.GrenadeDamage = Abilities->GrenadeDamage;
	S.GrenadeBlastRadius = Abilities->GrenadeBlastRadius;
	S.MissileCooldown = Abilities->MissileSlot.Cooldown;
	S.MissileDamage = Abilities->MissileDamage;
}

FJavelinSuitStats AJavelinCharacter::MakeSuitStats(EJavelinSuitClass Class) const
{
	// Anything not set here stays as the Ranger has it.
	FJavelinSuitStats S = RangerStats;
	switch (Class)
	{
	case EJavelinSuitClass::Exo07:
		// Heavy assault and area denial: a walking fortress. Huge magazine and armor, slow on its feet and in the air,
		// runs hotter. The HMG-88 spins up before it fires, then hoses the area with suppressing fire.
		S.Name = NSLOCTEXT("Javelin", "Exo07Name", "USMC EXO-07");
		S.Role = NSLOCTEXT("Javelin", "Exo07Role", "Heavy Assault / Area Denial");
		S.WeaponName = NSLOCTEXT("Javelin", "Hmg88", "HMG-88 HEAVY MACHINE GUN");
		S.Damage = 24.f;
		S.FireRate = 780.f;
		S.MagazineSize = 150;
		S.MaxReserveAmmo = 600;
		S.ReloadTime = 3.4f;
		S.HipSpread = 3.f;
		S.AimSpread = 1.f;
		S.SpreadPerShot = 0.12f;
		S.MaxBloom = 2.5f;
		S.RecoilPitch = 0.22f;
		S.RecoilYaw = 0.22f;
		S.HitImpulse = 70.f;
		S.TracerColor = FLinearColor(1.f, 0.55f, 0.18f);
		S.SpinUpTime = 0.45f;

		S.MaxShields = 220.f;
		S.MaxArmor = 650.f;
		S.ShieldRegenDelay = 4.5f;
		S.ShieldRegenPerSecond = 55.f;

		S.WalkSpeed = 440.f;
		S.SprintSpeed = 820.f;
		S.DodgeSpeed = 1800.f;
		S.DodgeCooldown = 1.f;
		S.FlightCruiseSpeed = 2700.f;
		S.FlightBoostSpeed = 4000.f;
		S.FlightTurnRate = 140.f;
		S.HoverSpeed = 480.f;

		S.FlightHeatPerSecond = 10.5f;
		S.HoverHeatPerSecond = 5.f;
		S.PassiveCoolingPerSecond = 18.f;

		// Frags come round quicker and hit harder: three on the sheet, so they're the EXO's go-to.
		S.GrenadeCooldown = 6.f;
		S.GrenadeDamage = 230.f;
		S.GrenadeBlastRadius = 600.f;
		S.MissileCooldown = 14.f;
		break;

	case EJavelinSuitClass::Semper03:
		// Assault and defense: a frontline breacher. Accurate rifle, sturdy shields, and the ballistic shield to hold
		// a line. The thruster pack makes it the quickest dodger and a cooler, faster flier.
		S.Name = NSLOCTEXT("Javelin", "Semper03Name", "SEMPER-03");
		S.Role = NSLOCTEXT("Javelin", "Semper03Role", "Assault / Defense");
		S.WeaponName = NSLOCTEXT("Javelin", "Semper03Rifle", "M27-X ASSAULT RIFLE");
		S.Damage = 32.f;
		S.FireRate = 600.f;
		S.MagazineSize = 36;
		S.MaxReserveAmmo = 288;
		S.ReloadTime = 1.9f;
		S.HipSpread = 2.2f;
		S.AimSpread = 0.5f;
		S.SpreadPerShot = 0.3f;
		S.MaxBloom = 2.8f;
		S.RecoilPitch = 0.4f;
		S.RecoilYaw = 0.18f;
		S.HitImpulse = 50.f;
		S.TracerColor = FLinearColor(1.f, 0.78f, 0.35f);
		S.SpinUpTime = 0.f;

		S.MaxShields = 260.f;
		S.MaxArmor = 420.f;
		S.ShieldRegenDelay = 3.5f;
		S.ShieldRegenPerSecond = 70.f;

		S.WalkSpeed = 500.f;
		S.SprintSpeed = 960.f;
		S.DodgeSpeed = 2600.f;
		S.DodgeCooldown = 0.6f;
		S.FlightCruiseSpeed = 3100.f;
		S.FlightBoostSpeed = 4700.f;
		S.FlightTurnRate = 175.f;
		S.HoverSpeed = 620.f;

		S.FlightHeatPerSecond = 8.5f;
		S.HoverHeatPerSecond = 3.5f;
		S.PassiveCoolingPerSecond = 22.f;
		break;

	default:
		break;
	}
	return S;
}

void AJavelinCharacter::BuildVisorModes()
{
	VisorModes.Reset();
	switch (SuitClass)
	{
	case EJavelinSuitClass::Exo07:
		VisorModes.Add(FJavelinVisorMode{ TEXT("STANDARD"), TEXT("Operational"), FLinearColor(0.1f, 0.35f, 1.f), EJavelinVisorEffect::None });
		VisorModes.Add(FJavelinVisorMode{ TEXT("SUPPORT"), TEXT("Systems Active"), FLinearColor(0.15f, 1.f, 0.15f), EJavelinVisorEffect::Support });
		VisorModes.Add(FJavelinVisorMode{ TEXT("STEALTH"), TEXT("Low Profile"), FLinearColor(0.5f, 0.1f, 1.f), EJavelinVisorEffect::LowProfile });
		CombatVisor = FJavelinVisorMode{ TEXT("COMBAT"), TEXT("Target Lock"), FLinearColor(1.f, 0.05f, 0.03f), EJavelinVisorEffect::Combat };
		AlertVisor = FJavelinVisorMode{ TEXT("ALERT"), TEXT("Threat Detected"), FLinearColor(1.f, 0.5f, 0.03f), EJavelinVisorEffect::Alert };
		bHasAutoVisors = true;
		break;
	case EJavelinSuitClass::Semper03:
		VisorModes.Add(FJavelinVisorMode{ TEXT("NORMAL"), TEXT("Optical"), SuitColor(150, 160, 166), EJavelinVisorEffect::None });
		VisorModes.Add(FJavelinVisorMode{ TEXT("TACTICAL"), TEXT("Night Vision"), FLinearColor(0.1f, 1.f, 0.2f), EJavelinVisorEffect::NightVision });
		VisorModes.Add(FJavelinVisorMode{ TEXT("SCAN"), TEXT("Recon"), FLinearColor(0.15f, 0.5f, 1.f), EJavelinVisorEffect::Scan });
		CombatVisor = FJavelinVisorMode{ TEXT("TARGETING"), TEXT("Combat Lock"), FLinearColor(1.f, 0.08f, 0.05f), EJavelinVisorEffect::Combat };
		AlertVisor = FJavelinVisorMode{ TEXT("TARGETING"), TEXT("Threat Detected"), FLinearColor(1.f, 0.08f, 0.05f), EJavelinVisorEffect::Alert };
		bHasAutoVisors = true;
		break;
	default:
		VisorModes.Add(FJavelinVisorMode{ TEXT("STANDARD"), TEXT("Operational"), AccentColor, EJavelinVisorEffect::None });
		CombatVisor = VisorModes[0];
		AlertVisor = VisorModes[0];
		bHasAutoVisors = false;
		break;
	}
	VisorIndex = 0;
	ShownVisorKey = -100;
}

void AJavelinCharacter::ApplySuitClass()
{
	ActiveStats = MakeSuitStats(SuitClass);
	UpdateSuitVisibility();

	// Swap guns: EXO-07 fires from the HMG-88, the others from the rifle.
	const bool bHmg = SuitClass == EJavelinSuitClass::Exo07;
	UStaticMeshComponent* NewFlash = bHmg ? HmgMuzzleFlash : RifleMuzzleFlash;
	if (MuzzleFlash && MuzzleFlash != NewFlash)
	{
		MuzzleFlash->SetVisibility(false);
	}
	Muzzle = bHmg ? HmgMuzzle : RifleMuzzle;
	MuzzleFlash = NewFlash;
	MuzzleFlashScale = bHmg ? FVector(0.24f, 0.24f, 0.4f) : FVector(0.14f, 0.14f, 0.24f);
	MuzzleLight->AttachToComponent(Muzzle, FAttachmentTransformRules::KeepRelativeTransform);
	Weapon->SetMuzzle(Muzzle);

	ApplyPaint();

	const FJavelinSuitStats& S = ActiveStats;
	Weapon->WeaponName = S.WeaponName;
	Weapon->Damage = S.Damage;
	Weapon->FireRate = S.FireRate;
	Weapon->MagazineSize = S.MagazineSize;
	Weapon->MaxReserveAmmo = S.MaxReserveAmmo;
	Weapon->ReloadTime = S.ReloadTime;
	Weapon->HipSpread = S.HipSpread;
	Weapon->AimSpread = S.AimSpread;
	Weapon->SpreadPerShot = S.SpreadPerShot;
	Weapon->MaxBloom = S.MaxBloom;
	Weapon->RecoilPitch = S.RecoilPitch;
	Weapon->RecoilYaw = S.RecoilYaw;
	Weapon->HitImpulse = S.HitImpulse;
	Weapon->TracerColor = S.TracerColor;
	Weapon->SpinUpTime = S.SpinUpTime;
	Weapon->ResetAmmo();

	SuitHealth->SetMaxValues(S.MaxShields, S.MaxArmor);
	SuitHealth->ShieldRegenDelay = S.ShieldRegenDelay;
	SuitHealth->ShieldRegenPerSecond = S.ShieldRegenPerSecond;

	UJavelinMovementComponent* Movement = GetJavelinMovement();
	Movement->MaxWalkSpeed = S.WalkSpeed;
	Movement->SprintSpeed = S.SprintSpeed;
	Movement->DodgeSpeed = S.DodgeSpeed;
	Movement->DodgeCooldown = S.DodgeCooldown;
	Movement->FlightCruiseSpeed = S.FlightCruiseSpeed;
	Movement->FlightBoostSpeed = S.FlightBoostSpeed;
	Movement->FlightTurnRate = S.FlightTurnRate;
	Movement->HoverSpeed = S.HoverSpeed;

	Heat->FlightHeatPerSecond = S.FlightHeatPerSecond;
	Heat->HoverHeatPerSecond = S.HoverHeatPerSecond;
	Heat->PassiveCoolingPerSecond = S.PassiveCoolingPerSecond;

	Abilities->GrenadeSlot.Cooldown = S.GrenadeCooldown;
	Abilities->GrenadeDamage = S.GrenadeDamage;
	Abilities->GrenadeBlastRadius = S.GrenadeBlastRadius;
	Abilities->MissileSlot.Cooldown = S.MissileCooldown;
	Abilities->MissileDamage = S.MissileDamage;

	// Ballistic shield back at the side, at full strength.
	bWantsShield = false;
	ShieldRaise = 0.f;
	ShieldIntegrity = ShieldMaxIntegrity;
	ShieldBrokenUntil = -100.f;
	LastShieldHitTime = -100.f;
	ShieldPivot->SetRelativeLocationAndRotation(ShieldCarriedLocation, ShieldCarriedRotation);

	BuildVisorModes();
	UpdateVisor();
}

// ---------------------------------------------------------------------------------------------------------
// Switching suits
// ---------------------------------------------------------------------------------------------------------

bool AJavelinCharacter::CanChangeSuit() const
{
	return !IsSuitDown() && GetJavelinMovement()->IsMovingOnGround();
}

bool AJavelinCharacter::SetSuitClass(EJavelinSuitClass NewClass)
{
	if (NewClass == SuitClass)
	{
		return true;
	}
	if (!HasActorBegunPlay())
	{
		SuitClass = NewClass;
		UpdateSuitVisibility();
		return true;
	}
	if (!CanChangeSuit())
	{
		SuitChangeDeniedTime = GetWorld()->GetTimeSeconds();
		return false;
	}

	SuitClass = NewClass;
	ApplySuitClass();
	SuitChangedTime = GetWorld()->GetTimeSeconds();

	// A burst of dust and a jolt as the new suit locks on.
	ADustEffect::Spawn(GetWorld(), GetFeetLocation(), 160.f, 0.8f, 7, DustColor, 0.9f);
	AddCameraTrauma(0.2f);
	PlayRumble(0.4f, 0.2f, true);
	return true;
}

float AJavelinCharacter::GetTimeSinceSuitChanged() const
{
	return GetWorld()->GetTimeSeconds() - SuitChangedTime;
}

float AJavelinCharacter::GetTimeSinceSuitChangeDenied() const
{
	return GetWorld()->GetTimeSeconds() - SuitChangeDeniedTime;
}

void AJavelinCharacter::OnSuitRangerPressed()
{
	SetSuitClass(EJavelinSuitClass::Ranger);
}

void AJavelinCharacter::OnSuitExoPressed()
{
	SetSuitClass(EJavelinSuitClass::Exo07);
}

void AJavelinCharacter::OnSuitSemperPressed()
{
	SetSuitClass(EJavelinSuitClass::Semper03);
}

void AJavelinCharacter::OnSuitNextPressed()
{
	SetSuitClass(static_cast<EJavelinSuitClass>((static_cast<int32>(SuitClass) + 1) % 3));
}

void AJavelinCharacter::OnSuitPreviousPressed()
{
	SetSuitClass(static_cast<EJavelinSuitClass>((static_cast<int32>(SuitClass) + 2) % 3));
}

// ---------------------------------------------------------------------------------------------------------
// Visor modes
// ---------------------------------------------------------------------------------------------------------

const FJavelinVisorMode& AJavelinCharacter::GetSelectedVisorMode() const
{
	static const FJavelinVisorMode Fallback{ TEXT("STANDARD"), TEXT("Operational"), FLinearColor(0.1f, 0.8f, 1.f), EJavelinVisorEffect::None };
	return VisorModes.IsValidIndex(VisorIndex) ? VisorModes[VisorIndex] : Fallback;
}

const FJavelinVisorMode& AJavelinCharacter::GetVisorMode() const
{
	if (ShownVisorKey == -1)
	{
		return CombatVisor;
	}
	if (ShownVisorKey == -2)
	{
		return AlertVisor;
	}
	return GetSelectedVisorMode();
}

float AJavelinCharacter::GetTimeSinceVisorChanged() const
{
	return GetWorld()->GetTimeSeconds() - VisorChangedTime;
}

float AJavelinCharacter::GetDetectionRangeScale() const
{
	// Stealth only holds while the visor is on it. Shooting or getting hit flips it to Combat or Alert and gives you away.
	return GetSelectedVisorMode().Effect == EJavelinVisorEffect::LowProfile && ShownVisorKey >= 0 && !IsSuitDown() ? 0.5f : 1.f;
}

bool AJavelinCharacter::IsScanVisorActive() const
{
	return GetSelectedVisorMode().Effect == EJavelinVisorEffect::Scan && !IsSuitDown();
}

void AJavelinCharacter::OnVisorPressed()
{
	if (VisorModes.Num() <= 1 || IsSuitDown())
	{
		return;
	}
	VisorIndex = (VisorIndex + 1) % VisorModes.Num();
	ShownVisorKey = -100;
	UpdateVisor();
}

void AJavelinCharacter::UpdateVisor()
{
	if (!Weapon || !SuitHealth || !GetWorld())
	{
		return;
	}

	// Combat (aiming, shooting or holding the shield up) and Alert (just hit) take over the visor by themselves.
	int32 Key = VisorIndex;
	if (bHasAutoVisors && !IsSuitDown())
	{
		if (Weapon->IsAiming() || Weapon->GetTimeSinceLastShot() < VisorCombatHoldTime || IsShieldRaised())
		{
			Key = -1;
		}
		else if (SuitHealth->GetTimeSinceDamage() < VisorAlertHoldTime)
		{
			Key = -2;
		}
	}
	if (Key == ShownVisorKey)
	{
		return;
	}
	ShownVisorKey = Key;
	VisorChangedTime = GetWorld()->GetTimeSeconds();

	const FJavelinVisorMode& Shown = GetVisorMode();
	for (UMaterialInstanceDynamic* Material : VisorMaterials)
	{
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("Color"), Shown.Color);
		}
	}
	const bool bLowProfile = GetDetectionRangeScale() < 1.f;
	VisorLight->SetLightColor(Shown.Color);
	VisorLight->SetIntensity(bLowProfile ? 1.5f : 6.f);

	// What the picked mode does stays on under Combat and Alert, except stealth, which shooting breaks.
	const EJavelinVisorEffect Effect = GetSelectedVisorMode().Effect;

	// Support: shields start recharging sooner and refill faster.
	const bool bSupport = Effect == EJavelinVisorEffect::Support;
	SuitHealth->ShieldRegenDelay = bSupport ? FMath::Max(0.5f, ActiveStats.ShieldRegenDelay - 1.5f) : ActiveStats.ShieldRegenDelay;
	SuitHealth->ShieldRegenPerSecond = ActiveStats.ShieldRegenPerSecond * (bSupport ? 1.35f : 1.f);

	// Tactical: night vision. Brighter, washed out and green.
	const bool bNightVision = Effect == EJavelinVisorEffect::NightVision;
	FPostProcessSettings& View = FollowCamera->PostProcessSettings;
	View.bOverride_AutoExposureBias = bNightVision;
	View.AutoExposureBias = 1.5f;
	View.bOverride_ColorSaturation = bNightVision;
	View.ColorSaturation = FVector4(0.25f, 0.25f, 0.25f, 1.f);
	View.bOverride_ColorGain = bNightVision;
	View.ColorGain = FVector4(0.55f, 1.35f, 0.6f, 1.f);
	View.bOverride_VignetteIntensity = bNightVision;
	View.VignetteIntensity = 0.75f;
}

// ---------------------------------------------------------------------------------------------------------
// SEMPER-03's ballistic shield
// ---------------------------------------------------------------------------------------------------------

bool AJavelinCharacter::IsShieldRaised() const
{
	return HasBallisticShield() && ShieldRaise >= 0.7f && !IsShieldBroken();
}

bool AJavelinCharacter::IsShieldBroken() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < ShieldBrokenUntil;
}

float AJavelinCharacter::GetShieldIntegrityPercent() const
{
	return ShieldMaxIntegrity > 0.f ? FMath::Clamp(ShieldIntegrity / ShieldMaxIntegrity, 0.f, 1.f) : 0.f;
}

float AJavelinCharacter::GetTimeSinceShieldBlock() const
{
	return GetWorld()->GetTimeSeconds() - LastShieldHitTime;
}

void AJavelinCharacter::OnShieldPressed()
{
	bWantsShield = true;
}

void AJavelinCharacter::OnShieldReleased()
{
	bWantsShield = false;
}

void AJavelinCharacter::OnShieldTogglePressed()
{
	bWantsShield = !bWantsShield;
}

void AJavelinCharacter::UpdateShield(float DeltaSeconds)
{
	if (!HasBallisticShield())
	{
		ShieldRaise = 0.f;
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	UJavelinMovementComponent* Movement = GetJavelinMovement();

	// The shield is for fighting on foot: flying or hovering stows it, and a broken one has to recover first.
	const bool bCanRaise = !IsSuitDown() && !IsShieldBroken() && !Movement->IsInFlight() && !Movement->IsHovering();
	ShieldRaise = FMath::FInterpConstantTo(ShieldRaise, bWantsShield && bCanRaise ? 1.f : 0.f, DeltaSeconds, ShieldRaiseSpeed);

	// It recovers once it hasn't been hit for a little while.
	if (ShieldIntegrity < ShieldMaxIntegrity && Now - LastShieldHitTime > ShieldRecoverDelay)
	{
		ShieldIntegrity = FMath::Min(ShieldMaxIntegrity, ShieldIntegrity + ShieldRecoverPerSecond * DeltaSeconds);
	}

	// Heavy to hold up: slower walking and no sprinting.
	Movement->MaxWalkSpeed = ActiveStats.WalkSpeed * FMath::Lerp(1.f, ShieldMoveSpeedScale, ShieldRaise);
	if (ShieldRaise > 0.3f)
	{
		Movement->SetSprinting(false);
	}

	// Swing it from the side up to the front.
	const float Raise = FMath::SmoothStep(0.f, 1.f, ShieldRaise);
	const FVector Location = FMath::Lerp(ShieldCarriedLocation, ShieldRaisedLocation, Raise);
	const FQuat Rotation = FQuat::Slerp(ShieldCarriedRotation.Quaternion(), ShieldRaisedRotation.Quaternion(), Raise);
	ShieldPivot->SetRelativeLocationAndRotation(Location, Rotation);
}

float AJavelinCharacter::AbsorbWithShield(float Damage, const FVector& HitDirection)
{
	if (!IsShieldRaised() || Damage <= 0.f)
	{
		return Damage;
	}

	// HitDirection points from the attacker to the suit, so a hit from the front points against the suit's facing.
	const FVector TowardAttacker = -HitDirection.GetSafeNormal2D();
	const float CosAngle = FMath::Cos(FMath::DegreesToRadians(ShieldBlockAngle));
	if (TowardAttacker.IsNearlyZero() || FVector::DotProduct(TowardAttacker, GetActorForwardVector()) < CosAngle)
	{
		return Damage;
	}

	const float Absorbed = FMath::Min(Damage, ShieldIntegrity);
	ShieldIntegrity -= Absorbed;
	LastShieldHitTime = GetWorld()->GetTimeSeconds();

	// Rounds clanging off the shield: a small jolt, much less than a hit on the suit.
	AddCameraTrauma(0.05f + 0.1f * FMath::Clamp(Absorbed / 60.f, 0.f, 1.f));
	PlayRumble(0.25f, 0.08f, false);

	if (ShieldIntegrity <= 0.f)
	{
		// Broken: it drops and can't come back up for a few seconds.
		ShieldIntegrity = 0.f;
		ShieldBrokenUntil = LastShieldHitTime + ShieldBrokenTime;
		AddCameraTrauma(0.35f);
		PlayRumble(0.8f, 0.3f, true);
	}
	return Damage - Absorbed;
}

// ---------------------------------------------------------------------------------------------------------
// Per-frame extras
// ---------------------------------------------------------------------------------------------------------

void AJavelinCharacter::UpdateSuitExtras(float DeltaSeconds)
{
	// The HMG-88's barrels whirl as they spin up and keep turning a moment after you let go.
	if (SuitClass == EJavelinSuitClass::Exo07 && HmgBarrels)
	{
		BarrelAngle = FMath::Fmod(BarrelAngle + Weapon->GetBarrelSpin() * 1440.f * DeltaSeconds, 360.f);
		HmgBarrels->SetRelativeRotation(FRotator(0.f, BarrelAngle, 0.f));
	}
}
