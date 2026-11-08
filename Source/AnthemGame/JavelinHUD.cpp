#include "JavelinHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EnemyEncounter.h"
#include "JavelinCharacter.h"
#include "JavelinEnemy.h"
#include "JavelinHeatComponent.h"
#include "JavelinMovementComponent.h"
#include "JavelinSuitHealthComponent.h"
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

	const FLinearColor ShieldColor(0.25f, 0.7f, 1.f);
	const FLinearColor ArmorColor(1.f, 0.62f, 0.2f);
	const FLinearColor HealColor(0.2f, 1.f, 0.35f);
	const FLinearColor EnemyColor(1.f, 0.22f, 0.15f);

	/** Enemy health bars only show within this distance. */
	constexpr float EnemyMarkerRange = 9000.f;
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
	DrawDamageFeedback(Javelin, CenterX, CenterY);
	DrawWorldMarkers(Weapon);
	if (Javelin->IsSuitDown())
	{
		DrawSuitDown(Javelin, CenterX, CenterY);
	}
	else
	{
		DrawMissileLocks(Javelin);
		DrawCrosshair(Javelin, CenterX, CenterY);
	}
	DrawWeaponInfo(Weapon);
	DrawSuitStatus(Javelin);
	DrawGear(Javelin, CenterX);
	DrawEncounterInfo(Javelin, CenterX);

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
			TEXT("Q  frag grenade     E  homing missiles (lock onto enemies near the crosshair)"),
			TEXT("Guns are stowed while flying: press Space to hover and shoot"),
			TEXT("In flight: W boost, S slow, dive steeply or skim water to cool"),
			TEXT("Enemy camp: follow the red marker. Aim for glowing weak spots"),
			TEXT("Green orbs repair armor, white orbs give ammo. Shields recharge"),
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

	// White normally, red when it's over an enemy (like Anthem), faded while the gun is stowed.
	FLinearColor CrosshairColor(1.f, 1.f, 1.f, 0.85f);
	if (!bCanFire)
	{
		CrosshairColor.A = 0.3f;
	}
	else if (Weapon->IsCrosshairOnTarget())
	{
		CrosshairColor = FLinearColor(1.f, 0.2f, 0.15f, 0.95f);
	}
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

	// Health bars and names over enemies that are fighting you or that you've just shot.
	const APawn* Pawn = GetOwningPawn();
	for (TActorIterator<AJavelinEnemy> It(GetWorld()); It; ++It)
	{
		const AJavelinEnemy* Enemy = *It;
		FVector2D Screen;
		const bool bShow = Enemy->IsEngaged() || Enemy->GetTimeSinceHit() < HealthBarShowTime;
		if (!Enemy->IsAlive() || !bShow || !Pawn || FVector::Dist(Pawn->GetActorLocation(), Enemy->GetActorLocation()) > EnemyMarkerRange
			|| !ProjectToScreen(Enemy->GetHealthBarLocation(), Screen))
		{
			continue;
		}
		const float Width = 80.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), Screen.X - Width * 0.5f - 2.f, Screen.Y - 2.f, Width + 4.f, 10.f);
		DrawRect(EnemyColor, Screen.X - Width * 0.5f, Screen.Y, Width * Enemy->GetHealthPercent(), 6.f);
		DrawCenteredText(Enemy->GetEnemyName().ToString(), FLinearColor(1.f, 0.5f, 0.45f, 0.9f), Screen.X, Screen.Y - 18.f, 0.7f);
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

void AJavelinHUD::DrawSuitStatus(const AJavelinCharacter* Javelin)
{
	// Shields over armor in the bottom left, Anthem style.
	const UJavelinSuitHealthComponent* Health = Javelin->GetSuitHealth();
	const float Time = GetWorld()->GetTimeSeconds();
	const float Left = 40.f;
	const float Width = 300.f;
	const float ArmorY = Canvas->ClipY - 70.f;
	const float ShieldY = ArmorY - 22.f;
	UFont* SmallFont = GEngine->GetSmallFont();

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Left - 3.f, ShieldY - 3.f, Width + 6.f, 44.f);

	// Shields: flash white when they break, pulse while recharging.
	FLinearColor Shield = ShieldColor;
	if (Health->IsRegeneratingShields())
	{
		Shield.A = 0.7f + 0.3f * FMath::Sin(Time * 8.f);
	}
	const float SinceBroke = Health->GetTimeSinceShieldsBroke();
	if (SinceBroke < 0.4f)
	{
		Shield = FLinearColor::LerpUsingHSV(FLinearColor::White, ShieldColor, SinceBroke / 0.4f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.5f * (1.f - SinceBroke / 0.4f)), Left, ShieldY, Width, 14.f);
	}
	DrawRect(Shield, Left, ShieldY, Width * Health->GetShieldPercent(), 14.f);

	// Armor: green flash when repaired, red pulse when nearly gone.
	FLinearColor Armor = ArmorColor;
	const float ArmorPercent = Health->GetArmorPercent();
	if (ArmorPercent < 0.3f)
	{
		Armor = FLinearColor::LerpUsingHSV(EnemyColor, FLinearColor::White, 0.25f + 0.25f * FMath::Sin(Time * 10.f));
	}
	const float SinceRepair = Health->GetTimeSinceArmorRepaired();
	if (SinceRepair < 0.6f)
	{
		Armor = FLinearColor::LerpUsingHSV(HealColor, Armor, SinceRepair / 0.6f);
	}
	DrawRect(Armor, Left, ArmorY, Width * ArmorPercent, 18.f);

	const FString Label = FString::Printf(TEXT("SHIELDS %d     ARMOR %d"), FMath::CeilToInt(Health->GetShields()), FMath::CeilToInt(Health->GetArmor()));
	DrawText(Label, FLinearColor::White, Left, ArmorY + 24.f, SmallFont);

	if (Health->GetShields() <= 0.f && !Health->IsDown())
	{
		DrawText(TEXT("SHIELDS DOWN - take cover"), ShieldColor, Left, ShieldY - 20.f, SmallFont);
	}
	else if (Health->IsRegeneratingShields())
	{
		DrawText(TEXT("RECHARGING"), ShieldColor, Left, ShieldY - 20.f, SmallFont);
	}
}

void AJavelinHUD::DrawDamageFeedback(const AJavelinCharacter* Javelin, float CenterX, float CenterY)
{
	const UJavelinSuitHealthComponent* Health = Javelin->GetSuitHealth();
	const float Time = GetWorld()->GetTimeSeconds();

	// Red glow around the screen edges after a hit; brighter when it's eating armor, and steady while armor is low.
	const float SinceDamage = Health->GetTimeSinceDamage();
	float Edge = SinceDamage < 0.5f ? (1.f - SinceDamage / 0.5f) * (Health->GetShields() > 0.f ? 0.3f : 0.6f) : 0.f;
	if (Health->GetArmorPercent() < 0.3f && !Health->IsDown())
	{
		Edge = FMath::Max(Edge, 0.2f + 0.08f * FMath::Sin(Time * 5.f));
	}
	if (Edge > 0.f)
	{
		// Bands that fade toward the middle fake a soft gradient.
		const float BandSize = 14.f;
		for (int32 Band = 0; Band < 6; ++Band)
		{
			const float Inset = Band * BandSize;
			const FLinearColor Color(0.9f, 0.05f, 0.02f, Edge * (1.f - Band / 6.f) * 0.5f);
			DrawRect(Color, Inset, Inset, Canvas->ClipX - Inset * 2.f, BandSize);
			DrawRect(Color, Inset, Canvas->ClipY - Inset - BandSize, Canvas->ClipX - Inset * 2.f, BandSize);
			DrawRect(Color, Inset, Inset + BandSize, BandSize, Canvas->ClipY - (Inset + BandSize) * 2.f);
			DrawRect(Color, Canvas->ClipX - Inset - BandSize, Inset + BandSize, BandSize, Canvas->ClipY - (Inset + BandSize) * 2.f);
		}
	}

	// Arcs around the crosshair pointing toward whoever shot you.
	const APlayerCameraManager* Camera = PlayerOwner ? PlayerOwner->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	const float CameraYaw = Camera->GetCameraRotation().Yaw;
	const float Radius = 120.f;
	for (const FJavelinDamageIndicator& Indicator : Health->GetDamageIndicators())
	{
		const FVector ToSource = Indicator.SourceLocation - Javelin->GetActorLocation();
		const float Angle = FMath::DegreesToRadians(FRotator::NormalizeAxis(ToSource.Rotation().Yaw - CameraYaw));
		const float Alpha = 1.f - (Time - Indicator.Time) / UJavelinSuitHealthComponent::DamageIndicatorLifetime;
		const FLinearColor Color(1.f, 0.15f, 0.08f, FMath::Clamp(Alpha, 0.f, 1.f));

		// Straight ahead is up the screen, to the right is right.
		const float HalfArc = FMath::DegreesToRadians(14.f);
		const int32 Segments = 5;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float A = Angle - HalfArc + 2.f * HalfArc * Index / Segments;
			const float B = Angle - HalfArc + 2.f * HalfArc * (Index + 1) / Segments;
			DrawLine(CenterX + FMath::Sin(A) * Radius, CenterY - FMath::Cos(A) * Radius,
				CenterX + FMath::Sin(B) * Radius, CenterY - FMath::Cos(B) * Radius, Color, 5.f);
		}
		// A notch on the outside of the arc's middle makes it read as an arrow.
		DrawLine(CenterX + FMath::Sin(Angle) * Radius, CenterY - FMath::Cos(Angle) * Radius,
			CenterX + FMath::Sin(Angle) * (Radius + 14.f), CenterY - FMath::Cos(Angle) * (Radius + 14.f), Color, 5.f);
	}
}

void AJavelinHUD::DrawEncounterInfo(const AJavelinCharacter* Javelin, float CenterX)
{
	TActorIterator<AEnemyEncounter> It(GetWorld());
	if (!It)
	{
		return;
	}
	const AEnemyEncounter* Encounter = *It;

	// Wave status at the top of the screen.
	FString Status;
	FLinearColor StatusColor = FLinearColor::White;
	const float UntilNext = Encounter->GetTimeUntilNextWave();
	if (UntilNext >= 0.f)
	{
		Status = FString::Printf(TEXT("CAMP CLEARED - wave %d incoming in %d"), Encounter->GetWave() + 1, FMath::CeilToInt(UntilNext));
		StatusColor = HealColor;
	}
	else
	{
		Status = FString::Printf(TEXT("ENEMY CAMP - WAVE %d - %d HOSTILES"), Encounter->GetWave(), Encounter->GetEnemiesRemaining());
		StatusColor = Encounter->IsInCombat() ? EnemyColor : FLinearColor(1.f, 0.8f, 0.7f);
	}
	DrawCenteredText(Status, StatusColor, CenterX, 20.f, 1.1f);

	// A waypoint over the camp while you're away from it.
	const FVector CampLocation = Encounter->GetActorLocation();
	const float Distance = FVector::Dist2D(Javelin->GetActorLocation(), CampLocation);
	FVector2D Screen;
	if (Distance > Encounter->CampRadius && ProjectToScreen(CampLocation + FVector(0.f, 0.f, 1500.f), Screen))
	{
		const float Size = 10.f;
		DrawLine(Screen.X, Screen.Y - Size, Screen.X + Size, Screen.Y, EnemyColor, 3.f);
		DrawLine(Screen.X + Size, Screen.Y, Screen.X, Screen.Y + Size, EnemyColor, 3.f);
		DrawLine(Screen.X, Screen.Y + Size, Screen.X - Size, Screen.Y, EnemyColor, 3.f);
		DrawLine(Screen.X - Size, Screen.Y, Screen.X, Screen.Y - Size, EnemyColor, 3.f);
		DrawCenteredText(FString::Printf(TEXT("ENEMY CAMP  %d m"), FMath::RoundToInt(Distance / 100.f)), EnemyColor, Screen.X, Screen.Y + 14.f, 0.8f);
	}
}

void AJavelinHUD::DrawSuitDown(const AJavelinCharacter* Javelin, float CenterX, float CenterY)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawCenteredText(TEXT("SUIT DOWN"), EnemyColor, CenterX, CenterY - 50.f, 2.5f);
	DrawCenteredText(FString::Printf(TEXT("Rebooting in %d..."), FMath::CeilToInt(Javelin->GetTimeUntilReboot())), FLinearColor::White, CenterX, CenterY + 10.f, 1.2f);
}

void AJavelinHUD::DrawGear(const AJavelinCharacter* Javelin, float CenterX)
{
	// Two gear icons to the right of the heat bar, Anthem style. While cooling down the icon fills up from the
	// bottom and shows the seconds left; when it's ready it lights up in the gear's color and flashes once.
	const UJavelinAbilityComponent* Abilities = Javelin->GetAbilities();
	if (!Abilities)
	{
		return;
	}

	struct FGearIcon
	{
		EJavelinGear Gear;
		const TCHAR* Key;
	};
	const FGearIcon Icons[] = { { EJavelinGear::Grenade, TEXT("Q") }, { EJavelinGear::Missiles, TEXT("E") } };

	const float Size = 54.f;
	const float Gap = 14.f;
	const float Top = Canvas->ClipY - 124.f;
	const float Left = CenterX + 240.f;
	const bool bUsable = Javelin->CanFireWeapon();
	UFont* SmallFont = GEngine->GetSmallFont();

	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Icons)); ++Index)
	{
		const EJavelinGear Gear = Icons[Index].Gear;
		const FJavelinGearSlot& Slot = Abilities->GetSlot(Gear);
		const bool bReady = Abilities->IsGearReady(Gear);
		const float X = Left + Index * (Size + Gap);
		const float MidX = X + Size * 0.5f;
		const float MidY = Top + Size * 0.5f;

		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Top - 2.f, Size + 4.f, Size + 4.f);
		if (bReady)
		{
			FLinearColor Fill = Slot.Color;
			Fill.A = bUsable ? 0.3f : 0.12f;
			DrawRect(Fill, X, Top, Size, Size);
		}
		else
		{
			const float Percent = Abilities->GetCooldownPercent(Gear);
			FLinearColor Fill = Slot.Color * 0.45f;
			Fill.A = 0.7f;
			DrawRect(Fill, X, Top + Size * (1.f - Percent), Size, Size * Percent);
		}

		FLinearColor GlyphColor = FLinearColor::White;
		GlyphColor.A = bReady ? (bUsable ? 1.f : 0.4f) : 0.3f;
		DrawGearGlyph(Gear, MidX, MidY, GlyphColor);
		if (!bReady)
		{
			DrawCenteredText(FString::FromInt(FMath::CeilToInt(Abilities->GetCooldownRemaining(Gear))), FLinearColor::White, MidX, MidY - 12.f, 1.3f);
		}

		// Border: gear color when ready, grey while cooling down, a white flash the moment it's ready again,
		// and a red flash if you try to use it too early.
		FLinearColor Border = bReady ? Slot.Color : FLinearColor(0.5f, 0.5f, 0.5f, 0.8f);
		const float SinceReady = Abilities->GetTimeSinceReady(Gear);
		if (bReady && SinceReady < 0.5f)
		{
			Border = FLinearColor::LerpUsingHSV(FLinearColor::White, Slot.Color, SinceReady / 0.5f);
		}
		const float SinceDenied = Abilities->GetTimeSinceDenied(Gear);
		if (SinceDenied < 0.3f)
		{
			Border = KillColor;
		}
		const float Thickness = bReady ? 2.f : 1.f;
		DrawLine(X, Top, X + Size, Top, Border, Thickness);
		DrawLine(X + Size, Top, X + Size, Top + Size, Border, Thickness);
		DrawLine(X + Size, Top + Size, X, Top + Size, Border, Thickness);
		DrawLine(X, Top + Size, X, Top, Border, Thickness);

		const FString Label = FString::Printf(TEXT("%s  %s"), Icons[Index].Key, *Slot.ShortName.ToString());
		float LabelWidth = 0.f;
		float LabelHeight = 0.f;
		GetTextSize(Label, LabelWidth, LabelHeight, SmallFont);
		DrawText(Label, FLinearColor(1.f, 1.f, 1.f, 0.8f), MidX - LabelWidth * 0.5f, Top + Size + 5.f, SmallFont);
	}
}

void AJavelinHUD::DrawGearGlyph(EJavelinGear Gear, float CenterX, float CenterY, const FLinearColor& Color)
{
	if (Gear == EJavelinGear::Grenade)
	{
		// A round grenade with its fuse cap and pin ring on top.
		const float Radius = 12.f;
		const int32 Segments = 12;
		const float Y = CenterY + 4.f;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			const float A = UE_TWO_PI * Index / Segments;
			const float B = UE_TWO_PI * (Index + 1) / Segments;
			DrawLine(CenterX + FMath::Cos(A) * Radius, Y + FMath::Sin(A) * Radius, CenterX + FMath::Cos(B) * Radius, Y + FMath::Sin(B) * Radius, Color, 3.f);
		}
		DrawRect(Color, CenterX - 4.f, Y - Radius - 6.f, 8.f, 6.f);
		DrawLine(CenterX + 4.f, Y - Radius - 4.f, CenterX + 11.f, Y - Radius + 2.f, Color, 2.f);
		DrawLine(CenterX + 4.f, Y - Radius - 6.f, CenterX + 10.f, Y - Radius - 11.f, Color, 2.f);
		return;
	}

	// Three missiles side by side, noses up.
	for (const float Offset : { -12.f, 0.f, 12.f })
	{
		const float X = CenterX + Offset;
		const float Lift = Offset == 0.f ? 4.f : 0.f;
		const float Bottom = CenterY + 13.f - Lift;
		const float Shoulder = CenterY - 5.f - Lift;
		DrawLine(X, Bottom, X, Shoulder, Color, 4.f);
		DrawLine(X - 2.5f, Shoulder, X, Shoulder - 7.f, Color, 2.f);
		DrawLine(X + 2.5f, Shoulder, X, Shoulder - 7.f, Color, 2.f);
		DrawLine(X - 2.f, Bottom - 4.f, X - 5.f, Bottom + 1.f, Color, 2.f);
		DrawLine(X + 2.f, Bottom - 4.f, X + 5.f, Bottom + 1.f, Color, 2.f);
	}
}

void AJavelinHUD::DrawMissileLocks(const AJavelinCharacter* Javelin)
{
	// Corner brackets around each enemy the missiles would go after, so you know what pressing E will hit.
	const UJavelinAbilityComponent* Abilities = Javelin->GetAbilities();
	if (!Abilities)
	{
		return;
	}
	const FLinearColor BaseColor = Abilities->GetSlot(EJavelinGear::Missiles).Color;
	const float Pulse = 0.75f + 0.25f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f);
	const FLinearColor Color(BaseColor.R, BaseColor.G, BaseColor.B, Pulse);

	for (const TWeakObjectPtr<AActor>& Lock : Abilities->GetMissileLocks())
	{
		FVector2D Screen;
		if (!Lock.IsValid() || !ProjectToScreen(UJavelinAbilityComponent::GetTargetCenter(Lock.Get()), Screen))
		{
			continue;
		}
		const float Half = 18.f;
		const float Arm = 7.f;
		for (const FVector2D Corner : { FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, -1.f) })
		{
			const float CornerX = Screen.X + Corner.X * Half;
			const float CornerY = Screen.Y + Corner.Y * Half;
			DrawLine(CornerX, CornerY, CornerX - Corner.X * Arm, CornerY, Color, 2.f);
			DrawLine(CornerX, CornerY, CornerX, CornerY - Corner.Y * Arm, Color, 2.f);
		}
	}
}
