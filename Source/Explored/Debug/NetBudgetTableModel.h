#pragma once

#include "CoreMinimal.h"

/** Un canal del presupuesto de red: mensajes de un tamaño a una frecuencia, o una reserva fija. */
struct EXPLORED_API FNetBudgetChannel
{
	FString Name;
	double BytesPerMessage = 0.0;
	double MessagesPerSecond = 0.0;
	/** Cuántos emisores iguales (personajes, animales, barcos…). */
	double Count = 1.0;
	/** Reserva medida o estimada sin modelo propio todavía (kbps); se suma aparte. */
	double FixedKbps = 0.0;

	double Kbps() const { return BytesPerMessage * MessagesPerSecond * Count * 8.0 / 1000.0 + FixedKbps; }
};

/**
 * Tabla de presupuesto de ancho de banda servidor → cliente (biblia 08 §3), escrita con
 * los tamaños reales de los paquetes de los modelos de red (reloj de 11 B, anclas de 10 B,
 * vegetación de 14 B por cambio, barco de 19 B) para que un cambio de formato que rompa
 * el objetivo lo cace un test, no la beta. Los canales que aún no tienen modelo propio
 * (personajes, terreno, cartografía) llevan la cifra de la biblia como reserva fija.
 *
 * Es la estimación de diseño; la medida real es el CSV de `Explored.NetBudget` (H0) y el
 * criterio de salida de H5 se cumple con ese CSV, no con esta tabla.
 */
class EXPLORED_API FNetBudgetTableModel
{
public:
	/** Objetivo duro por cliente en reposo y en pico (criterio de salida de H5). */
	static constexpr double RestTargetKbps = 64.0;
	static constexpr double PeakTargetKbps = 256.0;

	// Canales sin modelo de paquete propio todavía (cifras de biblia 08).
	static constexpr double CharacterBytes = 24.0;
	static constexpr double CharacterHz = 20.0;
	static constexpr double TerrestrialAnimalBytes = 14.0;
	/** Tope duro de fauna terrestre replicada por cliente (08 §2.7 b). */
	static constexpr int32 NearAnimalsCap = 12;
	static constexpr double NearAnimalsHz = 10.0;
	static constexpr int32 FarAnimalsCap = 24;
	static constexpr double FarAnimalsHz = 2.0;
	static constexpr double OwnBodyBytes = 24.0;
	static constexpr double OwnBodyHz = 1.0;
	static constexpr double OtherBodyBytes = 2.0;
	static constexpr double OtherBodyHz = 0.5;
	static constexpr double ChunkChecksBytesPerSecond = 5.0;
	static constexpr double ActorChannelsReserveKbps = 1.5;
	static constexpr double LooseObjectBytes = 12.0;
	static constexpr int32 LooseObjectsAwakeCap = 32;
	static constexpr double LooseObjectHz = 10.0;
	/** Ráfaga completa de terreno al unirse o entrar en chunks nuevos (08 §3), durante 5 s. */
	static constexpr double TerrainBurstKbps = 128.0;

	/** Reposo (08 §3): Players jugadores juntos en la base, nadie cavando ni talando. */
	static TArray<FNetBudgetChannel> RestTable(int32 Players);

	/** Pico realista (08 §3): pala, tala, dos barcos en ciclón, arena, cofre abierto… */
	static TArray<FNetBudgetChannel> PeakTable(int32 Players);

	/** Pico más la ráfaga completa de terreno. */
	static TArray<FNetBudgetChannel> PeakWithTerrainBurstTable(int32 Players);

	static double TotalKbps(const TArray<FNetBudgetChannel>& Channels);

	/** Fracción libre del objetivo (0,48 = 48 % de margen); negativa si se pasa. */
	static double Margin01(const TArray<FNetBudgetChannel>& Channels, double TargetKbps);

	/**
	 * True si el total cabe en el objetivo. Si no, OutReason dice cuánto se pasa y cuáles
	 * son los tres canales más caros, que es por donde hay que recortar.
	 */
	static bool FitsTarget(const TArray<FNetBudgetChannel>& Channels, double TargetKbps, FString& OutReason);

	/** «canal;kbps» por línea con la fila de total, en el orden de la tabla (mismo formato que el CSV de NetBudget). */
	static FString ToCsv(const TArray<FNetBudgetChannel>& Channels);
};
