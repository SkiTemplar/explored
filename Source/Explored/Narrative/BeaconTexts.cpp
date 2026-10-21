#include "Narrative/BeaconTexts.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Explored.h"
#include "Narrative/NotebookModel.h"

namespace BeaconTextsDetail
{
	using FJsonArray = TArray<TSharedPtr<FJsonValue>>;

	FString CultureName()
	{
		return FInternationalization::Get().GetCurrentCulture()->GetName();
	}

	TSharedPtr<FJsonObject> ReadObject(const FString& Directory, const TCHAR* File, TArray<FString>& OutErrors)
	{
		FString Text;
		const FString Path = FPaths::Combine(Directory, File);
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: no se puede leer"), File));
			return nullptr;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: JSON mal formado"), File));
			return nullptr;
		}
		return Root;
	}

	/** Pareja español/inglés de un registro; false si falta el español. */
	bool Pair(const TSharedPtr<FJsonObject>& Entry, const TCHAR* EsField, const TCHAR* EnField, FExploredLocalizedString& Out)
	{
		FString Es;
		FString En;
		if (!Entry->TryGetStringField(EsField, Es) || Es.IsEmpty())
		{
			return false;
		}
		Entry->TryGetStringField(EnField, En);
		Out = FExploredLocalizedString(Es, En);
		return true;
	}

	/** Recorre la lista Field del objeto y añade cada registro con id y textEs/textEn. */
	void AddList(const TSharedPtr<FJsonObject>& Root, const TCHAR* File, const TCHAR* Field, FBeaconTexts& Out, TArray<FString>& OutErrors)
	{
		const FJsonArray* Array = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(Field, Array))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: falta la lista «%s»"), File, Field));
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Id;
			FExploredLocalizedString Text;
			if (!Entry.IsValid() || !Entry->TryGetStringField(TEXT("id"), Id) || !Pair(Entry, TEXT("textEs"), TEXT("textEn"), Text))
			{
				OutErrors.Add(FString::Printf(TEXT("%s/%s: registro sin id o sin textEs"), File, Field));
				continue;
			}
			Out.Texts.Add(FName(*Id), Text);
		}
	}

	void AddPages(const TSharedPtr<FJsonObject>& Root, FBeaconTexts& Out, TArray<FString>& OutErrors)
	{
		const FJsonArray* Array = nullptr;
		if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("pages"), Array))
		{
			OutErrors.Add(TEXT("notebook.json: falta la lista «pages»"));
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
			int32 Page = 0;
			FExploredLocalizedString Text;
			FExploredLocalizedString Note;
			if (!Entry.IsValid() || !Entry->TryGetNumberField(TEXT("page"), Page) || !FNotebookModel::IsValidPage(Page)
				|| !Pair(Entry, TEXT("textEs"), TEXT("textEn"), Text) || !Pair(Entry, TEXT("iriaEs"), TEXT("iriaEn"), Note))
			{
				OutErrors.Add(TEXT("notebook.json: página sin número 1-8, sin texto o sin nota de Iria"));
				continue;
			}
			Out.Texts.Add(FNotebookModel::PageTextId(Page), Text);
			Out.Texts.Add(FNotebookModel::NoteTextId(Page), Note);
		}
	}
}

FText FBeaconTexts::Get(FName Id) const
{
	const FExploredLocalizedString* Found = Texts.Find(Id);
	return Found ? FText::FromString(Found->Get(BeaconTextsDetail::CultureName())) : FText::GetEmpty();
}

FBeaconTexts ExploredBeaconTexts::LoadFromDirectory(const FString& Directory, TArray<FString>& OutErrors)
{
	using namespace BeaconTextsDetail;
	FBeaconTexts Out;
	const TSharedPtr<FJsonObject> Notebook = ReadObject(Directory, TEXT("notebook.json"), OutErrors);
	if (Notebook.IsValid())
	{
		AddPages(Notebook, Out, OutErrors);
		AddList(Notebook, TEXT("notebook.json"), TEXT("cards"), Out, OutErrors);
		AddList(Notebook, TEXT("notebook.json"), TEXT("epilogue"), Out, OutErrors);
	}
	const TSharedPtr<FJsonObject> Objectives = ReadObject(Directory, TEXT("objectives.json"), OutErrors);
	if (Objectives.IsValid())
	{
		AddList(Objectives, TEXT("objectives.json"), TEXT("objectives"), Out, OutErrors);
	}
	const TSharedPtr<FJsonObject> Thoughts = ReadObject(Directory, TEXT("thoughts.json"), OutErrors);
	if (Thoughts.IsValid())
	{
		AddList(Thoughts, TEXT("thoughts.json"), TEXT("thoughts"), Out, OutErrors);
		double Seconds = Out.SubtitleSeconds;
		if (Thoughts->TryGetNumberField(TEXT("subtitleSeconds"), Seconds) && FMath::IsFinite(Seconds) && Seconds > 0.0)
		{
			Out.SubtitleSeconds = static_cast<float>(Seconds);
		}
	}
	return Out;
}

const FBeaconTexts& ExploredBeaconTexts::Get()
{
	static const FBeaconTexts Loaded = []()
	{
		TArray<FString> Errors;
		FBeaconTexts Texts = LoadFromDirectory(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data")), Errors);
		for (const FString& Error : Errors)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Baliza] %s"), *Error);
		}
		return Texts;
	}();
	return Loaded;
}
