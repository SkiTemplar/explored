#include "Save/SaveWorldDeltas.h"

namespace SaveWorldDeltasDetail
{
	const ANSICHAR* const Base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	int32 PopCount(uint32 V)
	{
		V = V - ((V >> 1) & 0x55555555u);
		V = (V & 0x33333333u) + ((V >> 2) & 0x33333333u);
		return static_cast<int32>((((V + (V >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
	}

	int32 Base64Value(TCHAR C)
	{
		if (C >= TEXT('A') && C <= TEXT('Z')) { return static_cast<int32>(C - TEXT('A')); }
		if (C >= TEXT('a') && C <= TEXT('z')) { return static_cast<int32>(C - TEXT('a')) + 26; }
		if (C >= TEXT('0') && C <= TEXT('9')) { return static_cast<int32>(C - TEXT('0')) + 52; }
		if (C == TEXT('+')) { return 62; }
		if (C == TEXT('/')) { return 63; }
		return -1;
	}

	void AppendNumber(FString& Out, int32 Value)
	{
		Out += FString::Printf(TEXT("%d"), Value);
	}

	/** Lee un entero decimal sin signo en [0, Max]; avanza Pos. */
	bool ReadNumber(const TCHAR* Data, int32 Len, int32& Pos, int32 Max, int32& Out)
	{
		const int32 Start = Pos;
		int64 Value = 0;
		while (Pos < Len && Data[Pos] >= TEXT('0') && Data[Pos] <= TEXT('9'))
		{
			Value = Value * 10 + static_cast<int64>(Data[Pos] - TEXT('0'));
			if (Value > Max)
			{
				return false;
			}
			++Pos;
		}
		if (Pos == Start || (Pos - Start > 1 && Data[Start] == TEXT('0')))
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	bool CellLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
	}
}

// ---------------------------------------------------------------------------
// FSaveIndexSet
// ---------------------------------------------------------------------------

bool FSaveIndexSet::Add(int32 Index)
{
	if (Index < 0 || Index > MaxIndex)
	{
		return false;
	}
	const int32 Word = Index >> 5;
	const uint32 Bit = 1u << (Index & 31);
	if (Word >= Words.Num())
	{
		Words.SetNumZeroed(Word + 1);
	}
	if (Words[Word] & Bit)
	{
		return false;
	}
	Words[Word] |= Bit;
	++Count;
	return true;
}

bool FSaveIndexSet::Remove(int32 Index)
{
	if (!Contains(Index))
	{
		return false;
	}
	Words[Index >> 5] &= ~(1u << (Index & 31));
	--Count;
	TrimTrailingZeros();
	return true;
}

bool FSaveIndexSet::Contains(int32 Index) const
{
	if (Index < 0 || Index > MaxIndex)
	{
		return false;
	}
	const int32 Word = Index >> 5;
	return Word < Words.Num() && (Words[Word] & (1u << (Index & 31))) != 0;
}

void FSaveIndexSet::Reset()
{
	Words.Reset();
	Count = 0;
}

int32 FSaveIndexSet::Union(const FSaveIndexSet& Other)
{
	if (Other.Words.Num() > Words.Num())
	{
		Words.SetNumZeroed(Other.Words.Num());
	}
	const int32 Before = Count;
	for (int32 I = 0; I < Other.Words.Num(); ++I)
	{
		const uint32 Added = Other.Words[I] & ~Words[I];
		Words[I] |= Other.Words[I];
		Count += SaveWorldDeltasDetail::PopCount(Added);
	}
	TrimTrailingZeros();
	return Count - Before;
}

TArray<int32> FSaveIndexSet::ToArray() const
{
	TArray<int32> Out;
	Out.Reserve(Count);
	for (int32 W = 0; W < Words.Num(); ++W)
	{
		uint32 Bits = Words[W];
		for (int32 B = 0; Bits != 0; ++B, Bits >>= 1)
		{
			if (Bits & 1u)
			{
				Out.Add(W * 32 + B);
			}
		}
	}
	return Out;
}

void FSaveIndexSet::SetRange(int32 First, int32 Last)
{
	const int32 LastWord = Last >> 5;
	if (LastWord >= Words.Num())
	{
		Words.SetNumZeroed(LastWord + 1);
	}
	for (int32 W = First >> 5; W <= LastWord; ++W)
	{
		const int32 Lo = W == (First >> 5) ? (First & 31) : 0;
		const int32 Hi = W == LastWord ? (Last & 31) : 31;
		const uint32 HighMask = Hi == 31 ? 0xFFFFFFFFu : ((1u << (Hi + 1)) - 1u);
		const uint32 Mask = HighMask & ~((1u << Lo) - 1u);
		Count += SaveWorldDeltasDetail::PopCount(Mask & ~Words[W]);
		Words[W] |= Mask;
	}
}

void FSaveIndexSet::TrimTrailingZeros()
{
	int32 NewNum = Words.Num();
	while (NewNum > 0 && Words[NewNum - 1] == 0)
	{
		--NewNum;
	}
	if (NewNum != Words.Num())
	{
		Words.SetNum(NewNum);
	}
}

FString FSaveIndexSet::EncodeRanges() const
{
	FString Out = TEXT("r:");
	bool bFirst = true;
	int32 RangeStart = -1;
	int32 Previous = -2;
	auto Flush = [&Out, &bFirst](int32 Start, int32 End)
	{
		if (!bFirst)
		{
			Out.AppendChar(TEXT(','));
		}
		bFirst = false;
		SaveWorldDeltasDetail::AppendNumber(Out, Start);
		if (End != Start)
		{
			Out.AppendChar(TEXT('-'));
			SaveWorldDeltasDetail::AppendNumber(Out, End);
		}
	};
	for (int32 W = 0; W < Words.Num(); ++W)
	{
		if (Words[W] == 0)
		{
			continue;
		}
		for (int32 B = 0; B < 32; ++B)
		{
			if ((Words[W] & (1u << B)) == 0)
			{
				continue;
			}
			const int32 Index = W * 32 + B;
			if (Index != Previous + 1)
			{
				if (RangeStart >= 0)
				{
					Flush(RangeStart, Previous);
				}
				RangeStart = Index;
			}
			Previous = Index;
		}
	}
	if (RangeStart >= 0)
	{
		Flush(RangeStart, Previous);
	}
	return Out;
}

FString FSaveIndexSet::EncodeBits() const
{
	// Bytes en little endian, sin los ceros finales; base64 sin relleno.
	TArray<uint8> Bytes;
	Bytes.Reserve(Words.Num() * 4);
	for (const uint32 Word : Words)
	{
		Bytes.Add(static_cast<uint8>(Word & 0xFF));
		Bytes.Add(static_cast<uint8>((Word >> 8) & 0xFF));
		Bytes.Add(static_cast<uint8>((Word >> 16) & 0xFF));
		Bytes.Add(static_cast<uint8>((Word >> 24) & 0xFF));
	}
	while (Bytes.Num() > 0 && Bytes.Last() == 0)
	{
		Bytes.Pop();
	}
	FString Out = TEXT("b:");
	const ANSICHAR* const Alphabet = SaveWorldDeltasDetail::Base64Alphabet;
	for (int32 I = 0; I < Bytes.Num(); I += 3)
	{
		const int32 Remaining = Bytes.Num() - I;
		const uint32 Chunk = (static_cast<uint32>(Bytes[I]) << 16)
			| (Remaining > 1 ? static_cast<uint32>(Bytes[I + 1]) << 8 : 0u)
			| (Remaining > 2 ? static_cast<uint32>(Bytes[I + 2]) : 0u);
		Out.AppendChar(static_cast<TCHAR>(Alphabet[(Chunk >> 18) & 63]));
		Out.AppendChar(static_cast<TCHAR>(Alphabet[(Chunk >> 12) & 63]));
		if (Remaining > 1)
		{
			Out.AppendChar(static_cast<TCHAR>(Alphabet[(Chunk >> 6) & 63]));
		}
		if (Remaining > 2)
		{
			Out.AppendChar(static_cast<TCHAR>(Alphabet[Chunk & 63]));
		}
	}
	return Out;
}

FString FSaveIndexSet::Encode() const
{
	FString Ranges = EncodeRanges();
	FString Bits = EncodeBits();
	return Bits.Len() < Ranges.Len() ? Bits : Ranges;
}

bool FSaveIndexSet::DecodeRanges(const FString& Text)
{
	const TCHAR* Data = *Text;
	const int32 Len = Text.Len();
	int32 Pos = 2;
	int32 PreviousEnd = -1;
	if (Pos == Len)
	{
		return true;
	}
	while (true)
	{
		int32 First = 0;
		if (!SaveWorldDeltasDetail::ReadNumber(Data, Len, Pos, MaxIndex, First))
		{
			return false;
		}
		int32 Last = First;
		if (Pos < Len && Data[Pos] == TEXT('-'))
		{
			++Pos;
			if (!SaveWorldDeltasDetail::ReadNumber(Data, Len, Pos, MaxIndex, Last) || Last < First)
			{
				return false;
			}
		}
		// Rangos estrictamente ascendentes: acota el trabajo a MaxIndex aunque el texto sea hostil.
		if (First <= PreviousEnd)
		{
			return false;
		}
		SetRange(First, Last);
		PreviousEnd = Last;
		if (Pos == Len)
		{
			return true;
		}
		if (Data[Pos] != TEXT(','))
		{
			return false;
		}
		++Pos;
	}
}

bool FSaveIndexSet::DecodeBits(const FString& Text)
{
	const TCHAR* Data = *Text;
	const int32 Len = Text.Len() - 2;
	if (Len % 4 == 1 || Len > ((MaxIndex + 1) / 8 * 4 + 2) / 3)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Reserve(Len * 3 / 4 + 1);
	uint32 Buffer = 0;
	int32 BufferBits = 0;
	for (int32 I = 0; I < Len; ++I)
	{
		const int32 Value = SaveWorldDeltasDetail::Base64Value(Data[I + 2]);
		if (Value < 0)
		{
			return false;
		}
		Buffer = (Buffer << 6) | static_cast<uint32>(Value);
		BufferBits += 6;
		if (BufferBits >= 8)
		{
			BufferBits -= 8;
			Bytes.Add(static_cast<uint8>((Buffer >> BufferBits) & 0xFF));
		}
	}
	// Los bits sobrantes del último carácter deben ser cero (codificación canónica).
	if ((Buffer & ((1u << BufferBits) - 1u)) != 0)
	{
		return false;
	}
	if (Bytes.Num() > (MaxIndex + 1) / 8)
	{
		return false;
	}
	Words.SetNumZeroed((Bytes.Num() + 3) / 4);
	for (int32 I = 0; I < Bytes.Num(); ++I)
	{
		Words[I / 4] |= static_cast<uint32>(Bytes[I]) << ((I % 4) * 8);
	}
	Count = 0;
	for (const uint32 Word : Words)
	{
		Count += SaveWorldDeltasDetail::PopCount(Word);
	}
	TrimTrailingZeros();
	return true;
}

bool FSaveIndexSet::Decode(const FString& Text)
{
	Reset();
	bool bOk = false;
	if (Text.Len() >= 2 && Text[1] == TEXT(':'))
	{
		if (Text[0] == TEXT('r'))
		{
			bOk = DecodeRanges(Text);
		}
		else if (Text[0] == TEXT('b'))
		{
			bOk = DecodeBits(Text);
		}
	}
	if (!bOk)
	{
		Reset();
	}
	return bOk;
}

bool FSaveIndexSet::operator==(const FSaveIndexSet& Other) const
{
	return Count == Other.Count && Words == Other.Words;
}

// ---------------------------------------------------------------------------
// FSaveScatterDeltas
// ---------------------------------------------------------------------------

int32 FSaveScatterDeltas::FindCellIndex(const FIntPoint& Cell, bool& bOutFound) const
{
	int32 Lo = 0;
	int32 Hi = Cells.Num();
	while (Lo < Hi)
	{
		const int32 Mid = Lo + (Hi - Lo) / 2;
		if (SaveWorldDeltasDetail::CellLess(Cells[Mid].Cell, Cell))
		{
			Lo = Mid + 1;
		}
		else
		{
			Hi = Mid;
		}
	}
	bOutFound = Lo < Cells.Num() && Cells[Lo].Cell == Cell;
	return Lo;
}

bool FSaveScatterDeltas::Add(const FIntPoint& Cell, int32 Index)
{
	if (Index < 0 || Index > FSaveIndexSet::MaxIndex)
	{
		return false;
	}
	bool bFound = false;
	const int32 At = FindCellIndex(Cell, bFound);
	if (!bFound)
	{
		FSaveCellDeltas NewCell;
		NewCell.Cell = Cell;
		Cells.Insert(NewCell, At);
	}
	return Cells[At].Indices.Add(Index);
}

bool FSaveScatterDeltas::Remove(const FIntPoint& Cell, int32 Index)
{
	bool bFound = false;
	const int32 At = FindCellIndex(Cell, bFound);
	if (!bFound || !Cells[At].Indices.Remove(Index))
	{
		return false;
	}
	if (Cells[At].Indices.IsEmpty())
	{
		Cells.RemoveAt(At);
	}
	return true;
}

bool FSaveScatterDeltas::Contains(const FIntPoint& Cell, int32 Index) const
{
	const FSaveIndexSet* Set = FindCell(Cell);
	return Set && Set->Contains(Index);
}

const FSaveIndexSet* FSaveScatterDeltas::FindCell(const FIntPoint& Cell) const
{
	bool bFound = false;
	const int32 At = FindCellIndex(Cell, bFound);
	return bFound ? &Cells[At].Indices : nullptr;
}

int32 FSaveScatterDeltas::Num() const
{
	int32 Total = 0;
	for (const FSaveCellDeltas& Cell : Cells)
	{
		Total += Cell.Indices.Num();
	}
	return Total;
}

void FSaveScatterDeltas::Merge(const FSaveScatterDeltas& Other)
{
	for (const FSaveCellDeltas& OtherCell : Other.Cells)
	{
		if (OtherCell.Indices.IsEmpty())
		{
			continue;
		}
		bool bFound = false;
		const int32 At = FindCellIndex(OtherCell.Cell, bFound);
		if (bFound)
		{
			Cells[At].Indices.Union(OtherCell.Indices);
		}
		else
		{
			Cells.Insert(OtherCell, At);
		}
	}
}

void FSaveScatterDeltas::ForEach(TFunctionRef<void(const FIntPoint&, int32)> Visit) const
{
	for (const FSaveCellDeltas& Cell : Cells)
	{
		for (const int32 Index : Cell.Indices.ToArray())
		{
			Visit(Cell.Cell, Index);
		}
	}
}

FSaveValue FSaveScatterDeltas::ToValue() const
{
	FSaveValue Out = FSaveValue::MakeArray();
	for (const FSaveCellDeltas& Cell : Cells)
	{
		if (Cell.Indices.IsEmpty())
		{
			continue;
		}
		FSaveValue Entry = FSaveValue::MakeArray();
		Entry.Add(FSaveValue::MakeInt(Cell.Cell.X));
		Entry.Add(FSaveValue::MakeInt(Cell.Cell.Y));
		Entry.Add(FSaveValue::MakeString(Cell.Indices.Encode()));
		Out.Add(MoveTemp(Entry));
	}
	return Out;
}

bool FSaveScatterDeltas::FromValue(const FSaveValue& Value)
{
	Reset();
	if (!Value.IsArray())
	{
		return false;
	}
	FSaveScatterDeltas Result;
	for (int32 I = 0; I < Value.Num(); ++I)
	{
		const FSaveValue& Entry = Value.At(I);
		FIntPoint Cell;
		FSaveIndexSet Indices;
		if (!Entry.IsArray() || Entry.Num() != 3
			|| !TSaveTraits<int32>::FromValue(Entry.At(0), Cell.X)
			|| !TSaveTraits<int32>::FromValue(Entry.At(1), Cell.Y)
			|| !Entry.At(2).IsString()
			|| !Indices.Decode(Entry.At(2).AsString()))
		{
			return false;
		}
		if (Indices.IsEmpty())
		{
			continue;
		}
		// Una celda repetida (texto editado a mano) se une en vez de rechazar la partida.
		bool bFound = false;
		const int32 At = Result.FindCellIndex(Cell, bFound);
		if (bFound)
		{
			Result.Cells[At].Indices.Union(Indices);
		}
		else
		{
			FSaveCellDeltas NewCell;
			NewCell.Cell = Cell;
			NewCell.Indices = MoveTemp(Indices);
			Result.Cells.Insert(MoveTemp(NewCell), At);
		}
	}
	*this = MoveTemp(Result);
	return true;
}

bool FSaveScatterDeltas::operator==(const FSaveScatterDeltas& Other) const
{
	if (Cells.Num() != Other.Cells.Num())
	{
		return false;
	}
	for (int32 I = 0; I < Cells.Num(); ++I)
	{
		if (Cells[I].Cell != Other.Cells[I].Cell || Cells[I].Indices != Other.Cells[I].Indices)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// FSaveWorldDeltas
// ---------------------------------------------------------------------------

void FSaveWorldDeltas::Merge(const FSaveWorldDeltas& Other)
{
	for (const auto& Pair : Other.Layers)
	{
		Layers.FindOrAdd(Pair.Key).Merge(Pair.Value);
	}
}

void FSaveWorldDeltas::Save(FSaveArchive& Ar) const
{
	Ar.Write(TEXT("seed"), Seed);
	FSaveValue LayersValue = FSaveValue::MakeObject();
	for (const auto& Pair : Layers)
	{
		if (!Pair.Value.IsEmpty())
		{
			LayersValue.Set(Pair.Key.ToString(), Pair.Value.ToValue());
		}
	}
	Ar.SetValue(TEXT("layers"), MoveTemp(LayersValue));
}

void FSaveWorldDeltas::Load(const FSaveArchive& Ar)
{
	*this = FSaveWorldDeltas();
	Ar.Read(TEXT("seed"), Seed);
	const FSaveValue* LayersValue = Ar.FindValue(TEXT("layers"));
	if (!LayersValue || !LayersValue->IsObject())
	{
		return;
	}
	// Orden ordinal de las claves: el mapa resultante no depende del orden del texto.
	for (int32 I = 0; I < LayersValue->Num(); ++I)
	{
		FSaveScatterDeltas Deltas;
		if (Deltas.FromValue(LayersValue->GetValueAt(I)) && !Deltas.IsEmpty())
		{
			Layers.Add(FName(*LayersValue->GetKeys()[I]), MoveTemp(Deltas));
		}
	}
}

bool FSaveWorldDeltas::operator==(const FSaveWorldDeltas& Other) const
{
	if (Seed != Other.Seed)
	{
		return false;
	}
	// Las capas vacías equivalen a capas ausentes.
	auto Covers = [](const FSaveWorldDeltas& A, const FSaveWorldDeltas& B)
	{
		for (const auto& Pair : A.Layers)
		{
			const FSaveScatterDeltas* Match = B.Layers.Find(Pair.Key);
			if (Pair.Value.IsEmpty() ? (Match && !Match->IsEmpty()) : (!Match || !(*Match == Pair.Value)))
			{
				return false;
			}
		}
		return true;
	};
	return Covers(*this, Other) && Covers(Other, *this);
}

// ---------------------------------------------------------------------------
// FSavePlayerState
// ---------------------------------------------------------------------------

void FSavePlayerState::Save(FSaveArchive& Ar) const
{
	Ar.Write(TEXT("location"), Location);
	Ar.Write(TEXT("controlRotation"), ControlRotation);
	Ar.Write(TEXT("velocity"), Velocity);
	FSaveArchive Body;
	Body.Write(TEXT("health"), Health);
	Body.Write(TEXT("hunger"), Hunger);
	Body.Write(TEXT("thirst"), Thirst);
	Body.Write(TEXT("energy"), Energy);
	Body.Write(TEXT("rest"), Rest);
	Body.Write(TEXT("morale"), Morale);
	Body.Write(TEXT("bodyTemperature"), BodyTemperature);
	Body.Write(TEXT("wetness"), Wetness);
	Ar.Write(TEXT("body"), Body);
}

void FSavePlayerState::Load(const FSaveArchive& Ar)
{
	*this = FSavePlayerState();
	Ar.Read(TEXT("location"), Location);
	Ar.Read(TEXT("controlRotation"), ControlRotation);
	Ar.Read(TEXT("velocity"), Velocity);
	FSaveArchive Body;
	if (Ar.Read(TEXT("body"), Body))
	{
		Body.Read(TEXT("health"), Health);
		Body.Read(TEXT("hunger"), Hunger);
		Body.Read(TEXT("thirst"), Thirst);
		Body.Read(TEXT("energy"), Energy);
		Body.Read(TEXT("rest"), Rest);
		Body.Read(TEXT("morale"), Morale);
		Body.Read(TEXT("bodyTemperature"), BodyTemperature);
		Body.Read(TEXT("wetness"), Wetness);
	}
}

bool FSavePlayerState::operator==(const FSavePlayerState& Other) const
{
	return Location == Other.Location && ControlRotation == Other.ControlRotation && Velocity == Other.Velocity
		&& Health == Other.Health && Hunger == Other.Hunger && Thirst == Other.Thirst && Energy == Other.Energy
		&& Rest == Other.Rest && Morale == Other.Morale && BodyTemperature == Other.BodyTemperature
		&& Wetness == Other.Wetness;
}
