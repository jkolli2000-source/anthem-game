#include "JavelinGameMode.h"
#include "EngineUtils.h"
#include "FlightTestArena.h"
#include "JavelinCharacter.h"
#include "JavelinHUD.h"

AJavelinGameMode::AJavelinGameMode()
{
	DefaultPawnClass = AJavelinCharacter::StaticClass();
	HUDClass = AJavelinHUD::StaticClass();
}

void AJavelinGameMode::InitGameState()
{
	Super::InitGameState();

	// Runs before the player spawns, so the ground is there to land on.
	UWorld* World = GetWorld();
	if (bSpawnFlightTestArena && World && !TActorIterator<AFlightTestArena>(World))
	{
		World->SpawnActor<AFlightTestArena>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
}
