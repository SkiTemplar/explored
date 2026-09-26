#include "Save/SaveSlots.h"

namespace SaveSlotsDetail
{
	/** Posición fija de la ranura (auto, manual1…) para desempatar de forma estable. */
	int32 SlotOrder(const FString& SlotId)
	{
		const TArray<FString> All = FSaveSlotPolicy::AllSlotIds();
		for (int32 I = 0; I < All.Num(); ++I)
		{
			if (FSaveValue::CompareKeys(All[I], SlotId) == 0)
			{
				return I;
			}
		}
		return All.Num();
	}

	FString LowerAscii(const FString& Text)
	{
		FString Out;
		const TCHAR* P = *Text;
		for (int32 I = 0; I < Text.Len(); ++I)
		{
			TCHAR C = P[I];
			if (C >= TEXT('A') && C <= TEXT('Z'))
			{
				C = static_cast<TCHAR>(C - TEXT('A') + TEXT('a'));
			}
			Out.AppendChar(C);
		}
		return Out;
	}
}

FString FSaveSlotPolicy::AutoSlotId()
{
	return TEXT("auto");
}

FString FSaveSlotPolicy::ManualSlotId(int32 Index)
{
	return FString::Printf(TEXT("manual%d"), Index);
}

TArray<FString> FSaveSlotPolicy::AllSlotIds()
{
	TArray<FString> Out;
	Out.Add(AutoSlotId());
	for (int32 I = 1; I <= NumManualSlots; ++I)
	{
		Out.Add(ManualSlotId(I));
	}
	return Out;
}

bool FSaveSlotPolicy::IsValidSlotId(const FString& SlotId)
{
	return SaveSlotsDetail::SlotOrder(SlotId) < NumManualSlots + 1;
}

FString FSaveSlotPolicy::NormalizeSlotId(const FString& Requested)
{
	const FString Lower = SaveSlotsDetail::LowerAscii(Requested.TrimStartAndEnd());
	if (IsValidSlotId(Lower))
	{
		return Lower;
	}
	for (int32 I = 1; I <= NumManualSlots; ++I)
	{
		if (FSaveValue::CompareKeys(Lower, FString::Printf(TEXT("%d"), I)) == 0)
		{
			return ManualSlotId(I);
		}
	}
	return FString();
}

bool FSaveSlotPolicy::ShouldAutosave(ESaveTrigger Trigger, double SecondsSinceLastAutosave)
{
	switch (Trigger)
	{
	case ESaveTrigger::Manual:
	case ESaveTrigger::Sleep:
	case ESaveTrigger::Quit:
		return true;
	case ESaveTrigger::Campfire:
		return SecondsSinceLastAutosave < 0.0 || SecondsSinceLastAutosave >= MinSecondsBetweenCampfireSaves;
	}
	return false;
}

FString FSaveSlotPolicy::SlotForTrigger(ESaveTrigger Trigger, const FString& ManualSlotIdIn)
{
	if (Trigger == ESaveTrigger::Manual)
	{
		const FString Normalized = NormalizeSlotId(ManualSlotIdIn);
		return Normalized.IsEmpty() ? AutoSlotId() : Normalized;
	}
	return AutoSlotId();
}

FString FSaveSlotPolicy::JoinPath(const FString& Directory, const FString& FileName)
{
	if (Directory.IsEmpty())
	{
		return FileName;
	}
	if (Directory.EndsWith(TEXT("/")) || Directory.EndsWith(TEXT("\\")))
	{
		return Directory + FileName;
	}
	return Directory + TEXT("/") + FileName;
}

TArray<FSaveFileOp> FSaveSlotPolicy::PlanWrite(const FString& Directory, const FString& SlotId, const FString& Text, bool bMainExists)
{
	const FString Main = JoinPath(Directory, MainFileName(SlotId));
	const FString Backup = JoinPath(Directory, BackupFileName(SlotId));
	const FString Temp = JoinPath(Directory, TempFileName(SlotId));

	TArray<FSaveFileOp> Ops;
	FSaveFileOp& WriteTemp = Ops.AddDefaulted_GetRef();
	WriteTemp.Kind = FSaveFileOp::EKind::Write;
	WriteTemp.Path = Temp;
	WriteTemp.Text = Text;
	if (bMainExists)
	{
		FSaveFileOp& Rotate = Ops.AddDefaulted_GetRef();
		Rotate.Kind = FSaveFileOp::EKind::MoveReplace;
		Rotate.From = Main;
		Rotate.Path = Backup;
	}
	FSaveFileOp& Commit = Ops.AddDefaulted_GetRef();
	Commit.Kind = FSaveFileOp::EKind::MoveReplace;
	Commit.From = Temp;
	Commit.Path = Main;
	return Ops;
}

FSaveReadOutcome FSaveSlotPolicy::ReadSlot(const FString* MainText, const FString* BackupText,
	const FSaveMigrations& Migrations, int32 CurrentVersion)
{
	FSaveReadOutcome Main;
	if (MainText)
	{
		Main.Result = FSaveCodec::Decode(*MainText, Migrations, CurrentVersion, Main.Document, Main.Error);
		// Una partida futura no está dañada: no se sustituye en silencio por la copia antigua.
		if (Main.Result == ESaveLoadResult::Ok || Main.Result == ESaveLoadResult::FutureVersion)
		{
			return Main;
		}
	}
	if (BackupText)
	{
		FSaveReadOutcome Backup;
		Backup.bFromBackup = true;
		Backup.Result = FSaveCodec::Decode(*BackupText, Migrations, CurrentVersion, Backup.Document, Backup.Error);
		if (Backup.Result == ESaveLoadResult::Ok || !MainText)
		{
			return Backup;
		}
	}
	return Main;
}

void FSaveSlotPolicy::SortForContinue(TArray<FSaveSlotInfo>& Slots)
{
	Slots.StableSort([](const FSaveSlotInfo& A, const FSaveSlotInfo& B)
	{
		if (A.IsLoadable() != B.IsLoadable())
		{
			return A.IsLoadable();
		}
		if (A.Header.TimestampUnix != B.Header.TimestampUnix)
		{
			return A.Header.TimestampUnix > B.Header.TimestampUnix;
		}
		if (A.Header.PlayTimeSeconds != B.Header.PlayTimeSeconds)
		{
			return A.Header.PlayTimeSeconds > B.Header.PlayTimeSeconds;
		}
		return SaveSlotsDetail::SlotOrder(A.SlotId) < SaveSlotsDetail::SlotOrder(B.SlotId);
	});
}

// ---------------------------------------------------------------------------
// FSaveSlotStore
// ---------------------------------------------------------------------------

FSaveSlotStore::FSaveSlotStore(ISaveFileSystem& InFileSystem, const FString& InDirectory, const FSaveMigrations& InMigrations,
	int32 InCurrentVersion)
	: FileSystem(InFileSystem)
	, Directory(InDirectory)
	, Migrations(InMigrations)
	, CurrentVersion(InCurrentVersion)
{
}

bool FSaveSlotStore::Write(const FString& SlotId, const FSaveDocument& Document, FString& OutError)
{
	if (!FSaveSlotPolicy::IsValidSlotId(SlotId))
	{
		OutError = FString::Printf(TEXT("Ranura desconocida: %s"), *SlotId);
		return false;
	}
	FSaveDocument ToWrite = Document;
	ToWrite.Header.SlotId = SlotId;
	ToWrite.Header.FormatVersion = CurrentVersion;
	const FString Text = FSaveCodec::Encode(ToWrite);

	const FString Main = FSaveSlotPolicy::JoinPath(Directory, FSaveSlotPolicy::MainFileName(SlotId));
	const TArray<FSaveFileOp> Plan = FSaveSlotPolicy::PlanWrite(Directory, SlotId, Text, FileSystem.FileExists(Main));
	for (const FSaveFileOp& Op : Plan)
	{
		const bool bOk = Op.Kind == FSaveFileOp::EKind::Write
			? FileSystem.WriteText(Op.Path, Op.Text)
			: FileSystem.MoveReplace(Op.From, Op.Path);
		if (!bOk)
		{
			OutError = FString::Printf(TEXT("No se pudo escribir %s"), *Op.Path);
			return false;
		}
	}
	OutError.Reset();
	return true;
}

FSaveReadOutcome FSaveSlotStore::Read(const FString& SlotId) const
{
	FString MainText;
	FString BackupText;
	const bool bMain = FileSystem.ReadText(FSaveSlotPolicy::JoinPath(Directory, FSaveSlotPolicy::MainFileName(SlotId)), MainText);
	const bool bBackup = FileSystem.ReadText(FSaveSlotPolicy::JoinPath(Directory, FSaveSlotPolicy::BackupFileName(SlotId)), BackupText);
	return FSaveSlotPolicy::ReadSlot(bMain ? &MainText : nullptr, bBackup ? &BackupText : nullptr, Migrations, CurrentVersion);
}

TArray<FSaveSlotInfo> FSaveSlotStore::List() const
{
	TArray<FSaveSlotInfo> Out;
	for (const FString& SlotId : FSaveSlotPolicy::AllSlotIds())
	{
		const FSaveReadOutcome Outcome = Read(SlotId);
		if (Outcome.Result == ESaveLoadResult::NotFound)
		{
			continue;
		}
		FSaveSlotInfo& Info = Out.AddDefaulted_GetRef();
		Info.SlotId = SlotId;
		Info.Result = Outcome.Result;
		Info.bFromBackup = Outcome.bFromBackup;
		Info.Header = Outcome.Document.Header;
	}
	FSaveSlotPolicy::SortForContinue(Out);
	return Out;
}

FString FSaveSlotStore::FindContinueSlot() const
{
	const TArray<FSaveSlotInfo> Slots = List();
	return Slots.Num() > 0 && Slots[0].IsLoadable() ? Slots[0].SlotId : FString();
}

bool FSaveSlotStore::DeleteSlot(const FString& SlotId)
{
	bool bAny = false;
	const TArray<FString> Files = { FSaveSlotPolicy::MainFileName(SlotId), FSaveSlotPolicy::BackupFileName(SlotId), FSaveSlotPolicy::TempFileName(SlotId) };
	for (const FString& File : Files)
	{
		const FString Path = FSaveSlotPolicy::JoinPath(Directory, File);
		if (FileSystem.FileExists(Path))
		{
			bAny |= FileSystem.Delete(Path);
		}
	}
	return bAny;
}
