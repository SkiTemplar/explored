#include "Audio/ExploredMusicSubsystem.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

#include "Audio/ExploredAmbienceSubsystem.h"
#include "Player/ExploredCharacter.h"
#include "Player/SwimComponent.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredPlayerController.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "Weather/WeatherModel.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace ExploredMusicDetail
{
	/** Semilla del director: fija por mundo para que la música sea reproducible. */
	constexpr uint32 MusicSeed = FArchipelagoLayout::OfficialSeed ^ 0x4D55u;
	/** Frecuencia con la que se recoge el contexto (s). */
	constexpr float ContextInterval = 0.25f;
	/** Frecuencia con la que se busca el próximo ciclón en el planificador (s). */
	constexpr float CycloneInterval = 5.0f;
	/** Volumen mínimo en lugar de 0: un componente a 0 exacto puede virtualizarse y perder la fase. */
	constexpr float MinAudible = 0.0001f;
	/** Pausa mínima entre notas de flauta (evita la repetición de tecla del sistema). */
	constexpr double FluteMinInterval = 0.08;

	FString MusicLayersPath()
	{
		return FPaths::ProjectContentDir() / TEXT("Data") / TEXT("music_layers.json");
	}

	bool ReadNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double& Out)
	{
		return Obj.IsValid() && Obj->TryGetNumberField(Field, Out);
	}
}

bool UExploredMusicSubsystem::ParseMusicLayers(const FString& Json, FMusicCatalog& OutCatalog, FString& OutError)
{
	OutCatalog = FMusicCatalog();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("JSON mal formado");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PiecesJson = nullptr;
	if (!Root->TryGetArrayField(TEXT("pieces"), PiecesJson) || !PiecesJson)
	{
		OutError = TEXT("Falta \"pieces\"");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *PiecesJson)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		FString RoleKey;
		if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Id) || !Obj->TryGetStringField(TEXT("role"), RoleKey))
		{
			OutError = TEXT("Pieza sin \"id\" o sin \"role\"");
			return false;
		}
		FMusicPiece Piece;
		Piece.Id = FName(*Id);
		if (!MusicRoleFromKey(RoleKey, Piece.Role))
		{
			// Un papel nuevo en Tools/Audio que el director aún no conoce: se ignora.
			continue;
		}
		Obj->TryGetStringField(TEXT("variant"), Piece.Variant);
		Obj->TryGetStringField(TEXT("asset"), Piece.Asset);
		double Number = 0.0;
		if (ExploredMusicDetail::ReadNumber(Obj, TEXT("bpm"), Number))
		{
			Piece.Bpm = static_cast<float>(Number);
		}
		if (ExploredMusicDetail::ReadNumber(Obj, TEXT("beats_per_bar"), Number))
		{
			Piece.BeatsPerBar = FMath::RoundToInt32(Number);
		}
		if (ExploredMusicDetail::ReadNumber(Obj, TEXT("bars"), Number))
		{
			Piece.Bars = static_cast<float>(Number);
		}
		Obj->TryGetBoolField(TEXT("loop"), Piece.bLoop);
		OutCatalog.Add(Piece);
	}

	const TSharedPtr<FJsonObject>* Days = nullptr;
	if (Root->TryGetObjectField(TEXT("day_variants"), Days) && Days && Days->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Days)->Values)
		{
			EIslandArchetype Archetype = EIslandArchetype::Count;
			if (!IslandArchetypeFromMusicKey(Pair.Key, Archetype) || !Pair.Value.IsValid())
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Id : Pair.Value->AsArray())
			{
				const int32 Index = Id.IsValid() ? OutCatalog.FindById(FName(*Id->AsString())) : INDEX_NONE;
				if (Index != INDEX_NONE)
				{
					OutCatalog.DayVariants[static_cast<int32>(Archetype)].Add(Index);
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* FluteJson = nullptr;
	if (Root->TryGetObjectField(TEXT("flute"), FluteJson) && FluteJson && FluteJson->IsValid())
	{
		FString Sample;
		if ((*FluteJson)->TryGetStringField(TEXT("sample"), Sample))
		{
			OutCatalog.Flute.SampleId = FName(*Sample);
		}
		(*FluteJson)->TryGetStringField(TEXT("asset"), OutCatalog.Flute.Asset);
		double Hz = 0.0;
		if (ExploredMusicDetail::ReadNumber(*FluteJson, TEXT("sample_hz"), Hz))
		{
			OutCatalog.Flute.SampleHz = static_cast<float>(Hz);
		}
		const TArray<TSharedPtr<FJsonValue>>* Semitones = nullptr;
		if ((*FluteJson)->TryGetArrayField(TEXT("semitones"), Semitones) && Semitones)
		{
			for (const TSharedPtr<FJsonValue>& Semitone : *Semitones)
			{
				OutCatalog.Flute.Semitones.Add(Semitone.IsValid() ? FMath::RoundToInt32(Semitone->AsNumber()) : 0);
			}
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
	if (!FFileHelper::LoadFileToString(Json, *ExploredMusicDetail::MusicLayersPath()))
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] No se encontró %s: sin música adaptativa"), *ExploredMusicDetail::MusicLayersPath());
		return;
	}
	if (!ParseMusicLayers(Json, Catalog, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[Explored] music_layers.json inválido: %s"), *Error);
		return;
	}
	Director = MakeUnique<FMusicDirectorModel>(Catalog, ExploredMusicDetail::MusicSeed);
	Flute = MakeUnique<FFluteModel>(Catalog.Flute);
	Clock = 0.0;
}

void UExploredMusicSubsystem::Deinitialize()
{
	for (UAudioComponent* Component : Pool)
	{
		if (Component)
		{
			Component->Stop();
		}
	}
	Pool.Reset();
	Layers.Reset();
	Pending.Reset();
	Sounds.Reset();
	Director.Reset();
	Flute.Reset();
	Super::Deinitialize();
}

void UExploredMusicSubsystem::SetDanger(float Danger01, FName Source)
{
	FDangerSource& Entry = Danger.FindOrAdd(Source);
	Entry.Value = FMath::Clamp(Danger01, 0.0f, 1.0f);
	Entry.Time = Clock;
}

void UExploredMusicSubsystem::SetFinale(bool bActive, EMusicFinale Finale)
{
	bFinale = bActive;
	FinaleKind = Finale;
}

void UExploredMusicSubsystem::NotifyDiscovery()
{
	if (!Director)
	{
		return;
	}
	TArray<FMusicLayerTarget> Targets;
	Director->NotifyDiscovery(Clock, Targets);
	Enqueue(Targets);
}

void UExploredMusicSubsystem::NotifyKeyMoment()
{
	if (Director)
	{
		Director->NotifyKeyMoment();
	}
}

float UExploredMusicSubsystem::GetFluteMoraleRatePerHour(float FireHeat) const
{
	return Flute ? Flute->MoraleRatePerHour(Clock, FireHeat) : 0.0f;
}

bool UExploredMusicSubsystem::PlayFluteNote(int32 NoteIndex)
{
	if (!Flute || NoteIndex < 0 || NoteIndex >= FFluteModel::NumNotes || Clock - LastFluteNoteTime < ExploredMusicDetail::FluteMinInterval)
	{
		return false;
	}
	LastFluteNoteTime = Clock;
	const FFluteNote Note = Flute->Play(Clock, NoteIndex);

	TObjectPtr<USoundBase>* Cached = Sounds.Find(Note.SampleId);
	USoundBase* Sound = Cached ? Cached->Get() : nullptr;
	if (!Sound && !Catalog.Flute.Asset.IsEmpty())
	{
		Sound = LoadObject<USoundBase>(nullptr, *Catalog.Flute.Asset, nullptr, LOAD_Quiet | LOAD_NoWarn);
		Sounds.Add(Note.SampleId, Sound);
	}
	if (!Sound)
	{
		return false;
	}
	// Diegética: suena como un efecto del jugador, no pasa por el volumen de Música.
	UGameplayStatics::PlaySound2D(this, Sound, 1.0f, Note.PitchMultiplier);
	return true;
}

float UExploredMusicSubsystem::GetSettingsVolume(bool bMusic) const
{
	const UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get();
	if (!Settings)
	{
		return 1.0f;
	}
	// Los USoundClass de los ajustes aún no están asignados a los assets (revisión H7):
	// se aplican aquí el volumen general y el de Música.
	const float Master = Settings->GetVolume(EExploredAudioChannel::Master) / 100.0f;
	const float Channel = bMusic ? Settings->GetVolume(EExploredAudioChannel::Music) / 100.0f : 1.0f;
	return FMath::Clamp(Master * Channel, 0.0f, 1.0f);
}

FMusicContext UExploredMusicSubsystem::GatherContext()
{
	UWorld* World = GetWorld();
	FMusicContext Out;
	Out.bSailing = bSailing;
	Out.bCredits = bCredits;
	Out.bFinale = bFinale;
	Out.Finale = FinaleKind;
	if (!World)
	{
		return Out;
	}

	if (const AExploredPlayerController* PC = Cast<AExploredPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
	{
		Out.bInMenu = PC->GetUIMode() == EExploredUIMode::Menu;
	}

	// Oyente: isla, noche y agua ya las evalúa el paisaje sonoro en la cámara.
	if (const UExploredAmbienceSubsystem* Ambience = World->GetSubsystem<UExploredAmbienceSubsystem>())
	{
		const FAmbienceEnvironment& Env = Ambience->GetListenerEnvironment();
		Out.Island = Ambience->GetListenerIsland();
		Out.Night = Env.Night;
		Out.Underwater = Env.Underwater;
	}
	else if (const UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Out.Night = Time->IsNight() ? 1.0f : 0.0f;
	}
	if (const AExploredCharacter* Character = Cast<AExploredCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)))
	{
		if (const USwimComponent* Swim = Character->GetSwimComponent())
		{
			if (Swim->GetWaterState() == EWaterState::Diving)
			{
				Out.Underwater = 1.0f;
			}
		}
	}

	// Clima: ciclón en curso, temporal y, cada pocos segundos, cuánto falta para el próximo ciclón.
	if (const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>())
	{
		const EWeatherState State = Weather->GetState();
		Out.bCyclone = State == EWeatherState::Cyclone;
		Out.Storm = (State == EWeatherState::Gale || State == EWeatherState::Thunderstorm) ? 1.0f : 0.0f;

		const UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>();
		if (CycloneTimer <= 0.0f && Time && Weather->GetModel())
		{
			CycloneTimer = ExploredMusicDetail::CycloneInterval;
			CycloneEtaHours = -1.0f;
			const float Days = Time->GetTotalDays();
			FWeatherSpan Span;
			// NextSevereEvent también devuelve galernas: se busca el primer ciclón.
			float From = Days;
			for (int32 Attempt = 0; Attempt < 4 && Weather->GetModel()->NextSevereEvent(From, Span); ++Attempt)
			{
				if (Span.State == EWeatherState::Cyclone)
				{
					CycloneEtaHours = FMath::Max(0.0f, (Span.Start - Days) * 24.0f);
					break;
				}
				From = Span.End + 0.01f;
			}
		}
		Out.CycloneEtaHours = Out.bCyclone ? -1.0f : CycloneEtaHours;
	}

	// Peligro: el máximo de las fuentes que se han refrescado hace poco.
	for (auto It = Danger.CreateIterator(); It; ++It)
	{
		if (Clock - It.Value().Time > DangerTimeoutSeconds)
		{
			It.RemoveCurrent();
			continue;
		}
		Out.Danger = FMath::Max(Out.Danger, It.Value().Value);
	}
	return Out;
}

USoundBase* UExploredMusicSubsystem::GetSound(const FName& PieceId)
{
	if (TObjectPtr<USoundBase>* Cached = Sounds.Find(PieceId))
	{
		return Cached->Get();
	}
	const int32 Index = Catalog.FindById(PieceId);
	USoundBase* Sound = Index != INDEX_NONE
		? LoadObject<USoundBase>(nullptr, *Catalog.Pieces[Index].Asset, nullptr, LOAD_Quiet | LOAD_NoWarn)
		: nullptr;
	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Explored] Falta la pieza de música %s (¿importado el audio?)"), *PieceId.ToString());
	}
	Sounds.Add(PieceId, Sound);
	return Sound;
}

UAudioComponent* UExploredMusicSubsystem::AcquireComponent(USoundBase* Sound)
{
	for (UAudioComponent* Component : Pool)
	{
		const bool bInUse = Layers.ContainsByPredicate([Component](const FLayer& Layer) { return Layer.Component.Get() == Component; });
		if (Component && !bInUse && !Component->IsPlaying())
		{
			Component->SetSound(Sound);
			return Component;
		}
	}
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(GetWorld(), Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	if (Component)
	{
		// Como sonido de interfaz sigue sonando con el juego en pausa (menú de pausa).
		Component->bIsUISound = true;
		Pool.Add(Component);
	}
	return Component;
}

void UExploredMusicSubsystem::Enqueue(const TArray<FMusicLayerTarget>& Targets)
{
	for (const FMusicLayerTarget& Target : Targets)
	{
		if (Target.Action == EMusicLayerAction::Cancel)
		{
			// Se descartan ya las órdenes pendientes de esa capa; si no había empezado, no empezará.
			Pending.RemoveAll([&Target](const FMusicLayerTarget& P) { return P.Voice == Target.Voice; });
			Layers.RemoveAll([&Target](const FLayer& Layer) { return Layer.Voice == Target.Voice && !Layer.bStarted; });
			continue;
		}
		// Inserción estable por instante: las órdenes del mismo instante conservan su orden.
		int32 Index = Pending.Num();
		while (Index > 0 && Pending[Index - 1].AtSeconds > Target.AtSeconds)
		{
			--Index;
		}
		Pending.Insert(Target, Index);
	}
}

void UExploredMusicSubsystem::Execute(const FMusicLayerTarget& Target)
{
	FLayer* Layer = Layers.FindByPredicate([&Target](const FLayer& L) { return L.Voice == Target.Voice; });
	if (Target.Action == EMusicLayerAction::Start)
	{
		USoundBase* Sound = GetSound(Target.PieceId);
		UAudioComponent* Component = Sound ? AcquireComponent(Sound) : nullptr;
		if (!Component)
		{
			return;
		}
		FLayer NewLayer;
		NewLayer.Voice = Target.Voice;
		NewLayer.Component = Component;
		NewLayer.bStarted = true;
		NewLayer.bLoop = Target.bLoop;
		NewLayer.FromVolume = 0.0f;
		NewLayer.ToVolume = Target.Volume;
		NewLayer.FadeStart = Target.AtSeconds;
		NewLayer.FadeSeconds = Target.FadeSeconds;
		Component->SetVolumeMultiplier(ExploredMusicDetail::MinAudible);
		// Si el fotograma llega tarde al límite de compás, se arranca adelantado lo que se
		// perdió para no desplazar la rejilla de la pieza.
		Component->Play(static_cast<float>(FMath::Max(0.0, Clock - Target.AtSeconds)));
		Layers.Add(NewLayer);
		return;
	}
	if (!Layer)
	{
		return;
	}
	Layer->FromVolume = Layer->Volume;
	Layer->ToVolume = Target.Action == EMusicLayerAction::Stop ? 0.0f : Target.Volume;
	Layer->FadeStart = Target.AtSeconds;
	Layer->FadeSeconds = Target.FadeSeconds;
	Layer->bStopAfterFade = Target.Action == EMusicLayerAction::Stop;
}

void UExploredMusicSubsystem::UpdateLayers(float DeltaTime)
{
	// Bus: se acerca a lo que pide el director en su tiempo de fundido.
	const FMusicBus& Bus = Director->GetBus();
	const float BusRate = 1.0f / FMath::Max(Bus.FadeSeconds, 0.05f);
	BusVolume = FMath::FInterpConstantTo(BusVolume, Bus.Volume, DeltaTime, BusRate);
	const float LogHz = FMath::FInterpConstantTo(FMath::Loge(BusLowPassHz), FMath::Loge(Bus.LowPassHz), DeltaTime, BusRate * 4.0f);
	BusLowPassHz = FMath::Exp(LogHz);
	const float Settings = GetSettingsVolume(true);
	const bool bFilter = BusLowPassHz < 19000.0f;

	for (int32 I = Layers.Num() - 1; I >= 0; --I)
	{
		FLayer& Layer = Layers[I];
		UAudioComponent* Component = Layer.Component.Get();
		if (!Component)
		{
			Layers.RemoveAt(I);
			continue;
		}
		const double Alpha = Layer.FadeSeconds > 0.0f
			? FMath::Clamp((Clock - Layer.FadeStart) / Layer.FadeSeconds, 0.0, 1.0)
			: (Clock >= Layer.FadeStart ? 1.0 : 0.0);
		Layer.Volume = FMath::Lerp(Layer.FromVolume, Layer.ToVolume, static_cast<float>(Alpha));

		const bool bFadedOut = Layer.bStopAfterFade && Alpha >= 1.0;
		const bool bEnded = !Layer.bLoop && !Component->IsPlaying();
		if (bFadedOut || bEnded)
		{
			Component->Stop();
			Layers.RemoveAt(I);
			continue;
		}
		Component->SetVolumeMultiplier(FMath::Max(ExploredMusicDetail::MinAudible, Layer.Volume * BusVolume * Settings));
		Component->SetLowPassFilterEnabled(bFilter);
		Component->SetLowPassFilterFrequency(BusLowPassHz);
	}
}

void UExploredMusicSubsystem::Tick(float DeltaTime)
{
	if (!Director)
	{
		return;
	}
	Clock += DeltaTime;
	ContextTimer -= DeltaTime;
	CycloneTimer -= DeltaTime;

	TArray<FMusicLayerTarget> Targets;
	if (ContextTimer <= 0.0f)
	{
		ContextTimer = ExploredMusicDetail::ContextInterval;
		Context = GatherContext();
	}
	Director->Update(Clock, Context, Targets);
	Enqueue(Targets);

	// Órdenes que ya han llegado a su instante, en orden.
	int32 Due = 0;
	while (Due < Pending.Num() && Pending[Due].AtSeconds <= Clock)
	{
		++Due;
	}
	if (Due > 0)
	{
		TArray<FMusicLayerTarget> Ready(Pending.GetData(), Due);
		Pending.RemoveAt(0, Due);
		for (const FMusicLayerTarget& Target : Ready)
		{
			Execute(Target);
		}
	}

	UpdateLayers(DeltaTime);
}

TStatId UExploredMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredMusicSubsystem, STATGROUP_Tickables);
}
