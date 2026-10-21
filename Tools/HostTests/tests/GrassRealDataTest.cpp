// Césped con los datos reales: Content/Data/ground_field.bin (la malla del terreno) y la disposición de
// FIslandLayout::Provisional() (copia de island.json). Sin island_props.json (el host no lee JSON): las
// cifras sobreestiman un poco lo que se ve en el juego, que además excluye los objetos colocados.
#include "HostTest.h"

#include "WorldGen/GrassFieldModel.h"
#include "WorldGen/GroundField.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <vector>

namespace GrassRealData
{
	TSharedPtr<FGroundField> LoadGround()
	{
		std::ifstream File(std::string(EXPLORED_REPO_ROOT) + "/Content/Data/ground_field.bin", std::ios::binary);
		const std::vector<char> Raw((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		TArray<uint8> Bytes(reinterpret_cast<const uint8*>(Raw.data()), static_cast<int32>(Raw.size()));
		TSharedPtr<FGroundField> Ground = MakeShared<FGroundField>();
		FString Error;
		if (!Ground->Parse(Bytes, Error))
		{
			std::fprintf(stderr, "    ground_field.bin: %s\n", Error.Std().c_str());
			return nullptr;
		}
		return Ground;
	}

	TSharedPtr<FGrassWorld> MakeWorld(const FGrassConfig& Config)
	{
		TSharedPtr<FGrassWorld> World = MakeShared<FGrassWorld>();
		World->Ground = LoadGround();
		World->Layout = FIslandLayout::Provisional();
		for (const FIslandPoint& Point : World->Layout.Points)
		{
			World->Exclusions.Add({Point.Location, Config.PointClearanceM});
		}
		return World;
	}

	int32 SettleAt(FGrassFieldModel& Model, const FVector2D& Camera, double& OutMsPerCell, int32& OutFrames, int32& OutMaxWork)
	{
		int32 Cells = 0;
		const auto Start = std::chrono::steady_clock::now();
		OutFrames = 0;
		do
		{
			const FGrassUpdate Update = Model.Update(Camera);
			Cells += Update.CellsBuilt;
			OutMaxWork = FMath::Max(OutMaxWork, Update.Work);
		}
		while (Model.GetPendingCellCount() > 0 && ++OutFrames < 5000);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		OutMsPerCell = Cells > 0 ? Ms / Cells : 0.0;
		return Model.GetLiveInstanceCount();
	}
}

HOST_TEST("Explored.WorldGen.GrassRealData.CampoDeSueloValido")
{
	const TSharedPtr<FGroundField> Ground = GrassRealData::LoadGround();
	EXPECT_TRUE(Ground != nullptr);
	if (Ground)
	{
		EXPECT_GT(Ground->GetProbeCount(), 200);
		// El Palmar es sobre todo hierba en la malla.
		int32 Grass = 0, Total = 0;
		for (int32 X = -50; X <= -10; X += 2)
		{
			for (int32 Y = -25; Y <= 15; Y += 2)
			{
				FGroundSample S;
				Total += Ground->Sample(X, Y, S) ? 1 : 0;
				Grass += S.Surface == EGroundSurface::Grass ? 1 : 0;
			}
		}
		EXPECT_EQ(Total, 441);
		EXPECT_GT(Grass, Total * 6 / 10);
	}
}

HOST_TEST("Explored.WorldGen.GrassRealData.MatasPorLugarYApoyo")
{
	using namespace GrassRealData;
	const FGrassConfig Config = FGrassConfig::Defaults();
	const TSharedPtr<FGrassWorld> World = MakeWorld(Config);
	if (!World->Ground)
	{
		EXPECT_TRUE(false);
		return;
	}
	// Triángulos de cada malla (Art/Export/K6_Cesped/manifest.json).
	const int32 Tris[GrassKindCount] = {25, 27, 25, 47, 44, 42, 100, 40, 69, 74, 61};
	const FIslandLayout& Layout = World->Layout;
	struct FSpot { const char* Name; FVector2D At; };
	const FSpot Spots[] = {
		{"palmar", Layout.FindZone(FName(TEXT("palmar")))->Center},
		{"colina_faro", Layout.FindZone(FName(TEXT("colina_faro")))->Center},
		{"cala_varada", Layout.FindZone(FName(TEXT("cala_varada")))->Center},
		{"player_start", Layout.FindPoint(FName(TEXT("player_start")))->Location},
	};
	for (const FSpot& Spot : Spots)
	{
		FGrassFieldModel Model(Config, World);
		double MsPerCell = 0.0;
		int32 Frames = 0;
		int32 MaxWork = 0;
		const int32 Live = SettleAt(Model, Spot.At, MsPerCell, Frames, MaxWork);
		int64 WorstView = 0;
		for (int32 Angle = 0; Angle < 360; Angle += 15)
		{
			const double A = FMath::DegreesToRadians(static_cast<double>(Angle));
			// FOV horizontal de 90° con un 10 % de margen, como AExploredGrassField::GetGrassTrianglesInView.
			WorstView = FMath::Max(WorstView, Model.CountTrianglesInView(Spot.At, FVector2D(FMath::Cos(A), FMath::Sin(A)), 0.7854f * 1.1f, Tris));
		}
		std::printf("    césped en %s: %d matas en %d celdas, %.2f M triángulos a la vista (peor dirección); relleno en %d frames, %.3f ms por celda, trabajo máx. %d\n",
			Spot.Name, Live, Model.GetActiveCellCount(), WorstView / 1.0e6, Frames, MsPerCell, MaxWork);
		if (std::string(Spot.Name) == "palmar")
		{
			EXPECT_GE(Live, 15000);
		}
		EXPECT_LE(Live, 90000);
		// Presupuesto de geometría (docs/v3/investigacion-cesped-arena.md): ≤ ~1,5 M a la vista.
		EXPECT_LE(WorstView, 1500000);

		// Ninguna mata fuera de hierba/tierra, en un camino ni flotando o enterrada.
		int32 Bad = 0;
		for (int32 X = -40; X <= 40; ++X)
		{
			for (int32 Y = -40; Y <= 40; ++Y)
			{
				const TArray<FGrassInstance>* Cell = Model.FindCell(FIntPoint(X, Y));
				for (int32 I = 0; Cell && I < Cell->Num(); I += 7)
				{
					const FGrassInstance& G = (*Cell)[I];
					FGroundSample S;
					const bool bOk = World->Ground->Sample(G.Location.X, G.Location.Y, S)
						&& (S.Surface == EGroundSurface::Grass || S.Surface == EGroundSurface::Dirt)
						&& G.Location.Z <= S.Height - Config.SinkM + 1e-3 && G.Location.Z >= S.Height - 0.25;
					Bad += bOk ? 0 : 1;
				}
			}
		}
		EXPECT_EQ(Bad, 0);
	}
}

HOST_TEST("Explored.WorldGen.GrassRealData.SiembraAlCaminar")
{
	using namespace GrassRealData;
	const FGrassConfig Config = FGrassConfig::Defaults();
	const TSharedPtr<FGrassWorld> World = MakeWorld(Config);
	if (!World->Ground)
	{
		EXPECT_TRUE(false);
		return;
	}
	// Del Palmar a la colina a 5 m/s y 60 fps: coste medio y peor frame de la siembra.
	FGrassFieldModel Model(Config, World);
	const FVector2D From = World->Layout.FindZone(FName(TEXT("palmar")))->Center;
	const FVector2D To = World->Layout.FindZone(FName(TEXT("colina_faro")))->Center;
	double Ms = 0.0;
	int32 Frames = 0;
	int32 MaxWork = 0;
	SettleAt(Model, From, Ms, Frames, MaxWork);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(From, To) / (5.0 / 60.0)));
	double Total = 0.0;
	double Worst = 0.0;
	for (int32 I = 1; I <= Steps; ++I)
	{
		const FVector2D Camera = From + (To - From) * (static_cast<double>(I) / Steps);
		const auto Start = std::chrono::steady_clock::now();
		Model.Update(Camera);
		const double Frame = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		Total += Frame;
		Worst = FMath::Max(Worst, Frame);
	}
	std::printf("    caminando %d frames: siembra media %.3f ms, peor frame %.2f ms\n", Steps, Total / Steps, Worst);
	EXPECT_LE(Total / Steps, 1.0);
}
