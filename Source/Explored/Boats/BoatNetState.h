#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatTypes.h"

/** Lo que el cliente recupera de un FExploredBoatNetState (valores ya decuantizados). */
struct EXPLORED_API FBoatNetSnapshot
{
	FVector LocationCm = FVector::ZeroVector;
	FVector2D VelocityCmS = FVector2D::ZeroVector;
	float YawDeg = 0.0f;
	float RollDeg = 0.0f;
	bool bSailRaised = false;
	float SailTrim01 = 0.0f;
	float WaterInHullKg = 0.0f;
	float HullDamage01 = 0.0f;
	/** Uniones cuaderna–tablón rotas (FBoatPiecesModel::BreachCount), hasta MaxBreaches. */
	int32 Breaches = 0;
	EBoatCondition Condition = EBoatCondition::Afloat;
};

/**
 * Estado replicado de un barco (biblia 08 §2.5): 19 B a 20 Hz, sin fiabilidad,
 * el último gana. Se replica el estado, no la transformación; el cliente
 * extrapola con el mismo FBoatModel::Step. Modelo puro: la capa de red solo
 * copia los 19 bytes de ToBytes/FromBytes.
 *
 * | Campo | Cuantización | Bytes |
 * |---|---|---|
 * | Posición | X e Y 21 bits con signo a 1 cm (±10,48 km), Z 14 bits a 1 cm (±81,9 m) | 7 |
 * | Velocidad horizontal | 2 × int16 a 10 cm/s | 4 |
 * | Rumbo | uint16, 360/65 536 ≈ 0,0055° | 2 |
 * | Escora | int8 en grados (±127) | 1 |
 * | Vela izada + trimado | bit 7 izada, 7 bits de escota (0–127) | 1 |
 * | Agua embarcada | uint16 a 0,1 kg (hasta 6553,5 kg) | 2 |
 * | Integridad de casco | uint8, 255 = intacto | 1 |
 * | Piezas dañadas y estado | 6 bits de brechas (0–63), 2 bits de EBoatCondition | 1 |
 *
 * Fuera de rango se recorta al extremo; NaN e infinitos cuentan como 0 (el
 * rumbo, como 0°). Little-endian en el cable, sea cual sea la máquina.
 */
struct EXPLORED_API FExploredBoatNetState
{
	static constexpr int32 SizeBytes = 19;
	static constexpr float RateHz = 20.0f;

	static constexpr int32 PositionXYBits = 21;
	static constexpr int32 PositionZBits = 14;
	static constexpr int32 MaxPositionXYCm = (1 << (PositionXYBits - 1)) - 1;
	static constexpr int32 MinPositionXYCm = -(1 << (PositionXYBits - 1));
	static constexpr int32 MaxPositionZCm = (1 << (PositionZBits - 1)) - 1;
	static constexpr int32 MinPositionZCm = -(1 << (PositionZBits - 1));
	static constexpr float VelocityStepCmS = 10.0f;
	static constexpr float WaterStepKg = 0.1f;
	static constexpr int32 MaxHeelDeg = 127;
	static constexpr int32 MaxTrimSteps = 127;
	static constexpr int32 MaxBreaches = 63;

	/** Valores cuantizados, tal y como viajan. */
	int32 PosX = 0;
	int32 PosY = 0;
	int32 PosZ = 0;
	int16 VelX = 0;
	int16 VelY = 0;
	uint16 Heading = 0;
	int8 Heel = 0;
	/** Bit 7: vela izada; bits 0–6: escota (0 cazada, 127 largada). */
	uint8 Sail = 0;
	uint16 WaterDeciKg = 0;
	uint8 Integrity = 255;
	/** Bits 0–5: brechas; bits 6–7: EBoatCondition. */
	uint8 Status = 0;

	/** Cuantiza el estado del servidor con la escota que manda el timonel y las brechas del casco por piezas. */
	static FExploredBoatNetState Quantize(const FBoatState& State, float SailTrim01, int32 Breaches);

	FBoatNetSnapshot Dequantize() const;

	/** Copia al estado del cliente lo que viaja (posición, velocidad, rumbo, escora, vela, agua, daño, estado); el resto no se toca. */
	void ApplyTo(FBoatState& State) const;

	void ToBytes(uint8 (&Out)[SizeBytes]) const;
	TArray<uint8> ToBytes() const;
	/** False (y Out sin tocar) si no son exactamente SizeBytes. */
	static bool FromBytes(const uint8* Data, int32 Num, FExploredBoatNetState& Out);

	bool operator==(const FExploredBoatNetState& Other) const;
	bool operator!=(const FExploredBoatNetState& Other) const { return !(*this == Other); }

	/** Bytes por segundo de un barco ocupado (biblia 08 §2.5: 380 B/s). */
	static constexpr float BytesPerSecond() { return SizeBytes * RateHz; }
};
