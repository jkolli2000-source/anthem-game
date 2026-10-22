#include "JavelinHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "JavelinCharacter.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"

void AJavelinHUD::DrawCenteredText(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Scale)
{
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, GEngine->GetMediumFont(), Scale);
	DrawText(Text, Color, CenterX - Width * 0.5f, Y, GEngine->GetMediumFont(), Scale);
}

void AJavelinHUD::DrawHUD()
{
	Super::DrawHUD();

	const AJavelinCharacter* Javelin = Cast<AJavelinCharacter>(GetOwningPawn());
	if (!Canvas || !Javelin)
	{
		return;
	}

	const UJavelinHeatComponent* Heat = Javelin->GetHeatComponent();
	const UJavelinMovementComponent* Movement = Javelin->GetJavelinMovement();
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const float Time = GetWorld()->GetTimeSeconds();

	// Crosshair
	const FLinearColor CrosshairColor(1.f, 1.f, 1.f, 0.8f);
	DrawLine(CenterX - 10.f, CenterY, CenterX - 3.f, CenterY, CrosshairColor, 2.f);
	DrawLine(CenterX + 3.f, CenterY, CenterX + 10.f, CenterY, CrosshairColor, 2.f);
	DrawLine(CenterX, CenterY - 10.f, CenterX, CenterY - 3.f, CrosshairColor, 2.f);
	DrawLine(CenterX, CenterY + 3.f, CenterX, CenterY + 10.f, CrosshairColor, 2.f);

	// Heat bar under the crosshair area. Green when cool, red near overheating.
	const float BarWidth = 420.f;
	const float BarHeight = 14.f;
	const float BarX = CenterX - BarWidth * 0.5f;
	const float BarY = Canvas->ClipY - 90.f;
	const float HeatPercent = Heat->GetHeatPercent();

	FLinearColor FillColor = FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.9f, 0.3f), FLinearColor(1.f, 0.1f, 0.05f), HeatPercent);
	if (Heat->IsOverheated())
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 12.f);
		FillColor = FLinearColor(1.f, 0.1f * Pulse, 0.05f, 0.6f + 0.4f * Pulse);
	}

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), BarX - 3.f, BarY - 3.f, BarWidth + 6.f, BarHeight + 6.f);
	DrawRect(FillColor, BarX, BarY, BarWidth * HeatPercent, BarHeight);

	FString HeatLabel = FString::Printf(TEXT("HEAT %d%%"), FMath::RoundToInt(HeatPercent * 100.f));
	FLinearColor HeatLabelColor = FLinearColor::White;
	if (Heat->IsOverheated())
	{
		HeatLabel = TEXT("OVERHEATED - land and cool down");
		HeatLabelColor = FLinearColor(1.f, 0.25f, 0.2f);
	}
	else if (Heat->IsBeingCooled())
	{
		HeatLabel += TEXT("   COOLING");
		HeatLabelColor = FLinearColor(0.4f, 0.85f, 1.f);
	}
	DrawCenteredText(HeatLabel, HeatLabelColor, CenterX, BarY + BarHeight + 6.f);

	// Movement state and speed above the bar.
	FString State = TEXT("ON FOOT");
	switch (Movement->GetJavelinMode())
	{
	case EJavelinMoveMode::Flight:
		State = Movement->IsDiving() ? TEXT("DIVING") : TEXT("FLYING");
		break;
	case EJavelinMoveMode::Hover:
		State = TEXT("HOVERING");
		break;
	default:
		if (Movement->IsFalling())
		{
			State = TEXT("AIRBORNE");
		}
		else if (Movement->IsSprinting())
		{
			State = TEXT("SPRINTING");
		}
		break;
	}
	const int32 SpeedKmh = FMath::RoundToInt(Javelin->GetVelocity().Size() * 0.036f);
	DrawCenteredText(FString::Printf(TEXT("%s   %d km/h"), *State, SpeedKmh), FLinearColor::White, CenterX, BarY - 30.f, 1.1f);

	if (bShowControls)
	{
		static const TCHAR* Lines[] = {
			TEXT("WASD  move            Mouse  look"),
			TEXT("Space  jump / hover in the air / stop hovering"),
			TEXT("Shift  hold to sprint / fly while airborne / stop flying"),
			TEXT("F  take off straight into flight"),
			TEXT("In flight: W boost, S slow, dive steeply or skim water to cool"),
		};
		float Y = 20.f;
		for (const TCHAR* Line : Lines)
		{
			DrawText(Line, FLinearColor(1.f, 1.f, 1.f, 0.75f), 20.f, Y, GEngine->GetSmallFont());
			Y += 18.f;
		}
	}
}
