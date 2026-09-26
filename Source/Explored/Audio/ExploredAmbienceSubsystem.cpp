#include "Audio/ExploredAmbienceSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Sky/TimeOfDaySubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

namespace
{
	constexpr int32 NumLayers = static_cast<int32>(EAmbienceLayer::Count);
	constexpr float SampleInterval = 0.4f;
	constexpr float FadeSpeed = 0.6f; // unidades de volumen por segundo
	constexpr uint32 WorldSeed = 20260926;

	const TCHAR* LayerAssets[NumLayers] = {
		TEXT("/Game/Generated/Audio/Ambiente/amb_ocean_calm.amb_ocean_calm"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_ocean_rough.amb_ocean_rough"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_wind_light.amb_wind_light"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_wind_strong.amb_wind_strong"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_jungle_day.amb_jungle_day"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_jungle_night.amb_jungle_night"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_rain_light.amb_rain_light"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_rain_heavy.amb_rain_heavy"),
		TEXT("/Game/Generated/Audio/Ambiente/amb_underwater.amb_underwater"),
	};

	float VegetationWeight(EIslandArchetype Archetype)
	{
		switch (Archetype)
		{
		case EIslandArchetype::Emerald: return 1.0f;
		case EIslandArchetype::Mangrove: return 0.9f;
		case EIslandArchetype::Landing: return 0.75f;
		case EIslandArchetype::Mesa: return 0.5f;
		case EIslandArchetype::Smoke: return 0.35f;
		case EIslandArchetype::Teeth: return 0.25f;
		case EIslandArchetype::WhiteSands: return 0.4f;
		default: return 0.0f;
		}
	}
}

void FAmbienceMixer::Mix(const FAmbienceEnvironment& Env, float Out[NumLayers])
{
	const float Dry = 1.0f - Env.Underwater;
	const float Calm = 1.0f - FMath::SmoothStep(0.3f, 0.8f, Env.SeaState);

	Out[static_cast<int32>(EAmbienceLayer::OceanCalm)] = Env.Coast * Calm * Dry;
	Out[static_cast<int32>(EAmbienceLayer::OceanRough)] = FMath::Max(Env.Coast, 0.35f) * (1.0f - Calm) * Dry;
	Out[static_cast<int32>(EAmbienceLayer::WindLight)] = (0.25f + 0.5f * Env.Altitude) * (1.0f - Env.Vegetation * 0.5f) * Dry;
	Out[static_cast<int32>(EAmbienceLayer::WindStrong)] = FMath::SmoothStep(0.5f, 1.0f, Env.Altitude) * 0.8f * Dry;
	Out[static_cast<int32>(EAmbienceLayer::JungleDay)] = Env.Vegetation * (1.0f - Env.Night) * (1.0f - Env.Rain * 0.7f) * Dry;
	Out[static_cast<int32>(EAmbienceLayer::JungleNight)] = FMath::Max(Env.Vegetation, 0.3f) * Env.Night * (1.0f - Env.Rain * 0.6f) * Dry;
	Out[static_cast<int32>(EAmbienceLayer::RainLight)] = (1.0f - FMath::SmoothStep(0.4f, 0.8f, Env.Rain)) * FMath::SmoothStep(0.0f, 0.2f, Env.Rain) * Dry;
	Out[static_cast<int32>(EAmbienceLayer::RainHeavy)] = FMath::SmoothStep(0.5f, 1.0f, Env.Rain) * Dry;
	Out[static_cast<int32>(EAmbienceLayer::Underwater)] = Env.Underwater;
}

bool UExploredAmbienceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredAmbienceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Density = MakeShared<FTerrainDensity>(FArchipelagoLayout::Generate(WorldSeed));

	CurrentVolumes.Init(0.0f, NumLayers);
	TargetVolumes.Init(0.0f, NumLayers);
	for (int32 I = 0; I < NumLayers; ++I)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, LayerAssets[I], nullptr, LOAD_Quiet | LOAD_NoWarn);
		UAudioComponent* Component = Sound ? UGameplayStatics::CreateSound2D(&InWorld, Sound, 1.0f, 1.0f, 0.0f, nullptr, true, false) : nullptr;
		if (Component)
		{
			Component->SetVolumeMultiplier(0.0f);
			// Arranque en un punto aleatorio del bucle para que las capas no suenen sincronizadas.
			Component->Play(FMath::FRandRange(0.0f, 20.0f));
		}
		Layers.Add(Component);
	}
}

void UExploredAmbienceSubsystem::Deinitialize()
{
	for (UAudioComponent* Component : Layers)
	{
		if (Component)
		{
			Component->Stop();
		}
	}
	Layers.Reset();
	Super::Deinitialize();
}

FAmbienceEnvironment UExploredAmbienceSubsystem::Evaluate(const FVector& ListenerCm) const
{
	FAmbienceEnvironment Env;
	Env.Rain = Rain;
	Env.SeaState = SeaState;
	if (!Density)
	{
		return Env;
	}

	const FVector P = ListenerCm / 100.0;
	const FTerrainColumn Here = Density->SampleColumn(P.X, P.Y);
	Env.Underwater = P.Z < -0.3f ? 1.0f : 0.0f;
	Env.Altitude = FMath::Clamp((P.Z - FMath::Max(Here.Height, 0.0f) * 0.2f) / 250.0f, 0.0f, 1.0f);

	// Distancia a la costa: primera muestra de agua en 8 direcciones.
	float Nearest = 160.0f;
	if (Here.Height < 0.0f)
	{
		Nearest = 0.0f;
	}
	else
	{
		for (int32 Dir = 0; Dir < 8; ++Dir)
		{
			const float Angle = Dir * UE_TWO_PI / 8.0f;
			for (float R = 10.0f; R < Nearest; R += 15.0f)
			{
				if (Density->SampleColumn(P.X + FMath::Cos(Angle) * R, P.Y + FMath::Sin(Angle) * R).Height < 0.0f)
				{
					Nearest = R;
					break;
				}
			}
		}
	}
	Env.Coast = 1.0f - FMath::SmoothStep(15.0f, 150.0f, Nearest + FMath::Max(0.0f, static_cast<float>(P.Z) - 5.0f) * 0.8f);

	if (Here.IslandIndex != INDEX_NONE && Here.Height > 2.5f)
	{
		Env.Vegetation = VegetationWeight(Density->GetLayout().Islands[Here.IslandIndex].Archetype) *
			(1.0f - FMath::SmoothStep(120.0f, 260.0f, Here.Height));
	}

	if (const UTimeOfDaySubsystem* Time = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		const float SunZ = static_cast<float>(ExploredSky::SunDirection(Time->GetHours(), Time->GetDayOfYear()).Z);
		Env.Night = 1.0f - FMath::SmoothStep(-0.12f, 0.05f, SunZ);
	}
	return Env;
}

void UExploredAmbienceSubsystem::Tick(float DeltaTime)
{
	SampleTimer -= DeltaTime;
	if (SampleTimer <= 0.0f)
	{
		SampleTimer = SampleInterval;
		const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC && PC->PlayerCameraManager)
		{
			float Mix[NumLayers];
			FAmbienceMixer::Mix(Evaluate(PC->PlayerCameraManager->GetCameraLocation()), Mix);
			for (int32 I = 0; I < NumLayers; ++I)
			{
				TargetVolumes[I] = Mix[I];
			}
		}
	}

	for (int32 I = 0; I < Layers.Num(); ++I)
	{
		CurrentVolumes[I] = FMath::FInterpConstantTo(CurrentVolumes[I], TargetVolumes[I], DeltaTime, FadeSpeed);
		if (Layers[I])
		{
			Layers[I]->SetVolumeMultiplier(FMath::Max(0.0001f, CurrentVolumes[I] * MasterVolume));
		}
	}
}

TStatId UExploredAmbienceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredAmbienceSubsystem, STATGROUP_Tickables);
}
