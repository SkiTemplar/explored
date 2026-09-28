#include "Items/ContentIdTableModel.h"

const TCHAR* LexToString(EContentKind Kind)
{
	switch (Kind)
	{
	case EContentKind::Item: return TEXT("items.json");
	case EContentKind::Template: return TEXT("templates.json");
	case EContentKind::BuildingPiece: return TEXT("building_pieces.json");
	case EContentKind::Plant: return TEXT("plants.json");
	case EContentKind::Boat: return TEXT("boats.json");
	case EContentKind::Achievement: return TEXT("achievements.json");
	default: return TEXT("?");
	}
}

bool FContentIdTableModel::IsValidContentId(const FString& Id)
{
	if (Id.Len() == 0)
	{
		return false;
	}
	for (int32 I = 0; I < Id.Len(); ++I)
	{
		const TCHAR C = Id[I];
		const bool bOk = (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_');
		if (!bOk)
		{
			return false;
		}
	}
	return true;
}

bool FContentIdTableModel::OrdinalLess(const FString& A, const FString& B)
{
	const int32 Common = FMath::Min(A.Len(), B.Len());
	for (int32 I = 0; I < Common; ++I)
	{
		const uint32 CA = static_cast<uint32>(A[I]);
		const uint32 CB = static_cast<uint32>(B[I]);
		if (CA != CB)
		{
			return CA < CB;
		}
	}
	return A.Len() < B.Len();
}

bool FContentIdTableModel::SetIds(EContentKind Kind, const TArray<FName>& InIds, FString& OutError)
{
	const int32 K = static_cast<int32>(Kind);
	if (K < 0 || K >= static_cast<int32>(EContentKind::Count))
	{
		OutError = TEXT("Tabla desconocida");
		return false;
	}
	if (InIds.Num() > MaxIdsPerKind)
	{
		OutError = FString::Printf(TEXT("%s: %d ids no caben en uint16"), LexToString(Kind), InIds.Num());
		return false;
	}

	// FName no distingue mayúsculas y conserva las de la primera vez que se vio:
	// se compara siempre en minúsculas para que el orden no dependa de eso.
	TArray<FString> Keys;
	Keys.Reserve(InIds.Num());
	for (const FName& Id : InIds)
	{
		const FString Key = Id.IsNone() ? FString() : Id.ToString().ToLower();
		if (!IsValidContentId(Key))
		{
			OutError = FString::Printf(TEXT("%s: id no válido «%s» (solo a-z, 0-9 y _)"), LexToString(Kind), *Key);
			return false;
		}
		Keys.Add(Key);
	}
	Keys.Sort([](const FString& A, const FString& B) { return OrdinalLess(A, B); });
	for (int32 I = 1; I < Keys.Num(); ++I)
	{
		if (Keys[I] == Keys[I - 1])
		{
			OutError = FString::Printf(TEXT("%s: id repetido «%s»"), LexToString(Kind), *Keys[I]);
			return false;
		}
	}

	Ids[K].Reset();
	Index[K].Reset();
	for (int32 I = 0; I < Keys.Num(); ++I)
	{
		const FName Name(*Keys[I]);
		Ids[K].Add(Name);
		Index[K].Add(Name, static_cast<uint16>(I));
	}
	OutError.Reset();
	return true;
}

uint16 FContentIdTableModel::ToNetId(EContentKind Kind, FName Id) const
{
	const int32 K = static_cast<int32>(Kind);
	if (K < 0 || K >= static_cast<int32>(EContentKind::Count))
	{
		return InvalidNetId;
	}
	const uint16* Found = Index[K].Find(Id);
	return Found ? *Found : InvalidNetId;
}

FName FContentIdTableModel::FromNetId(EContentKind Kind, uint16 NetId) const
{
	const int32 K = static_cast<int32>(Kind);
	if (K < 0 || K >= static_cast<int32>(EContentKind::Count) || !Ids[K].IsValidIndex(NetId))
	{
		return NAME_None;
	}
	return Ids[K][NetId];
}

int32 FContentIdTableModel::Num(EContentKind Kind) const
{
	const int32 K = static_cast<int32>(Kind);
	return (K >= 0 && K < static_cast<int32>(EContentKind::Count)) ? Ids[K].Num() : 0;
}

const TArray<FName>& FContentIdTableModel::GetIds(EContentKind Kind) const
{
	static const TArray<FName> Empty;
	const int32 K = static_cast<int32>(Kind);
	return (K >= 0 && K < static_cast<int32>(EContentKind::Count)) ? Ids[K] : Empty;
}

uint64 FContentIdTableModel::Fnv1a64(const uint8* Data, int32 Num, uint64 Hash)
{
	for (int32 I = 0; I < Num; ++I)
	{
		Hash ^= static_cast<uint64>(Data[I]);
		Hash *= FnvPrime;
	}
	return Hash;
}

uint64 FContentIdTableModel::ComputeContentHash(const TArray<FContentFile>& Files)
{
	// Índices y no punteros: TArray::Sort de UE desreferencia los punteros al llamar al predicado.
	TArray<int32> Order;
	Order.Reserve(Files.Num());
	for (int32 I = 0; I < Files.Num(); ++I)
	{
		Order.Add(I);
	}
	Order.Sort([&Files](int32 A, int32 B) { return OrdinalLess(Files[A].Name, Files[B].Name); });

	uint64 Hash = FnvOffsetBasis;
	for (int32 FileIndex : Order)
	{
		const FContentFile* File = &Files[FileIndex];
		// Los nombres de Content/Data son ASCII: un byte por carácter.
		for (int32 I = 0; I < File->Name.Len(); ++I)
		{
			const uint8 Byte = static_cast<uint8>(File->Name[I] & 0xFF);
			Hash = Fnv1a64(&Byte, 1, Hash);
		}
		const uint8 Separator = 0;
		Hash = Fnv1a64(&Separator, 1, Hash);
		uint8 Length[8];
		const uint64 Num = static_cast<uint64>(File->Bytes.Num());
		for (int32 I = 0; I < 8; ++I)
		{
			Length[I] = static_cast<uint8>((Num >> (8 * I)) & 0xFF);
		}
		Hash = Fnv1a64(Length, 8, Hash);
		Hash = Fnv1a64(File->Bytes.GetData(), File->Bytes.Num(), Hash);
	}
	return Hash;
}
