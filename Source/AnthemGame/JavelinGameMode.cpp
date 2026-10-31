#include "JavelinGameMode.h"
#include "EngineUtils.h"
#include "FlightTestArena.h"
#include "JavelinCharacter.h"
#include "JavelinHUD.h"
#include "JungleWorld.h"

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
	if (!World || TActorIterator<AJungleWorld>(World) || TActorIterator<AFlightTestArena>(World))
	{
		return;
	}
	if (bSpawnJungle)
	{
		World->SpawnActor<AJungleWorld>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	else if (bSpawnFlightTestArena)
	{
		World->SpawnActor<AFlightTestArena>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
}
