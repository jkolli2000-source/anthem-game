#pragma once

#include "CoreMinimal.h"
#include "JavelinEnemy.h"
#include "EnemySoldier.generated.h"

/**
 * A ground trooper with a rifle. It walks toward you until you're in range,
 * then strafes from side to side while firing three-round bursts of red
 * bolts. Its glowing visor is a weak spot, so aim for the head, and so is
 * the power cell on its back if you can get behind it.
 *
 * It can't fly, so getting up high or taking off breaks its line of fire
 * for a while.
 */
UCLASS()
class ANTHEMGAME_API AEnemySoldier : public AJavelinEnemy
{
	GENERATED_BODY()

public:
	AEnemySoldier();

	virtual FText GetEnemyName() const override;

	/** Distance covered by one full walk cycle (two steps). */
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation", meta = (Units = "cm"))
	float StrideLength = 160.f;

protected:
	virtual void UpdateMovement(float DeltaSeconds, const AJavelinCharacter* Target) override;
	virtual void UpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Components|Body")
	TObjectPtr<USceneComponent> LeftHip;

	UPROPERTY(VisibleAnywhere, Category = "Components|Body")
	TObjectPtr<USceneComponent> RightHip;

	UPROPERTY(VisibleAnywhere, Category = "Components|Body")
	TObjectPtr<USceneComponent> LeftShoulder;

	/** The gun arm. It pitches up and down to point the rifle at you. */
	UPROPERTY(VisibleAnywhere, Category = "Components|Body")
	TObjectPtr<USceneComponent> RightShoulder;

private:
	TWeakObjectPtr<const AJavelinCharacter> AimTarget;
	float FallSpeed = 0.f;
	float WalkPhase = 0.f;
	float AimPitch = 0.f;
	bool bBlocked = false;
};
