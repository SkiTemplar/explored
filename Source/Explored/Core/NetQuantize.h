#pragma once

#include "CoreMinimal.h"

/**
 * Cuantización y lectura/escritura de bytes para los paquetes de red de tamaño fijo de
 * la biblia 08 (reloj y clima, anclas de fauna, vegetación, barcos). Modelo puro: sin
 * UObject ni FArchive, para que el formato exacto del cable se pruebe en Tools/HostTests.
 *
 * Todo va en little-endian y con tamaño fijo: un paquete que no mide lo que debe se
 * rechaza entero antes de tocar el estado. Las entradas no finitas (NaN, ±inf) se
 * sanean de forma explícita, sin comparar en positivo, porque el editor compila con
 * matemáticas rápidas y ahí `!(x > 0)` no descarta los NaN (ver fix/amarre-nan).
 */
namespace ExploredNet
{
	/** Escritor de bytes en little-endian sobre un TArray<uint8>. */
	struct EXPLORED_API FByteWriter
	{
		TArray<uint8>& Out;

		explicit FByteWriter(TArray<uint8>& InOut) : Out(InOut) {}

		void U8(uint8 V) { Out.Add(V); }
		void I8(int8 V) { Out.Add(static_cast<uint8>(V)); }
		void U16(uint16 V);
		void I16(int16 V) { U16(static_cast<uint16>(V)); }
		void U32(uint32 V);
	};

	/** Lector de bytes en little-endian; `bOk` pasa a false (y se queda) al leer de más. */
	struct EXPLORED_API FByteReader
	{
		const TArray<uint8>& In;
		int32 Pos = 0;
		bool bOk = true;

		explicit FByteReader(const TArray<uint8>& InBytes) : In(InBytes) {}

		uint8 U8();
		int8 I8() { return static_cast<int8>(U8()); }
		uint16 U16();
		int16 I16() { return static_cast<int16>(U16()); }
		uint32 U32();

		/** Leído todo y sin errores. */
		bool IsDone() const { return bOk && Pos == In.Num(); }
	};

	/** NaN e infinitos → Fallback; el resto, tal cual. */
	EXPLORED_API double Finite(double V, double Fallback = 0.0);

	/** Redondeo al entero más cercano acotado a [Lo, Hi], a salvo de NaN (→ Lo si Lo ≤ 0 ≤ Hi no se cumple, 0 si sí). */
	EXPLORED_API int64 RoundClamped(double V, int64 Lo, int64 Hi);

	/** 0–1 → 0–255 (redondeo, acotado). NaN → 0. */
	EXPLORED_API uint8 Quantize01(float V);
	EXPLORED_API float Dequantize01(uint8 V);

	/** Ángulo en grados → 0–255 (vuelta completa en 256 pasos, envolviendo). NaN → 0. */
	EXPLORED_API uint8 QuantizeAngle8(float Deg);
	EXPLORED_API float DequantizeAngle8(uint8 V);

	/** Ángulo en grados → 0–65535 (0,0055° por paso, envolviendo). NaN → 0. */
	EXPLORED_API uint16 QuantizeAngle16(float Deg);
	EXPLORED_API float DequantizeAngle16(uint16 V);

	/** Diferencia angular más corta B − A en grados, en (−180, 180]. */
	EXPLORED_API float AngleDeltaDeg(float A, float B);

	/**
	 * Posición del mundo en 7 bytes (56 bits), el hueco que la biblia 08 reserva para
	 * `FVector_NetQuantize100` en las anclas de fauna y en el estado de barco:
	 * X e Y con 22 bits con signo a 2 cm (±41,9 km, el archipiélago mide 6,4 km) y Z con
	 * 12 bits con signo a 10 cm (±204,7 m, de la fosa a las bandadas). Fuera de rango se
	 * acota al borde; NaN → 0.
	 */
	constexpr int32 PositionBytes = 7;
	constexpr double PositionXYStepCm = 2.0;
	constexpr double PositionZStepCm = 10.0;
	constexpr int32 PositionXYBits = 22;
	constexpr int32 PositionZBits = 12;
	/** Mayor |X| o |Y| representable (cm). */
	constexpr double PositionXYMaxCm = ((1 << (PositionXYBits - 1)) - 1) * PositionXYStepCm;
	/** Mayor |Z| representable (cm). */
	constexpr double PositionZMaxCm = ((1 << (PositionZBits - 1)) - 1) * PositionZStepCm;

	EXPLORED_API void WritePosition7(FByteWriter& W, const FVector& PositionCm);
	EXPLORED_API FVector ReadPosition7(FByteReader& R);
	/** La posición tal como llega al otro lado (útil para comparar y para el servidor). */
	EXPLORED_API FVector QuantizePosition7(const FVector& PositionCm);
}
