#include "Exploration/ExplorationContentLoader.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Exploration/ExplorationContentModel.h"

bool ExplorationContentLoader::ParseJson(const FString& JsonText, FExplorationCatalog& OutCatalog, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("exploration.json no es un JSON válido");
		return false;
	}

	FExplorationCatalog Result;
	const TArray<TSharedPtr<FJsonValue>>* Islands = nullptr;
	if (!Root->TryGetArrayField(TEXT("islands"), Islands) || !Islands)
	{
		OutError = TEXT("exploration.json sin «islands»");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& IslandValue : *Islands)
	{
		const TSharedPtr<FJsonObject> IslandObj = IslandValue.IsValid() ? IslandValue->AsObject() : nullptr;
		FString IslandId;
		if (!IslandObj.IsValid() || !IslandObj->TryGetStringField(TEXT("id"), IslandId) || IslandId.IsEmpty())
		{
			OutError = TEXT("Isla sin «id» en exploration.json");
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* LandmarkValues = nullptr;
		if (!IslandObj->TryGetArrayField(TEXT("landmarks"), LandmarkValues) || !LandmarkValues)
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& LandmarkValue : *LandmarkValues)
		{
			const TSharedPtr<FJsonObject> Obj = LandmarkValue.IsValid() ? LandmarkValue->AsObject() : nullptr;
			FString Id;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
			{
				OutError = FString::Printf(TEXT("Lugar sin «id» en la isla «%s»"), *IslandId);
				return false;
			}
			FExplorationLandmark Landmark;
			Landmark.Id = FName(*Id);
			Landmark.IslandId = FName(*IslandId);
			FString Kind, LinkedPoi, MapMark;
			Obj->TryGetStringField(TEXT("kind"), Kind);
			Obj->TryGetStringField(TEXT("nameEs"), Landmark.NameEs);
			Obj->TryGetStringField(TEXT("nameEn"), Landmark.NameEn);
			Obj->TryGetNumberField(TEXT("angleDeg"), Landmark.AngleDeg);
			Obj->TryGetNumberField(TEXT("distanceFrac"), Landmark.DistanceFrac);
			Obj->TryGetStringField(TEXT("linkedPoi"), LinkedPoi);
			Obj->TryGetStringField(TEXT("mapMark"), MapMark);
			Obj->TryGetStringField(TEXT("accessEs"), Landmark.AccessEs);
			Obj->TryGetStringField(TEXT("rewardEs"), Landmark.RewardEs);
			Landmark.Kind = FName(*Kind);
			Landmark.LinkedPoi = LinkedPoi.IsEmpty() ? FName() : FName(*LinkedPoi);
			Landmark.MapMark = MapMark.IsEmpty() ? FName() : FName(*MapMark);
			Result.Landmarks.Add(MoveTemp(Landmark));
		}
	}

	TArray<FString> Errors;
	if (!Result.Validate(Errors))
	{
		OutError = FString::Join(Errors, TEXT("; "));
		return false;
	}

	OutCatalog = MoveTemp(Result);
	return true;
}
