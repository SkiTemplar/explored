#include "UI/ExploredSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/SubsystemCollection.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace ExploredSaveDisk
{
	/** ISaveFileSystem sobre el disco: UTF-8 con BOM al escribir, detección automática al leer. */
	class FDiskFileSystem final : public ISaveFileSystem
	{
	public:
		virtual bool FileExists(const FString& Path) const override
		{
			return IFileManager::Get().FileExists(*Path);
		}

		virtual bool ReadText(const FString& Path, FString& OutText) const override
		{
			return IFileManager::Get().FileExists(*Path) && FFileHelper::LoadFileToString(OutText, *Path);
		}

		virtual bool WriteText(const FString& Path, const FString& Text) override
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
		}

		virtual bool MoveReplace(const FString& From, const FString& To) override
		{
			return IFileManager::Get().Move(*To, *From, /*Replace*/ true, /*EvenIfReadOnly*/ true);
		}

		virtual bool Delete(const FString& Path) override
		{
			return IFileManager::Get().Delete(*Path, /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
		}
	};

	const TCHAR* const PlayerSection = TEXT("player");
}

void UExploredSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	WorldSeed = static_cast<int64>(FArchipelagoLayout::OfficialSeed);
	SessionStartSeconds = FPlatformTime::Seconds();
	RegisterPlayerSection();
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UExploredSaveSubsystem::HandlePostLoadMap);
}

void UExploredSaveSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();
	Super::Deinitialize();
}

FString UExploredSaveSubsystem::GetSaveDirectory() const
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames");
}

FString UExploredSaveSubsystem::GetGameVersion() const
{
	FString Version = TEXT("0.0.0");
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	}
	return Version;
}

double UExploredSaveSubsystem::GetPlayTimeSeconds() const
{
	return PlayTimeAtLoad + FMath::Max(0.0, FPlatformTime::Seconds() - SessionStartSeconds);
}

bool UExploredSaveSubsystem::HasSaveGame() const
{
	return !GetContinueSlotId().IsEmpty();
}

TArray<FString> UExploredSaveSubsystem::GetSlotNames() const
{
	TArray<FString> Names;
	for (const FSaveSlotInfo& Info : ListSlots())
	{
		if (Info.IsLoadable())
		{
			Names.Add(Info.SlotId);
		}
	}
	return Names;
}

TArray<FSaveSlotInfo> UExploredSaveSubsystem::ListSlots() const
{
	ExploredSaveDisk::FDiskFileSystem Disk;
	const FSaveSlotStore Store(Disk, GetSaveDirectory(), Migrations);
	return Store.List();
}

TArray<FString> UExploredSaveSubsystem::GetSlotsWithBackup() const
{
	TArray<FString> Out;
	const FString Directory = GetSaveDirectory();
	for (const FString& SlotId : FSaveSlotPolicy::AllSlotIds())
	{
		if (IFileManager::Get().FileExists(*FSaveSlotPolicy::JoinPath(Directory, FSaveSlotPolicy::BackupFileName(SlotId))))
		{
			Out.Add(SlotId);
		}
	}
	return Out;
}

FString UExploredSaveSubsystem::GetContinueSlotId() const
{
	ExploredSaveDisk::FDiskFileSystem Disk;
	const FSaveSlotStore Store(Disk, GetSaveDirectory(), Migrations);
	return Store.FindContinueSlot();
}

void UExploredSaveSubsystem::RequestSave(FName Slot)
{
	const FString SlotId = FSaveSlotPolicy::SlotForTrigger(ESaveTrigger::Manual, Slot.ToString());
	FString Error;
	if (SaveToSlot(SlotId, Error))
	{
		OnSaveCompleted.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] No se pudo guardar en %s: %s"), *SlotId, *Error);
		OnSaveFailed.Broadcast(Error);
	}
}

void UExploredSaveSubsystem::RequestAutosave(ESaveTrigger Trigger)
{
	const double Now = FPlatformTime::Seconds();
	const double SinceLast = LastAutosaveSeconds < 0.0 ? -1.0 : Now - LastAutosaveSeconds;
	if (!FSaveSlotPolicy::ShouldAutosave(Trigger, SinceLast))
	{
		return;
	}
	FString Error;
	if (SaveToSlot(FSaveSlotPolicy::SlotForTrigger(Trigger, FString()), Error))
	{
		LastAutosaveSeconds = Now;
		OnSaveCompleted.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Autoguardado fallido: %s"), *Error);
		OnSaveFailed.Broadcast(Error);
	}
}

bool UExploredSaveSubsystem::SaveToSlot(const FString& SlotId, FString& OutError)
{
	FSaveDocument Document;
	Document.Header.GameVersion = GetGameVersion();
	Document.Header.Seed = WorldSeed;
	Document.Header.PlayTimeSeconds = GetPlayTimeSeconds();
	Document.Header.TimestampUnix = FDateTime::UtcNow().ToUnixTimestamp();
	Document.Sections = Sections.Capture();

	ExploredSaveDisk::FDiskFileSystem Disk;
	FSaveSlotStore Store(Disk, GetSaveDirectory(), Migrations);
	return Store.Write(SlotId, Document, OutError);
}

ESaveLoadResult UExploredSaveSubsystem::LoadFromSlot(const FString& SlotId, FString& OutError)
{
	ExploredSaveDisk::FDiskFileSystem Disk;
	const FSaveSlotStore Store(Disk, GetSaveDirectory(), Migrations);
	FSaveReadOutcome Outcome = Store.Read(SlotId);
	if (Outcome.Result != ESaveLoadResult::Ok)
	{
		OutError = Outcome.Error;
		UE_LOG(LogTemp, Warning, TEXT("[Explored] No se pudo cargar %s (%s): %s"), *SlotId, LexToString(Outcome.Result), *OutError);
		return Outcome.Result;
	}
	if (Outcome.bFromBackup)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] La partida %s estaba dañada: se carga su copia de seguridad"), *SlotId);
	}

	WorldSeed = Outcome.Document.Header.Seed;
	PlayTimeAtLoad = Outcome.Document.Header.PlayTimeSeconds;
	SessionStartSeconds = FPlatformTime::Seconds();
	Sections.Apply(Outcome.Document.Sections);
	OnLoadCompleted.Broadcast(SlotId);
	OutError.Reset();
	return ESaveLoadResult::Ok;
}

bool UExploredSaveSubsystem::LoadContinueGame()
{
	const FString SlotId = GetContinueSlotId();
	if (SlotId.IsEmpty())
	{
		return false;
	}
	FString Error;
	return LoadFromSlot(SlotId, Error) == ESaveLoadResult::Ok;
}

bool UExploredSaveSubsystem::RegisterSection(const FString& Name, FSaveSectionRegistry::FSaveFunc Save, FSaveSectionRegistry::FLoadFunc Load)
{
	return Sections.Register(Name, MoveTemp(Save), MoveTemp(Load));
}

bool UExploredSaveSubsystem::UnregisterSection(const FString& Name)
{
	return Sections.Unregister(Name);
}

// ---------------------------------------------------------------------------
// Sección «player»
// ---------------------------------------------------------------------------

void UExploredSaveSubsystem::RegisterPlayerSection()
{
	TWeakObjectPtr<UExploredSaveSubsystem> WeakThis(this);
	Sections.Register(ExploredSaveDisk::PlayerSection,
		[WeakThis](FSaveArchive& Ar)
		{
			if (UExploredSaveSubsystem* Self = WeakThis.Get())
			{
				Self->SavePlayer(Ar);
			}
		},
		[WeakThis](const FSaveArchive& Ar)
		{
			if (UExploredSaveSubsystem* Self = WeakThis.Get())
			{
				Self->LoadPlayer(Ar);
			}
		});
}

void UExploredSaveSubsystem::SavePlayer(FSaveArchive& Ar)
{
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	// Si el personaje aún no existe (o hay una carga pendiente), se guarda el último estado conocido.
	if (Pawn && !bPlayerPending)
	{
		PlayerState.Location = Pawn->GetActorLocation();
		PlayerState.ControlRotation = Controller->GetControlRotation();
		PlayerState.Velocity = Pawn->GetVelocity();
	}
	PlayerState.Save(Ar);
}

void UExploredSaveSubsystem::LoadPlayer(const FSaveArchive& Ar)
{
	PlayerState.Load(Ar);
	// Sin sección (partida sin jugador): se deja al personaje donde lo puso el mapa.
	bPlayerPending = !Ar.IsEmpty();
	ApplyPendingPlayer();
}

void UExploredSaveSubsystem::ApplyPendingPlayer()
{
	if (!bPlayerPending)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return;
	}
	Pawn->SetActorLocation(PlayerState.Location, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	Controller->SetControlRotation(PlayerState.ControlRotation);
	bPlayerPending = false;

	// El punto guardado puede caer en cualquier lugar de los 6x6 km del archipiélago: con World
	// Partition, el terreno y la vegetación de esa zona pueden tardar unos fotogramas en
	// transmitirse tras el teleport de arriba (SM_Terrain_DeepFloor, siempre cargado, evita que se
	// caiga al vacío mientras tanto, pero se vería el suelo grueso de repuesto en vez del terreno
	// real). Se oculta al personaje hasta que el streaming de su celda termine, con un margen de
	// seguridad por si nunca converge (misma idea que UExploredShotSubsystem::Tick).
	Pawn->SetActorHiddenInGame(true);
	World->GetTimerManager().ClearTimer(StreamingRevealHandle);
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<UExploredSaveSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(StreamingRevealHandle, FTimerDelegate::CreateLambda([WeakThis, WeakPawn, WeakWorld, Deadline]()
	{
		UExploredSaveSubsystem* Self = WeakThis.Get();
		UWorld* TimerWorld = WeakWorld.Get();
		APawn* TimerPawn = WeakPawn.Get();
		if (!Self || !TimerWorld || !TimerPawn)
		{
			return;
		}
		const UWorldPartitionSubsystem* Partition = TimerWorld->GetSubsystem<UWorldPartitionSubsystem>();
		const bool bReady = !Partition || Partition->IsStreamingCompleted();
		if (bReady || FPlatformTime::Seconds() >= Deadline)
		{
			TimerPawn->SetActorHiddenInGame(false);
			TimerWorld->GetTimerManager().ClearTimer(Self->StreamingRevealHandle);
		}
	}), 0.1f, true);
}

void UExploredSaveSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		ApplyPendingPlayer();
	}
}
