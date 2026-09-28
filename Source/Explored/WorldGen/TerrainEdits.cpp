#include "WorldGen/TerrainEdits.h"

#include "Save/SaveWorldDeltas.h"
#include "WorldGen/TerrainDensity.h"

namespace TerrainEditsDetail
{
	bool ChunkLess(const FIntVector& A, const FIntVector& B)
	{
		if (A.Z != B.Z) { return A.Z < B.Z; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.X < B.X;
	}

	/** Ordena y quita repetidos. */
	void SortUnique(TArray<FIntVector>& Chunks)
	{
		Chunks.Sort(&ChunkLess);
		int32 Write = 0;
		for (int32 Read = 0; Read < Chunks.Num(); ++Read)
		{
			if (Write == 0 || Chunks[Write - 1] != Chunks[Read])
			{
				Chunks[Write++] = Chunks[Read];
			}
		}
		Chunks.SetNum(Write);
	}
}

// ---------------------------------------------------------------------------
// Datos de diseño
// ---------------------------------------------------------------------------

const FTerrainDigToolInfo& FTerrainEdits::ToolInfo(ETerrainDigTool Tool)
{
	// Biblia 02 §2.2 (picos). La pala tosca no está en esa tabla: su esfera es el hueco de
	// una unidad de botín (1/6 m³ ⇒ r ≈ 0,34 m) y su golpe dura una pasada de pala
	// (FTerrainEditModel::SecondsPerShovelStroke).
	static const FTerrainDigToolInfo Table[] = {
		{ 1, 0.35f, 1.2f }, // PalaTosca
		{ 2, 0.40f, 1.3f }, // PicoPiedra
		{ 3, 0.42f, 1.2f }, // PicoTallado
		{ 4, 0.50f, 1.0f }, // PicoObsidiana
		{ 4, 0.55f, 1.1f }, // PicoRescatado
	};
	const int32 Index = FMath::Clamp(static_cast<int32>(Tool), 0, static_cast<int32>(ETerrainDigTool::Count) - 1);
	return Table[Index];
}

float FTerrainEdits::HitsPerCubicMeter(ETerrainMaterial Material, ETerrainDigTool Tool)
{
	return FTerrainEditModel::DesignHitsPerCubicMeter(Material, ToolInfo(Tool).ToolTier);
}

float FTerrainEdits::SecondsPerCubicMeter(ETerrainMaterial Material, ETerrainDigTool Tool)
{
	return HitsPerCubicMeter(Material, Tool) * ToolInfo(Tool).SecondsPerHit;
}

bool FTerrainEdits::IsCadenceValid(ETerrainDigTool Tool, float SecondsSinceLastHit)
{
	// NaN no pasa: la comparación es falsa.
	return SecondsSinceLastHit >= ToolInfo(Tool).SecondsPerHit * (1.0f - CadenceTolerance);
}

const TCHAR* FTerrainEdits::LootItemId(ETerrainMaterial Material)
{
	switch (Material)
	{
	case ETerrainMaterial::Arena: return TEXT("arena");
	case ETerrainMaterial::Tierra: return TEXT("tierra_suelta");
	case ETerrainMaterial::Caliza: return TEXT("caliza");
	case ETerrainMaterial::Basalto: return TEXT("basalto");
	case ETerrainMaterial::Obsidiana: return TEXT("obsidiana");
	default: return TEXT("tierra_suelta");
	}
}

int32 FTerrainEdits::TakeLootUnits(double VolumeRemoved, double& InOutRemainder)
{
	if (!FMath::IsFinite(InOutRemainder) || !(InOutRemainder >= 0.0))
	{
		InOutRemainder = 0.0;
	}
	if (!(VolumeRemoved > 0.0) || !FMath::IsFinite(VolumeRemoved))
	{
		return 0;
	}
	// El tope de un golpe se alcanza por bisección y los deltas se redondean al milímetro:
	// un golpe lleno arranca un 0,4 % menos de 1/6 m³ y daría 0,996 unidades. Una centésima
	// de tolerancia lo cuenta entero (el jugador gana como mucho un 1 %, nunca pierde un golpe).
	const double Units = InOutRemainder + VolumeRemoved * UnitsPerCubicMeter;
	const double Whole = FMath::Min(FMath::FloorToDouble(Units + 0.01), 1.0e6);
	InOutRemainder = FMath::Max(Units - Whole, 0.0);
	return static_cast<int32>(Whole);
}

void FTerrainEdits::RenderChunksTouchingBox(const FBox& Meters, const FTerrainChunkSettings& Render, TArray<FIntVector>& Out)
{
	const double Size = Render.ChunkSizeMeters();
	if (!(Size > 0.0) || !Meters.IsValid)
	{
		return;
	}
	// El chunk C lee la densidad en [C·S − V, (C + 1)·S + V] (una muestra de solape) y las
	// normales de sus vértices media celda más allá.
	const double Margin = 1.5 * Render.VoxelSize;
	const FVector Lo = (Meters.Min - FVector(Margin)) / Size;
	const FVector Hi = (Meters.Max + FVector(Margin)) / Size;
	static constexpr double Limit = 1.0e9;
	if (!(FMath::Abs(Lo.X) < Limit && FMath::Abs(Lo.Y) < Limit && FMath::Abs(Lo.Z) < Limit
		&& FMath::Abs(Hi.X) < Limit && FMath::Abs(Hi.Y) < Limit && FMath::Abs(Hi.Z) < Limit))
	{
		return;
	}
	const FIntVector Min(FMath::CeilToInt32(Lo.X) - 1, FMath::CeilToInt32(Lo.Y) - 1, FMath::CeilToInt32(Lo.Z) - 1);
	const FIntVector Max(FMath::FloorToInt32(Hi.X), FMath::FloorToInt32(Hi.Y), FMath::FloorToInt32(Hi.Z));
	for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				Out.Add(FIntVector(X, Y, Z));
			}
		}
	}
	TerrainEditsDetail::SortUnique(Out);
}

// ---------------------------------------------------------------------------
// Capa
// ---------------------------------------------------------------------------

FTerrainEdits::FTerrainEdits(const FTerrainEditSettings& InSettings, const FTerrainChunkSettings& InRender)
	: Model(InSettings)
	, RenderSettings(InRender)
{
}

void FTerrainEdits::RenderChunksForSphere(const FVector& Center, float SampleReach, TArray<FIntVector>& Out) const
{
	// Una muestra cambiada mueve la densidad interpolada hasta una celda alrededor.
	const double Reach = SampleReach + Model.GetSettings().CellSize;
	RenderChunksTouchingBox(FBox(Center - FVector(Reach), Center + FVector(Reach)), RenderSettings, Out);
}

TArray<FIntVector> FTerrainEdits::EditedRenderChunks() const
{
	TArray<FIntVector> Out;
	const int32 N = Model.GetSettings().CellsPerChunk;
	const double H = Model.GetSettings().CellSize;
	for (const FIntVector& Chunk : Model.EditedChunks())
	{
		// Muestras propias del chunk, más la celda de interpolación a cada lado.
		const FVector First = Model.SamplePosition(Chunk * N);
		const FVector Last = Model.SamplePosition(Chunk * N + FIntVector(N - 1));
		RenderChunksTouchingBox(FBox(First - FVector(H), Last + FVector(H)), RenderSettings, Out);
	}
	TerrainEditsDetail::SortUnique(Out);
	return Out;
}

void FTerrainEdits::ChunksToInvalidate(const FVector& ImpactPoint, ETerrainDigTool Tool, TArray<FIntVector>& OutEditChunks,
	TArray<FIntVector>& OutRenderChunks) const
{
	const float Reach = Model.SphereDigReach(ToolInfo(Tool).Radius);
	Model.ChunksTouchedBySphere(ImpactPoint, Reach, OutEditChunks);
	RenderChunksForSphere(ImpactPoint, Reach, OutRenderChunks);
}

FTerrainDigResult FTerrainEdits::Dig(const FTerrainDigHit& Hit, FTerrainEditModel::FBaseDensity ProceduralBase)
{
	FTerrainDigResult Result;
	const FTerrainDigToolInfo& Info = ToolInfo(Hit.Tool);
	Result.Seconds = Info.SecondsPerHit;
	const float Factor = FTerrainEditModel::ToolFactor(Hit.Material, Info.ToolTier);
	if (Factor <= 0.0f)
	{
		Result.Edit.bRejected = true;
		return Result;
	}
	if (!FMath::IsFinite(Hit.ImpactPoint.X) || !FMath::IsFinite(Hit.ImpactPoint.Y) || !FMath::IsFinite(Hit.ImpactPoint.Z))
	{
		return Result;
	}
	// Golpes por m³ de diseño, con la dureza extra de un camino compactado en ese punto.
	const float Compacted = Model.HardnessAt(Hit.Material, Hit.ImpactPoint) / FTerrainEditModel::MaterialInfo(Hit.Material).Hardness;
	const float Hits = FMath::Max(6.0f * Compacted / Factor, FTerrainEditModel::MinHitsPerCubicMeter);
	Result.MaxVolume = 1.0 / Hits;

	FSphereDig Sphere;
	Sphere.Center = Hit.ImpactPoint;
	Sphere.Radius = Info.Radius;
	Sphere.Material = Hit.Material;
	Sphere.ToolTier = Info.ToolTier;
	Sphere.MaxVolume = Result.MaxVolume;
	Result.Edit = Model.DigSphere(Sphere, ProceduralBase);
	if (Result.Edit.Changed())
	{
		RenderChunksForSphere(Hit.ImpactPoint, Model.SphereDigReach(Info.Radius), Result.RenderChunks);
	}
	return Result;
}

FTerrainDigResult FTerrainEdits::Dig(const FTerrainDigHit& Hit, const FTerrainDensity& Terrain)
{
	return Dig(Hit, [&Terrain](const FVector& P) { return Terrain.ProceduralDensity(P); });
}

// ---------------------------------------------------------------------------
// Guardado
// ---------------------------------------------------------------------------

void FTerrainEdits::SaveTo(FSaveWorldDeltas& World) const
{
	World.Terrain = Model.IsEmpty() ? FSaveValue() : Model.ToValue();
}

bool FTerrainEdits::LoadFrom(const FSaveWorldDeltas& World)
{
	if (World.Terrain.IsNull())
	{
		Model.Reset();
		return true;
	}
	return Model.FromValue(World.Terrain);
}
