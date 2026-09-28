#include "Boats/RaftFurrowModel.h"

namespace RaftFurrowDetail
{
	bool IsFiniteValue(double V)
	{
		return FMath::IsFinite(V) && FMath::Abs(V) < 1.0e30;
	}

	bool ColumnLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
	}

	void SortUnique(TArray<FIntPoint>& List)
	{
		List.Sort(&ColumnLess);
		TArray<FIntPoint> Unique;
		for (const FIntPoint& C : List)
		{
			if (Unique.Num() == 0 || Unique.Last() != C)
			{
				Unique.Add(C);
			}
		}
		List = MoveTemp(Unique);
	}

	void Append(FSandResult& Into, const FSandResult& From)
	{
		Into.DirtyChunks.Append(From.DirtyChunks);
		Into.ChangedColumns.Append(From.ChangedColumns);
		Into.Mass += From.Mass;
	}

	/** Caja en planta de una pieza del fondo, en el marco del casco (cm). */
	struct FFootBox
	{
		double MinX, MaxX, MinY, MaxY;
	};

	/** Columna de la huella: lado respecto al eje (cm) y si la superficie de debajo es arena mojada. */
	struct FFootCell
	{
		double LY = 0.0;
		bool bWetSurface = false;
	};

	/** Media pieza más grande que se marca (cm): lo mismo que FRaftYardModel deja guardar (50 m de lado). */
	constexpr double MaxFootHalfCm = 2500.0;
}

int32 FRaftFurrowModel::SinkMm(float PressureKPa, bool bWet)
{
	if (!RaftFurrowDetail::IsFiniteValue(PressureKPa) || PressureKPa <= 0.0f)
	{
		return 0;
	}
	const float Sink = PressureKPa * (bWet ? WetSinkMmPerKPa : DrySinkMmPerKPa);
	const int32 Mm = FMath::Min(FMath::FloorToInt(FMath::Min(Sink, 1.0e6f)), MaxSinkMm);
	return Mm < MinSinkMm ? 0 : Mm;
}

bool FRaftFurrowModel::IsSandSurface(ELaunchSurface Surface)
{
	return Surface == ELaunchSurface::Sand || Surface == ELaunchSurface::WetSand;
}

FRaftFurrowResult FRaftFurrowModel::Drag(const FRaftYardModel& Yard, float SFrom, float STo, double HighTide, bool bRaining,
	FSandModel& Sand, FSandModel::FBaseHeight Base)
{
	using namespace RaftFurrowDetail;
	FRaftFurrowResult Result;
	const FLaunchPath& Path = Yard.GetPath();
	if (Yard.GetState() != ERaftYardState::Ashore || Yard.IsOnRollers() || !IsFiniteValue(SFrom) || !IsFiniteValue(STo)
		|| SFrom == STo || !IsFiniteValue(HighTide) || !IsFiniteValue(Path.StartCm.X) || !IsFiniteValue(Path.StartCm.Y)
		|| !IsFiniteValue(Path.YawDeg))
	{
		return Result;
	}

	// Huella: la caja en planta de cada pieza del fondo.
	TArray<FFootBox> Boxes;
	double AreaM2 = 0.0;
	const TArray<FHullPiece>& Pieces = Yard.GetHull().GetPieces();
	for (int32 I = 0; I < Pieces.Num(); ++I)
	{
		if (!Yard.IsBottomPiece(I))
		{
			continue;
		}
		FVector Half = FHullAssemblyModel::EffectiveSizeCm(Pieces[I]) * 0.5;
		const FVector& C = Pieces[I].CenterCm;
		// Una pieza desmesurada recorrería millones de columnas por muestra: no se marca.
		if (!IsFiniteValue(C.X) || !IsFiniteValue(C.Y) || !IsFiniteValue(Half.X) || !IsFiniteValue(Half.Y) || Half.X <= 0.0
			|| Half.Y <= 0.0 || Half.X > MaxFootHalfCm || Half.Y > MaxFootHalfCm)
		{
			continue;
		}
		AreaM2 += 4.0 * Half.X * Half.Y / 1.0e4;
		// Una pieza más estrecha que la celda (un tronco de 22 cm) podría caer entre dos puntos
		// de la rejilla y no marcar nada: su huella ocupa al menos una celda.
		const double MinHalf = Sand.GetSettings().CellSize * 50.0;
		Half.X = FMath::Max(Half.X, MinHalf);
		Half.Y = FMath::Max(Half.Y, MinHalf);
		Boxes.Add({ C.X - Half.X, C.X + Half.X, C.Y - Half.Y, C.Y + Half.Y });
	}
	if (Boxes.Num() == 0 || AreaM2 <= 0.0)
	{
		return Result;
	}

	// Presión: el peso que no sostiene el agua, repartido en la huella.
	const float Slope = Path.SlopeRadAt(Yard.GetCenterS());
	const double NormalN = Yard.TotalMassKg() * FRaftYardModel::Gravity * FMath::Cos(Slope) * (1.0f - Yard.WaterSupport01());
	Result.PressureKPa = static_cast<float>(FMath::Max(NormalN, 0.0) / AreaM2 / 1000.0);
	Result.DrySinkMm = SinkMm(Result.PressureKPa, false);
	Result.WetSinkMm = SinkMm(Result.PressureKPa, true);
	if (Result.DrySinkMm == 0 && Result.WetSinkMm == 0)
	{
		return Result;
	}

	// Marco del camino (m): hacia delante y hacia estribor. El rumbo es el mismo en todo el camino.
	const double Yaw = FMath::DegreesToRadians(static_cast<double>(Path.YawDeg));
	const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw));
	const FVector2D Right(-Fwd.Y, Fwd.X);
	const FVector2D Start(Path.StartCm.X / 100.0, Path.StartCm.Y / 100.0);
	const double Cell = Sand.GetSettings().CellSize;

	// Tramo muestreado cada media celda (o más espaciado si el salto es enorme).
	const double Total = Path.TotalLengthCm();
	const double S0 = FMath::Clamp(static_cast<double>(SFrom), 0.0, Total);
	const double S1 = FMath::Clamp(static_cast<double>(STo), 0.0, Total);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(static_cast<float>(FMath::Abs(S1 - S0) / (Cell * 50.0))), 1, MaxStations);
	// Columna → coordenada lateral (cm) respecto al eje del camino.
	TMap<FIntPoint, FFootCell> Footprint;
	// Superficie bajo un punto del camino; pasado el final del camino, arena.
	const auto SurfaceAt = [&Path](double S)
	{
		const int32 Segment = Path.SegmentIndexAt(static_cast<float>(S));
		return Segment == INDEX_NONE ? ELaunchSurface::Sand : Path.Segments[Segment].Surface;
	};
	for (int32 K = 0; K <= Steps; ++K)
	{
		const double S = S0 + (S1 - S0) * K / Steps;
		const FVector2D Origin = Start + Fwd * (S / 100.0);
		for (const FFootBox& Box : Boxes)
		{
			// Caja envolvente en el mundo de la caja girada (m), con una celda de margen.
			double MinWX = TNumericLimits<double>::Max(), MinWY = MinWX, MaxWX = -MinWX, MaxWY = -MinWX;
			for (const double LX : { Box.MinX, Box.MaxX })
			{
				for (const double LY : { Box.MinY, Box.MaxY })
				{
					const FVector2D W = Origin + Fwd * (LX / 100.0) + Right * (LY / 100.0);
					MinWX = FMath::Min(MinWX, W.X);
					MaxWX = FMath::Max(MaxWX, W.X);
					MinWY = FMath::Min(MinWY, W.Y);
					MaxWY = FMath::Max(MaxWY, W.Y);
				}
			}
			if (!IsFiniteValue(MinWX) || !IsFiniteValue(MaxWX) || !IsFiniteValue(MinWY) || !IsFiniteValue(MaxWY)
				|| FMath::Abs(MinWX) > FSandModel::MaxAbsColumn * Cell * 0.5 || FMath::Abs(MaxWX) > FSandModel::MaxAbsColumn * Cell * 0.5
				|| FMath::Abs(MinWY) > FSandModel::MaxAbsColumn * Cell * 0.5 || FMath::Abs(MaxWY) > FSandModel::MaxAbsColumn * Cell * 0.5)
			{
				return Result;
			}
			const FIntPoint Lo = Sand.ColumnOf(MinWX - Cell, MinWY - Cell);
			const FIntPoint Hi = Sand.ColumnOf(MaxWX + Cell, MaxWY + Cell);
			for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
			{
				for (int32 X = Lo.X; X <= Hi.X; ++X)
				{
					const FIntPoint Column(X, Y);
					const FVector2D P = Sand.ColumnPosition(Column) - Origin;
					const double LX = FVector2D::DotProduct(P, Fwd) * 100.0;
					const double LY = FVector2D::DotProduct(P, Right) * 100.0;
					if (LX >= Box.MinX && LX <= Box.MaxX && LY >= Box.MinY && LY <= Box.MaxY)
					{
						// La superficie cuenta bajo cada columna, no bajo el centro del casco: con el
						// centro en la roca y la proa en la arena, la proa marca; al revés, no.
						const ELaunchSurface Surface = SurfaceAt(S + LX);
						if (IsSandSurface(Surface))
						{
							Footprint.Add(Column, { LY, Surface == ELaunchSurface::WetSand });
						}
					}
				}
			}
		}
	}
	Result.FootprintColumns = Footprint.Num();
	if (Footprint.Num() == 0)
	{
		return Result;
	}

	TArray<FIntPoint> Columns;
	Footprint.GetKeys(Columns);
	Columns.Sort(&ColumnLess);

	// Primera columna fuera de la huella hacia un costado y cuántos pasos de celda ha costado.
	const int32 MaxWalk = 256;
	auto Berm = [&](const FIntPoint& From, double Side, int32& OutSteps)
	{
		const FVector2D P = Sand.ColumnPosition(From);
		for (int32 K = 1; K <= MaxWalk; ++K)
		{
			const FVector2D Q = P + Right * (Side * K * Cell);
			const FIntPoint To = Sand.ColumnOf(Q.X, Q.Y);
			if (To != From && !Footprint.Contains(To))
			{
				OutSteps = K;
				return To;
			}
		}
		OutSteps = MaxWalk + 1;
		return From;
	};

	FSandResult& Moved = Result.Sand;
	for (const FIntPoint& Column : Columns)
	{
		const FFootCell& Foot = Footprint.FindChecked(Column);
		const bool bWet = bRaining || Foot.bWetSurface || Sand.Height(Column, Base) <= HighTide;
		const int32 Sink = bWet ? Result.WetSinkMm : Result.DrySinkMm;
		if (Sink == 0 || Sand.IsAnchored(Column))
		{
			continue;
		}
		int64 Excess = static_cast<int64>(Sand.DeltaMm(Column)) + Sink;
		if (Excess <= 0)
		{
			continue;
		}
		int32 StepsR = 0, StepsL = 0;
		const FIntPoint ToR = Berm(Column, 1.0, StepsR);
		const FIntPoint ToL = Berm(Column, -1.0, StepsL);
		// Costado más cercano; a igual distancia, hacia fuera del eje, y en el eje, por paridad.
		const double LY = Foot.LY;
		bool bRightFirst = StepsR != StepsL ? StepsR < StepsL : (LY != 0.0 ? LY > 0.0 : ((Column.X + Column.Y) & 1) == 0);
		for (int32 Try = 0; Try < 2 && Excess > 0; ++Try)
		{
			const bool bRight = (Try == 0) == bRightFirst;
			const FIntPoint To = bRight ? ToR : ToL;
			if (To == Column)
			{
				continue;
			}
			FSandMove Move;
			Move.From = Column;
			Move.To = To;
			Move.Mm = static_cast<int32>(FMath::Min<int64>(Excess, MAX_int32));
			const FSandResult R = Sand.Transfer({ Move }, Base);
			Append(Moved, R);
			Excess -= R.Mass;
		}
		Result.BlockedMass += Excess;
	}
	SortUnique(Moved.DirtyChunks);
	SortUnique(Moved.ChangedColumns);
	Moved.ColumnsChanged = Moved.ChangedColumns.Num();
	return Result;
}
