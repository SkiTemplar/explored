#include "Fauna/FaunaAnchorNetModel.h"

#include "Core/NetQuantize.h"

void FFaunaAnchorNetModel::Append(const FFaunaGroupAnchor& Anchor, TArray<uint8>& Out)
{
	ExploredNet::FByteWriter W(Out);
	W.U16(Anchor.GroupId);
	ExploredNet::WritePosition7(W, Anchor.CentroidCm);
	W.U8(Anchor.BrainState);
}

bool FFaunaAnchorNetModel::EncodeBatch(const TArray<FFaunaGroupAnchor>& Anchors, TArray<uint8>& Out)
{
	Out.Reset();
	if (Anchors.Num() > MaxGroupsPerClient)
	{
		return false;
	}
	for (int32 i = 0; i < Anchors.Num(); ++i)
	{
		for (int32 j = 0; j < i; ++j)
		{
			if (Anchors[j].GroupId == Anchors[i].GroupId)
			{
				Out.Reset();
				return false;
			}
		}
		Append(Anchors[i], Out);
	}
	return true;
}

bool FFaunaAnchorNetModel::DecodeBatch(const TArray<uint8>& In, TArray<FFaunaGroupAnchor>& OutAnchors)
{
	OutAnchors.Reset();
	if (In.Num() % PacketBytes != 0 || In.Num() / PacketBytes > MaxGroupsPerClient)
	{
		return false;
	}
	ExploredNet::FByteReader R(In);
	TArray<FFaunaGroupAnchor> Read;
	while (R.bOk && R.Pos < In.Num())
	{
		FFaunaGroupAnchor Anchor;
		Anchor.GroupId = R.U16();
		Anchor.CentroidCm = ExploredNet::ReadPosition7(R);
		Anchor.BrainState = R.U8();
		for (const FFaunaGroupAnchor& Seen : Read)
		{
			if (Seen.GroupId == Anchor.GroupId)
			{
				return false;
			}
		}
		Read.Add(Anchor);
	}
	if (!R.IsDone())
	{
		return false;
	}
	OutAnchors = MoveTemp(Read);
	return true;
}

FFaunaGroupAnchor FFaunaAnchorNetModel::Quantize(const FFaunaGroupAnchor& Anchor)
{
	FFaunaGroupAnchor Out = Anchor;
	Out.CentroidCm = ExploredNet::QuantizePosition7(Anchor.CentroidCm);
	return Out;
}

void FFaunaAnchorNetModel::SelectForClient(const TArray<FFaunaGroupAnchor>& Groups, const FVector& ReceiverCm, TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	if (!FMath::IsFinite(ReceiverCm.X) || !FMath::IsFinite(ReceiverCm.Y) || !FMath::IsFinite(ReceiverCm.Z))
	{
		return;
	}
	struct FCandidate
	{
		int32 Index;
		double DistSq;
		uint16 GroupId;
	};
	TArray<FCandidate> Candidates;
	const double MaxSq = RelevanceRadiusCm * RelevanceRadiusCm;
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		const double DistSq = FVector::DistSquared(Groups[Index].CentroidCm, ReceiverCm);
		// Comprobación explícita: con matemáticas rápidas, NaN <= MaxSq puede dar cierto.
		if (FMath::IsFinite(DistSq) && DistSq <= MaxSq)
		{
			Candidates.Add({Index, DistSq, Groups[Index].GroupId});
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.DistSq != B.DistSq)
		{
			return A.DistSq < B.DistSq;
		}
		if (A.GroupId != B.GroupId)
		{
			return A.GroupId < B.GroupId;
		}
		return A.Index < B.Index;
	});
	for (const FCandidate& Candidate : Candidates)
	{
		if (OutIndices.Num() >= MaxGroupsPerClient)
		{
			break;
		}
		// Un id repetido (dos grupos con el mismo id) no puede ir dos veces en el lote.
		bool bDuplicate = false;
		for (int32 Chosen : OutIndices)
		{
			bDuplicate |= Groups[Chosen].GroupId == Candidate.GroupId;
		}
		if (!bDuplicate)
		{
			OutIndices.Add(Candidate.Index);
		}
	}
}

double FFaunaAnchorNetModel::SendPhaseSeconds(uint16 GroupId)
{
	// Secuencia de Weyl con la razón áurea: ids consecutivos quedan repartidos por el periodo.
	constexpr uint32 Golden = 0x9E3779B9u;
	const uint32 Mixed = static_cast<uint32>(GroupId) * Golden;
	return (static_cast<double>(Mixed) / 4294967296.0) * SendIntervalSeconds;
}

bool FFaunaAnchorNetModel::IsDue(uint16 GroupId, double PrevSeconds, double NowSeconds)
{
	if (!FMath::IsFinite(PrevSeconds) || !FMath::IsFinite(NowSeconds) || NowSeconds <= PrevSeconds)
	{
		return false;
	}
	const double Phase = SendPhaseSeconds(GroupId);
	const double PrevSlot = FMath::FloorToDouble((PrevSeconds - Phase) / SendIntervalSeconds);
	const double NowSlot = FMath::FloorToDouble((NowSeconds - Phase) / SendIntervalSeconds);
	return NowSlot > PrevSlot;
}

float FFaunaAnchorNetModel::PullAlpha(float SecondsSinceAnchor, float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return 0.0f;
	}
	const float Elapsed = FMath::IsFinite(SecondsSinceAnchor) ? FMath::Max(0.0f, SecondsSinceAnchor) : PullSeconds;
	// Holgura de 0,1 ms: el tiempo acumulado en float no debe dejar un último paso con
	// alfa 0,9999 que arrastre un resto de milímetros un fotograma más.
	const float Remaining = PullSeconds - Elapsed;
	if (Remaining <= DeltaSeconds + 1e-4f)
	{
		return 1.0f;
	}
	return DeltaSeconds / Remaining;
}
