#include "JungleWeather.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr float Meter = 100.f;

	// Rain falls from this high above the camera down to this far below it. Meters.
	constexpr float RainTop = 14.f;
	constexpr float RainBottom = 9.f;

	// The sun hands over to the moon this far below the horizon, in degrees, with both dark at that moment.
	constexpr float HandOver = -4.f;

	const FLinearColor LowSunColor(1.f, 0.52f, 0.28f);
	const FLinearColor HighSunColor(1.f, 0.93f, 0.84f);
	const FLinearColor StormLightColor(0.78f, 0.83f, 0.9f);
	const FLinearColor StormFogColor(0.1f, 0.12f, 0.13f);
	const FLinearColor FlashFogColor(0.35f, 0.4f, 0.5f);

	/** A little pseudo-random number from 0 to 1 for each raindrop, so each streak keeps its own length. */
	float DropRandom(int32 Index)
	{
		return FMath::Frac(Index * 0.6180339f + 0.13f);
	}
}

AJungleWeather::AJungleWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the camera has moved, so the rain is placed around where the camera is this frame.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Thin streaks. They don't block, cast shadows or feed into the global lighting, which keeps them cheap.
	Rain = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rain"));
	Rain->SetupAttachment(RootComponent);
	Rain->SetStaticMesh(CubeMesh.Object);
	Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rain->SetCanEverAffectNavigation(false);
	Rain->SetCastShadow(false);
	Rain->bAffectDistanceFieldLighting = false;
	Rain->bAffectDynamicIndirectLighting = false;
	Rain->bVisibleInRayTracing = false;
	Rain->SetReceivesDecals(false);

	Lightning = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Lightning"));
	Lightning->SetupAttachment(RootComponent);
	Lightning->SetMobility(EComponentMobility::Movable);
	Lightning->SetCastShadows(false);
	Lightning->SetIntensity(0.f);
	Lightning->SetLightColor(FLinearColor(0.78f, 0.85f, 1.f));
	Lightning->SetAtmosphereSunLight(false);
	Lightning->SetVolumetricScatteringIntensity(3.f);
	Lightning->SetVisibility(false);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 1.f;
}

void AJungleWeather::BeginPlay()
{
	Super::BeginPlay();

	TimeOfDay = FMath::Fmod(TimeOfDay, 24.f);
	StormStrength = ForcedStorm >= 0.f ? ForcedStorm : FMath::Lerp(MinStorm, MaxStorm, 0.6f);
	FindLevelLighting();

	if (UMaterialInterface* Base = Rain->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetVectorParameterValue(TEXT("Color"), RainColor);
		Rain->SetMaterial(0, Material);
	}

	// All the drops exist from the start; the ones not falling right now are shrunk to nothing.
	Drops.SetNum(MaxRaindrops);
	DropTransforms.SetNum(MaxRaindrops);
	FVector Camera = GetActorLocation();
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Camera = CameraManager->GetCameraLocation();
	}
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const FVector2D Spot = FMath::RandPointInCircle(RainRadius * Meter);
		Drops[Index] = Camera + FVector(Spot.X, Spot.Y, FMath::FRandRange(-RainBottom, RainTop) * Meter);
		DropTransforms[Index] = FTransform(FQuat::Identity, Drops[Index], FVector::ZeroVector);
	}
	Rain->AddInstances(DropTransforms, false, true);

	NextLightning = FMath::FRandRange(3.f, 6.f);
	UpdateSunAndMoon();
	UpdateAtmosphere();
}

void AJungleWeather::FindLevelLighting()
{
	UWorld* World = GetWorld();

	// The level's sun (or the one the jungle made). It has to be movable to move across the sky.
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->GetOwner() != this && !It->IsTemplate())
		{
			Sun = *It;
			break;
		}
	}
	if (!Sun.IsValid())
	{
		UDirectionalLightComponent* NewSun = NewObject<UDirectionalLightComponent>(this, TEXT("WeatherSun"));
		NewSun->SetMobility(EComponentMobility::Movable);
		NewSun->SetupAttachment(RootComponent);
		NewSun->RegisterComponent();
		Sun = NewSun;
	}
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->SetCastShadows(true);
	Sun->SetVolumetricScatteringIntensity(1.5f);
	// Small contact shadows ground the plants and rocks where the regular shadows are too coarse.
	Sun->ContactShadowLength = 0.03f;
	Sun->bCastCloudShadows = bVolumetricClouds;
	Sun->MarkRenderStateDirty();

	// The sky light has to recapture the sky as it changes, from day to storm to night.
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && !It->IsTemplate())
		{
			SkyLight = *It;
			break;
		}
	}
	if (SkyLight.IsValid())
	{
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLight->bRealTimeCapture = true;
		SkyLight->MarkRenderStateDirty();
		SkyLight->RecaptureSky();
		BaseSkyLightIntensity = SkyLight->Intensity;
	}

	for (TObjectIterator<UExponentialHeightFogComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && !It->IsTemplate())
		{
			Fog = *It;
			break;
		}
	}
	if (Fog.IsValid())
	{
		BaseFogDensity = Fog->FogDensity;
		BaseFogColor = Fog->FogInscatteringLuminance;
	}

	// Volumetric clouds, using the simple cloud material that comes with the engine.
	bool bHasClouds = false;
	for (TObjectIterator<UVolumetricCloudComponent> It; It; ++It)
	{
		bHasClouds |= It->GetWorld() == World && !It->IsTemplate();
	}
	if (bVolumetricClouds && !bHasClouds)
	{
		UMaterialInterface* CloudMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
		if (CloudMaterial)
		{
			Clouds = NewObject<UVolumetricCloudComponent>(this, TEXT("StormClouds"));
			Clouds->SetupAttachment(RootComponent);
			Clouds->SetMaterial(CloudMaterial);
			// Low, heavy storm clouds. Kilometers.
			Clouds->SetLayerBottomAltitude(1.2f);
			Clouds->SetLayerHeight(7.f);
			Clouds->RegisterComponent();
		}
	}
}

void AJungleWeather::SetTimeOfDay(float Hour)
{
	TimeOfDay = FMath::Fmod(FMath::Fmod(Hour, 24.f) + 24.f, 24.f);
	SunUpdateTimer = 0.f;
	UpdateSunAndMoon();
	UpdateAtmosphere();
}

void AJungleWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateClock(DeltaSeconds);
	UpdateStorm(DeltaSeconds);
	UpdateLightning(DeltaSeconds);

	// Moving the sun redraws all of its shadows, so it only moves once a second. At normal speed that's a
	// quarter of a degree each time, too little to notice.
	SunUpdateTimer -= DeltaSeconds;
	if (SunUpdateTimer <= 0.f)
	{
		SunUpdateTimer = TimeSpeed > 4.f ? 0.1f : 1.f;
		UpdateSunAndMoon();
	}

	// The fog and sky light follow every frame during a lightning flash, otherwise a few times a second.
	AtmosphereTimer -= DeltaSeconds;
	if (AtmosphereTimer <= 0.f || GetFlash() > 0.f)
	{
		AtmosphereTimer = 0.2f;
		UpdateAtmosphere();
	}

	UpdateRain(DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------------------------------
// Time and storm

void AJungleWeather::UpdateClock(float DeltaSeconds)
{
	if (!bTimePasses)
	{
		return;
	}
	const bool bNight = GetSunElevation() < HandOver;
	const float HoursPerSecond = 24.f / (DayLengthMinutes * 60.f) * TimeSpeed * (bNight ? NightSpeed : 1.f);
	TimeOfDay = FMath::Fmod(TimeOfDay + HoursPerSecond * DeltaSeconds, 24.f);
}

void AJungleWeather::UpdateStorm(float DeltaSeconds)
{
	StormClock += DeltaSeconds;

	float Target = ForcedStorm;
	if (Target < 0.f)
	{
		// A slow swell plus some unevenness, so no two storm cycles are quite the same.
		const float Cycle = StormClock / (StormCycleMinutes * 60.f);
		const float Swell = 0.5f + 0.5f * FMath::Sin(Cycle * 2.f * UE_PI);
		const float Gusts = 0.5f + 0.5f * FMath::PerlinNoise1D(Cycle * 3.7f + 11.3f);
		Target = FMath::Lerp(MinStorm, MaxStorm, FMath::Clamp(Swell * 0.7f + Gusts * 0.3f, 0.f, 1.f));
	}
	StormStrength = FMath::FInterpConstantTo(StormStrength, FMath::Clamp(Target, 0.f, 1.f), DeltaSeconds, 0.15f);
}

float AJungleWeather::GetSunElevation() const
{
	const FVector Direction = GetSunDirection();
	return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Direction.Z, -1.f, 1.f)));
}

FVector AJungleWeather::GetSunDirection() const
{
	// The sun rises along East at 6, is highest at 12, sets along -East at 18, and is under the world at 0.
	const float Angle = (TimeOfDay - 6.f) / 24.f * 2.f * UE_PI;
	const float Tilt = FMath::DegreesToRadians(NoonSunHeight);
	const FVector East = FRotator(0.f, SunriseDirection, 0.f).Vector();
	const FVector North = FVector::CrossProduct(FVector::UpVector, East);
	const FVector Overhead = North * FMath::Cos(Tilt) + FVector::UpVector * FMath::Sin(Tilt);
	return (East * FMath::Cos(Angle) + Overhead * FMath::Sin(Angle)).GetSafeNormal();
}

float AJungleWeather::GetDaylight() const
{
	return FMath::SmoothStep(-6.f, 8.f, GetSunElevation());
}

// ---------------------------------------------------------------------------------------------------------------------
// Light

void AJungleWeather::UpdateSunAndMoon()
{
	if (!Sun.IsValid())
	{
		return;
	}

	// One light plays both parts: the sun by day and the moon by night. It hands over below the horizon,
	// where both are dark, so the switch can't be seen.
	const float Elevation = GetSunElevation();
	const FVector SunDirection = GetSunDirection();
	FVector LightFrom;
	float Intensity;
	FLinearColor Color;
	if (Elevation > HandOver)
	{
		LightFrom = SunDirection;
		Intensity = SunBrightness * (1.f - StormDimming * StormStrength) * FMath::SmoothStep(HandOver, 6.f, Elevation);
		Color = FLinearColor::LerpUsingHSV(LowSunColor, HighSunColor, FMath::SmoothStep(0.f, 25.f, Elevation));
		Color = FMath::Lerp(Color, StormLightColor, StormStrength * 0.6f);
	}
	else
	{
		LightFrom = -SunDirection;
		Intensity = MoonBrightness * (1.f - 0.5f * StormStrength) * FMath::SmoothStep(-HandOver, 14.f, -Elevation);
		Color = MoonColor;
	}

	Sun->SetWorldRotation((-LightFrom).Rotation());
	Sun->SetIntensity(Intensity);
	Sun->SetLightColor(Color);

	// Thick clouds spread the light out, so shadows get softer as the storm builds.
	Sun->LightSourceAngle = FMath::Lerp(0.6f, 3.f, StormStrength);
	Sun->MarkRenderStateDirty();
}

void AJungleWeather::UpdateAtmosphere()
{
	const float Daylight = GetDaylight();
	const float Flash = GetFlash();

	if (Fog.IsValid())
	{
		// Thicker in the storm and a little mistier at night. Gray-dark in the storm, nearly black at night.
		Fog->SetFogDensity(BaseFogDensity * FMath::Lerp(1.f, StormFog, StormStrength) * FMath::Lerp(1.2f, 1.f, Daylight));
		FLinearColor Color = FMath::Lerp(BaseFogColor, StormFogColor, StormStrength * 0.85f) * FMath::Lerp(0.03f, 1.f, Daylight);
		Color += FlashFogColor * Flash * StormStrength;
		Fog->SetFogInscatteringColor(Color);
		Fog->SetVolumetricFogExtinctionScale(FMath::Lerp(1.5f, 2.5f, StormStrength));
	}

	if (SkyLight.IsValid())
	{
		SkyLight->SetIntensity(BaseSkyLightIntensity * FMath::Lerp(1.f, 0.8f, StormStrength) * (1.f + 2.5f * Flash));
	}

	FPostProcessSettings& Settings = PostProcess->Settings;
	// Let the camera's exposure go darker at night and in the storm instead of brightening everything back up.
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = FMath::Lerp(NightExposure, 0.f, Daylight) - 0.4f * StormStrength;
	if (bColorGrade)
	{
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(1.f, 1.f, 1.f, FMath::Lerp(1.f, 0.72f, StormStrength));
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(1.f, 1.f, 1.f, 1.06f);
		// Lower is colder and bluer.
		Settings.bOverride_WhiteTemp = true;
		Settings.WhiteTemp = FMath::Lerp(6500.f, 5700.f, StormStrength);
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 0.5f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.45f;
		Settings.bOverride_FilmGrainIntensity = true;
		Settings.FilmGrainIntensity = 0.12f;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Lightning

float AJungleWeather::GetFlash() const
{
	// A strike flickers: a bright flash, a dimmer one, then a last big one that fades out.
	static const float Starts[] = { 0.f, 0.11f, 0.24f };
	static const float Strengths[] = { 1.f, 0.5f, 0.85f };
	static const float Fades[] = { 0.05f, 0.04f, 0.18f };
	if (SinceLightning > 1.5f)
	{
		return 0.f;
	}
	float Flash = 0.f;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float Time = SinceLightning - Starts[Index];
		if (Time >= 0.f)
		{
			Flash += Strengths[Index] * FMath::Exp(-Time / Fades[Index]);
		}
	}
	return Flash < 0.01f ? 0.f : FMath::Min(Flash, 1.f);
}

void AJungleWeather::UpdateLightning(float DeltaSeconds)
{
	SinceLightning += DeltaSeconds;
	NextLightning -= DeltaSeconds;

	if (NextLightning <= 0.f)
	{
		// Lightning comes more often in a heavier storm, and stops when it's just overcast.
		const float Activity = FMath::Clamp((StormStrength - 0.35f) / 0.65f, 0.f, 1.f);
		NextLightning = LightningEvery * FMath::FRandRange(0.4f, 1.6f) / FMath::Max(Activity, 0.25f);
		if (Activity > 0.f)
		{
			SinceLightning = 0.f;
			const FVector From = FRotator(FMath::FRandRange(35.f, 70.f), FMath::FRandRange(0.f, 360.f), 0.f).Vector();
			Lightning->SetWorldRotation((-From).Rotation());
		}
	}

	const float Flash = GetFlash();
	Lightning->SetVisibility(Flash > 0.f);
	if (Flash > 0.f)
	{
		Lightning->SetIntensity(LightningBrightness * Flash * FMath::Max(StormStrength, 0.4f));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Rain

void AJungleWeather::UpdateRain(float DeltaSeconds)
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager || Drops.Num() == 0 || DeltaSeconds <= 0.f)
	{
		return;
	}
	const FVector Camera = CameraManager->GetCameraLocation();

	// No rain inside a cave or under a rock overhang. Leaves have no collision, so rain still drips through
	// the canopy, like it would.
	ShelterTimer -= DeltaSeconds;
	if (ShelterTimer <= 0.f)
	{
		ShelterTimer = 0.25f;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RainShelter), false);
		Params.AddIgnoredActor(this);
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Params.AddIgnoredActor(Player);
		}
		FHitResult Hit;
		bSheltered = GetWorld()->LineTraceSingleByChannel(Hit, Camera, Camera + FVector(0.f, 0.f, 60.f * Meter), ECC_Visibility, Params);
	}
	RainCover = FMath::FInterpConstantTo(RainCover, bSheltered ? 0.f : 1.f, DeltaSeconds, 2.f);

	const float Heaviness = FMath::Clamp((StormStrength - 0.25f) / 0.75f, 0.f, 1.f);
	const int32 Falling = FMath::RoundToInt(Drops.Num() * Heaviness * RainCover);

	const FVector Velocity = FVector(Wind.X, Wind.Y, -RainFallSpeed) * Meter;
	const FQuat Streak = FRotationMatrix::MakeFromZ(Velocity).ToQuat();
	const float Radius = RainRadius * Meter;
	const float RadiusSquared = FMath::Square(Radius * 1.15f);

	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		FVector& Drop = Drops[Index];
		Drop += Velocity * DeltaSeconds;

		// Drops that fall out of the space around the camera start again at the top, so it always rains
		// wherever you are, even flying fast.
		const FVector Offset = Drop - Camera;
		if (Offset.Z < -RainBottom * Meter || Offset.Z > RainTop * Meter * 1.5f || FVector2D(Offset.X, Offset.Y).SizeSquared() > RadiusSquared)
		{
			const FVector2D Spot = FMath::RandPointInCircle(Radius);
			const float Height = Offset.Z < -RainBottom * Meter ? RainTop : FMath::FRandRange(-RainBottom, RainTop);
			Drop = Camera + FVector(Spot.X, Spot.Y, Height * Meter);
		}

		const float Length = Index < Falling ? FMath::Lerp(0.35f, 0.75f, DropRandom(Index)) : 0.f;
		const float Thickness = Index < Falling ? 0.007f : 0.f;
		DropTransforms[Index] = FTransform(Streak, Drop, FVector(Thickness, Thickness, Length));
	}

	Rain->BatchUpdateInstancesTransforms(0, DropTransforms, true, true, true);
}
