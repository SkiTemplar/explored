#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/DrainageModel.h"
#include "WorldGen/TerrainErosion.h"
#include "WorldGen/TerrainMetricsModel.h"

/** Cómo se erosiona el relieve de una isla. */
struct EXPLORED_API FIslandReliefSettings
{
	/** Celdas por lado de la rejilla; cubre Q en [-QExtent, QExtent]². */
	int32 Resolution = 400;
	float QExtent = 1.3f;
	FErosionParams Erosion;
	bool bRivers = false;
	FDrainageParams Drainage;
};

/**
 * Lo que la erosión y los ríos cambian en el relieve de una isla, guardado como diferencia
 * (erosionado - base) en una rejilla en coordenadas Q locales. La altura final es la base
 * analítica + esta diferencia, que se apaga hacia el borde de la rejilla: fuera de ella el
 * terreno es exactamente la base, sin costura. Inmutable una vez construida.
 */
class EXPLORED_API FIslandReliefGrid
{
public:
	/**
	 * Muestrea BaseHeight(Qx, Qy) en metros, lo erosiona (con la erosionabilidad de
	 * Erodibility(Qx, Qy) en [0, 1]), talla los ríos si toca y guarda la diferencia.
	 */
	static TSharedPtr<const FIslandReliefGrid> Build(const FIslandReliefSettings& Settings,
		TFunctionRef<float(float, float)> BaseHeight, TFunctionRef<float(float, float)> Erodibility);

	/** Diferencia (m) en Q, bilineal y apagada hacia el borde; 0 fuera de la rejilla. */
	float SampleDelta(float Qx, float Qy) const;
	/** Profundidad tallada por los ríos (m) en Q; 0 donde no hay cauce. */
	float SampleRiver(float Qx, float Qy) const;

	float GetQExtent() const { return QExtent; }
	int32 GetResolution() const { return Delta.Width; }

	/**
	 * Pozos (mínimos locales de al menos 0,3 m, en tierra) antes y después de erosionar: los
	 * que añade la erosión son hoyos de gota, un artefacto; los de antes son del relieve base.
	 */
	int32 GetPitsBefore() const { return PitsBefore; }
	int32 GetPitsAfter() const { return PitsAfter; }
	/** Orientación de la pendiente en las celdas de tierra que la erosión y los ríos han rebajado. */
	const FOrientationStats& GetChannelOrientation() const { return ChannelOrientation; }

private:
	bool ToCell(float Qx, float Qy, float& OutX, float& OutY) const;

	FErosionHeightGrid Delta;
	FErosionHeightGrid River;
	float QExtent = 1.3f;
	int32 PitsBefore = 0;
	int32 PitsAfter = 0;
	FOrientationStats ChannelOrientation;
};

/**
 * Parámetros de erosión por arquetipo y caché compartida de rejillas (la erosión no es
 * gratis: se calcula una vez por isla y proceso, y la comparten todas las FTerrainDensity).
 */
class EXPLORED_API FIslandReliefModel
{
public:
	/**
	 * Ajustes para una isla de este arquetipo y radio (m); false si el arquetipo no se
	 * erosiona. Rejilla de unos 4 m por celda y una gota por cada tres celdas como mínimo:
	 * con menos gotas y un pincel pequeño quedan surcos sueltos ("gusanos") y pozos.
	 */
	static bool SettingsFor(EIslandArchetype Archetype, float Radius, uint32 Seed, FIslandReliefSettings& OutSettings);

	/**
	 * Rejilla de la caché para esta isla, construyéndola con Build si aún no existe. La clave
	 * es todo lo que cambia la rejilla (semilla, arquetipo, altura máxima y radio): dos
	 * layouts con la misma semilla y otra altura no deben compartirla. Se construye fuera del
	 * cerrojo, así dos islas distintas pueden erosionarse a la vez en hilos distintos.
	 */
	static TSharedPtr<const FIslandReliefGrid> GetOrBuild(const FIslandDesc& Island, const FIslandReliefSettings& Settings,
		TFunctionRef<float(float, float)> BaseHeight, TFunctionRef<float(float, float)> Erodibility);

	/** Ajustes propios de cada arquetipo sobre los comunes (talud, cuencas, canales de marea...). */
	static void TuneForArchetype(EIslandArchetype Archetype, FIslandReliefSettings& Settings);

	/** Tamaño de celda buscado, m. */
	static constexpr float TargetCellMeters = 4.0f;
	static constexpr int32 MaxResolution = 512;
};
