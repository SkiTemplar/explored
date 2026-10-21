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

	int32 SettleAt(FGrassFieldModel& Model, const FVector2D& Camera, double& OutMsPerCell)
	{
		int32 Cells = 0;
		const auto Start = std::chrono::steady_clock::now();
		int32 Guard = 0;
		do
		{
			Cells += Model.Update(Camera).CellsBuilt;
		}
		while (Model.GetPendingCellCount() > 0 && ++Guard < 2000);
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
		const int32 Live = SettleAt(Model, Spot.At, MsPerCell);
		std::printf("    césped en %s: %d matas en %d celdas (%.2f ms por celda)\n", Spot.Name, Live, Model.GetActiveCellCount(), MsPerCell);
		if (std::string(Spot.Name) == "palmar")
		{
			EXPECT_GE(Live, 15000);
			EXPECT_LE(Live, 40000);
		}
		EXPECT_LE(Live, 45000);

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
