#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShootingTarget.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EShootingTargetStyle : uint8
{
	/** A training dummy on a post. Its head is the weak spot. */
	Dummy,
	/** A floating drone, for practicing shots while hovering. Its glowing eye is the weak spot. */
	Drone
};

/**
 * Something to shoot at. Flashes when hit, blows apart when its health runs
 * out, and comes back after a few seconds. It can strafe side to side and
 * bob up and down, and always turns to face the player so its glowing weak
 * spot is visible.
 */
UCLASS()
class ANTHEMGAME_API AShootingTarget : public AActor
{
	GENERATED_BODY()

public:
	AShootingTarget();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Changes the shape. Set before the target is spawned or placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	EShootingTargetStyle Style = EShootingTargetStyle::Dummy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (ClampMin = "1"))
	float MaxHealth = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target", meta = (ClampMin = "0", Units = "s"))
	float RespawnDelay = 4.f;

	/** How far it strafes to each side. 0 stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target|Movement", meta = (ClampMin = "0", Units = "cm"))
	float StrafeDistance = 0.f;

	/** Full side-to-side trips per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target|Movement", meta = (ClampMin = "0"))
	float StrafeFrequency = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target|Movement", meta = (ClampMin = "0", Units = "cm"))
	float BobHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target|Paint")
	FLinearColor BodyColor = FLinearColor(0.45f, 0.12f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target|Paint")
	FLinearColor WeakSpotColor = FLinearColor(1.f, 0.55f, 0.f);

	UFUNCTION(BlueprintPure, Category = "Target")
	bool IsAlive() const { return Health > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Target")
	float GetHealthPercent() const { return Health / MaxHealth; }

	float GetTimeSinceHit() const;

	/** Where the HUD draws this target's health bar. */
	FVector GetHealthBarLocation() const;

protected:
	virtual void BeginPlay() override;

	/** The parts you shoot. OnConstruction shapes them for the Style and hides any it doesn't use. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	static constexpr int32 MaxParts = 5;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	void SetPart(int32 Index, UStaticMesh* Mesh, const FVector& Location, const FVector& Size, bool bWeakSpot);
	void BuildParts();
	void Die();
	void Respawn();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PartMaterials;

	TArray<FLinearColor> PartColors;
	FVector Origin = FVector::ZeroVector;
	FVector StrafeAxis = FVector::RightVector;
	float Health = 0.f;
	float HitFlash = 0.f;
	float LastHitTime = -100.f;
	float MoveTime = 0.f;
	float TopHeight = 0.f;
	FTimerHandle RespawnTimer;
};
