#pragma once

#include "CoreMinimal.h"

#include "WorldGen/FellingModel.h"

/**
 * Lo que suelta un árbol talado que cae al agua (biblia 02 §1.5, GDD v2 §3.12).
 * FFellingModel coloca cada unidad en el plano; este modelo decide qué pasa
 * con las que caen en el agua:
 *
 * - Flota lo que es menos denso que el agua de mar y hay bastante agua bajo
 *   ello (más que su calado). Lo que se hunde (madera dura, resina) se queda en
 *   el fondo, donde se recoge buceando.
 * - Lo que flota deriva con la corriente local (FOceanCurrents::CurrentAt), la
 *   misma que arrastra un barco sin amarrar (biblia 02 §8.4).
 * - Al llegar a menos agua que su calado, vara en el bajío, en el punto de
 *   contacto (el primero con menos agua que su calado, a ~1 mm). Solo vuelve a
 *   flotar si sube el agua: un tronco caído en la franja intermareal con la
 *   bajamar se va con la pleamar siguiente si nadie lo recoge.
 * - Lo que lleva flotando HandoverFloatingS sin que nadie lo recoja sale del
 *   modelo: los troncos pasan a ser `madera_flotante` normal (la que se pesca y
 *   llega a las playas), los cocos siguen siendo cocos y lo demás se deshace.
 *   Quien llama también puede entregarlo antes (sale del radio activo).
 *
 * Cada pieza está siempre en un solo estado y ninguna desaparece sin pasar por
 * Collect, HandOver o el deshecho: el número de piezas no cambia nunca.
 *
 * Paso fijo de FixedStepS con acumulador: el resultado no depende del ritmo de
 * fotogramas. Ningún tramo de un paso avanza más de MaxSegmentCm sin mirar la
 * profundidad, así que una pieza no salta por encima de una barra de arena de
 * una celda. Sin aleatoriedad.
 */

/** Cómo se comporta en el agua un objeto que suelta la tala. */
struct EXPLORED_API FDriftFloatSpec
{
	FName ItemId;
	/** Menos denso que el agua de mar (propiedad `Flota` del material). */
	bool bFloats = false;
	/** Lo que se hunde bajo la superficie flotando (cm): con menos agua que esto, vara. */
	float DraftCm = 0.0f;
	/** En qué se convierte al entregarlo tras flotar sin que nadie lo recoja; NAME_None = se deshace. */
	FName HandoverItemId;
};

enum class EFelledPieceState : uint8
{
	/** Quieta en tierra, en el bajío o en el fondo. Se refloata si sube el agua (y flota). */
	Resting,
	/** Flotando: deriva con la corriente. */
	Floating,
	/** La ha recogido alguien. */
	Collected,
	/** Entregada al sistema de madera flotante como HandoverItemId. */
	HandedOver,
	/** Se ha deshecho en el mar (hojas, fibra, cáscaras, ramas finas). */
	Decayed
};

EXPLORED_API const TCHAR* LexToString(EFelledPieceState State);

struct EXPLORED_API FFelledPiece
{
	FFellingDrop Drop;
	/** Ficha de flotación de Drop.ItemId, copiada al añadirla. */
	FDriftFloatSpec Spec;
	EFelledPieceState State = EFelledPieceState::Resting;
	/** Pasos fijos que lleva flotando en total (entero: sin deriva). */
	int32 FloatingSteps = 0;

	bool IsActive() const { return State == EFelledPieceState::Resting || State == EFelledPieceState::Floating; }
};

/** Lo que ha cambiado en un avance, en orden. Índices en GetPieces(); una pieza puede varar y volver a flotar en el mismo avance. */
struct EXPLORED_API FFelledDriftReport
{
	int32 StepsRun = 0;
	/** Pasos acumulados que se descartaron por pasar de MaxStepsPerAdvance. */
	int32 StepsDropped = 0;
	TArray<int32> Refloated;
	TArray<int32> Beached;
	TArray<int32> HandedOver;
	TArray<int32> Decayed;
};

class EXPLORED_API FFelledDriftModel
{
public:
	/** Profundidad del agua sobre el suelo en un punto (cm; ≤ 0 en seco), con la marea del momento. */
	using FWaterDepth = TFunctionRef<float(const FVector2D&)>;
	/** Velocidad del agua en un punto (cm/s, plano). */
	using FWaterCurrent = TFunctionRef<FVector2D(const FVector2D&)>;

	/** Paso fijo de la deriva (s). La deriva es lenta: medio segundo basta. */
	static constexpr float FixedStepS = 0.5f;
	/** Tramo máximo sin mirar la profundidad (cm): una columna de la arena viva (§3.13). */
	static constexpr double MaxSegmentCm = 25.0;
	/** Bisecciones para hallar el punto de contacto al varar: 25 cm / 2⁸ ≈ 1 mm. */
	static constexpr int32 ContactBisections = 8;
	/** Tope de pasos por avance (2 min): lo que pase de ahí se descarta, como la arena viva al acercarse. */
	static constexpr int32 MaxStepsPerAdvance = 240;
	/** Tiempo flotando tras el que se entrega a la madera flotante (s): a 1 m/s ya está a 600 m, fuera del radio activo. */
	static constexpr float HandoverFloatingS = 600.0f;
	/** Tope de velocidad de corriente que se acepta (cm/s): un dato disparatado no teletransporta nada. */
	static constexpr double MaxCurrentCmS = 500.0;
	/** Tope de piezas: un guardado manipulado no puede dejar Advance calculando minutos. */
	static constexpr int32 MaxPieces = 1024;

	static int32 HandoverSteps() { return FMath::CeilToInt(HandoverFloatingS / FixedStepS); }

	/** Tabla por defecto: los objetos que suelta FFellingModel::DefaultProfiles. */
	static TArray<FDriftFloatSpec> DefaultFloatSpecs();
	/** Ficha del objeto; un objeto que no está en la tabla se hunde (no se lo lleva el mar). */
	static FDriftFloatSpec FindFloatSpec(const TArray<FDriftFloatSpec>& Specs, FName ItemId);

	/**
	 * Hace falta seguir esta pieza: flota y, con la pleamar del día, tendría
	 * bastante agua para flotar. El resto de lo que suelta la tala es un objeto
	 * normal en el suelo y no entra en el modelo.
	 */
	static bool NeedsTracking(const FDriftFloatSpec& Spec, float DepthAtHighTideCm);

	explicit FFelledDriftModel(TArray<FDriftFloatSpec> InSpecs = DefaultFloatSpecs());

	/**
	 * Añade una unidad suelta por la tala. Flota ya si hay agua bastante bajo
	 * ella. INDEX_NONE si hay MaxPieces o la posición no es finita.
	 */
	int32 AddPiece(const FFellingDrop& Drop, FWaterDepth WaterDepth);

	/** Avanza DeltaSeconds (no finito o ≤ 0 no hace nada; el resto queda acumulado). */
	FFelledDriftReport Advance(float DeltaSeconds, FWaterDepth WaterDepth, FWaterCurrent Current);

	/** Alguien la recoge. False si ya no está activa. */
	bool Collect(int32 Index);
	/** La entrega ya (sale del radio activo). Solo lo que está flotando; lo que se deshace queda Decayed. */
	bool HandOver(int32 Index);

	const TArray<FFelledPiece>& GetPieces() const { return Pieces; }
	int32 NumActive() const;
	int32 NumFloating() const;
	/** Celda de la arena viva / chunk a la que pertenece una pieza (suelo, bien en negativo). */
	static FIntPoint CellOf(const FFelledPiece& Piece, double CellSizeCm) { return FFellingModel::CellOf(Piece.Drop.Position, CellSizeCm); }

private:
	void Step(int32 Index, FWaterDepth WaterDepth, FWaterCurrent Current, FFelledDriftReport& Report);
	void Finish(int32 Index, FFelledDriftReport& Report);
	static float SafeDepth(FWaterDepth WaterDepth, const FVector2D& At);

	TArray<FDriftFloatSpec> Specs;
	TArray<FFelledPiece> Pieces;
	/** Tiempo acumulado sin gastar (s). En double: 120 fotogramas de 1/60 s dan exactamente 4 pasos. */
	double PendingS = 0.0;
};
