#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "JavelinHUD.generated.h"

/** Crosshair, heat bar, movement state and a controls reminder, drawn without any UI assets. */
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
};
