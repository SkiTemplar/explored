#include "WorldGen/ExploredGrassField.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"

#include "Explored.h"
#include "Weather/ExploredWeatherSubsystem.h"
#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GroundField.h"
#include "WorldGen/GroundHeight.h"
#include "WorldGen/IslandDecorBuilder.h"
#include "WorldGen/IslandDecorModel.h"
#include "WorldGen/IslandLayoutLoader.h"
#include "WorldGen/IslandPropsLoader.h"

namespace GrassFieldActorDetail
{
	constexpr double MetersToCm = 100.0;
	/** Un salto de cámara mayor que esto en un frame es un teletransporte: se vuelve a medir el relleno. */
	constexpr double JumpM = 30.0;
	/** Datos por instancia del material: rango de adelgazamiento y variación del parche. */
	constexpr int32 CustomFloats = 2;
	const FName GrassTag(TEXT("IslandGrass"));
	const FName WindSway(TEXT("WindSway"));
	const FName WindStrength(TEXT("WindStrength"));
	const FName WindFadeStart(TEXT("WindFadeStartM"));
	const FName WindFadeEnd(TEXT("WindFadeEndM"));
	/** Ranura libre: diminuta y muy por debajo del mar, fuera de cualquier vista. */
	const FTransform Hidden(FQuat::Identity, FVector(0.0, 0.0, -1.0e6), FVector(1.0e-4));
	/** Rango de las ranuras libres: el material las deja a 0 aunque se vieran. */
	constexpr float HiddenRank = 2.0f;

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

	/** Mallas que alguna mezcla usa: las demás no necesitan componente. */
	bool IsKindUsed(const FGrassConfig& Config, int32 Kind)
	{
		for (const FGrassMix& Mix : Config.Mixes)
		{
			if (Mix.Weights[Kind] > 0.0f)
			{
				return true;
			}
		}
		return false;
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

	// Relee el campo compartido: el auditor, el bot y las capturas usan la misma altura que el césped.
	const TSharedPtr<const FGroundField> Ground = ExploredGround::ReloadShared(Error);
	if (!Ground)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] ground_field.bin no se puede usar (%s): la isla queda sin césped (uv run island-groundfield en Tools/Island)"), *Error);
		return false;
	}

	TSharedPtr<FGrassWorld> World = MakeShared<FGrassWorld>();
	World->Ground = Ground;
	World->Layout = *ExploredIsland::Get();
	const TSharedRef<const FIslandProps> Props = ExploredIslandProps::Get();
	World->Exclusions = FGrassWorld::ExclusionsFrom(*Props, World->Layout, Config);
	World->Palms = FGrassWorld::PalmsFrom(*Props);
	GrassFieldActorDetail::AddDecorPalms(World->Palms);

	CreateComponents(Assets, Config);
	ApplyThinningParameters(Assets, Config.Thinning);
	Model = MakeUnique<FGrassFieldModel>(Config, World);
	ResetSeedStats();
	UE_LOG(LogExplored, Display, TEXT("[Césped] %d × %d vértices de suelo, %d exclusiones, %d palmeras, denso hasta %.0f m y adelgazado hasta %.0f m (%d niveles)"),
		Ground->GetNx(), Ground->GetNy(), World->Exclusions.Num(), World->Palms.Num(), Config.Thinning.DenseM,
		Config.Thinning.EndM, Config.Thinning.LevelCount());
	return true;
}

void AExploredGrassField::CreateComponents(const FGrassAssets& Assets, const FGrassConfig& Config)
{
	using namespace GrassFieldActorDetail;
	ClearComponents();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Assets.Material);
	if (!Material)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] falta %s: se usa %s (hojas de una cara, sin viento ni adelgazamiento)"), *Assets.Material, *Assets.FallbackMaterial);
		Material = LoadObject<UMaterialInterface>(nullptr, *Assets.FallbackMaterial);
	}
	UMaterialInstanceDynamic* Mid = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
	if (Mid)
	{
		Mid->SetScalarParameterValue(WindSway, 1.0f);
		Mid->SetScalarParameterValue(WindFadeStart, Assets.WindFadeStartM);
		Mid->SetScalarParameterValue(WindFadeEnd, Assets.WindFadeEndM);
		Materials.Add(Mid);
	}
	// Más allá de EndM el material ya deja cada parche a 0; el corte del componente solo evita dibujarlos.
	const int32 CullCm = FMath::RoundToInt32((Config.Thinning.EndM + 2.0f) * MetersToCm);
	for (int32 K = 0; K < GrassKindCount; ++K)
	{
		TrisPerKind[K] = 0;
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		KindComponents.Add(Ism);
		if (!IsKindUsed(Config, K))
		{
			continue;  // ranura vacía: el modelo nunca escribe en ella
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Assets.Meshes[K]);
		if (!Mesh)
		{
			UE_LOG(LogExplored, Warning, TEXT("[Césped] falta %s: se usa %s"), *Assets.Meshes[K], *Assets.FallbackMesh);
			Mesh = LoadObject<UStaticMesh>(nullptr, *Assets.FallbackMesh);
		}
		if (Mesh)
		{
			Ism->SetStaticMesh(Mesh);
			TrisPerKind[K] = Mesh->GetNumTriangles(0);
		}
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCollisionResponseToAllChannels(ECR_Ignore);
		Ism->SetGenerateOverlapEvents(false);
		Ism->SetCanEverAffectNavigation(false);
		Ism->SetCastShadow(Assets.bCastShadow[K]);
		Ism->bAffectDistanceFieldLighting = false;
		Ism->bAffectDynamicIndirectLighting = false;
		Ism->SetReceivesDecals(false);
		Ism->SetNumCustomDataFloats(CustomFloats);
		Ism->SetCullDistances(CullCm, CullCm);
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
	}
}

void AExploredGrassField::ApplyThinningParameters(const FGrassAssets& Assets, const FGrassThinning& Thinning)
{
	UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *Assets.ParameterCollection);
	if (!Collection)
	{
		UE_LOG(LogExplored, Warning, TEXT("[Césped] falta %s: el material usa sus valores por defecto del adelgazamiento"), *Assets.ParameterCollection);
		return;
	}
	const TPair<const TCHAR*, float> Params[] = {
		{TEXT("GrassDenseM"), Thinning.DenseM}, {TEXT("GrassEndM"), Thinning.EndM},
		{TEXT("GrassEndFadeM"), Thinning.EndFadeM}, {TEXT("GrassExponent"), Thinning.Exponent},
		{TEXT("GrassMaxGrow"), Thinning.MaxGrow}, {TEXT("GrassGrowZ"), Thinning.GrowZ},
		{TEXT("GrassBand"), Thinning.Band},
	};
	for (const TPair<const TCHAR*, float>& Param : Params)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, FName(Param.Key), Param.Value);
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

void AExploredGrassField::ResetSeedStats()
{
	SeedMsSum = 0.0;
	SeedMsMax = 0.0;
	SeedFrames = 0;
}

void AExploredGrassField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector2D Camera;
	FVector2D Forward;
	float HalfFov = 0.0f;
	if (!Model || !CameraView(Camera, Forward, HalfFov))
	{
		return;
	}
	// Un salto largo (capturas, bench) vuelve a anotar el relleno del nuevo sitio.
	if (bLoggedSettled && FVector2D::Distance(Camera, LoggedCamera) > GrassFieldActorDetail::JumpM * 2.0)
	{
		bLoggedSettled = false;
		ResetSeedStats();
	}
	const double Start = FPlatformTime::Seconds();
	const FGrassUpdate Update = Model->Update(Camera);
	if (!Update.IsEmpty())
	{
		Apply(Update);
	}
	const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
	SeedMsSum += Ms;
	SeedMsMax = FMath::Max(SeedMsMax, Ms);
	++SeedFrames;
	if (!Update.IsEmpty() && !bLoggedSettled && Model->GetPendingCellCount() == 0)
	{
		bLoggedSettled = true;
		LoggedCamera = Camera;
		UE_LOG(LogExplored, Display, TEXT("[Césped] %d parches y matas en %d celdas alrededor de (%.0f, %.0f) m; %.2f M triángulos a la vista; relleno en %d frames (media %.2f ms, máx. %.2f ms)"),
			Model->GetLiveInstanceCount(), Model->GetActiveCellCount(), Camera.X, Camera.Y,
			GetGrassTrianglesInView() / 1.0e6, SeedFrames, GetGrassSeedMs(), GetGrassSeedMaxMs());
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
	using namespace GrassFieldActorDetail;
	for (int32 K = 0; K < GrassKindCount && K < KindComponents.Num(); ++K)
	{
		UInstancedStaticMeshComponent* Ism = KindComponents[K];
		if (!IsValid(Ism) || (Update.Writes[K].IsEmpty() && Update.Hides[K].IsEmpty()))
		{
			continue;
		}
		// Sin MarkRenderStateDirty: el gestor de instancias del ISM sube solo las ranuras cambiadas.
		for (const FGrassWrite& Write : Update.Writes[K])
		{
			const FTransform T = ToTransform(Write.Instance);
			int32 Index = Write.Slot;
			if (Write.Slot < Ism->GetInstanceCount())
			{
				Ism->UpdateInstanceTransform(Write.Slot, T, true, false, true);
			}
			else
			{
				// El modelo entrega las ranuras nuevas en orden: Slot == número de instancias.
				Index = Ism->AddInstance(T, true);
			}
			const float Data[CustomFloats] = {Write.Instance.Rank, Write.Instance.Variation};
			Ism->SetCustomData(Index, MakeArrayView(Data, CustomFloats), false);
		}
		const float HiddenData[CustomFloats] = {HiddenRank, 0.0f};
		for (const int32 Slot : Update.Hides[K])
		{
			Ism->UpdateInstanceTransform(Slot, Hidden, true, false, true);
			Ism->SetCustomData(Slot, MakeArrayView(HiddenData, CustomFloats), false);
		}
	}
}

bool AExploredGrassField::CameraView(FVector2D& OutMeters, FVector2D& OutForward, float& OutHalfFovRad) const
{
	const UWorld* World = GetWorld();
	const APlayerController* Pc = World ? World->GetFirstPlayerController() : nullptr;
	if (!Pc || !Pc->PlayerCameraManager)
	{
		return false;
	}
	const FVector Cam = Pc->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = Pc->PlayerCameraManager->GetCameraRotation().Vector();
	OutMeters = FVector2D(Cam.X, Cam.Y) / GrassFieldActorDetail::MetersToCm;
	OutForward = FVector2D(Dir.X, Dir.Y).GetSafeNormal();
	OutHalfFovRad = FMath::DegreesToRadians(Pc->PlayerCameraManager->GetFOVAngle() * 0.5f);
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

int64 AExploredGrassField::GetGrassTrianglesInView() const
{
	FVector2D Camera;
	FVector2D Forward;
	float HalfFov = 0.0f;
	if (!Model || !CameraView(Camera, Forward, HalfFov))
	{
		return 0;
	}
	// El ángulo horizontal real a 16:9 es algo mayor que el FOV nominal en el borde; un 10 % de margen.
	return Model->CountTrianglesInView(Camera, Forward, FMath::Min(HalfFov * 1.1f, UE_PI), TrisPerKind);
}

void AExploredGrassField::RebuildGrass()
{
	Model.Reset();
	bLoggedSettled = false;
	ClearComponents();
	SetActorTickEnabled(GetNetMode() != NM_DedicatedServer && Setup());
}
