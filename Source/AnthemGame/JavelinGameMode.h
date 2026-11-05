#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JavelinGameMode.generated.h"

/** Spawns the player as a javelin with the flight HUD. */
UCLASS()
class ANTHEMGAME_API AJavelinGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJavelinGameMode();

	virtual void InitGameState() override;

	/** Builds the rainforest jungle at play time if the level doesn't already have a jungle or a flight course. */
	UPROPERTY(EditAnywhere, Category = "Javelin")
	bool bSpawnJungle = true;

	/** With the jungle turned off, builds the old graybox flight course instead. */
	UPROPERTY(EditAnywhere, Category = "Javelin")
	bool bSpawnFlightTestArena = true;

	// Console commands for trying out the weather while you play. Press ` (the key under Esc) to open the console.

	/** Jumps to an hour of the day, 0 to 24. For example "JungleTime 21" for night. */
	UFUNCTION(Exec)
	void JungleTime(float Hour);

	/** Speeds up the clock. "JungleTimeSpeed 30" runs a whole day in a minute; "JungleTimeSpeed 1" is normal. */
	UFUNCTION(Exec)
	void JungleTimeSpeed(float Multiplier);

	/** Holds the storm at a strength from 0 (overcast) to 1 (downpour). "JungleStorm -1" lets it drift again. */
	UFUNCTION(Exec)
	void JungleStorm(float Strength);
};
