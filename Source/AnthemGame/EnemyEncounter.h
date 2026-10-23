#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyEncounter.generated.h"

class AJavelinEnemy;

/**
 * An enemy camp that sends waves at you. It spawns soldiers on the ground
 * and drones in the air around itself; when you've killed them all it waits
 * a few seconds and sends a bigger wave.
 *
 * The flight test arena places one. You can also drag one into a level.
 */
UCLASS()
class ANTHEMGAME_API AEnemyEncounter : public AActor
{
	GENERATED_BODY()

public:
	AEnemyEncounter();

	virtual void Tick(float DeltaSeconds) override;

	/** Enemies spawn within this distance of the camp's center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "100", Units = "cm"))
	float CampRadius = 2200.f;

	/** Rest between clearing a wave and the next one arriving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0", Units = "s"))
	float TimeBetweenWaves = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0"))
	int32 FirstWaveSoldiers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0"))
	int32 FirstWaveDrones = 1;

	/** Each wave adds a soldier, and a drone every other wave, up to these limits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0"))
	int32 MaxSoldiers = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0"))
	int32 MaxDrones = 4;

	/** Drones spawn this high above the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0", Units = "cm"))
	float DroneSpawnHeight = 1000.f;

	UFUNCTION(BlueprintPure, Category = "Encounter")
	int32 GetWave() const { return Wave; }

	UFUNCTION(BlueprintPure, Category = "Encounter")
	int32 GetEnemiesRemaining() const { return Enemies.Num(); }

	/** Seconds until the next wave arrives, or -1 while a wave is still being fought. */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	float GetTimeUntilNextWave() const;

	/** True if any of this camp's enemies is fighting the player. */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsInCombat() const;

	/** Sends the next wave now. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void SpawnWave();

protected:
	virtual void BeginPlay() override;

private:
	/** A random spot on the ground within the camp, Height above it. */
	FVector PickSpawnPoint(float Height) const;

	TArray<TWeakObjectPtr<AJavelinEnemy>> Enemies;
	float NextWaveTime = -1.f;
	int32 Wave = 0;
};
