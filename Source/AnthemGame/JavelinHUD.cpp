#include "JavelinHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "JavelinCharacter.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"
#include "JavelinWeaponComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ShootingTarget.h"

namespace
{
	const FLinearColor WeakSpotHitColor(1.f, 0.6f, 0.1f);
	const FLinearColor KillColor(1.f, 0.2f, 0.15f);
	constexpr float HitMarkerTime = 0.2f;
	constexpr float HealthBarShowTime = 4.f;
}

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

	const UJavelinWeaponComponent* Weapon = Javelin->GetWeapon();
	DrawWorldMarkers(Weapon);
	DrawCrosshair(Javelin, CenterX, CenterY);
	DrawWeaponInfo(Weapon);

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
			TEXT("Left mouse  fire     Right mouse  aim     R  reload"),
			TEXT("Guns are stowed while flying: press Space to hover and shoot"),
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

bool AJavelinHUD::ProjectToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const
{
	const APlayerCameraManager* Camera = PlayerOwner ? PlayerOwner->PlayerCameraManager.Get() : nullptr;
	if (!Camera || FVector::DotProduct(WorldLocation - Camera->GetCameraLocation(), Camera->GetCameraRotation().Vector()) <= 0.f)
	{
		return false;
	}
	const FVector Screen = Project(WorldLocation, false);
	OutScreen = FVector2D(Screen.X, Screen.Y);
	return true;
}

void AJavelinHUD::DrawCrosshair(const AJavelinCharacter* Javelin, float CenterX, float CenterY)
{
	const UJavelinWeaponComponent* Weapon = Javelin->GetWeapon();
	const bool bCanFire = Javelin->CanFireWeapon();

	// The four ticks sit on the edge of the spread cone, so they open up as you fire and close in when you aim.
	float Gap = 6.f;
	if (const APlayerCameraManager* Camera = PlayerOwner ? PlayerOwner->PlayerCameraManager.Get() : nullptr)
	{
		const float HalfFOV = FMath::DegreesToRadians(Camera->GetFOVAngle() * 0.5f);
		const float HalfSpread = FMath::DegreesToRadians(Weapon->GetCurrentSpread() * 0.5f);
		Gap = FMath::Max(3.f, CenterX * FMath::Tan(HalfSpread) / FMath::Tan(HalfFOV));
	}

	const FLinearColor CrosshairColor = bCanFire ? FLinearColor(1.f, 1.f, 1.f, 0.85f) : FLinearColor(1.f, 1.f, 1.f, 0.3f);
	const float Length = 8.f;
	DrawLine(CenterX - Gap - Length, CenterY, CenterX - Gap, CenterY, CrosshairColor, 2.f);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + Length, CenterY, CrosshairColor, 2.f);
	DrawLine(CenterX, CenterY - Gap - Length, CenterX, CenterY - Gap, CrosshairColor, 2.f);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + Length, CrosshairColor, 2.f);
	if (Weapon->IsAiming())
	{
		DrawRect(CrosshairColor, CenterX - 1.f, CenterY - 1.f, 2.f, 2.f);
	}

	// Hit marker: a white X, orange for a weak spot, red and bigger for a kill.
	const float SinceHit = Weapon->GetTimeSinceLastHit();
	if (SinceHit < HitMarkerTime)
	{
		FLinearColor MarkerColor = FLinearColor::White;
		float Size = 7.f;
		if (Weapon->WasLastHitKill())
		{
			MarkerColor = KillColor;
			Size = 11.f;
		}
		else if (Weapon->WasLastHitWeakSpot())
		{
			MarkerColor = WeakSpotHitColor;
		}
		MarkerColor.A = 1.f - SinceHit / HitMarkerTime;
		const float Inner = 5.f;
		for (const FVector2D Corner : { FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, -1.f) })
		{
			DrawLine(CenterX + Corner.X * Inner, CenterY + Corner.Y * Inner,
				CenterX + Corner.X * (Inner + Size), CenterY + Corner.Y * (Inner + Size), MarkerColor, 2.f);
		}
	}

	if (!bCanFire)
	{
		DrawCenteredText(TEXT("Weapons stowed in flight - press Space to hover and shoot"), FLinearColor(1.f, 1.f, 1.f, 0.6f), CenterX, CenterY + 28.f, 0.9f);
	}
}

void AJavelinHUD::DrawWeaponInfo(const UJavelinWeaponComponent* Weapon)
{
	// Ammo in the bottom right corner, Anthem style: big magazine count, reserve beside it.
	const float Right = Canvas->ClipX - 40.f;
	const float Bottom = Canvas->ClipY - 60.f;
	UFont* LargeFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetSmallFont();

	const int32 Ammo = Weapon->GetAmmo();
	const bool bLow = Ammo <= Weapon->MagazineSize / 4;
	const FLinearColor AmmoColor = Ammo == 0 ? KillColor : (bLow ? WeakSpotHitColor : FLinearColor::White);

	const FString Reserve = FString::Printf(TEXT(" / %d"), Weapon->GetReserveAmmo());
	float ReserveWidth = 0.f;
	float ReserveHeight = 0.f;
	GetTextSize(Reserve, ReserveWidth, ReserveHeight, GEngine->GetMediumFont(), 1.2f);

	const FString Count = FString::FromInt(Ammo);
	const float CountScale = 2.f;
	float CountWidth = 0.f;
	float CountHeight = 0.f;
	GetTextSize(Count, CountWidth, CountHeight, LargeFont, CountScale);

	const float CountX = Right - ReserveWidth - CountWidth;
	DrawText(Count, AmmoColor, CountX, Bottom - CountHeight, LargeFont, CountScale);
	DrawText(Reserve, FLinearColor(1.f, 1.f, 1.f, 0.7f), Right - ReserveWidth, Bottom - ReserveHeight - 4.f, GEngine->GetMediumFont(), 1.2f);

	const FString Name = Weapon->WeaponName.ToString();
	float NameWidth = 0.f;
	float NameHeight = 0.f;
	GetTextSize(Name, NameWidth, NameHeight, SmallFont);
	DrawText(Name, FLinearColor(1.f, 1.f, 1.f, 0.75f), Right - NameWidth, Bottom - CountHeight - NameHeight - 2.f, SmallFont);

	// Reload bar or a prompt under the count.
	const float BarWidth = 160.f;
	const float BarY = Bottom + 8.f;
	if (Weapon->IsReloading())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Right - BarWidth, BarY, BarWidth, 6.f);
		DrawRect(FLinearColor::White, Right - BarWidth, BarY, BarWidth * Weapon->GetReloadProgress(), 6.f);
		DrawText(TEXT("RELOADING"), FLinearColor::White, Right - BarWidth, BarY + 10.f, SmallFont);
	}
	else if (Ammo == 0 && Weapon->GetReserveAmmo() == 0)
	{
		DrawText(TEXT("NO AMMO - walk to the yellow-striped ammo crate"), KillColor, Right - 330.f, BarY, SmallFont);
	}
	else if (bLow && Weapon->GetReserveAmmo() > 0)
	{
		DrawText(TEXT("R  RELOAD"), WeakSpotHitColor, Right - BarWidth, BarY, SmallFont);
	}
}

void AJavelinHUD::DrawWorldMarkers(const UJavelinWeaponComponent* Weapon)
{
	// Health bars over targets you've hit recently.
	for (TActorIterator<AShootingTarget> It(GetWorld()); It; ++It)
	{
		const AShootingTarget* Target = *It;
		FVector2D Screen;
		if (!Target->IsAlive() || Target->GetTimeSinceHit() > HealthBarShowTime || !ProjectToScreen(Target->GetHealthBarLocation(), Screen))
		{
			continue;
		}
		const float Width = 70.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), Screen.X - Width * 0.5f - 2.f, Screen.Y - 2.f, Width + 4.f, 9.f);
		DrawRect(KillColor, Screen.X - Width * 0.5f, Screen.Y, Width * Target->GetHealthPercent(), 5.f);
	}

	// Damage numbers rise and fade. Weak spot hits are bigger and orange.
	const float Now = GetWorld()->GetTimeSeconds();
	for (const FJavelinDamageNumber& Number : Weapon->GetDamageNumbers())
	{
		FVector2D Screen;
		if (!ProjectToScreen(Number.Location, Screen))
		{
			continue;
		}
		const float Age = (Now - Number.SpawnTime) / UJavelinWeaponComponent::DamageNumberLifetime;
		FLinearColor Color = Number.bWeakSpot ? WeakSpotHitColor : FLinearColor::White;
		Color.A = 1.f - Age * Age;
		const float Scale = Number.bWeakSpot ? 1.4f : 1.f;
		DrawCenteredText(FString::FromInt(FMath::RoundToInt(Number.Amount)), Color, Screen.X, Screen.Y - 20.f - Age * 50.f, Scale);
	}
}
