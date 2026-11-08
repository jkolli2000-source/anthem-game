#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JungleWeather.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UPostProcessComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;

/**
 * A dark, stormy rainforest sky that changes over time.
 *
 * - Day and night: the sun crosses the sky and sets, then a pale moon lights the jungle until morning.
 *   Light color, shadows, fog and the camera's exposure all follow the time of day.
 * - The storm: heavy rain falls around the camera, lightning flashes light up the clouds and fog, and the
 *   storm slowly builds and eases off on its own, but never fully clears.
 * - Realistic lighting: soft sun shadows with contact shadows, a sky light that follows the sky, volumetric
 *   clouds, and a gentle color grade (a bit of bloom, vignette and film grain, colder and grayer in the storm).
 *
 * The jungle spawns one at play time if the level has none. Drag one into the level to change its settings.
 * Console commands for testing (press ` to open the console): JungleTime 21, JungleTimeSpeed 30, JungleStorm 0.3.
 */
UCLASS()
class ANTHEMGAME_API AJungleWeather : public AActor
{
	GENERATED_BODY()

public:
	AJungleWeather();

	virtual void Tick(float DeltaSeconds) override;

	// ---- Time of day ----

	/** The hour when play starts, 0 to 24. 6 is sunrise, 12 is noon, 18 is sunset, 0 is midnight. */
	UPROPERTY(EditAnywhere, Category = "Weather|Time", meta = (ClampMin = "0", ClampMax = "24"))
	float TimeOfDay = 15.5f;

	/** How many real minutes a whole day and night takes. */
	UPROPERTY(EditAnywhere, Category = "Weather|Time", meta = (ClampMin = "1"))
	float DayLengthMinutes = 30.f;

	/** Nights go by this many times faster than days, so you aren't in the dark too long. */
	UPROPERTY(EditAnywhere, Category = "Weather|Time", meta = (ClampMin = "0.1", ClampMax = "10"))
	float NightSpeed = 2.f;

	/** Turn off to freeze the clock at Time Of Day. */
	UPROPERTY(EditAnywhere, Category = "Weather|Time")
	bool bTimePasses = true;

	/** Which way the sun rises, in degrees around the up axis. */
	UPROPERTY(EditAnywhere, Category = "Weather|Time")
	float SunriseDirection = 35.f;

	/** How high the sun gets at noon, in degrees above the horizon. */
	UPROPERTY(EditAnywhere, Category = "Weather|Time", meta = (ClampMin = "20", ClampMax = "90"))
	float NoonSunHeight = 65.f;

	// ---- Storm ----

	/** How stormy it is, 0 (just overcast) to 1 (a downpour). It drifts between the min and max on its own. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "0", ClampMax = "1"))
	float MinStorm = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxStorm = 1.f;

	/** Roughly how many minutes the storm takes to build up and ease off again. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "0.5"))
	float StormCycleMinutes = 6.f;

	/** Raindrops around the camera in the heaviest rain. Turn it down if the game runs slowly. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "0", ClampMax = "8000"))
	int32 MaxRaindrops = 3000;

	/** How far around the camera the rain falls. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "5", Units = "m"))
	float RainRadius = 28.f;

	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (Units = "m/s"))
	float RainFallSpeed = 11.f;

	/** The wind blowing the rain sideways, in meters per second. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm")
	FVector2D Wind = FVector2D(3.f, 1.5f);

	UPROPERTY(EditAnywhere, Category = "Weather|Storm")
	FLinearColor RainColor = FLinearColor(0.42f, 0.46f, 0.5f);

	/** Seconds between lightning strikes in the heaviest storm. It's less often when the storm eases. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "1"))
	float LightningEvery = 9.f;

	/** How bright a lightning flash is, in lux. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm", meta = (ClampMin = "0"))
	float LightningBrightness = 25.f;

	/** Adds 3D volumetric clouds. They look great from up high, but cost some speed. */
	UPROPERTY(EditAnywhere, Category = "Weather|Storm")
	bool bVolumetricClouds = true;

	// ---- Light ----

	/** Sunlight on a clear day, in lux. The storm dims it. */
	UPROPERTY(EditAnywhere, Category = "Weather|Light", meta = (ClampMin = "0"))
	float SunBrightness = 10.f;

	/** How much of the sunlight the storm clouds block at their thickest, 0 to 1. */
	UPROPERTY(EditAnywhere, Category = "Weather|Light", meta = (ClampMin = "0", ClampMax = "1"))
	float StormDimming = 0.7f;

	/** Moonlight, in lux. */
	UPROPERTY(EditAnywhere, Category = "Weather|Light", meta = (ClampMin = "0"))
	float MoonBrightness = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Weather|Light")
	FLinearColor MoonColor = FLinearColor(0.55f, 0.66f, 1.f);

	/** How dark nights get. Lower is darker; raise it if you can't see at night. */
	UPROPERTY(EditAnywhere, Category = "Weather|Light", meta = (ClampMin = "-6", ClampMax = "6"))
	float NightExposure = -1.2f;

	/** The fog and haze in the heaviest storm, as a multiple of the normal jungle fog. */
	UPROPERTY(EditAnywhere, Category = "Weather|Light", meta = (ClampMin = "1", ClampMax = "5"))
	float StormFog = 1.7f;

	/** The color grade: a bit of bloom, vignette and grain, and grayer, colder colors in the storm. */
	UPROPERTY(EditAnywhere, Category = "Weather|Light")
	bool bColorGrade = true;

	/** The current time of day, 0 to 24. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetTimeOfDay() const { return TimeOfDay; }

	/** How stormy it is right now, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetStormStrength() const { return StormStrength; }

	/** Jumps to an hour, 0 to 24. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetTimeOfDay(float Hour);

	/** Speeds up the clock (1 is normal), for testing. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetTimeSpeed(float Multiplier) { TimeSpeed = FMath::Max(0.f, Multiplier); }

	/** Holds the storm at a strength from 0 to 1. Pass a negative number to let it drift again. */
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetStorm(float Strength) { ForcedStorm = Strength; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Rain;

	/** A light with no shadows that flashes on for a moment from the direction of each strike. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> Lightning;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> PostProcess;

private:
	void FindLevelLighting();
	void UpdateClock(float DeltaSeconds);
	void UpdateStorm(float DeltaSeconds);
	void UpdateSunAndMoon();
	void UpdateAtmosphere();
	void UpdateLightning(float DeltaSeconds);
	void UpdateRain(float DeltaSeconds);

	/** Sun height in degrees above the horizon (negative at night). */
	float GetSunElevation() const;
	FVector GetSunDirection() const;

	/** 1 in full daylight, 0 at night, smooth through dusk and dawn. */
	float GetDaylight() const;

	/** 0 to 1 brightness of the current lightning flash. */
	float GetFlash() const;

	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY()
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	// The fog as the jungle set it up, before the storm thickens it.
	float BaseFogDensity = 0.03f;
	FLinearColor BaseFogColor = FLinearColor(0.45f, 0.55f, 0.6f);
	float BaseSkyLightIntensity = 1.f;

	float TimeSpeed = 1.f;
	float StormStrength = 0.8f;
	float ForcedStorm = -1.f;
	float StormClock = 0.f;
	float SunUpdateTimer = 0.f;
	float AtmosphereTimer = 0.f;

	float NextLightning = 4.f;
	float SinceLightning = 100.f;

	// Raindrops: positions in world space, and how much of the rain is falling right now.
	TArray<FVector> Drops;
	TArray<FTransform> DropTransforms;
	float RainCover = 1.f;
	float ShelterTimer = 0.f;
	bool bSheltered = false;
};
