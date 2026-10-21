#include "WorldGen/GrassFieldModel.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/IslandPropsModel.h"

namespace GrassFieldDetail
{
	const TCHAR* const KindNames[GrassKindCount] = {
		TEXT("tuft_a"), TEXT("tuft_b"), TEXT("tuft_c"), TEXT("tall"), TEXT("flower_a"), TEXT("flower_b"), TEXT("fern"), TEXT("clover")};
	const TCHAR* const ContextNames[GrassContextCount] = {
		TEXT("common"), TEXT("edge"), TEXT("clearing"), TEXT("palm"), TEXT("dirt")};
	/** Nada por debajo de esta cota (m): ni orilla mojada ni charcos. */
	constexpr float MinHeightM = 0.3f;
	/** Subrejilla máxima por eje (64 × 64 candidatos por celda). */
	constexpr int32 MaxSubdivisions = 64;
	constexpr uint32 ClearingSalt = 0xC1EA41u;
	constexpr uint32 CandidateSalt = 0x6A55u;

	/** Tramo de camino de la celda: extremos y media anchura. */
	struct FSegment
	{
		FVector2D A;
		FVector2D B;
		double HalfWidth = 0.0;
	};

	struct FCellContext
	{
		TArray<FGrassExclusion> Exclusions;
		TArray<FVector2D> Palms;
		TArray<FSegment> Segments;
		TArray<FIslandRock> Rocks;
	};

	float Roll(uint32 Hash, uint32 Channel)
	{
		return ExploredHash::ToUnitFloat(ExploredHash::Hash32(Hash + Channel * 0x9E3779B9u));
	}

	double DistanceToBox(const FVector2D& P, const FVector2D& Lo, const FVector2D& Hi)
	{
		const double Dx = FMath::Max3(Lo.X - P.X, 0.0, P.X - Hi.X);
		const double Dy = FMath::Max3(Lo.Y - P.Y, 0.0, P.Y - Hi.Y);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	double DistanceToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D V = B - A;
		const double LenSq = FMath::Max(V.SizeSquared(), 1e-9);
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, V) / LenSq, 0.0, 1.0);
		return FVector2D::Distance(P, A + V * T);
	}

	/** Ruido de valor (0-1) con celdas de CellM metros: los claros. */
	float ValueNoise(uint32 Seed, const FVector2D& P, float CellM)
	{
		const double Fx = P.X / CellM;
		const double Fy = P.Y / CellM;
		const int32 Ix = FMath::FloorToInt32(Fx);
		const int32 Iy = FMath::FloorToInt32(Fy);
		const float Tx = static_cast<float>(Fx - Ix);
		const float Ty = static_cast<float>(Fy - Iy);
		const float Sx = Tx * Tx * (3.0f - 2.0f * Tx);
		const float Sy = Ty * Ty * (3.0f - 2.0f * Ty);
		auto V = [Seed](int32 X, int32 Y) { return ExploredHash::ToUnitFloat(ExploredHash::Hash2D(Seed ^ ClearingSalt, X, Y)); };
		return FMath::Lerp(FMath::Lerp(V(Ix, Iy), V(Ix + 1, Iy), Sx), FMath::Lerp(V(Ix, Iy + 1), V(Ix + 1, Iy + 1), Sx), Sy);
	}

	FCellContext Gather(const FGrassConfig& Config, const FGrassWorld& World, const FVector2D& Lo, const FVector2D& Hi)
	{
		FCellContext Ctx;
		for (const FGrassExclusion& Ex : World.Exclusions)
		{
			if (DistanceToBox(Ex.Center, Lo, Hi) < Ex.Radius)
			{
				Ctx.Exclusions.Add(Ex);
			}
		}
		for (const FVector2D& Palm : World.Palms)
		{
			if (DistanceToBox(Palm, Lo, Hi) < Config.PalmRadiusM)
			{
				Ctx.Palms.Add(Palm);
			}
		}
		const FVector2D Center = (Lo + Hi) * 0.5;
		const double HalfDiag = FVector2D::Distance(Lo, Hi) * 0.5;
		for (const FIslandPath& Path : World.Layout.Paths)
		{
			const double Reach = HalfDiag + Path.Width * 0.5 + Config.PathMarginM + Config.EdgeBandM;
			for (int32 I = 0; I + 1 < Path.Points.Num(); ++I)
			{
				if (DistanceToSegment(Center, Path.Points[I], Path.Points[I + 1]) < Reach)
				{
					Ctx.Segments.Add({Path.Points[I], Path.Points[I + 1], Path.Width * 0.5});
				}
			}
		}
		for (const FIslandRock& Rock : World.Layout.Rocks)
		{
			if (Rock.Height > 0.0f && DistanceToBox(Rock.Center, Lo, Hi) < Rock.Radius * 0.6f + Config.EdgeBandM)
			{
				Ctx.Rocks.Add(Rock);
			}
		}
		return Ctx;
	}

	double PathDistance(const FCellContext& Ctx, const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (const FSegment& S : Ctx.Segments)
		{
			Best = FMath::Min(Best, DistanceToSegment(P, S.A, S.B) - S.HalfWidth);
		}
		return Best;
	}

	bool IsExcluded(const FCellContext& Ctx, const FVector2D& P)
	{
		for (const FGrassExclusion& Ex : Ctx.Exclusions)
		{
			if (FVector2D::DistSquared(P, Ex.Center) < static_cast<double>(Ex.Radius) * Ex.Radius)
			{
				return true;
			}
		}
		return false;
	}

	bool InPalmRing(const FGrassConfig& Config, const FCellContext& Ctx, const FVector2D& P)
	{
		for (const FVector2D& Palm : Ctx.Palms)
		{
			const double D = FVector2D::Distance(P, Palm);
			if (D > Config.PalmTrunkM && D < Config.PalmRadiusM)
			{
				return true;
			}
		}
		return false;
	}

	bool NearRock(const FGrassConfig& Config, const FCellContext& Ctx, const FVector2D& P)
	{
		for (const FIslandRock& Rock : Ctx.Rocks)
		{
			if (FVector2D::Distance(P, Rock.Center) < Rock.Radius * 0.6f + Config.EdgeBandM)
			{
				return true;
			}
		}
		return false;
	}

	/** Densidad por zona con un fundido de ZoneFeatherM en su borde; independiente del orden de las zonas. */
	float ZoneDensityAt(const FGrassConfig& Config, const FIslandLayout& Layout, const FVector2D& P)
	{
		float WeightSum = 0.0f;
		float Weighted = 0.0f;
		for (const FIslandZone& Zone : Layout.Zones)
		{
			const float* Density = Config.ZoneDensity.Find(Zone.Id);
			if (!Density)
			{
				continue;
			}
			const float Dist = static_cast<float>(FVector2D::Distance(P, Zone.Center));
			const float W = 1.0f - FMath::SmoothStep(Zone.Radius - Config.ZoneFeatherM, Zone.Radius, Dist);
			WeightSum += W;
			Weighted += W * *Density;
		}
		if (WeightSum >= 1.0f)
		{
			return Weighted / WeightSum;
		}
		return Weighted + (1.0f - WeightSum) * Config.DensityPerM2;
	}

	void SetMix(FGrassMix& Mix, std::initializer_list<float> Weights)
	{
		int32 I = 0;
		for (const float W : Weights)
		{
			Mix.Weights[I++] = W;
		}
	}
}

const TCHAR* LexToString(EGrassKind Kind)
{
	const int32 I = static_cast<int32>(Kind);
	return I < GrassKindCount ? GrassFieldDetail::KindNames[I] : TEXT("?");
}

bool GrassKindFromName(const FString& Name, EGrassKind& Out)
{
	for (int32 I = 0; I < GrassKindCount; ++I)
	{
		if (Name == GrassFieldDetail::KindNames[I])
		{
			Out = static_cast<EGrassKind>(I);
			return true;
		}
	}
	return false;
}

const TCHAR* LexToString(EGrassContext Context)
{
	const int32 I = static_cast<int32>(Context);
	return I < GrassContextCount ? GrassFieldDetail::ContextNames[I] : TEXT("?");
}

bool GrassContextFromName(const FString& Name, EGrassContext& Out)
{
	for (int32 I = 0; I < GrassContextCount; ++I)
	{
		if (Name == GrassFieldDetail::ContextNames[I])
		{
			Out = static_cast<EGrassContext>(I);
			return true;
		}
	}
	return false;
}

float FGrassMix::Total() const
{
	float Sum = 0.0f;
	for (const float W : Weights)
	{
		Sum += FMath::Max(W, 0.0f);
	}
	return Sum;
}

EGrassKind FGrassMix::Pick(float U) const
{
	const float Target = FMath::Clamp(U, 0.0f, 0.999999f) * Total();
	float Acc = 0.0f;
	int32 Last = 0;
	for (int32 I = 0; I < GrassKindCount; ++I)
	{
		if (Weights[I] <= 0.0f)
		{
			continue;
		}
		Last = I;
		Acc += Weights[I];
		if (Target < Acc)
		{
			return static_cast<EGrassKind>(I);
		}
	}
	return static_cast<EGrassKind>(Last);
}

FGrassConfig FGrassConfig::Defaults()
{
	using GrassFieldDetail::SetMix;
	FGrassConfig C;
	C.ZoneDensity.Add(FName(TEXT("palmar")), ZonePalmarDensity);
	C.ZoneDensity.Add(FName(TEXT("colina_faro")), ZoneColinaDensity);
	C.ZoneDensity.Add(FName(TEXT("bosque_bambu")), ZoneBambuDensity);
	C.ZoneDensity.Add(FName(TEXT("cala_varada")), ZoneCalaDensity);
	C.ZoneDensity.Add(FName(TEXT("playa_norte")), ZonePlayaDensity);
	C.ZoneDensity.Add(FName(TEXT("rocas_pescador")), ZoneRocasDensity);
	C.ZoneDensity.Add(FName(TEXT("isla_carguero")), ZoneCargueroDensity);
	// Orden de los pesos: tuft_a, tuft_b, tuft_c, tall, flower_a, flower_b, fern, clover.
	SetMix(C.Mixes[static_cast<int32>(EGrassContext::Common)], {4.0f, 3.0f, 2.5f, 0.4f, 0.2f, 0.2f, 0.1f, 1.2f});
	SetMix(C.Mixes[static_cast<int32>(EGrassContext::Edge)], {2.0f, 1.0f, 2.0f, 4.0f, 0.2f, 0.2f, 0.3f, 0.5f});
	SetMix(C.Mixes[static_cast<int32>(EGrassContext::Clearing)], {1.5f, 2.5f, 0.5f, 0.1f, 2.5f, 2.5f, 0.0f, 2.0f});
	SetMix(C.Mixes[static_cast<int32>(EGrassContext::Palm)], {1.5f, 0.5f, 1.5f, 0.5f, 0.0f, 0.0f, 4.0f, 0.5f});
	SetMix(C.Mixes[static_cast<int32>(EGrassContext::Dirt)], {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f});
	return C;
}

float FGrassConfig::MaxDensity() const
{
	float Max = FMath::Max(DensityPerM2, DirtDensityPerM2);
	for (const TPair<FName, float>& Pair : ZoneDensity)
	{
		Max = FMath::Max(Max, Pair.Value);
	}
	return Max;
}

bool FGrassConfig::Validate(FString& OutError) const
{
	auto Bad = [&OutError](const TCHAR* What) { OutError = What; return false; };
	if (!(CellSizeM >= 2.0f && CellSizeM <= 32.0f)) { return Bad(TEXT("cellSizeM fuera de 2-32")); }
	if (!(RadiusM >= CellSizeM && RadiusM <= 200.0f)) { return Bad(TEXT("radiusM fuera de cellSizeM-200")); }
	if (CellsPerFrame < 1 || CellsPerFrame > 64) { return Bad(TEXT("cellsPerFrame fuera de 1-64")); }
	if (!(MaxSlopeDeg > 0.0f && MaxSlopeDeg < 90.0f)) { return Bad(TEXT("maxSlopeDeg fuera de 0-90")); }
	if (!(ScaleMin > 0.0f && ScaleMax >= ScaleMin && ScaleMax <= 5.0f)) { return Bad(TEXT("escala no válida")); }
	if (!(AlignToNormal >= 0.0f && AlignToNormal <= 1.0f)) { return Bad(TEXT("alignToNormal fuera de 0-1")); }
	const float MaxD = MaxDensity();
	if (!(MaxD > 0.0f && MaxD <= 16.0f) || DensityPerM2 < 0.0f || DirtDensityPerM2 < 0.0f)
	{
		return Bad(TEXT("densidades fuera de 0-16 matas/m²"));
	}
	for (const TPair<FName, float>& Pair : ZoneDensity)
	{
		if (!(Pair.Value >= 0.0f)) { return Bad(TEXT("densidad de zona negativa")); }
	}
	if (!(ClearingCellM > 1.0f && ClearingDensity >= 0.0f && ClearingDensity <= 1.0f)) { return Bad(TEXT("claros no válidos")); }
	if (!(PalmRadiusM > PalmTrunkM && PalmTrunkM >= 0.0f)) { return Bad(TEXT("corona de palmera no válida")); }
	for (const float S : KindScale)
	{
		if (!(S > 0.0f && S <= 5.0f)) { return Bad(TEXT("escala de malla no válida")); }
	}
	for (const FGrassMix& Mix : Mixes)
	{
		for (const float W : Mix.Weights)
		{
			if (!(W >= 0.0f) || !FMath::IsFinite(W)) { return Bad(TEXT("peso de mezcla negativo")); }
		}
	}
	if (Mixes[static_cast<int32>(EGrassContext::Common)].Total() <= 0.0f) { return Bad(TEXT("la mezcla «common» está vacía")); }
	return true;
}

TArray<FGrassExclusion> FGrassWorld::ExclusionsFrom(const FIslandProps& Props, const FIslandLayout& Layout, const FGrassConfig& Config)
{
	TArray<FGrassExclusion> Out;
	for (const FIslandProp& Prop : Props.Props)
	{
		if (!Prop.Anchor.IsNone())
		{
			continue;  // sobre otra pieza (casco, torre): ya la cubre la de su ancla
		}
		const FIslandPropType* Type = Props.FindType(Prop.Type);
		double Radius = Type ? 0.5 * FMath::Sqrt(Type->Size.X * Type->Size.X + Type->Size.Y * Type->Size.Y) : 0.5;
		if (!Prop.Extent.IsNearlyZero())
		{
			Radius = FMath::Max(Radius, FMath::Sqrt(Prop.Extent.X * Prop.Extent.X + Prop.Extent.Y * Prop.Extent.Y));
		}
		Out.Add({Prop.Location, static_cast<float>(Radius) + Config.ClearanceM});
	}
	for (const FIslandPoint& Point : Layout.Points)
	{
		Out.Add({Point.Location, Config.PointClearanceM});
	}
	return Out;
}

TArray<FVector2D> FGrassWorld::PalmsFrom(const FIslandProps& Props)
{
	TArray<FVector2D> Out;
	for (const FIslandProp& Prop : Props.Props)
	{
		if (Prop.Type == FName(TEXT("palma")))
		{
			Out.Add(Prop.Location);
		}
	}
	return Out;
}

void FGrassFieldModel::GenerateCell(const FGrassConfig& Config, const FGrassWorld& World, const FIntPoint& Cell, TArray<FGrassInstance>& Out)
{
	using namespace GrassFieldDetail;
	if (!World.Ground || !World.Ground->IsValid())
	{
		return;
	}
	const double Size = Config.CellSizeM;
	const FVector2D Lo(Cell.X * Size, Cell.Y * Size);
	const FVector2D Hi = Lo + FVector2D(Size, Size);
	const int32 K = FMath::Clamp(FMath::CeilToInt32(Size * FMath::Sqrt(Config.MaxDensity())), 1, MaxSubdivisions);
	const double Sub = Size / K;
	const float SubArea = static_cast<float>(Sub * Sub);
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(Config.MaxSlopeDeg));
	const FCellContext Ctx = Gather(Config, World, Lo, Hi);

	for (int32 I = 0; I < K; ++I)
	{
		for (int32 J = 0; J < K; ++J)
		{
			const uint32 H = ExploredHash::Hash3D(Config.Seed, Cell.X * K + I, Cell.Y * K + J, static_cast<int32>(CandidateSalt));
			const FVector2D P = Lo + FVector2D((I + Roll(H, 0)) * Sub, (J + Roll(H, 1)) * Sub);
			const double PathDist = PathDistance(Ctx, P);
			if (PathDist < Config.PathMarginM || IsExcluded(Ctx, P))
			{
				continue;
			}
			FGroundSample Ground;
			if (!World.Ground->Sample(P.X, P.Y, Ground) || Ground.Height < MinHeightM || Ground.Normal.Z < MinNormalZ)
			{
				continue;
			}
			EGrassContext Context = EGrassContext::Common;
			float Density = 0.0f;
			if (Ground.Surface == EGroundSurface::Dirt)
			{
				Context = EGrassContext::Dirt;
				Density = Config.DirtDensityPerM2;
			}
			else if (Ground.Surface == EGroundSurface::Grass)
			{
				Density = ZoneDensityAt(Config, World.Layout, P);
				if (InPalmRing(Config, Ctx, P))
				{
					Context = EGrassContext::Palm;
				}
				else if (PathDist < Config.PathMarginM + Config.EdgeBandM || NearRock(Config, Ctx, P))
				{
					Context = EGrassContext::Edge;
				}
				else if (ValueNoise(Config.Seed, P, Config.ClearingCellM) > Config.ClearingThreshold)
				{
					Context = EGrassContext::Clearing;
					Density *= Config.ClearingDensity;
				}
			}
			const FGrassMix& Mix = Config.Mixes[static_cast<int32>(Context)];
			if (Roll(H, 2) >= Density * SubArea || Mix.Total() <= 0.0f)
			{
				continue;
			}
			FGrassInstance Inst;
			Inst.Kind = Mix.Pick(Roll(H, 3));
			Inst.Context = Context;
			Inst.Scale = FMath::Lerp(Config.ScaleMin, Config.ScaleMax, Roll(H, 4)) * Config.KindScale[static_cast<int32>(Inst.Kind)];
			Inst.YawDeg = Roll(H, 5) * 360.0f;
			const FVector3f Up = FMath::Lerp(FVector3f(0.0f, 0.0f, 1.0f), Ground.Normal, Config.AlignToNormal);
			Inst.Up = Up.GetSafeNormal(1e-6f, FVector3f(0.0f, 0.0f, 1.0f));
			// En pendiente, el lado de abajo de la base quedaría en el aire: se hunde lo que no corrige la inclinación.
			const float NZ = FMath::Max(Ground.Normal.Z, 0.05f);
			const float Tan = FMath::Sqrt(FMath::Max(1.0f - NZ * NZ, 0.0f)) / NZ;
			const float Sink = Config.SinkM + Config.FootprintM * Inst.Scale * Tan * (1.0f - Config.AlignToNormal);
			Inst.Location = FVector(P.X, P.Y, Ground.Height - Sink);
			Out.Add(Inst);
		}
	}
}
