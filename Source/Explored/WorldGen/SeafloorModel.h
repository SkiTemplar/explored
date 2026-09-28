#pragma once

#include "CoreMinimal.h"
#include "Core/ExploredNoise.h"
#include "WorldGen/ArchipelagoLayout.h"

/**
 * Montículo del fondo marino. Los sumergidos se quedan bajo la cota de los bultos (nunca
 * rozan la superficie); los islotes asoman como farallones sueltos, con su pedestal.
 * Metros, coordenadas del mundo.
 */
struct EXPLORED_API FSeamountDesc
{
	FVector2D Center = FVector2D::ZeroVector;
	/** Radio del pedestal: a esta distancia el montículo ya es fondo. */
	float Radius = 100.0f;
	/** Altura de la cima (m): negativa en los sumergidos, positiva en los islotes. */
	float Peak = -8.0f;
	float Aspect = 1.0f;
	float Angle = 0.0f;
	uint32 Seed = 0;

	bool IsIslet() const { return Peak > 0.0f; }
};

/**
 * Fondo marino continuo del archipiélago: llanura abisal que sigue bajando lejos de la
 * cadena, dorsal submarina con la cresta irregular (sin meseta), ondulaciones de arena y
 * montículos e islotes sueltos. También funde con el fondo real los perfiles submarinos de
 * las islas y de los cayos, que antes terminaban en una cota fija (fondo recortado a
 * OceanFloor) o en un corte (escalón al final del cayo). Modelo puro, determinista por
 * semilla.
 */
class EXPLORED_API FSeafloorModel
{
public:
	explicit FSeafloorModel(const FArchipelagoLayout& Layout);

	/** Altura del fondo (m, negativa) con dorsal, relieve y montículos. */
	float FloorHeight(float X, float Y) const;
	/** Igual pero sin montículos: la base sobre la que se apoyan. */
	float BaseFloorHeight(float X, float Y) const;

	const TArray<FSeamountDesc>& GetSeamounts() const { return Seamounts; }

	/**
	 * Funde la altura de una isla con el fondo al acercarse a su alcance (ReachFraction =
	 * distancia al centro / alcance → 1), donde deja de evaluarse: antes el talud se cortaba
	 * allí aunque aún no hubiera llegado al fondo y dejaba un escalón en arco.
	 */
	static float BlendIslandToFloor(float IslandHeight, float Floor, float ReachFraction);

	/**
	 * Altura de un cayo satélite cuya falda baja hasta Floor (la altura que ya hay debajo) y
	 * se funde con ella en su borde, sin el corte a -22 m de antes. Floor fuera de su alcance.
	 */
	static float CayHeight(const FCayDesc& Cay, float X, float Y, float Wobble, float Floor);

	/** Montículos e islotes deterministas, lejos de islas, cayos y entre sí. */
	static TArray<FSeamountDesc> GenerateSeamounts(const FArchipelagoLayout& Layout);

	/** Cima máxima de un montículo sumergido: por debajo de la cota de bulto de las métricas. */
	static constexpr float MaxSubmergedPeak = -5.0f;
	/** Distancia libre mínima, en radios de isla, entre una isla y el borde de un montículo. */
	static constexpr float IslandClearance = 2.35f;

private:
	float SeamountHeight(const FSeamountDesc& Mount, float X, float Y, float Base) const;

	TArray<FVector2D> Spine;
	TArray<FSeamountDesc> Seamounts;
	FExploredNoise FloorNoise;
	FExploredNoise ReliefNoise;
};
