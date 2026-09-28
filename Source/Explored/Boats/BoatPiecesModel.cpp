#include "Boats/BoatPiecesModel.h"

#include "Boats/BoatModel.h"

const TCHAR* LexToString(EBoatPieceType Type)
{
	switch (Type)
	{
	case EBoatPieceType::Keel: return TEXT("Keel");
	case EBoatPieceType::Frame: return TEXT("Frame");
	case EBoatPieceType::HullPlank: return TEXT("HullPlank");
	case EBoatPieceType::Deck: return TEXT("Deck");
	case EBoatPieceType::Mast: return TEXT("Mast");
	case EBoatPieceType::Sail: return TEXT("Sail");
	case EBoatPieceType::Outrigger: return TEXT("Outrigger");
	case EBoatPieceType::Rudder: return TEXT("Rudder");
	case EBoatPieceType::RowingBench: return TEXT("RowingBench");
	case EBoatPieceType::Mooring: return TEXT("Mooring");
	default: return TEXT("Unknown");
	}
}

bool LexFromString(EBoatPieceType& OutType, const TCHAR* Name)
{
	if (Name == nullptr)
	{
		return false;
	}
	for (int32 I = 0; I < static_cast<int32>(EBoatPieceType::Count); ++I)
	{
		const EBoatPieceType Type = static_cast<EBoatPieceType>(I);
		if (FCString::Strcmp(Name, LexToString(Type)) == 0)
		{
			OutType = Type;
			return true;
		}
	}
	return false;
}

const TCHAR* LexToString(EBoatHullIssue Issue)
{
	switch (Issue)
	{
	case EBoatHullIssue::None: return TEXT("None");
	case EBoatHullIssue::NoKeel: return TEXT("NoKeel");
	case EBoatHullIssue::NoFrames: return TEXT("NoFrames");
	case EBoatHullIssue::NoPlanks: return TEXT("NoPlanks");
	case EBoatHullIssue::FrameUnattached: return TEXT("FrameUnattached");
	case EBoatHullIssue::PlankUnsupported: return TEXT("PlankUnsupported");
	case EBoatHullIssue::MastUnstepped: return TEXT("MastUnstepped");
	case EBoatHullIssue::SailWithoutMast: return TEXT("SailWithoutMast");
	case EBoatHullIssue::OutriggerUnattached: return TEXT("OutriggerUnattached");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(EBoatHullVerdict Verdict)
{
	switch (Verdict)
	{
	case EBoatHullVerdict::Floats: return TEXT("Floats");
	case EBoatHullVerdict::TakingWater: return TEXT("TakingWater");
	case EBoatHullVerdict::Capsizes: return TEXT("Capsizes");
	case EBoatHullVerdict::Sinks: return TEXT("Sinks");
	case EBoatHullVerdict::Incomplete: return TEXT("Incomplete");
	default: return TEXT("Unknown");
	}
}

namespace BoatPiecesDetail
{
	using T = EBoatPieceType;

	/** Catálogo (biblia 02 §8.1). Ids de Content/Data/boat_pieces.json. */
	struct FSpecTable
	{
		FBoatPieceSpec Entries[static_cast<int32>(EBoatPieceType::Count)];

		FSpecTable()
		{
			auto Set = [this](T Type, const TCHAR* Id, const FVector& Size, float Mass, float Liters, float Cargo, float Sail, int32 Integrity)
			{
				FBoatPieceSpec& S = Entries[static_cast<int32>(Type)];
				S.Type = Type;
				S.Id = Id;
				S.DefaultSizeCm = Size;
				S.MassKg = Mass;
				S.BuoyancyLiters = Liters;
				S.CargoKg = Cargo;
				S.SailAreaM2 = Sail;
				S.BaseIntegrity = Integrity;
			};
			// Quilla de madera dura: 5 kg por metro.
			Set(T::Keel, TEXT("quilla"), FVector(200.0, 12.0, 12.0), 10.0f, 0.0f, 0.0f, 0.0f, 100);
			// Cuaderna de 70 × 50 cm: su tamaño da la manga y el puntal del casco.
			Set(T::Frame, TEXT("cuaderna"), FVector(6.0, 70.0, 50.0), 5.0f, 0.0f, 0.0f, 0.0f, 80);
			// Tablón de 2 m: 35 L de hueco por metro de casco. La canoa de 4 m con cuatro tablones
			// encierra 560 L: cuatro tripulantes y su carga máxima pasan del 95 % (biblia 08 §2.5).
			Set(T::HullPlank, TEXT("tablon_casco"), FVector(200.0, 3.0, 20.0), 5.0f, 70.0f, 0.0f, 0.0f, 70);
			// Cubierta: 60 kg de carga por metro.
			Set(T::Deck, TEXT("cubierta"), FVector(100.0, 60.0, 3.0), 8.0f, 0.0f, 60.0f, 0.0f, 70);
			Set(T::Mast, TEXT("mastil"), FVector(8.0, 8.0, 250.0), 12.0f, 0.0f, 0.0f, 0.0f, 90);
			// Vela de hoja de palma trenzada de 2 × 2,25 m.
			Set(T::Sail, TEXT("vela"), FVector(1.0, 200.0, 225.0), 6.0f, 0.0f, 0.0f, 4.5f, 50);
			// Flotador de bambú grueso sellado: 90 L en 3 m.
			Set(T::Outrigger, TEXT("balancin"), FVector(300.0, 20.0, 20.0), 18.0f, 90.0f, 0.0f, 0.0f, 70);
			Set(T::Rudder, TEXT("timon"), FVector(120.0, 4.0, 30.0), 4.0f, 0.0f, 0.0f, 0.0f, 80);
			Set(T::RowingBench, TEXT("banco_remo"), FVector(6.0, 60.0, 3.0), 3.0f, 0.0f, 0.0f, 0.0f, 80);
			Set(T::Mooring, TEXT("amarre"), FVector(10.0, 10.0, 10.0), 1.0f, 0.0f, 0.0f, 0.0f, 100);
		}
	};

	/** Una fila de un plano canónico: tipo, centro y tamaño (cm, marco del casco, quilla en Z = 0). */
	struct FBlueprintRow
	{
		EBoatPieceType Type;
		double X, Y, Z;
		double SX, SY, SZ;
	};

	// Planos canónicos (biblia 02 §8.4). Una fila por pieza: Tools/DataCheck cuenta las
	// filas de cada tabla y las compara con «pieces» de Content/Data/boats.json.

	// Balsa: siete troncos de 2,2 m (la quilla y seis de piel) sobre dos travesaños.
	const FBlueprintRow RaftRows[] = {
		{T::Keel, 0.0, 0.0, 10.0, 220.0, 23.0, 20.0},
		{T::HullPlank, 0.0, -69.0, 10.0, 220.0, 23.0, 20.0},
		{T::HullPlank, 0.0, -46.0, 10.0, 220.0, 23.0, 20.0},
		{T::HullPlank, 0.0, -23.0, 10.0, 220.0, 23.0, 20.0},
		{T::HullPlank, 0.0, 23.0, 10.0, 220.0, 23.0, 20.0},
		{T::HullPlank, 0.0, 46.0, 10.0, 220.0, 23.0, 20.0},
		{T::HullPlank, 0.0, 69.0, 10.0, 220.0, 23.0, 20.0},
		{T::Frame, -80.0, 0.0, 11.0, 10.0, 160.0, 22.0},
		{T::Frame, 80.0, 0.0, 11.0, 10.0, 160.0, 22.0},
		{T::Deck, 0.0, 0.0, 21.5, 200.0, 150.0, 3.0},
		{T::RowingBench, -70.0, 0.0, 24.0, 6.0, 150.0, 3.0},
		{T::Mooring, 105.0, 0.0, 22.0, 10.0, 10.0, 10.0},
	};

	// Canoa: quilla de 4 m, tres cuadernas de 80 × 50 cm y dos tablones por banda.
	const FBlueprintRow CanoeRows[] = {
		{T::Keel, 0.0, 0.0, 6.0, 400.0, 12.0, 12.0},
		{T::Frame, -120.0, 0.0, 25.0, 6.0, 80.0, 50.0},
		{T::Frame, 0.0, 0.0, 25.0, 6.0, 80.0, 50.0},
		{T::Frame, 120.0, 0.0, 25.0, 6.0, 80.0, 50.0},
		{T::HullPlank, 0.0, -38.5, 14.0, 400.0, 3.0, 20.0},
		{T::HullPlank, 0.0, -38.5, 38.0, 400.0, 3.0, 20.0},
		{T::HullPlank, 0.0, 38.5, 14.0, 400.0, 3.0, 20.0},
		{T::HullPlank, 0.0, 38.5, 38.0, 400.0, 3.0, 20.0},
		{T::Deck, 0.0, 0.0, 13.0, 250.0, 50.0, 3.0},
		{T::RowingBench, -100.0, 0.0, 30.0, 6.0, 60.0, 3.0},
		{T::RowingBench, 100.0, 0.0, 30.0, 6.0, 60.0, 3.0},
		{T::Mooring, 195.0, 0.0, 45.0, 10.0, 10.0, 10.0},
	};

	// Canoa con balancín y vela: la canoa, dos botalones hasta un flotador a 1,2 m por
	// estribor, una plataforma sobre ellos, mástil de 2,5 m, vela de 4,5 m² y espadilla.
	const FBlueprintRow OutriggerRows[] = {
		{T::Keel, 0.0, 0.0, 6.0, 400.0, 12.0, 12.0},
		{T::Frame, -120.0, 0.0, 25.0, 6.0, 80.0, 50.0},
		{T::Frame, 0.0, 0.0, 25.0, 6.0, 80.0, 50.0},
		{T::Frame, 120.0, 0.0, 25.0, 6.0, 80.0, 50.0},
		{T::HullPlank, 0.0, -38.5, 14.0, 400.0, 3.0, 20.0},
		{T::HullPlank, 0.0, -38.5, 38.0, 400.0, 3.0, 20.0},
		{T::HullPlank, 0.0, 38.5, 14.0, 400.0, 3.0, 20.0},
		{T::HullPlank, 0.0, 38.5, 38.0, 400.0, 3.0, 20.0},
		{T::Deck, 0.0, 0.0, 13.0, 250.0, 50.0, 3.0},
		{T::RowingBench, -100.0, 0.0, 30.0, 6.0, 60.0, 3.0},
		{T::RowingBench, 100.0, 0.0, 30.0, 6.0, 60.0, 3.0},
		{T::Mooring, 195.0, 0.0, 45.0, 10.0, 10.0, 10.0},
		{T::Frame, -80.0, 50.0, 45.0, 6.0, 160.0, 10.0},
		{T::Frame, 80.0, 50.0, 45.0, 6.0, 160.0, 10.0},
		{T::Outrigger, 0.0, 120.0, 45.0, 300.0, 20.0, 20.0},
		{T::Deck, 0.0, 70.0, 51.5, 170.0, 60.0, 3.0},
		{T::Mast, 0.0, 0.0, 125.0, 8.0, 8.0, 250.0},
		{T::Sail, 5.0, 0.0, 140.0, 1.0, 200.0, 225.0},
		{T::Rudder, -210.0, 0.0, 20.0, 120.0, 4.0, 30.0},
	};

	// «Limón»: canoa doble de 6,5 m con cinco travesaños de 2,8 m, dos cubiertas,
	// mástil de 3,5 m sobre el travesaño central, vela de 12 m² y dos espadillas.
	const FBlueprintRow LimonRows[] = {
		{T::Keel, 0.0, -105.0, 7.0, 650.0, 14.0, 14.0},
		{T::Keel, 0.0, 105.0, 7.0, 650.0, 14.0, 14.0},
		{T::Frame, -260.0, 0.0, 35.0, 8.0, 280.0, 70.0},
		{T::Frame, -130.0, 0.0, 35.0, 8.0, 280.0, 70.0},
		{T::Frame, 0.0, 0.0, 35.0, 8.0, 280.0, 70.0},
		{T::Frame, 130.0, 0.0, 35.0, 8.0, 280.0, 70.0},
		{T::Frame, 260.0, 0.0, 35.0, 8.0, 280.0, 70.0},
		{T::HullPlank, 0.0, -124.5, 12.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, -124.5, 35.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, -124.5, 58.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, -85.5, 12.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, -85.5, 35.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, 85.5, 12.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, 85.5, 35.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, 124.5, 12.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, 124.5, 35.0, 650.0, 3.0, 22.0},
		{T::HullPlank, 0.0, 124.5, 58.0, 650.0, 3.0, 22.0},
		{T::Deck, 0.0, -60.0, 71.5, 420.0, 100.0, 3.0},
		{T::Deck, 0.0, 60.0, 71.5, 420.0, 100.0, 3.0},
		{T::Mast, 0.0, 0.0, 245.0, 10.0, 10.0, 350.0},
		{T::Sail, 6.0, 0.0, 240.0, 1.0, 300.0, 400.0},
		{T::Rudder, -330.0, -105.0, 20.0, 120.0, 4.0, 30.0},
		{T::Rudder, -330.0, 105.0, 20.0, 120.0, 4.0, 30.0},
		{T::RowingBench, -150.0, 0.0, 74.0, 6.0, 80.0, 3.0},
		{T::RowingBench, 150.0, 0.0, 74.0, 6.0, 80.0, 3.0},
		{T::Mooring, 330.0, 0.0, 70.0, 10.0, 10.0, 10.0},
	};

	/** Filas de un plano (vacío para un tipo que no existe). */
	struct FBlueprintRows
	{
		const FBlueprintRow* Rows = nullptr;
		int32 Num = 0;

		const FBlueprintRow* begin() const { return Rows; }
		const FBlueprintRow* end() const { return Rows + Num; }
	};

	FBlueprintRows RowsOf(EBoatType Type)
	{
		switch (Type)
		{
		case EBoatType::Raft: return {RaftRows, static_cast<int32>(UE_ARRAY_COUNT(RaftRows))};
		case EBoatType::Canoe: return {CanoeRows, static_cast<int32>(UE_ARRAY_COUNT(CanoeRows))};
		case EBoatType::Outrigger: return {OutriggerRows, static_cast<int32>(UE_ARRAY_COUNT(OutriggerRows))};
		case EBoatType::Limon: return {LimonRows, static_cast<int32>(UE_ARRAY_COUNT(LimonRows))};
		default: return {};
		}
	}

	bool IsValidType(EBoatPieceType Type)
	{
		return static_cast<int32>(Type) < static_cast<int32>(EBoatPieceType::Count);
	}

	bool IsFinite(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}

	struct FBox3
	{
		FVector Min;
		FVector Max;
	};

	FBox3 BoxOf(const FBoatPiece& Piece)
	{
		const FVector Half = FBoatPiecesModel::EffectiveSizeCm(Piece) * 0.5;
		return FBox3{Piece.CenterCm - Half, Piece.CenterCm + Half};
	}

	/** Hueco entre dos cajas (cm; 0 si se tocan o se solapan). */
	double GapCm(const FBoatPiece& A, const FBoatPiece& B)
	{
		const FBox3 BA = BoxOf(A);
		const FBox3 BB = BoxOf(B);
		const double DX = FMath::Max(0.0, FMath::Max(BA.Min.X - BB.Max.X, BB.Min.X - BA.Max.X));
		const double DY = FMath::Max(0.0, FMath::Max(BA.Min.Y - BB.Max.Y, BB.Min.Y - BA.Max.Y));
		const double DZ = FMath::Max(0.0, FMath::Max(BA.Min.Z - BB.Max.Z, BB.Min.Z - BA.Max.Z));
		return FMath::Sqrt(DX * DX + DY * DY + DZ * DZ);
	}

	bool Touches(const FBoatPiece& A, const FBoatPiece& B)
	{
		return GapCm(A, B) <= FBoatPiecesModel::ContactToleranceCm;
	}

	/** Toca alguna pieza de uno de los tipos dados. */
	bool TouchesAny(const TArray<FBoatPiece>& Pieces, int32 Self, std::initializer_list<EBoatPieceType> Types)
	{
		for (int32 I = 0; I < Pieces.Num(); ++I)
		{
			if (I == Self)
			{
				continue;
			}
			for (const EBoatPieceType Type : Types)
			{
				if (Pieces[I].Type == Type && Touches(Pieces[Self], Pieces[I]))
				{
					return true;
				}
			}
		}
		return false;
	}

	/** Extensión en un eje de las piezas de ciertos tipos; false si no hay ninguna. */
	bool Extent(const TArray<FBoatPiece>& Pieces, std::initializer_list<EBoatPieceType> Types, int32 Axis, double& OutMin, double& OutMax)
	{
		bool bAny = false;
		OutMin = 0.0;
		OutMax = 0.0;
		for (const FBoatPiece& Piece : Pieces)
		{
			bool bMatch = false;
			for (const EBoatPieceType Type : Types)
			{
				bMatch |= Piece.Type == Type;
			}
			if (!bMatch)
			{
				continue;
			}
			const FBox3 Box = BoxOf(Piece);
			const double Lo = Box.Min[Axis];
			const double Hi = Box.Max[Axis];
			OutMin = bAny ? FMath::Min(OutMin, Lo) : Lo;
			OutMax = bAny ? FMath::Max(OutMax, Hi) : Hi;
			bAny = true;
		}
		return bAny;
	}
}

// ----------------------------------------------------------------------------- catálogo y planos

const FBoatPieceSpec& FBoatPiecesModel::Spec(EBoatPieceType Type)
{
	static const BoatPiecesDetail::FSpecTable Table;
	const int32 Index = FMath::Clamp(static_cast<int32>(Type), 0, static_cast<int32>(EBoatPieceType::Count) - 1);
	return Table.Entries[Index];
}

FBoatPiecesModel FBoatPiecesModel::Blueprint(EBoatType Type)
{
	FBoatPiecesModel Model;
	for (const BoatPiecesDetail::FBlueprintRow& Row : BoatPiecesDetail::RowsOf(Type))
	{
		FBoatPiece Piece;
		Piece.Type = Row.Type;
		Piece.CenterCm = FVector(Row.X, Row.Y, Row.Z);
		Piece.SizeCm = FVector(Row.SX, Row.SY, Row.SZ);
		Model.AddPiece(Piece);
	}
	return Model;
}

int32 FBoatPiecesModel::BlueprintCount(EBoatType Type, EBoatPieceType Piece)
{
	int32 Count = 0;
	for (const BoatPiecesDetail::FBlueprintRow& Row : BoatPiecesDetail::RowsOf(Type))
	{
		Count += Row.Type == Piece ? 1 : 0;
	}
	return Count;
}

uint32 FBoatPiecesModel::RequiredShipPartsMask(EBoatType Type)
{
	// El «Limón» lleva las cuatro piezas del Albatros (GDD §4.3, boats.json «requiresShipParts»).
	return Type == EBoatType::Limon ? AllShipPartsMask : 0u;
}

bool FBoatPiecesModel::HasShipPartsFor(EBoatType Type, uint32 RecoveredShipPartsMask)
{
	const uint32 Required = RequiredShipPartsMask(Type);
	return (RecoveredShipPartsMask & Required) == Required;
}

// ----------------------------------------------------------------------------- montaje

FVector FBoatPiecesModel::EffectiveSizeCm(const FBoatPiece& Piece)
{
	const FVector Default = Spec(Piece.Type).DefaultSizeCm;
	FVector Size;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double V = Piece.SizeCm[Axis];
		Size[Axis] = (FMath::IsFinite(V) && V > 0.0) ? FMath::Clamp(V, static_cast<double>(MinPieceSizeCm), static_cast<double>(MaxPieceSizeCm)) : Default[Axis];
	}
	return Size;
}

float FBoatPiecesModel::ScaleOf(const FBoatPiece& Piece)
{
	const FVector D = Spec(Piece.Type).DefaultSizeCm;
	const FVector S = EffectiveSizeCm(Piece);
	switch (Piece.Type)
	{
	case EBoatPieceType::Frame:
		// Costilla en U: crece con el perímetro (manga + dos puntales).
		return static_cast<float>((S.Y + 2.0 * S.Z) / (D.Y + 2.0 * D.Z));
	case EBoatPieceType::Mast:
		return static_cast<float>(S.Z / D.Z);
	case EBoatPieceType::Sail:
		return static_cast<float>((S.Y * S.Z) / (D.Y * D.Z));
	case EBoatPieceType::RowingBench:
		return static_cast<float>(S.Y / D.Y);
	case EBoatPieceType::Mooring:
		return 1.0f;
	default:
		// Quilla, tablón, cubierta, balancín y timón: su largo.
		return static_cast<float>(S.X / D.X);
	}
}

float FBoatPiecesModel::PieceMassKg(const FBoatPiece& Piece)
{
	return Spec(Piece.Type).MassKg * ScaleOf(Piece);
}

float FBoatPiecesModel::PieceBuoyancyLiters(const FBoatPiece& Piece)
{
	return Spec(Piece.Type).BuoyancyLiters * ScaleOf(Piece);
}

int32 FBoatPiecesModel::AddPiece(const FBoatPiece& Piece)
{
	if (!BoatPiecesDetail::IsValidType(Piece.Type) || !BoatPiecesDetail::IsFinite(Piece.CenterCm) || Pieces.Num() >= MaxPieces)
	{
		return INDEX_NONE;
	}
	FBoatPiece Clean = Piece;
	Clean.SizeCm = EffectiveSizeCm(Piece);
	const int32 Index = Pieces.Add(Clean);
	RebuildJoints();
	return Index;
}

bool FBoatPiecesModel::RemovePiece(int32 Index)
{
	if (!Pieces.IsValidIndex(Index))
	{
		return false;
	}
	// Renumera las uniones que sobreviven para que RebuildJoints conserve su integridad.
	TArray<FBoatHullJoint> Kept;
	for (const FBoatHullJoint& J : Joints)
	{
		if (J.Frame == Index || J.Plank == Index)
		{
			continue;
		}
		FBoatHullJoint Moved = J;
		Moved.Frame -= J.Frame > Index ? 1 : 0;
		Moved.Plank -= J.Plank > Index ? 1 : 0;
		Kept.Add(Moved);
	}
	Pieces.RemoveAt(Index);
	Joints = MoveTemp(Kept);
	RebuildJoints();
	return true;
}

void FBoatPiecesModel::RebuildJoints()
{
	const TArray<FBoatHullJoint> Previous = Joints;
	Joints.Reset();
	for (int32 F = 0; F < Pieces.Num(); ++F)
	{
		if (Pieces[F].Type != EBoatPieceType::Frame)
		{
			continue;
		}
		for (int32 P = 0; P < Pieces.Num(); ++P)
		{
			if (Pieces[P].Type != EBoatPieceType::HullPlank || !BoatPiecesDetail::Touches(Pieces[F], Pieces[P]))
			{
				continue;
			}
			FBoatHullJoint J;
			J.Frame = F;
			J.Plank = P;
			J.LocationCm = FVector(Pieces[F].CenterCm.X, Pieces[P].CenterCm.Y, Pieces[P].CenterCm.Z);
			J.MaxIntegrity = static_cast<float>(FMath::Min(Spec(EBoatPieceType::Frame).BaseIntegrity, Spec(EBoatPieceType::HullPlank).BaseIntegrity));
			J.Integrity = J.MaxIntegrity;
			for (const FBoatHullJoint& Old : Previous)
			{
				if (Old.Frame == F && Old.Plank == P)
				{
					J.Integrity = FMath::Clamp(Old.Integrity, 0.0f, J.MaxIntegrity);
					break;
				}
			}
			Joints.Add(J);
		}
	}
}

int32 FBoatPiecesModel::CountOf(EBoatPieceType Type) const
{
	int32 Count = 0;
	for (const FBoatPiece& Piece : Pieces)
	{
		Count += Piece.Type == Type ? 1 : 0;
	}
	return Count;
}

uint32 FBoatPiecesModel::FindIssues() const
{
	using namespace BoatPiecesDetail;
	uint32 Issues = 0;
	auto Flag = [&Issues](EBoatHullIssue Issue) { Issues |= static_cast<uint32>(Issue); };

	if (CountOf(T::Keel) == 0)
	{
		Flag(EBoatHullIssue::NoKeel);
	}
	if (CountOf(T::Frame) == 0)
	{
		Flag(EBoatHullIssue::NoFrames);
	}
	if (CountOf(T::HullPlank) == 0)
	{
		Flag(EBoatHullIssue::NoPlanks);
	}
	TArray<uint8> PlankSupported;
	PlankSupported.Init(0, Pieces.Num());
	for (const FBoatHullJoint& J : Joints)
	{
		PlankSupported[J.Plank] = 1;
	}
	for (int32 I = 0; I < Pieces.Num(); ++I)
	{
		switch (Pieces[I].Type)
		{
		case T::Frame:
			// Una cuaderna cruza la quilla; un botalón de balancín se ata sobre los tablones.
			if (!TouchesAny(Pieces, I, {T::Keel, T::HullPlank}))
			{
				Flag(EBoatHullIssue::FrameUnattached);
			}
			break;
		case T::HullPlank:
			if (PlankSupported[I] == 0)
			{
				Flag(EBoatHullIssue::PlankUnsupported);
			}
			break;
		case T::Mast:
			// Plantado en la quilla, o en un travesaño en un casco doble.
			if (!TouchesAny(Pieces, I, {T::Keel, T::Frame}))
			{
				Flag(EBoatHullIssue::MastUnstepped);
			}
			break;
		case T::Sail:
			if (!TouchesAny(Pieces, I, {T::Mast}))
			{
				Flag(EBoatHullIssue::SailWithoutMast);
			}
			break;
		case T::Outrigger:
			if (!TouchesAny(Pieces, I, {T::Frame}))
			{
				Flag(EBoatHullIssue::OutriggerUnattached);
			}
			break;
		default:
			break;
		}
	}
	return Issues;
}

// ----------------------------------------------------------------------------- hidrostática

FBoatHullReport FBoatPiecesModel::EvaluateWithCrew() const
{
	FBoatLoadout Loadout;
	double KeelY = 0.0;
	int32 Keels = 0;
	for (const FBoatPiece& Piece : Pieces)
	{
		if (Piece.Type == EBoatPieceType::Keel)
		{
			KeelY += Piece.CenterCm.Y;
			++Keels;
		}
	}
	FBoatLoadMass Crew;
	Crew.MassKg = FBoatModel::CrewMassKg;
	Crew.CenterCm = FVector(0.0, Keels > 0 ? KeelY / Keels : 0.0, CrewSeatedHeightCm);
	Loadout.Masses.Add(Crew);
	return Evaluate(Loadout);
}

FBoatHullReport FBoatPiecesModel::Evaluate(const FBoatLoadout& Loadout) const
{
	using namespace BoatPiecesDetail;
	FBoatHullReport R;
	R.Issues = FindIssues();

	// Quilla: el cero de las alturas. Sin quilla, la pieza más baja.
	double KeelZ = 0.0;
	bool bHasKeel = false;
	double LowestZ = 0.0;
	for (int32 I = 0; I < Pieces.Num(); ++I)
	{
		const double Bottom = BoxOf(Pieces[I]).Min.Z;
		LowestZ = I == 0 ? Bottom : FMath::Min(LowestZ, Bottom);
		if (Pieces[I].Type == T::Keel)
		{
			KeelZ = bHasKeel ? FMath::Min(KeelZ, Bottom) : Bottom;
			bHasKeel = true;
		}
	}
	if (!bHasKeel)
	{
		KeelZ = LowestZ;
	}

	// Masas, flotación, carga y vela.
	double Mass = 0.0, MZ = 0.0;
	double Liters = 0.0, HullLiters = 0.0;
	double SailArea = 0.0, SailZ = 0.0;
	double OutriggerY = 0.0;
	int32 Outriggers = 0;
	double KeelY = 0.0;
	int32 Keels = 0;
	// El peso de un flotador lo sostiene su propia flotación: no escora el casco.
	double MomentMY = 0.0, MomentMass = 0.0;
	for (const FBoatPiece& Piece : Pieces)
	{
		const double M = PieceMassKg(Piece);
		Mass += M;
		MZ += M * Piece.CenterCm.Z;
		const double L = PieceBuoyancyLiters(Piece);
		Liters += L;
		if (Piece.Type != T::Outrigger)
		{
			MomentMY += M * Piece.CenterCm.Y;
			MomentMass += M;
		}
		switch (Piece.Type)
		{
		case T::HullPlank:
			HullLiters += L;
			break;
		case T::Deck:
			R.DeckCargoKg += Spec(T::Deck).CargoKg * ScaleOf(Piece);
			break;
		case T::Sail:
			SailArea += Spec(T::Sail).SailAreaM2 * ScaleOf(Piece);
			SailZ += Spec(T::Sail).SailAreaM2 * ScaleOf(Piece) * Piece.CenterCm.Z;
			break;
		case T::Outrigger:
			OutriggerY += Piece.CenterCm.Y;
			++Outriggers;
			break;
		case T::Keel:
			KeelY += Piece.CenterCm.Y;
			++Keels;
			break;
		default:
			break;
		}
	}
	R.StructureMassKg = static_cast<float>(Mass);
	const double AxisY = Keels > 0 ? KeelY / Keels : 0.0;
	const bool bHasMast = CountOf(T::Mast) > 0;
	R.SailAreaM2 = bHasMast ? static_cast<float>(SailArea) : 0.0f;
	const double SailCenterZ = SailArea > 0.0 ? SailZ / SailArea : 0.0;

	for (const FBoatLoadMass& Load : Loadout.Masses)
	{
		if (!FMath::IsFinite(Load.MassKg) || Load.MassKg <= 0.0f || !IsFinite(Load.CenterCm))
		{
			continue;
		}
		Mass += Load.MassKg;
		MZ += Load.MassKg * Load.CenterCm.Z;
		MomentMY += Load.MassKg * Load.CenterCm.Y;
		MomentMass += Load.MassKg;
	}

	// Medidas del casco.
	double MinX, MaxX, MinY, MaxY, MinZ, MaxZ;
	if (Extent(Pieces, {T::Keel, T::HullPlank, T::Outrigger}, 0, MinX, MaxX))
	{
		R.LengthCm = static_cast<float>(MaxX - MinX);
	}
	if (Extent(Pieces, {T::Frame, T::HullPlank, T::Outrigger, T::Keel}, 1, MinY, MaxY))
	{
		R.BeamCm = static_cast<float>(MaxY - MinY);
	}
	if (Extent(Pieces, {T::Frame}, 2, MinZ, MaxZ))
	{
		R.DepthCm = static_cast<float>(FMath::Max(0.0, MaxZ - KeelZ));
	}
	const double DepthM = R.DepthCm / 100.0;
	R.WaterplaneAreaM2 = DepthM > 0.0 ? static_cast<float>(Liters / 1000.0 / DepthM) : 0.0f;
	R.DesignDraftCm = DesignDraftFraction * R.DepthCm;
	R.MaxBuoyancyKg = static_cast<float>(Liters * SeaWaterKgPerLiter);

	// Agua embarcada: en el fondo, sobre el eje.
	const float Water = FMath::IsFinite(Loadout.WaterInHullKg) ? FMath::Max(Loadout.WaterInHullKg, 0.0f) : 0.0f;
	const double DryMass = Mass;
	if (Water > 0.0f)
	{
		Mass += Water;
		MomentMY += Water * AxisY;
		MomentMass += Water;
		MZ += Water * (KeelZ + 0.25 * R.DepthCm);
	}
	R.TotalMassKg = static_cast<float>(Mass);
	R.SwampWaterKg = static_cast<float>(FMath::Max(0.0, R.MaxBuoyancyKg - DryMass));
	R.LoadRatio = R.MaxBuoyancyKg > 0.0f ? R.TotalMassKg / R.MaxBuoyancyKg : (Mass > 0.0 ? TNumericLimits<float>::Max() : 0.0f);

	const double RhoWater = SeaWaterKgPerLiter * 1000.0;
	if (R.WaterplaneAreaM2 > 0.0f)
	{
		const double Draft = Mass / (RhoWater * R.WaterplaneAreaM2) * 100.0;
		R.EquilibriumDraftCm = static_cast<float>(FMath::Min(Draft, static_cast<double>(R.DepthCm)));
	}

	// Estabilidad inicial: casco de caja con la flotación de los tablones.
	if (Mass > 0.0)
	{
		R.KGCm = static_cast<float>(MZ / Mass - KeelZ);
	}
	R.KBCm = 0.5f * R.EquilibriumDraftCm;
	double HullMinY, HullMaxY;
	if (DepthM > 0.0 && Mass > 0.0 && Extent(Pieces, {T::HullPlank, T::Keel}, 1, HullMinY, HullMaxY))
	{
		const double HullBeamM = (HullMaxY - HullMinY) / 100.0;
		const double HullArea = HullLiters / 1000.0 / DepthM;
		const double Displaced = Mass / RhoWater;
		R.BMCm = static_cast<float>(HullArea * HullBeamM * HullBeamM / 12.0 / Displaced * 100.0);
	}
	R.GMCm = R.KBCm + R.BMCm - R.KGCm;

	// Balancín: hundido suma la inercia de su flotación (sección redonda, a su brazo del eje);
	// levantado, solo su peso endereza hasta que se despega del agua.
	double FloatInertiaM4 = 0.0, FloatWeightMomentNm = 0.0, FloatSide = 0.0;
	for (const FBoatPiece& Piece : Pieces)
	{
		if (Piece.Type != T::Outrigger)
		{
			continue;
		}
		const FVector Size = EffectiveSizeCm(Piece);
		const double ArmM = (Piece.CenterCm.Y - AxisY) / 100.0;
		const double AreaM2 = Size.X / 100.0 * Size.Y / 100.0 * UE_PI / 4.0;
		FloatInertiaM4 += AreaM2 * ArmM * ArmM;
		FloatWeightMomentNm += PieceMassKg(Piece) * Gravity * FMath::Abs(ArmM);
		FloatSide += ArmM;
	}
	if (Mass > 0.0 && Outriggers > 0)
	{
		R.OutriggerBMCm = static_cast<float>(FloatInertiaM4 / (Mass / RhoWater) * 100.0);
	}

	// Momento escorante respecto al eje de la quilla: masas fuera del eje (salvo los
	// flotadores, que sostiene su propia flotación) y vela izada con viento de través.
	double Heeling = (MomentMY - MomentMass * AxisY) * Gravity / 100.0;
	if (Loadout.bSailRaised && R.SailAreaM2 > 0.0f && FMath::IsFinite(Loadout.BeamWindMS))
	{
		const double V = Loadout.BeamWindMS;
		const double Force = 0.5 * AirDensity * R.SailAreaM2 * V * FMath::Abs(V) * SailSideForceCoefficient;
		const double ArmM = FMath::Max(0.0, SailCenterZ - KeelZ - R.EquilibriumDraftCm) / 100.0;
		Heeling += Force * ArmM;
	}
	// Momento que de verdad escora el casco y altura metacéntrica con la que lo aguanta.
	double Net = Heeling;
	R.EffectiveGMCm = R.GMCm;
	if (Outriggers > 0 && FloatSide != 0.0)
	{
		if (FloatSide * Heeling >= 0.0)
		{
			// Hacia el flotador: lo hunde, y además quita el 60 % del momento (biblia 02 §8.2).
			Heeling *= OutriggerHeelFactor;
			Net = Heeling;
			R.EffectiveGMCm = R.GMCm + R.OutriggerBMCm;
		}
		else
		{
			// Hacia el otro lado: el peso del flotador sujeta hasta que se despega.
			Net = FMath::Sign(Heeling) * FMath::Max(0.0, FMath::Abs(Heeling) - FloatWeightMomentNm);
		}
	}
	R.HeelingMomentNm = static_cast<float>(Heeling);
	bool bUnstable = false;
	if (Mass > 0.0)
	{
		if (R.EffectiveGMCm > 0.0f)
		{
			const double Righting = Mass * Gravity * R.EffectiveGMCm / 100.0;
			R.HeelDeg = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Net, Righting)));
		}
		else
		{
			// Sin altura metacéntrica vuelca en cuanto algo lo escora; sin balancín, siempre.
			bUnstable = Outriggers == 0 || Net != 0.0;
			R.HeelDeg = Net != 0.0 ? static_cast<float>(FMath::Sign(Net) * 90.0) : 0.0f;
		}
	}

	// Veredicto, del peor al mejor.
	if (R.Issues != 0)
	{
		R.Verdict = EBoatHullVerdict::Incomplete;
	}
	else if (R.MaxBuoyancyKg <= 0.0f || R.LoadRatio > SinkLoadRatio)
	{
		R.Verdict = EBoatHullVerdict::Sinks;
	}
	else if (bUnstable || FMath::Abs(R.HeelDeg) > FBoatCapsizeTimer::SustainedHeelDeg)
	{
		R.Verdict = EBoatHullVerdict::Capsizes;
	}
	else if (R.LoadRatio > TakingWaterLoadRatio)
	{
		R.Verdict = EBoatHullVerdict::TakingWater;
	}
	else
	{
		R.Verdict = EBoatHullVerdict::Floats;
	}
	R.OverloadIngressKgS = R.LoadRatio > TakingWaterLoadRatio && R.Issues == 0 ? OverloadIngressKgS : 0.0f;
	return R;
}

// ----------------------------------------------------------------------------- ficha de navegación

FBoatDefinition FBoatPiecesModel::ToBoatDefinition(const FBoatDefinition& Tuning) const
{
	const FBoatHullReport R = EvaluateWithCrew();
	FBoatDefinition D = Tuning;
	D.LengthCm = FMath::Max(R.LengthCm, 1.0f);
	D.BeamCm = FMath::Max(R.BeamCm, 1.0f);
	D.HullDepthCm = FMath::Max(R.DepthCm, 1.0f);
	D.HullMassKg = FMath::Max(R.StructureMassKg, 1.0f);
	// Área de flotación = eslora × manga × coeficiente: así el calado de FBoatModel es el de las piezas.
	D.WaterplaneCoefficient = FMath::Clamp(R.WaterplaneAreaM2 / (D.LengthCm / 100.0f * D.BeamCm / 100.0f), 0.05f, 1.0f);
	// Carga: lo que quepa en las cubiertas sin pasar del calado de diseño con el tripulante.
	const float DesignCapacityKg = FBoatModel::WaterDensity * R.WaterplaneAreaM2 * R.DesignDraftCm / 100.0f;
	D.MaxCargoKg = FMath::Max(0.0f, FMath::Min(R.DeckCargoKg, DesignCapacityKg - R.StructureMassKg - FBoatModel::CrewMassKg));
	D.SailAreaM2 = R.SailAreaM2;
	D.SailCenterOfEffortCm = 0.0f;
	if (D.SailAreaM2 > 0.0f)
	{
		double Area = 0.0, Z = 0.0, KeelZ = TNumericLimits<double>::Max();
		for (const FBoatPiece& Piece : Pieces)
		{
			if (Piece.Type == EBoatPieceType::Sail)
			{
				const double A = Spec(Piece.Type).SailAreaM2 * ScaleOf(Piece);
				Area += A;
				Z += A * Piece.CenterCm.Z;
			}
			if (Piece.Type == EBoatPieceType::Keel)
			{
				KeelZ = FMath::Min(KeelZ, Piece.CenterCm.Z - EffectiveSizeCm(Piece).Z * 0.5);
			}
		}
		if (KeelZ == TNumericLimits<double>::Max())
		{
			KeelZ = 0.0;
		}
		D.SailCenterOfEffortCm = static_cast<float>(FMath::Max(0.0, Z / Area - KeelZ - R.EquilibriumDraftCm));
	}
	// FBoatModel no distingue bandas: con balancín, la media entre el flotador hundido y levantado.
	D.MetacentricHeightCm = FMath::Max(R.GMCm + 0.5f * R.OutriggerBMCm, 1.0f);
	return D;
}

// ----------------------------------------------------------------------------- integridad

FBoatImpactReport FBoatPiecesModel::ApplyImpact(float SpeedCmS, const FVector& PointCm)
{
	FBoatImpactReport Report;
	if (!FMath::IsFinite(SpeedCmS) || SpeedCmS <= FBoatModel::SafeImpactSpeedCmS || !BoatPiecesDetail::IsFinite(PointCm) || Joints.Num() == 0)
	{
		return Report;
	}
	const double ExcessMS = (SpeedCmS - FBoatModel::SafeImpactSpeedCmS) / 100.0;
	double Nearest = TNumericLimits<double>::Max();
	for (const FBoatHullJoint& J : Joints)
	{
		Nearest = FMath::Min(Nearest, FVector::Dist(J.LocationCm, PointCm));
	}
	for (FBoatHullJoint& J : Joints)
	{
		if (J.IsBreached())
		{
			continue;
		}
		// La más cercana se lleva todo; las de alrededor, menos cuanto más lejos.
		const double Weight = 1.0 - (FVector::Dist(J.LocationCm, PointCm) - Nearest) / ImpactRadiusCm;
		if (Weight <= 0.0)
		{
			continue;
		}
		const float Damage = static_cast<float>(ImpactIntegrityPerMS * ExcessMS * Weight);
		const float Before = J.Integrity;
		J.Integrity = FMath::Max(0.0f, J.Integrity - Damage);
		Report.IntegrityLost += Before - J.Integrity;
		++Report.JointsDamaged;
		if (J.IsBreached())
		{
			++Report.NewBreaches;
		}
	}
	return Report;
}

bool FBoatPiecesModel::RepairJoint(int32 JointIndex, float Points)
{
	if (!Joints.IsValidIndex(JointIndex) || !FMath::IsFinite(Points) || Points <= 0.0f)
	{
		return false;
	}
	FBoatHullJoint& J = Joints[JointIndex];
	if (J.Integrity >= J.MaxIntegrity)
	{
		return false;
	}
	J.Integrity = FMath::Min(J.MaxIntegrity, J.Integrity + Points);
	return true;
}

int32 FBoatPiecesModel::BreachCount() const
{
	int32 Count = 0;
	for (const FBoatHullJoint& J : Joints)
	{
		Count += J.IsBreached() ? 1 : 0;
	}
	return Count;
}

float FBoatPiecesModel::LeakKgS() const
{
	return BreachCount() * BreachLeakLitersPerS * SeaWaterKgPerLiter;
}

float FBoatPiecesModel::Integrity01() const
{
	double Sum = 0.0, Max = 0.0;
	for (const FBoatHullJoint& J : Joints)
	{
		Sum += J.Integrity;
		Max += J.MaxIntegrity;
	}
	return Max > 0.0 ? static_cast<float>(Sum / Max) : 1.0f;
}

// ----------------------------------------------------------------------------- vuelco

bool FBoatCapsizeTimer::Step(float HeelDeg, float DeltaSeconds)
{
	if (bCapsized)
	{
		return true;
	}
	// Una escora no finita es un casco que ya no sabe dónde está: se trata como vuelco.
	const float Heel = FMath::IsFinite(HeelDeg) ? FMath::Abs(HeelDeg) : 180.0f;
	const float Dt = FMath::IsFinite(DeltaSeconds) ? FMath::Max(DeltaSeconds, 0.0f) : 0.0f;
	if (Heel > InstantHeelDeg)
	{
		bCapsized = true;
	}
	else if (Heel > SustainedHeelDeg)
	{
		OverSeconds += Dt;
		bCapsized = OverSeconds > SustainedSeconds;
	}
	else
	{
		OverSeconds = 0.0f;
	}
	return bCapsized;
}
