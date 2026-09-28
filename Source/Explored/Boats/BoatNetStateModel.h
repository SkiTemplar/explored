#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatTypes.h"

/**
 * Estado replicado de un barco (`FExploredBoatNetState`, biblia 08 §2.5): simula el
 * servidor con `FBoatModel` y se replica el estado, no la transformación, en **19 bytes**
 * a 20 Hz (3,0 kbps por barco ocupado):
 *
 * | Campo | Tipo | Bytes |
 * |---|---|---|
 * | Posición | `ExploredNet::WritePosition7` | 7 |
 * | Velocidad horizontal | 2 × `int16` a 10 cm/s | 4 |
 * | Rumbo | `uint16` (0,0055°) | 2 |
 * | Escora | `int8` (grados) | 1 |
 * | Vela izada (bit 7) + trimado (bits 0–6) | `uint8` | 1 |
 * | Agua embarcada | `uint16` × 0,1 kg | 2 |
 * | Integridad de casco | `uint8` × 1/255 | 1 |
 * | Reservado (piezas dañadas, H3) | `uint8`, a cero | 1 |
 *
 * Y los mandos del timonel, `Server_SetBoatControls`, en **4 bytes** a 20 Hz sin
 * fiabilidad: escota `uint8`, timón `int8`, banderas `uint8`, palada `uint8`.
 * Olas y corrientes no viajan: son funciones puras del tiempo que ya lleva el paquete de
 * reloj (FWorldClockNetModel).
 */
struct EXPLORED_API FBoatNetState
{
	FVector PositionCm = FVector::ZeroVector;
	FVector2D VelocityCmS = FVector2D::ZeroVector;
	float YawDeg = 0.0f;
	/** Escora (RollDeg de FBoatState), en grados. */
	float HeelDeg = 0.0f;
	bool bSailRaised = false;
	float SailTrim01 = 0.5f;
	float SwampWaterKg = 0.0f;
	float HullIntegrity01 = 1.0f;
};

/** Palada pedida en este envío de mandos. */
enum class EBoatNetStroke : uint8
{
	None,
	Port,
	Starboard,
	Count
};

struct EXPLORED_API FBoatNetControls
{
	FBoatControls Controls;
	EBoatNetStroke Stroke = EBoatNetStroke::None;
};

class EXPLORED_API FBoatNetStateModel
{
public:
	static constexpr int32 StateBytes = 19;
	static constexpr int32 ControlsBytes = 4;
	static constexpr double SendHz = 20.0;
	/** Un barco más lejos de 250 m no cuesta nada. */
	static constexpr double NetCullDistanceCm = 25000.0;
	/** El cliente corrige hacia el estado recibido en 200 ms. */
	static constexpr float CorrectionSeconds = 0.2f;
	static constexpr double VelocityStepCmS = 10.0;
	static constexpr double SwampStepKg = 0.1;

	/** Lo que se manda de un barco: estado del modelo + trimado de la escota del timonel. */
	static FBoatNetState FromBoatState(const FBoatState& State, float SailTrim01);

	static void EncodeState(const FBoatNetState& State, TArray<uint8>& Out);
	/** False si no mide 19 bytes o el byte reservado no va a cero; OutState no cambia. */
	static bool DecodeState(const TArray<uint8>& In, FBoatNetState& OutState);
	static FBoatNetState Quantize(const FBoatNetState& State);

	static void EncodeControls(const FBoatNetControls& Controls, TArray<uint8>& Out);
	/**
	 * False si no mide 4 bytes, el timón vale −128 (fuera de −127..127), hay banderas
	 * desconocidas o la palada no existe. El servidor descarta el envío entero.
	 */
	static bool DecodeControls(const TArray<uint8>& In, FBoatNetControls& OutControls);

	/** Fracción del error que el cliente cierra en este paso para llegar en 200 ms (ver FFaunaAnchorNetModel::PullAlpha). */
	static float CorrectionAlpha(float SecondsSinceState, float DeltaSeconds);

	/** kbps de un barco ocupado (19 B × 20 Hz). */
	static double KbpsPerBoat() { return StateBytes * SendHz * 8.0 / 1000.0; }
};
