#include "WorldGen/IslandLayoutLoader.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Explored.h"

namespace IslandLayoutLoaderDetail
{
	using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

	bool ReadVector2D(const FJsonObject& Obj, const TCHAR* Field, FVector2D& Out)
	{
		const FJsonArray* Values = nullptr;
		if (!Obj.TryGetArrayField(Field, Values) || !Values || Values->Num() != 2)
		{
			return false;
		}
		double X = 0.0;
		double Y = 0.0;
		if (!(*Values)[0].IsValid() || !(*Values)[1].IsValid() || !(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y))
		{
			return false;
		}
		Out = FVector2D(X, Y);
		return true;
	}

	bool ReadFloat(const FJsonObject& Obj, const TCHAR* Field, float& Out, bool bRequired)
	{
		double Value = 0.0;
		if (!Obj.TryGetNumberField(Field, Value))
		{
			return !bRequired;
		}
		Out = static_cast<float>(Value);
		return FMath::IsFinite(Out);
	}

	bool ReadId(const FJsonObject& Obj, FName& Out)
	{
		FString Id;
		if (!Obj.TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			return false;
		}
		Out = FName(*Id);
		return true;
	}

	bool ReadBiome(const FJsonObject& Obj, EIslandArchetype& Out)
	{
		FString Biome;
		return Obj.TryGetStringField(TEXT("biome"), Biome) && IslandArchetypeFromName(Biome, Out);
	}

	bool ReadObjects(const FJsonObject& Root, const TCHAR* Field, TArray<TSharedPtr<FJsonObject>>& Out, FString& OutError)
	{
		const FJsonArray* Values = nullptr;
		if (!Root.TryGetArrayField(Field, Values) || !Values)
		{
			OutError = FString::Printf(TEXT("falta el array «%s»"), Field);
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Obj.IsValid())
			{
				OutError = FString::Printf(TEXT("«%s» contiene algo que no es un objeto"), Field);
				return false;
			}
			Out.Add(Obj);
		}
		return true;
	}

	/** «coast»: lista opcional de [k, amplitud, fase en grados]. */
	bool ParseCoast(const FJsonObject& Obj, TArray<FCoastHarmonic>& Out)
	{
		const FJsonArray* Values = nullptr;
		if (!Obj.TryGetArrayField(TEXT("coast"), Values) || !Values)
		{
			return true;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const FJsonArray* Triple = nullptr;
			double K = 0.0;
			double Amplitude = 0.0;
			double PhaseDeg = 0.0;
			if (!Value.IsValid() || !Value->TryGetArray(Triple) || !Triple || Triple->Num() != 3
				|| !(*Triple)[0]->TryGetNumber(K) || !(*Triple)[1]->TryGetNumber(Amplitude) || !(*Triple)[2]->TryGetNumber(PhaseDeg)
				|| K < 1.0 || FMath::Abs(Amplitude) >= 0.5)
			{
				return false;
			}
			FCoastHarmonic& Harmonic = Out.AddDefaulted_GetRef();
			Harmonic.K = static_cast<int32>(K);
			Harmonic.Amplitude = static_cast<float>(Amplitude);
			Harmonic.Phase = FMath::DegreesToRadians(static_cast<float>(PhaseDeg));
		}
		return true;
	}

	bool ParseRock(const FJsonObject& Obj, FIslandRock& Out)
	{
		return ReadId(Obj, Out.Id) && ReadVector2D(Obj, TEXT("center"), Out.Center) && ReadFloat(Obj, TEXT("radius"), Out.Radius, true)
			&& ReadFloat(Obj, TEXT("height"), Out.Height, true);
	}

	bool ParsePath(const FJsonObject& Obj, FIslandPath& Out)
	{
		const FJsonArray* Values = nullptr;
		if (!ReadId(Obj, Out.Id) || !ReadFloat(Obj, TEXT("width"), Out.Width, false) || !Obj.TryGetArrayField(TEXT("points"), Values) || !Values)
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const FJsonArray* Pair = nullptr;
			double X = 0.0;
			double Y = 0.0;
			if (!Value.IsValid() || !Value->TryGetArray(Pair) || !Pair || Pair->Num() != 2 || !(*Pair)[0]->TryGetNumber(X) || !(*Pair)[1]->TryGetNumber(Y))
			{
				return false;
			}
			Out.Points.Add(FVector2D(X, Y));
		}
		return true;
	}

	bool ParseIsland(const FJsonObject& Obj, FIslandDesc& Out)
	{
		float RotationDeg = 0.0f;
		const bool bOk = ReadId(Obj, Out.Id) && ReadBiome(Obj, Out.Archetype) && ReadVector2D(Obj, TEXT("center"), Out.Center)
			&& ReadFloat(Obj, TEXT("radius"), Out.Radius, true) && ReadFloat(Obj, TEXT("aspect"), Out.Aspect, false)
			&& ReadFloat(Obj, TEXT("rotationDeg"), RotationDeg, false) && ReadFloat(Obj, TEXT("maxHeight"), Out.MaxHeight, true);
		Out.Rotation = FMath::DegreesToRadians(RotationDeg);
		return bOk && ParseCoast(Obj, Out.Coast);
	}

	bool ParseZone(const FJsonObject& Obj, FIslandZone& Out)
	{
		return ReadId(Obj, Out.Id) && Obj.TryGetStringField(TEXT("name"), Out.Name) && ReadBiome(Obj, Out.Biome)
			&& ReadVector2D(Obj, TEXT("center"), Out.Center) && ReadFloat(Obj, TEXT("radius"), Out.Radius, true)
			&& ReadFloat(Obj, TEXT("hill"), Out.Hill, false) && ReadFloat(Obj, TEXT("hillSigma"), Out.HillSigma, false);
	}

	bool ParsePoint(const FJsonObject& Obj, FIslandPoint& Out)
	{
		return ReadId(Obj, Out.Id) && ReadVector2D(Obj, TEXT("position"), Out.Location) && ReadFloat(Obj, TEXT("yaw"), Out.Yaw, false);
	}

	TSharedRef<const FIslandLayout> Load()
	{
		FString Json;
		FIslandLayout Layout;
		FString Error;
		const FString Path = ExploredIsland::IslandJsonPath();
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogExplored, Warning, TEXT("[Isla] No se pudo leer %s; se usa la disposición provisional"), *Path);
		}
		else if (!ExploredIsland::ParseIslandJson(Json, Layout, Error))
		{
			UE_LOG(LogExplored, Warning, TEXT("[Isla] %s no es válido (%s); se usa la disposición provisional"), *Path, *Error);
		}
		else
		{
			UE_LOG(LogExplored, Display, TEXT("[Isla] %d tierras, %d zonas, %d rocas y %d puntos desde island.json"),
				Layout.Islands.Num(), Layout.Zones.Num(), Layout.Rocks.Num(), Layout.Points.Num());
			return MakeShared<const FIslandLayout>(MoveTemp(Layout));
		}
		return MakeShared<const FIslandLayout>(FIslandLayout::Provisional());
	}
}

namespace ExploredIsland
{
	FString IslandJsonPath()
	{
		return FPaths::ProjectContentDir() / TEXT("Data") / TEXT("island.json");
	}

	bool ParseIslandJson(const FString& Json, FIslandLayout& OutLayout, FString& OutError)
	{
		using namespace IslandLayoutLoaderDetail;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("JSON mal formado");
			return false;
		}
		FIslandLayout Layout;
		double Seed = 0.0;
		if (Root->TryGetNumberField(TEXT("seed"), Seed))
		{
			if (Seed < 0.0 || Seed > static_cast<double>(MAX_uint32))
			{
				OutError = TEXT("«seed» fuera de rango");
				return false;
			}
			Layout.Seed = static_cast<uint32>(Seed);
		}

		TArray<TSharedPtr<FJsonObject>> Islands;
		TArray<TSharedPtr<FJsonObject>> Zones;
		TArray<TSharedPtr<FJsonObject>> Points;
		if (!ReadObjects(*Root, TEXT("islands"), Islands, OutError) || !ReadObjects(*Root, TEXT("zones"), Zones, OutError)
			|| !ReadObjects(*Root, TEXT("points"), Points, OutError))
		{
			return false;
		}
		for (const TSharedPtr<FJsonObject>& Obj : Islands)
		{
			if (!ParseIsland(*Obj, Layout.Islands.AddDefaulted_GetRef()))
			{
				OutError = FString::Printf(TEXT("tierra %d incompleta"), Layout.Islands.Num() - 1);
				return false;
			}
		}
		for (const TSharedPtr<FJsonObject>& Obj : Zones)
		{
			if (!ParseZone(*Obj, Layout.Zones.AddDefaulted_GetRef()))
			{
				OutError = FString::Printf(TEXT("zona %d incompleta"), Layout.Zones.Num() - 1);
				return false;
			}
		}
		// «rocks» es opcional (los layouts de prueba antiguos no lo traen).
		TArray<TSharedPtr<FJsonObject>> Rocks;
		if (Root->HasField(TEXT("rocks")) && !ReadObjects(*Root, TEXT("rocks"), Rocks, OutError))
		{
			return false;
		}
		for (const TSharedPtr<FJsonObject>& Obj : Rocks)
		{
			if (!ParseRock(*Obj, Layout.Rocks.AddDefaulted_GetRef()))
			{
				OutError = FString::Printf(TEXT("roca %d incompleta"), Layout.Rocks.Num() - 1);
				return false;
			}
		}
		TArray<TSharedPtr<FJsonObject>> Paths;
		if (Root->HasField(TEXT("paths")) && !ReadObjects(*Root, TEXT("paths"), Paths, OutError))
		{
			return false;
		}
		for (const TSharedPtr<FJsonObject>& Obj : Paths)
		{
			if (!ParsePath(*Obj, Layout.Paths.AddDefaulted_GetRef()))
			{
				OutError = FString::Printf(TEXT("camino %d incompleto"), Layout.Paths.Num() - 1);
				return false;
			}
		}
		for (const TSharedPtr<FJsonObject>& Obj : Points)
		{
			if (!ParsePoint(*Obj, Layout.Points.AddDefaulted_GetRef()))
			{
				OutError = FString::Printf(TEXT("punto %d incompleto"), Layout.Points.Num() - 1);
				return false;
			}
		}
		if (!Layout.Validate(OutError))
		{
			return false;
		}
		OutLayout = MoveTemp(Layout);
		return true;
	}

	TSharedRef<const FIslandLayout> Get()
	{
		static const TSharedRef<const FIslandLayout> Cached = IslandLayoutLoaderDetail::Load();
		return Cached;
	}
}
