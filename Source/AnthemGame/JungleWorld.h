#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JungleWorld.generated.h"

class UCoolingZoneComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;

/**
 * A big rainforest built entirely from code, Anthem style. Rolling jungle
 * hills, cliffs and plateaus, two rivers cutting canyons through them, lakes
 * and swamps in the lowlands, waterfalls pouring off cliffs, rock tunnels to
 * fly through, mountains around the edge, and ruins with enemy camps in them.
 *
 * It's covered in thousands of trees, palms, ferns, bushes and flowers, with
 * a few giant canopy trees whose branches are platforms you can land on.
 * Fog and sunlight are tuned for a humid jungle when play starts.
 *
 * Any water cools your suit: skim the rivers and lakes, or fly through a
 * waterfall. The shooting range and first enemy camp sit in a clearing at
 * the start.
 *
 * The game mode spawns one at play time if the level has none. You can also
 * drag one into a level from the Place Actors panel to see and tweak it; it
 * rebuilds whenever you change a setting, which takes a second or two.
 */
UCLASS()
class ANTHEMGAME_API AJungleWorld : public AActor
{
	GENERATED_BODY()

public:
	AJungleWorld();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Change this to get a different jungle. */
	UPROPERTY(EditAnywhere, Category = "Jungle")
	int32 RandomSeed = 2026;

	/** Width of the whole world, edge to edge. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "1000", ClampMax = "8000", Units = "m"))
	float WorldSize = 4000.f;

	/** Distance between terrain points. Smaller is smoother but slower to build. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "5", ClampMax = "40", Units = "m"))
	float TerrainSpacing = 10.f;

	/** How tall the hills get. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "0", ClampMax = "200", Units = "m"))
	float HillHeight = 45.f;

	/** How tall the mountains get. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "0", ClampMax = "600", Units = "m"))
	float MountainHeight = 190.f;

	/** Rivers, lakes and swamps fill everything below this height. The start clearing is at 0. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (Units = "m"))
	float WaterLevel = -4.f;

	/**
	 * Multiplies the number of trees and plants. Turn it down if the game runs slowly,
	 * or up on a fast PC.
	 */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "0", ClampMax = "3"))
	float VegetationDensity = 1.f;

	/** Trees farther away than this aren't drawn. The fog hides the edge. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "200", Units = "m"))
	float TreeDrawDistance = 1000.f;

	/** Ferns, bushes and flowers farther away than this aren't drawn. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "30", Units = "m"))
	float PlantDrawDistance = 120.f;

	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "0", ClampMax = "100"))
	int32 GiantTreeCount = 30;

	/** Heat removed per second while you're in or just above any water. */
	UPROPERTY(EditAnywhere, Category = "Jungle", meta = (ClampMin = "0"))
	float WaterCoolingPerSecond = 50.f;

	/** Puts the shooting range and the first enemy camp in the start clearing. */
	UPROPERTY(EditAnywhere, Category = "Jungle")
	bool bSpawnStartArea = true;

	/** Puts enemy camps in the ruins out in the jungle. */
	UPROPERTY(EditAnywhere, Category = "Jungle")
	bool bSpawnRuinCamps = true;

	/** Turns the level's fog and sunlight into a humid, misty jungle when play starts. */
	UPROPERTY(EditAnywhere, Category = "Jungle")
	bool bSetUpJungleAtmosphere = true;

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor GrassColor = FLinearColor(0.045f, 0.11f, 0.025f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor MudColor = FLinearColor(0.085f, 0.065f, 0.04f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor RockColor = FLinearColor(0.2f, 0.21f, 0.17f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor RuinColor = FLinearColor(0.32f, 0.3f, 0.24f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor WaterColor = FLinearColor(0.03f, 0.16f, 0.14f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor WaterfallColor = FLinearColor(0.55f, 0.75f, 0.8f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor BarkColor = FLinearColor(0.1f, 0.065f, 0.04f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor LeafDarkColor = FLinearColor(0.02f, 0.07f, 0.02f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor LeafColor = FLinearColor(0.04f, 0.14f, 0.03f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor LeafLightColor = FLinearColor(0.12f, 0.22f, 0.04f);

	UPROPERTY(EditAnywhere, Category = "Jungle|Colors")
	FLinearColor FlowerColor = FLinearColor(0.9f, 0.15f, 0.35f);

	/** Ground height in centimeters at a world location, from the terrain. */
	UFUNCTION(BlueprintPure, Category = "Jungle")
	float GetGroundHeight(const FVector& WorldLocation) const;

	/** True if there's water (river, lake, swamp or pool) at a world location. */
	UFUNCTION(BlueprintPure, Category = "Jungle")
	bool IsWaterAt(const FVector& WorldLocation) const;

	/** Number of waterfalls. Each has its own cooling zone. */
	static constexpr int32 WaterfallCount = 3;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Terrain;

	/** Water has no collision so you can wade and fly through it. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Water;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Waterfalls;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Rocks;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Ruins;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Trunks;

	/** Big tree trunks, roots, branches and the platforms on top, which you can land on. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GiantWood;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GiantPlatforms;

	/** Leaves have no collision, so you can fly through the canopy. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LeavesDark;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Leaves;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LeavesLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PalmFronds;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Vines;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Ferns;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Bushes;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Flowers;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UCoolingZoneComponent>> WaterfallCooling;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	/** A flattened spot in the terrain: the start clearing, ruins and caves. */
	struct FFlatSite
	{
		FVector2f Center;	// meters
		float Radius;		// fully flat inside this
		float Blend;		// then blends back into the hills over this
		float Height;		// meters
	};

	/** A cliff with water pouring off it into a pool. Faces along +X, -X, +Y or -Y. */
	struct FWaterfallSite
	{
		FVector2f Edge;		// meters, the middle of the cliff edge
		FVector2f Facing;	// the way the water falls
		float Top;			// meters above the water
		float Width;		// meters
	};

	/** Ruins with an enemy camp in them. */
	struct FRuinSite
	{
		FVector2f Center;	// meters
		float Radius;		// meters
		int32 Soldiers;
		int32 Drones;
	};

	/** A rock tunnel to fly through. */
	struct FCaveSite
	{
		FVector2f Center;	// meters
		float Yaw;			// degrees, the way the tunnel runs
		float Length;		// meters
	};

	void PlanSites();
	void BuildTerrain();
	void BuildWater();
	void BuildWaterfalls();
	void BuildRuins();
	void BuildCaves();
	void BuildRocks(FRandomStream& Random);
	void BuildGiantTrees(FRandomStream& Random);
	void BuildForest(FRandomStream& Random);
	void BuildUndergrowth(FRandomStream& Random);

	void SpawnStartArea();
	void SpawnRuinCamps();
	void SetUpAtmosphere();
	void HideTemplateFloor();
	void UpdateWaterCooling();

	/** The shape of the land in meters, before it's sampled into the terrain grid. */
	float ComputeHeight(float X, float Y) const;
	float ComputeNaturalHeight(float X, float Y) const;

	/** Distance in meters from a point to the middle of the nearest river. */
	float DistanceToRiver(float X, float Y) const;

	/** Ground height and slope (1 = flat, 0 = vertical) in meters, from the terrain grid, in the actor's space. */
	float SampleHeight(float X, float Y) const;
	float SampleFlatness(float X, float Y) const;

	/** True if trees and plants may grow here: dry, not too steep, and away from the clearing, ruins and caves. */
	bool CanGrowAt(float X, float Y, float MinFlatness, float Clearance) const;

	UHierarchicalInstancedStaticMeshComponent* MakeInstances(FName Name, UStaticMesh* Mesh, bool bCollides, bool bCastShadow);
	void SetColor(UHierarchicalInstancedStaticMeshComponent* Component, const FLinearColor& Color);

	/**
	 * Queues an engine shape (1 m across, pivot in the middle) by center and full size, both in meters.
	 * Queued shapes are added all at once by AddQueuedShapes, which is much faster for thousands of them.
	 */
	void AddShape(UHierarchicalInstancedStaticMeshComponent* Component, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	void AddQueuedShapes();

	/** Works out the sites and the terrain grid. Fast enough to redo at play time if they're missing. */
	void ComputeGrid();

	TArray<FFlatSite> FlatSites;
	TArray<FWaterfallSite> WaterfallSites;
	TArray<FRuinSite> RuinSites;
	TArray<FCaveSite> CaveSites;

	/** River paths: each is a sum of sine waves, offset from a straight line. */
	struct FRiver
	{
		bool bAlongX;		// runs along X (west to east) instead of along Y
		float Offset;		// meters from the center line
		float Amplitudes[3];
		float Wavelengths[3];
		float Phases[3];
	};
	TArray<FRiver> Rivers;

	FVector2D NoiseOffset = FVector2D::ZeroVector;
	float RiverHalfWidth = 16.f;

	/** Terrain heights in meters, a grid of GridPoints x GridPoints. */
	TArray<float> Heights;

	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>> QueuedShapes;
	TArray<FVector2f> GiantTreeSpots;
	TWeakObjectPtr<class UJavelinHeatComponent> CooledHeat;
	int32 GridPoints = 0;
	float GridSpacing = 10.f;
	float HalfSize = 2000.f;
};
