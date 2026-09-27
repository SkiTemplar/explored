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

	/** Añade Bytes enviados por Channel a Client en el instante NowSeconds (servidor → cliente). Ignora Bytes <= 0. */
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

private:
	struct FOpenSecond
	{
		int64 SecondIndex = 0;
		bool bStarted = false;
		TMap<FName, int32> BytesByChannel;
	};

	FSecondSummary CloseSecond(FName Client, const FOpenSecond& Open) const;

	TMap<FName, FOpenSecond> OpenByClient;
	TArray<FSecondSummary> ClosedQueue;
};
