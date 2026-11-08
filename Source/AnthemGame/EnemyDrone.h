#pragma once

#include "CoreMinimal.h"
#include "JavelinEnemy.h"
#include "EnemyDrone.generated.h"

/**
 * A flying drone. It keeps above and away from you, circling, and follows
 * you into the air, so flying off doesn't shake it. Every few seconds its
 * eye charges up and it fires a big slow plasma orb that hits hard but is
 * easy to dodge if you're watching. The glowing eye is the weak spot.
 * It's light, so hits and blasts push it around, even up and down.
 */
UCLASS()
class ANTHEMGAME_API AEnemyDrone : public AJavelinEnemy
{
	GENERATED_BODY()

public:
	AEnemyDrone();

	virtual FText GetEnemyName() const override;

	/** How high above you it likes to fly while fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (Units = "cm"))
	float CombatHeight = 700.f;

	/** It never flies lower than this above the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0", Units = "cm"))
	float MinAltitude = 350.f;

	/** How fast it circles around you, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0"))
	float OrbitSpeed = 18.f;

protected:
	virtual void UpdateMovement(float DeltaSeconds, const AJavelinCharacter* Target) override;
	virtual void UpdateFacing(float DeltaSeconds, const AJavelinCharacter* Target) override;
	virtual void UpdateAnimation(float DeltaSeconds) override;

	/** Shot down, it loses power and drops, spinning and trailing smoke, and blows up when it hits the ground. */
	virtual void UpdateDeath(float DeltaSeconds) override;

	/** The flat ring around the body. It spins. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Body")
	TObjectPtr<USceneComponent> Ring;

private:
	float AnimTime = 0.f;
	float BodyPitch = 0.f;
	float SmokeTimer = 0.f;
};
