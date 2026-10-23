#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "JavelinHUD.generated.h"

/**
 * Crosshair, hit markers, damage numbers, target and enemy health bars, ammo,
 * shields and armor, damage direction arrows, heat bar, movement state, the
 * enemy camp's wave status and a controls reminder, drawn without any UI assets.
 */
UCLASS()
class ANTHEMGAME_API AJavelinHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "HUD")
	bool bShowControls = true;

private:
	void DrawCenteredText(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale = 1.f);
	void DrawCrosshair(const class AJavelinCharacter* Javelin, float CenterX, float CenterY);
	void DrawWeaponInfo(const class UJavelinWeaponComponent* Weapon);
	void DrawWorldMarkers(const class UJavelinWeaponComponent* Weapon);
	void DrawSuitStatus(const class AJavelinCharacter* Javelin);
	void DrawDamageFeedback(const class AJavelinCharacter* Javelin, float CenterX, float CenterY);
	void DrawEncounterInfo(const class AJavelinCharacter* Javelin, float CenterX);
	void DrawSuitDown(const class AJavelinCharacter* Javelin, float CenterX, float CenterY);

	/** Screen position of a world point, or false if it's behind the camera. */
	bool ProjectToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const;
};
