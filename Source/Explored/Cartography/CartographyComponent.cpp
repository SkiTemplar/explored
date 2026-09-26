#include "Cartography/CartographyComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Cartography/CoastlineTrace.h"
#include "Player/SwimComponent.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

UCartographyComponent::UCartographyComponent()
	: Model(FArchipelagoLayout::OfficialSeed)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.33f;
}

void UCartographyComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(FMath::Clamp(SampleInterval, 0.1f, 1.0f));

	if (AActor* Owner = GetOwner())
	{
		Swim = Owner->FindComponentByClass<USwimComponent>();
	}
	BuildReferenceCoasts();
}

void UCartographyComponent::BuildReferenceCoasts()
{
	// Mismo archipiélago que genera el terreno (semilla publicada). Unas 7 × 180 columnas
	// por rayo: ~0,2 s una sola vez al empezar. TODO(P-SAVE): semilla de la partida.
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	const int32 NumIslands = Density.GetLayout().Islands.Num();
	for (int32 Island = 0; Island < NumIslands; ++Island)
	{
		Model.RegisterIslandCoast(Island, FCoastlineTrace::TraceIsland(Density, Island, CoastRays));
	}
	ChartedIslands.Init(0, NumIslands);
	for (int32 Island = 0; Island < NumIslands; ++Island)
	{
		ChartedIslands[Island] = Model.IsIslandCharted(Island) ? 1 : 0;
	}
}

void UCartographyComponent::LoadState(const FCartographyState& State)
{
	Model.LoadState(State);
	// Las costas de referencia se vuelven a registrar: conservan lo recorrido si coinciden.
	BuildReferenceCoasts();
	LastSampleTime = -1.0;
}

FVector2D UCartographyComponent::GetOwnerPositionMeters() const
{
	const AActor* Owner = GetOwner();
	const FVector Location = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	return FVector2D(Location.X, Location.Y) / 100.0;
}

ECartographyLocomotion UCartographyComponent::ReadLocomotion() const
{
	if (const USwimComponent* SwimComponent = Swim.Get())
	{
		const EWaterState WaterState = SwimComponent->GetWaterState();
		if (WaterState == EWaterState::Swimming || WaterState == EWaterState::Diving)
		{
			return ECartographyLocomotion::Swimming;
		}
	}
	const AActor* Owner = GetOwner();
	const double Speed = Owner ? Owner->GetVelocity().Size2D() : 0.0;
	return Speed > RunSpeedThreshold ? ECartographyLocomotion::Running : ECartographyLocomotion::Walking;
}

FCartographyExposure UCartographyComponent::ReadExposure() const
{
	FCartographyExposure Exposure;
	Exposure.bStoredDry = bMapStoredDry;
	if (const UWorld* World = GetWorld())
	{
		if (const UExploredWeatherSubsystem* Weather = World->GetSubsystem<UExploredWeatherSubsystem>())
		{
			// TODO(P-BUILD): bajo techo no llueve sobre el mapa.
			Exposure.Rain = Weather->GetCurrent().Rain;
		}
	}
	if (const USwimComponent* SwimComponent = Swim.Get())
	{
		const EWaterState WaterState = SwimComponent->GetWaterState();
		Exposure.bInSeaWater = WaterState == EWaterState::Swimming || WaterState == EWaterState::Diving;
	}
	return Exposure;
}

void UCartographyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Con intervalo de tick se mide el tiempo real entre muestras (también tras una pausa).
	const double Now = World->GetTimeSeconds();
	const float Elapsed = LastSampleTime < 0.0 ? SampleInterval : static_cast<float>(Now - LastSampleTime);
	LastSampleTime = Now;

	FCartographySample Sample;
	Sample.WorldPosition = GetOwnerPositionMeters();
	Sample.DeltaSeconds = Elapsed;
	Sample.Locomotion = ReadLocomotion();
	Sample.bHasCompass = bHasCompass;
	int32 Island = INDEX_NONE;
	Sample.DistanceToShore = Model.DistanceToReferenceCoast(Sample.WorldPosition, Island);
	Sample.IslandIndex = Island;

	const bool bHadDrawnCoast = Model.HasDrawnAnyCoast();
	Model.Sample(Sample);
	Model.TickWetness(ReadExposure(), Elapsed);
	BroadcastProgress(bHadDrawnCoast);
}

void UCartographyComponent::BroadcastProgress(bool bHadDrawnCoast)
{
	if (!bHadDrawnCoast && Model.HasDrawnAnyCoast())
	{
		OnFirstCoastDrawn.Broadcast();
	}
	for (int32 Island = 0; Island < ChartedIslands.Num(); ++Island)
	{
		if (ChartedIslands[Island] == 0 && Model.IsIslandCharted(Island))
		{
			ChartedIslands[Island] = 1;
			OnIslandCharted.Broadcast(Island);
		}
	}
}

bool UCartographyComponent::AddMarkHere(FName StampId, const FString& Text)
{
	return Model.AddMark(StampId, GetOwnerPositionMeters(), Text) != INDEX_NONE;
}

bool UCartographyComponent::AddSpyglassMark(FName StampId, FVector TargetLocation, const FString& Text)
{
	const FVector2D Target = FVector2D(TargetLocation.X, TargetLocation.Y) / 100.0;
	return Model.AddSpyglassMark(StampId, GetOwnerPositionMeters(), Target, Text) != INDEX_NONE;
}

bool UCartographyComponent::AddSextantMarkHere(FName StampId, const FString& Text)
{
	return Model.AddSextantMark(StampId, GetOwnerPositionMeters(), Text) != INDEX_NONE;
}

int32 UCartographyComponent::SketchFromViewpoint(float RangeMeters)
{
	// TODO(P-UI): llamarlo al interactuar con un mirador (EPoiType::Viewpoint).
	const FVector2D Here = GetOwnerPositionMeters();
	const FVector2D HereOnMap = FCartographyModel::WorldToMap(Here);
	const double Range = FCartographyModel::MetersToMap(RangeMeters);
	int32 Sketched = 0;
	// Copia: AddSketch modifica el estado del modelo mientras se recorren las costas.
	const TArray<FMapIslandCoverage> Coasts = Model.GetState().Coverage;
	for (const FMapIslandCoverage& Entry : Coasts)
	{
		bool bInSight = false;
		for (const FVector2D& P : Entry.Coast)
		{
			if (FVector2D::Distance(P, HereOnMap) <= Range)
			{
				bInSight = true;
				break;
			}
		}
		if (!bInSight)
		{
			continue;
		}
		TArray<FVector2D> Outline;
		Outline.Reserve(Entry.Coast.Num());
		for (const FVector2D& P : Entry.Coast)
		{
			Outline.Add(FCartographyModel::MapToWorld(P));
		}
		Sketched += Model.AddSketch(Entry.IslandIndex, Outline) != INDEX_NONE ? 1 : 0;
	}
	return Sketched;
}
