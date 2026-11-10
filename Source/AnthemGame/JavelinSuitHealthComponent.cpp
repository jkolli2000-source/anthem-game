#include "JavelinSuitHealthComponent.h"
#include "Engine/World.h"

UJavelinSuitHealthComponent::UJavelinSuitHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UJavelinSuitHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Shields = MaxShields;
	Armor = MaxArmor;
}

float UJavelinSuitHealthComponent::Now() const
{
	return GetWorld()->GetTimeSeconds();
}

float UJavelinSuitHealthComponent::ApplyDamage(float Amount, const FVector& SourceLocation)
{
	if (bDown || Amount <= 0.f)
	{
		return 0.f;
	}

	LastDamageTime = Now();

	// Up to four arrows at once; the oldest makes room for a new one.
	if (DamageIndicators.Num() >= 4)
	{
		DamageIndicators.RemoveAt(0);
	}
	FJavelinDamageIndicator& Indicator = DamageIndicators.AddDefaulted_GetRef();
	Indicator.SourceLocation = SourceLocation;
	Indicator.Time = LastDamageTime;

	// Shields soak damage first, and whatever is left over goes into armor.
	const bool bHadShields = Shields > 0.f;
	const float ToShields = FMath::Min(Shields, Amount);
	Shields -= ToShields;
	const float ToArmor = FMath::Min(Armor, Amount - ToShields);
	Armor -= ToArmor;

	if (bHadShields && Shields <= 0.f)
	{
		ShieldsBrokeTime = LastDamageTime;
		OnShieldsBroken.Broadcast();
	}
	if (Armor <= 0.f)
	{
		bDown = true;
		OnSuitDown.Broadcast();
	}
	return ToShields + ToArmor;
}

bool UJavelinSuitHealthComponent::RepairArmor(float Amount)
{
	if (bDown || Armor >= MaxArmor)
	{
		return false;
	}
	Armor = FMath::Min(MaxArmor, Armor + Amount);
	ArmorRepairedTime = Now();
	return true;
}

void UJavelinSuitHealthComponent::SetMaxValues(float NewMaxShields, float NewMaxArmor)
{
	const float ShieldShare = MaxShields > 0.f ? Shields / MaxShields : 1.f;
	const float ArmorShare = MaxArmor > 0.f ? Armor / MaxArmor : 1.f;
	MaxShields = FMath::Max(0.f, NewMaxShields);
	MaxArmor = FMath::Max(1.f, NewMaxArmor);
	Shields = MaxShields * ShieldShare;
	// A suit that is still standing keeps at least a sliver of armor.
	Armor = bDown ? 0.f : FMath::Max(1.f, MaxArmor * ArmorShare);
}

void UJavelinSuitHealthComponent::Restore()
{
	bDown = false;
	Shields = MaxShields;
	Armor = MaxArmor;
	DamageIndicators.Reset();
}

bool UJavelinSuitHealthComponent::IsRegeneratingShields() const
{
	return !bDown && Shields < MaxShields && GetTimeSinceDamage() >= ShieldRegenDelay;
}

void UJavelinSuitHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsRegeneratingShields())
	{
		Shields = FMath::Min(MaxShields, Shields + ShieldRegenPerSecond * DeltaTime);
	}

	const float Time = Now();
	DamageIndicators.RemoveAll([Time](const FJavelinDamageIndicator& Indicator) { return Time - Indicator.Time > DamageIndicatorLifetime; });
}

float UJavelinSuitHealthComponent::GetTimeSinceDamage() const
{
	return Now() - LastDamageTime;
}

float UJavelinSuitHealthComponent::GetTimeSinceShieldsBroke() const
{
	return Now() - ShieldsBrokeTime;
}

float UJavelinSuitHealthComponent::GetTimeSinceArmorRepaired() const
{
	return Now() - ArmorRepairedTime;
}
