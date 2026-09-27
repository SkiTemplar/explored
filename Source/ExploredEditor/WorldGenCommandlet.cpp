#include "WorldGenCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Factories/WorldFactory.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"

#include "Explored.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/BeachDebrisModel.h"
#include "WorldGen/FormationPlacementModel.h"
#include "WorldGen/PointsOfInterest.h"
#include "WorldGen/TerrainChunkBuilder.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/ExploredVegetationCell.h"
#include "WorldGen/VegetationScatter.h"

namespace
{
	constexpr uint32 DefaultSeed = FArchipelagoLayout::OfficialSeed;
	const TCHAR* TerrainFolder = TEXT("/Game/World/Terrain");
	const TCHAR* MapPath = TEXT("/Game/Maps/Archipelago");
	const TCHAR* TerrainMaterialPath = TEXT("/Game/Materials/M_Terrain.M_Terrain");
	const FName TerrainTag(TEXT("ExploredTerrain"));

	/** Profundidad a partir de la cual el fondo se representa con la malla gruesa. */
	constexpr float DeepFloorThreshold = -32.0f;

	bool SavePackageToDisk(UPackage* Package, UObject* Asset, bool bIsMap)
	{
		const FString Extension = bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), Extension);
		FSavePackageArgs Args;
		Args.TopLevelFlags = bIsMap ? RF_NoFlags : (RF_Public | RF_Standalone);
		Args.Error = GError;
		Args.SaveFlags = SAVE_NoError;
		// Se borra el fichero anterior: sobrescribir un mapa existente desde un paquete nuevo falla.
		if (bIsMap)
		{
			IFileManager::Get().Delete(*Filename, false, true, true);
		}
		const FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *Filename, Args);
		if (Result.Result != ESavePackageResult::Success)
		{
			UE_LOG(LogExplored, Error, TEXT("No se pudo guardar %s (código %d)"), *Filename, static_cast<int32>(Result.Result));
			return false;
		}
		return true;
	}

	/**
	 * Convierte todos los actores del nivel a paquetes externos (One File Per Actor) y los guarda
	 * uno a uno. `bUseExternalActors=true` en el nivel solo afecta a como se comportan los actores
	 * que se creen desde ahora (no re-empaqueta los que ya existen) y, aunque lo hiciera, guardar
	 * el paquete del mapa NO guarda los paquetes de los actores externos: cada uno vive en su
	 * propio UPackage y hay que guardarlo aparte, o el mapa se queda sin terreno ni vegetación al
	 * volver a abrirlo. Mismo patrón que UWorldPartitionConvertCommandlet::PrepareStreamingLevelForConversion.
	 */
	bool SaveExternalActorPackages(UWorld* World)
	{
		World->PersistentLevel->ConvertAllActorsToPackaging(true);

		int32 TotalActors = 0;
		TArray<AActor*> ExternalActors;
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (!Actor)
			{
				continue;
			}
			++TotalActors;
			if (Actor->IsPackageExternal())
			{
				Actor->MarkPackageDirty();
				ExternalActors.Add(Actor);
			}
		}
		UE_LOG(LogExplored, Display, TEXT("OFPA: %d/%d actores externalizados"), ExternalActors.Num(), TotalActors);

		// El actor se pasa como Base: los actores no llevan RF_Standalone, así que filtrar por
		// TopLevelFlags=RF_Standalone sin Base deja el paquete sin objeto raíz y el guardado falla
		// (ESavePackageResult::Error) sin decir por qué. Guardado síncrono y sin SAVE_NoError para
		// que cualquier fallo real llegue al log.
		int32 SaveFailures = 0;
		for (AActor* Actor : ExternalActors)
		{
			UPackage* ActorPackage = Actor->GetExternalPackage();
			const FString Filename = FPackageName::LongPackageNameToFilename(ActorPackage->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_NoFlags;
			Args.SaveFlags = SAVE_None;
			Args.Error = GWarn;
			const FSavePackageResultStruct Result = UPackage::Save(ActorPackage, Actor, *Filename, Args);
			if (Result.Result != ESavePackageResult::Success)
			{
				++SaveFailures;
				if (SaveFailures <= 10)
				{
					UE_LOG(LogExplored, Error, TEXT("No se pudo guardar el actor externo %s (%s, código %d)"),
						*Filename, *Actor->GetActorLabel(), static_cast<int32>(Result.Result));
				}
			}
		}
		UE_LOG(LogExplored, Display, TEXT("Guardados %d paquetes de actores externos (OFPA), %d fallos"),
			ExternalActors.Num() - SaveFailures, SaveFailures);
		return SaveFailures == 0;
	}

	// ------------------------------------------------------------------
	// World Partition: grids de streaming y capas HLOD.
	// ------------------------------------------------------------------

	const TCHAR* TerrainGridName = TEXT("MainGrid");
	const TCHAR* VegetationGridName = TEXT("VegetationGrid");
	const TCHAR* HLODFolder = TEXT("/Game/World/HLOD");

	// Celdas de 384 m (dentro de los 256-512 m pedidos) y 1,2 km de rango de carga: con un
	// archipiélago de 6x6 km (WorldHalfExtent=3000 m) da una rejilla de ~16x16 celdas.
	constexpr int32 TerrainCellSizeCm = 38400;
	constexpr double TerrainLoadingRangeCm = 120000.0;

	// 512 m: mismo tamaño que las celdas de instancias que ya usa SpawnVegetation (CellSizeCm de
	// abajo), así una celda de streaming cubre exactamente un AExploredVegetationCell. Rango de
	// carga algo mayor que el del terreno: los HISM tardan un fotograma en construir el árbol de
	// culling tras transmitirse.
	constexpr int32 VegetationCellSizeCm = 51200;
	constexpr double VegetationLoadingRangeCm = 140000.0;

	// El HLOD debe recoger el testigo justo donde termina el rango de carga del grid base (si
	// fuera menor, habría un hueco sin terreno fino NI HLOD). Terreno: una única celda HLOD mayor
	// que el mundo entero (silueta de todas las islas siempre cargada desde cualquier punto,
	// incluida la cámara del menú, que orbita a 3 km de radio). Vegetación: HLOD por instancing
	// hasta 3 km; más allá, la silueta del terreno ya lleva el peso del horizonte.
	constexpr int32 TerrainHLODCellSizeCm = 640000;
	constexpr double TerrainHLODLoadingRangeCm = 640000.0;
	constexpr int32 VegetationHLODCellSizeCm = 102400;
	constexpr double VegetationHLODLoadingRangeCm = 300000.0;

	/** Radio alrededor del punto de aparición que se mantiene siempre cargado
	 * (bIsSpatiallyLoaded=false): el jugador aparece ahí antes de que exista ninguna fuente de
	 * streaming que dispare la celda, así que no puede depender de ella. */
	constexpr double LandingSafetyRadiusCm = 40000.0;

	/**
	 * Accede a una UPROPERTY aunque sea C++ private (Grids de UWorldPartitionRuntimeSpatialHash,
	 * CellSize/LoadingRange de UHLODLayer no tienen setter público fuera del editor de detalles).
	 * La reflexión de Unreal no comprueba visibilidad de C++, solo el offset real en memoria del
	 * UPROPERTY, así que esto es seguro siempre que el nombre y el tipo coincidan exactamente con
	 * los del header del motor.
	 */
	template <typename T>
	T& AccessPrivateProperty(UObject* Object, const TCHAR* PropertyName)
	{
		FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		checkf(Property, TEXT("%s no tiene la propiedad %s"), *Object->GetClass()->GetName(), PropertyName);
		return *Property->ContainerPtrToValuePtr<T>(Object);
	}

	/** Crea (o reutiliza) una capa HLOD guardada en disco con el tipo y el grid indicados. */
	UHLODLayer* CreateHLODLayer(const FString& Name, EHLODLayerType LayerType, int32 CellSize, double LoadingRange)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), HLODFolder, *Name);
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();

		UHLODLayer* Layer = NewObject<UHLODLayer>(Package, *Name, RF_Public | RF_Standalone);
		Layer->SetLayerType(LayerType);
		Layer->SetIsSpatiallyLoaded(true);
		AccessPrivateProperty<int32>(Layer, TEXT("CellSize")) = CellSize;
		AccessPrivateProperty<double>(Layer, TEXT("LoadingRange")) = LoadingRange;

		// HLODBuilderSettings (el objeto que de verdad decide cómo fusiona/instancia el HLOD) lo crea
		// UHLODLayer::PostLoad() según LayerType, pero solo se dispara al cargar de disco y además es
		// privado. Aquí el asset se configura y guarda en la misma pasada sin recargarlo, así que se
		// llama a mano al mismo módulo que usa ese PostLoad() por debajo.
		if (IWorldPartitionHLODUtilitiesModule* HLODUtilitiesModule =
			FModuleManager::Get().LoadModulePtr<IWorldPartitionHLODUtilitiesModule>(TEXT("WorldPartitionHLODUtilities")))
		{
			if (IWorldPartitionHLODUtilities* HLODUtilities = HLODUtilitiesModule->GetUtilities())
			{
				AccessPrivateProperty<TObjectPtr<UHLODBuilderSettings>>(Layer, TEXT("HLODBuilderSettings")) =
					HLODUtilities->CreateHLODBuilderSettings(Layer);
			}
		}
		else
		{
			UE_LOG(LogExplored, Warning, TEXT("No se pudo cargar WorldPartitionHLODUtilities: %s sin HLODBuilderSettings explícito"), *Name);
		}

		Layer->MarkPackageDirty();
		SavePackageToDisk(Package, Layer, false);
		return Layer;
	}

	/**
	 * Configura el mapa como World Partition: grid de terreno (MainGrid) y de vegetación
	 * (VegetationGrid) con celdas y rango de carga acordes al tamaño del archipiélago, más sus
	 * capas HLOD (terreno fusionado/simplificado, vegetación por instancing) para que el
	 * horizonte no desaparezca cuando esas celdas se descargan.
	 */
	bool SetupWorldPartition(UWorld* World)
	{
		// No se deja que UWorldFactory cree el World Partition (Factory->bCreateWorldPartition):
		// en este motor/proyecto el UWorldPartitionRuntimeHash por defecto es
		// UWorldPartitionRuntimeHashSet (el nuevo esquema orientado a capas HLOD, UE 5.4+), no
		// UWorldPartitionRuntimeSpatialHash (grid clásico de Grids[]/CellSize/LoadingRange) sobre
		// el que está escrito el resto de esta función. Se crea aquí a mano, pidiendo la clase
		// clásica explícitamente.
		World->PersistentLevel->bUseExternalActors = true;
		UWorldPartition* Partition = UWorldPartition::CreateOrRepairWorldPartition(
			World->GetWorldSettings(), nullptr, UWorldPartitionRuntimeSpatialHash::StaticClass());
		if (!Partition)
		{
			UE_LOG(LogExplored, Error, TEXT("CreateOrRepairWorldPartition no devolvió World Partition"));
			return false;
		}
		Partition->bEnableStreaming = true;

		UWorldPartitionRuntimeSpatialHash* RuntimeHash = Cast<UWorldPartitionRuntimeSpatialHash>(Partition->RuntimeHash);
		if (!RuntimeHash)
		{
			UE_LOG(LogExplored, Error, TEXT("RuntimeHash no es UWorldPartitionRuntimeSpatialHash (clase %s)"),
				Partition->RuntimeHash ? *Partition->RuntimeHash->GetClass()->GetName() : TEXT("null"));
			return false;
		}

		UHLODLayer* HLODTerrain = CreateHLODLayer(TEXT("HLOD_Terrain"), EHLODLayerType::MeshMerge,
			TerrainHLODCellSizeCm, TerrainHLODLoadingRangeCm);
		UHLODLayer* HLODVegetation = CreateHLODLayer(TEXT("HLOD_Vegetation"), EHLODLayerType::Instancing,
			VegetationHLODCellSizeCm, VegetationHLODLoadingRangeCm);

		FSpatialHashRuntimeGrid MainGrid;
		MainGrid.GridName = FName(TerrainGridName);
		MainGrid.CellSize = TerrainCellSizeCm;
		MainGrid.LoadingRange = TerrainLoadingRangeCm;
		MainGrid.DebugColor = FLinearColor(0.2f, 0.55f, 0.2f);
		MainGrid.HLODLayer = HLODTerrain;

		FSpatialHashRuntimeGrid VegetationGrid;
		VegetationGrid.GridName = FName(VegetationGridName);
		VegetationGrid.CellSize = VegetationCellSizeCm;
		VegetationGrid.LoadingRange = VegetationLoadingRangeCm;
		VegetationGrid.DebugColor = FLinearColor(0.1f, 0.4f, 0.1f);
		VegetationGrid.HLODLayer = HLODVegetation;

		AccessPrivateProperty<TArray<FSpatialHashRuntimeGrid>>(RuntimeHash, TEXT("Grids")) = {MainGrid, VegetationGrid};
		Partition->SetDefaultHLODLayer(HLODTerrain);

		// GetNumGrids() cuenta StreamingGrids (se genera al transmitir/cocinar, todavía vacío aquí),
		// no Grids; se relee la propiedad de configuración para confirmar que el array de arriba
		// cuajó de verdad.
		const int32 ConfiguredGrids = AccessPrivateProperty<TArray<FSpatialHashRuntimeGrid>>(RuntimeHash, TEXT("Grids")).Num();
		UE_LOG(LogExplored, Display,
			TEXT("World Partition: %s cell=%dm range=%.0fm · %s cell=%dm range=%.0fm (Grids configurados=%d)"),
			TerrainGridName, TerrainCellSizeCm / 100, TerrainLoadingRangeCm / 100.0,
			VegetationGridName, VegetationCellSizeCm / 100, VegetationLoadingRangeCm / 100.0,
			ConfiguredGrids);
		return true;
	}

	// ------------------------------------------------------------------
	// Vista previa cenital
	// ------------------------------------------------------------------

	int32 RunPreview(const FTerrainDensity& Density, int32 Size, const FString& OutPath)
	{
		const float Extent = FArchipelagoLayout::WorldHalfExtent;
		const float Step = 2.0f * Extent / Size;
		TArray<FColor> Pixels;
		Pixels.SetNumZeroed(Size * Size);

		ParallelFor(Size, [&](int32 Row)
		{
			const float Y = Extent - (Row + 0.5f) * Step;
			for (int32 Col = 0; Col < Size; ++Col)
			{
				const float X = -Extent + (Col + 0.5f) * Step;
				const float H = Density.SampleColumn(X, Y).Height;
				const float Hx = Density.SampleColumn(X + Step, Y).Height;
				const float Hy = Density.SampleColumn(X, Y + Step).Height;
				const FVector Normal = FVector(-(Hx - H), -(Hy - H), Step).GetSafeNormal();
				FLinearColor Color = Density.SurfaceColor(FVector(X, Y, H), Normal);

				// Sombreado de relieve con luz del noroeste.
				const float Light = FMath::Clamp(FVector::DotProduct(Normal, FVector(-0.5f, 0.5f, 0.7f).GetSafeNormal()), 0.0f, 1.0f);
				Color *= 0.45f + 0.75f * Light;

				if (H < 0.0f)
				{
					const float Depth = FMath::Clamp(-H / 30.0f, 0.0f, 1.0f);
					const FLinearColor Shallow(FColor(64, 214, 208));
					const FLinearColor Deep(FColor(12, 52, 110));
					const FLinearColor Water = FMath::Lerp(Shallow, Deep, FMath::Pow(Depth, 0.6f));
					Color = FMath::Lerp(Color, Water, FMath::Clamp(0.55f + Depth * 0.45f, 0.0f, 1.0f));
				}
				Color.A = 1.0f;
				Pixels[Row * Size + Col] = Color.ToFColor(true);
			}
		});

		// Marca los centros de las islas.
		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			const int32 Cx = FMath::RoundToInt32((Island.Center.X + Extent) / Step);
			const int32 Cy = FMath::RoundToInt32((Extent - Island.Center.Y) / Step);
			for (int32 D = -3; D <= 3; ++D)
			{
				for (const FIntPoint P : {FIntPoint(Cx + D, Cy), FIntPoint(Cx, Cy + D)})
				{
					if (P.X >= 0 && P.X < Size && P.Y >= 0 && P.Y < Size)
					{
						Pixels[P.Y * Size + P.X] = FColor::Red;
					}
				}
			}
		}

		IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> Png = ImageModule.CreateImageWrapper(EImageFormat::PNG);
		Png->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Size, Size, ERGBFormat::BGRA, 8);
		const TArray64<uint8>& Compressed = Png->GetCompressed();
		if (!FFileHelper::SaveArrayToFile(Compressed, *OutPath))
		{
			UE_LOG(LogExplored, Error, TEXT("No se pudo escribir %s"), *OutPath);
			return 1;
		}
		UE_LOG(LogExplored, Display, TEXT("Vista previa escrita en %s"), *OutPath);
		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			UE_LOG(LogExplored, Display, TEXT("  %-10s centro (%.0f, %.0f) radio %.0f altura %.0f"),
				LexToString(Island.Archetype), Island.Center.X, Island.Center.Y, Island.Radius, Island.MaxHeight);
		}
		return 0;
	}

	// ------------------------------------------------------------------
	// Horneado
	// ------------------------------------------------------------------

	FMeshDescription ToMeshDescription(const FTerrainMeshData& Mesh, const FVector& ChunkOriginMeters)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();

		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		// UV0: coordenadas de mundo; UV1 y UV2: pesos de capa (arena, hojarasca) y (roca, volcánico).
		UVs.SetNumChannels(3);
		Attributes.GetPolygonGroupMaterialSlotNames();

		Description.ReserveNewVertices(Mesh.Positions.Num());
		Description.ReserveNewVertexInstances(Mesh.Positions.Num());
		Description.ReserveNewTriangles(Mesh.NumTriangles());

		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = FName(TEXT("Terrain"));

		TArray<FVertexInstanceID> Instances;
		Instances.Reserve(Mesh.Positions.Num());
		for (int32 I = 0; I < Mesh.Positions.Num(); ++I)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = Mesh.Positions[I];
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			Normals[Instance] = Mesh.Normals[I];
			const FLinearColor& C = Mesh.Colors[I];
			Colors[Instance] = FVector4f(C.R, C.G, C.B, C.A);
			// UV en metros del mundo / 4 para texturas de detalle.
			const FVector3f WorldMeters = Mesh.Positions[I] / 100.0f + FVector3f(ChunkOriginMeters);
			UVs.Set(Instance, 0, FVector2f(WorldMeters.X, WorldMeters.Y) / 4.0f);
			const FVector4f L = Mesh.Layers.IsValidIndex(I) ? Mesh.Layers[I] : FVector4f(0.0f, 0.0f, C.A, 0.5f);
			UVs.Set(Instance, 1, FVector2f(L.X, L.Y));
			UVs.Set(Instance, 2, FVector2f(L.Z, L.W));
			Instances.Add(Instance);
		}

		for (int32 T = 0; T < Mesh.Indices.Num(); T += 3)
		{
			const FVertexInstanceID Tri[3] = {
				Instances[Mesh.Indices[T]], Instances[Mesh.Indices[T + 1]], Instances[Mesh.Indices[T + 2]]};
			Description.CreateTriangle(Group, Tri);
		}
		return Description;
	}

	UStaticMesh* CreateTerrainMesh(const FString& Name, FMeshDescription&& Description, UMaterialInterface* Material)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), TerrainFolder, *Name);
		UPackage* Package = CreatePackage(*PackageName);
		Package->FullyLoad();

		UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, *Name, RF_Public | RF_Standalone);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, FName(TEXT("Terrain")), FName(TEXT("Terrain"))));

		FStaticMeshSourceModel& Source = Mesh->AddSourceModel();
		Source.BuildSettings.bRecomputeNormals = false;
		Source.BuildSettings.bRecomputeTangents = true;
		Source.BuildSettings.bUseMikkTSpace = true;
		Source.BuildSettings.bGenerateLightmapUVs = false;
		Source.BuildSettings.bBuildReversedIndexBuffer = false;
		Source.BuildSettings.DistanceFieldResolutionScale = 2.0f;

		Mesh->CreateMeshDescription(0, MoveTemp(Description));
		Mesh->CommitMeshDescription(0);

		Mesh->NaniteSettings.bEnabled = true;
		Mesh->NaniteSettings.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
		Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;

		Mesh->CreateBodySetup();
		Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
		Mesh->bAllowCPUAccess = false;
		return Mesh;
	}

	/** Fondo marino profundo: malla de altura gruesa que cubre todo el mundo por debajo de los chunks finos. */
	FTerrainMeshData BuildDeepFloor(const FTerrainDensity& Density, float Spacing)
	{
		FTerrainMeshData Mesh;
		const float Extent = FArchipelagoLayout::WorldHalfExtent + 1000.0f;
		const int32 Count = FMath::CeilToInt32(2.0f * Extent / Spacing) + 1;
		Mesh.Positions.Reserve(Count * Count);
		for (int32 J = 0; J < Count; ++J)
		{
			for (int32 I = 0; I < Count; ++I)
			{
				const float X = -Extent + I * Spacing;
				const float Y = -Extent + J * Spacing;
				const float H = FMath::Min(Density.SampleColumn(X, Y).Height, DeepFloorThreshold) - 1.5f;
				const FVector World(X, Y, H);
				Mesh.Positions.Add(FVector3f(World * 100.0));
				Mesh.Normals.Add(FVector3f::UpVector);
				FLinearColor Color = Density.SurfaceColor(World, FVector::UpVector);
				Color.A = 0.0f;
				Mesh.Colors.Add(Color);
				// Fondo marino: todo arena, con el carácter volcánico de la isla más cercana.
				Mesh.Layers.Add(FVector4f(1.0f, 0.0f, 0.0f, Density.SurfaceLayers(World, FVector::UpVector).W));
			}
		}
		for (int32 J = 0; J + 1 < Count; ++J)
		{
			for (int32 I = 0; I + 1 < Count; ++I)
			{
				const uint32 A = J * Count + I;
				const uint32 B = A + 1;
				const uint32 C = A + Count;
				const uint32 D = C + 1;
				// Orientación de Unreal: (B - A) x (C - A) opuesto a la normal (+Z).
				Mesh.Indices.Append({A, C, B, B, C, D});
			}
		}
		return Mesh;
	}

	void SpawnMeshActor(UWorld* World, UStaticMesh* Mesh, const FVector& LocationCm, const FString& Label, bool bAlwaysLoaded = false)
	{
		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(World->PersistentLevel, AStaticMeshActor::StaticClass(), FName(*Label));
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(LocationCm, FRotator::ZeroRotator, Params);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		Actor->Tags.Add(TerrainTag);
		Actor->SetActorLabel(Label);
		Actor->SetFolderPath(FName(TEXT("Terrain")));
		if (bAlwaysLoaded)
		{
			Actor->SetIsSpatiallyLoaded(false);
		}
	}

	void SpawnOptional(UWorld* World, const TCHAR* ClassPath, const FVector& Location, const TCHAR* Label)
	{
		UClass* Class = StaticLoadClass(AActor::StaticClass(), nullptr, ClassPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (!Class)
		{
			UE_LOG(LogExplored, Warning, TEXT("Clase %s no disponible; se omite"), ClassPath);
			return;
		}
		AActor* Actor = World->SpawnActor<AActor>(Class, Location, FRotator::ZeroRotator);
		Actor->SetActorLabel(Label);
		// Cielo, océano y otros actores globales: siempre cargados, no dependen del streaming.
		Actor->SetIsSpatiallyLoaded(false);
	}

	FVector FindSpawnPoint(const FTerrainDensity& Density)
	{
		// Playa de la isla de inicio frente a la laguna: busca, desde el centro, el primer punto a 1,5–3 m.
		const FIslandDesc* Landing = Density.GetLayout().FindIsland(EIslandArchetype::Landing);
		check(Landing);
		const FVector2D Dir(FMath::Cos(Landing->Rotation), FMath::Sin(Landing->Rotation));
		for (float R = 0.0f; R < Landing->Radius * 1.2f; R += 2.0f)
		{
			const FVector2D P = Landing->Center - Dir * R;
			const float H = Density.SampleColumn(P.X, P.Y).Height;
			if (H > 1.5f && H < 3.0f)
			{
				return FVector(P.X, P.Y, H + 1.2f);
			}
		}
		return FVector(Landing->Center, Density.SampleColumn(Landing->Center.X, Landing->Center.Y).Height + 2.0f);
	}

	int32 RunBake(const FTerrainDensity& Density, const FTerrainChunkSettings& Settings, const FBox2D& Region)
	{
		const double StartTime = FPlatformTime::Seconds();
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TerrainMaterialPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (!Material)
		{
			UE_LOG(LogExplored, Warning, TEXT("No existe %s; se usa el material por defecto"), TerrainMaterialPath);
		}

		// 1) Chunks candidatos, descartando las columnas de mar profundo.
		TArray<FIntVector> Candidates = FTerrainChunkBuilder::FindCandidateChunks(Density, Settings, Region);
		{
			TMap<FIntPoint, float> ColumnMax;
			const float Size = Settings.ChunkSizeMeters();
			for (const FIntVector& C : Candidates)
			{
				const FIntPoint Key(C.X, C.Y);
				if (!ColumnMax.Contains(Key))
				{
					float MinH = 0.0f;
					float MaxH = 0.0f;
					const FBox2D Rect(FVector2D(C.X, C.Y) * Size, FVector2D(C.X + 1, C.Y + 1) * Size);
					Density.HeightBounds(Rect, Settings.VoxelSize * 2.0f, MinH, MaxH);
					ColumnMax.Add(Key, MaxH);
				}
			}
			Candidates.RemoveAll([&](const FIntVector& C)
			{
				return ColumnMax[FIntPoint(C.X, C.Y)] < DeepFloorThreshold + 6.0f;
			});
		}
		UE_LOG(LogExplored, Display, TEXT("Chunks candidatos: %d"), Candidates.Num());

		// 2) Poligonización en paralelo.
		TArray<FTerrainMeshData> Meshes;
		Meshes.SetNum(Candidates.Num());
		std::atomic<int32> Done = 0;
		ParallelFor(Candidates.Num(), [&](int32 I)
		{
			Meshes[I] = FTerrainChunkBuilder::Build(Density, Candidates[I], Settings);
			const int32 N = ++Done;
			if (N % 200 == 0)
			{
				UE_LOG(LogExplored, Display, TEXT("  poligonizados %d / %d"), N, Candidates.Num());
			}
		});

		// 3) Limpia la carpeta de terreno anterior, solo si se hornea el mundo entero (M10): con
		//    -region= se conservan los chunks de fuera (el mapa se compone con todos y la
		//    vegetación cubre el mundo) y los de dentro se sobrescriben por nombre.
		const float WorldExtent = FArchipelagoLayout::WorldHalfExtent;
		const bool bWholeWorld = Region.Min.X <= -WorldExtent && Region.Min.Y <= -WorldExtent &&
			Region.Max.X >= WorldExtent && Region.Max.Y >= WorldExtent;
		if (bWholeWorld)
		{
			const FString TerrainDir = FPackageName::LongPackageNameToFilename(TerrainFolder, TEXT(""));
			IFileManager::Get().DeleteDirectory(*TerrainDir, false, true);
		}
		else
		{
			UE_LOG(LogExplored, Display, TEXT("Horneado por región: se conservan los chunks de fuera de la región"));
		}

		// 4) Crea y construye las mallas.
		TArray<UStaticMesh*> Built;
		TArray<FVector> Locations;
		TArray<FString> Names;
		int64 Triangles = 0;
		for (int32 I = 0; I < Candidates.Num(); ++I)
		{
			if (Meshes[I].IsEmpty())
			{
				continue;
			}
			const FIntVector& C = Candidates[I];
			const FVector Origin = FTerrainChunkBuilder::ChunkOrigin(C, Settings);
			const FString Name = FString::Printf(TEXT("SM_Terrain_%d_%d_%d"), C.X, C.Y, C.Z);
			Triangles += Meshes[I].NumTriangles();
			Built.Add(CreateTerrainMesh(Name, ToMeshDescription(Meshes[I], Origin), Material));
			Locations.Add(Origin * 100.0);
			Names.Add(Name);
			Meshes[I] = FTerrainMeshData();
		}

		FTerrainMeshData Floor = BuildDeepFloor(Density, 25.0f);
		Triangles += Floor.NumTriangles();
		Built.Add(CreateTerrainMesh(TEXT("SM_Terrain_DeepFloor"), ToMeshDescription(Floor, FVector::ZeroVector), Material));
		Locations.Add(FVector::ZeroVector);
		Names.Add(TEXT("SM_Terrain_DeepFloor"));

		UE_LOG(LogExplored, Display, TEXT("Construyendo %d mallas (%lld triángulos)..."), Built.Num(), Triangles);
		UStaticMesh::BatchBuild(Built, true);

		for (UStaticMesh* Mesh : Built)
		{
			Mesh->MarkPackageDirty();
			SavePackageToDisk(Mesh->GetPackage(), Mesh, false);
		}

		UE_LOG(LogExplored, Display, TEXT("Terreno horneado: %d mallas, %lld triángulos, %.1f s"),
			Built.Num(), Triangles, FPlatformTime::Seconds() - StartTime);
		return 0;
	}

	// ------------------------------------------------------------------
	// Composición del mapa
	// ------------------------------------------------------------------

	struct FTerrainPiece
	{
		UStaticMesh* Mesh = nullptr;
		FVector LocationCm = FVector::ZeroVector;
	};

	/** Localiza las mallas de terreno ya horneadas y su posición a partir del nombre. */
	TArray<FTerrainPiece> FindTerrainPieces(const FTerrainChunkSettings& Settings)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({FString(TerrainFolder)}, true);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(TerrainFolder), Assets, true);

		TArray<FTerrainPiece> Pieces;
		for (const FAssetData& Asset : Assets)
		{
			const FString Name = Asset.AssetName.ToString();
			FTerrainPiece Piece;
			if (Name != TEXT("SM_Terrain_DeepFloor"))
			{
				TArray<FString> Parts;
				Name.ParseIntoArray(Parts, TEXT("_"));
				if (Parts.Num() != 5)
				{
					continue;
				}
				const FIntVector Coord(FCString::Atoi(*Parts[2]), FCString::Atoi(*Parts[3]), FCString::Atoi(*Parts[4]));
				Piece.LocationCm = FTerrainChunkBuilder::ChunkOrigin(Coord, Settings) * 100.0;
			}
			Piece.Mesh = Cast<UStaticMesh>(Asset.GetAsset());
			if (Piece.Mesh)
			{
				Pieces.Add(Piece);
			}
		}
		return Pieces;
	}

	/** Lee Art/Export/Meshes/manifest.json y asigna mallas importadas a cada regla por categoría. */
	void ResolveScatterMeshes(TArray<FScatterRule>& Rules)
	{
		const FString ManifestPath = FPaths::ProjectDir() / TEXT("Art/Export/Meshes/manifest.json");
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ManifestPath))
		{
			UE_LOG(LogExplored, Warning, TEXT("No hay manifest de mallas en %s: sin vegetación"), *ManifestPath);
			return;
		}
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogExplored, Error, TEXT("Manifest de mallas inválido"));
			return;
		}

		// Acepta tanto una lista de mallas como un objeto con la lista en «meshes».
		TArray<TSharedPtr<FJsonValue>> Entries;
		if (Root->Type == EJson::Array)
		{
			Entries = Root->AsArray();
		}
		else if (Root->Type == EJson::Object && Root->AsObject()->HasField(TEXT("meshes")))
		{
			Entries = Root->AsObject()->GetArrayField(TEXT("meshes"));
		}

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({TEXT("/Game/Generated/Meshes")}, true);
		TArray<FAssetData> Imported;
		Registry.GetAssetsByPath(FName(TEXT("/Game/Generated/Meshes")), Imported, true);

		for (const TSharedPtr<FJsonValue>& Entry : Entries)
		{
			const TSharedPtr<FJsonObject> Obj = Entry->AsObject();
			if (!Obj)
			{
				continue;
			}
			const FString Name = Obj->GetStringField(TEXT("name"));
			const FString Category = Obj->GetStringField(TEXT("category"));
			const FAssetData* Asset = Imported.FindByPredicate([&Name](const FAssetData& A)
			{
				return A.AssetName.ToString() == Name;
			});
			if (!Asset)
			{
				UE_LOG(LogExplored, Warning, TEXT("Malla %s del manifest no importada"), *Name);
				continue;
			}
			for (FScatterRule& Rule : Rules)
			{
				if (Rule.ManifestCategory == Category && (Rule.NameFilter.IsEmpty() || Name.Contains(Rule.NameFilter)))
				{
					Rule.Meshes.Add(Asset->GetSoftObjectPath());
				}
			}
		}
		for (const FScatterRule& Rule : Rules)
		{
			UE_LOG(LogExplored, Display, TEXT("  %s: %d mallas"), *Rule.Species.ToString(), Rule.Meshes.Num());
		}
	}

	void SpawnVegetation(UWorld* World, const FTerrainDensity& Density, const FVector& SpawnLocationCm)
	{
		TArray<FScatterRule> Rules = FVegetationScatter::DefaultRules();
		ResolveScatterMeshes(Rules);
		Rules.RemoveAll([](const FScatterRule& R) { return R.Meshes.IsEmpty(); });
		if (Rules.IsEmpty())
		{
			return;
		}

		const double Start = FPlatformTime::Seconds();
		const float E = FArchipelagoLayout::WorldHalfExtent;
		const FScatterResult Result = FVegetationScatter::Generate(Density, Rules, FBox2D(FVector2D(-E), FVector2D(E)),
			Density.GetLayout().Seed);
		UE_LOG(LogExplored, Display, TEXT("Vegetación: %d instancias en %.1f s"), Result.Total(), FPlatformTime::Seconds() - Start);

		// Celdas de 512 m: el culling y el streaming trabajan por celda.
		constexpr float CellSizeCm = 51200.0f;
		TMap<FIntPoint, AExploredVegetationCell*> Cells;
		for (int32 R = 0; R < Rules.Num(); ++R)
		{
			TArray<UStaticMesh*> Meshes;
			for (const FSoftObjectPath& Path : Rules[R].Meshes)
			{
				Meshes.Add(Cast<UStaticMesh>(Path.TryLoad()));
			}
			for (const FScatterInstance& Instance : Result.PerRule[R])
			{
				UStaticMesh* Mesh = Meshes[Instance.MeshIndex];
				if (!Mesh)
				{
					continue;
				}
				const FVector Location = Instance.Transform.GetLocation();
				const FIntPoint Key(FMath::FloorToInt32(Location.X / CellSizeCm), FMath::FloorToInt32(Location.Y / CellSizeCm));
				AExploredVegetationCell*& Cell = Cells.FindOrAdd(Key);
				if (!Cell)
				{
					const FVector CellOrigin(Key.X * CellSizeCm + CellSizeCm * 0.5f, Key.Y * CellSizeCm + CellSizeCm * 0.5f, 0.0f);
					Cell = World->SpawnActor<AExploredVegetationCell>(CellOrigin, FRotator::ZeroRotator);
					Cell->CellCoord = Key;
					Cell->SetActorLabel(FString::Printf(TEXT("Vegetation_%d_%d"), Key.X, Key.Y));
					Cell->SetFolderPath(FName(TEXT("Vegetation")));
					Cell->SetRuntimeGrid(FName(VegetationGridName));
					if (FVector::DistXY(CellOrigin, SpawnLocationCm) < LandingSafetyRadiusCm)
					{
						// Celda de vegetación de la playa de aparición: siempre cargada, igual que
						// el terreno de esa zona (ver ComposeMap), para que el jugador no vea
						// vegetación apareciendo de golpe en su primer segundo de juego.
						Cell->SetIsSpatiallyLoaded(false);
					}
				}
				UHierarchicalInstancedStaticMeshComponent* Component = Cell->GetOrCreateComponent(Mesh, Rules[R].Species,
					!Rules[R].bNoCollision, Rules[R].CullDistance, Rules[R].bCastShadow);
				Component->AddInstance(Instance.Transform, true);
			}
		}
		for (const auto& Pair : Cells)
		{
			for (UActorComponent* C : Pair.Value->GetComponents())
			{
				if (UHierarchicalInstancedStaticMeshComponent* H = Cast<UHierarchicalInstancedStaticMeshComponent>(C))
				{
					H->BuildTreeIfOutdated(false, true);
				}
			}
		}
		UE_LOG(LogExplored, Display, TEXT("Vegetación repartida en %d celdas"), Cells.Num());
	}

	// ------------------------------------------------------------------
	// Formaciones rocosas y microdetalle de playa (docs/diseno/exploracion.md §4.3-4.4)
	// ------------------------------------------------------------------
	// Bloque aislado a propósito: un único punto de entrada, SpawnFormations(...), llamado una
	// vez desde ComposeMap (ver más abajo). Otro agente está migrando este commandlet a World
	// Partition; mover o adaptar este bloque no debería tocar nada de lo de arriba. Reutiliza el
	// mismo esquema de celdas de 512 m por malla (AExploredVegetationCell) que SpawnVegetation,
	// sin compartir su TMap de celdas: las formaciones y la vegetación pueden vivir en actores de
	// celda distintos sin que eso cambie el resultado.

	struct FPropManifestEntry
	{
		FString Name;
		FString Group;
		FSoftObjectPath Path;
	};

	/** Lee Art/Export/Props/manifest.json y resuelve cada malla ya importada en /Game/Generated/Meshes/<grupo>/<nombre>. */
	TArray<FPropManifestEntry> LoadPropManifest()
	{
		TArray<FPropManifestEntry> Entries;
		const FString ManifestPath = FPaths::ProjectDir() / TEXT("Art/Export/Props/manifest.json");
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ManifestPath))
		{
			UE_LOG(LogExplored, Warning, TEXT("No hay manifest de props en %s: sin formaciones"), *ManifestPath);
			return Entries;
		}
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogExplored, Error, TEXT("Manifest de props inválido"));
			return Entries;
		}
		TArray<TSharedPtr<FJsonValue>> List;
		if (Root->Type == EJson::Object && Root->AsObject()->HasField(TEXT("meshes")))
		{
			List = Root->AsObject()->GetArrayField(TEXT("meshes"));
		}
		else if (Root->Type == EJson::Array)
		{
			List = Root->AsArray();
		}

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({TEXT("/Game/Generated/Meshes")}, true);
		TArray<FAssetData> Imported;
		Registry.GetAssetsByPath(FName(TEXT("/Game/Generated/Meshes")), Imported, true);

		for (const TSharedPtr<FJsonValue>& Item : List)
		{
			const TSharedPtr<FJsonObject> Obj = Item->AsObject();
			if (!Obj)
			{
				continue;
			}
			const FString Name = Obj->GetStringField(TEXT("name"));
			const FString Group = Obj->GetStringField(TEXT("group"));
			const FAssetData* Asset = Imported.FindByPredicate([&Name](const FAssetData& A)
			{
				return A.AssetName.ToString() == Name;
			});
			if (!Asset)
			{
				UE_LOG(LogExplored, Warning, TEXT("Malla de props %s no importada"), *Name);
				continue;
			}
			Entries.Add({Name, Group, Asset->GetSoftObjectPath()});
		}
		return Entries;
	}

	/** Malla candidata para una instancia, por grupo del manifiesto y filtro de subcadena en el nombre. */
	UStaticMesh* ResolveFormationMesh(const TArray<FPropManifestEntry>& Manifest, const FFormationInstance& Instance,
		TSet<FString>& WarnedOnce)
	{
		TArray<const FPropManifestEntry*> Candidates;
		const FString Group = Instance.ManifestGroup.ToString();
		const FString Filter = Instance.MeshFilter.ToString();
		for (const FPropManifestEntry& Entry : Manifest)
		{
			if (Entry.Group == Group && (Filter.IsEmpty() || Entry.Name.Contains(Filter)))
			{
				Candidates.Add(&Entry);
			}
		}
		if (Candidates.IsEmpty())
		{
			const FString Key = Group + TEXT("/") + Filter;
			if (!WarnedOnce.Contains(Key))
			{
				UE_LOG(LogExplored, Warning, TEXT("Sin mallas para %s: se omite esa formación"), *Key);
				WarnedOnce.Add(Key);
			}
			return nullptr;
		}
		const int32 Index = static_cast<int32>(static_cast<uint32>(Instance.VariantIndex) % static_cast<uint32>(Candidates.Num()));
		return Cast<UStaticMesh>(Candidates[Index]->Path.TryLoad());
	}

	/** Reglas de microdetalle de playa con sus mallas resueltas contra Art/Export/Meshes/manifest.json (mismo patrón que ResolveScatterMeshes). */
	void ResolveBeachDebrisMeshes(TArray<FBeachDebrisRule>& Rules)
	{
		const FString ManifestPath = FPaths::ProjectDir() / TEXT("Art/Export/Meshes/manifest.json");
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ManifestPath))
		{
			return;
		}
		TSharedPtr<FJsonValue> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}
		TArray<TSharedPtr<FJsonValue>> Entries;
		if (Root->Type == EJson::Array)
		{
			Entries = Root->AsArray();
		}
		else if (Root->Type == EJson::Object && Root->AsObject()->HasField(TEXT("meshes")))
		{
			Entries = Root->AsObject()->GetArrayField(TEXT("meshes"));
		}

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({TEXT("/Game/Generated/Meshes")}, true);
		TArray<FAssetData> Imported;
		Registry.GetAssetsByPath(FName(TEXT("/Game/Generated/Meshes")), Imported, true);

		for (const TSharedPtr<FJsonValue>& Entry : Entries)
		{
			const TSharedPtr<FJsonObject> Obj = Entry->AsObject();
			if (!Obj)
			{
				continue;
			}
			const FString Name = Obj->GetStringField(TEXT("name"));
			const FString Category = Obj->GetStringField(TEXT("category"));
			const FAssetData* Asset = Imported.FindByPredicate([&Name](const FAssetData& A)
			{
				return A.AssetName.ToString() == Name;
			});
			if (!Asset)
			{
				continue;
			}
			for (FBeachDebrisRule& Rule : Rules)
			{
				if (Rule.ManifestCategory == Category && (Rule.NameFilter.IsEmpty() || Name.Contains(Rule.NameFilter)))
				{
					Rule.Meshes.Add(Asset->GetSoftObjectPath());
				}
			}
		}
	}

	AExploredVegetationCell* GetOrCreateFormationCell(UWorld* World, TMap<FIntPoint, AExploredVegetationCell*>& Cells,
		const FVector& LocationCm, float CellSizeCm)
	{
		const FIntPoint Key(FMath::FloorToInt32(LocationCm.X / CellSizeCm), FMath::FloorToInt32(LocationCm.Y / CellSizeCm));
		AExploredVegetationCell*& Cell = Cells.FindOrAdd(Key);
		if (!Cell)
		{
			const FVector CellOrigin(Key.X * CellSizeCm + CellSizeCm * 0.5f, Key.Y * CellSizeCm + CellSizeCm * 0.5f, 0.0f);
			Cell = World->SpawnActor<AExploredVegetationCell>(CellOrigin, FRotator::ZeroRotator);
			Cell->CellCoord = Key;
			Cell->SetActorLabel(FString::Printf(TEXT("Formations_%d_%d"), Key.X, Key.Y));
			Cell->SetFolderPath(FName(TEXT("Formations")));
		}
		return Cell;
	}

	/** Distancia de corte por tipo: los hitos grandes (paredes, arco, farallones) se dejan ver de más lejos. */
	float CullDistanceForKind(EFormationKind Kind)
	{
		switch (Kind)
		{
		case EFormationKind::CliffWall:
		case EFormationKind::SeaArch:
		case EFormationKind::SeaStack:
			return 700.0f;
		case EFormationKind::CliffSpur:
			return 550.0f;
		default:
			return 350.0f; // Boulder, Cobble, LimestoneSlab.
		}
	}

	/**
	 * Formaciones rocosas (paredes, espolones, farallones, arco marino, bloques y losas) y
	 * microdetalle de playa. Único punto de entrada de este bloque: se llama una vez desde
	 * ComposeMap. No falla si faltan los manifiestos o las mallas; avisa y sigue.
	 */
	void SpawnFormations(UWorld* World, const FTerrainDensity& Density)
	{
		const double Start = FPlatformTime::Seconds();
		constexpr float CellSizeCm = 51200.0f; // mismas celdas de 512 m que la vegetación.

		// Puntos protegidos: el spawn de Landing y todos los puntos de interés ya colocados, para
		// no tapar ni el aterrizaje ni ningún hito jugable (no hay un sistema de rutas aparte que
		// proteger todavía; FindBeach/FindInland resuelven sobre el terreno, no sobre trazos fijos).
		TArray<FVector> AvoidPoints;
		AvoidPoints.Add(FindSpawnPoint(Density));
		for (const FPointOfInterest& Poi : FPoiLayout::Generate(Density))
		{
			AvoidPoints.Add(Poi.Location);
		}

		const uint32 Seed = Density.GetLayout().Seed;
		const TArray<FFormationInstance> Formations = FFormationPlacementModel::Generate(Density, Seed, AvoidPoints);

		TArray<FBeachDebrisRule> BeachRules = FBeachDebrisModel::DefaultRules();
		ResolveBeachDebrisMeshes(BeachRules);
		BeachRules.RemoveAll([](const FBeachDebrisRule& R) { return R.Meshes.IsEmpty(); }); // conchas y algas, sin malla todavía.
		const TArray<FBeachDebrisInstance> BeachDebris = BeachRules.IsEmpty()
			? TArray<FBeachDebrisInstance>()
			: FBeachDebrisModel::Generate(Density, BeachRules, Seed, AvoidPoints);

		const TArray<FPropManifestEntry> PropManifest = LoadPropManifest();
		TSet<FString> WarnedOnce;
		TMap<FIntPoint, AExploredVegetationCell*> Cells;

		int32 SpawnedFormations = 0;
		for (const FFormationInstance& Instance : Formations)
		{
			UStaticMesh* Mesh = ResolveFormationMesh(PropManifest, Instance, WarnedOnce);
			if (!Mesh)
			{
				continue;
			}
			AExploredVegetationCell* Cell = GetOrCreateFormationCell(World, Cells, Instance.Transform.GetLocation(), CellSizeCm);
			UHierarchicalInstancedStaticMeshComponent* Component = Cell->GetOrCreateComponent(Mesh,
				FName(LexToString(Instance.Kind)), /*bCollision=*/true, CullDistanceForKind(Instance.Kind), /*bCastShadow=*/true);
			Component->AddInstance(Instance.Transform, true);
			++SpawnedFormations;
		}

		int32 SpawnedBeachDebris = 0;
		for (const FBeachDebrisInstance& Instance : BeachDebris)
		{
			const FBeachDebrisRule* Rule = BeachRules.FindByPredicate([&Instance](const FBeachDebrisRule& R)
			{
				return R.Species == Instance.Species;
			});
			if (!Rule || !Rule->Meshes.IsValidIndex(Instance.MeshIndex))
			{
				continue;
			}
			UStaticMesh* Mesh = Cast<UStaticMesh>(Rule->Meshes[Instance.MeshIndex].TryLoad());
			if (!Mesh)
			{
				continue;
			}
			AExploredVegetationCell* Cell = GetOrCreateFormationCell(World, Cells, Instance.Transform.GetLocation(), CellSizeCm);
			// Clutter pequeño: sin colisión ni sombra propia, se corta pronto (igual que Debris en VegetationScatter).
			UHierarchicalInstancedStaticMeshComponent* Component = Cell->GetOrCreateComponent(Mesh, Instance.Species,
				/*bCollision=*/false, /*CullDistanceMeters=*/110.0f, /*bCastShadow=*/false);
			Component->AddInstance(Instance.Transform, true);
			++SpawnedBeachDebris;
		}

		for (const auto& Pair : Cells)
		{
			for (UActorComponent* C : Pair.Value->GetComponents())
			{
				if (UHierarchicalInstancedStaticMeshComponent* H = Cast<UHierarchicalInstancedStaticMeshComponent>(C))
				{
					H->BuildTreeIfOutdated(false, true);
				}
			}
		}

		UE_LOG(LogExplored, Display, TEXT("Formaciones: %d rocas + %d microdetalle de playa en %d celdas (%.1f s)"),
			SpawnedFormations, SpawnedBeachDebris, Cells.Num(), FPlatformTime::Seconds() - Start);
	}

	int32 ComposeMap(const FTerrainDensity& Density, const TArray<FTerrainPiece>& Terrain, bool bVegetation, bool bFormations)
	{
		// El mapa se recrea desde cero para que el proceso sea idempotente.
		UPackage* MapPackage = CreatePackage(MapPath);
		UWorldFactory* Factory = NewObject<UWorldFactory>();
		Factory->WorldType = EWorldType::Editor;
		Factory->bInformEngineOfWorld = true;
		// World Partition se crea a mano en SetupWorldPartition, forzando la clase clásica de
		// RuntimeHash (ver el comentario allí); aquí se deja desactivado para que el factory no cree
		// primero uno con la clase por defecto del proyecto.
		Factory->bCreateWorldPartition = false;
		UWorld* World = CastChecked<UWorld>(Factory->FactoryCreateNew(UWorld::StaticClass(), MapPackage,
			FName(TEXT("Archipelago")), RF_Public | RF_Standalone, nullptr, GWarn));

		if (!SetupWorldPartition(World))
		{
			World->DestroyWorld(false);
			return 1;
		}

		// El punto de aparición hace falta antes de repartir terreno y vegetación: la burbuja de
		// LandingSafetyRadiusCm alrededor de él se marca siempre cargada (bIsSpatiallyLoaded=false)
		// porque el jugador aparece ahí antes de que ninguna fuente de streaming la reclame.
		const FVector Spawn = FindSpawnPoint(Density) * 100.0;

		for (const FTerrainPiece& Piece : Terrain)
		{
			const FString Name = Piece.Mesh->GetName();
			// El fondo marino grueso ("SM_Terrain_DeepFloor") cubre el mundo entero con una sola
			// malla: se deja siempre cargado como red de seguridad, para que el jugador nunca caiga
			// al vacío mientras el trozo de terreno fino de su zona todavía está transmitiéndose
			// (p. ej. al cargar una partida guardada lejos del punto de aparición; ver
			// UExploredSaveSubsystem::ApplyPendingPlayer).
			const bool bAlwaysLoaded = Name == TEXT("SM_Terrain_DeepFloor") ||
				FVector::DistXY(Piece.LocationCm, Spawn) < LandingSafetyRadiusCm;
			SpawnMeshActor(World, Piece.Mesh, Piece.LocationCm, Name, bAlwaysLoaded);
		}

		World->SpawnActor<APlayerStart>(Spawn, FRotator::ZeroRotator)->SetActorLabel(TEXT("PlayerStart"));
		SpawnOptional(World, TEXT("/Script/Explored.ExploredSkyController"), FVector(0, 0, 10000), TEXT("Sky"));
		SpawnOptional(World, TEXT("/Script/Explored.ExploredOcean"), FVector::ZeroVector, TEXT("Ocean"));
		if (bVegetation)
		{
			SpawnVegetation(World, Density, Spawn);
		}
		if (bFormations)
		{
			SpawnFormations(World, Density);
		}

		const bool bActorsSaved = SaveExternalActorPackages(World);
		const bool bSaved = SavePackageToDisk(MapPackage, World, true) && bActorsSaved;
		World->DestroyWorld(false);
		UE_LOG(LogExplored, Display, TEXT("Mapa %s con %d piezas de terreno"), bSaved ? TEXT("guardado") : TEXT("NO guardado"), Terrain.Num());
		return bSaved ? 0 : 1;
	}
}

UExploredWorldGenCommandlet::UExploredWorldGenCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UExploredWorldGenCommandlet::Main(const FString& Params)
{
	FString Mode = TEXT("preview");
	FParse::Value(*Params, TEXT("mode="), Mode);
	uint32 Seed = DefaultSeed;
	FParse::Value(*Params, TEXT("seed="), Seed);

	const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(Seed);
	const FTerrainDensity Density(Layout);

	if (Mode == TEXT("preview"))
	{
		int32 Size = 1024;
		FParse::Value(*Params, TEXT("size="), Size);
		FString Out = FPaths::ProjectSavedDir() / TEXT("WorldGen") / FString::Printf(TEXT("preview_%u.png"), Seed);
		FParse::Value(*Params, TEXT("out="), Out);
		return RunPreview(Density, Size, Out);
	}

	if (Mode == TEXT("bake") || Mode == TEXT("terrain") || Mode == TEXT("map"))
	{
		FTerrainChunkSettings Settings;
		FParse::Value(*Params, TEXT("voxel="), Settings.VoxelSize);
		FParse::Value(*Params, TEXT("cells="), Settings.CellsPerChunk);

		const float E = FArchipelagoLayout::WorldHalfExtent;
		FBox2D Region(FVector2D(-E, -E), FVector2D(E, E));
		FString RegionText;
		if (FParse::Value(*Params, TEXT("region="), RegionText, false))
		{
			TArray<FString> Parts;
			RegionText.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() == 4)
			{
				Region = FBox2D(FVector2D(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1])),
					FVector2D(FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3])));
			}
		}
		if (Mode != TEXT("map"))
		{
			const int32 TerrainResult = RunBake(Density, Settings, Region);
			if (TerrainResult != 0 || Mode == TEXT("terrain"))
			{
				return TerrainResult;
			}
		}
		const bool bVegetation = !FParse::Param(*Params, TEXT("novegetation"));
		const bool bFormations = !FParse::Param(*Params, TEXT("noformations"));
		return ComposeMap(Density, FindTerrainPieces(Settings), bVegetation, bFormations);
	}

	UE_LOG(LogExplored, Error, TEXT("Modo desconocido: %s"), *Mode);
	return 1;
}
