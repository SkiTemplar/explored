#pragma once

#include "CoreMinimal.h"

/** Cayo visto desde su isla: rumbo en Q (radianes) y distancia normalizada a la costa (T). */
struct EXPLORED_API FShelfCay
{
	float Angle = 0.0f;
	float T = 1.5f;
	/** Semiancho angular del espolón que lo une a la plataforma (radianes). */
	float HalfWidth = 0.2f;
};

/**
 * Plataforma submarina de una isla tabulada a lo largo del perímetro: anchura (en T), cota del
 * borde y anchura del talud cambian con el rumbo, y se ensancha en espolones hasta los cayos.
 * Inmutable una vez construida.
 */
class EXPLORED_API FIslandShelf
{
public:
	/** Anchura de la plataforma en T (1 = radio nominal) desde la costa hasta el borde. */
	float Width(float Angle) const { return Sample(Widths, Angle); }
	/** Cota (m, negativa) del borde de la plataforma. */
	float EdgeDepth(float Angle) const { return Sample(Edges, Angle); }
	/** Anchura del talud en T, del borde de la plataforma al fondo. */
	float SlopeWidth(float Angle) const { return Sample(Slopes, Angle); }

	/** Rumbos tabulados alrededor de la isla. */
	static constexpr int32 Bins = 512;

private:
	friend class FIslandShelfModel;
	float Sample(const TArray<float>& Table, float Angle) const;

	TArray<float> Widths;
	TArray<float> Edges;
	TArray<float> Slopes;
};

/**
 * Perfil submarino de las islas (modelo puro, determinista por semilla). Sustituye la
 * plataforma de antes, plana y de anchura casi constante con un anillo de cresta y un talud
 * en dientes de sierra: ahora la plataforma baja en rampa, su anchura y su talud varían a lo
 * largo del perímetro (costas de plataforma ancha y costas que caen enseguida al fondo) y los
 * cayos se asientan sobre espolones de la plataforma en vez de salir sueltos del talud.
 */
class EXPLORED_API FIslandShelfModel
{
public:
	/** Tablas de la plataforma para una isla (semilla y cayos en coordenadas de la isla). */
	static FIslandShelf Build(uint32 Seed, const TArray<FShelfCay>& Cays);

	/**
	 * Altura (m) del fondo a la distancia normalizada T >= 1 en el rumbo Angle: rampa desde
	 * FootDepth en la costa hasta el borde, talud y fundido con Floor (el fondo real). Detail
	 * en [-1, 1] riza la plataforma en proporción a su profundidad (nunca asoma un bajío).
	 */
	static float Profile(const FIslandShelf& Shelf, float Angle, float T, float Floor, float FootDepth, float Detail);

	/** La plataforma y el talud acaban antes de esta T (el alcance de la isla es 1,9). */
	static constexpr float MaxReach = 1.86f;
};
