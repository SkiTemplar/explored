#include "Audio/ExploredMusicSubsystem.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

#include "Audio/ExploredAmbienceSubsystem.h"
#include "Explored.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredPlayerController.h"
#include "Weather/ExploredWeatherSubsystem.h"

namespace ExploredMusicDetail
{
	/** Semilla fija: la banda sonora de una partida es reproducible. */
	constexpr uint32 Seed = 0xBA112A7u;
	/** Frecuencia con la que se recoge el contexto (s). */
	constexpr float ContextInterval = 0.25f;
	/** Fundidos (s) al atenuar la música para un sonido que manda y al devolverla. */
	constexpr float DuckInSeconds = 0.4f;
	constexpr float DuckOutSeconds = 1.5f;

	FString SoundtrackPath()
	{
		return FPaths::ProjectContentDir() / TEXT("Data") / TEXT("soundtrack.json");
	}

	bool ReadStateConfig(const TSharedPtr<FJsonObject>& Obj, FSoundtrackCatalog& Catalog, FSoundtrackStateConfig& Out, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(TEXT("pieces"), Pieces) || !Pieces)
		{
			OutError = TEXT("estado sin \"pieces\"");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Pieces)
		{
			const int32 Index = Value.IsValid() ? Catalog.FindById(FName(*Value->AsString())) : INDEX_NONE;
			if (Index == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("pieza desconocida %s"), Value.IsValid() ? *Value->AsString() : TEXT("?"));
				return false;
			}
			Out.Pieces.Add(Index);
		}
		double Number = 0.0;
		Out.Gain = Obj->TryGetNumberField(TEXT("gain"), Number) ? static_cast<float>(Number) : Out.Gain;
		Out.FadeIn = Obj->TryGetNumberField(TEXT("fadeIn"), Number) ? static_cast<float>(Number) : Out.FadeIn;
		Out.FadeOut = Obj->TryGetNumberField(TEXT("fadeOut"), Number) ? static_cast<float>(Number) : Out.FadeOut;
		const TArray<TSharedPtr<FJsonValue>>* Rest = nullptr;
		if (Obj->TryGetArrayField(TEXT("rest"), Rest) && Rest && Rest->Num() == 2)
		{
			Out.RestMin = static_cast<float>((*Rest)[0]->AsNumber());
			Out.RestMax = static_cast<float>((*Rest)[1]->AsNumber());
		}
		Obj->TryGetBoolField(TEXT("loop"), Out.bLoop);
		return true;
	}
}

bool UExploredMusicSubsystem::ParseSoundtrack(const FString& Json, FSoundtrackCatalog& OutCatalog, FString& OutError)
{
	OutCatalog = FSoundtrackCatalog();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON mal formado");
		return false;
	}
	FString AssetRoot = TEXT("/Game/Audio/Musica");
	Root->TryGetStringField(TEXT("assetRoot"), AssetRoot);
	const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
	if (!Root->TryGetArrayField(TEXT("pieces"), Pieces) || !Pieces)
	{
		OutError = TEXT("falta \"pieces\"");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Pieces)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		double Seconds = 0.0;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || !Obj->TryGetNumberField(TEXT("seconds"), Seconds))
		{
			OutError = TEXT("pieza sin \"id\" o sin \"seconds\"");
			return false;
		}
		FSoundtrackPiece Piece;
		Piece.Id = FName(*Id);
		Piece.Asset = FString::Printf(TEXT("%s/%s.%s"), *AssetRoot, *Id, *Id);
		Piece.Seconds = static_cast<float>(Seconds);
		OutCatalog.Pieces.Add(Piece);
	}
	const TSharedPtr<FJsonObject>* States = nullptr;
	if (!Root->TryGetObjectField(TEXT("states"), States) || !States || !States->IsValid())
	{
		OutError = TEXT("falta \"states\"");
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*States)->Values)
	{
		ESoundtrackState State = ESoundtrackState::Silence;
		if (!SoundtrackStateFromKey(Pair.Key, State) || State == ESoundtrackState::Silence)
		{
			OutError = FString::Printf(TEXT("estado desconocido %s"), *Pair.Key);
			return false;
		}
		if (!ExploredMusicDetail::ReadStateConfig(Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr, OutCatalog,
				OutCatalog.Config(State), OutError))
		{
			OutError = Pair.Key + TEXT(": ") + OutError;
			return false;
		}
	}
	return OutCatalog.Validate(OutError);
}

bool UExploredMusicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UTimeOfDaySubsystem>();
	Collection.InitializeDependency<UExploredWeatherSubsystem>();
	Collection.InitializeDependency<UExploredAmbienceSubsystem>();
}

void UExploredMusicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString Json;
	FString Error;
	if (!FFileHelper::LoadFileToString(Json, *ExploredMusicDetail::SoundtrackPath()))
	{
		UE_LOG(LogExplored, Error, TEXT("[Música] No se encontró %s: sin música"), *ExploredMusicDetail::SoundtrackPath());
		return;
	}
	if (!ParseSoundtrack(Json, Catalog, Error))
	{
		UE_LOG(LogExplored, Error, TEXT("[Música] soundtrack.json inválido: %s"), *Error);
		return;
	}
	Model = MakeUnique<FSoundtrackModel>(Catalog, ExploredMusicDetail::Seed);
	Clock = 0.0;
}

void UExploredMusicSubsystem::Deinitialize()
{
	for (const TPair<int32, TObjectPtr<UAudioComponent>>& Pair : Voices)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	Voices.Reset();
	Sounds.Reset();
	Model.Reset();
	Super::Deinitialize();
}

FSoundtrackContext UExploredMusicSubsystem::GatherContext() const
{
	FSoundtrackContext Out;
	Out.bCredits = bCredits;
	Out.bFinale = bFinale;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Out;
	}
	if (const AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
	{
		Out.bInMenu = PC->GetUIMode() == EExploredUIMode::Menu;
	}
	if (const UExploredAmbienceSubsystem* Ambience = World->GetSubsystem<UExploredAmbienceSubsystem>())
	{
		Out.Night = Ambience->GetListenerEnvironment().Night;
	}
	else if (const UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Out.Night = Time->IsNight() ? 1.0f : 0.0f;
	}
	if (const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>())
	{
		const EStormState Storm = Weather->GetStormState();
		Out.bCycloneWall = Weather->IsCycloneWall();
		Out.bCycloneEye = Storm == EStormState::CycloneEye;
	}
	return Out;
}

USoundBase* UExploredMusicSubsystem::GetSound(const FName& PieceId)
{
	if (const TObjectPtr<USoundBase>* Cached = Sounds.Find(PieceId))
	{
		return Cached->Get();
	}
	const int32 Index = Catalog.FindById(PieceId);
	const FSoftObjectPath Path(Index != INDEX_NONE ? Catalog.Pieces[Index].Asset : FString());
	USoundBase* Sound = Path.IsValid() && FPackageName::DoesPackageExist(Path.GetLongPackageName())
		? Cast<USoundBase>(Path.TryLoad()) : nullptr;
	if (!Sound)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Música] Falta la pieza %s (Tools/Audio/scripts/export_soundtrack.py e import_audio.py)"),
			*PieceId.ToString());
	}
	Sounds.Add(PieceId, Sound);
	return Sound;
}

void UExploredMusicSubsystem::Execute(const FSoundtrackCommand& Command)
{
	if (Command.Action == ESoundtrackAction::Stop)
	{
		TObjectPtr<UAudioComponent> Component;
		if (Voices.RemoveAndCopyValue(Command.Voice, Component) && Component)
		{
			// Las voces en bucle se crean sin autodestrucción; al pararlas sí se destruyen.
			Component->bAutoDestroy = true;
			Component->FadeOut(FMath::Max(Command.FadeSeconds, 0.05f), 0.0f);
		}
		return;
	}
	USoundBase* Sound = GetSound(Command.PieceId);
	if (!Sound)
	{
		return;
	}
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(GetWorld(), Sound, Command.Volume, 1.0f, 0.0f, nullptr, false, !Command.bLoop);
	if (!Component)
	{
		return;
	}
	// Como sonido de interfaz sigue sonando con el juego en pausa (menú de pausa).
	Component->bIsUISound = true;
	// Las piezas grabadas no son bucles en el asset: el bucle de menú, final y créditos va aquí.
	if (Command.bLoop)
	{
		const TWeakObjectPtr<UExploredMusicSubsystem> WeakThis(this);
		const int32 Voice = Command.Voice;
		const float Volume = Command.Volume;
		Component->OnAudioFinishedNative.AddLambda([WeakThis, Voice, Volume](UAudioComponent* Finished)
		{
			UExploredMusicSubsystem* Self = WeakThis.Get();
			const TObjectPtr<UAudioComponent>* Live = Self ? Self->Voices.Find(Voice) : nullptr;
			if (Live && Live->Get() == Finished)
			{
				Finished->FadeIn(1.0f, Volume);
			}
		});
	}
	Component->FadeIn(FMath::Max(Command.FadeSeconds, 0.05f), Command.Volume);
	Voices.Add(Command.Voice, Component);
}

void UExploredMusicSubsystem::Duck(float Gain, float HoldSeconds)
{
	if (!FMath::IsFinite(Gain) || !FMath::IsFinite(HoldSeconds) || HoldSeconds <= 0.0f)
	{
		return;
	}
	DuckRemaining = HoldSeconds;
	ApplyDuck(FMath::Clamp(Gain, 0.0f, 1.0f), ExploredMusicDetail::DuckInSeconds);
}

void UExploredMusicSubsystem::ApplyDuck(float Gain, float FadeSeconds)
{
	for (const TPair<int32, TObjectPtr<UAudioComponent>>& Voice : Voices)
	{
		if (UAudioComponent* Component = Voice.Value.Get())
		{
			Component->AdjustVolume(FadeSeconds, Gain);
		}
	}
}

void UExploredMusicSubsystem::Tick(float DeltaTime)
{
	if (!Model)
	{
		return;
	}
	if (DuckRemaining > 0.0f)
	{
		DuckRemaining -= DeltaTime;
		if (DuckRemaining <= 0.0f)
		{
			DuckRemaining = 0.0f;
			ApplyDuck(1.0f, ExploredMusicDetail::DuckOutSeconds);
		}
	}
	Clock += DeltaTime;
	ContextTimer -= DeltaTime;
	if (ContextTimer <= 0.0f)
	{
		ContextTimer = ExploredMusicDetail::ContextInterval;
		Context = GatherContext();
	}
	const ESoundtrackState Before = Model->GetState();
	TArray<FSoundtrackCommand> Commands;
	Model->Update(Clock, Context, Commands);
	if (Model->GetState() != Before)
	{
		UE_LOG(LogExplored, Display, TEXT("[Música] %s → %s"), LexToString(Before), LexToString(Model->GetState()));
	}
	for (const FSoundtrackCommand& Command : Commands)
	{
		Execute(Command);
	}
}

TStatId UExploredMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredMusicSubsystem, STATGROUP_Tickables);
}
