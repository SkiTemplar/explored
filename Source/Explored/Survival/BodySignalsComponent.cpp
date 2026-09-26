#include "Survival/BodySignalsComponent.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Sound/SoundBase.h"

#include "Player/SwimComponent.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "UI/Widgets/SExploredWristWatch.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "Weather/WeatherModel.h"

namespace BodySignalsComponentDetail
{
	// Nombres de los parámetros del material de postproceso (docs/tecnico/cuerpo.md).
	const FName ParamVignette(TEXT("BodyVignette"));
	const FName ParamVignetteTint(TEXT("BodyVignetteTint"));
	const FName ParamBlur(TEXT("BodyBlur"));
	const FName ParamDesaturation(TEXT("BodyDesaturation"));
	const FName ParamBleedPulse(TEXT("BodyBleedPulse"));
	const FName ParamHeartRateHz(TEXT("BodyHeartRateHz"));
	const FName ParamHallucination(TEXT("BodyHallucination"));

	constexpr float EnvironmentSampleSeconds = 1.0f;
	constexpr float SunTraceCm = 5000.0f;
	constexpr float RoofTraceCm = 400.0f;

	float StormIntensityFor(EWeatherState Weather)
	{
		switch (Weather)
		{
		case EWeatherState::Shower: return 0.2f;
		case EWeatherState::Thunderstorm: return 0.6f;
		case EWeatherState::Gale: return 0.8f;
		case EWeatherState::Cyclone: return 1.0f;
		default: return 0.0f;
		}
	}
}

UBodySignalsComponent::UBodySignalsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f / 30.0f;

	BodyPostProcessMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_PP_Body.M_PP_Body")));
	HeartbeatLoop = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_heartbeat_low_loop.sfx_heartbeat_low_loop")));
	BreathSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_breath_tired.sfx_breath_tired")));
	StomachGrowlSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_stomach_growl.sfx_stomach_growl")));
}

void UBodySignalsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Material y sonidos son opcionales: si no existen todavía, el cuerpo sigue
	// funcionando con los ajustes de postproceso de la cámara y en silencio.
	if (UMaterialInterface* Material = BodyPostProcessMaterial.LoadSynchronous())
	{
		BodyPostProcessMID = UMaterialInstanceDynamic::Create(Material, this);
		if (UCameraComponent* Camera = FindCamera())
		{
			Camera->AddOrUpdateBlendable(BodyPostProcessMID.Get(), 1.0f);
		}
	}
	if (USoundBase* Loop = HeartbeatLoop.LoadSynchronous())
	{
		HeartbeatAudio = UGameplayStatics::CreateSound2D(this, Loop, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (HeartbeatAudio)
		{
			HeartbeatAudio->SetVolumeMultiplier(0.0f);
			HeartbeatAudio->Play();
		}
	}
	LoadedBreathSound = BreathSound.LoadSynchronous();
	LoadedStomachSound = StomachGrowlSound.LoadSynchronous();

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		FallApexZ = Character->GetActorLocation().Z;
	}
	SampleEnvironment();
}

void UBodySignalsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveWristWatch();
	if (HeartbeatAudio)
	{
		HeartbeatAudio->Stop();
		HeartbeatAudio = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UBodySignalsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	EnvironmentTimer -= DeltaTime;
	if (EnvironmentTimer <= 0.0f)
	{
		EnvironmentTimer = BodySignalsComponentDetail::EnvironmentSampleSeconds;
		SampleEnvironment();
	}

	UpdateEnergy(DeltaTime);
	UpdateFallDetection();

	StepAccumulator += DeltaTime;
	if (StepAccumulator >= SurvivalStepSeconds)
	{
		StepSurvival(StepAccumulator);
		StepAccumulator = 0.0f;
	}

	// Señales: objetivo sin memoria + suavizado por canal.
	FBodySignalContext Context;
	Context.EnergyRatio = State.Energy / FMath::Max(State.MaxEnergy(), 1.0f);
	const USwimComponent* Swim = GetOwner() ? GetOwner()->FindComponentByClass<USwimComponent>() : nullptr;
	Context.Oxygen01 = Swim ? Swim->GetOxygen01() : 1.0f;
	switch (Inputs.Activity)
	{
	case EActivity::Sprinting: Context.Exertion = 1.0f; break;
	case EActivity::Swimming: Context.Exertion = 0.6f; break;
	case EActivity::Working: Context.Exertion = 0.5f; break;
	default: Context.Exertion = 0.0f; break;
	}
	TargetSignals = FBodySignalsModel::Evaluate(State, Context);
	Signals = FBodySignalsModel::Smooth(Signals, TargetSignals, DeltaTime);
	TremorTime += DeltaTime;

	ApplyPostProcess();
	ApplyAudio(DeltaTime);

	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	WatchReadout = FBodySignalsModel::WristWatch(State, Time ? Time->GetHours() : 12.0f, bShowWatchNeeds);
	EnsureWristWatch();
}

void UBodySignalsComponent::Consume(const FConsumable& Item)
{
	TArray<ESurvivalEvent> Events;
	FSurvivalModel::Consume(State, Item, FMath::FRand(), Events);
	Broadcast(Events);
}

void UBodySignalsComponent::AddCut(float Depth)
{
	FBodyModel::AddCut(State, Depth);
	FBodyModel::ApplyMoraleEvent(State, EMoraleEvent::Injured);
}

void UBodySignalsComponent::TreatWounds(EWoundTreatment Treatment)
{
	FBodyModel::TreatWounds(State, Treatment);
}

void UBodySignalsComponent::ApplySting(EStingKind Kind)
{
	TArray<ESurvivalEvent> Events;
	FBodyModel::ApplySting(State, Kind, Events);
	Broadcast(Events);
}

void UBodySignalsComponent::ApplyMoraleEvent(EMoraleEvent Event)
{
	FBodyModel::ApplyMoraleEvent(State, Event);
}

void UBodySignalsComponent::ApplyFall(float HeightM, ELandingSurface Surface)
{
	TArray<ESurvivalEvent> Events;
	FBodyModel::ApplyFall(State, HeightM, Surface, ModeSettings, Events);
	Broadcast(Events);
}

FVector UBodySignalsComponent::GetHandTremorOffset(bool bRightHand) const
{
	const float Amount = FMath::Clamp(Signals.HandTremor + FBodyModel::Clumsiness(State) * 0.25f, 0.0f, 1.0f) * HandTremorMaxCm;
	if (Amount <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	// Ruido de Perlin a unos 9 Hz: temblor fino, distinto en cada mano.
	const float Seed = bRightHand ? 37.0f : 0.0f;
	const float T = TremorTime * 9.0f;
	return FVector(
		FMath::PerlinNoise1D(T + Seed) * 0.4f,
		FMath::PerlinNoise1D(T + Seed + 11.0f),
		FMath::PerlinNoise1D(T + Seed + 23.0f)) * Amount;
}

FRotator UBodySignalsComponent::GetShiverRotation() const
{
	const float Amount = Signals.Shivering * ShiverMaxDegrees;
	if (Amount <= KINDA_SMALL_NUMBER)
	{
		return FRotator::ZeroRotator;
	}
	const float T = TremorTime * 12.0f;
	return FRotator(FMath::PerlinNoise1D(T) * Amount, FMath::PerlinNoise1D(T + 17.0f) * Amount * 0.6f,
		FMath::PerlinNoise1D(T + 31.0f) * Amount * 0.5f);
}

void UBodySignalsComponent::StepSurvival(float RealSeconds)
{
	const float DeltaHours = RealSeconds * GameHoursPerSecond();
	if (DeltaHours <= 0.0f || State.IsDead())
	{
		return;
	}
	Inputs.Activity = CurrentActivity();
	TArray<ESurvivalEvent> Events;
	FSurvivalModel::Tick(State, Inputs, DeltaHours, ModeSettings, FMath::FRand(), Events);
	Broadcast(Events);

	// Estómago: la señal es una probabilidad por minuto real.
	if (LoadedStomachSound && FMath::FRand() < Signals.StomachGrowl * RealSeconds / 60.0f)
	{
		UGameplayStatics::PlaySound2D(this, LoadedStomachSound, 0.8f);
	}
}

void UBodySignalsComponent::SampleEnvironment()
{
	using namespace BodySignalsComponentDetail;

	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	float CloudCover = 0.2f;
	if (const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>())
	{
		const FWeatherSample& Sample = Weather->GetCurrent();
		Inputs.AirTemperature = Sample.Temperature;
		Inputs.Wind = Sample.Wind;
		Inputs.Rain = Sample.Rain;
		CloudCover = Sample.CloudCover;
		Inputs.StormIntensity = StormIntensityFor(Weather->GetState());
	}

	const USwimComponent* Swim = Owner->FindComponentByClass<USwimComponent>();
	Inputs.bInWater = Swim && Swim->IsInWater();

	const UCameraComponent* Camera = FindCamera();
	const FVector Eye = Camera ? Camera->GetComponentLocation() : Owner->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodySignalsEnvironment), false, Owner);

	// Refugio: algo sólido justo encima (techo; bajo un árbol frondoso también cuenta).
	FHitResult Hit;
	Inputs.bSheltered = World->LineTraceSingleByChannel(Hit, Eye, Eye + FVector::UpVector * RoofTraceCm, ECC_Visibility, Params);

	// Sol: altura del Sol, nubes y sombra real (traza hacia el Sol).
	Inputs.SunExposure = 0.0f;
	if (const UTimeOfDaySubsystem* Time = World->GetSubsystem<UTimeOfDaySubsystem>())
	{
		const FVector SunDir = ExploredSky::SunDirection(Time->GetHours(), Time->GetDayOfYear()).GetSafeNormal();
		if (SunDir.Z > 0.0f)
		{
			const bool bShaded = World->LineTraceSingleByChannel(Hit, Eye, Eye + SunDir * SunTraceCm, ECC_Visibility, Params);
			const float Elevation = FMath::Clamp(SunDir.Z * 1.4f, 0.0f, 1.0f);
			Inputs.SunExposure = Elevation * (1.0f - CloudCover * 0.7f) * (bShaded ? 0.1f : 1.0f);
		}
	}
}

void UBodySignalsComponent::UpdateEnergy(float DeltaSeconds)
{
	const EActivity Activity = CurrentActivity();
	const float Drain = FSurvivalModel::EnergyDrainPerSecond(Activity, Inputs.CarriedWeightRatio);
	State.Energy = FMath::Clamp(State.Energy - Drain * DeltaSeconds, 0.0f, State.MaxEnergy());
}

void UBodySignalsComponent::UpdateFallDetection()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	const float Z = Character->GetActorLocation().Z;
	const bool bFalling = Movement->IsFalling();
	if (Movement->MovementMode == MOVE_Flying)
	{
		// El vuelo de depuración no cuenta como caída al soltarlo.
		bFallFromFlight = true;
	}
	if (bFalling)
	{
		FallApexZ = bWasFalling ? FMath::Max(FallApexZ, Z) : Z;
	}
	else if (bWasFalling)
	{
		const float HeightM = (FallApexZ - Z) / 100.0f;
		if (!bFallFromFlight && HeightM > 0.0f)
		{
			const USwimComponent* Swim = Character->FindComponentByClass<USwimComponent>();
			const bool bWater = Swim && Swim->IsInWater();
			ApplyFall(HeightM, bWater ? ELandingSurface::Water : ELandingSurface::Ground);
		}
		bFallFromFlight = false;
	}
	if (!bFalling && Movement->MovementMode != MOVE_Flying)
	{
		bFallFromFlight = false;
	}
	bWasFalling = bFalling;
}

void UBodySignalsComponent::ApplyPostProcess()
{
	using namespace BodySignalsComponentDetail;

	if (BodyPostProcessMID)
	{
		BodyPostProcessMID->SetScalarParameterValue(ParamVignette, Signals.Vignette);
		BodyPostProcessMID->SetVectorParameterValue(ParamVignetteTint, Signals.VignetteTint);
		BodyPostProcessMID->SetScalarParameterValue(ParamBlur, Signals.Blur);
		BodyPostProcessMID->SetScalarParameterValue(ParamDesaturation, Signals.Desaturation);
		BodyPostProcessMID->SetScalarParameterValue(ParamBleedPulse, Signals.BleedingPulse);
		BodyPostProcessMID->SetScalarParameterValue(ParamHeartRateHz, FBodySignalsModel::HeartRateBpm(Signals.Heartbeat) / 60.0f);
		BodyPostProcessMID->SetScalarParameterValue(ParamHallucination, Signals.Hallucination);
		return;
	}

	// Respaldo sin material: viñeta, saturación y un leve tinte con el postproceso de la cámara.
	UCameraComponent* Camera = FindCamera();
	if (!Camera)
	{
		return;
	}
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.4f + Signals.Vignette * 1.2f;
	PP.bOverride_ColorSaturation = true;
	const float Saturation = 1.0f - Signals.Desaturation * 0.8f;
	PP.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
	PP.bOverride_SceneColorTint = true;
	PP.SceneColorTint = FLinearColor::LerpUsingHSV(FLinearColor::White, Signals.VignetteTint * 2.0f, Signals.Vignette * 0.15f);
}

void UBodySignalsComponent::ApplyAudio(float DeltaSeconds)
{
	if (HeartbeatAudio)
	{
		// Solo se oye cuando el corazón se acelera de verdad.
		const float Audible = FMath::Clamp((Signals.Heartbeat - 0.3f) / 0.7f, 0.0f, 1.0f);
		HeartbeatAudio->SetVolumeMultiplier(Audible);
		HeartbeatAudio->SetPitchMultiplier(FMath::Clamp(FBodySignalsModel::HeartRateBpm(Signals.Heartbeat) / 70.0f, 0.8f, 2.0f));
	}
	if (LoadedBreathSound && Signals.Breathing > 0.4f)
	{
		BreathTimer -= DeltaSeconds;
		if (BreathTimer <= 0.0f)
		{
			BreathTimer = 60.0f / FBodySignalsModel::BreathsPerMinute(Signals.Breathing);
			UGameplayStatics::PlaySound2D(this, LoadedBreathSound, FMath::Clamp((Signals.Breathing - 0.4f) / 0.6f, 0.2f, 1.0f));
		}
	}
}

void UBodySignalsComponent::EnsureWristWatch()
{
	if (WristWatchWidget.IsValid())
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !Pawn->IsLocallyControlled() || !World)
	{
		return;
	}
	UGameViewportClient* Viewport = World->GetGameViewport();
	if (!Viewport)
	{
		return;
	}
	TWeakObjectPtr<UBodySignalsComponent> WeakThis(this);
	WristWatchWidget = SNew(SExploredWristWatch)
		.Readout_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() ? WeakThis->GetWristWatchReadout() : FWristWatchReadout();
		})
		.Raised_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() && WeakThis->IsWristWatchRaised();
		});
	Viewport->AddViewportWidgetContent(WristWatchWidget.ToSharedRef(), 800);
}

void UBodySignalsComponent::RemoveWristWatch()
{
	if (!WristWatchWidget.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(WristWatchWidget.ToSharedRef());
		}
	}
	WristWatchWidget.Reset();
}

void UBodySignalsComponent::Broadcast(const TArray<ESurvivalEvent>& Events)
{
	for (const ESurvivalEvent Event : Events)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Explored] Cuerpo: suceso %d"), static_cast<int32>(Event));
		OnSurvivalEvent.Broadcast(Event);
	}
}

float UBodySignalsComponent::GameHoursPerSecond() const
{
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const float DayMinutes = Time ? Time->GetDayLengthMinutes() : FallbackDayLengthMinutes;
	return 24.0f / FMath::Max(DayMinutes * 60.0f, 1.0f);
}

EActivity UBodySignalsComponent::CurrentActivity() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return EActivity::Walking;
	}
	const USwimComponent* Swim = Character->FindComponentByClass<USwimComponent>();
	const EWaterState Water = Swim ? Swim->GetWaterState() : EWaterState::OnLand;
	if (Water == EWaterState::Swimming || Water == EWaterState::Diving)
	{
		return EActivity::Swimming;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Speed = Character->GetVelocity().Size2D();
	if (Speed < 10.0f)
	{
		return EActivity::Resting;
	}
	// Corre si va claramente por encima de la velocidad de andar configurada en el personaje.
	const float WalkSpeed = Movement ? Movement->MaxWalkSpeed : 450.0f;
	return Speed > 500.0f && WalkSpeed > 500.0f ? EActivity::Sprinting : EActivity::Walking;
}

UCameraComponent* UBodySignalsComponent::FindCamera() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
}
