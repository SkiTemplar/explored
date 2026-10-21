#include "WorldGen/GrassConfigLoader.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GrassConfigDetail
{
	const TCHAR* const K6Meshes[GrassKindCount] = {
		TEXT("SM_GrassTuft_A_01"), TEXT("SM_GrassTuft_B_01"), TEXT("SM_GrassTuft_C_01"), TEXT("SM_GrassTall_01"),
		TEXT("SM_FlowerA_01"), TEXT("SM_FlowerB_01"), TEXT("SM_Fern_01"), TEXT("SM_Clover_01"),
		TEXT("SM_GrassPatch_A_01"), TEXT("SM_GrassPatch_B_01"), TEXT("SM_GrassPatch_C_01")};

	void ReadFloat(const FJsonObject& Obj, const TCHAR* Field, float& Out)
	{
		double V = 0.0;
		if (Obj.TryGetNumberField(Field, V))
		{
			Out = static_cast<float>(V);
		}
	}

	void ReadString(const FJsonObject& Obj, const TCHAR* Field, FString& Out)
	{
		FString V;
		if (Obj.TryGetStringField(Field, V) && !V.IsEmpty())
		{
			Out = V;
		}
	}

	const FJsonObject* Child(const FJsonObject& Obj, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Found = nullptr;
		return Obj.TryGetObjectField(Field, Found) && Found && Found->IsValid() ? Found->Get() : nullptr;
	}

	void ReadTop(const FJsonObject& Root, FGrassConfig& C)
	{
		ReadFloat(Root, TEXT("cellSizeM"), C.CellSizeM);
		double Work = C.WorkPerFrame;
		Root.TryGetNumberField(TEXT("workPerFrame"), Work);
		C.WorkPerFrame = static_cast<int32>(Work);
		double Seed = C.Seed;
		Root.TryGetNumberField(TEXT("seed"), Seed);
		C.Seed = static_cast<uint32>(FMath::Max(Seed, 0.0));
		ReadFloat(Root, TEXT("maxSlopeDeg"), C.MaxSlopeDeg);
		ReadFloat(Root, TEXT("clearanceM"), C.ClearanceM);
		ReadFloat(Root, TEXT("pathMarginM"), C.PathMarginM);
		ReadFloat(Root, TEXT("pointClearanceM"), C.PointClearanceM);
		ReadFloat(Root, TEXT("sinkM"), C.SinkM);
		ReadFloat(Root, TEXT("alignToNormal"), C.AlignToNormal);
		ReadFloat(Root, TEXT("edgeBandM"), C.EdgeBandM);
		const TArray<TSharedPtr<FJsonValue>>* Scale = nullptr;
		if (Root.TryGetArrayField(TEXT("scale"), Scale) && Scale && Scale->Num() == 2)
		{
			C.ScaleMin = static_cast<float>((*Scale)[0]->AsNumber());
			C.ScaleMax = static_cast<float>((*Scale)[1]->AsNumber());
		}
	}

	void ReadThinning(const FJsonObject& Root, FGrassThinning& T, FGrassAssets& A)
	{
		if (const FJsonObject* Thin = Child(Root, TEXT("thinning")))
		{
			ReadFloat(*Thin, TEXT("denseM"), T.DenseM);
			ReadFloat(*Thin, TEXT("endM"), T.EndM);
			ReadFloat(*Thin, TEXT("endFadeM"), T.EndFadeM);
			ReadFloat(*Thin, TEXT("exponent"), T.Exponent);
			ReadFloat(*Thin, TEXT("maxGrow"), T.MaxGrow);
			ReadFloat(*Thin, TEXT("growZ"), T.GrowZ);
			ReadFloat(*Thin, TEXT("band"), T.Band);
			ReadFloat(*Thin, TEXT("levelRatio"), T.LevelRatio);
			ReadFloat(*Thin, TEXT("minLevel"), T.MinLevel);
			ReadFloat(*Thin, TEXT("marginM"), T.MarginM);
			ReadFloat(*Thin, TEXT("hysteresisM"), T.HysteresisM);
		}
		if (const FJsonObject* Wind = Child(Root, TEXT("wind")))
		{
			ReadFloat(*Wind, TEXT("fadeStartM"), A.WindFadeStartM);
			ReadFloat(*Wind, TEXT("fadeEndM"), A.WindFadeEndM);
		}
	}

	bool ReadDensity(const FJsonObject& Root, FGrassConfig& C, FString& OutError)
	{
		if (const FJsonObject* Density = Child(Root, TEXT("density")))
		{
			ReadFloat(*Density, TEXT("default"), C.DensityPerM2);
			ReadFloat(*Density, TEXT("dirt"), C.DirtDensityPerM2);
			ReadFloat(*Density, TEXT("zoneFeatherM"), C.ZoneFeatherM);
			if (const FJsonObject* Zones = Child(*Density, TEXT("zones")))
			{
				C.ZoneDensity.Empty();
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Zones->Values)
				{
					double V = 0.0;
					if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(V))
					{
						OutError = FString::Printf(TEXT("density.zones.%s no es un número"), *Pair.Key);
						return false;
					}
					C.ZoneDensity.Add(FName(*Pair.Key), static_cast<float>(V));
				}
			}
		}
		if (const FJsonObject* Clearings = Child(Root, TEXT("clearings")))
		{
			ReadFloat(*Clearings, TEXT("cellM"), C.ClearingCellM);
			ReadFloat(*Clearings, TEXT("threshold"), C.ClearingThreshold);
			ReadFloat(*Clearings, TEXT("density"), C.ClearingDensity);
		}
		if (const FJsonObject* Palms = Child(Root, TEXT("palms")))
		{
			ReadFloat(*Palms, TEXT("trunkM"), C.PalmTrunkM);
			ReadFloat(*Palms, TEXT("radiusM"), C.PalmRadiusM);
		}
		return true;
	}

	bool ReadMeshes(const FJsonObject& Root, FGrassConfig& C, FGrassAssets& A, FString& OutError)
	{
		const FJsonObject* Meshes = Child(Root, TEXT("meshes"));
		if (!Meshes)
		{
			return true;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Meshes->Values)
		{
			EGrassKind Kind;
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!GrassKindFromName(Pair.Key, Kind) || !Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry)
			{
				OutError = FString::Printf(TEXT("meshes.%s no es una malla conocida"), *Pair.Key);
				return false;
			}
			const int32 K = static_cast<int32>(Kind);
			ReadString(**Entry, TEXT("mesh"), A.Meshes[K]);
			ReadFloat(**Entry, TEXT("scale"), C.KindScale[K]);
			ReadFloat(**Entry, TEXT("footprintM"), C.KindFootprintM[K]);
			(*Entry)->TryGetBoolField(TEXT("castShadow"), A.bCastShadow[K]);
		}
		return true;
	}

	bool ReadMixes(const FJsonObject& Root, FGrassConfig& C, FString& OutError)
	{
		const FJsonObject* Mixes = Child(Root, TEXT("mixes"));
		if (!Mixes)
		{
			return true;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Mixes->Values)
		{
			EGrassContext Context;
			const TSharedPtr<FJsonObject>* Weights = nullptr;
			if (!GrassContextFromName(Pair.Key, Context) || !Pair.Value.IsValid() || !Pair.Value->TryGetObject(Weights) || !Weights)
			{
				OutError = FString::Printf(TEXT("mixes.%s no es un sitio conocido"), *Pair.Key);
				return false;
			}
			FGrassMix Mix;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& W : (*Weights)->Values)
			{
				EGrassKind Kind;
				double V = 0.0;
				if (!GrassKindFromName(W.Key, Kind) || !W.Value.IsValid() || !W.Value->TryGetNumber(V))
				{
					OutError = FString::Printf(TEXT("mixes.%s.%s no válido"), *Pair.Key, *W.Key);
					return false;
				}
				Mix.Weights[static_cast<int32>(Kind)] = static_cast<float>(V);
			}
			C.Mixes[static_cast<int32>(Context)] = Mix;
		}
		return true;
	}
}

FGrassAssets FGrassAssets::Defaults()
{
	FGrassAssets A;
	for (int32 K = 0; K < GrassKindCount; ++K)
	{
		A.Meshes[K] = FString(TEXT("/Game/Art/K6_Cesped/")) + GrassConfigDetail::K6Meshes[K];
	}
	return A;
}

FString ExploredGrass::GrassJsonPath()
{
	return FPaths::ProjectContentDir() / TEXT("Data/grass.json");
}

FString ExploredGrass::GroundFieldPath()
{
	return FPaths::ProjectContentDir() / TEXT("Data/ground_field.bin");
}

bool ExploredGrass::ParseGrassJson(const FString& Json, FGrassConfig& OutConfig, FGrassAssets& OutAssets, FString& OutError)
{
	using namespace GrassConfigDetail;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON mal formado");
		return false;
	}
	FGrassConfig Config = FGrassConfig::Defaults();
	FGrassAssets Assets = FGrassAssets::Defaults();
	ReadTop(*Root, Config);
	ReadString(*Root, TEXT("material"), Assets.Material);
	ReadString(*Root, TEXT("fallbackMaterial"), Assets.FallbackMaterial);
	ReadString(*Root, TEXT("fallbackMesh"), Assets.FallbackMesh);
	ReadString(*Root, TEXT("parameterCollection"), Assets.ParameterCollection);
	ReadThinning(*Root, Config.Thinning, Assets);
	if (!ReadDensity(*Root, Config, OutError) || !ReadMeshes(*Root, Config, Assets, OutError) || !ReadMixes(*Root, Config, OutError))
	{
		return false;
	}
	if (!(Assets.WindFadeStartM >= 0.0f && Assets.WindFadeEndM > Assets.WindFadeStartM))
	{
		OutError = TEXT("wind: fadeStartM debe ser ≥ 0 y menor que fadeEndM");
		return false;
	}
	if (!Config.Validate(OutError))
	{
		return false;
	}
	OutConfig = MoveTemp(Config);
	OutAssets = MoveTemp(Assets);
	return true;
}
