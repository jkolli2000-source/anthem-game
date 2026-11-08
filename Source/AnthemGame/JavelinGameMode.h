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

	/** Builds the graybox flight course at play time if the level doesn't already have one. */
	UPROPERTY(EditAnywhere, Category = "Javelin")
	bool bSpawnFlightTestArena = true;
};
