#include "Player/SwimComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Carry/CarryComponent.h"
#include "Ocean/ExploredOcean.h"
#include "Player/SwimModel.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace SwimComponentDetail
{
	constexpr float OceanSearchIntervalSeconds = 2.0f;
}

// GetWaterState convierte ESwimState (modelo puro) en el UENUM por su valor.
static_assert(static_cast<uint8>(ESwimState::OnLand) == static_cast<uint8>(EWaterState::OnLand), "ESwimState y EWaterState deben coincidir");
static_assert(static_cast<uint8>(ESwimState::Wading) == static_cast<uint8>(EWaterState::Wading), "ESwimState y EWaterState deben coincidir");
static_assert(static_cast<uint8>(ESwimState::Swimming) == static_cast<uint8>(EWaterState::Swimming), "ESwimState y EWaterState deben coincidir");
static_assert(static_cast<uint8>(ESwimState::Diving) == static_cast<uint8>(EWaterState::Diving), "ESwimState y EWaterState deben coincidir");

USwimComponent::USwimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SwimStrokeSounds = {
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_swim_stroke_01.sfx_swim_stroke_01"))),
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_swim_stroke_02.sfx_swim_stroke_02"))),
	};
	BubbleSounds = {
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_bubbles_01.sfx_bubbles_01"))),
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_bubbles_02.sfx_bubbles_02"))),
	};
	DiveSplashSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_dive_splash.sfx_dive_splash")));
	// TODO(audio): no hay un jadeo dedicado en el catálogo; sfx_breath_tired cubre el hueco hasta que exista uno.
	GaspSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/Efectos/sfx_breath_tired.sfx_breath_tired")));
}

void USwimComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			// Nuestros ajustes de velocidad tienen que aplicarse después de que el
			// movimiento integre el fotograma, si no se pisan entre sí.
			AddTickPrerequisiteComponent(Movement);
		}
		Carry = Character->FindComponentByClass<UCarryComponent>();
	}

	const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed);
	Straits = FOceanCurrents::BuildStraits(Layout);
	Ocean = FindOcean();
}

AExploredOcean* USwimComponent::FindOcean() const
{
	return Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(GetWorld(), AExploredOcean::StaticClass()));
}

float USwimComponent::GetCarriedWeightKg() const
{
	const UCarryComponent* CarryPtr = Carry.Get();
	return CarryPtr ? CarryPtr->GetTotalWeight() : 0.0f;
}

FSwimTuning USwimComponent::MakeTuning() const
{
	FSwimTuning Tuning;
	Tuning.SwimDepthCm = SwimDepthCm;
	Tuning.SwimExitHysteresisCm = SwimExitHysteresisCm;
	Tuning.FloatCenterDepthCm = FloatCenterDepthCm;
	Tuning.HeadHeightCm = HeadHeightCm;
	Tuning.HeadSurfaceHysteresisCm = HeadSurfaceHysteresisCm;
	Tuning.SwimSpeed = SwimSpeed;
	Tuning.DiveSpeed = DiveSpeed;
	Tuning.BuoyancyRiseSpeedCm = BuoyancyRiseSpeedCm;
	Tuning.SwimBrakingDeceleration = SwimBrakingDeceleration;
	Tuning.ComfortableWeightKg = ComfortableWeightKg;
	Tuning.StrokeFrequency = StrokeFrequency;
	Tuning.GaspThreshold = GaspThreshold;
	Tuning.DrowningDamagePerSecond = DrowningDamagePerSecond;
	return Tuning;
}

void USwimComponent::PlayOneShot(const TSoftObjectPtr<USoundBase>& SoundRef) const
{
	// Las rutas apuntan al catálogo de Tools/Audio; si el paquete de audio aún
	// no se ha construido, LoadSynchronous no encuentra el asset y no suena
	// nada (no hay crash: es el modo degradado esperado hasta que exista).
	if (USoundBase* Sound = SoundRef.LoadSynchronous())
	{
		if (const AActor* Owner = GetOwner())
		{
			UGameplayStatics::PlaySoundAtLocation(Owner, Sound, Owner->GetActorLocation());
		}
	}
}

AExploredOcean* USwimComponent::ResolveOcean(float DeltaTime)
{
	AExploredOcean* OceanActor = Ocean.Get();
	if (!OceanActor)
	{
		TimeSinceOceanSearch -= DeltaTime;
		if (TimeSinceOceanSearch <= 0.0f)
		{
			TimeSinceOceanSearch = SwimComponentDetail::OceanSearchIntervalSeconds;
			OceanActor = FindOcean();
			Ocean = OceanActor;
		}
	}
	return OceanActor;
}

FVector2D USwimComponent::SampleCurrent(const FVector& Location) const
{
	if (Straits.Num() == 0)
	{
		return FVector2D::ZeroVector;
	}
	const UWorld* World = GetWorld();
	const UTimeOfDaySubsystem* Time = World ? World->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
	const UExploredWeatherSubsystem* WeatherSys = World ? World->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	const float TotalDays = Time ? Time->GetTotalDays() : 0.0f;
	const float MoonPhase01 = Time ? Time->GetMoonPhase() : 0.0f;
	const float Wind01 = WeatherSys ? WeatherSys->GetCurrent().Wind : 0.2f;
	return FOceanCurrents::CurrentAt(Straits, FVector2D(Location.X, Location.Y),
		FOceanTide::Flow(TotalDays), FOceanTide::SpringNeapFactor(MoonPhase01), Wind01);
}

void USwimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	AExploredOcean* OceanActor = (Character && Capsule) ? ResolveOcean(DeltaTime) : nullptr;

	FSwimInputs In;
	In.bDiveHeld = bDiveHeld;
	In.CarriedWeightKg = GetCarriedWeightKg();
	In.LungCapacityRatio = LungCapacityRatio;
	if (Character && Capsule && OceanActor)
	{
		const FVector Location = Character->GetActorLocation();
		In.bHasWater = true;
		In.WaterZ = OceanActor->GetWaterHeightAt(Location);
		In.CenterZ = static_cast<float>(Location.Z);
		In.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		In.Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
		// La corriente solo arrastra a quien nada; muestrearla en seco sería trabajo perdido.
		if (Model.IsSwimming() || In.WaterZ - (In.CenterZ - In.HalfHeight) >= SwimDepthCm)
		{
			In.CurrentCmPerSecond = SampleCurrent(Location);
		}
	}

	const FSwimStep Step = Model.Tick(MakeTuning(), In, DeltaTime);
	ApplyStep(Step, DeltaTime);
}

void USwimComponent::ApplyStep(const FSwimStep& Step, float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	// L7: el vuelo de depuración (sin colisión) manda sobre el nado: no se toca su movimiento.
	if (Character && !Character->GetActorEnableCollision())
	{
		return;
	}

	// Modo de movimiento: el modelo ya aplica la histéresis (H5), así que la
	// zambullida suena una sola vez al entrar y no en cada valle de ola.
	if (Step.bSwimming)
	{
		if (Movement && Movement->MovementMode != MOVE_Flying)
		{
			Movement->SetMovementMode(MOVE_Flying);
			Movement->GravityScale = 0.0f;
		}
		if (Step.bEnteredWater)
		{
			OnDiveSplash.Broadcast();
			PlayOneShot(DiveSplashSound);
		}
	}
	else if (Step.bLeftWater && Movement && Movement->MovementMode == MOVE_Flying)
	{
		// En tierra o vadeando: sigue de pie, se deja el movimiento normal al CharacterMovementComponent.
		Movement->SetMovementMode(MOVE_Falling);
		Movement->GravityScale = 1.0f;
	}

	if (Step.bSwimming && Character && Movement)
	{
		Movement->MaxFlySpeed = Step.MaxSpeed;
		Movement->BrakingDecelerationFlying = Step.BrakingDeceleration;
		Movement->Velocity = Step.Velocity;

		// La corriente arrastra como desplazamiento (M4): sumada a la velocidad
		// como aceleración, el frenado en vuelo la anulaba.
		if (!Step.CurrentOffset.IsNearlyZero())
		{
			Character->AddActorWorldOffset(FVector(Step.CurrentOffset.X, Step.CurrentOffset.Y, 0.0), true);
		}

		if (Step.bStrokeCompleted && SwimStrokeSounds.Num() > 0)
		{
			PlayOneShot(SwimStrokeSounds[FMath::RandRange(0, SwimStrokeSounds.Num() - 1)]);
		}
	}

	AExploredOcean* OceanActor = Ocean.Get();
	if (Model.GetState() == ESwimState::Swimming && Character && OceanActor)
	{
		// La cámara siente la pendiente de la ola bajo el jugador; bajo el agua no hay oleaje que sentir.
		const FVector Location = Character->GetActorLocation();
		const FVector Normal = OceanActor->GetWaterNormalAt(Location);
		const FVector Right = Character->GetActorRightVector();
		const FVector Forward = Character->GetActorForwardVector();
		const float RollRad = FMath::Asin(FMath::Clamp(FVector::DotProduct(Normal, Right), -1.0f, 1.0f));
		const float PitchRad = FMath::Asin(FMath::Clamp(FVector::DotProduct(Normal, Forward), -1.0f, 1.0f));
		const FRotator TargetTilt(FMath::RadiansToDegrees(-PitchRad), 0.0f, FMath::RadiansToDegrees(RollRad));
		WaveTilt = FMath::RInterpTo(WaveTilt, TargetTilt, DeltaTime, 2.0f);
	}
	else if (Model.GetState() == ESwimState::Diving)
	{
		WaveTilt = FMath::RInterpTo(WaveTilt, FRotator::ZeroRotator, DeltaTime, 2.0f);
	}
	else
	{
		WaveTilt = FMath::RInterpTo(WaveTilt, FRotator::ZeroRotator, DeltaTime, 4.0f);
	}

	// Burbujas solo con la cabeza bajo el agua (antes sonaban también al mantener
	// la tecla de bucear en la superficie).
	if (Model.IsHeadUnderwater())
	{
		BubbleTimer -= DeltaTime;
		if (BubbleTimer <= 0.0f)
		{
			BubbleTimer = FMath::RandRange(1.6f, 3.2f);
			if (BubbleSounds.Num() > 0)
			{
				PlayOneShot(BubbleSounds[FMath::RandRange(0, BubbleSounds.Num() - 1)]);
			}
		}
	}

	// Apnea: el modelo decide; aquí solo se emiten los eventos.
	if (Step.bOxygenChanged)
	{
		OnOxygenChanged.Broadcast(GetOxygen01());
	}
	if (Step.DrowningDamagePerSecond > 0.0f)
	{
		OnDrowningDamage.Broadcast(Step.DrowningDamagePerSecond);
	}
	if (Step.bGasp)
	{
		OnGaspForAir.Broadcast();
		PlayOneShot(GaspSound);
	}
}
