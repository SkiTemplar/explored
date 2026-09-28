#include "WorldGen/IslandReliefModel.h"

#include "Core/ExploredRandom.h"
#include "Misc/ScopeLock.h"
#include "WorldGen/TerrainMetricsModel.h"

namespace
{
	/** La diferencia se apaga en el último 15 % de la rejilla: fuera, el terreno es la base. */
	constexpr float EdgeFadeStart = 0.85f;

	/** Lo que el tallado de ríos quita, limitado por la erosionabilidad (la roca dura no se talla). */
	void CarveRiversRespectingRock(FErosionHeightGrid& Work, const FDrainageParams& Drainage, FErosionHeightGrid& OutRiver)
	{
		const TArray<float> Before = Work.Heights;
		FDrainageModel::CarveRivers(Work, Drainage);
		OutRiver.Init(Work.Width, Work.Height, 0.0f);
		for (int32 I = 0; I < Before.Num(); ++I)
		{
			const float Erodibility = Work.ErodibilityAt(I % Work.Width, I / Work.Width);
			const float Cut = FMath::Max(0.0f, Before[I] - Work.Heights[I]) * Erodibility;
			// El relleno de pozos puede subir celdas: eso se respeta; lo que baja, según la roca.
			Work.Heights[I] = FMath::Max(Work.Heights[I], Before[I] - Cut);
			OutRiver.Heights[I] = Cut;
		}
	}
}

TSharedPtr<const FIslandReliefGrid> FIslandReliefGrid::Build(const FIslandReliefSettings& Settings,
	TFunctionRef<float(float, float)> BaseHeight, TFunctionRef<float(float, float)> Erodibility)
{
	TSharedPtr<FIslandReliefGrid> Result = MakeShared<FIslandReliefGrid>();
	const int32 Res = FMath::Clamp(Settings.Resolution, 16, FIslandReliefModel::MaxResolution);
	Result->QExtent = FMath::IsFinite(Settings.QExtent) && Settings.QExtent > 0.1f ? Settings.QExtent : 1.3f;
	const float Step = 2.0f * Result->QExtent / (Res - 1);

	FErosionHeightGrid Base;
	Base.Init(Res, Res, 0.0f);
	Base.Erodibility.Init(1.0f, Res * Res);
	for (int32 Gy = 0; Gy < Res; ++Gy)
	{
		for (int32 Gx = 0; Gx < Res; ++Gx)
		{
			const float Qx = -Result->QExtent + Gx * Step;
			const float Qy = -Result->QExtent + Gy * Step;
			Base.At(Gx, Gy) = BaseHeight(Qx, Qy);
			Base.Erodibility[Gy * Res + Gx] = Erodibility(Qx, Qy);
		}
	}
	FTerrainErosionModel::SanitizeGrid(Base);

	FErosionHeightGrid Work = Base;
	FTerrainErosionModel::Erode(Work, Settings.Erosion);
	if (Settings.bRivers)
	{
		CarveRiversRespectingRock(Work, Settings.Drainage, Result->River);
	}

	Result->PitsBefore = FTerrainMetricsModel::CountPits(Base.Heights, Res, Res, 0.5f, 0.3f);
	Result->PitsAfter = FTerrainMetricsModel::CountPits(Work.Heights, Res, Res, 0.5f, 0.3f);
	TArray<uint8> Carved;
	Carved.Init(0, Res * Res);
	for (int32 I = 0; I < Carved.Num(); ++I)
	{
		Carved[I] = Base.Heights[I] > 0.5f && Work.Heights[I] < Base.Heights[I] - 0.3f ? 1 : 0;
	}
	Result->ChannelOrientation = FTerrainMetricsModel::GradientOrientation(Work.Heights, Res, Res, Carved, 0.02f);
	Result->Delta.Init(Res, Res, 0.0f);
	for (int32 I = 0; I < Work.Heights.Num(); ++I)
	{
		Result->Delta.Heights[I] = Work.Heights[I] - Base.Heights[I];
	}
	return Result;
}

bool FIslandReliefGrid::ToCell(float Qx, float Qy, float& OutX, float& OutY) const
{
	if (Delta.Width < 2 || !FMath::IsFinite(Qx) || !FMath::IsFinite(Qy) || FMath::Abs(Qx) >= QExtent || FMath::Abs(Qy) >= QExtent)
	{
		return false;
	}
	const float Step = 2.0f * QExtent / (Delta.Width - 1);
	OutX = (Qx + QExtent) / Step;
	OutY = (Qy + QExtent) / Step;
	return true;
}

float FIslandReliefGrid::SampleDelta(float Qx, float Qy) const
{
	float X = 0.0f;
	float Y = 0.0f;
	if (!ToCell(Qx, Qy, X, Y))
	{
		return 0.0f;
	}
	const float Edge = FMath::Max(FMath::Abs(Qx), FMath::Abs(Qy));
	return Delta.Sample(X, Y) * (1.0f - FMath::SmoothStep(EdgeFadeStart * QExtent, QExtent, Edge));
}

float FIslandReliefGrid::SampleRiver(float Qx, float Qy) const
{
	float X = 0.0f;
	float Y = 0.0f;
	return River.Heights.Num() > 0 && ToCell(Qx, Qy, X, Y) ? River.Sample(X, Y) : 0.0f;
}

bool FIslandReliefModel::SettingsFor(EIslandArchetype Archetype, float Radius, uint32 Seed, FIslandReliefSettings& OutSettings)
{
	if (Archetype == EIslandArchetype::WhiteSands || Archetype == EIslandArchetype::Teeth || !(Radius > 0.0f) || !FMath::IsFinite(Radius))
	{
		return false;
	}
	FIslandReliefSettings S;
	S.Resolution = FMath::Clamp(FMath::RoundToInt32(2.0f * S.QExtent * Radius / TargetCellMeters), 64, MaxResolution);
	S.Erosion.Seed = ExploredHash::Hash32(Seed ^ 0xE205u);
	S.Erosion.CellSizeMeters = 2.0f * S.QExtent * Radius / S.Resolution;
	S.Erosion.DropletCount = S.Resolution * S.Resolution * 2 / 5;
	S.Erosion.MaxDropletLifetime = 40;
	S.Erosion.ErosionRadius = 3;
	S.Erosion.ThermalIterations = 30;
	S.Erosion.TalusAngleTangent = 0.9f;
	// Las gotas que llegan al mar o a una laguna sueltan ahí el sedimento (si no, la ciegan).
	S.Erosion.SeaLevel = 0.0f;
	S.bRivers = true;
	// Arroyos de isla: cuencas de al menos unos 2,5 ha y cauces de 10-24 m de ancho.
	S.Drainage.MinRiverArea = 1500.0f;
	S.Drainage.BaseHalfWidth = 2.0f;
	S.Drainage.HalfWidthPerSqrtArea = 0.03f;
	S.Drainage.MaxHalfWidth = 3.0f;
	S.Drainage.MaxRiverDepth = 3.0f;
	TuneForArchetype(Archetype, S);
	OutSettings = S;
	return true;
}

void FIslandReliefModel::TuneForArchetype(EIslandArchetype Archetype, FIslandReliefSettings& S)
{
	switch (Archetype)
	{
	case EIslandArchetype::Mesa:
		// Llano entre torres: los derrubios caen al pie de las paredes con talud de caliza.
		S.Erosion.ThermalIterations = 50;
		S.Erosion.TalusAngleTangent = 0.85f;
		break;
	case EIslandArchetype::Mangrove:
		// Llano de barro de pocos metros: poca erosión (si no, lo plancha) y canales de marea
		// que bajan algo por debajo del mar, con orillas tendidas.
		S.Erosion.DropletCount = S.Resolution * S.Resolution / 3;
		S.Erosion.MaxDropletLifetime = 24;
		S.Erosion.SedimentCapacityFactor = 2.2f;
		S.Erosion.ErodeSpeed = 0.25f;
		S.Erosion.DepositSpeed = 0.4f;
		S.Erosion.ThermalIterations = 6;
		S.Erosion.TalusAngleTangent = 0.7f;
		S.Drainage.MinRiverArea = 800.0f;
		S.Drainage.DepthPerSqrtArea = 0.08f;
		S.Drainage.MaxRiverDepth = 2.5f;
		S.Drainage.MinBedHeight = -1.6f;
		break;
	case EIslandArchetype::Smoke:
		// Barrancos en el cono: cuencas pequeñas pero muchas.
		S.Erosion.TalusAngleTangent = 0.8f;
		S.Erosion.MaxDropletLifetime = 60;
		S.Drainage.MinRiverArea = 1200.0f;
		break;
	case EIslandArchetype::Emerald:
		// Selva lluviosa: ríos más hondos que acaban en cascadas y gargantas.
		S.Drainage.MinRiverArea = 1000.0f;
		S.Drainage.MaxRiverDepth = 5.0f;
		S.Drainage.MaxHalfWidth = 4.0f;
		break;
	default:
		break;
	}
}

namespace
{
	struct FReliefKey
	{
		uint32 Seed = 0;
		uint32 Archetype = 0;
		uint32 HeightBits = 0;
		uint32 RadiusBits = 0;

		bool operator==(const FReliefKey& O) const
		{
			return Seed == O.Seed && Archetype == O.Archetype && HeightBits == O.HeightBits && RadiusBits == O.RadiusBits;
		}
		friend uint32 GetTypeHash(const FReliefKey& K)
		{
			return ExploredHash::Hash32(K.Seed ^ ExploredHash::Hash32(K.Archetype ^ ExploredHash::Hash32(K.HeightBits ^ ExploredHash::Hash32(K.RadiusBits))));
		}
	};

	FReliefKey MakeKey(const FIslandDesc& Island)
	{
		FReliefKey Key;
		Key.Seed = Island.Seed;
		Key.Archetype = static_cast<uint32>(Island.Archetype);
		FMemory::Memcpy(&Key.HeightBits, &Island.MaxHeight, sizeof(Key.HeightBits));
		FMemory::Memcpy(&Key.RadiusBits, &Island.Radius, sizeof(Key.RadiusBits));
		return Key;
	}
}

TSharedPtr<const FIslandReliefGrid> FIslandReliefModel::GetOrBuild(const FIslandDesc& Island, const FIslandReliefSettings& Settings,
	TFunctionRef<float(float, float)> BaseHeight, TFunctionRef<float(float, float)> Erodibility)
{
	static FCriticalSection Mutex;
	static TMap<FReliefKey, TSharedPtr<const FIslandReliefGrid>> Cache;
	const FReliefKey Key = MakeKey(Island);
	{
		FScopeLock Lock(&Mutex);
		if (const TSharedPtr<const FIslandReliefGrid>* Found = Cache.Find(Key))
		{
			return *Found;
		}
	}
	TSharedPtr<const FIslandReliefGrid> Built = FIslandReliefGrid::Build(Settings, BaseHeight, Erodibility);
	FScopeLock Lock(&Mutex);
	// Si otro hilo la ha construido mientras tanto, gana la primera (son idénticas).
	if (const TSharedPtr<const FIslandReliefGrid>* Found = Cache.Find(Key))
	{
		return *Found;
	}
	Cache.Add(Key, Built);
	return Built;
}
