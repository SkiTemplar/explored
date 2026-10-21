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

	bool ParseIsland(const FJsonObject& Obj, FIslandDesc& Out)
	{
		float RotationDeg = 0.0f;
		const bool bOk = ReadId(Obj, Out.Id) && ReadBiome(Obj, Out.Archetype) && ReadVector2D(Obj, TEXT("center"), Out.Center)
			&& ReadFloat(Obj, TEXT("radius"), Out.Radius, true) && ReadFloat(Obj, TEXT("aspect"), Out.Aspect, false)
			&& ReadFloat(Obj, TEXT("rotationDeg"), RotationDeg, false) && ReadFloat(Obj, TEXT("maxHeight"), Out.MaxHeight, true);
		Out.Rotation = FMath::DegreesToRadians(RotationDeg);
		return bOk;
	}

	bool ParseZone(const FJsonObject& Obj, FIslandZone& Out)
	{
		return ReadId(Obj, Out.Id) && Obj.TryGetStringField(TEXT("name"), Out.Name) && ReadBiome(Obj, Out.Biome)
			&& ReadVector2D(Obj, TEXT("center"), Out.Center) && ReadFloat(Obj, TEXT("radius"), Out.Radius, true)
			&& ReadFloat(Obj, TEXT("hill"), Out.Hill, false);
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
			UE_LOG(LogExplored, Display, TEXT("[Isla] %d tierras, %d zonas y %d puntos desde island.json"),
				Layout.Islands.Num(), Layout.Zones.Num(), Layout.Points.Num());
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
