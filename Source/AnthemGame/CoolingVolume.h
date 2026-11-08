#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "CoolingVolume.generated.h"

/** Box that cools any javelin inside it, such as water or a waterfall. */
UCLASS(ClassGroup = (Javelin), meta = (BlueprintSpawnableComponent))
class ANTHEMGAME_API UCoolingZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UCoolingZoneComponent();

	/** Heat removed per second while inside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooling", meta = (ClampMin = "0"))
	float CoolingPerSecond = 60.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

/** Drop this in a level and scale it over water, waterfalls or cold air. */
UCLASS()
class ANTHEMGAME_API ACoolingVolume : public AActor
{
	GENERATED_BODY()

public:
	ACoolingVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cooling")
	TObjectPtr<UCoolingZoneComponent> CoolingZone;
};
