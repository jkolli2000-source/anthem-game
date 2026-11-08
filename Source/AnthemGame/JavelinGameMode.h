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
};
