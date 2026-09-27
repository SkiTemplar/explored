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
	if (Bytes <= 0)
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

	int32& Accum = Open.BytesByChannel.FindOrAdd(Channel);
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
		*Row.Client.ToString(), Row.SecondIndex, Row.TotalKbps, LimitKbps);
	return false;
}
