#include "WorldGen/DrainageModel.h"

namespace
{
	/** Pendiente mínima que el relleno deja en los llanos para que el agua sepa hacia dónde ir. */
	constexpr float FlatEpsilon = 1.0e-3f;
	/** Una celda sigue en una depresión si el relleno la sube más que esto. */
	constexpr float DepressionTolerance = 5.0e-3f;

	const FIntPoint Neighbors8[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

	float NeighborDistance(int32 N) { return N < 4 ? 1.0f : 1.41421356f; }

	/** Montículo binario mínimo por (altura, índice): el desempate por índice lo hace determinista. */
	struct FCellHeap
	{
		struct FNode
		{
			float Height;
			int32 Index;
			bool operator<(const FNode& O) const { return Height < O.Height || (Height == O.Height && Index < O.Index); }
		};
		TArray<FNode> Nodes;

		bool IsEmpty() const { return Nodes.IsEmpty(); }

		void Push(float Height, int32 Index)
		{
			Nodes.Add({Height, Index});
			int32 I = Nodes.Num() - 1;
			while (I > 0 && Nodes[I] < Nodes[(I - 1) / 2])
			{
				Nodes.Swap(I, (I - 1) / 2);
				I = (I - 1) / 2;
			}
		}

		FNode Pop()
		{
			const FNode Top = Nodes[0];
			Nodes[0] = Nodes.Last();
			Nodes.Pop();
			int32 I = 0;
			for (;;)
			{
				const int32 L = 2 * I + 1;
				const int32 R = L + 1;
				int32 Best = I;
				Best = (L < Nodes.Num() && Nodes[L] < Nodes[Best]) ? L : Best;
				Best = (R < Nodes.Num() && Nodes[R] < Nodes[Best]) ? R : Best;
				if (Best == I)
				{
					return Top;
				}
				Nodes.Swap(I, Best);
				I = Best;
			}
		}
	};

	bool IsOutlet(const FErosionHeightGrid& Grid, int32 X, int32 Y, float SeaLevel)
	{
		return X == 0 || Y == 0 || X == Grid.Width - 1 || Y == Grid.Height - 1 || Grid.At(X, Y) <= SeaLevel;
	}

	/** Etiqueta (8-vecindad) una depresión y devuelve su celda más honda respecto al relleno. */
	float FloodDepression(const FErosionHeightGrid& Grid, const TArray<float>& Filled, int32 Start, int32 Id, TArray<int32>& Label)
	{
		float Deepest = 0.0f;
		TArray<int32> Stack = {Start};
		Label[Start] = Id;
		while (!Stack.IsEmpty())
		{
			const int32 Cell = Stack.Pop();
			Deepest = FMath::Max(Deepest, Filled[Cell] - Grid.Heights[Cell]);
			for (const FIntPoint& N : Neighbors8)
			{
				const int32 Nx = Cell % Grid.Width + N.X;
				const int32 Ny = Cell / Grid.Width + N.Y;
				const int32 NI = Ny * Grid.Width + Nx;
				if (Grid.IsValidCoord(Nx, Ny) && Label[NI] == INDEX_NONE && Filled[NI] - Grid.Heights[NI] > DepressionTolerance)
				{
					Label[NI] = Id;
					Stack.Add(NI);
				}
			}
		}
		return Deepest;
	}

	FDrainageParams SanitizeDrainage(const FDrainageParams& In)
	{
		const FDrainageParams D;
		auto Pick = [](float V, float Fallback, float Lo, float Hi) { return FMath::IsFinite(V) ? FMath::Clamp(V, Lo, Hi) : Fallback; };
		FDrainageParams P;
		P.SeaLevel = Pick(In.SeaLevel, D.SeaLevel, -1000.0f, 1000.0f);
		P.MaxFillDepth = Pick(In.MaxFillDepth, D.MaxFillDepth, 0.0f, 100.0f);
		P.FlowExponent = Pick(In.FlowExponent, D.FlowExponent, 0.1f, 16.0f);
		P.MinRiverArea = Pick(In.MinRiverArea, D.MinRiverArea, 1.0f, 1.0e9f);
		P.DepthPerSqrtArea = Pick(In.DepthPerSqrtArea, D.DepthPerSqrtArea, 0.0f, 10.0f);
		P.MaxRiverDepth = Pick(In.MaxRiverDepth, D.MaxRiverDepth, 0.0f, 100.0f);
		P.BaseHalfWidth = Pick(In.BaseHalfWidth, D.BaseHalfWidth, 1.5f, 16.0f);
		P.HalfWidthPerSqrtArea = Pick(In.HalfWidthPerSqrtArea, D.HalfWidthPerSqrtArea, 0.0f, 1.0f);
		P.MaxHalfWidth = Pick(In.MaxHalfWidth, D.MaxHalfWidth, 0.5f, 16.0f);
		P.MinBedHeight = Pick(In.MinBedHeight, D.MinBedHeight, -1000.0f, 1000.0f);
		return P;
	}
}

TArray<float> FDrainageModel::FilledSurface(const FErosionHeightGrid& Grid, float SeaLevel)
{
	TArray<float> Filled = Grid.Heights;
	TArray<uint8> Visited;
	Visited.Init(0, Grid.Heights.Num());
	FCellHeap Heap;
	for (int32 Y = 0; Y < Grid.Height; ++Y)
	{
		for (int32 X = 0; X < Grid.Width; ++X)
		{
			if (IsOutlet(Grid, X, Y, SeaLevel))
			{
				Visited[Y * Grid.Width + X] = 1;
				Heap.Push(Grid.At(X, Y), Y * Grid.Width + X);
			}
		}
	}
	while (!Heap.IsEmpty())
	{
		const FCellHeap::FNode Cell = Heap.Pop();
		for (const FIntPoint& N : Neighbors8)
		{
			const int32 Nx = Cell.Index % Grid.Width + N.X;
			const int32 Ny = Cell.Index / Grid.Width + N.Y;
			const int32 NI = Ny * Grid.Width + Nx;
			if (Grid.IsValidCoord(Nx, Ny) && !Visited[NI])
			{
				Visited[NI] = 1;
				Filled[NI] = FMath::Max(Grid.Heights[NI], Filled[Cell.Index] + FlatEpsilon);
				Heap.Push(Filled[NI], NI);
			}
		}
	}
	return Filled;
}

void FDrainageModel::FillShallowDepressions(FErosionHeightGrid& Grid, float SeaLevel, float MaxFillDepth)
{
	FTerrainErosionModel::SanitizeGrid(Grid);
	const TArray<float> Filled = FilledSurface(Grid, SeaLevel);
	TArray<int32> Label;
	Label.Init(INDEX_NONE, Grid.Heights.Num());
	TArray<uint8> Shallow;
	for (int32 Cell = 0; Cell < Grid.Heights.Num(); ++Cell)
	{
		if (Label[Cell] == INDEX_NONE && Filled[Cell] - Grid.Heights[Cell] > DepressionTolerance)
		{
			Shallow.Add(FloodDepression(Grid, Filled, Cell, Shallow.Num(), Label) <= MaxFillDepth ? 1 : 0);
		}
	}
	for (int32 Cell = 0; Cell < Grid.Heights.Num(); ++Cell)
	{
		if (Label[Cell] != INDEX_NONE && Shallow[Label[Cell]])
		{
			Grid.Heights[Cell] = Filled[Cell];
		}
	}
}

TArray<float> FDrainageModel::FlowAccumulation(const FErosionHeightGrid& InGrid, float SeaLevel, float Exponent)
{
	FErosionHeightGrid Grid = InGrid;
	FTerrainErosionModel::SanitizeGrid(Grid);
	const TArray<float> Filled = FilledSurface(Grid, SeaLevel);
	TArray<int32> Order;
	Order.SetNum(Filled.Num());
	for (int32 I = 0; I < Order.Num(); ++I)
	{
		Order[I] = I;
	}
	Order.Sort([&Filled](int32 A, int32 B) { return Filled[A] > Filled[B] || (Filled[A] == Filled[B] && A < B); });

	TArray<float> Flow;
	Flow.Init(1.0f, Filled.Num());
	for (int32 Cell : Order)
	{
		const int32 X = Cell % Grid.Width;
		const int32 Y = Cell / Grid.Width;
		if (IsOutlet(Grid, X, Y, SeaLevel))
		{
			continue;
		}
		float Weights[8] = {};
		float Sum = 0.0f;
		for (int32 N = 0; N < 8; ++N)
		{
			const int32 NI = (Y + Neighbors8[N].Y) * Grid.Width + X + Neighbors8[N].X;
			const float Drop = Filled[Cell] - Filled[NI];
			Weights[N] = Drop > 0.0f ? FMath::Pow(Drop / NeighborDistance(N), Exponent) : 0.0f;
			Sum += Weights[N];
		}
		for (int32 N = 0; N < 8 && Sum > 0.0f; ++N)
		{
			Flow[(Y + Neighbors8[N].Y) * Grid.Width + X + Neighbors8[N].X] += Flow[Cell] * Weights[N] / Sum;
		}
	}
	return Flow;
}

namespace
{
	/** Marca, con sección en U, cuánto hay que rebajar alrededor de un tramo de río. */
	void StampSection(const FErosionHeightGrid& Grid, TArray<float>& Carve, int32 X, int32 Y, float Depth, float HalfWidth)
	{
		const int32 Reach = FMath::CeilToInt32(HalfWidth);
		for (int32 Dy = -Reach; Dy <= Reach; ++Dy)
		{
			for (int32 Dx = -Reach; Dx <= Reach; ++Dx)
			{
				const float D = FMath::Sqrt(static_cast<float>(Dx * Dx + Dy * Dy)) / HalfWidth;
				if (D >= 1.0f || !Grid.IsValidCoord(X + Dx, Y + Dy))
				{
					continue;
				}
				const int32 I = (Y + Dy) * Grid.Width + X + Dx;
				Carve[I] = FMath::Max(Carve[I], Depth * (1.0f - D * D * (2.0f - D * D)));
			}
		}
	}

	/**
	 * Suavizado gaussiano separable (núcleo 1-4-6-4-1). Las secciones se estampan centradas en
	 * celdas enteras: sin suavizar, el lecho queda con hoyuelos cuya pendiente apunta a la celda
	 * vecina, es decir, a los ejes y diagonales de la rejilla.
	 */
	TArray<float> Blur5(const TArray<float>& In, int32 Width, int32 Height)
	{
		static const float Kernel[5] = {1.0f / 16.0f, 4.0f / 16.0f, 6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f};
		TArray<float> Tmp;
		Tmp.Init(0.0f, In.Num());
		TArray<float> Out;
		Out.Init(0.0f, In.Num());
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				float Sum = 0.0f;
				for (int32 K = -2; K <= 2; ++K)
				{
					Sum += Kernel[K + 2] * In[Y * Width + FMath::Clamp(X + K, 0, Width - 1)];
				}
				Tmp[Y * Width + X] = Sum;
			}
		}
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				float Sum = 0.0f;
				for (int32 K = -2; K <= 2; ++K)
				{
					Sum += Kernel[K + 2] * Tmp[FMath::Clamp(Y + K, 0, Height - 1) * Width + X];
				}
				Out[Y * Width + X] = Sum;
			}
		}
		return Out;
	}

	/** Rebaje sin suavizar de todos los tramos de río. */
	TArray<float> StampRivers(const FErosionHeightGrid& Grid, const FDrainageParams& Params, const TArray<float>& Flow, const TArray<float>& Filled)
	{
		TArray<float> Carve;
		Carve.Init(0.0f, Grid.Heights.Num());
		for (int32 Y = 0; Y < Grid.Height; ++Y)
		{
			for (int32 X = 0; X < Grid.Width; ++X)
			{
				const int32 I = Y * Grid.Width + X;
				const float H = Grid.Heights[I];
				// Ni en el mar ni dentro de un lago (depresión honda que se ha respetado).
				if (Flow[I] < Params.MinRiverArea || H <= Params.SeaLevel || Filled[I] - H > DepressionTolerance)
				{
					continue;
				}
				const float Root = FMath::Sqrt(Flow[I]);
				const float Head = FMath::SmoothStep(Params.MinRiverArea, 2.0f * Params.MinRiverArea, Flow[I]);
				const float Depth = FMath::Min(Params.MaxRiverDepth, Params.DepthPerSqrtArea * Root) * Head;
				const float HalfWidth = FMath::Min(Params.MaxHalfWidth, Params.BaseHalfWidth + Params.HalfWidthPerSqrtArea * Root);
				StampSection(Grid, Carve, X, Y, Depth, HalfWidth);
			}
		}
		return Carve;
	}
}

TArray<float> FDrainageModel::CarveRivers(FErosionHeightGrid& Grid, const FDrainageParams& InParams)
{
	const FDrainageParams Params = SanitizeDrainage(InParams);
	FillShallowDepressions(Grid, Params.SeaLevel, Params.MaxFillDepth);
	const TArray<float> Flow = FlowAccumulation(Grid, Params.SeaLevel, Params.FlowExponent);
	const TArray<float> Filled = FilledSurface(Grid, Params.SeaLevel);
	const TArray<float> Original = Grid.Heights;
	const TArray<float> Carve = Blur5(StampRivers(Grid, Params, Flow, Filled), Grid.Width, Grid.Height);

	for (int32 I = 0; I < Original.Num(); ++I)
	{
		// El lecho no baja de la cota mínima, y lo que ya estaba más bajo no se toca.
		const float Floor = FMath::Min(Params.MinBedHeight, Original[I]);
		Grid.Heights[I] = FMath::Max(Original[I] - Carve[I], Floor);
	}
	// Donde dos cauces se cruzan puede quedar algún pozo: se rellena, sin tocar lagos.
	FillShallowDepressions(Grid, Params.SeaLevel, 0.5f);

	TArray<float> Carved;
	Carved.Init(0.0f, Original.Num());
	for (int32 I = 0; I < Original.Num(); ++I)
	{
		Carved[I] = FMath::Max(0.0f, Original[I] - Grid.Heights[I]);
	}
	return Carved;
}
