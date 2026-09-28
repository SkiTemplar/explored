#pragma once

#include "CoreMinimal.h"

/**
 * Contador y validador de presupuesto de red por segundo (biblia 08 «Cooperativo y red»
 * §3): acumula bytes enviados por canal y por cliente, cierra cada segundo entero y
 * compara el total contra los techos duros del presupuesto. Es la base del comando
 * `Explored.NetBudget` (00-TODO.md, hito H0): el comando solo llama a esto y vuelca el
 * resultado a CSV; toda la lógica de acumulación y validación vive aquí, sin motor.
 */
class EXPLORED_API FNetBudgetModel
{
public:
	/** Techo duro en reposo (biblia 08 §3): sin ráfagas declaradas no debería superarse. */
	static constexpr double RestKbpsLimit = 64.0;
	/** Techo duro de pico (biblia 08 §3), con las ráfagas declaradas incluidas. */
	static constexpr double PeakKbpsLimit = 256.0;
	/** Cola de terreno (biblia 08 §2.2 y §3): 64 kbps sostenidos y 128 kbps de ráfaga durante 5 s. */
	static constexpr double TerrainSustainedKbps = 64.0;
	static constexpr double TerrainBurstKbps = 128.0;
	static constexpr double TerrainBurstSeconds = 5.0;
	/** Margen de redondeo al comparar kbps que vienen de sumar bytes enteros. */
	static constexpr double KbpsTolerance = 1e-6;

	/** Nombre del canal de la cola de terreno en el CSV. */
	static FName TerrainChannel() { return FName(TEXT("Terreno")); }

	/** Escenario de la matriz de biblia 08 §7.3 que se está midiendo. */
	enum class EScenario : uint8
	{
		/** En reposo: el total no pasa de RestKbpsLimit. */
		Rest,
		/** En pico: el total no pasa de PeakKbpsLimit. */
		Peak,
	};

	struct FChannelRow
	{
		FName Channel;
		double Kbps = 0.0;
	};

	/** Fila cerrada de un segundo entero para un cliente: kbps por canal, ordenados por nombre de canal. */
	struct FSecondSummary
	{
		FName Client;
		int64 SecondIndex = 0;
		TArray<FChannelRow> Channels;
		double TotalKbps = 0.0;
	};

	/**
	 * Añade Bytes enviados por Channel a Client en el instante NowSeconds (servidor →
	 * cliente). Ignora Bytes <= 0 y un NowSeconds no finito o fuera de ±1e12 s (convertirlo
	 * a segundo entero sería comportamiento indefinido).
	 */
	void RecordBytes(FName Client, FName Channel, int32 Bytes, double NowSeconds);

	/**
	 * Mueve a `OutRows` todas las filas de segundos ya cerrados (los que un `RecordBytes`
	 * posterior ha dejado atrás), ordenadas por (cliente, segundo) ascendente, y las quita
	 * del histórico interno: cada segundo se drena una sola vez.
	 */
	void DrainClosedSeconds(TArray<FSecondSummary>& OutRows);

	/** Cierra el segundo en curso de todos los clientes sin esperar a un RecordBytes posterior (último segundo de una sesión). */
	void CloseAllOpenSeconds();

	void Reset();

	/** true si Row.TotalKbps no pasa de LimitKbps; si no, false con el motivo en OutReason. */
	static bool Validate(const FSecondSummary& Row, double LimitKbps, FString& OutReason);

	/**
	 * Valida una serie de filas (de uno o varios clientes, en cualquier orden) contra el
	 * presupuesto de biblia 08 §3. Por cada cliente y segundo:
	 * - el total no pasa del techo del escenario (64 kbps en reposo, 256 en pico);
	 * - el canal de terreno no pasa nunca de TerrainBurstKbps;
	 * - el canal de terreno no gasta más ráfaga de la que hay: el exceso acumulado sobre
	 *   TerrainSustainedKbps (que se recupera a ese mismo ritmo, y un segundo sin fila
	 *   cuenta como cero bytes) no pasa de (128 − 64) kbps × 5 s. Es el dual exacto del
	 *   cubo de fichas de `FTerrainDeltaQueueModel`: cinco segundos seguidos a 128 kbps
	 *   pasan, un sexto no.
	 * Devuelve true si no hay ninguna violación; si las hay, una línea por violación en
	 * `OutViolations`, ordenadas por (cliente, segundo). Un total o canal no finito es
	 * siempre una violación.
	 */
	static bool ValidateSeries(const TArray<FSecondSummary>& Rows, EScenario Scenario, TArray<FString>& OutViolations);

	/**
	 * CSV en formato largo, estable para comparar entre ejecuciones: cabecera
	 * `cliente,segundo,canal,kbps` y, por fila, una línea por canal más una `total`. Punto
	 * decimal y tres decimales, sin depender de la configuración regional. Un nombre de
	 * cliente con coma, comillas o salto de línea va entre comillas dobles (RFC 4180).
	 */
	static FString ToCsv(const TArray<FSecondSummary>& Rows);

private:
	struct FOpenSecond
	{
		int64 SecondIndex = 0;
		bool bStarted = false;
		TMap<FName, int64> BytesByChannel;
	};

	FSecondSummary CloseSecond(FName Client, const FOpenSecond& Open) const;

	TMap<FName, FOpenSecond> OpenByClient;
	TArray<FSecondSummary> ClosedQueue;
};
