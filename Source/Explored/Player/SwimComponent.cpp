#include "Player/SwimComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "Carry/CarryComponent.h"
#include "Ocean/ExploredOcean.h"
#include "Sky/TimeOfDaySubsystem.h"
#include "Survival/SurvivalModel.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"

namespace
{
	// El material nada en superficie con la cabeza fuera; la cámara queda a esta altura de la ola.
	constexpr float EyeOffsetCm = 40.0f;
	constexpr float BuoyancyRiseSpeedCm = 60.0f;
	constexpr float OceanSearchIntervalSeconds = 2.0f;
}

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

float USwimComponent::GetCarriedWeightRatio() const
{
	const UCarryComponent* CarryPtr = Carry.Get();
	if (!CarryPtr || ComfortableWeightKg <= 0.0f)
	{
		return 0.0f;
	}
	return CarryPtr->GetTotalWeight() / ComfortableWeightKg;
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

void USwimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateWaterState(DeltaTime);
	ApplySwimMovement(DeltaTime);
	TickBreath(DeltaTime);
}

void USwimComponent::UpdateWaterState(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !Capsule)
	{
		State = EWaterState::OnLand;
		return;
	}

	AExploredOcean* OceanActor = Ocean.Get();
	if (!OceanActor)
	{
		TimeSinceOceanSearch -= DeltaTime;
		if (TimeSinceOceanSearch <= 0.0f)
		{
			TimeSinceOceanSearch = OceanSearchIntervalSeconds;
			OceanActor = FindOcean();
			Ocean = OceanActor;
		}
		if (!OceanActor)
		{
			State = EWaterState::OnLand;
			return;
		}
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Location = Character->GetActorLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float FootZ = Location.Z - HalfHeight;
	const float WaterZ = OceanActor->GetWaterHeightAt(Location);
	const float Coverage = WaterZ - FootZ; // cm de agua por encima de los pies.

	const bool bWasSwimmingOrDiving = State == EWaterState::Swimming || State == EWaterState::Diving;

	if (Coverage <= 0.0f || Coverage < SwimDepthCm)
	{
		// En tierra o vadeando: sigue de pie, se deja el movimiento normal al CharacterMovementComponent.
		if (bWasSwimmingOrDiving && Movement && Movement->MovementMode == MOVE_Flying)
		{
			Movement->SetMovementMode(MOVE_Falling);
			Movement->GravityScale = 1.0f;
		}
		State = Coverage <= 0.0f ? EWaterState::OnLand : EWaterState::Wading;
		return;
	}

	// Agua lo bastante profunda para no hacer pie: nada o bucea, según la tecla mantenida.
	State = bDiveHeld ? EWaterState::Diving : EWaterState::Swimming;
	if (Movement && Movement->MovementMode != MOVE_Flying)
	{
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.0f;
		if (!bWasSwimmingOrDiving)
		{
			OnDiveSplash.Broadcast();
			PlayOneShot(DiveSplashSound);
		}
	}
}

void USwimComponent::ApplySwimMovement(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (State != EWaterState::Swimming && State != EWaterState::Diving)
	{
		WaveTilt = FMath::RInterpTo(WaveTilt, FRotator::ZeroRotator, DeltaTime, 4.0f);
		StrokePhase = 0.0f;
		bIsExertingUnderwater = false;
		return;
	}
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	AExploredOcean* OceanActor = Ocean.Get();
	if (!Movement || !OceanActor)
	{
		return;
	}

	const FVector Location = Character->GetActorLocation();
	const float WeightRatio = GetCarriedWeightRatio();
	// Cargar de más cansa y hunde al nadar (GDD §4.1): menos velocidad máxima.
	const float Fatigue = FMath::Clamp(1.0f - WeightRatio * 0.35f, 0.35f, 1.0f);
	Movement->MaxFlySpeed = (State == EWaterState::Diving ? DiveSpeed : SwimSpeed) * Fatigue;
	Movement->BrakingDecelerationFlying = 900.0f;

	FVector Velocity = Movement->Velocity;
	const float WaterZ = OceanActor->GetWaterHeightAt(Location);
	if (State == EWaterState::Swimming)
	{
		// Flota a la altura de la ola bajo el jugador, con la cabeza fuera.
		const float DesiredVelZ = FMath::Clamp((WaterZ - EyeOffsetCm - Location.Z) * 6.0f, -300.0f, 300.0f);
		Velocity.Z = FMath::FInterpTo(Velocity.Z, DesiredVelZ, DeltaTime, 3.0f);
	}
	else
	{
		// Bucear es mantener la tecla para bajar; soltarla deja que el pulmón empuje hacia arriba.
		const float DesiredVelZ = bDiveHeld ? -DiveSpeed * 0.6f : BuoyancyRiseSpeedCm;
		Velocity.Z = FMath::FInterpTo(Velocity.Z, DesiredVelZ, DeltaTime, 2.0f);
	}

	if (Straits.Num() > 0)
	{
		const UTimeOfDaySubsystem* Time = GetWorld() ? GetWorld()->GetSubsystem<UTimeOfDaySubsystem>() : nullptr;
		const UExploredWeatherSubsystem* WeatherSys = GetWorld() ? GetWorld()->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
		const float TotalDays = Time ? Time->GetTotalDays() : 0.0f;
		const float MoonPhase01 = Time ? Time->GetMoonPhase() : 0.0f;
		const float Wind01 = WeatherSys ? WeatherSys->GetCurrent().Wind : 0.2f;
		const FVector2D Current = FOceanCurrents::CurrentAt(Straits, FVector2D(Location.X, Location.Y),
			FOceanTide::Flow(TotalDays), FOceanTide::SpringNeapFactor(MoonPhase01), Wind01);
		Velocity.X += Current.X * DeltaTime;
		Velocity.Y += Current.Y * DeltaTime;
	}
	Movement->Velocity = Velocity;
	bIsExertingUnderwater = Velocity.Size2D() > SwimSpeed * 0.5f;

	// Brazadas: fase continua con la velocidad; el personaje la usa para animar las manos.
	const float SpeedRatio = FMath::Clamp(Velocity.Size2D() / FMath::Max(SwimSpeed, 1.0f), 0.0f, 1.2f);
	if (SpeedRatio > KINDA_SMALL_NUMBER)
	{
		const float Prev = StrokePhase;
		StrokePhase = FMath::Fmod(StrokePhase + DeltaTime * SpeedRatio * StrokeFrequency, 1.0f);
		if (StrokePhase < Prev && SwimStrokeSounds.Num() > 0)
		{
			PlayOneShot(SwimStrokeSounds[FMath::RandRange(0, SwimStrokeSounds.Num() - 1)]);
		}
	}

	if (State == EWaterState::Swimming)
	{
		// La cámara siente la pendiente de la ola bajo el jugador; bajo el agua no hay oleaje que sentir.
		const FVector Normal = OceanActor->GetWaterNormalAt(Location);
		const FVector Right = Character->GetActorRightVector();
		const FVector Forward = Character->GetActorForwardVector();
		const float RollRad = FMath::Asin(FMath::Clamp(FVector::DotProduct(Normal, Right), -1.0f, 1.0f));
		const float PitchRad = FMath::Asin(FMath::Clamp(FVector::DotProduct(Normal, Forward), -1.0f, 1.0f));
		const FRotator TargetTilt(FMath::RadiansToDegrees(-PitchRad), 0.0f, FMath::RadiansToDegrees(RollRad));
		WaveTilt = FMath::RInterpTo(WaveTilt, TargetTilt, DeltaTime, 2.0f);
	}
	else
	{
		WaveTilt = FMath::RInterpTo(WaveTilt, FRotator::ZeroRotator, DeltaTime, 2.0f);
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
}

void USwimComponent::TickBreath(float DeltaTime)
{
	const bool bSubmerged = State == EWaterState::Diving;
	if (bSubmerged)
	{
		const float Drain = FSurvivalModel::OxygenDrainPerSecond(GetCarriedWeightRatio(), LungCapacityRatio, bIsExertingUnderwater);
		const float NewOxygen = FMath::Max(0.0f, Oxygen - Drain * DeltaTime);
		// Se escribe siempre: con un umbral, a FPS altos el paso por frame
		// (Drain * DeltaTime) quedaba por debajo y el oxigeno no variaba nunca.
		const bool bChanged = NewOxygen != Oxygen;
		Oxygen = NewOxygen;
		if (bChanged)
		{
			OnOxygenChanged.Broadcast(GetOxygen01());
		}
		if (Oxygen <= 0.0f)
		{
			OnDrowningDamage.Broadcast(DrowningDamagePerSecond);
		}
	}
	else
	{
		if (bWasSubmergedLastTick && Oxygen < GaspThreshold)
		{
			OnGaspForAir.Broadcast();
			PlayOneShot(GaspSound);
		}
		const float Recovery = FSurvivalModel::OxygenRecoveryPerSecond(LungCapacityRatio);
		const float NewOxygen = FMath::Min(100.0f, Oxygen + Recovery * DeltaTime);
		// Igual que al consumir: sin umbral, para que la recuperacion no se
		// detenga a FPS altos.
		const bool bChanged = NewOxygen != Oxygen;
		Oxygen = NewOxygen;
		if (bChanged)
		{
			OnOxygenChanged.Broadcast(GetOxygen01());
		}
	}
	bWasSubmergedLastTick = bSubmerged;
}
