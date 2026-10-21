#include "Items/IslandDataLoader.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Core/ExploredLocalization.h"
#include "Explored.h"

namespace IslandDataDetail
{
	using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

	TSharedPtr<FJsonValue> ParseValue(const FString& Text)
	{
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Text, const TCHAR* File, TArray<FString>& Errors)
	{
		const TSharedPtr<FJsonValue> Root = ParseValue(Text);
		if (!Root.IsValid() || Root->Type != EJson::Object)
		{
			Errors.Add(FString::Printf(TEXT("%s: JSON mal formado o sin objeto raíz"), File));
			return nullptr;
		}
		return Root->AsObject();
	}

	float Number(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Default)
	{
		double Value = Default;
		return Obj.IsValid() && Obj->TryGetNumberField(Field, Value) ? static_cast<float>(Value) : Default;
	}

	TArray<FRecipeAmount> Amounts(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		TArray<FRecipeAmount> Out;
		const FJsonArray* Array = nullptr;
		if (!Obj->TryGetArrayField(Field, Array))
		{
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Item;
			if (Entry.IsValid() && Entry->TryGetStringField(TEXT("item"), Item))
			{
				Out.Add({ FName(*Item), FMath::Max(1, static_cast<int32>(Number(Entry, TEXT("count"), 1.0f))) });
			}
		}
		return Out;
	}

	void ParseItems(FIslandData& Data, const FString& Text, TArray<FString>& Errors)
	{
		const TSharedPtr<FJsonValue> Root = ParseValue(Text);
		if (!Root.IsValid() || Root->Type != EJson::Array)
		{
			Errors.Add(TEXT("items.json: JSON mal formado o sin lista raíz"));
			return;
		}
		FBackpackRules Rules;
		Rules.Slots = Data.Backpack.Slots;
		for (const TSharedPtr<FJsonValue>& Value : Root->AsArray())
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Id, Es, En;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
			{
				Errors.Add(TEXT("items.json: objeto sin id"));
				continue;
			}
			FBackpackItemRule Rule;
			Rule.Id = FName(*Id);
			Rule.Stack = FMath::Max(1, static_cast<int32>(Number(Obj, TEXT("stack"), 1.0f)));
			Obj->TryGetBoolField(TEXT("heavy"), Rule.bHeavy);
			Rules.Items.Add(Rule.Id, Rule);
			Obj->TryGetStringField(TEXT("nameEs"), Es);
			Obj->TryGetStringField(TEXT("nameEn"), En);
			Data.ItemNames.Add(Rule.Id, TPair<FString, FString>(Es, En));
			FString DescEs, DescEn;
			Obj->TryGetStringField(TEXT("descEs"), DescEs);
			Obj->TryGetStringField(TEXT("descEn"), DescEn);
			Data.ItemDescriptions.Add(Rule.Id, TPair<FString, FString>(DescEs, DescEn));
		}
		if (Rules.Items.Num() > 0)
		{
			Data.Backpack = MoveTemp(Rules);
		}
	}

	bool ParseRecipe(const TSharedPtr<FJsonObject>& Obj, FRecipeDef& Out)
	{
		FString Id, Verb, Place;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || !Obj->TryGetStringField(TEXT("verb"), Verb)
			|| !Obj->TryGetStringField(TEXT("place"), Place) || !CraftPlaceFromName(Place, Out.Place))
		{
			return false;
		}
		Out.Id = FName(*Id);
		Out.Verb = FName(*Verb);
		Obj->TryGetStringField(TEXT("nameEs"), Out.NameEs);
		Obj->TryGetStringField(TEXT("nameEn"), Out.NameEn);
		Out.Inputs = Amounts(Obj, TEXT("inputs"));
		Out.StationNeeds = Amounts(Obj, TEXT("stationNeeds"));
		TArray<FString> Tools;
		Obj->TryGetStringArrayField(TEXT("tools"), Tools);
		for (const FString& Tool : Tools)
		{
			Out.Tools.Add(FName(*Tool));
		}
		Out.Seconds = Number(Obj, TEXT("seconds"), 1.0f);
		Out.Steps = static_cast<int32>(Number(Obj, TEXT("steps"), 1.0f));
		const TSharedPtr<FJsonObject>* Output = nullptr;
		if (!Obj->TryGetObjectField(TEXT("output"), Output) || !Output)
		{
			return false;
		}
		FString Item, World;
		if ((*Output)->TryGetStringField(TEXT("item"), Item))
		{
			Out.OutputItem = FName(*Item);
			Out.OutputCount = static_cast<int32>(Number(*Output, TEXT("count"), 1.0f));
		}
		if ((*Output)->TryGetStringField(TEXT("world"), World))
		{
			Out.OutputWorld = FName(*World);
		}
		return true;
	}

	void ParseRecipes(FIslandData& Data, const FString& RecipesText, const FString& VerbsText, TArray<FString>& Errors)
	{
		FRecipeBook Book;
		const TSharedPtr<FJsonValue> Verbs = ParseValue(VerbsText);
		if (Verbs.IsValid() && Verbs->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Value : Verbs->AsArray())
			{
				FString Id;
				if (Value.IsValid() && Value->AsObject().IsValid() && Value->AsObject()->TryGetStringField(TEXT("id"), Id))
				{
					Book.Verbs.Add(FName(*Id));
				}
			}
		}
		const TSharedPtr<FJsonObject> Root = ParseObject(RecipesText, TEXT("recipes.json"), Errors);
		const FJsonArray* Recipes = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("recipes"), Recipes))
		{
			Errors.Add(TEXT("recipes.json: falta la lista «recipes»"));
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Recipes)
		{
			FRecipeDef Recipe;
			if (ParseRecipe(Value.IsValid() ? Value->AsObject() : nullptr, Recipe))
			{
				Book.Recipes.Add(MoveTemp(Recipe));
			}
			else
			{
				Errors.Add(TEXT("recipes.json: receta incompleta (id, verb, place y output)"));
				return;
			}
		}
		FString Error;
		if (Book.Recipes.Num() == 0)
		{
			Errors.Add(TEXT("recipes.json: sin recetas"));
			return;
		}
		if (!Book.Validate(Error))
		{
			Errors.Add(FString::Printf(TEXT("recipes.json: %s"), *Error));
			return;
		}
		Data.Recipes = MoveTemp(Book);
	}

	void ParseFuels(FIslandData& Data, const FString& Text, TArray<FString>& Errors)
	{
		const TSharedPtr<FJsonObject> Root = ParseObject(Text, TEXT("fuels.json"), Errors);
		const TSharedPtr<FJsonObject>* Fire = nullptr;
		if (!Root.IsValid() || !Root->TryGetObjectField(TEXT("campfire"), Fire) || !Fire)
		{
			return;
		}
		FCampfireConfig& Config = Data.Campfire;
		Config.BurnSeconds = Number(*Fire, TEXT("burnSeconds"), Config.BurnSeconds);
		Config.MaxBurnSeconds = Number(*Fire, TEXT("maxBurnSeconds"), Config.MaxBurnSeconds);
		Config.HeatRadiusMeters = Number(*Fire, TEXT("heatRadiusMeters"), Config.HeatRadiusMeters);
		Config.WarmthPerMinute = Number(*Fire, TEXT("warmthPerMinute"), Config.WarmthPerMinute);
		const FJsonArray* Fuels = nullptr;
		if (Root->TryGetArrayField(TEXT("fuels"), Fuels))
		{
			Config.FuelSeconds.Reset();
			for (const TSharedPtr<FJsonValue>& Value : *Fuels)
			{
				const TSharedPtr<FJsonObject> Fuel = Value.IsValid() ? Value->AsObject() : nullptr;
				FString Item;
				if (Fuel.IsValid() && Fuel->TryGetStringField(TEXT("item"), Item))
				{
					Config.FuelSeconds.Add(FName(*Item), Number(Fuel, TEXT("secondsPerFuel"), 0.0f));
				}
			}
		}
	}

	TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Obj.IsValid() && Obj->TryGetObjectField(Field, Out) && Out ? *Out : nullptr;
	}

	/** Barras de survival_needs.json: «needs» (inicio y pérdidas), umbrales, «faint» y «sleep». */
	void ParseBody(FIslandData& Data, const TSharedPtr<FJsonObject>& Root, TArray<FString>& Errors)
	{
		FIslandBodyTuning Body = Data.Body;
		const FJsonArray* Needs = nullptr;
		if (Root->TryGetArrayField(TEXT("needs"), Needs))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Needs)
			{
				const TSharedPtr<FJsonObject> Need = Value.IsValid() ? Value->AsObject() : nullptr;
				FString Id;
				if (!Need.IsValid() || !Need->TryGetStringField(TEXT("id"), Id))
				{
					continue;
				}
				if (Id == TEXT("sed"))
				{
					Body.Initial.Thirst = Number(Need, TEXT("initial"), Body.Initial.Thirst);
					Body.ThirstLossPerMinute = Number(Need, TEXT("lossPerMinute"), Body.ThirstLossPerMinute);
				}
				else if (Id == TEXT("hambre"))
				{
					Body.Initial.Hunger = Number(Need, TEXT("initial"), Body.Initial.Hunger);
					Body.HungerLossPerMinute = Number(Need, TEXT("lossPerMinute"), Body.HungerLossPerMinute);
				}
				else if (Id == TEXT("energia"))
				{
					Body.Initial.Energy = Number(Need, TEXT("initial"), Body.Initial.Energy);
					Body.EnergyLossPerMinute = Number(Need, TEXT("lossPerMinute"), Body.EnergyLossPerMinute);
				}
				else if (Id == TEXT("calor"))
				{
					Body.Initial.Warmth = Number(Need, TEXT("initial"), Body.Initial.Warmth);
					Body.NightWarmthLossPerMinute = Number(Need, TEXT("nightLossPerMinute"), Body.NightWarmthLossPerMinute);
					Body.RainWarmthLossPerMinute = Number(Need, TEXT("rainLossPerMinute"), Body.RainWarmthLossPerMinute);
					Body.FireWarmthGainPerMinute = Number(Need, TEXT("fireGainPerMinute"), Body.FireWarmthGainPerMinute);
				}
			}
		}
		Body.LowThreshold = Number(Root, TEXT("lowThreshold"), Body.LowThreshold);
		Body.LowSpeedFactor = Number(Root, TEXT("lowSpeedFactor"), Body.LowSpeedFactor);
		Body.WarningAt = Number(Root, TEXT("warningAt"), Body.WarningAt);
		if (const TSharedPtr<FJsonObject> Faint = Child(Root, TEXT("faint")))
		{
			FBodyEventsTuning& E = Body.Events;
			E.FaintSeconds = Number(Faint, TEXT("zeroSeconds"), E.FaintSeconds);
			E.FaintHours = Number(Faint, TEXT("skipHours"), static_cast<float>(E.FaintHours));
			E.WakeAfterFaint.Thirst = Number(Faint, TEXT("wakeThirst"), E.WakeAfterFaint.Thirst);
			E.WakeAfterFaint.Hunger = Number(Faint, TEXT("wakeHunger"), E.WakeAfterFaint.Hunger);
			E.WakeAfterFaint.Energy = Number(Faint, TEXT("wakeEnergy"), E.WakeAfterFaint.Energy);
			E.WakeAfterFaint.Warmth = Number(Faint, TEXT("wakeWarmth"), E.WakeAfterFaint.Warmth);
		}
		if (const TSharedPtr<FJsonObject> Sleep = Child(Root, TEXT("sleep")))
		{
			FBodyEventsTuning& E = Body.Events;
			E.SleepFromHour = Number(Sleep, TEXT("fromHour"), static_cast<float>(E.SleepFromHour));
			E.SleepUntilHour = Number(Sleep, TEXT("toHour"), static_cast<float>(E.SleepUntilHour));
			E.WakeHour = Number(Sleep, TEXT("wakeHour"), static_cast<float>(E.WakeHour));
			E.SleepThirstDelta = Number(Sleep, TEXT("thirstDelta"), E.SleepThirstDelta);
			E.SleepHungerDelta = Number(Sleep, TEXT("hungerDelta"), E.SleepHungerDelta);
			E.SleepThirstWarning = Number(Sleep, TEXT("warnThirstBelow"), E.SleepThirstWarning);
		}
		if (!Body.IsValid())
		{
			Errors.Add(TEXT("survival_needs.json: valores del cuerpo no válidos"));
			return;
		}
		Data.Body = Body;
	}

	bool StateFromName(const FString& Name, EStormState& Out)
	{
		static const TCHAR* const Names[] = { TEXT("Clear"), TEXT("Squall"), TEXT("HighClouds"), TEXT("SeaClosed"),
			TEXT("CycloneWall1"), TEXT("CycloneEye"), TEXT("CycloneWall2") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (Name == Names[Index])
			{
				Out = static_cast<EStormState>(Index);
				return true;
			}
		}
		return false;
	}

	/** {"day": D, "hour": H} en horas absolutas desde el día 1 a las 00:00. */
	double AbsHour(const TSharedPtr<FJsonObject>& Obj, double Default)
	{
		if (!Obj.IsValid())
		{
			return Default;
		}
		double Day = 0.0;
		double Hour = 0.0;
		return Obj->TryGetNumberField(TEXT("day"), Day) && Obj->TryGetNumberField(TEXT("hour"), Hour)
			? FStormClockModel::ToAbsoluteHours(static_cast<int32>(Day), Hour) : Default;
	}

	void ParseNeeds(FIslandData& Data, const FString& Text, TArray<FString>& Errors)
	{
		const TSharedPtr<FJsonObject> Root = ParseObject(Text, TEXT("survival_needs.json"), Errors);
		const FJsonArray* Consumables = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("consumables"), Consumables))
		{
			return;
		}
		FIslandConsumables Table;
		for (const TSharedPtr<FJsonValue>& Value : *Consumables)
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Item, Leaves;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("item"), Item))
			{
				continue;
			}
			FConsumableEffect Effect;
			Effect.Item = FName(*Item);
			Effect.Thirst = Number(Obj, TEXT("thirst"), 0.0f);
			Effect.Hunger = Number(Obj, TEXT("hunger"), 0.0f);
			Effect.Energy = Number(Obj, TEXT("energy"), 0.0f);
			Effect.bSips = Obj->HasField(TEXT("sips"));
			if (Obj->TryGetStringField(TEXT("leaves"), Leaves))
			{
				Effect.Leaves = FName(*Leaves);
			}
			Table.Effects.Add(Effect);
		}
		if (Table.Effects.Num() > 0)
		{
			Data.Consumables = MoveTemp(Table);
		}
		ParseBody(Data, Root, Errors);
		Data.Backpack.Slots = FMath::Max(1, static_cast<int32>(Number(Root, TEXT("backpackSlots"), static_cast<float>(Data.Backpack.Slots))));
	}

	FVector2f Range(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FVector2f& Default)
	{
		const FJsonArray* Array = nullptr;
		if (!Obj->TryGetArrayField(Field, Array) || Array->Num() != 2)
		{
			return Default;
		}
		return FVector2f(static_cast<float>((*Array)[0]->AsNumber()), static_cast<float>((*Array)[1]->AsNumber()));
	}

	void ParseFish(FIslandData& Data, const FString& Text, TArray<FString>& Errors)
	{
		const TSharedPtr<FJsonObject> Root = ParseObject(Text, TEXT("fish.json"), Errors);
		if (!Root.IsValid())
		{
			return;
		}
		FCastFishingConfig& Config = Data.Fishing;
		Config.MeanBiteSeconds = Number(Root, TEXT("meanBiteSeconds"), Config.MeanBiteSeconds);
		Config.MinBiteSeconds = Number(Root, TEXT("minBiteSeconds"), Config.MinBiteSeconds);
		Config.BiteWindowSeconds = Number(Root, TEXT("biteWindowSeconds"), Config.BiteWindowSeconds);
		Config.GoldenHourFactor = Number(Root, TEXT("goldenHourFactor"), Config.GoldenHourFactor);
		Config.Dawn = Range(Root, TEXT("dawn"), Config.Dawn);
		Config.Dusk = Range(Root, TEXT("dusk"), Config.Dusk);
		Config.Night = Range(Root, TEXT("night"), Config.Night);
		FString Rod, Catch;
		if (Root->TryGetStringField(TEXT("rod"), Rod))
		{
			Config.Rod = FName(*Rod);
		}
		if (Root->TryGetStringField(TEXT("catch"), Catch))
		{
			Config.Catch = FName(*Catch);
		}
	}

	FString ReadData(const TCHAR* File)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), File));
		return Text;
	}

	TUniquePtr<FIslandData>& Cache()
	{
		static TUniquePtr<FIslandData> Instance;
		return Instance;
	}
}

FIslandData FIslandData::Defaults()
{
	FIslandData Data;
	Data.Backpack = FBackpackRules::Default();
	Data.Recipes = FRecipeBook::Default();
	Data.Consumables = FIslandConsumables::Default();
	Data.Campfire = FCampfireConfig::Default();
	Data.Storm = FStormClockConfig::Gdd();
	return Data;
}

TArray<FString> FIslandData::ParseInto(FIslandData& InOut, const FString& ItemsJson, const FString& RecipesJson, const FString& VerbsJson,
	const FString& FuelsJson, const FString& NeedsJson, const FString& FishJson)
{
	using namespace IslandDataDetail;
	TArray<FString> Errors;
	if (!ItemsJson.IsEmpty()) { ParseItems(InOut, ItemsJson, Errors); }
	if (!RecipesJson.IsEmpty()) { ParseRecipes(InOut, RecipesJson, VerbsJson, Errors); }
	if (!FuelsJson.IsEmpty()) { ParseFuels(InOut, FuelsJson, Errors); }
	if (!NeedsJson.IsEmpty()) { ParseNeeds(InOut, NeedsJson, Errors); }
	if (!FishJson.IsEmpty()) { ParseFish(InOut, FishJson, Errors); }
	return Errors;
}

TArray<FString> FIslandData::ParseStorm(FStormClockConfig& InOut, const FString& StormJson)
{
	using namespace IslandDataDetail;
	TArray<FString> Errors;
	if (StormJson.IsEmpty())
	{
		return Errors;
	}
	const TSharedPtr<FJsonObject> Root = ParseObject(StormJson, TEXT("storm_curve.json"), Errors);
	if (!Root.IsValid())
	{
		return Errors;
	}
	FStormClockConfig Config = InOut;
	const FJsonArray* Points = nullptr;
	if (Root->TryGetArrayField(TEXT("points"), Points))
	{
		Config.Points.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Points)
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			FStormCurvePoint Point;
			Point.Hour = AbsHour(Obj, -1.0);
			Point.Hpa = Number(Obj, TEXT("hpa"), 0.0f);
			Config.Points.Add(Point);
		}
	}
	const FJsonArray* Windows = nullptr;
	if (Root->TryGetArrayField(TEXT("windows"), Windows))
	{
		Config.Windows.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Windows)
		{
			const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
			FString StateName;
			FStormWindow Window;
			if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("state"), StateName) || !StateFromName(StateName, Window.State))
			{
				Errors.Add(TEXT("storm_curve.json: ventana sin estado conocido"));
				continue;
			}
			Window.StartHour = AbsHour(Obj, -1.0);
			// Ventanas seguidas: un inicio a menos de 1 s de juego del final anterior se pega a él.
			if (Config.Windows.Num() > 0 && FMath::Abs(Window.StartHour - Config.Windows.Last().EndHour) < 1.0 / 3600.0)
			{
				Window.StartHour = Config.Windows.Last().EndHour;
			}
			Window.EndHour = Window.StartHour + Number(Obj, TEXT("hours"), 0.0f);
			Config.Windows.Add(Window);
		}
	}
	if (const TSharedPtr<FJsonObject> Rain = Child(Root, TEXT("rain")))
	{
		Config.SquallRain = Number(Rain, TEXT("squall"), Config.SquallRain);
		Config.CycloneRain = Number(Rain, TEXT("cyclone"), Config.CycloneRain);
	}
	if (const TSharedPtr<FJsonObject> Charges = Child(Root, TEXT("chargesPerHour")))
	{
		Config.SquallChargesPerHour = Number(Charges, TEXT("squall"), Config.SquallChargesPerHour);
		Config.CycloneChargesPerHour = Number(Charges, TEXT("cyclone"), Config.CycloneChargesPerHour);
	}
	Config.RaftLastHour = AbsHour(Child(Root, TEXT("raftLast")), Config.RaftLastHour);
	Config.ForcedReturnHour = AbsHour(Child(Root, TEXT("forcedReturn")), Config.ForcedReturnHour);
	Config.RaftReopenHour = AbsHour(Child(Root, TEXT("raftReopen")), Config.RaftReopenHour);
	if (!Config.IsValid())
	{
		Errors.Add(TEXT("storm_curve.json: curva o ventanas no válidas"));
		return Errors;
	}
	InOut = Config;
	return Errors;
}

const FIslandData& ExploredIslandData::Get()
{
	TUniquePtr<FIslandData>& Cache = IslandDataDetail::Cache();
	if (!Cache.IsValid())
	{
		Reload();
	}
	return *Cache;
}

void ExploredIslandData::Reload()
{
	using namespace IslandDataDetail;
	TUniquePtr<FIslandData> Data = MakeUnique<FIslandData>(FIslandData::Defaults());
	const TArray<FString> Errors = FIslandData::ParseInto(*Data, ReadData(TEXT("items.json")), ReadData(TEXT("recipes.json")),
		ReadData(TEXT("verbs.json")), ReadData(TEXT("fuels.json")), ReadData(TEXT("survival_needs.json")), ReadData(TEXT("fish.json")));
	TArray<FString> AllErrors = Errors;
	AllErrors.Append(FIslandData::ParseStorm(Data->Storm, ReadData(TEXT("storm_curve.json"))));
	for (const FString& Error : AllErrors)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Isla] %s (se usan los valores por defecto)"), *Error);
	}
	Cache() = MoveTemp(Data);
}

FText ExploredIslandData::ItemDescription(FName ItemId)
{
	const TPair<FString, FString>* Texts = Get().ItemDescriptions.Find(ItemId);
	if (!Texts)
	{
		return FText::GetEmpty();
	}
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	return FText::FromString(ExploredLocalization::Pick(Texts->Key, Texts->Value, Culture));
}

FText ExploredIslandData::ItemName(FName ItemId)
{
	const TPair<FString, FString>* Names = Get().ItemNames.Find(ItemId);
	if (!Names)
	{
		return FText::FromName(ItemId);
	}
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	return FText::FromString(ExploredLocalization::Pick(Names->Key, Names->Value, Culture));
}
