// Diagnóstico del terreno del archipiélago (programa aparte, no entra en la suite de tests).
// Muestrea el campo de alturas, imprime las métricas de realismo de FTerrainSurvey y escribe
// dos imágenes PPM (relieve sombreado y pendiente) para comparar antes/después.
//
//   TerrainDiagnostics <prefijo_salida> [semilla] [paso_m] [isla_para_zoom]
//
// Sin isla: mundo entero. Con isla (índice del layout): recorte de 1,6 radios alrededor de ella.
// Las imágenes salen como <prefijo>_relieve.ppm y <prefijo>_pendiente.ppm; se pasan a PNG con
// cualquier conversor (p. ej. `magick x.ppm x.png`). No las guardes en el repositorio.
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/TerrainPlayabilitySurvey.h"
#include "WorldGen/TerrainSurvey.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
	struct FRgb
	{
		unsigned char R = 0, G = 0, B = 0;
	};

	FRgb HeightTint(float H)
	{
		auto Mix = [](FRgb A, FRgb B, float T)
		{
			T = std::fmin(std::fmax(T, 0.0f), 1.0f);
			return FRgb{static_cast<unsigned char>(A.R + (B.R - A.R) * T), static_cast<unsigned char>(A.G + (B.G - A.G) * T),
				static_cast<unsigned char>(A.B + (B.B - A.B) * T)};
		};
		if (H < 0.0f)
		{
			return Mix(FRgb{90, 190, 200}, FRgb{10, 30, 80}, -H / 80.0f);
		}
		if (H < 2.0f)
		{
			return FRgb{230, 215, 170};
		}
		return Mix(FRgb{80, 140, 60}, FRgb{235, 235, 225}, (H - 2.0f) / 250.0f);
	}

	/**
	 * Zonas construibles: tierra con pendiente < 15° en verde claro, ladera en ocre, pared de
	 * más de 60° en rojo; el mar, azul según la profundidad. Sombreado leve para leer el relieve.
	 */
	FRgb BuildableTint(float H, float SlopeDeg, float Shade)
	{
		FRgb C;
		if (H <= 0.0f)
		{
			const float T = std::fmin(-H / 60.0f, 1.0f);
			C = FRgb{static_cast<unsigned char>(40 - 30 * T), static_cast<unsigned char>(90 - 60 * T), static_cast<unsigned char>(160 - 60 * T)};
		}
		else if (SlopeDeg < 15.0f && H > 0.5f)
		{
			C = FRgb{120, 220, 90};
		}
		else if (SlopeDeg > 60.0f)
		{
			C = FRgb{220, 40, 40};
		}
		else
		{
			const float T = std::fmin((SlopeDeg - 15.0f) / 45.0f, 1.0f);
			C = FRgb{static_cast<unsigned char>(200 - 60 * T), static_cast<unsigned char>(170 - 80 * T), static_cast<unsigned char>(110 - 50 * T)};
		}
		const float K = 0.75f + 0.25f * std::fmin(Shade, 1.2f);
		return FRgb{static_cast<unsigned char>(std::fmin(255.0f, C.R * K)), static_cast<unsigned char>(std::fmin(255.0f, C.G * K)),
			static_cast<unsigned char>(std::fmin(255.0f, C.B * K))};
	}

	void WritePpm(const std::string& Path, int32 W, int32 H, const std::vector<FRgb>& Pixels)
	{
		FILE* File = std::fopen(Path.c_str(), "wb");
		if (!File)
		{
			std::fprintf(stderr, "No se puede escribir %s\n", Path.c_str());
			return;
		}
		std::fprintf(File, "P6\n%d %d\n255\n", W, H);
		std::fwrite(Pixels.data(), sizeof(FRgb), Pixels.size(), File);
		std::fclose(File);
	}

	/** Relieve sombreado (luz del noroeste) teñido por altura, y pendiente en grados (0-60°). */
	void WriteImages(const FTerrainSampleGrid& Grid, const std::string& Prefix)
	{
		std::vector<FRgb> Relief(static_cast<size_t>(Grid.Width) * Grid.Height);
		std::vector<FRgb> Slope(Relief.size());
		std::vector<FRgb> Buildable(Relief.size());
		for (int32 Y = 0; Y < Grid.Height; ++Y)
		{
			for (int32 X = 0; X < Grid.Width; ++X)
			{
				auto At = [&](int32 Ax, int32 Ay) { return Grid.Heights[Grid.Index(FMath::Clamp(Ax, 0, Grid.Width - 1), FMath::Clamp(Ay, 0, Grid.Height - 1))]; };
				const float Dx = (At(X + 1, Y) - At(X - 1, Y)) / (2.0f * Grid.Spacing);
				const float Dy = (At(X, Y + 1) - At(X, Y - 1)) / (2.0f * Grid.Spacing);
				const float Nz = 1.0f / std::sqrt(1.0f + Dx * Dx + Dy * Dy);
				const float Shade = std::fmax(0.25f, (-Dx * -0.6f + -Dy * -0.6f + 0.53f) * Nz * 1.2f);
				const FRgb Tint = HeightTint(At(X, Y));
				// Norte arriba: la fila de la imagen crece hacia el sur.
				const size_t Pixel = static_cast<size_t>(Grid.Height - 1 - Y) * Grid.Width + X;
				Relief[Pixel] = FRgb{static_cast<unsigned char>(std::fmin(255.0f, Tint.R * Shade)),
					static_cast<unsigned char>(std::fmin(255.0f, Tint.G * Shade)), static_cast<unsigned char>(std::fmin(255.0f, Tint.B * Shade))};
				const float Deg = std::atan(std::sqrt(Dx * Dx + Dy * Dy)) * 57.2958f;
				const unsigned char V = static_cast<unsigned char>(std::fmin(255.0f, Deg / 60.0f * 255.0f));
				Slope[Pixel] = FRgb{V, static_cast<unsigned char>(V / 2), static_cast<unsigned char>(255 - V)};
				Buildable[Pixel] = BuildableTint(At(X, Y), Deg, Shade);
			}
		}
		WritePpm(Prefix + "_relieve.ppm", Grid.Width, Grid.Height, Relief);
		WritePpm(Prefix + "_pendiente.ppm", Grid.Width, Grid.Height, Slope);
		WritePpm(Prefix + "_construible.ppm", Grid.Width, Grid.Height, Buildable);
	}

	FTerrainSampleGrid SampleAround(const FTerrainDensity& Density, const FIslandDesc& Island, float Spacing)
	{
		const float Half = Island.Radius * 1.6f;
		const int32 Cells = static_cast<int32>(2.0f * Half / Spacing) + 1;
		FTerrainSampleGrid Grid;
		Grid.Init(Cells, Cells, Island.Center - FVector2D(Half), Spacing);
		for (int32 Y = 0; Y < Grid.Height; ++Y)
		{
			for (int32 X = 0; X < Grid.Width; ++X)
			{
				const FVector2D P = Grid.WorldPosition(X, Y);
				const FTerrainColumn Column = Density.SampleColumn(static_cast<float>(P.X), static_cast<float>(P.Y));
				Grid.Heights[Grid.Index(X, Y)] = Column.Height;
				Grid.IslandIndex[Grid.Index(X, Y)] = Column.IslandIndex;
			}
		}
		return Grid;
	}

	void PrintReport(const FTerrainDensity& Density, const FTerrainRealismReport& R)
	{
		for (const FSeamountDesc& M : Density.GetSeafloor().GetSeamounts())
		{
			std::printf("  %s en (%.0f, %.0f): radio %.0f m, cima %.1f m\n", M.IsIslet() ? "islote" : "monticulo", M.Center.X, M.Center.Y,
				M.Radius, M.Peak);
		}
		std::printf("bultos_sin_explicar=%d  bulto_mas_alto=%.2f m\n", R.UnexplainedBumps, R.HighestUnexplainedBump);
		for (const FTerrainBump& Bump : R.LargestBumps)
		{
			std::printf("  bulto en (%.0f, %.0f): %d celdas, cima %.2f m\n", Bump.Centroid.X, Bump.Centroid.Y, Bump.CellCount, Bump.MaxHeight);
		}
		std::printf("fondo: muestras=%d  cota_dominante=%.2f m (%.2f %%)  pico_histograma=%.2f en %.1f m  salto_max=%.2f m en (%.0f, %.0f)\n",
			R.Seafloor.SampleCount, R.Seafloor.ModeHeight, R.Seafloor.ModeFraction * 100.0f, R.Seafloor.MaxSpike,
			R.Seafloor.SpikeHeight, R.MaxSeafloorStep, R.MaxSeafloorStepAt.X, R.MaxSeafloorStepAt.Y);
		for (int32 I = 0; I < R.Islands.Num(); ++I)
		{
			const FIslandRealism& Isl = R.Islands[I];
			std::printf("  [%d] %-10s var_fina=%7.3f m2  lisa=%4.1f%%  cauces=%5d ejes=%.2f diag=%.2f  pozos/km2=%.1f  pozos_erosion=%d->%d\n", I,
				LexToString(Density.GetLayout().Islands[I].Archetype), Isl.FineVariance, Isl.SmoothFraction * 100.0f,
				Isl.Channels.SampleCount, Isl.Channels.AxisExcess, Isl.Channels.DiagonalExcess, Isl.PitsPerKm2,
				Isl.ErosionPitsBefore, Isl.ErosionPitsAfter);
		}
	}

	void PrintPlayability(const FTerrainDensity& Density, const FPlayabilityReport& P)
	{
		std::printf("motas_mar=%d  clark_evans=%.2f\n", P.SeaMotes.Num(), P.SeaMoteClarkEvans);
		for (const FVector2D& M : P.SeaMotes)
		{
			std::printf("  mota en (%.0f, %.0f)\n", M.X, M.Y);
		}
		for (int32 I = 0; I < P.Islands.Num(); ++I)
		{
			const FIslandPlayability& Isl = P.Islands[I];
			const FCoastStats& C = Isl.Coast;
			std::printf("  [%d] %-10s llano=%5.1f%% parches>=400=%3d >=2000=%3d mayor=%7.0f m2 | acantilado=%4.1f%% (alt. med %.0f max %.0f m, %d/%d rayos)"
				" | plataforma cv=%.2f talud cv=%.2f dentado=%.3f (n=%d) | rios=%d desemb=%d resultante=%.2f radial=%.2f sinuos=%.2f\n",
				I, LexToString(Density.GetLayout().Islands[I].Archetype), Isl.Flat.FlatFraction * 100.0f, Isl.Flat.CountAtLeast(400.0f),
				Isl.Flat.CountAtLeast(2000.0f), Isl.Flat.PatchAreas.IsEmpty() ? 0.0f : Isl.Flat.PatchAreas[0], C.CliffFraction * 100.0f,
				C.CliffMedianHeight, C.CliffMaxHeight, C.CliffRays, C.Rays, C.ShelfWidthCV, C.SlopeWidthCV, C.ShelfJaggedness,
				C.ShelfWidths.Num(), Isl.Drainage.RiverCells, Isl.Drainage.Mouths, Isl.Drainage.MouthResultant, Isl.Drainage.Radiality,
				Isl.Drainage.Sinuosity);
		}
	}
}

/** `TerrainDiagnostics probe X Y`: alturas en una cruz de ±12 m alrededor de un punto. */
int Probe(float X, float Y)
{
	const FTerrainDensity Density(FArchipelagoLayout::Generate(FArchipelagoLayout::OfficialSeed));
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		std::printf("%s:", Axis == 0 ? "X" : "Y");
		for (int32 D = -12; D <= 12; D += 2)
		{
			const FTerrainColumn C = Density.SampleColumn(X + (Axis == 0 ? D : 0), Y + (Axis == 1 ? D : 0));
			std::printf(" %.1f[%d,%.2f]", C.Height, C.IslandIndex, C.NormalizedDistance);
		}
		std::printf("\n");
	}
	return 0;
}

int main(int Argc, char** Argv)
{
	if (Argc > 3 && std::string(Argv[1]) == "probe")
	{
		return Probe(static_cast<float>(std::atof(Argv[2])), static_cast<float>(std::atof(Argv[3])));
	}
	const std::string Prefix = Argc > 1 ? Argv[1] : "terreno";
	const uint32 Seed = Argc > 2 ? static_cast<uint32>(std::strtoul(Argv[2], nullptr, 10)) : FArchipelagoLayout::OfficialSeed;
	const float Spacing = Argc > 3 ? static_cast<float>(std::atof(Argv[3])) : 6.0f;
	const int32 Zoom = Argc > 4 ? std::atoi(Argv[4]) : INDEX_NONE;

	const auto Start = std::chrono::steady_clock::now();
	const FTerrainDensity Density(FArchipelagoLayout::Generate(Seed));
	const auto Built = std::chrono::steady_clock::now();
	const bool bZoom = Density.GetLayout().Islands.IsValidIndex(Zoom);
	const FTerrainSampleGrid Grid = bZoom ? SampleAround(Density, Density.GetLayout().Islands[Zoom], Spacing)
		: FTerrainSurvey::SampleWorld(Density, FArchipelagoLayout::WorldHalfExtent, Spacing);
	const auto Sampled = std::chrono::steady_clock::now();

	std::printf("=== Terreno: semilla %u, paso %.1f m, %dx%d celdas%s ===\n", Seed, Spacing, Grid.Width, Grid.Height,
		bZoom ? " (zoom)" : "");
	std::printf("construccion=%.2f s  muestreo=%.2f s\n", std::chrono::duration<double>(Built - Start).count(),
		std::chrono::duration<double>(Sampled - Built).count());
	if (!bZoom)
	{
		PrintReport(Density, FTerrainSurvey::Measure(Density, Grid));
		const auto PlayStart = std::chrono::steady_clock::now();
		const FPlayabilityReport Play = FTerrainPlayabilitySurvey::Measure(Density, 4.0f);
		std::printf("jugabilidad=%.2f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - PlayStart).count());
		PrintPlayability(Density, Play);
	}
	WriteImages(Grid, Prefix);
	return 0;
}
