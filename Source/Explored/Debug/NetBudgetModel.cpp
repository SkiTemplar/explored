#include "Debug/NetBudgetModel.h"

namespace NetBudgetDetail
{
	bool RowLess(const FNetBudgetModel::FSecondSummary& A, const FNetBudgetModel::FSecondSummary& B)
	{
		const FString ClientA = A.Client.ToString();
		const FString ClientB = B.Client.ToString();
		if (ClientA != ClientB)
		{
			return ClientA < ClientB;
		}
		return A.SecondIndex < B.SecondIndex;
	}

	bool ChannelLess(const FNetBudgetModel::FChannelRow& A, const FNetBudgetModel::FChannelRow& B)
	{
		return A.Channel.ToString() < B.Channel.ToString();
	}
}

void FNetBudgetModel::RecordBytes(FName Client, FName Channel, int32 Bytes, double NowSeconds)
{
	if (Bytes <= 0 || !FMath::IsFinite(NowSeconds) || FMath::Abs(NowSeconds) > 1e12)
	{
		return;
	}

	const int64 SecondIndex = FMath::FloorToInt64(NowSeconds);
	FOpenSecond& Open = OpenByClient.FindOrAdd(Client);
	if (!Open.bStarted)
	{
		Open.bStarted = true;
		Open.SecondIndex = SecondIndex;
	}
	else if (SecondIndex > Open.SecondIndex)
	{
		ClosedQueue.Add(CloseSecond(Client, Open));
		Open.BytesByChannel.Reset();
		Open.SecondIndex = SecondIndex;
	}
	// Si SecondIndex < Open.SecondIndex (reloj que retrocede), se acumula igualmente en el
	// segundo ya abierto en vez de perder la muestra; no debería pasar con un reloj de red real.

	int64& Accum = Open.BytesByChannel.FindOrAdd(Channel);
	Accum += Bytes;
}

FNetBudgetModel::FSecondSummary FNetBudgetModel::CloseSecond(FName Client, const FOpenSecond& Open) const
{
	FSecondSummary Row;
	Row.Client = Client;
	Row.SecondIndex = Open.SecondIndex;
	for (const auto& Pair : Open.BytesByChannel)
	{
		const double Kbps = (static_cast<double>(Pair.Value) * 8.0) / 1000.0;
		Row.Channels.Add(FChannelRow{ Pair.Key, Kbps });
		Row.TotalKbps += Kbps;
	}
	Row.Channels.Sort(&NetBudgetDetail::ChannelLess);
	return Row;
}

void FNetBudgetModel::DrainClosedSeconds(TArray<FSecondSummary>& OutRows)
{
	ClosedQueue.Sort(&NetBudgetDetail::RowLess);
	OutRows = MoveTemp(ClosedQueue);
	ClosedQueue.Reset();
}

void FNetBudgetModel::CloseAllOpenSeconds()
{
	for (auto& Pair : OpenByClient)
	{
		if (Pair.Value.bStarted)
		{
			ClosedQueue.Add(CloseSecond(Pair.Key, Pair.Value));
			Pair.Value.BytesByChannel.Reset();
			Pair.Value.bStarted = false;
		}
	}
}

void FNetBudgetModel::Reset()
{
	OpenByClient.Reset();
	ClosedQueue.Reset();
}

bool FNetBudgetModel::Validate(const FSecondSummary& Row, double LimitKbps, FString& OutReason)
{
	if (Row.TotalKbps <= LimitKbps)
	{
		return true;
	}
	OutReason = FString::Printf(TEXT("%s en el segundo %lld: %.2f kbps supera el tope de %.2f kbps"),
		*Row.Client.ToString(), static_cast<long long>(Row.SecondIndex), Row.TotalKbps, LimitKbps);
	return false;
}

namespace NetBudgetDetail
{
	FString CsvField(const FString& Value)
	{
		if (!Value.Contains(TEXT(",")) && !Value.Contains(TEXT("\"")) && !Value.Contains(TEXT("\n")) && !Value.Contains(TEXT("\r")))
		{
			return Value;
		}
		return FString(TEXT("\"")) + Value.Replace(TEXT("\""), TEXT("\"\"")) + FString(TEXT("\""));
	}

	FString FormatKbps(double Kbps)
	{
		// %.3f de Printf usa siempre punto decimal (UE no aplica la configuración regional).
		return FString::Printf(TEXT("%.3f"), Kbps);
	}
}

bool FNetBudgetModel::ValidateSeries(const TArray<FSecondSummary>& Rows, EScenario Scenario, TArray<FString>& OutViolations)
{
	OutViolations.Reset();

	TArray<FSecondSummary> Sorted = Rows;
	Sorted.StableSort(&NetBudgetDetail::RowLess);

	const double TotalLimit = Scenario == EScenario::Rest ? RestKbpsLimit : PeakKbpsLimit;
	const TCHAR* ScenarioName = Scenario == EScenario::Rest ? TEXT("reposo") : TEXT("pico");
	const double BurstCreditKbpsSeconds = (TerrainBurstKbps - TerrainSustainedKbps) * TerrainBurstSeconds;
	const FName Terrain = TerrainChannel();

	FName CurrentClient;
	bool bHaveClient = false;
	int64 LastSecond = 0;
	double TerrainExcess = 0.0;

	for (const FSecondSummary& Row : Sorted)
	{
		const FString ClientName = Row.Client.ToString();
		if (!bHaveClient || !(Row.Client == CurrentClient))
		{
			bHaveClient = true;
			CurrentClient = Row.Client;
			TerrainExcess = 0.0;
		}
		else
		{
			// Los segundos sin fila no mandaron nada de terreno: la ráfaga se recupera.
			const int64 Gap = Row.SecondIndex - LastSecond - 1;
			if (Gap > 0)
			{
				TerrainExcess = FMath::Max(0.0, TerrainExcess - TerrainSustainedKbps * static_cast<double>(Gap));
			}
		}
		LastSecond = Row.SecondIndex;

		if (!FMath::IsFinite(Row.TotalKbps) || Row.TotalKbps > TotalLimit + KbpsTolerance)
		{
			OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: %.3f kbps en total supera el tope de %s (%.0f kbps)"),
				*ClientName, static_cast<long long>(Row.SecondIndex), Row.TotalKbps, ScenarioName, TotalLimit));
		}

		double TerrainKbps = 0.0;
		for (const FChannelRow& Channel : Row.Channels)
		{
			if (!FMath::IsFinite(Channel.Kbps))
			{
				OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: el canal %s no tiene un valor finito"),
					*ClientName, static_cast<long long>(Row.SecondIndex), *Channel.Channel.ToString()));
				continue;
			}
			if (Channel.Channel == Terrain)
			{
				TerrainKbps += Channel.Kbps;
			}
		}

		if (TerrainKbps > TerrainBurstKbps + KbpsTolerance)
		{
			OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: la cola de terreno manda %.3f kbps y su ráfaga es de %.0f kbps"),
				*ClientName, static_cast<long long>(Row.SecondIndex), TerrainKbps, TerrainBurstKbps));
		}
		TerrainExcess = FMath::Max(0.0, TerrainExcess + TerrainKbps - TerrainSustainedKbps);
		if (TerrainExcess > BurstCreditKbpsSeconds + KbpsTolerance)
		{
			OutViolations.Add(FString::Printf(TEXT("%s en el segundo %lld: la cola de terreno lleva más ráfaga de la permitida (%.0f kbps durante %.0f s)"),
				*ClientName, static_cast<long long>(Row.SecondIndex), TerrainBurstKbps, TerrainBurstSeconds));
		}
	}
	return OutViolations.IsEmpty();
}

FString FNetBudgetModel::ToCsv(const TArray<FSecondSummary>& Rows)
{
	FString Csv(TEXT("cliente,segundo,canal,kbps\n"));
	for (const FSecondSummary& Row : Rows)
	{
		const FString Client = NetBudgetDetail::CsvField(Row.Client.ToString());
		const FString Second = FString::Printf(TEXT("%lld"), static_cast<long long>(Row.SecondIndex));
		for (const FChannelRow& Channel : Row.Channels)
		{
			Csv += Client + TEXT(",") + Second + TEXT(",") + NetBudgetDetail::CsvField(Channel.Channel.ToString())
				+ TEXT(",") + NetBudgetDetail::FormatKbps(Channel.Kbps) + TEXT("\n");
		}
		Csv += Client + TEXT(",") + Second + TEXT(",total,") + NetBudgetDetail::FormatKbps(Row.TotalKbps) + TEXT("\n");
	}
	return Csv;
}
