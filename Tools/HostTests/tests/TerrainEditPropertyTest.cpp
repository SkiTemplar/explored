// Propiedades del terreno editable con ediciones aleatorias (semilla fija): cada
// herramienta solo toca muestras dentro de su pincel, el volumen que declara es el
// que cambia de verdad, ensucia exactamente los chunks que leen lo cambiado y el
// guardado y la repetición dan el mismo estado.
#include "HostTest.h"

#include "WorldGen/TerrainEditModel.h"

#include <cmath>
#include <map>
#include <random>
#include <set>
#include <tuple>

namespace
{
	using FKey = std::tuple<int32, int32, int32>;

	/** Ladera ondulada con una pared casi vertical en X = 3: pico y pala encuentran de todo. */
	float Ground(const FVector& P)
	{
		const double Wall = P.X > 3.0 ? P.Z - 2.5 : P.Z;
		return static_cast<float>(Wall + 0.25 * std::sin(P.X * 1.7) * std::cos(P.Y * 1.3) - 0.2 * P.Y);
	}

	FKey Key(const FIntVector& G) { return { G.X, G.Y, G.Z }; }

	/** Todas las muestras editadas con su delta en metros. */
	std::map<FKey, float> Deltas(const FTerrainEditModel& Model)
	{
		std::map<FKey, float> Out;
		const int32 N = Model.GetSettings().CellsPerChunk;
		for (const FIntVector& C : Model.EditedChunks())
		{
			for (int32 Z = 0; Z < N; ++Z)
				for (int32 Y = 0; Y < N; ++Y)
					for (int32 X = 0; X < N; ++X)
					{
						const FIntVector G(C.X * N + X, C.Y * N + Y, C.Z * N + Z);
						const float D = Model.SampleDelta(G);
						if (D != 0.0f)
						{
							Out[Key(G)] = D;
						}
					}
		}
		return Out;
	}

	struct FOp
	{
		int32 Kind = 0;
		FVector Center;
		FVector Dir;
		float A = 0.0f;
		float B = 0.0f;
		double Budget = 0.0;
		uint32 Seed = 0;
		int32 Tier = 2;
		ETerrainMaterial Material = ETerrainMaterial::Tierra;
	};

	FOp RandomOp(std::mt19937& Rng)
	{
		std::uniform_real_distribution<double> U(0.0, 1.0);
		FOp Op;
		Op.Kind = static_cast<int32>(Rng() % 5);
		// Centros alrededor de las esquinas de los chunks (8 m) para cruzar bordes.
		Op.Center = FVector(-1.0 + 10.0 * U(Rng), -1.0 + 10.0 * U(Rng), -1.5 + 4.0 * U(Rng));
		if (U(Rng) < 0.3)
		{
			Op.Center.X = 8.0 + (U(Rng) - 0.5) * 0.3;
		}
		Op.Dir = FVector(U(Rng) - 0.5, U(Rng) - 0.5, U(Rng) - 0.8);
		Op.A = static_cast<float>(0.1 + 1.2 * U(Rng));
		Op.B = static_cast<float>(0.1 + 1.0 * U(Rng));
		Op.Budget = 0.6 * U(Rng);
		Op.Seed = static_cast<uint32>(Rng());
		Op.Tier = 2 + static_cast<int32>(Rng() % 3);
		Op.Material = static_cast<ETerrainMaterial>(Rng() % 4);
		return Op;
	}

	/** Aplica la operación y devuelve el resultado y la distancia máxima a la que puede tocar. */
	FTerrainEditResult Apply(FTerrainEditModel& Model, const FOp& Op, double& OutReach, double& OutBudget, double& OutMaxVolume)
	{
		const auto Base = [](const FVector& P) { return Ground(P); };
		const float Cell = Model.GetSettings().CellSize;
		OutBudget = -1.0;
		OutMaxVolume = -1.0;
		switch (Op.Kind)
		{
		case 0:
		{
			FPickaxeHit Hit;
			Hit.ImpactPoint = Op.Center;
			Hit.Direction = Op.Dir;
			Hit.Material = Op.Material;
			Hit.ToolTier = Op.Tier;
			Hit.Seed = Op.Seed;
			Hit.Radius = Op.A;
			const float R = FMath::Clamp(Op.A, FTerrainEditModel::MinPickaxeRadius, FTerrainEditModel::MaxPickaxeRadius);
			OutReach = FTerrainEditModel::PickaxeBite + R * (1.0 + FTerrainEditModel::PickaxeIrregularity);
			return Model.Pickaxe(Hit, Base);
		}
		case 1:
		{
			FShovelStroke S;
			S.Center = Op.Center;
			S.PlaneNormal = FVector(Op.Dir.X * 0.4, Op.Dir.Y * 0.4, 1.0);
			S.Radius = Op.A;
			S.EdgeWidth = Op.B;
			S.VerticalReach = 0.3f + Op.B;
			S.SoilBudget = Op.Budget;
			OutBudget = Op.Budget;
			// Cota de la muestra más lejana: horizontal del pincel más alcance vertical.
			OutReach = std::sqrt(double(Op.A + Op.B) * (Op.A + Op.B) * 2.0 + double(S.VerticalReach + Op.A + Op.B) * (S.VerticalReach + Op.A + Op.B)) + Cell;
			return Model.Shovel(S, Base);
		}
		case 2:
		{
			FSoilPlacement P;
			P.Center = Op.Center;
			P.Radius = Op.A;
			P.SoilBudget = Op.Budget;
			OutBudget = Op.Budget;
			OutReach = Op.A + Cell;
			return Model.PlaceSoil(P, Base);
		}
		case 3:
		{
			FSphereDig D;
			D.Center = Op.Center;
			D.Radius = Op.A;
			D.ToolTier = Op.Tier;
			D.Material = Op.Material;
			D.MaxVolume = Op.Budget;
			OutMaxVolume = Op.Budget > 0.0 ? Op.Budget : -1.0;
			OutReach = Model.SphereDigReach(Op.A);
			return Model.DigSphere(D, Base);
		}
		default:
		{
			FStairCarve In;
			In.Start = Op.Center;
			In.Direction = Op.Dir;
			In.StepRise = (Op.Seed & 1) ? 0.3f : -0.3f;
			In.StepRun = 0.3f;
			In.NumSteps = 1 + static_cast<int32>(Op.Seed % 5);
			In.Width = Op.A;
			In.Headroom = 1.0f + Op.B;
			In.ToolTier = Op.Tier;
			In.MaxVolume = Op.Budget;
			OutMaxVolume = Op.Budget > 0.0 ? Op.Budget : -1.0;
			FStairCarve S;
			if (!FTerrainEditModel::SnapStairs(In, S))
			{
				OutReach = 0.0;
				return FTerrainEditResult();
			}
			const double Len = S.NumSteps * S.StepRun;
			const double Rise = S.NumSteps * std::fabs(S.StepRise) + S.Headroom;
			OutReach = std::sqrt((Len + Cell) * (Len + Cell) + double(S.Width) * S.Width + (Rise + Cell) * (Rise + Cell)) + 2.0 * Cell;
			// Las escaleras se miden desde su pie, no desde el centro de la operación.
			const_cast<FOp&>(Op).Center = S.Start;
			return Model.CarveStairs(S, Base);
		}
		}
	}
}

HOST_TEST("Explored.TerrainEditProperty ediciones aleatorias: pincel, volumen, chunks y guardado")
{
	FTerrainEditModel Model;
	const double Cell = Model.GetSettings().CellSize;
	const double Cell3 = Cell * Cell * Cell;
	const auto Base = [](const FVector& P) { return Ground(P); };
	std::mt19937 Rng(20260929u);
	std::vector<FOp> Ops;
	int32 Failures = 0;

	for (int32 Step = 0; Step < 160 && Failures < 5; ++Step)
	{
		FOp Op = RandomOp(Rng);
		const std::map<FKey, float> Before = Deltas(Model);
		double Reach = 0.0, Budget = -1.0, MaxVolume = -1.0;
		const FTerrainEditResult R = Apply(Model, Op, Reach, Budget, MaxVolume);
		Ops.push_back(Op);
		const std::map<FKey, float> After = Deltas(Model);

		std::set<FKey> Changed;
		for (const auto& [K, D] : After)
		{
			auto It = Before.find(K);
			if (It == Before.end() || It->second != D)
				Changed.insert(K);
		}
		for (const auto& [K, D] : Before)
		{
			if (!After.count(K))
				Changed.insert(K);
		}

		double Net = 0.0;
		std::set<FKey> Expected;
		const int32 Kind = Op.Kind;
		for (const FKey& K : Changed)
		{
			const FIntVector G(std::get<0>(K), std::get<1>(K), std::get<2>(K));
			const FVector P = Model.SamplePosition(G);
			const double Dist = (P - Op.Center).Size();
			if (Dist > Reach + 1.0e-4)
			{
				++Failures;
				HostTest::Fail(__FILE__, __LINE__, "herramienta " + std::to_string(Kind) + " toca una muestra a "
					+ std::to_string(Dist) + " m, fuera de su alcance " + std::to_string(Reach));
				break;
			}
			const float BaseV = Ground(P);
			auto It = Before.find(K);
			const float Old = BaseV + (It == Before.end() ? 0.0f : It->second);
			const float New = Model.SampleDensity(G, Base);
			Net += (FTerrainEditModel::Occupancy(New, Cell) - FTerrainEditModel::Occupancy(Old, Cell)) * Cell3;
			TArray<FIntVector> Readers;
			Model.ChunksReadingSample(G, Readers);
			for (const FIntVector& C : Readers)
				Expected.insert(Key(C));
		}

		EXPECT_EQ(static_cast<int32>(Changed.size()), R.SamplesChanged);
		const double Declared = R.VolumeAdded - R.VolumeRemoved;
		// 1 mm de cuantización por muestra mueve como mucho 4 milésimas de ocupación.
		const double Tol = 1.0e-6 + Changed.size() * Cell3 * 0.0045;
		if (std::fabs(Net - Declared) > Tol)
		{
			++Failures;
			HostTest::Fail(__FILE__, __LINE__, "herramienta " + std::to_string(Kind) + ": declara " + std::to_string(Declared)
				+ " m³ y cambia " + std::to_string(Net) + " m³");
		}
		// La pala rellena con lo que lleva más lo que corta en la misma pasada.
		const double Carried = Kind == 1 ? Budget + R.VolumeRemoved : Budget;
		if (Budget >= 0.0 && R.VolumeAdded > Carried + 1.0e-9)
			HostTest::Fail(__FILE__, __LINE__, "herramienta " + std::to_string(Kind) + " añade " + std::to_string(R.VolumeAdded * 1e6) + " cm³ con " + std::to_string(Carried * 1e6));
		if (MaxVolume >= 0.0 && R.VolumeRemoved > MaxVolume + 1.0e-9)
			HostTest::Fail(__FILE__, __LINE__, "herramienta " + std::to_string(Kind) + " quita " + std::to_string(R.VolumeRemoved * 1e6) + " cm³ con tope " + std::to_string(MaxVolume * 1e6));

		std::set<FKey> Dirty;
		for (const FIntVector& C : R.DirtyChunks)
			Dirty.insert(Key(C));
		for (const FKey& K : Expected)
		{
			if (!Dirty.count(K))
			{
				++Failures;
				HostTest::Fail(__FILE__, __LINE__, "herramienta " + std::to_string(Kind) + " no marca sucio un chunk que lee una muestra cambiada");
				break;
			}
		}
		if (Kind != 1)
		{
			// La pala también ensucia por compactar sin cambiar forma; el resto no ensucia de más.
			if (Dirty.size() != Expected.size())
				HostTest::Fail(__FILE__, __LINE__, "herramienta " + std::to_string(Kind) + " ensucia " + std::to_string(Dirty.size()) + " chunks y cambia muestras de " + std::to_string(Expected.size()));
		}
	}

	// Guardado en las dos versiones y repetición desde cero.
	for (int32 Version : { 1, 2 })
	{
		FTerrainEditModel Loaded;
		EXPECT_TRUE(Loaded.FromValue(Model.ToValue(Version)));
		EXPECT_TRUE(Loaded == Model);
	}
	FTerrainEditModel Replay;
	for (const FOp& Op : Ops)
	{
		double A, B, C;
		Apply(Replay, Op, A, B, C);
	}
	EXPECT_TRUE(Replay == Model);
}
