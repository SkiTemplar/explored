#include "Fauna/ExploredMarineCreature.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

AExploredMarineCreature::AExploredMarineCreature()
{
	// La actualiza el gestor de fauna con su LOD; el actor no tiene Tick propio.
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetMobility(EComponentMobility::Movable);
	RootComponent = Body;
}

void AExploredMarineCreature::InitCreature(const FMarineBrainConfig& Config, const FFaunaWorldQuery& World, UStaticMesh* Mesh)
{
	Brain.Init(Config, World);
	AnimState.Phase01 = FFaunaAnimation::InitialPhase(Config.Seed);
	if (Mesh)
	{
		Body->SetStaticMesh(Mesh);
	}
	ApplyToActor(0.0f);
}

FMarineBrainEvents AExploredMarineCreature::StepCreature(float DeltaSeconds, const FFaunaStimuli& Stimuli, const FFaunaWorldQuery& World)
{
	const FMarineBrainEvents Events = Brain.Tick(DeltaSeconds, Stimuli, World);
	ApplyToActor(DeltaSeconds);
	return Events;
}

void AExploredMarineCreature::ApplyToActor(float DeltaSeconds)
{
	const FVector Forward = Brain.GetForward();
	const FVector Velocity = Brain.GetVelocity();
	// Cabeceo suave con la velocidad vertical (el delfín al saltar, la tortuga al subir a respirar).
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Velocity.Z), FMath::Max(1.0f, static_cast<float>(Velocity.Size2D()))));
	const FRotator Rotation(FMath::Clamp(Pitch, -45.0f, 45.0f), Forward.Rotation().Yaw, 0.0f);
	SetActorLocationAndRotation(Brain.GetPosition(), Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(!Brain.IsVisible());

	const bool bResting = Brain.GetState() == EMarineState::Buried || Brain.GetState() == EMarineState::Sounding
		|| Brain.GetState() == EMarineState::Nesting;
	const FFaunaAnimParams Anim = FFaunaAnimation::Advance(Brain.GetSpecies(), AnimState, static_cast<float>(Velocity.Size()),
		static_cast<float>(Velocity.Z), DeltaSeconds, bResting);
	Body->SetCustomPrimitiveDataFloat(0, Anim.Phase01);
	Body->SetCustomPrimitiveDataFloat(1, Anim.Amplitude);
	Body->SetCustomPrimitiveDataFloat(2, Anim.FrequencyHz);
	Body->SetCustomPrimitiveDataFloat(3, Anim.Secondary);
}
