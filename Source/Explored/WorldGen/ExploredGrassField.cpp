#include "WorldGen/ExploredGrassField.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"

#include "Explored.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GroundField.h"
#include "WorldGen/IslandDecorBuilder.h"
#include "WorldGen/IslandDecorModel.h"
#include "WorldGen/IslandLayoutLoader.h"
#include "WorldGen/IslandPropsLoader.h"

namespace GrassFieldActorDetail
{
	constexpr double MetersToCm = 100.0;
	const FName GrassTag(TEXT("IslandGrass"));
	const FName FadeStart(TEXT("FadeStartCm"));
	const FName FadeEnd(TEXT("FadeEndCm"));
	const FName WindSway(TEXT("WindSway"));
	const FName WindStrength(TEXT("WindStrength"));
	/** Ranura libre: diminuta y muy por debajo del mar, fuera de cualquier vista. */
	const FTransform Hidden(FQuat::Identity, FVector(0.0, 0.0, -1.0e6), FVector(1.0e-4));

	FTransform ToTransform(const FGrassInstance& Inst)
	{
		const FVector Up(Inst.Up.X, Inst.Up.Y, Inst.Up.Z);
		const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(static_cast<double>(Inst.YawDeg)));
		return FTransform(Tilt * Yaw, Inst.Location * MetersToCm, FVector(Inst.Scale));
	}

	/** Palmeras decorativas de island_decor.json (corona de helechos). */
	void AddDecorPalms(TArray<FVector2D>& Palms)
	{
		FIslandDecor Decor;
		FString Error;
		if (!ExploredIslandDecor::Load(ExploredIslandDecor::DefaultPath(), Decor, Error))
		{
			return;
		}
		for (const FIslandDecorInstance& Inst : Decor.Instances)
		{
			if (Decor.Meshes.IsValidIndex(Inst.Mesh) && Decor.Meshes[Inst.Mesh].Contains(TEXT("SM_Palm")))
			{
				Palms.Add(FVector2D(Inst.Location.X, Inst.Location.Y));
			}
		}
	}
}

AExploredGrassField::AExploredGrassField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

AExploredGrassField::~AExploredGrassField() = default;

void AExploredGrassField::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	SetActorTickEnabled(Setup());
}

void AExploredGrassField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Model.Reset();
	Super::EndPlay(EndPlayReason);
}

bool AExploredGrassField::Setup()
{
	FGrassConfig Config = FGrassConfig::Defaults();
	FGrassAssets Assets = FGrassAssets::Defaults();
	FString Json;
	FString Error;
	if (!FFileHelper::LoadFileToString(Json, *ExploredGrass::GrassJsonPath()))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] falta grass.json: se usan los valores por defecto"));
	}
	else if (!ExploredGrass::ParseGrassJson(Json, Config, Assets, Error))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] grass.json no válido (%s): se usan los valores por defecto"), *Error);
		Config = FGrassConfig::Defaults();
		Assets = FGrassAssets::Defaults();
	}

	TArray<uint8> Bytes;
	TSharedPtr<FGroundField> Ground = MakeShared<FGroundField>();
	if (!FFileHelper::LoadFileToArray(Bytes, *ExploredGrass::GroundFieldPath()) || !Ground->Parse(Bytes, Error))
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] ground_field.bin no se puede usar (%s): la isla queda sin césped (uv run island-groundfield en Tools/Island)"),
			Bytes.IsEmpty() ? TEXT("no existe") : *Error);
		return false;
	}

	TSharedPtr<FGrassWorld> World = MakeShared<FGrassWorld>();
	World->Ground = Ground;
	World->Layout = *ExploredIsland::Get();
	const TSharedRef<const FIslandProps> Props = ExploredIslandProps::Get();
	World->Exclusions = FGrassWorld::ExclusionsFrom(*Props, World->Layout, Config);
	World->Palms = FGrassWorld::PalmsFrom(*Props);
	GrassFieldActorDetail::AddDecorPalms(World->Palms);

	CreateComponents(Assets);
	Model = MakeUnique<FGrassFieldModel>(Config, World);
	UE_LOG(LogExplored, Display, TEXT("[Césped] %d × %d vértices de suelo, %d exclusiones, %d palmeras, radio %.0f m"),
		Ground->GetNx(), Ground->GetNy(), World->Exclusions.Num(), World->Palms.Num(), Config.RadiusM);
	return true;
}

void AExploredGrassField::CreateComponents(const FGrassAssets& Assets)
{
	using namespace GrassFieldActorDetail;
	ClearComponents();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Assets.Material);
	if (!Material)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] falta %s: se usa %s (hojas de una cara, sin viento por vértice)"), *Assets.Material, *Assets.FallbackMaterial);
		Material = LoadObject<UMaterialInterface>(nullptr, *Assets.FallbackMaterial);
	}
	UMaterialInstanceDynamic* Mid = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	if (Mid)
	{
		Mid->SetScalarParameterValue(FadeStart, Assets.FadeStartM * MetersToCm);
		Mid->SetScalarParameterValue(FadeEnd, Assets.FadeEndM * MetersToCm);
		Mid->SetScalarParameterValue(WindSway, 1.0f);
		Materials.Add(Mid);
	}
	for (int32 K = 0; K < GrassKindCount; ++K)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Assets.Meshes[K]);
		if (!Mesh)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Césped] falta %s: se usa %s"), *Assets.Meshes[K], *Assets.FallbackMesh);
			Mesh = LoadObject<UStaticMesh>(nullptr, *Assets.FallbackMesh);
		}
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		if (Mesh)
		{
			Ism->SetStaticMesh(Mesh);
		}
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCollisionResponseToAllChannels(ECR_Ignore);
		Ism->SetGenerateOverlapEvents(false);
		Ism->SetCanEverAffectNavigation(false);
		Ism->SetCastShadow(Assets.bCastShadow[K]);
		Ism->bAffectDistanceFieldLighting = false;
		Ism->SetReceivesDecals(false);
		Ism->SetCullDistances(FMath::RoundToInt32(Assets.FadeStartM * MetersToCm), FMath::RoundToInt32(Assets.FadeEndM * MetersToCm));
		Ism->ComponentTags.Add(GrassTag);
		if (Mid && Mesh)
		{
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Ism->SetMaterial(Slot, Mid);
			}
		}
		Ism->SetupAttachment(RootComponent);
		Ism->SetWorldTransform(FTransform::Identity);
		Ism->RegisterComponent();
		AddInstanceComponent(Ism);
		KindComponents.Add(Ism);
	}
}

void AExploredGrassField::ClearComponents()
{
	for (UInstancedStaticMeshComponent* Ism : KindComponents)
	{
		if (IsValid(Ism))
		{
			Ism->DestroyComponent();
		}
	}
	KindComponents.Reset();
	Materials.Reset();
}

void AExploredGrassField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector2D Camera;
	if (!Model || !CameraMeters(Camera))
	{
		return;
	}
	const FGrassUpdate Update = Model->Update(Camera);
	if (!Update.IsEmpty())
	{
		Apply(Update);
		if (!bLoggedSettled && Model->GetPendingCellCount() == 0)
		{
			bLoggedSettled = true;
			UE_LOG(LogExplored, Display, TEXT("[Césped] %d matas en %d celdas alrededor de (%.0f, %.0f) m"),
				Model->GetLiveInstanceCount(), Model->GetActiveCellCount(), Camera.X, Camera.Y);
		}
	}
	WindTimer -= DeltaSeconds;
	if (WindTimer <= 0.0f)
	{
		WindTimer = WindRefreshSeconds;
		RefreshWind();
	}
}

void AExploredGrassField::Apply(const FGrassUpdate& Update)
{
	for (int32 K = 0; K < GrassKindCount && K < KindComponents.Num(); ++K)
	{
		UInstancedStaticMeshComponent* Ism = KindComponents[K];
		if (!IsValid(Ism) || (Update.Writes[K].IsEmpty() && Update.Hides[K].IsEmpty()))
		{
			continue;
		}
		for (const FGrassWrite& Write : Update.Writes[K])
		{
			const FTransform T = GrassFieldActorDetail::ToTransform(Write.Instance);
			if (Write.Slot < Ism->GetInstanceCount())
			{
				Ism->UpdateInstanceTransform(Write.Slot, T, true, false, true);
			}
			else
			{
				// El modelo entrega las ranuras nuevas en orden: Slot == número de instancias.
				Ism->AddInstance(T, true);
			}
		}
		for (const int32 Slot : Update.Hides[K])
		{
			Ism->UpdateInstanceTransform(Slot, GrassFieldActorDetail::Hidden, true, false, true);
		}
		Ism->MarkRenderStateDirty();
	}
}

bool AExploredGrassField::CameraMeters(FVector2D& Out) const
{
	const UWorld* World = GetWorld();
	const APlayerController* Pc = World ? World->GetFirstPlayerController() : nullptr;
	if (!Pc || !Pc->PlayerCameraManager)
	{
		return false;
	}
	const FVector Cam = Pc->PlayerCameraManager->GetCameraLocation();
	Out = FVector2D(Cam.X, Cam.Y) / GrassFieldActorDetail::MetersToCm;
	return true;
}

void AExploredGrassField::RefreshWind()
{
	const UExploredWeatherSubsystem* Weather = GetWorld() ? GetWorld()->GetSubsystem<UExploredWeatherSubsystem>() : nullptr;
	const float Wind = Weather ? Weather->GetCurrent().Wind : 0.2f;
	for (UMaterialInstanceDynamic* Mid : Materials)
	{
		if (Mid)
		{
			Mid->SetScalarParameterValue(GrassFieldActorDetail::WindStrength, Wind);
		}
	}
}

int32 AExploredGrassField::GetGrassInstanceCount() const
{
	return Model ? Model->GetLiveInstanceCount() : 0;
}

int32 AExploredGrassField::GetGrassCellCount() const
{
	return Model ? Model->GetActiveCellCount() : 0;
}

void AExploredGrassField::RebuildGrass()
{
	Model.Reset();
	bLoggedSettled = false;
	ClearComponents();
	SetActorTickEnabled(GetNetMode() != NM_DedicatedServer && Setup());
}
