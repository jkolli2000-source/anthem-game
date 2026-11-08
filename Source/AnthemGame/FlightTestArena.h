#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightTestArena.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UCoolingZoneComponent;

/**
 * Graybox course for testing flight: a 1 km ground plane, rock pillars of
 * different heights, arches to fly through, a landing tower, and a lake with
 * a waterfall that cool the suit.
 *
 * The game mode spawns one at play time if the level has none. You can also
 * drag one into a level from the Place Actors panel to see and tweak it.
 */
UCLASS()
class ANTHEMGAME_API AFlightTestArena : public AActor
{
	GENERATED_BODY()

public:
	AFlightTestArena();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Change this to get a different pillar layout. */
	UPROPERTY(EditAnywhere, Category = "Arena")
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, Category = "Arena", meta = (ClampMin = "0", ClampMax = "200"))
	int32 PillarCount = 45;

	UPROPERTY(EditAnywhere, Category = "Arena")
	FLinearColor GroundColor = FLinearColor(0.12f, 0.14f, 0.11f);

	UPROPERTY(EditAnywhere, Category = "Arena")
	FLinearColor RockColor = FLinearColor(0.35f, 0.33f, 0.3f);

	UPROPERTY(EditAnywhere, Category = "Arena")
	FLinearColor WaterColor = FLinearColor(0.05f, 0.35f, 0.8f);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Rocks;

	/** Water has no collision so you can wade and fly through it. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Water;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCoolingZoneComponent> LakeCooling;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCoolingZoneComponent> WaterfallCooling;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	void SetColor(UInstancedStaticMeshComponent* Component, const FLinearColor& Color);

	/** Adds a box by center and full size in centimeters. */
	static void AddBox(UInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, float Yaw = 0.f);
};
