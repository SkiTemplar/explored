#include "Achievements/AchievementsSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Save/SaveSystemStates.h"
#include "Subsystems/SubsystemCollection.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredSaveSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogExploredAchievements, Log, All);

namespace AchievementsJsonDetail
{
	bool ParseKind(const FString& Text, EAchievementStatKind& Out)
	{
		if (Text == TEXT("counter")) { Out = EAchievementStatKind::Counter; return true; }
		if (Text == TEXT("max")) { Out = EAchievementStatKind::Maximum; return true; }
		if (Text == TEXT("set")) { Out = EAchievementStatKind::Set; return true; }
		if (Text == TEXT("flag")) { Out = EAchievementStatKind::Flag; return true; }
		return false;
	}

	bool ParseScope(const FString& Text, EAchievementStatScope& Out)
	{
		if (Text == TEXT("profile")) { Out = EAchievementStatScope::Profile; return true; }
		if (Text == TEXT("run")) { Out = EAchievementStatScope::Run; return true; }
		return false;
	}

	bool ParseCondition(const TSharedPtr<FJsonObject>& Obj, const FString& Where, FAchievementCondition& Out, FString& OutError);

	bool ParseChildren(const TArray<TSharedPtr<FJsonValue>>& List, const FString& Where, TArray<FAchievementCondition>& Out, FString& OutError)
	{
		for (const TSharedPtr<FJsonValue>& Entry : List)
		{
			FAchievementCondition Child;
			if (!Entry.IsValid() || !ParseCondition(Entry->AsObject(), Where, Child, OutError))
			{
				if (OutError.IsEmpty())
				{
					OutError = FString::Printf(TEXT("%s: condición hija vacía"), *Where);
				}
				return false;
			}
			Out.Add(MoveTemp(Child));
		}
		return true;
	}

	bool ParseCondition(const TSharedPtr<FJsonObject>& Obj, const FString& Where, FAchievementCondition& Out, FString& OutError)
	{
		if (!Obj.IsValid())
		{
			OutError = FString::Printf(TEXT("%s: la condición debe ser un objeto"), *Where);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Obj->TryGetArrayField(TEXT("all"), List) && List)
		{
			Out.Kind = EAchievementConditionKind::All;
			return ParseChildren(*List, Where, Out.Children, OutError);
		}
		if (Obj->TryGetArrayField(TEXT("any"), List) && List)
		{
			Out.Kind = EAchievementConditionKind::Any;
			return ParseChildren(*List, Where, Out.Children, OutError);
		}

		const TSharedPtr<FJsonObject>* NotObject = nullptr;
		if (Obj->TryGetObjectField(TEXT("not"), NotObject) && NotObject)
		{
			FAchievementCondition Child;
			if (!ParseCondition(*NotObject, Where, Child, OutError))
			{
				return false;
			}
			Out = FAchievementCondition::Not(MoveTemp(Child));
			return true;
		}

		FString FlagName;
		if (Obj->TryGetStringField(TEXT("flag"), FlagName))
		{
			Out = FAchievementCondition::Flag(FName(*FlagName));
			return true;
		}

		FString StatName;
		if (!Obj->TryGetStringField(TEXT("stat"), StatName) || StatName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s: condición sin «stat», «flag», «all», «any» ni «not»"), *Where);
			return false;
		}

		FString Item;
		if (Obj->TryGetStringField(TEXT("contains"), Item))
		{
			Out = FAchievementCondition::Contains(FName(*StatName), FName(*Item));
			return true;
		}

		FString OpText;
		double Value = 0.0;
		EAchievementCompareOp Op = EAchievementCompareOp::GreaterEqual;
		if (!Obj->TryGetStringField(TEXT("op"), OpText) || !Obj->TryGetNumberField(TEXT("value"), Value))
		{
			OutError = FString::Printf(TEXT("%s: la comparación de «%s» necesita «op» y «value»"), *Where, *StatName);
			return false;
		}
		if (!ParseAchievementCompareOp(OpText, Op))
		{
			OutError = FString::Printf(TEXT("%s: operador «%s» desconocido"), *Where, *OpText);
			return false;
		}
		Out = FAchievementCondition::Compare(FName(*StatName), Op, Value);
		return true;
	}
}

namespace AchievementsSaveDetail
{
	const TCHAR* const Section = TEXT("achievements");
}

void UAchievementsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadFromDisk();

	// Sección «achievements» de la partida (P-WIRE).
	if (UExploredSaveSubsystem* Save = Collection.InitializeDependency<UExploredSaveSubsystem>())
	{
		TWeakObjectPtr<UAchievementsSubsystem> WeakThis(this);
		Save->RegisterSection(AchievementsSaveDetail::Section,
			[WeakThis](FSaveArchive& Ar)
			{
				if (const UAchievementsSubsystem* Self = WeakThis.Get())
				{
					ExploredSaveStates::SaveAchievements(Ar, Self->Model.GetState());
				}
			},
			[WeakThis](const FSaveArchive& Ar)
			{
				UAchievementsSubsystem* Self = WeakThis.Get();
				if (!Self)
				{
					return;
				}
				FAchievementsState Loaded;
				ExploredSaveStates::LoadAchievements(Ar, Loaded);
				Self->Model.RestoreState(ExploredSaveStates::MergeLoadedAchievements(Self->Model.GetState(), Loaded));
				Self->EvaluatePending();
			});
	}
}

void UAchievementsSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UExploredSaveSubsystem* Save = GameInstance->GetSubsystem<UExploredSaveSubsystem>())
		{
			Save->UnregisterSection(AchievementsSaveDetail::Section);
		}
	}
	Super::Deinitialize();
}

UAchievementsSubsystem* UAchievementsSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = (GEngine && WorldContextObject)
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAchievementsSubsystem>() : nullptr;
}

bool UAchievementsSubsystem::ReloadFromDisk()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("achievements.json");
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		UE_LOG(LogExploredAchievements, Error, TEXT("No se pudo leer %s"), *Path);
		return false;
	}

	TArray<FAchievementStatDef> Stats;
	TArray<FAchievementDef> Achievements;
	FString Error;
	if (!ParseAchievementsJson(JsonText, Stats, Achievements, Error) || !Model.Configure(Stats, Achievements, Error))
	{
		UE_LOG(LogExploredAchievements, Error, TEXT("achievements.json no es válido: %s"), *Error);
		return false;
	}
	// El acceso anticipado publica solo los logros «AA» (biblia 07 §2). Al abrir la fase 2,
	// subir a Phase2: los ya cumplidos se desbloquean en el siguiente EvaluatePending.
	Model.SetReleasedPhase(EAchievementPhase::EarlyAccess);
	UE_LOG(LogExploredAchievements, Log, TEXT("Cargados %d logros y %d estadísticas"), Achievements.Num(), Stats.Num());
	return true;
}

bool UAchievementsSubsystem::ParseAchievementsJson(const FString& JsonText, TArray<FAchievementStatDef>& OutStats,
	TArray<FAchievementDef>& OutAchievements, FString& OutError)
{
	OutStats.Reset();
	OutAchievements.Reset();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON mal formado");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* StatsArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* AchievementsArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("stats"), StatsArray) || !StatsArray
		|| !Root->TryGetArrayField(TEXT("achievements"), AchievementsArray) || !AchievementsArray)
	{
		OutError = TEXT("Faltan las listas «stats» o «achievements»");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Entry : *StatsArray)
	{
		const TSharedPtr<FJsonObject> Obj = Entry.IsValid() ? Entry->AsObject() : TSharedPtr<FJsonObject>();
		FString Id, Kind, Scope;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || !Obj->TryGetStringField(TEXT("kind"), Kind)
			|| !Obj->TryGetStringField(TEXT("scope"), Scope))
		{
			OutError = TEXT("Estadística sin id, kind o scope");
			return false;
		}
		FAchievementStatDef Def;
		Def.Id = FName(*Id);
		if (!AchievementsJsonDetail::ParseKind(Kind, Def.Kind) || !AchievementsJsonDetail::ParseScope(Scope, Def.Scope))
		{
			OutError = FString::Printf(TEXT("Estadística «%s»: kind «%s» o scope «%s» desconocido"), *Id, *Kind, *Scope);
			return false;
		}
		OutStats.Add(Def);
	}

	for (const TSharedPtr<FJsonValue>& Entry : *AchievementsArray)
	{
		const TSharedPtr<FJsonObject> Obj = Entry.IsValid() ? Entry->AsObject() : TSharedPtr<FJsonObject>();
		FString Id;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty())
		{
			OutError = TEXT("Logro sin id");
			return false;
		}
		FAchievementDef Def;
		Def.Id = FName(*Id);
		Obj->TryGetStringField(TEXT("nameEs"), Def.NameEs);
		Obj->TryGetStringField(TEXT("nameEn"), Def.NameEn);
		Obj->TryGetStringField(TEXT("descriptionEs"), Def.DescriptionEs);
		Obj->TryGetStringField(TEXT("descriptionEn"), Def.DescriptionEn);
		Obj->TryGetBoolField(TEXT("hidden"), Def.bHidden);
		FString Icon;
		Obj->TryGetStringField(TEXT("icon"), Icon);
		Def.Icon = FName(*Icon);

		TArray<FString> Modes;
		if (Obj->TryGetStringArrayField(TEXT("modes"), Modes))
		{
			for (const FString& Mode : Modes)
			{
				Def.Modes.Add(FName(*Mode));
			}
		}

		// Fase, rareza y alcance cooperativo (biblia 07 §2, biblia 08 §5.7). Si faltan valen AA,
		// común y actor; si están mal escritos, el fichero entero se rechaza.
		FString Phase, Rarity, CoopScope;
		if (Obj->TryGetStringField(TEXT("phase"), Phase) && !ParseAchievementPhase(Phase, Def.Phase))
		{
			OutError = FString::Printf(TEXT("Logro «%s»: phase «%s» desconocida"), *Id, *Phase);
			return false;
		}
		if (Obj->TryGetStringField(TEXT("rarity"), Rarity) && !ParseAchievementRarity(Rarity, Def.Rarity))
		{
			OutError = FString::Printf(TEXT("Logro «%s»: rarity «%s» desconocida"), *Id, *Rarity);
			return false;
		}
		if (Obj->TryGetStringField(TEXT("coopScope"), CoopScope) && !ParseAchievementCoopScope(CoopScope, Def.CoopScope))
		{
			OutError = FString::Printf(TEXT("Logro «%s»: coopScope «%s» desconocido"), *Id, *CoopScope);
			return false;
		}

		const TSharedPtr<FJsonObject>* ConditionObject = nullptr;
		if (!Obj->TryGetObjectField(TEXT("condition"), ConditionObject) || !ConditionObject)
		{
			OutError = FString::Printf(TEXT("Logro «%s» sin condición"), *Id);
			return false;
		}
		if (!AchievementsJsonDetail::ParseCondition(*ConditionObject, Id, Def.Condition, OutError))
		{
			return false;
		}
		OutAchievements.Add(MoveTemp(Def));
	}

	OutError.Reset();
	return true;
}

void UAchievementsSubsystem::ReportStat(FName Stat, double Amount)
{
	if (!Model.IsKnownStat(Stat))
	{
		WarnUnknownStat(Stat);
		return;
	}
	Announce(Model.Report(Stat, Amount));
}

void UAchievementsSubsystem::ReportStatItem(FName Stat, FName Item)
{
	if (!Model.IsKnownStat(Stat))
	{
		WarnUnknownStat(Stat);
		return;
	}
	Announce(Model.ReportItem(Stat, Item));
}

void UAchievementsSubsystem::BeginRun(EExploredGameplayMode Mode)
{
	Model.BeginRun(ModeId(Mode));
	OnRunStarted.Broadcast(Mode);
}

void UAchievementsSubsystem::EvaluatePending()
{
	Announce(Model.Evaluate());
}

bool UAchievementsSubsystem::IsUnlocked(FName AchievementId) const
{
	return Model.IsUnlocked(AchievementId);
}

float UAchievementsSubsystem::GetProgress(FName AchievementId) const
{
	return Model.GetProgress(AchievementId);
}

bool UAchievementsSubsystem::IsHidden(FName AchievementId) const
{
	const FAchievementDef* Def = Model.FindAchievement(AchievementId);
	return Def && Def->bHidden;
}

FText UAchievementsSubsystem::GetDisplayName(FName AchievementId) const
{
	const FAchievementDef* Def = Model.FindAchievement(AchievementId);
	if (!Def)
	{
		return FText::FromName(AchievementId);
	}
	const FString& Name = (UseEnglish() && !Def->NameEn.IsEmpty()) ? Def->NameEn : Def->NameEs;
	return FText::FromString(Name);
}

FText UAchievementsSubsystem::GetDescription(FName AchievementId) const
{
	const FAchievementDef* Def = Model.FindAchievement(AchievementId);
	if (!Def)
	{
		return FText::GetEmpty();
	}
	const FString& Description = (UseEnglish() && !Def->DescriptionEn.IsEmpty()) ? Def->DescriptionEn : Def->DescriptionEs;
	return FText::FromString(Description);
}

TArray<FName> UAchievementsSubsystem::GetAchievementIds() const
{
	TArray<FName> Ids;
	for (const FAchievementDef& Def : Model.GetAchievements())
	{
		Ids.Add(Def.Id);
	}
	return Ids;
}

FName UAchievementsSubsystem::ModeId(EExploredGameplayMode Mode)
{
	switch (Mode)
	{
	case EExploredGameplayMode::Explorer: return FName(TEXT("Explorer"));
	case EExploredGameplayMode::Survivor: return FName(TEXT("Survivor"));
	case EExploredGameplayMode::Castaway: return FName(TEXT("Castaway"));
	default: return NAME_None;
	}
}

void UAchievementsSubsystem::NotifyPlatformAchievementUnlocked(FName AchievementId)
{
	// Sin plataforma (GDD §20). Aquí irá SteamUserStats()->SetAchievement + StoreStats.
}

void UAchievementsSubsystem::Announce(const TArray<FName>& Unlocked)
{
	for (const FName& Id : Unlocked)
	{
		UE_LOG(LogExploredAchievements, Log, TEXT("Logro desbloqueado: %s"), *Id.ToString());
		NotifyPlatformAchievementUnlocked(Id);
		OnAchievementUnlocked.Broadcast(Id);
	}
}

void UAchievementsSubsystem::WarnUnknownStat(FName Stat)
{
	if (!WarnedUnknownStats.Contains(Stat))
	{
		WarnedUnknownStats.Add(Stat);
		UE_LOG(LogExploredAchievements, Warning,
			TEXT("Estadística desconocida «%s»: añádela a docs/tecnico/estadisticas.md y a achievements.json"), *Stat.ToString());
	}
}

bool UAchievementsSubsystem::UseEnglish() const
{
	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	return Settings && Settings->GetLanguage() == EExploredLanguage::English;
}
