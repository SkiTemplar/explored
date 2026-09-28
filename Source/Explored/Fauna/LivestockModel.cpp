#include "Fauna/LivestockModel.h"

#include "Core/ExploredRandom.h"

namespace LivestockDetail
{
	constexpr int32 NumSpecies = static_cast<int32>(ELivestockSpecies::Count);
	/** Sales del hash: la cría, su sexo y el orden de los partos no comparten tirada. */
	constexpr uint32 BreedSalt = 0x6C1A7E55u;
	constexpr uint32 SexSalt = 0x2B9D04C3u;
	constexpr uint32 OrderSalt = 0x91E3F6A7u;
	/** Topes que admite la carga: acotan ficheros manipulados. */
	constexpr int64 MaxDay = MAX_int32;
	constexpr int64 MaxCounter = 10000000;
	constexpr int32 PenFields = 5;
	constexpr int32 AnimalFields = 11;

	bool ReadInt(const FSaveValue& List, int32 Index, int64 Min, int64 Max, int32& Out)
	{
		int64 V = 0;
		if (!List.At(Index).TryGetInt(V) || V < Min || V > Max)
		{
			return false;
		}
		Out = static_cast<int32>(V);
		return true;
	}

	bool ReadBool(const FSaveValue& List, int32 Index, bool& Out)
	{
		int32 V = 0;
		if (!ReadInt(List, Index, 0, 1, V))
		{
			return false;
		}
		Out = V != 0;
		return true;
	}

	uint8 Nibble(int32 V)
	{
		return static_cast<uint8>(FMath::Clamp(V, 0, 15));
	}

	uint8 Byte(int32 V)
	{
		return static_cast<uint8>(FMath::Clamp(V, 0, 255));
	}
}

const TCHAR* FLivestockModel::StatSpeciesRaised = TEXT("livestock_species_raised");
const TCHAR* FLivestockModel::StatEggsCollected = TEXT("eggs_collected");
const TCHAR* FLivestockModel::EggItem = TEXT("huevo");
const TCHAR* FLivestockModel::MilkItem = TEXT("leche_cabra");

const TCHAR* LexToString(ELivestockSpecies Species)
{
	switch (Species)
	{
	case ELivestockSpecies::Chicken: return TEXT("Chicken");
	case ELivestockSpecies::Pig: return TEXT("Pig");
	case ELivestockSpecies::Goat: return TEXT("Goat");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ELivestockPenKind Kind)
{
	switch (Kind)
	{
	case ELivestockPenKind::Coop: return TEXT("Coop");
	case ELivestockPenKind::Sty: return TEXT("Sty");
	case ELivestockPenKind::Pen: return TEXT("Pen");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ELivestockResult Result)
{
	switch (Result)
	{
	case ELivestockResult::Ok: return TEXT("Ok");
	case ELivestockResult::UnknownPen: return TEXT("UnknownPen");
	case ELivestockResult::UnknownAnimal: return TEXT("UnknownAnimal");
	case ELivestockResult::WrongPen: return TEXT("WrongPen");
	case ELivestockResult::PenFull: return TEXT("PenFull");
	case ELivestockResult::BaseFull: return TEXT("BaseFull");
	case ELivestockResult::PenNotEmpty: return TEXT("PenNotEmpty");
	case ELivestockResult::TroughFull: return TEXT("TroughFull");
	case ELivestockResult::InvalidArgument: return TEXT("InvalidArgument");
	default: return TEXT("Unknown");
	}
}

FLivestockSettings FLivestockSettings::Sanitized() const
{
	FLivestockSettings S = *this;
	S.MaxAlivePerBase = FMath::Clamp(S.MaxAlivePerBase, 0, 255);
	S.MaxAnimalsPerPen = FMath::Clamp(S.MaxAnimalsPerPen, 0, 255);
	S.BreedingChancePerDay = FMath::IsFinite(S.BreedingChancePerDay) ? FMath::Clamp(S.BreedingChancePerDay, 0.0f, 1.0f) : 0.0f;
	S.DaysToAdult = FMath::Clamp(S.DaysToAdult, 0, 3650);
	S.FeedPerDay = FMath::Clamp(S.FeedPerDay, 0, 1000);
	S.DaysFedToTame = FMath::Clamp(S.DaysFedToTame, 0, 3650);
	S.DaysUnfedToWild = FMath::Clamp(S.DaysUnfedToWild, 1, 3650);
	S.TroughCapacity = FMath::Clamp(S.TroughCapacity, 0, 100000);
	S.MaxStoredProducts = FMath::Clamp(S.MaxStoredProducts, 0, 255);
	S.MaxCatchUpDays = FMath::Clamp(S.MaxCatchUpDays, 1, 3650);
	return S;
}

bool FLivestockAnimal::operator==(const FLivestockAnimal& Other) const
{
	return Id == Other.Id && Species == Other.Species && Sex == Other.Sex && PenId == Other.PenId
		&& AgeDays == Other.AgeDays && bAdult == Other.bAdult && bTame == Other.bTame && TameDays == Other.TameDays
		&& UnfedDays == Other.UnfedDays && bFedToday == Other.bFedToday && LastBirthDay == Other.LastBirthDay;
}

bool FLivestockPen::operator==(const FLivestockPen& Other) const
{
	return Id == Other.Id && Kind == Other.Kind && Feed == Other.Feed && Eggs == Other.Eggs && Milk == Other.Milk;
}

void FLivestockPenNet::Pack(uint8 (&Out)[PackedBytes]) const
{
	using namespace LivestockDetail;
	// Tres bytes de medios bytes: (adultos, crías) de gallina, cerdo y cabra.
	for (int32 S = 0; S < NumSpecies; ++S)
	{
		Out[S] = static_cast<uint8>((Nibble(Adults[S]) << 4) | Nibble(Young[S]));
	}
	Out[3] = Feed;
	Out[4] = Eggs;
	Out[5] = Milk;
	Out[6] = bFull ? 1 : 0;
}

FLivestockPenNet FLivestockPenNet::Unpack(const uint8 (&In)[PackedBytes])
{
	FLivestockPenNet Net;
	for (int32 S = 0; S < LivestockDetail::NumSpecies; ++S)
	{
		Net.Adults[S] = static_cast<uint8>(In[S] >> 4);
		Net.Young[S] = static_cast<uint8>(In[S] & 0x0F);
	}
	Net.Feed = In[3];
	Net.Eggs = In[4];
	Net.Milk = In[5];
	Net.bFull = (In[6] & 1) != 0;
	return Net;
}

bool FLivestockPenNet::operator==(const FLivestockPenNet& Other) const
{
	for (int32 S = 0; S < LivestockDetail::NumSpecies; ++S)
	{
		if (Adults[S] != Other.Adults[S] || Young[S] != Other.Young[S])
		{
			return false;
		}
	}
	return Feed == Other.Feed && Eggs == Other.Eggs && Milk == Other.Milk && bFull == Other.bFull;
}

FLivestockModel::FLivestockModel(const FLivestockSettings& InSettings, uint32 InSeed)
	: Settings(InSettings.Sanitized())
	, Seed(InSeed)
{
}

void FLivestockModel::Reset()
{
	LastEndedDay = INDEX_NONE;
	NextPenId = 1;
	NextAnimalId = 1;
	Pens.Reset();
	Animals.Reset();
}

FName FLivestockModel::SpeciesId(ELivestockSpecies Species)
{
	switch (Species)
	{
	case ELivestockSpecies::Chicken: return FName(TEXT("gallina"));
	case ELivestockSpecies::Pig: return FName(TEXT("cerdo"));
	case ELivestockSpecies::Goat: return FName(TEXT("cabra"));
	default: return NAME_None;
	}
}

bool FLivestockModel::ParseSpecies(FName Id, ELivestockSpecies& Out)
{
	for (int32 S = 0; S < LivestockDetail::NumSpecies; ++S)
	{
		if (!Id.IsNone() && Id == SpeciesId(static_cast<ELivestockSpecies>(S)))
		{
			Out = static_cast<ELivestockSpecies>(S);
			return true;
		}
	}
	return false;
}

FName FLivestockModel::PenPieceId(ELivestockPenKind Kind)
{
	switch (Kind)
	{
	case ELivestockPenKind::Coop: return FName(TEXT("gallinero"));
	case ELivestockPenKind::Sty: return FName(TEXT("pocilga"));
	case ELivestockPenKind::Pen: return FName(TEXT("corral"));
	default: return NAME_None;
	}
}

bool FLivestockModel::ParsePenPiece(FName Id, ELivestockPenKind& Out)
{
	for (int32 K = 0; K < static_cast<int32>(ELivestockPenKind::Count); ++K)
	{
		if (!Id.IsNone() && Id == PenPieceId(static_cast<ELivestockPenKind>(K)))
		{
			Out = static_cast<ELivestockPenKind>(K);
			return true;
		}
	}
	return false;
}

bool FLivestockModel::PenAccepts(ELivestockPenKind Kind, ELivestockSpecies Species)
{
	if (Species >= ELivestockSpecies::Count)
	{
		return false;
	}
	switch (Kind)
	{
	case ELivestockPenKind::Coop: return Species == ELivestockSpecies::Chicken;
	case ELivestockPenKind::Sty: return Species == ELivestockSpecies::Pig;
	case ELivestockPenKind::Pen: return true;
	default: return false;
	}
}

int32 FLivestockModel::AddPen(ELivestockPenKind Kind)
{
	if (Kind >= ELivestockPenKind::Count || NextPenId == MAX_int32)
	{
		return INDEX_NONE;
	}
	FLivestockPen Pen;
	Pen.Id = NextPenId++;
	Pen.Kind = Kind;
	Pens.Add(Pen);
	return Pen.Id;
}

ELivestockResult FLivestockModel::RemovePen(int32 PenId)
{
	const int32 Index = Pens.IndexOfByPredicate([PenId](const FLivestockPen& P) { return P.Id == PenId; });
	if (Index == INDEX_NONE)
	{
		return ELivestockResult::UnknownPen;
	}
	if (NumAnimalsInPen(PenId) > 0)
	{
		return ELivestockResult::PenNotEmpty;
	}
	Pens.RemoveAt(Index);
	return ELivestockResult::Ok;
}

const FLivestockPen* FLivestockModel::FindPen(int32 PenId) const
{
	return Pens.FindByPredicate([PenId](const FLivestockPen& P) { return P.Id == PenId; });
}

FLivestockPen* FLivestockModel::FindPenMutable(int32 PenId)
{
	return Pens.FindByPredicate([PenId](const FLivestockPen& P) { return P.Id == PenId; });
}

const FLivestockAnimal* FLivestockModel::FindAnimal(int32 AnimalId) const
{
	return Animals.FindByPredicate([AnimalId](const FLivestockAnimal& A) { return A.Id == AnimalId; });
}

FLivestockAnimal* FLivestockModel::FindAnimalMutable(int32 AnimalId)
{
	return Animals.FindByPredicate([AnimalId](const FLivestockAnimal& A) { return A.Id == AnimalId; });
}

int32 FLivestockModel::NumAnimalsInPen(int32 PenId) const
{
	int32 N = 0;
	for (const FLivestockAnimal& A : Animals)
	{
		N += A.PenId == PenId ? 1 : 0;
	}
	return N;
}

bool FLivestockModel::IsPenFull(int32 PenId) const
{
	return FindPen(PenId) != nullptr && (NumAnimalsInPen(PenId) >= Settings.MaxAnimalsPerPen || IsBaseFull());
}

ELivestockResult FLivestockModel::CanAddAnimal(int32 PenId, ELivestockSpecies Species) const
{
	const FLivestockPen* Pen = FindPen(PenId);
	if (Pen == nullptr)
	{
		return ELivestockResult::UnknownPen;
	}
	if (Species >= ELivestockSpecies::Count)
	{
		return ELivestockResult::InvalidArgument;
	}
	if (!PenAccepts(Pen->Kind, Species))
	{
		return ELivestockResult::WrongPen;
	}
	if (IsBaseFull())
	{
		return ELivestockResult::BaseFull;
	}
	if (NumAnimalsInPen(PenId) >= Settings.MaxAnimalsPerPen)
	{
		return ELivestockResult::PenFull;
	}
	if (NextAnimalId == MAX_int32)
	{
		return ELivestockResult::InvalidArgument;
	}
	return ELivestockResult::Ok;
}

ELivestockResult FLivestockModel::AddAnimal(int32 PenId, ELivestockSpecies Species, ELivestockSex Sex,
	ELivestockOrigin Origin, int32& OutId)
{
	OutId = INDEX_NONE;
	if ((Sex != ELivestockSex::Female && Sex != ELivestockSex::Male) || Origin == ELivestockOrigin::Born
		|| (Origin != ELivestockOrigin::Captured && Origin != ELivestockOrigin::Traded))
	{
		return ELivestockResult::InvalidArgument;
	}
	const ELivestockResult Check = CanAddAnimal(PenId, Species);
	if (Check != ELivestockResult::Ok)
	{
		return Check;
	}
	FLivestockAnimal A;
	A.Id = NextAnimalId++;
	A.Species = Species;
	A.Sex = Sex;
	A.PenId = PenId;
	A.AgeDays = Settings.DaysToAdult;
	A.bAdult = true;
	// Sin días de doma, el animal capturado ya entra dócil.
	A.bTame = Origin == ELivestockOrigin::Traded || Settings.DaysFedToTame == 0;
	Animals.Add(A);
	OutId = A.Id;
	return ELivestockResult::Ok;
}

ELivestockResult FLivestockModel::RemoveAnimal(int32 AnimalId)
{
	const int32 Removed = Animals.RemoveAll([AnimalId](const FLivestockAnimal& A) { return A.Id == AnimalId; });
	return Removed > 0 ? ELivestockResult::Ok : ELivestockResult::UnknownAnimal;
}

ELivestockResult FLivestockModel::MoveAnimal(int32 AnimalId, int32 ToPenId)
{
	FLivestockAnimal* A = FindAnimalMutable(AnimalId);
	if (A == nullptr)
	{
		return ELivestockResult::UnknownAnimal;
	}
	const FLivestockPen* To = FindPen(ToPenId);
	if (To == nullptr)
	{
		return ELivestockResult::UnknownPen;
	}
	if (A->PenId == ToPenId)
	{
		return ELivestockResult::Ok;
	}
	if (!PenAccepts(To->Kind, A->Species))
	{
		return ELivestockResult::WrongPen;
	}
	if (NumAnimalsInPen(ToPenId) >= Settings.MaxAnimalsPerPen)
	{
		return ELivestockResult::PenFull;
	}
	A->PenId = ToPenId;
	return ELivestockResult::Ok;
}

ELivestockResult FLivestockModel::AddFeed(int32 PenId, int32 Count, int32& OutAccepted)
{
	OutAccepted = 0;
	FLivestockPen* Pen = FindPenMutable(PenId);
	if (Pen == nullptr)
	{
		return ELivestockResult::UnknownPen;
	}
	if (Count <= 0)
	{
		return ELivestockResult::InvalidArgument;
	}
	const int32 Space = FMath::Max(0, Settings.TroughCapacity - Pen->Feed);
	if (Space == 0)
	{
		return ELivestockResult::TroughFull;
	}
	OutAccepted = FMath::Min(Count, Space);
	Pen->Feed += OutAccepted;
	return ELivestockResult::Ok;
}

ELivestockResult FLivestockModel::Collect(int32 PenId, FLivestockCollect& Out)
{
	Out = FLivestockCollect();
	FLivestockPen* Pen = FindPenMutable(PenId);
	if (Pen == nullptr)
	{
		return ELivestockResult::UnknownPen;
	}
	Out.Eggs = Pen->Eggs;
	Out.Milk = Pen->Milk;
	Pen->Eggs = 0;
	Pen->Milk = 0;
	return ELivestockResult::Ok;
}

ELivestockResult FLivestockModel::OpenGate(int32 PenId, TArray<int32>& OutEscaped)
{
	OutEscaped.Reset();
	if (FindPen(PenId) == nullptr)
	{
		return ELivestockResult::UnknownPen;
	}
	for (const FLivestockAnimal& A : Animals)
	{
		if (A.PenId == PenId && !A.bTame)
		{
			OutEscaped.Add(A.Id);
		}
	}
	Animals.RemoveAll([PenId](const FLivestockAnimal& A) { return A.PenId == PenId && !A.bTame; });
	return ELivestockResult::Ok;
}

bool FLivestockModel::BreedingRoll(int32 PenId, int32 Day, int32 PairIndex) const
{
	const uint32 H = ExploredHash::Hash3D(Seed ^ LivestockDetail::BreedSalt, PenId, Day, PairIndex);
	return ExploredHash::ToUnitFloat(H) < Settings.BreedingChancePerDay;
}

ELivestockSex FLivestockModel::OffspringSex(int32 PenId, int32 Day, int32 PairIndex) const
{
	const uint32 H = ExploredHash::Hash3D(Seed ^ LivestockDetail::SexSalt, PenId, Day, PairIndex);
	return (H & 1u) != 0 ? ELivestockSex::Male : ELivestockSex::Female;
}

FLivestockDayReport FLivestockModel::EndDay(int32 Day)
{
	FLivestockDayReport Report;
	if (Day < 0 || Day <= LastEndedDay)
	{
		return Report;
	}
	int32 First = Day;
	if (LastEndedDay != INDEX_NONE)
	{
		First = FMath::Max(LastEndedDay + 1, Day - Settings.MaxCatchUpDays + 1);
	}
	// Se cuenta en int64: con Day = MAX_int32 un «++D» de int32 desbordaría.
	for (int64 D = First; D <= Day; ++D)
	{
		CloseOneDay(static_cast<int32>(D), Report);
	}
	LastEndedDay = Day;
	return Report;
}

void FLivestockModel::CloseOneDay(int32 Day, FLivestockDayReport& Report)
{
	using namespace LivestockDetail;
	++Report.DaysClosed;

	// 1. Las crías crecen con el calendario, coman o no (biblia 08 §5.6: sin escalado).
	for (FLivestockAnimal& A : Animals)
	{
		A.AgeDays = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(A.AgeDays) + 1, MaxCounter));
		if (!A.bAdult && A.AgeDays >= Settings.DaysToAdult)
		{
			A.bAdult = true;
			Report.GrewUp.Add(A.Id);
		}
	}

	// 2. Comida y doma, por orden de id (los animales están siempre ordenados por id).
	for (FLivestockAnimal& A : Animals)
	{
		FLivestockPen* Pen = FindPenMutable(A.PenId);
		A.bFedToday = Pen != nullptr && Pen->Feed >= Settings.FeedPerDay;
		if (A.bFedToday)
		{
			Pen->Feed -= Settings.FeedPerDay;
			A.UnfedDays = 0;
			if (!A.bTame && ++A.TameDays >= Settings.DaysFedToTame)
			{
				A.bTame = true;
				A.TameDays = 0;
				Report.Tamed.Add(A.Id);
			}
		}
		else
		{
			A.UnfedDays = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(A.UnfedDays) + 1, MaxCounter));
			A.TameDays = 0;
			if (A.bTame && A.UnfedDays >= Settings.DaysUnfedToWild)
			{
				A.bTame = false;
				Report.WentWild.Add(A.Id);
			}
		}
	}

	// 3. Huevos y leche de los adultos que han comido.
	for (const FLivestockAnimal& A : Animals)
	{
		FLivestockPen* Pen = FindPenMutable(A.PenId);
		if (Pen == nullptr || !A.bAdult || !A.bFedToday || A.Sex != ELivestockSex::Female)
		{
			continue;
		}
		if (A.Species == ELivestockSpecies::Chicken && Pen->Eggs < Settings.MaxStoredProducts)
		{
			++Pen->Eggs;
			++Report.EggsLaid;
		}
		const bool bRecentKid = A.LastBirthDay != INDEX_NONE && Day - A.LastBirthDay >= 1
			&& Day - A.LastBirthDay <= Settings.DaysToAdult;
		if (A.Species == ELivestockSpecies::Goat && bRecentKid && Pen->Milk < Settings.MaxStoredProducts)
		{
			++Pen->Milk;
			++Report.MilkGiven;
		}
	}

	// 4. Cría: parejas de macho y hembra adultos, domados y comidos, por corral y especie.
	// Primero se tiran todas las parejas y luego los partos se ordenan por un hash del día:
	// si solo queda sitio para una cría, no gana siempre el corral o la especie con id menor.
	struct FPendingBirth
	{
		uint32 Order = 0;
		int32 PenId = INDEX_NONE;
		int32 PairKey = 0;
		int32 Species = 0;
		int32 MotherId = INDEX_NONE;
	};
	TArray<FPendingBirth> Pending;
	for (const FLivestockPen& Pen : Pens)
	{
		for (int32 S = 0; S < NumSpecies; ++S)
		{
			TArray<int32> Males;
			TArray<int32> Females;
			for (const FLivestockAnimal& A : Animals)
			{
				if (A.PenId == Pen.Id && static_cast<int32>(A.Species) == S && A.bAdult && A.bTame && A.bFedToday)
				{
					(A.Sex == ELivestockSex::Male ? Males : Females).Add(A.Id);
				}
			}
			const int32 Pairs = FMath::Min(Males.Num(), Females.Num());
			for (int32 P = 0; P < Pairs; ++P)
			{
				// Clave de la pareja: especie y orden dentro del corral, estable entre máquinas.
				const int32 PairKey = S * 256 + P;
				if (BreedingRoll(Pen.Id, Day, PairKey))
				{
					FPendingBirth Birth;
					Birth.Order = ExploredHash::Hash3D(Seed ^ OrderSalt, Pen.Id, Day, PairKey);
					Birth.PenId = Pen.Id;
					Birth.PairKey = PairKey;
					Birth.Species = S;
					Birth.MotherId = Females[P];
					Pending.Add(Birth);
				}
			}
		}
	}
	Pending.Sort([](const FPendingBirth& A, const FPendingBirth& B)
	{
		return A.Order != B.Order ? A.Order < B.Order : (A.PenId != B.PenId ? A.PenId < B.PenId : A.PairKey < B.PairKey);
	});
	// Se trabaja con ids y no con punteros porque cada cría amplía el array.
	for (const FPendingBirth& P : Pending)
	{
		const ELivestockSpecies Species = static_cast<ELivestockSpecies>(P.Species);
		if (CanAddAnimal(P.PenId, Species) != ELivestockResult::Ok)
		{
			continue;
		}
		FLivestockAnimal Kid;
		Kid.Id = NextAnimalId++;
		Kid.Species = Species;
		Kid.Sex = OffspringSex(P.PenId, Day, P.PairKey);
		Kid.PenId = P.PenId;
		Kid.AgeDays = 0;
		Kid.bAdult = Settings.DaysToAdult == 0;
		Kid.bTame = true;
		if (FLivestockAnimal* Mother = FindAnimalMutable(P.MotherId))
		{
			Mother->LastBirthDay = Day;
		}
		Animals.Add(Kid);

		FLivestockBirth Birth;
		Birth.AnimalId = Kid.Id;
		Birth.MotherId = P.MotherId;
		Birth.PenId = P.PenId;
		Birth.Species = Species;
		Birth.Day = Day;
		Report.Births.Add(Birth);
	}
}

FLivestockAnimalNet FLivestockModel::MakeAnimalNet(const FLivestockAnimal& Animal)
{
	FLivestockAnimalNet Net;
	Net.Species = static_cast<uint8>(Animal.Species);
	Net.Flags = static_cast<uint8>((Animal.Sex == ELivestockSex::Male ? FLivestockAnimalNet::FlagMale : 0)
		| (Animal.bAdult ? FLivestockAnimalNet::FlagAdult : 0)
		| (Animal.bTame ? FLivestockAnimalNet::FlagTame : 0)
		| (Animal.bFedToday ? FLivestockAnimalNet::FlagFed : 0));
	return Net;
}

FLivestockPenNet FLivestockModel::MakePenNet(int32 PenId) const
{
	using namespace LivestockDetail;
	FLivestockPenNet Net;
	const FLivestockPen* Pen = FindPen(PenId);
	if (Pen == nullptr)
	{
		return Net;
	}
	int32 Adults[NumSpecies] = {};
	int32 Young[NumSpecies] = {};
	for (const FLivestockAnimal& A : Animals)
	{
		if (A.PenId == PenId)
		{
			++(A.bAdult ? Adults : Young)[static_cast<int32>(A.Species)];
		}
	}
	for (int32 S = 0; S < NumSpecies; ++S)
	{
		Net.Adults[S] = Nibble(Adults[S]);
		Net.Young[S] = Nibble(Young[S]);
	}
	Net.Feed = Byte(Pen->Feed);
	Net.Eggs = Byte(Pen->Eggs);
	Net.Milk = Byte(Pen->Milk);
	Net.bFull = IsPenFull(PenId);
	return Net;
}

FSaveValue FLivestockModel::ToValue() const
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("v"), FSaveValue::MakeInt(1));
	Root.Set(TEXT("day"), FSaveValue::MakeInt(LastEndedDay));
	Root.Set(TEXT("nextPen"), FSaveValue::MakeInt(NextPenId));
	Root.Set(TEXT("nextAnimal"), FSaveValue::MakeInt(NextAnimalId));
	FSaveValue PenList = FSaveValue::MakeArray();
	for (const FLivestockPen& P : Pens)
	{
		PenList.Add(FSaveValue::MakeInt(P.Id));
		PenList.Add(FSaveValue::MakeInt(static_cast<int32>(P.Kind)));
		PenList.Add(FSaveValue::MakeInt(P.Feed));
		PenList.Add(FSaveValue::MakeInt(P.Eggs));
		PenList.Add(FSaveValue::MakeInt(P.Milk));
	}
	FSaveValue AnimalList = FSaveValue::MakeArray();
	for (const FLivestockAnimal& A : Animals)
	{
		AnimalList.Add(FSaveValue::MakeInt(A.Id));
		AnimalList.Add(FSaveValue::MakeInt(static_cast<int32>(A.Species)));
		AnimalList.Add(FSaveValue::MakeInt(static_cast<int32>(A.Sex)));
		AnimalList.Add(FSaveValue::MakeInt(A.PenId));
		AnimalList.Add(FSaveValue::MakeInt(A.AgeDays));
		AnimalList.Add(FSaveValue::MakeInt(A.bAdult ? 1 : 0));
		AnimalList.Add(FSaveValue::MakeInt(A.bTame ? 1 : 0));
		AnimalList.Add(FSaveValue::MakeInt(A.TameDays));
		AnimalList.Add(FSaveValue::MakeInt(A.UnfedDays));
		AnimalList.Add(FSaveValue::MakeInt(A.bFedToday ? 1 : 0));
		AnimalList.Add(FSaveValue::MakeInt(A.LastBirthDay));
	}
	Root.Set(TEXT("pens"), MoveTemp(PenList));
	Root.Set(TEXT("animals"), MoveTemp(AnimalList));
	return Root;
}

bool FLivestockModel::FromValue(const FSaveValue& Value)
{
	using namespace LivestockDetail;
	Reset();
	const auto Fail = [this]()
	{
		Reset();
		return false;
	};
	const FSaveValue* Version = Value.Find(TEXT("v"));
	const FSaveValue* DayValue = Value.Find(TEXT("day"));
	const FSaveValue* NextPen = Value.Find(TEXT("nextPen"));
	const FSaveValue* NextAnimal = Value.Find(TEXT("nextAnimal"));
	const FSaveValue* PenList = Value.Find(TEXT("pens"));
	const FSaveValue* AnimalList = Value.Find(TEXT("animals"));
	if (!Value.IsObject() || !Version || Version->AsInt(-1) != 1 || !DayValue || !NextPen || !NextAnimal
		|| !PenList || !PenList->IsArray() || !AnimalList || !AnimalList->IsArray()
		|| PenList->Num() % PenFields != 0 || AnimalList->Num() % AnimalFields != 0)
	{
		return Fail();
	}
	int64 Day = 0;
	int64 NextP = 0;
	int64 NextA = 0;
	if (!DayValue->TryGetInt(Day) || Day < INDEX_NONE || Day > MaxDay
		|| !NextPen->TryGetInt(NextP) || NextP < 1 || NextP > MAX_int32
		|| !NextAnimal->TryGetInt(NextA) || NextA < 1 || NextA > MAX_int32)
	{
		return Fail();
	}
	TArray<FLivestockPen> LoadedPens;
	for (int32 I = 0; I < PenList->Num(); I += PenFields)
	{
		FLivestockPen P;
		int32 Kind = 0;
		if (!ReadInt(*PenList, I, 1, NextP - 1, P.Id)
			|| !ReadInt(*PenList, I + 1, 0, static_cast<int32>(ELivestockPenKind::Count) - 1, Kind)
			|| !ReadInt(*PenList, I + 2, 0, Settings.TroughCapacity, P.Feed)
			|| !ReadInt(*PenList, I + 3, 0, Settings.MaxStoredProducts, P.Eggs)
			|| !ReadInt(*PenList, I + 4, 0, Settings.MaxStoredProducts, P.Milk)
			|| (LoadedPens.Num() > 0 && P.Id <= LoadedPens.Last().Id))
		{
			return Fail();
		}
		P.Kind = static_cast<ELivestockPenKind>(Kind);
		LoadedPens.Add(P);
	}
	TArray<FLivestockAnimal> LoadedAnimals;
	for (int32 I = 0; I < AnimalList->Num(); I += AnimalFields)
	{
		FLivestockAnimal A;
		int32 Species = 0;
		int32 Sex = 0;
		if (!ReadInt(*AnimalList, I, 1, NextA - 1, A.Id)
			|| !ReadInt(*AnimalList, I + 1, 0, NumSpecies - 1, Species)
			|| !ReadInt(*AnimalList, I + 2, 0, 1, Sex)
			|| !ReadInt(*AnimalList, I + 3, 1, NextP - 1, A.PenId)
			|| !ReadInt(*AnimalList, I + 4, 0, MaxCounter, A.AgeDays)
			|| !ReadBool(*AnimalList, I + 5, A.bAdult)
			|| !ReadBool(*AnimalList, I + 6, A.bTame)
			|| !ReadInt(*AnimalList, I + 7, 0, FMath::Max(0, Settings.DaysFedToTame - 1), A.TameDays)
			|| !ReadInt(*AnimalList, I + 8, 0, MaxCounter, A.UnfedDays)
			|| !ReadBool(*AnimalList, I + 9, A.bFedToday)
			|| !ReadInt(*AnimalList, I + 10, INDEX_NONE, Day, A.LastBirthDay)
			|| (LoadedAnimals.Num() > 0 && A.Id <= LoadedAnimals.Last().Id))
		{
			return Fail();
		}
		A.Species = static_cast<ELivestockSpecies>(Species);
		A.Sex = static_cast<ELivestockSex>(Sex);
		const FLivestockPen* Pen = LoadedPens.FindByPredicate([&A](const FLivestockPen& P) { return P.Id == A.PenId; });
		// Una cría no puede tener la edad de un adulto ni un domado días de doma a medias.
		if (Pen == nullptr || !PenAccepts(Pen->Kind, A.Species) || (!A.bAdult && A.AgeDays >= Settings.DaysToAdult)
			|| (A.bTame && A.TameDays != 0))
		{
			return Fail();
		}
		LoadedAnimals.Add(A);
	}
	Pens = MoveTemp(LoadedPens);
	Animals = MoveTemp(LoadedAnimals);
	if (Animals.Num() > Settings.MaxAlivePerBase)
	{
		return Fail();
	}
	for (const FLivestockPen& P : Pens)
	{
		if (NumAnimalsInPen(P.Id) > Settings.MaxAnimalsPerPen)
		{
			return Fail();
		}
	}
	LastEndedDay = static_cast<int32>(Day);
	NextPenId = static_cast<int32>(NextP);
	NextAnimalId = static_cast<int32>(NextA);
	return true;
}

bool FLivestockModel::operator==(const FLivestockModel& Other) const
{
	return Seed == Other.Seed && LastEndedDay == Other.LastEndedDay && NextPenId == Other.NextPenId
		&& NextAnimalId == Other.NextAnimalId && Pens == Other.Pens && Animals == Other.Animals;
}
