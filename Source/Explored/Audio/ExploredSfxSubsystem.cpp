#include "Audio/ExploredSfxSubsystem.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

#include "Audio/ExploredAmbienceSubsystem.h"
#include "Boats/ExploredRaft.h"
#include "Carry/CarryComponent.h"
#include "Cooking/ExploredFire.h"
#include "Ocean/ExploredOcean.h"
#include "Player/ExploredCharacter.h"
#include "UI/ExploredGameUserSettings.h"

namespace SfxDetail
{
	const TCHAR* const Root = TEXT("/Game/Generated/Audio/Efectos");
	constexpr float SlowInterval = 0.5f;
	constexpr float FootVolume = 0.55f;
	constexpr float PaddleVolume = 0.8f;
	constexpr float GullVolume = 0.35f;
	/** Alcance (cm) de los bucles de las hogueras. */
	constexpr float FireRadiusCm = 250.0f;
	constexpr float FireFalloffCm = 1600.0f;

	void Attenuate(UAudioComponent& Component, float Radius, float Falloff)
	{
		Component.bOverrideAttenuation = true;
		Component.AttenuationOverrides.bAttenuate = true;
		Component.AttenuationOverrides.bSpatialize = true;
		Component.AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		Component.AttenuationOverrides.AttenuationShapeExtents = FVector(Radius);
		Component.AttenuationOverrides.FalloffDistance = Falloff;
	}

	bool IsTerrain(const FHitResult& Hit)
	{
		const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Hit.GetComponent());
		const UStaticMesh* Asset = Mesh ? Mesh->GetStaticMesh() : nullptr;
		return Asset && Asset->GetName().Contains(TEXT("Terrain"));
	}
}

bool UExploredSfxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UExploredSfxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UExploredAmbienceSubsystem>();
	if (UExploredGameUserSettings* Settings = UExploredGameUserSettings::Get())
	{
		EffectsClass = Settings->GetSoundClass(EExploredAudioChannel::Effects);
		AmbientClass = Settings->GetSoundClass(EExploredAudioChannel::Ambient);
	}
}

void UExploredSfxSubsystem::Deinitialize()
{
	for (const TPair<TObjectPtr<AExploredFire>, TObjectPtr<UAudioComponent>>& Pair : FireLoops)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	for (const TPair<TObjectPtr<AExploredFire>, TObjectPtr<UAudioComponent>>& Pair : SizzleLoops)
	{
		if (Pair.Value)
		{
			Pair.Value->Stop();
		}
	}
	FireLoops.Reset();
	SizzleLoops.Reset();
	Sounds.Reset();
	Super::Deinitialize();
}

USoundBase* UExploredSfxSubsystem::GetSound(const FString& Name)
{
	if (const TObjectPtr<USoundBase>* Cached = Sounds.Find(Name))
	{
		return Cached->Get();
	}
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), SfxDetail::Root, *Name, *Name);
	USoundBase* Sound = FPackageName::DoesPackageExist(FSoftObjectPath(Path).GetLongPackageName())
		? LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn) : nullptr;
	Sounds.Add(Name, Sound);
	return Sound;
}

void UExploredSfxSubsystem::PlayEffect(const FString& Name, FVector Location, bool bAtLocation, float Volume, float Pitch)
{
	USoundBase* Sound = GetSound(Name.StartsWith(TEXT("sfx_")) ? Name : TEXT("sfx_") + Name);
	UWorld* World = GetWorld();
	if (!Sound || !World)
	{
		return;
	}
	UAudioComponent* Component = bAtLocation
		? UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.0f, nullptr, nullptr, false)
		: UGameplayStatics::CreateSound2D(World, Sound, Volume, Pitch, 0.0f, nullptr, false, true);
	if (!Component)
	{
		return;
	}
	if (EffectsClass)
	{
		Component->SoundClassOverride = EffectsClass;
	}
	if (bAtLocation)
	{
		SfxDetail::Attenuate(*Component, 150.0f, 2500.0f);
	}
	else
	{
		Component->Play();
	}
}

void UExploredSfxSubsystem::BindPickups()
{
	const AExploredCharacter* Character = Cast<AExploredCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	UCarryComponent* Carry = Character ? Character->GetCarryComponent() : nullptr;
	if (!Carry || BoundCarry.Get() == Carry)
	{
		return;
	}
	Carry->OnItemPickedUp.AddUniqueDynamic(this, &UExploredSfxSubsystem::HandlePickedUp);
	BoundCarry = Carry;
}

void UExploredSfxSubsystem::HandlePickedUp(FText)
{
	PlayEffect(FString::Printf(TEXT("sfx_pickup_%02d"), Random.RandRange(1, 3)), FVector::ZeroVector, false, 0.7f,
		Random.FRandRange(0.95f, 1.05f));
}

void UExploredSfxSubsystem::UpdateFootsteps(float DeltaTime)
{
	const AExploredCharacter* Character = Cast<AExploredCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move || !Move->IsMovingOnGround() || Character->GetAttachParentActor())
	{
		StepCadence.Reset();
		return;
	}
	const float Speed = static_cast<float>(Move->Velocity.Size2D());
	if (Speed < ExploredSfxRules::MinStepSpeedCmS)
	{
		return;
	}
	if (!StepCadence.Advance(Speed * DeltaTime, ExploredSfxRules::StrideFor(Speed)))
	{
		return;
	}
	const FHitResult& Hit = Move->CurrentFloor.HitResult;
	const FVector Foot = Character->GetActorLocation() - FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FFootGround Ground;
	Ground.bTerrain = SfxDetail::IsTerrain(Hit);
	Ground.HeightM = static_cast<float>(Foot.Z / 100.0);
	Ground.SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Hit.ImpactNormal.Z), -1.0f, 1.0f)));
	if (const AExploredOcean* OceanActor = Ocean.Get())
	{
		Ground.WaterDepthM = FMath::Max(0.0f, static_cast<float>((OceanActor->GetWaterHeightAt(Foot) - Foot.Z) / 100.0));
	}
	const EFootSurface Surface = ExploredSfxRules::SurfaceFor(Ground);
	PlayEffect(ExploredSfxRules::FootstepSound(Surface, Variant++), Foot, true, SfxDetail::FootVolume, Random.FRandRange(0.93f, 1.07f));
	++Footsteps;
}

void UExploredSfxSubsystem::UpdateRaft(float DeltaTime)
{
	const AExploredRaft* Boat = Raft.Get();
	if (!Boat || !Boat->IsOccupied() || Boat->IsGrounded())
	{
		PaddleCadence.Reset();
		return;
	}
	const FRaftControls& Controls = Boat->GetControls();
	const float Effort = FMath::Max(Controls.Forward, FMath::Abs(Controls.Turn) * 0.7f);
	if (Effort < 0.1f || !Controls.bHasPaddle)
	{
		return;
	}
	if (PaddleCadence.Advance(DeltaTime * Effort, ExploredSfxRules::PaddlePeriodS))
	{
		PlayEffect(FString::Printf(TEXT("sfx_paddle_stroke_%02d"), Random.RandRange(1, 2)), Boat->GetActorLocation(), true,
			SfxDetail::PaddleVolume, Random.FRandRange(0.94f, 1.06f));
	}
}

void UExploredSfxSubsystem::UpdateFires()
{
	auto Loop = [this](TMap<TObjectPtr<AExploredFire>, TObjectPtr<UAudioComponent>>& Map, AExploredFire* Fire, bool bWanted,
		const TCHAR* Name, float Volume)
	{
		TObjectPtr<UAudioComponent>* Existing = Map.Find(Fire);
		if (!bWanted)
		{
			if (Existing && *Existing)
			{
				(*Existing)->FadeOut(1.5f, 0.0f);
			}
			Map.Remove(Fire);
			return;
		}
		if (Existing && *Existing)
		{
			return;
		}
		USoundBase* Sound = GetSound(Name);
		UAudioComponent* Component = Sound ? UGameplayStatics::SpawnSoundAttached(Sound, Fire->GetRootComponent(), NAME_None,
			FVector(0.0, 0.0, 40.0), EAttachLocation::KeepRelativeOffset, true, Volume, 1.0f, 0.0f, nullptr, nullptr, false) : nullptr;
		if (!Component)
		{
			return;
		}
		Component->Stop();
		SfxDetail::Attenuate(*Component, SfxDetail::FireRadiusCm, SfxDetail::FireFalloffCm);
		if (EffectsClass)
		{
			Component->SoundClassOverride = EffectsClass;
		}
		Component->FadeIn(1.5f, Volume);
		Map.Add(Fire, Component);
	};
	for (TActorIterator<AExploredFire> It(GetWorld()); It; ++It)
	{
		AExploredFire* Fire = *It;
		Loop(FireLoops, Fire, Fire->IsBurning(), TEXT("sfx_fire_loop"), 0.7f);
		Loop(SizzleLoops, Fire, Fire->IsBurning() && Fire->HasJob() && !Fire->IsJobReady(), TEXT("sfx_cooking_sizzle_loop"), 0.6f);
	}
}

void UExploredSfxSubsystem::UpdateGulls(float DeltaTime)
{
	GullTimer -= DeltaTime;
	if (GullTimer > 0.0f)
	{
		return;
	}
	GullTimer = ExploredSfxRules::GullGap(Random.FRand());
	const UExploredAmbienceSubsystem* Ambience = GetWorld()->GetSubsystem<UExploredAmbienceSubsystem>();
	if (!Ambience)
	{
		return;
	}
	const FAmbienceEnvironment& Env = Ambience->GetListenerEnvironment();
	if (Env.Underwater > 0.5f || Env.Interior > 0.5f || !ExploredSfxRules::GullsAudible(Env.Coast, Env.Night, Env.Rain))
	{
		return;
	}
	USoundBase* Sound = GetSound(FString::Printf(TEXT("sfx_bird_gull_%02d"), Random.RandRange(1, 3)));
	UAudioComponent* Component = Sound ? UGameplayStatics::CreateSound2D(GetWorld(), Sound, SfxDetail::GullVolume * Env.Coast,
		Random.FRandRange(0.9f, 1.1f), 0.0f, nullptr, false, true) : nullptr;
	if (Component)
	{
		if (AmbientClass)
		{
			Component->SoundClassOverride = AmbientClass;
		}
		Component->Play();
	}
}

void UExploredSfxSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || DeltaTime <= 0.0f)
	{
		return;
	}
	SlowTimer -= DeltaTime;
	if (SlowTimer <= 0.0f)
	{
		SlowTimer = SfxDetail::SlowInterval;
		if (!Ocean.IsValid())
		{
			Ocean = Cast<AExploredOcean>(UGameplayStatics::GetActorOfClass(World, AExploredOcean::StaticClass()));
		}
		if (!Raft.IsValid())
		{
			Raft = Cast<AExploredRaft>(UGameplayStatics::GetActorOfClass(World, AExploredRaft::StaticClass()));
		}
		BindPickups();
		UpdateFires();
	}
	UpdateFootsteps(DeltaTime);
	UpdateRaft(DeltaTime);
	UpdateGulls(DeltaTime);
}

TStatId UExploredSfxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UExploredSfxSubsystem, STATGROUP_Tickables);
}
