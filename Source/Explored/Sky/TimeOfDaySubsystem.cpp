#include "Sky/TimeOfDaySubsystem.h"

#include "UI/ExploredGameUserSettings.h"

namespace ExploredSky
{
	float SolarDeclinationDeg(float DayOfYear)
	{
		// Oscila ±23,44° a lo largo del año de juego; el día 0 es el equinoccio.
		return 23.44f * FMath::Sin(UE_TWO_PI * DayOfYear / DaysPerYear);
	}

	FVector SunDirection(float Hours, float DayOfYear)
	{
		const float Lat = FMath::DegreesToRadians(LatitudeDeg);
		const float Dec = FMath::DegreesToRadians(SolarDeclinationDeg(DayOfYear));
		// Ángulo horario: 0 a mediodía, 15° por hora.
		const float H = FMath::DegreesToRadians((Hours - 12.0f) * 15.0f);

		const float SinAlt = FMath::Sin(Lat) * FMath::Sin(Dec) + FMath::Cos(Lat) * FMath::Cos(Dec) * FMath::Cos(H);
		const float Alt = FMath::Asin(FMath::Clamp(SinAlt, -1.0f, 1.0f));

		// Componentes horizontales en el marco este-norte-arriba.
		const float East = -FMath::Cos(Dec) * FMath::Sin(H);
		const float North = FMath::Cos(Lat) * FMath::Sin(Dec) - FMath::Sin(Lat) * FMath::Cos(Dec) * FMath::Cos(H);
		const FVector Horizontal(North, East, 0.0f);
		const FVector Dir = Horizontal.GetSafeNormal() * FMath::Cos(Alt) + FVector::UpVector * FMath::Sin(Alt);
		return Dir.GetSafeNormal();
	}

	FVector MoonDirection(float Hours, float DayOfYear, float Phase)
	{
		// La Luna va retrasada respecto al Sol: en luna llena sale al anochecer.
		const float Offset = 12.0f + (Phase - 0.5f) * 24.0f;
		return SunDirection(FMath::Fmod(Hours + Offset + 24.0f, 24.0f), DayOfYear + 6.0f);
	}

	float MoonPhase(float TotalDays)
	{
		return FMath::Fmod(TotalDays / DaysPerLunarCycle, 1.0f);
	}

	float MoonIllumination(float Phase)
	{
		return 0.5f * (1.0f - FMath::Cos(UE_TWO_PI * Phase));
	}
}

void UTimeOfDaySubsystem::Tick(float DeltaTime)
{
	const float HoursPerSecond = 24.0f / (DayLengthMinutes * 60.0f);
	Hours += DeltaTime * HoursPerSecond * TimeScale;
	while (Hours >= 24.0f)
	{
		Hours -= 24.0f;
		++Day;
		OnNewDay.Broadcast(Day);
	}
}

TStatId UTimeOfDaySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTimeOfDaySubsystem, STATGROUP_Tickables);
}

bool UTimeOfDaySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTimeOfDaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// H7: antes nadie llamaba a SetDayLengthMinutes y el día duraba siempre 40 min.
	// Al pulsar Aplicar lo actualiza UExploredGameUserSettings::ApplyToWorld().
	if (const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get())
	{
		SetDayLengthMinutes(Settings->GetDayLengthMinutes());
	}
}

void UTimeOfDaySubsystem::SetTime(int32 InDay, float InHours)
{
	// Las horas >= 24 pasan al dia siguiente en vez de perderse.
	const float ClampedHours = FMath::Max(0.0f, InHours);
	Day = FMath::Max(0, InDay) + FMath::FloorToInt32(ClampedHours / 24.0f);
	Hours = FMath::Fmod(ClampedHours, 24.0f);
}
