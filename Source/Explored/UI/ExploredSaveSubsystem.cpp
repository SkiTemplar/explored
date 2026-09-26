#include "UI/ExploredSaveSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString UExploredSaveSubsystem::GetSaveDirectory() const
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames");
}

bool UExploredSaveSubsystem::HasSaveGame() const
{
	return GetSlotNames().Num() > 0;
}

TArray<FString> UExploredSaveSubsystem::GetSlotNames() const
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(GetSaveDirectory() / TEXT("*.sav")), true, false);
	for (FString& File : Files)
	{
		File = FPaths::GetBaseFilename(File);
	}
	return Files;
}

void UExploredSaveSubsystem::RequestSave(FName Slot)
{
	// Stub: escribe un marcador vacío para que HasSaveGame()/GetSlotNames()
	// reflejen que existe una partida. El equipo de guardado sustituye esto
	// por la escritura real de un USaveGame (semilla + deltas + jugador).
	const FString Directory = GetSaveDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString FilePath = Directory / (Slot.ToString() + TEXT(".sav"));
	FFileHelper::SaveStringToFile(TEXT("ExploredSaveStub"), *FilePath);

	OnSaveCompleted.Broadcast();
}
