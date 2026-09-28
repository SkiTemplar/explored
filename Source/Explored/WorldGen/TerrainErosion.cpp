#include "WorldGen/TerrainErosion.h"

#include "Core/ExploredRandom.h"

void FErosionHeightGrid::Init(int32 InWidth, int32 InHeight, float FillValue)
{
	Width = FMath::Max(InWidth, 1);
	Height = FMath::Max(InHeight, 1);
	Heights.Init(FillValue, Width * Height);
}

float FErosionHeightGrid::Sample(float X, float Y) const
{
	const float Cx = FMath::Clamp(X, 0.0f, static_cast<float>(Width - 1));
	const float Cy = FMath::Clamp(Y, 0.0f, static_cast<float>(Height - 1));
	const int32 X0 = FMath::FloorToInt32(Cx);
	const int32 Y0 = FMath::FloorToInt32(Cy);
	const int32 X1 = FMath::Min(X0 + 1, Width - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
	const float Tx = Cx - X0;
	const float Ty = Cy - Y0;
	const float H00 = At(X0, Y0);
	const float H10 = At(X1, Y0);
	const float H01 = At(X0, Y1);
	const float H11 = At(X1, Y1);
	return FMath::Lerp(FMath::Lerp(H00, H10, Tx), FMath::Lerp(H01, H11, Tx), Ty);
}

FVector2D FErosionHeightGrid::Gradient(float X, float Y) const
{
	const float Cx = FMath::Clamp(X, 0.0f, static_cast<float>(Width - 1));
	const float Cy = FMath::Clamp(Y, 0.0f, static_cast<float>(Height - 1));
	const int32 X0 = FMath::FloorToInt32(Cx);
	const int32 Y0 = FMath::FloorToInt32(Cy);
	const int32 X1 = FMath::Min(X0 + 1, Width - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
	const float Tx = Cx - X0;
	const float Ty = Cy - Y0;
	const float H00 = At(X0, Y0);
	const float H10 = At(X1, Y0);
	const float H01 = At(X0, Y1);
	const float H11 = At(X1, Y1);
	// Derivadas del bilineal respecto a X e Y, en alturas por celda.
	const float Gx = (H10 - H00) * (1.0f - Ty) + (H11 - H01) * Ty;
	const float Gy = (H01 - H00) * (1.0f - Tx) + (H11 - H10) * Tx;
	return FVector2D(Gx, Gy);
}

double FErosionHeightGrid::TotalHeight() const
{
	double Sum = 0.0;
	for (float H : Heights)
	{
		Sum += H;
	}
	return Sum;
}

float FErosionHeightGrid::MaxNeighborSlope(float CellSizeMeters) const
{
	float Best = 0.0f;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const float H = At(X, Y);
			if (X + 1 < Width)
			{
				Best = FMath::Max(Best, FMath::Abs(H - At(X + 1, Y)) / CellSizeMeters);
			}
			if (Y + 1 < Height)
			{
				Best = FMath::Max(Best, FMath::Abs(H - At(X, Y + 1)) / CellSizeMeters);
			}
		}
	}
	return Best;
}

namespace
{
	/** Pincel de erosión circular con pesos que decaen linealmente hacia el borde (Beyer 2015). */
	struct FErosionBrush
	{
		TArray<FIntPoint> Offsets;
		TArray<float> Weights;
	};

	FErosionBrush BuildBrush(int32 Radius)
	{
		FErosionBrush Brush;
		float WeightSum = 0.0f;
		for (int32 Y = -Radius; Y <= Radius; ++Y)
		{
			for (int32 X = -Radius; X <= Radius; ++X)
			{
				const float Dist = FMath::Sqrt(static_cast<float>(X * X + Y * Y));
				if (Dist > Radius)
				{
					continue;
				}
				const float Weight = 1.0f - Dist / FMath::Max(1, Radius);
				Brush.Offsets.Add(FIntPoint(X, Y));
				Brush.Weights.Add(Weight);
				WeightSum += Weight;
			}
		}
		if (WeightSum > KINDA_SMALL_NUMBER)
		{
			for (float& W : Brush.Weights)
			{
				W /= WeightSum;
			}
		}
		return Brush;
	}

	/** Reparte Amount entre los cuatro nodos que rodean (X, Y) según los pesos bilineales. */
	void DepositBilinear(FErosionHeightGrid& Grid, float X, float Y, float Amount)
	{
		const int32 X0 = FMath::Clamp(FMath::FloorToInt32(X), 0, Grid.Width - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt32(Y), 0, Grid.Height - 1);
		const int32 X1 = FMath::Min(X0 + 1, Grid.Width - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Grid.Height - 1);
		const float Tx = FMath::Clamp(X - X0, 0.0f, 1.0f);
		const float Ty = FMath::Clamp(Y - Y0, 0.0f, 1.0f);
		Grid.At(X0, Y0) += Amount * (1.0f - Tx) * (1.0f - Ty);
		Grid.At(X1, Y0) += Amount * Tx * (1.0f - Ty);
		Grid.At(X0, Y1) += Amount * (1.0f - Tx) * Ty;
		Grid.At(X1, Y1) += Amount * Tx * Ty;
	}

	/** Retira Amount de la rejilla alrededor de (CenterX, CenterY) con el pincel; devuelve lo retirado. */
	float ErodeWithBrush(FErosionHeightGrid& Grid, const FErosionBrush& Brush, int32 CenterX, int32 CenterY, float Amount)
	{
		float Removed = 0.0f;
		for (int32 I = 0; I < Brush.Offsets.Num(); ++I)
		{
			const int32 Gx = CenterX + Brush.Offsets[I].X;
			const int32 Gy = CenterY + Brush.Offsets[I].Y;
			if (!Grid.IsValidCoord(Gx, Gy))
			{
				continue;
			}
			const float Delta = Amount * Brush.Weights[I] * Grid.ErodibilityAt(Gx, Gy);
			Grid.At(Gx, Gy) -= Delta;
			Removed += Delta;
		}
		return Removed;
	}

	/**
	 * Traza una gota de agua desde Pos hasta que se evapora o sale de la rejilla, erosionando
	 * cuesta abajo y depositando cuando pierde capacidad de arrastre. Sigue el modelo de
	 * Beyer 2015 / Sebastian Lague: inercia, capacidad de sedimento y pincel de erosión.
	 */
	void SimulateDroplet(FErosionHeightGrid& Grid, const FErosionParams& Params, const FErosionBrush& Brush, FVector2D Pos, FExploredRandom& Rng)
	{
		FVector2D Dir = FVector2D::ZeroVector;
		float Speed = Params.InitialSpeed;
		float Water = Params.InitialWaterVolume;
		float Sediment = 0.0f;

		for (int32 Step = 0; Step < Params.MaxDropletLifetime; ++Step)
		{
			const int32 NodeX = FMath::FloorToInt32(Pos.X);
			const int32 NodeY = FMath::FloorToInt32(Pos.Y);
			if (!Grid.IsValidCoord(NodeX, NodeY))
			{
				break;
			}

			const FVector2D Gradient = Grid.Gradient(Pos.X, Pos.Y);
			const float HeightOld = Grid.Sample(Pos.X, Pos.Y);

			// La dirección mezcla inercia (velocidad previa) y el gradiente cuesta abajo.
			Dir = Dir * Params.Inertia - Gradient * (1.0f - Params.Inertia);
			if (Dir.IsNearlyZero())
			{
				const float Angle = Rng.RangeFloat(0.0f, UE_TWO_PI);
				Dir = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
			}
			else
			{
				Dir.Normalize();
			}

			const FVector2D NewPos = Pos + Dir;
			if (!Grid.IsValidCoord(FMath::FloorToInt32(NewPos.X), FMath::FloorToInt32(NewPos.Y)))
			{
				// Sale de la rejilla: deposita aquí lo que llevaba en vez de perderlo.
				break;
			}

			const float HeightNew = Grid.Sample(NewPos.X, NewPos.Y);
			const float DeltaHeight = HeightNew - HeightOld;
			const float SedimentCapacity = FMath::Max(-DeltaHeight * Speed * Water * Params.SedimentCapacityFactor, Params.MinSedimentCapacity);

			if (Sediment > SedimentCapacity || DeltaHeight > 0.0f)
			{
				// Cuesta arriba, o llevaba más sedimento del que puede sostener: deposita.
				const float AmountToDeposit = DeltaHeight > 0.0f
					? FMath::Min(DeltaHeight, Sediment)
					: (Sediment - SedimentCapacity) * Params.DepositSpeed;
				Sediment -= AmountToDeposit;
				DepositBilinear(Grid, Pos.X, Pos.Y, AmountToDeposit);
			}
			else
			{
				const float AmountToErode = FMath::Min((SedimentCapacity - Sediment) * Params.ErodeSpeed, -DeltaHeight);
				Sediment += ErodeWithBrush(Grid, Brush, NodeX, NodeY, AmountToErode);
			}

			// Energía potencial → cinética al bajar (DropHeight > 0 cuesta abajo).
			const float DropHeight = HeightOld - HeightNew;
			Speed = FMath::Sqrt(FMath::Max(Speed * Speed + 2.0f * Params.Gravity * DropHeight, 0.0f));
			Water *= (1.0f - Params.EvaporateSpeed);

			Pos = NewPos;
			if (Water < 0.01f)
			{
				break;
			}
		}

		// Nada de sedimento desaparece de la simulación: lo que la gota llevaba al morir
		// (por agotar su vida, evaporarse o salir de la rejilla) se deposita donde estaba.
		if (Sediment > 0.0f)
		{
			DepositBilinear(Grid, Pos.X, Pos.Y, Sediment);
		}
	}
}

void FTerrainErosionModel::ErodeHydraulic(FErosionHeightGrid& Grid, const FErosionParams& InParams)
{
	const FErosionParams Params = SanitizeParams(InParams);
	if (Grid.Width < 3 || Grid.Height < 3 || Params.DropletCount <= 0)
	{
		return;
	}
	SanitizeGrid(Grid);
	const int32 Radius = FMath::Clamp(Params.ErosionRadius, 1, FMath::Max(1, FMath::Min(Grid.Width, Grid.Height) / 2 - 1));
	const FErosionBrush Brush = BuildBrush(Radius);
	FExploredRandom Rng(static_cast<uint64>(Params.Seed) ^ 0x9E3779B97F4A7C15ULL);

	for (int32 I = 0; I < Params.DropletCount; ++I)
	{
		const FVector2D Start(Rng.RangeFloat(0.0f, static_cast<float>(Grid.Width - 1)), Rng.RangeFloat(0.0f, static_cast<float>(Grid.Height - 1)));
		SimulateDroplet(Grid, Params, Brush, Start, Rng);
	}
}

namespace
{
	const FIntPoint ThermalNeighbors[8] = {
		{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
	};

	/** Cede parte del exceso sobre el talud de (X, Y) al vecino que más lo incumple. */
	void RelaxCell(FErosionHeightGrid& Grid, const FErosionParams& Params, int32 X, int32 Y)
	{
		const float Erodibility = Grid.ErodibilityAt(X, Y);
		if (Erodibility <= 0.0f)
		{
			return;
		}
		const float H = Grid.At(X, Y);
		int32 BestN = -1;
		float BestExcess = 0.0f;
		for (int32 N = 0; N < 8; ++N)
		{
			const int32 Nx = X + ThermalNeighbors[N].X;
			const int32 Ny = Y + ThermalNeighbors[N].Y;
			if (!Grid.IsValidCoord(Nx, Ny))
			{
				continue;
			}
			const float Dist = (N < 4 ? 1.0f : 1.41421356f) * Params.CellSizeMeters;
			const float NeighborExcess = H - Grid.At(Nx, Ny) - Params.TalusAngleTangent * Dist;
			if (NeighborExcess > BestExcess)
			{
				BestExcess = NeighborExcess;
				BestN = N;
			}
		}
		if (BestN >= 0)
		{
			const float Transfer = BestExcess * 0.5f * Params.ThermalTransferRate * Erodibility;
			Grid.At(X, Y) -= Transfer;
			Grid.At(X + ThermalNeighbors[BestN].X, Y + ThermalNeighbors[BestN].Y) += Transfer;
		}
	}

	/** Rellena los pozos de una sola celda (más bajos que sus 8 vecinos) hasta el vecino más bajo. */
	void FillSingleCellPits(FErosionHeightGrid& Grid)
	{
		for (int32 Y = 1; Y < Grid.Height - 1; ++Y)
		{
			for (int32 X = 1; X < Grid.Width - 1; ++X)
			{
				float Lowest = TNumericLimits<float>::Max();
				for (const FIntPoint& N : ThermalNeighbors)
				{
					Lowest = FMath::Min(Lowest, Grid.At(X + N.X, Y + N.Y));
				}
				if (Lowest > Grid.At(X, Y))
				{
					Grid.At(X, Y) = Lowest;
				}
			}
		}
	}
}

void FTerrainErosionModel::ErodeThermal(FErosionHeightGrid& Grid, const FErosionParams& InParams)
{
	const FErosionParams Params = SanitizeParams(InParams);
	if (Grid.Width < 2 || Grid.Height < 2 || Params.ThermalIterations <= 0)
	{
		return;
	}
	SanitizeGrid(Grid);

	// Relajación en el sitio (Gauss-Seidel), no por doble búfer: cada celda cede, como mucho,
	// la mitad de su exceso sobre el talud hacia un único vecino, el que más lo incumple.
	// Mover material a un solo vecino por pasada (nunca repartido entre varios a la vez)
	// hace el esquema incondicionalmente estable para ThermalTransferRate en [0, 1]; con
	// varios vecinos a la vez (una arista, no solo una ladera) la versión por lotes oscila
	// sin converger porque cada celda puede llegar a ceder más altura de la que tiene.
	// El sentido del barrido rota en cada pasada (las cuatro combinaciones de X e Y): un
	// barrido siempre igual arrastra el material en su dirección y deja taludes orientados.
	for (int32 Iter = 0; Iter < Params.ThermalIterations; ++Iter)
	{
		const bool bReverseX = (Iter & 1) != 0;
		const bool bReverseY = (Iter & 2) != 0;
		for (int32 Row = 0; Row < Grid.Height; ++Row)
		{
			const int32 Y = bReverseY ? Grid.Height - 1 - Row : Row;
			for (int32 Col = 0; Col < Grid.Width; ++Col)
			{
				RelaxCell(Grid, Params, bReverseX ? Grid.Width - 1 - Col : Col, Y);
			}
		}
	}
}

void FTerrainErosionModel::Erode(FErosionHeightGrid& Grid, const FErosionParams& Params)
{
	SanitizeGrid(Grid);
	ErodeHydraulic(Grid, Params);
	ErodeThermal(Grid, Params);
	// Las gotas dejan pozos de una celda donde el pincel rebaja más de lo que el depósito
	// bilineal repone; en el juego se ven como hoyos de agua sin salida.
	FillSingleCellPits(Grid);
}

float FErosionHeightGrid::ErodibilityAt(int32 X, int32 Y) const
{
	if (Erodibility.Num() != Heights.Num() || !IsValidCoord(X, Y))
	{
		return 1.0f;
	}
	return Erodibility[Y * Width + X];
}

namespace
{
	float FiniteOr(float Value, float Fallback, float Lo, float Hi)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Lo, Hi) : Fallback;
	}
}

FErosionParams FTerrainErosionModel::SanitizeParams(const FErosionParams& Params)
{
	const FErosionParams Defaults;
	FErosionParams Safe = Params;
	Safe.CellSizeMeters = Params.CellSizeMeters > 0.0f ? FiniteOr(Params.CellSizeMeters, Defaults.CellSizeMeters, 0.01f, 1000.0f) : Defaults.CellSizeMeters;
	Safe.DropletCount = FMath::Clamp(Params.DropletCount, 0, MaxDroplets);
	Safe.MaxDropletLifetime = FMath::Clamp(Params.MaxDropletLifetime, 1, MaxLifetime);
	Safe.ErosionRadius = FMath::Clamp(Params.ErosionRadius, 1, MaxBrushRadius);
	Safe.ThermalIterations = FMath::Clamp(Params.ThermalIterations, 0, MaxThermalIterations);
	Safe.Inertia = FiniteOr(Params.Inertia, Defaults.Inertia, 0.0f, 1.0f);
	Safe.SedimentCapacityFactor = FiniteOr(Params.SedimentCapacityFactor, Defaults.SedimentCapacityFactor, 0.0f, 100.0f);
	Safe.MinSedimentCapacity = FiniteOr(Params.MinSedimentCapacity, Defaults.MinSedimentCapacity, 0.0f, 10.0f);
	Safe.ErodeSpeed = FiniteOr(Params.ErodeSpeed, Defaults.ErodeSpeed, 0.0f, 1.0f);
	Safe.DepositSpeed = FiniteOr(Params.DepositSpeed, Defaults.DepositSpeed, 0.0f, 1.0f);
	Safe.EvaporateSpeed = FiniteOr(Params.EvaporateSpeed, Defaults.EvaporateSpeed, 0.0f, 0.99f);
	Safe.Gravity = FiniteOr(Params.Gravity, Defaults.Gravity, 0.0f, 100.0f);
	Safe.InitialWaterVolume = FiniteOr(Params.InitialWaterVolume, Defaults.InitialWaterVolume, 0.02f, 100.0f);
	Safe.InitialSpeed = FiniteOr(Params.InitialSpeed, Defaults.InitialSpeed, 0.0f, 100.0f);
	Safe.TalusAngleTangent = Params.TalusAngleTangent > 0.0f ? FiniteOr(Params.TalusAngleTangent, Defaults.TalusAngleTangent, 0.01f, 100.0f) : Defaults.TalusAngleTangent;
	Safe.ThermalTransferRate = FiniteOr(Params.ThermalTransferRate, Defaults.ThermalTransferRate, 0.0f, 1.0f);
	return Safe;
}

void FTerrainErosionModel::SanitizeGrid(FErosionHeightGrid& Grid)
{
	float Lowest = TNumericLimits<float>::Max();
	bool bAnyBad = false;
	for (float H : Grid.Heights)
	{
		bAnyBad |= !FMath::IsFinite(H);
		Lowest = FMath::IsFinite(H) ? FMath::Min(Lowest, H) : Lowest;
	}
	if (bAnyBad)
	{
		const float Fill = Lowest < TNumericLimits<float>::Max() ? Lowest : 0.0f;
		for (float& H : Grid.Heights)
		{
			H = FMath::IsFinite(H) ? H : Fill;
		}
	}
	if (Grid.Erodibility.IsEmpty())
	{
		return;
	}
	Grid.Erodibility.SetNum(Grid.Heights.Num());
	for (float& E : Grid.Erodibility)
	{
		E = FMath::IsFinite(E) ? FMath::Clamp(E, 0.0f, 1.0f) : 1.0f;
	}
}
