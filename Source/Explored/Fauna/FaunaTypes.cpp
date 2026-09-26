#include "Fauna/FaunaTypes.h"

namespace FaunaTypesDetail
{
	FFaunaSpeciesInfo Make(const TCHAR* Mesh, EFaunaAnimStyle Style, EFaunaActivityProfile Activity, float Floor,
		float LengthCm, float CruiseCmS, float MaxCmS, float MinDepthCm, bool bBird = false)
	{
		FFaunaSpeciesInfo Info;
		Info.Mesh = Mesh;
		Info.AnimStyle = Style;
		Info.Activity = Activity;
		Info.ActivityFloor = Floor;
		Info.BodyLengthCm = LengthCm;
		Info.CruiseSpeedCmS = CruiseCmS;
		Info.MaxSpeedCmS = MaxCmS;
		Info.MinWaterDepthCm = MinDepthCm;
		Info.bBird = bBird;
		return Info;
	}

	/** Campana suave centrada en Center con semiancho Width (1 en el centro, 0 fuera). */
	float Bell(float X, float Center, float Width)
	{
		const float T = FMath::Clamp(1.0f - FMath::Abs(X - Center) / Width, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Pérdida de olor con la edad (s) y ensanchamiento de la mancha (cm/s). */
	constexpr float SmellDecaySeconds = 120.0f;
	constexpr float PlumeBaseRadiusCm = 200.0f;
	constexpr float PlumeGrowthCmS = 40.0f;
}

const TCHAR* LexToString(EFaunaSpecies Species)
{
	switch (Species)
	{
	case EFaunaSpecies::ReefFish: return TEXT("ReefFish");
	case EFaunaSpecies::OpenSeaFish: return TEXT("OpenSeaFish");
	case EFaunaSpecies::Stingray: return TEXT("Stingray");
	case EFaunaSpecies::Jellyfish: return TEXT("Jellyfish");
	case EFaunaSpecies::ReefShark: return TEXT("ReefShark");
	case EFaunaSpecies::TigerShark: return TEXT("TigerShark");
	case EFaunaSpecies::Dolphin: return TEXT("Dolphin");
	case EFaunaSpecies::SeaTurtle: return TEXT("SeaTurtle");
	case EFaunaSpecies::HumpbackWhale: return TEXT("HumpbackWhale");
	case EFaunaSpecies::Gull: return TEXT("Gull");
	case EFaunaSpecies::Frigatebird: return TEXT("Frigatebird");
	default: return TEXT("Unknown");
	}
}

const FFaunaSpeciesInfo& FFaunaSpeciesInfo::Get(EFaunaSpecies Species)
{
	using namespace FaunaTypesDetail;
	using S = EFaunaAnimStyle;
	using A = EFaunaActivityProfile;
	// Longitudes y mallas de Tools/Blender/animals; velocidades de crucero y máximas aproximadas a las reales.
	static const FFaunaSpeciesInfo Table[static_cast<int32>(EFaunaSpecies::Count)] = {
		Make(TEXT("ClownFish"), S::SpineWave, A::Diurnal, 0.25f, 15.0f, 60.0f, 260.0f, 80.0f),
		Make(TEXT("Tuna"), S::SpineWave, A::Constant, 0.7f, 90.0f, 250.0f, 700.0f, 800.0f),
		Make(TEXT("Stingray"), S::DiscWave, A::Crepuscular, 0.5f, 120.0f, 60.0f, 260.0f, 40.0f),
		Make(TEXT("Jellyfish"), S::Pulse, A::Constant, 1.0f, 30.0f, 5.0f, 20.0f, 150.0f),
		Make(TEXT("ReefShark"), S::SpineWave, A::Crepuscular, 0.45f, 140.0f, 150.0f, 550.0f, 300.0f),
		Make(TEXT("TigerShark"), S::SpineWave, A::Crepuscular, 0.55f, 310.0f, 180.0f, 750.0f, 3000.0f),
		Make(TEXT("Dolphin"), S::FlukeWave, A::Diurnal, 0.35f, 230.0f, 450.0f, 1000.0f, 600.0f),
		Make(TEXT("SeaTurtle"), S::Flipper, A::Diurnal, 0.3f, 100.0f, 60.0f, 180.0f, 80.0f),
		Make(TEXT("HumpbackWhale"), S::FlukeWave, A::Constant, 1.0f, 1300.0f, 250.0f, 500.0f, 2000.0f),
		Make(TEXT("Gull"), S::Flap, A::Diurnal, 0.05f, 45.0f, 900.0f, 1600.0f, 0.0f, true),
		Make(TEXT("Frigatebird"), S::Flap, A::Diurnal, 0.05f, 100.0f, 1000.0f, 1800.0f, 0.0f, true),
	};
	const int32 Index = FMath::Clamp(static_cast<int32>(Species), 0, static_cast<int32>(EFaunaSpecies::Count) - 1);
	return Table[Index];
}

float FFaunaActivity::Daylight(float Hours)
{
	const float H = FMath::Fmod(FMath::Fmod(Hours, 24.0f) + 24.0f, 24.0f);
	return FMath::SmoothStep(5.5f, 7.0f, H) * (1.0f - FMath::SmoothStep(17.5f, 19.0f, H));
}

float FFaunaActivity::Level(EFaunaSpecies Species, float Hours)
{
	using namespace FaunaTypesDetail;
	const FFaunaSpeciesInfo& Info = FFaunaSpeciesInfo::Get(Species);
	const float H = FMath::Fmod(FMath::Fmod(Hours, 24.0f) + 24.0f, 24.0f);
	const float Day = Daylight(H);
	float Raw = 1.0f;
	switch (Info.Activity)
	{
	case EFaunaActivityProfile::Diurnal:
		// Más activos a primera y última hora de luz (biblia §4.1: «el amanecer y el atardecer son los mejores momentos»).
		Raw = Day * (0.8f + 0.2f * FMath::Max(Bell(H, 7.5f, 2.0f), Bell(H, 17.0f, 2.0f)));
		break;
	case EFaunaActivityProfile::Crepuscular:
	{
		const float Peak = FMath::Max(Bell(H, 6.25f, 1.5f), Bell(H, 18.25f, 1.5f));
		Raw = FMath::Lerp(0.35f + 0.35f * (1.0f - Day), 1.0f, Peak);
		break;
	}
	case EFaunaActivityProfile::Constant:
	default:
		Raw = 1.0f;
		break;
	}
	return FMath::Clamp(FMath::Max(Raw, Info.ActivityFloor), 0.0f, 1.0f);
}

bool FFaunaPerception::CanSee(const FVector& EyeCm, const FVector& Forward, const FVector& TargetCm,
	float RangeCm, float HalfAngleDeg, float Light01)
{
	const FVector ToTarget = TargetCm - EyeCm;
	const double Distance = ToTarget.Size();
	// Con poca luz se ve a un tercio del alcance.
	const double Range = RangeCm * FMath::Lerp(0.35f, 1.0f, FMath::Clamp(Light01, 0.0f, 1.0f));
	if (Distance > Range)
	{
		return false;
	}
	if (Distance < 1.0)
	{
		return true;
	}
	const FVector Dir = Forward.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return true;
	}
	const double CosAngle = FVector::DotProduct(Dir, ToTarget / Distance);
	return CosAngle >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
}

bool FFaunaPerception::CanHear(const FVector& EarCm, const FVector& SourceCm, float Noise01, float HearingRangeCm)
{
	const float Noise = FMath::Clamp(Noise01, 0.0f, 1.0f);
	if (Noise <= 0.0f)
	{
		return false;
	}
	return FVector::Distance(EarCm, SourceCm) <= HearingRangeCm * Noise;
}

FVector FFaunaPerception::PlumeCenter(const FBloodSource& Source, const FFaunaWorldQuery& World)
{
	const FVector2D Drift = World.CurrentAt(FVector2D(Source.OriginCm.X, Source.OriginCm.Y)) * Source.AgeSeconds;
	return Source.OriginCm + FVector(Drift.X, Drift.Y, 0.0);
}

float FFaunaPerception::SmellAt(const FVector& PointCm, const TArray<FBloodSource>& Blood, const FFaunaWorldQuery& World)
{
	using namespace FaunaTypesDetail;
	float Total = 0.0f;
	for (const FBloodSource& Source : Blood)
	{
		const float Age = FMath::Max(0.0f, Source.AgeSeconds);
		const float Radius = PlumeBaseRadiusCm + PlumeGrowthCmS * Age;
		// El olor se diluye al ensancharse: la intensidad en el centro baja con el área de la mancha.
		const float Dilution = PlumeBaseRadiusCm / Radius;
		const float Strength = Source.Amount01 * FMath::Exp(-Age / SmellDecaySeconds) * Dilution;
		const float Distance = static_cast<float>(FVector::Distance(PointCm, PlumeCenter(Source, World)));
		Total += Strength * (1.0f - FMath::SmoothStep(0.0f, Radius, Distance));
	}
	return Total;
}
