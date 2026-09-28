// Herramienta de diagnóstico del terreno del archipiélago (fuera de la suite de tests). No
// se compila en el binario de tests: es un programa aparte que muestrea el campo de alturas
// completo y saca cifras + un volcado binario para convertirlo en PNG. Uso:
//   TerrainDiagnostics <fichero_salida.bin> [seed] [spacing_m]
//
// El fichero de salida es un volcado crudo: cabecera de 4 int32 (Width, Height, OriginX,
// OriginY en metros) seguida de Width*Height floats (altura en metros) y Width*Height
// int32 (índice de isla o -1). Tools/Art/terrain_diagnostics_png.py lo convierte en PNG.
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
	struct FGrid
	{
		int32 Width = 0;
		int32 Height = 0;
		float OriginX = 0.0f;
		float OriginY = 0.0f;
		float Spacing = 1.0f;
		std::vector<float> Heights;
		std::vector<int32> IslandIndex;

		int32 IndexOf(int32 X, int32 Y) const { return Y * Width + X; }
	};

	FGrid SampleWorld(const FTerrainDensity& Density, float HalfExtent, float Spacing)
	{
		FGrid Grid;
		Grid.OriginX = -HalfExtent;
		Grid.OriginY = -HalfExtent;
		Grid.Spacing = Spacing;
		Grid.Width = static_cast<int32>((2.0f * HalfExtent) / Spacing) + 1;
		Grid.Height = Grid.Width;
		Grid.Heights.resize(static_cast<size_t>(Grid.Width) * Grid.Height);
		Grid.IslandIndex.resize(static_cast<size_t>(Grid.Width) * Grid.Height);

		for (int32 Y = 0; Y < Grid.Height; ++Y)
		{
			const float WorldY = Grid.OriginY + Y * Spacing;
			for (int32 X = 0; X < Grid.Width; ++X)
			{
				const float WorldX = Grid.OriginX + X * Spacing;
				const FTerrainColumn Column = Density.SampleColumn(WorldX, WorldY);
				const int32 I = Grid.IndexOf(X, Y);
				Grid.Heights[static_cast<size_t>(I)] = Column.Height;
				Grid.IslandIndex[static_cast<size_t>(I)] = Column.IslandIndex;
			}
		}
		return Grid;
	}

	/** Componente conexa de celdas "someras" (Height > Threshold), 4-conectividad. */
	struct FComponent
	{
		int32 CellCount = 0;
		/** true si esta componente contiene la celda semilla (centro) de alguna isla: conectada
		 * por encima del umbral a tierra firme real, no solo etiquetada con su IslandIndex (un
		 * cayo satélite también lleva el IslandIndex de su isla aunque esté aislado en el mar). */
		bool bTouchesMainland = false;
		float CentroidX = 0.0f;
		float CentroidY = 0.0f;
		float MinHeight = 0.0f;
		float MaxHeight = 0.0f;
	};

	std::vector<FComponent> FindShallowComponents(const FGrid& Grid, float Threshold, std::vector<int32>& OutLabel)
	{
		OutLabel.assign(Grid.Heights.size(), -1);
		std::vector<int32>& Label = OutLabel;
		std::vector<FComponent> Components;
		std::vector<int32> Stack;

		for (int32 Start = 0; Start < static_cast<int32>(Grid.Heights.size()); ++Start)
		{
			if (Label[static_cast<size_t>(Start)] != -1 || Grid.Heights[static_cast<size_t>(Start)] <= Threshold)
			{
				continue;
			}
			const int32 CompId = static_cast<int32>(Components.size());
			FComponent Comp;
			double SumX = 0.0;
			double SumY = 0.0;
			Comp.MinHeight = Grid.Heights[static_cast<size_t>(Start)];
			Comp.MaxHeight = Grid.Heights[static_cast<size_t>(Start)];

			Stack.clear();
			Stack.push_back(Start);
			Label[static_cast<size_t>(Start)] = CompId;
			while (!Stack.empty())
			{
				const int32 Cell = Stack.back();
				Stack.pop_back();
				const int32 CX = Cell % Grid.Width;
				const int32 CY = Cell / Grid.Width;
				++Comp.CellCount;
				const float H = Grid.Heights[static_cast<size_t>(Cell)];
				Comp.MinHeight = H < Comp.MinHeight ? H : Comp.MinHeight;
				Comp.MaxHeight = H > Comp.MaxHeight ? H : Comp.MaxHeight;
				SumX += Grid.OriginX + CX * Grid.Spacing;
				SumY += Grid.OriginY + CY * Grid.Spacing;

				static const int32 DX[4] = {1, -1, 0, 0};
				static const int32 DY[4] = {0, 0, 1, -1};
				for (int32 D = 0; D < 4; ++D)
				{
					const int32 NX = CX + DX[D];
					const int32 NY = CY + DY[D];
					if (NX < 0 || NY < 0 || NX >= Grid.Width || NY >= Grid.Height)
					{
						continue;
					}
					const int32 NCell = Grid.IndexOf(NX, NY);
					if (Label[static_cast<size_t>(NCell)] == -1 && Grid.Heights[static_cast<size_t>(NCell)] > Threshold)
					{
						Label[static_cast<size_t>(NCell)] = CompId;
						Stack.push_back(NCell);
					}
				}
			}
			Comp.CentroidX = static_cast<float>(SumX / Comp.CellCount);
			Comp.CentroidY = static_cast<float>(SumY / Comp.CellCount);
			Components.push_back(Comp);
		}
		return Components;
	}

	void WriteGridBinary(const FGrid& Grid, const std::string& Path)
	{
		std::ofstream Out(Path, std::ios::binary);
		int32 Header[4] = {Grid.Width, Grid.Height, static_cast<int32>(Grid.OriginX), static_cast<int32>(Grid.OriginY)};
		Out.write(reinterpret_cast<const char*>(Header), sizeof(Header));
		float SpacingF = Grid.Spacing;
		Out.write(reinterpret_cast<const char*>(&SpacingF), sizeof(SpacingF));
		Out.write(reinterpret_cast<const char*>(Grid.Heights.data()), static_cast<std::streamsize>(Grid.Heights.size() * sizeof(float)));
		Out.write(reinterpret_cast<const char*>(Grid.IslandIndex.data()), static_cast<std::streamsize>(Grid.IslandIndex.size() * sizeof(int32)));
	}

	/** Varianza local (ventana Radius celdas) de la altura, promediada sobre las celdas de
	 * tierra de una isla. Si bExcludeChannels es true, no cuenta como centro de ventana
	 * ninguna celda que ya sea en sí misma un cauce (más de 0.5 m por debajo del máximo de su
	 * vecindario de 2 celdas): mide la planitud del terreno ALREDEDOR de los cauces, que es
	 * justo la queja del director ("el terreno alrededor no debería ser una llanura plana
	 * artificial"), no el escalón del propio cauce (que ya de por sí mete varianza y puede
	 * maquillar una meseta perfectamente plana). */
	float AveragePlateauVariance(const FGrid& Grid, int32 IslandIdx, float MinLandHeight, int32 Radius, bool bExcludeChannels = false)
	{
		double SumVar = 0.0;
		int64 Count = 0;
		for (int32 Y = Radius; Y < Grid.Height - Radius; ++Y)
		{
			for (int32 X = Radius; X < Grid.Width - Radius; ++X)
			{
				const int32 I = Grid.IndexOf(X, Y);
				if (Grid.IslandIndex[static_cast<size_t>(I)] != IslandIdx || Grid.Heights[static_cast<size_t>(I)] < MinLandHeight)
				{
					continue;
				}
				if (bExcludeChannels)
				{
					float NearMax = -1000.0f;
					for (int32 DY = -2; DY <= 2; ++DY)
					{
						for (int32 DX = -2; DX <= 2; ++DX)
						{
							const int32 Nx = FMath::Clamp(X + DX, 0, Grid.Width - 1);
							const int32 Ny = FMath::Clamp(Y + DY, 0, Grid.Height - 1);
							NearMax = std::max(NearMax, Grid.Heights[static_cast<size_t>(Grid.IndexOf(Nx, Ny))]);
						}
					}
					if (NearMax - Grid.Heights[static_cast<size_t>(I)] > 0.5f)
					{
						continue;
					}
				}
				double Mean = 0.0;
				int32 N = 0;
				for (int32 DY = -Radius; DY <= Radius; ++DY)
				{
					for (int32 DX = -Radius; DX <= Radius; ++DX)
					{
						Mean += Grid.Heights[static_cast<size_t>(Grid.IndexOf(X + DX, Y + DY))];
						++N;
					}
				}
				Mean /= N;
				double Var = 0.0;
				for (int32 DY = -Radius; DY <= Radius; ++DY)
				{
					for (int32 DX = -Radius; DX <= Radius; ++DX)
					{
						const double D = Grid.Heights[static_cast<size_t>(Grid.IndexOf(X + DX, Y + DY))] - Mean;
						Var += D * D;
					}
				}
				Var /= N;
				SumVar += Var;
				++Count;
			}
		}
		return Count > 0 ? static_cast<float>(SumVar / Count) : -1.0f;
	}

	/** Fracción de las celdas de tierra (no cauce) de una isla cuya altura cae a menos de
	 * BandMeters de la mediana. Cerca de 1 significa "toda la isla lee como una única meseta a
	 * una altura", que es literalmente la queja del director; un relieve real reparte las
	 * alturas y esa fracción baja aunque el borde de cada cauce meta algo de varianza local. */
	float FlatPlateauFraction(const FGrid& Grid, int32 IslandIdx, float MinLandHeight, float BandMeters)
	{
		std::vector<float> Land;
		for (int32 Cell = 0; Cell < static_cast<int32>(Grid.Heights.size()); ++Cell)
		{
			if (Grid.IslandIndex[static_cast<size_t>(Cell)] == IslandIdx && Grid.Heights[static_cast<size_t>(Cell)] >= MinLandHeight)
			{
				Land.push_back(Grid.Heights[static_cast<size_t>(Cell)]);
			}
		}
		if (Land.empty())
		{
			return -1.0f;
		}
		std::vector<float> Sorted = Land;
		std::sort(Sorted.begin(), Sorted.end());
		const float Median = Sorted[Sorted.size() / 2];
		int64 Within = 0;
		for (float H : Land)
		{
			Within += std::abs(H - Median) <= BandMeters ? 1 : 0;
		}
		return static_cast<float>(Within) / static_cast<float>(Land.size());
	}

	struct FChannelCell
	{
		float Height = 0.0f;
		/** Distancia en metros al centro de la isla (para correlar con el gradiente hacia la costa). */
		float DistanceToCenter = 0.0f;
	};

	/** Celdas de "cauce" de una isla: tierra cuya altura cae bajo el máximo de su vecindario
	 * en más de DepthMargin metros. Devuelve las alturas absolutas de esas celdas (para medir
	 * si todas comparten la misma profundidad mínima o si varían con gradiente hacia el mar). */
	std::vector<FChannelCell> ChannelFloorHeights(const FGrid& Grid, int32 IslandIdx, const FVector2D& Center, int32 Radius, float DepthMargin)
	{
		std::vector<FChannelCell> Result;
		for (int32 Y = Radius; Y < Grid.Height - Radius; ++Y)
		{
			for (int32 X = Radius; X < Grid.Width - Radius; ++X)
			{
				const int32 I = Grid.IndexOf(X, Y);
				// Solo tierra/orilla real: excluye el mar profundo que también queda "etiquetado"
				// con el IslandIndex de la isla más cercana aunque esté fuera de su plataforma.
				if (Grid.IslandIndex[static_cast<size_t>(I)] != IslandIdx || Grid.Heights[static_cast<size_t>(I)] < -2.5f)
				{
					continue;
				}
				float LocalMax = -1000.0f;
				for (int32 DY = -Radius; DY <= Radius; ++DY)
				{
					for (int32 DX = -Radius; DX <= Radius; ++DX)
					{
						LocalMax = std::max(LocalMax, Grid.Heights[static_cast<size_t>(Grid.IndexOf(X + DX, Y + DY))]);
					}
				}
				const float H = Grid.Heights[static_cast<size_t>(I)];
				if (LocalMax - H > DepthMargin)
				{
					const float Wx = Grid.OriginX + X * Grid.Spacing;
					const float Wy = Grid.OriginY + Y * Grid.Spacing;
					const float Dist = std::sqrt((Wx - Center.X) * (Wx - Center.X) + (Wy - Center.Y) * (Wy - Center.Y));
					Result.push_back({H, Dist});
				}
			}
		}
		return Result;
	}

	/** Correlación de Pearson entre la distancia al centro y la altura del cauce: negativa y
	 * apreciable significa que el cauce baja de verdad al acercarse a la costa (gradiente real
	 * hacia el mar), en vez de una profundidad plana sin relación con la posición. */
	float ChannelGradientCorrelation(const std::vector<FChannelCell>& Cells)
	{
		if (Cells.size() < 8)
		{
			return 0.0f;
		}
		double MeanD = 0.0;
		double MeanH = 0.0;
		for (const FChannelCell& C : Cells)
		{
			MeanD += C.DistanceToCenter;
			MeanH += C.Height;
		}
		MeanD /= Cells.size();
		MeanH /= Cells.size();
		double Cov = 0.0;
		double VarD = 0.0;
		double VarH = 0.0;
		for (const FChannelCell& C : Cells)
		{
			const double Dd = C.DistanceToCenter - MeanD;
			const double Dh = C.Height - MeanH;
			Cov += Dd * Dh;
			VarD += Dd * Dd;
			VarH += Dh * Dh;
		}
		const double Denom = std::sqrt(VarD * VarH);
		return Denom > 1e-6 ? static_cast<float>(Cov / Denom) : 0.0f;
	}

	void PrintStats(const char* Label, const std::vector<float>& Values)
	{
		if (Values.empty())
		{
			std::printf("%s: sin muestras\n", Label);
			return;
		}
		double Sum = 0.0;
		float Min = Values[0];
		float Max = Values[0];
		for (float V : Values)
		{
			Sum += V;
			Min = std::min(Min, V);
			Max = std::max(Max, V);
		}
		const double Mean = Sum / Values.size();
		double SqSum = 0.0;
		for (float V : Values)
		{
			SqSum += (V - Mean) * (V - Mean);
		}
		const double StdDev = std::sqrt(SqSum / Values.size());
		std::printf("%s: N=%zu min=%.3f max=%.3f media=%.3f desviacion=%.4f\n", Label, Values.size(), Min, Max, Mean, StdDev);
	}
}

int main(int Argc, char** Argv)
{
	const std::string OutPath = Argc > 1 ? Argv[1] : "terrain_dump.bin";
	const uint32 Seed = Argc > 2 ? static_cast<uint32>(std::strtoul(Argv[2], nullptr, 10)) : FArchipelagoLayout::OfficialSeed;
	const float Spacing = Argc > 3 ? static_cast<float>(std::atof(Argv[3])) : 8.0f;

	const FArchipelagoLayout Layout = FArchipelagoLayout::Generate(Seed);
	const FTerrainDensity Density(Layout);

	std::printf("=== Diagnostico de terreno (semilla %u, paso %.1f m) ===\n", Seed, Spacing);
	std::printf("Islas: %d\n", Layout.Islands.Num());
	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const FIslandDesc& Isl = Layout.Islands[I];
		const bool bRocky = Isl.Archetype == EIslandArchetype::Teeth || Isl.Archetype == EIslandArchetype::Mesa;
		std::printf("  [%d] %-10s centro=(%.0f,%.0f) radio=%.0f %s\n", I, LexToString(Isl.Archetype),
			Isl.Center.X, Isl.Center.Y, Isl.Radius, bRocky ? "(rocosa)" : "(tropical/playa)");
	}

	const FGrid Grid = SampleWorld(Density, FArchipelagoLayout::WorldHalfExtent, Spacing);
	WriteGridBinary(Grid, OutPath);
	std::printf("\nVolcado de alturas: %s (%dx%d celdas)\n", OutPath.c_str(), Grid.Width, Grid.Height);

	// --- 1) Bultos submarinos aislados -------------------------------------------------
	constexpr float ShallowThreshold = -3.0f;
	constexpr int32 MinBumpCells = 4; // ignora ruido de 1-3 celdas del muestreo discreto
	std::vector<int32> Label;
	std::vector<FComponent> Components = FindShallowComponents(Grid, ShallowThreshold, Label);

	// Semilla de tierra firme por isla: la celda más alta de las etiquetadas con su IslandIndex.
	// El centro publicado no sirve para atolón/archipiélago de islotes (Arenas Blancas y Los
	// Dientes tienen agua en el centro, ver WorldGenSpec), pero el punto más alto de la isla
	// siempre es su cresta o islote real, nunca un cayo satélite (mucho más bajo). Marca como
	// "toca tierra firme" toda componente que contenga esa celda.
	std::vector<int32> HighestCell(Layout.Islands.Num(), -1);
	std::vector<float> HighestZ(Layout.Islands.Num(), -1000.0f);
	for (int32 Cell = 0; Cell < static_cast<int32>(Grid.Heights.size()); ++Cell)
	{
		const int32 Idx = Grid.IslandIndex[static_cast<size_t>(Cell)];
		if (Idx != INDEX_NONE && Grid.Heights[static_cast<size_t>(Cell)] > HighestZ[static_cast<size_t>(Idx)])
		{
			HighestZ[static_cast<size_t>(Idx)] = Grid.Heights[static_cast<size_t>(Cell)];
			HighestCell[static_cast<size_t>(Idx)] = Cell;
		}
	}
	std::vector<bool> MainlandLabel(Components.size(), false);
	for (int32 Cell : HighestCell)
	{
		const int32 CompId = Cell >= 0 ? Label[static_cast<size_t>(Cell)] : -1;
		if (CompId >= 0)
		{
			MainlandLabel[static_cast<size_t>(CompId)] = true;
		}
	}
	for (size_t I = 0; I < Components.size(); ++I)
	{
		Components[I].bTouchesMainland = MainlandLabel[I];
	}

	std::vector<FComponent> Bumps;
	for (const FComponent& C : Components)
	{
		if (!C.bTouchesMainland && C.CellCount >= MinBumpCells)
		{
			Bumps.push_back(C);
		}
	}
	std::sort(Bumps.begin(), Bumps.end(), [](const FComponent& A, const FComponent& B) { return A.CellCount > B.CellCount; });
	std::printf("\n--- Bultos submarinos aislados (por encima de %.1f m, sin isla asociada) ---\n", ShallowThreshold);
	std::printf("Componentes someras totales: %zu | Bultos aislados: %zu\n", Components.size(), Bumps.size());
	const int32 ReportCount = std::min<int32>(20, static_cast<int32>(Bumps.size()));
	for (int32 I = 0; I < ReportCount; ++I)
	{
		const FComponent& C = Bumps[static_cast<size_t>(I)];
		std::printf("  #%2d centroide=(%.0f,%.0f) celdas=%d area=%.0fm2 altura=[%.2f,%.2f]\n",
			I, C.CentroidX, C.CentroidY, C.CellCount, C.CellCount * Spacing * Spacing, C.MinHeight, C.MaxHeight);
	}

	// --- 2) Planitud de mesetas y perfil de cauces --------------------------------------
	// Dos escalas: "fina" (textura bajo el pie, ventana de unas pocas celdas) y "paisaje"
	// (lomas y relieve a decenas de metros, que es lo que de verdad delata una meseta
	// artificial: una zona puede tener microrrelieve de ruido y aun así leerse como una mesa
	// perfectamente plana si no hay ondulación a mayor escala).
	const int32 FineRadius = 1;
	const int32 LandscapeRadius = std::max(1, static_cast<int32>(40.0f / Spacing));
	std::printf("\n--- Planitud (varianza local de altura) por isla ---\n");
	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const float FineVar = AveragePlateauVariance(Grid, I, 2.0f, FineRadius);
		const float LandscapeVar = AveragePlateauVariance(Grid, I, 2.0f, LandscapeRadius);
		const float UplandVar = AveragePlateauVariance(Grid, I, 2.0f, LandscapeRadius, /*bExcludeChannels=*/true);
		const float FlatFraction = FlatPlateauFraction(Grid, I, 2.0f, 0.4f);
		std::printf("  [%d] %-10s fina(%.0fm)=%.5f m2  paisaje(%.0fm)=%.5f m2  paisaje_sin_cauces=%.5f m2  meseta_+-0.4m=%.1f%%\n",
			I, LexToString(Layout.Islands[I].Archetype), (2 * FineRadius + 1) * Spacing, FineVar,
			(2 * LandscapeRadius + 1) * Spacing, LandscapeVar, UplandVar, FlatFraction * 100.0f);
	}

	std::printf("\n--- Perfil de cauces (celdas de tierra por debajo del maximo local en >0.5m) ---\n");
	for (int32 I = 0; I < Layout.Islands.Num(); ++I)
	{
		const std::vector<FChannelCell> Cells = ChannelFloorHeights(Grid, I, Layout.Islands[I].Center, 2, 0.5f);
		std::vector<float> Floors;
		Floors.reserve(Cells.size());
		for (const FChannelCell& C : Cells) { Floors.push_back(C.Height); }
		const std::string Label = std::string("[") + std::to_string(I) + "] " + LexToString(Layout.Islands[I].Archetype);
		PrintStats(Label.c_str(), Floors);
		std::printf("    correlacion distancia-al-centro/altura del cauce: %.3f (negativa = baja hacia la costa)\n",
			ChannelGradientCorrelation(Cells));
	}

	// --- 3) Histograma de profundidades del fondo marino --------------------------------
	std::printf("\n--- Histograma de profundidad del fondo marino (celdas con altura < 0) ---\n");
	constexpr int32 NumBins = 30;
	int64 Bins[NumBins] = {0};
	for (float H : Grid.Heights)
	{
		if (H >= 0.0f)
		{
			continue;
		}
		int32 Bin = static_cast<int32>(-H); // bins de 1m: 0..29 => 0..-29m, resto al ultimo bin
		Bin = std::min(Bin, NumBins - 1);
		++Bins[Bin];
	}
	for (int32 B = 0; B < NumBins; ++B)
	{
		if (Bins[B] == 0) { continue; }
		std::printf("  [%3d,%3d) m: %lld celdas\n", -(B + 1), -B, static_cast<long long>(Bins[B]));
	}

	return 0;
}
