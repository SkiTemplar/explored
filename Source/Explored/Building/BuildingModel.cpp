#include "Building/BuildingModel.h"

namespace BuildingModelDetail
{
	/** Tipos de hueco de la rejilla. Un hueco lo ocupa como mucho una pieza. */
	enum class ESlotKind : uint8
	{
		Corner = 1,    // esquina: pilares
		EdgeX = 2,     // lado sur de una celda (corre a lo largo de X): paredes y puertas
		EdgeY = 3,     // lado oeste de una celda (corre a lo largo de Y): paredes y puertas
		Center = 4,    // suelo de una celda
		Roof = 5,      // techo sobre una celda
		Interior = 6,  // lo que se apoya dentro de la celda: muebles, fuegos, escaleras
	};

	/** Empaqueta base, tipo y coordenadas en un uint64 (clave de TMap estable en UE y en el host). */
	uint64 MakeSlot(int32 BaseId, ESlotKind Kind, int32 X, int32 Y, int32 Z)
	{
		const uint64 Base = static_cast<uint64>(BaseId & 0xFFFF);
		const uint64 KindBits = static_cast<uint64>(Kind) & 0xF;
		const uint64 XBits = static_cast<uint64>((X + 32768) & 0xFFFF);
		const uint64 YBits = static_cast<uint64>((Y + 32768) & 0xFFFF);
		const uint64 ZBits = static_cast<uint64>((Z + 128) & 0xFF);
		return (Base << 48) | (KindBits << 44) | (XBits << 28) | (YBits << 12) | (ZBits << 4);
	}

	int32 NormalizeRotation(int32 Rotation)
	{
		return ((Rotation % 4) + 4) % 4;
	}

	/** Hacia dónde sube una escalera: el +Y local girado Rotation × 90° de guiñada. */
	FIntPoint StairsDirection(int32 Rotation)
	{
		switch (NormalizeRotation(Rotation))
		{
		case 1: return FIntPoint(-1, 0);
		case 2: return FIntPoint(0, -1);
		case 3: return FIntPoint(1, 0);
		default: return FIntPoint(0, 1);
		}
	}

	bool IsEdgeSocket(EBuildSocket Socket)
	{
		return Socket == EBuildSocket::Wall || Socket == EBuildSocket::Door;
	}

	/** Encajes que pueden apoyarse directamente en el terreno. */
	bool CanTouchGround(EBuildSocket Socket)
	{
		return Socket != EBuildSocket::Roof;
	}

	/** Encajes que solo se sostienen sobre el terreno. */
	bool NeedsGround(EBuildSocket Socket)
	{
		return Socket == EBuildSocket::Pillar || Socket == EBuildSocket::GroundOnly;
	}

	/** Lado de una pared: tipo de hueco y coordenadas (la paridad del giro decide la orientación). */
	void EdgeOf(const FBuildingPlacement& Placement, ESlotKind& OutKind, int32& OutX, int32& OutY)
	{
		OutKind = (NormalizeRotation(Placement.Rotation) % 2 == 0) ? ESlotKind::EdgeX : ESlotKind::EdgeY;
		OutX = Placement.Cell.X;
		OutY = Placement.Cell.Y;
	}

	/** Las dos celdas a los lados de un lado. */
	void CellsBesideEdge(ESlotKind Kind, int32 X, int32 Y, FIntPoint& OutA, FIntPoint& OutB)
	{
		OutA = FIntPoint(X, Y);
		OutB = Kind == ESlotKind::EdgeX ? FIntPoint(X, Y - 1) : FIntPoint(X - 1, Y);
	}

	/** Las dos esquinas en los extremos de un lado. */
	void CornersOfEdge(ESlotKind Kind, int32 X, int32 Y, FIntPoint& OutA, FIntPoint& OutB)
	{
		OutA = FIntPoint(X, Y);
		OutB = Kind == ESlotKind::EdgeX ? FIntPoint(X + 1, Y) : FIntPoint(X, Y + 1);
	}

	struct FEdgeRef
	{
		ESlotKind Kind;
		int32 X;
		int32 Y;
	};

	/** Los cuatro lados de una celda (sur, norte, oeste, este). */
	void EdgesOfCell(int32 X, int32 Y, FEdgeRef OutEdges[4])
	{
		OutEdges[0] = {ESlotKind::EdgeX, X, Y};
		OutEdges[1] = {ESlotKind::EdgeX, X, Y + 1};
		OutEdges[2] = {ESlotKind::EdgeY, X, Y};
		OutEdges[3] = {ESlotKind::EdgeY, X + 1, Y};
	}

	/** Los seis lados que comparten un extremo con otro (paredes contiguas o en esquina). */
	void EdgesTouchingEdge(ESlotKind Kind, int32 X, int32 Y, FEdgeRef OutEdges[6])
	{
		if (Kind == ESlotKind::EdgeX)
		{
			OutEdges[0] = {ESlotKind::EdgeX, X - 1, Y};
			OutEdges[1] = {ESlotKind::EdgeX, X + 1, Y};
			OutEdges[2] = {ESlotKind::EdgeY, X, Y};
			OutEdges[3] = {ESlotKind::EdgeY, X, Y - 1};
			OutEdges[4] = {ESlotKind::EdgeY, X + 1, Y};
			OutEdges[5] = {ESlotKind::EdgeY, X + 1, Y - 1};
		}
		else
		{
			OutEdges[0] = {ESlotKind::EdgeY, X, Y - 1};
			OutEdges[1] = {ESlotKind::EdgeY, X, Y + 1};
			OutEdges[2] = {ESlotKind::EdgeX, X, Y};
			OutEdges[3] = {ESlotKind::EdgeX, X - 1, Y};
			OutEdges[4] = {ESlotKind::EdgeX, X, Y + 1};
			OutEdges[5] = {ESlotKind::EdgeX, X - 1, Y + 1};
		}
	}

	const FIntPoint CellNeighbours[4] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};

	/** Pequeño margen para comparar estabilidades acumuladas en float. */
	constexpr float StabilityEpsilon = 1.0e-4f;

	void AddUniqueTools(const TArray<FName>& From, TArray<FName>& Out)
	{
		for (const FName& Tool : From)
		{
			Out.AddUnique(Tool);
		}
	}

	bool HasMaterials(const TArray<FBuildingCost>& Cost, const TMap<FName, int32>& Inventory, TArray<FBuildingCost>* OutMissing)
	{
		bool bOk = true;
		for (const FBuildingCost& Entry : Cost)
		{
			const int32* Have = Inventory.Find(Entry.Item);
			const int32 Count = Have ? *Have : 0;
			if (Count < Entry.Count)
			{
				bOk = false;
				if (OutMissing)
				{
					OutMissing->Add({Entry.Item, Entry.Count - Count});
				}
			}
		}
		return bOk;
	}

	void ConsumeMaterials(const TArray<FBuildingCost>& Cost, TMap<FName, int32>& Inventory)
	{
		for (const FBuildingCost& Entry : Cost)
		{
			if (int32* Have = Inventory.Find(Entry.Item))
			{
				*Have -= Entry.Count;
				if (*Have <= 0)
				{
					Inventory.Remove(Entry.Item);
				}
			}
		}
	}
}

namespace BMDetail = BuildingModelDetail;

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------

const TCHAR* LexToString(EBuildSocket Socket)
{
	switch (Socket)
	{
	case EBuildSocket::Pillar: return TEXT("pilar");
	case EBuildSocket::Floor: return TEXT("suelo");
	case EBuildSocket::Wall: return TEXT("pared");
	case EBuildSocket::Door: return TEXT("puerta");
	case EBuildSocket::Roof: return TEXT("techo");
	case EBuildSocket::Stairs: return TEXT("escalera");
	case EBuildSocket::Furniture: return TEXT("mueble");
	case EBuildSocket::GroundOnly: return TEXT("terreno");
	default: return TEXT("desconocido");
	}
}

const TCHAR* LexToString(EBuildFailReason Reason)
{
	switch (Reason)
	{
	case EBuildFailReason::None: return TEXT("None");
	case EBuildFailReason::UnknownPiece: return TEXT("UnknownPiece");
	case EBuildFailReason::InvalidRotation: return TEXT("InvalidRotation");
	case EBuildFailReason::UnknownBase: return TEXT("UnknownBase");
	case EBuildFailReason::Occupied: return TEXT("Occupied");
	case EBuildFailReason::NoSupport: return TEXT("NoSupport");
	case EBuildFailReason::Unstable: return TEXT("Unstable");
	case EBuildFailReason::NeedsGround: return TEXT("NeedsGround");
	case EBuildFailReason::MissingRequiredPiece: return TEXT("MissingRequiredPiece");
	case EBuildFailReason::MissingTools: return TEXT("MissingTools");
	case EBuildFailReason::MissingMaterials: return TEXT("MissingMaterials");
	case EBuildFailReason::NothingToRepair: return TEXT("NothingToRepair");
	case EBuildFailReason::UnknownInstance: return TEXT("UnknownInstance");
	default: return TEXT("Unknown");
	}
}

bool ParseBuildSocket(const FString& Text, EBuildSocket& OutSocket)
{
	for (int32 Index = 0; Index < static_cast<int32>(EBuildSocket::Count); ++Index)
	{
		const EBuildSocket Candidate = static_cast<EBuildSocket>(Index);
		if (Text.Equals(FString(LexToString(Candidate)), ESearchCase::IgnoreCase))
		{
			OutSocket = Candidate;
			return true;
		}
	}
	return false;
}

const FBuildingPieceDef* FBuildingCatalog::FindPiece(FName Id) const
{
	return Pieces.FindByPredicate([&Id](const FBuildingPieceDef& Def) { return Def.Id == Id; });
}

const FBuildingTierDef* FBuildingCatalog::FindTier(FName Id) const
{
	return Tiers.FindByPredicate([&Id](const FBuildingTierDef& Def) { return Def.Id == Id; });
}

int32 FBuildingCatalog::TierOrderOf(const FBuildingPieceDef& Piece) const
{
	const FBuildingTierDef* Tier = FindTier(Piece.Tier);
	return Tier ? Tier->Order : 0;
}

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------

FBuildingModel::FBuildingModel(FBuildingCatalog InCatalog)
	: Catalog(MoveTemp(InCatalog))
{
	for (int32 Index = 0; Index < Catalog.Pieces.Num(); ++Index)
	{
		DefLookup.Add(Catalog.Pieces[Index].Id, Index);
	}
}

FBuildingTierTuning FBuildingModel::TuningForTierOrder(int32 Order)
{
	// Valheim como referencia: la madera aguanta unas cinco piezas en voladizo y
	// la piedra casi nada en horizontal pero mucho en vertical. La hoja de palma se
	// pudre deprisa con la lluvia; la piedra seca apenas se entera.
	FBuildingTierTuning Tuning;
	switch (FMath::Clamp(Order, 0, 3))
	{
	case 0: // palma
		Tuning.VerticalLoss = 0.3f;
		Tuning.HorizontalLoss = 0.45f;
		Tuning.DecayPerDay = 0.02f;
		Tuning.RainDecayMultiplier = 3.0f;
		break;
	case 1: // bambú
		Tuning.VerticalLoss = 0.2f;
		Tuning.HorizontalLoss = 0.3f;
		Tuning.DecayPerDay = 0.012f;
		Tuning.RainDecayMultiplier = 2.5f;
		break;
	case 2: // madera
		Tuning.VerticalLoss = 0.125f;
		Tuning.HorizontalLoss = 0.2f;
		Tuning.DecayPerDay = 0.006f;
		Tuning.RainDecayMultiplier = 2.0f;
		break;
	default: // piedra
		Tuning.VerticalLoss = 0.05f;
		Tuning.HorizontalLoss = 0.5f;
		Tuning.DecayPerDay = 0.0015f;
		Tuning.RainDecayMultiplier = 1.0f;
		break;
	}
	return Tuning;
}

// --- Bases y rejilla --------------------------------------------------------

int32 FBuildingModel::FindBaseAt(const FVector& WorldPoint) const
{
	int32 Best = INDEX_NONE;
	double BestDistance = BaseRadiusCm;
	for (const FBuildingBaseState& Base : Bases)
	{
		const double Distance = FVector::Dist2D(Base.Origin, WorldPoint);
		if (Distance <= BestDistance && (Best == INDEX_NONE || Distance < BestDistance))
		{
			Best = Base.Id;
			BestDistance = Distance;
		}
	}
	return Best;
}

int32 FBuildingModel::FindOrCreateBase(const FVector& GroundPoint)
{
	const int32 Existing = FindBaseAt(GroundPoint);
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}
	// XY sobre la rejilla global para que dos bases vecinas queden alineadas.
	FBuildingBaseState& Base = Bases.AddDefaulted_GetRef();
	Base.Id = NextBaseId++;
	Base.Origin = FVector(
		FMath::RoundToDouble(GroundPoint.X / CellSizeCm) * CellSizeCm,
		FMath::RoundToDouble(GroundPoint.Y / CellSizeCm) * CellSizeCm,
		GroundPoint.Z);
	return Base.Id;
}

const FBuildingBaseState* FBuildingModel::FindBase(int32 BaseId) const
{
	return Bases.FindByPredicate([BaseId](const FBuildingBaseState& Base) { return Base.Id == BaseId; });
}

FBuildingPlacement FBuildingModel::SnapToGrid(int32 BaseId, const FVector& WorldPoint, EBuildSocket Socket, int32 Rotation) const
{
	FBuildingPlacement Out;
	Out.BaseId = BaseId;
	Out.Rotation = BMDetail::NormalizeRotation(Rotation);
	const FBuildingBaseState* Base = FindBase(BaseId);
	const FVector Local = Base ? WorldPoint - Base->Origin : WorldPoint;

	// El jugador apunta a la cara superior del suelo (o al terreno); el techo, a la
	// coronación de las paredes, que está un piso más arriba.
	int32 Level = FMath::RoundToInt32((Local.Z - FoundationHeightCm) / StoreyHeightCm);
	if (Socket == EBuildSocket::Roof)
	{
		Level -= 1;
	}
	Out.Cell.Z = FMath::Clamp(Level, -MaxLevels, MaxLevels);

	const double U = Local.X / CellSizeCm;
	const double V = Local.Y / CellSizeCm;
	const int32 CellX = FMath::RoundToInt32(U);
	const int32 CellY = FMath::RoundToInt32(V);

	if (Socket == EBuildSocket::Pillar)
	{
		// La esquina (X, Y) está en (X − ½, Y − ½) celdas.
		Out.Cell.X = FMath::RoundToInt32(U + 0.5);
		Out.Cell.Y = FMath::RoundToInt32(V + 0.5);
	}
	else if (BMDetail::IsEdgeSocket(Socket))
	{
		const double FracX = U - CellX;
		const double FracY = V - CellY;
		if (FMath::Abs(FracY) >= FMath::Abs(FracX))
		{
			// Lado sur o norte: corre a lo largo de X (giro par).
			Out.Cell.X = CellX;
			Out.Cell.Y = FracY < 0.0 ? CellY : CellY + 1;
			Out.Rotation = (Out.Rotation % 2 == 0) ? Out.Rotation : (Out.Rotation + 1) % 4;
		}
		else
		{
			// Lado oeste o este: corre a lo largo de Y (giro impar).
			Out.Cell.X = FracX < 0.0 ? CellX : CellX + 1;
			Out.Cell.Y = CellY;
			Out.Rotation = (Out.Rotation % 2 == 1) ? Out.Rotation : (Out.Rotation + 1) % 4;
		}
	}
	else
	{
		Out.Cell.X = CellX;
		Out.Cell.Y = CellY;
	}
	return Out;
}

bool FBuildingModel::PlacementToWorld(const FBuildingPlacement& Placement, EBuildSocket Socket, FVector& OutLocation, float& OutYawDegrees) const
{
	const FBuildingBaseState* Base = FindBase(Placement.BaseId);
	if (!Base)
	{
		return false;
	}
	const int32 Rotation = BMDetail::NormalizeRotation(Placement.Rotation);
	const double X = Placement.Cell.X * CellSizeCm;
	const double Y = Placement.Cell.Y * CellSizeCm;
	const double Half = CellSizeCm * 0.5;
	const double LevelBase = Placement.Cell.Z * StoreyHeightCm;
	const double FloorBottom = LevelBase + FoundationHeightCm;
	const double FloorTop = FloorBottom + FloorThicknessCm;
	OutYawDegrees = static_cast<float>(Rotation * 90);

	FVector Local;
	switch (Socket)
	{
	case EBuildSocket::Pillar:
		Local = FVector(X - Half, Y - Half, LevelBase);
		break;
	case EBuildSocket::Floor:
		Local = FVector(X, Y, FloorBottom);
		break;
	case EBuildSocket::Wall:
	case EBuildSocket::Door:
		Local = (Rotation % 2 == 0) ? FVector(X, Y - Half, FloorTop) : FVector(X - Half, Y, FloorTop);
		break;
	case EBuildSocket::Roof:
		Local = FVector(X, Y, FloorTop + WallHeightCm);
		break;
	case EBuildSocket::Stairs:
	{
		// Pivote en el centro del arranque (kit): medio lado antes del centro de la celda.
		const FIntPoint Dir = BMDetail::StairsDirection(Rotation);
		Local = FVector(X - Dir.X * Half, Y - Dir.Y * Half, FloorTop);
		break;
	}
	case EBuildSocket::GroundOnly:
		Local = FVector(X, Y, LevelBase);
		break;
	case EBuildSocket::Furniture:
	default:
		Local = FVector(X, Y, FloorTop);
		break;
	}
	OutLocation = Base->Origin + Local;
	return true;
}

// --- Huecos y apoyos --------------------------------------------------------

void FBuildingModel::CollectOccupiedSlots(EBuildSocket Socket, const FBuildingPlacement& Placement, TArray<uint64>& OutSlots) const
{
	const int32 B = Placement.BaseId;
	const FIntVector& C = Placement.Cell;
	switch (Socket)
	{
	case EBuildSocket::Pillar:
		OutSlots.Add(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, C.X, C.Y, C.Z));
		break;
	case EBuildSocket::Floor:
		OutSlots.Add(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X, C.Y, C.Z));
		break;
	case EBuildSocket::Wall:
	case EBuildSocket::Door:
	{
		BMDetail::ESlotKind Kind;
		int32 X, Y;
		BMDetail::EdgeOf(Placement, Kind, X, Y);
		OutSlots.Add(BMDetail::MakeSlot(B, Kind, X, Y, C.Z));
		break;
	}
	case EBuildSocket::Roof:
		OutSlots.Add(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Roof, C.X, C.Y, C.Z));
		break;
	case EBuildSocket::Stairs:
	{
		const FIntPoint Dir = BMDetail::StairsDirection(Placement.Rotation);
		OutSlots.Add(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Interior, C.X, C.Y, C.Z));
		OutSlots.Add(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Interior, C.X + Dir.X, C.Y + Dir.Y, C.Z));
		break;
	}
	case EBuildSocket::Furniture:
	case EBuildSocket::GroundOnly:
	default:
		OutSlots.Add(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Interior, C.X, C.Y, C.Z));
		break;
	}
}

int32 FBuildingModel::PieceAtSlot(uint64 Slot) const
{
	const int32* Owner = SlotOwners.Find(Slot);
	return Owner ? *Owner : INDEX_NONE;
}

bool FBuildingModel::IsBlocked(EBuildSocket Socket, const FBuildingPlacement& Placement) const
{
	const int32 B = Placement.BaseId;
	const FIntVector& C = Placement.Cell;
	switch (Socket)
	{
	case EBuildSocket::Floor:
	{
		// Un suelo no puede cortar el techo del piso de abajo, ni tapar lo que ya
		// está dentro de la celda, ni cerrar el hueco de una escalera que sube.
		if (PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Roof, C.X, C.Y, C.Z - 1)) != INDEX_NONE ||
			PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Interior, C.X, C.Y, C.Z)) != INDEX_NONE)
		{
			return true;
		}
		const int32 Below = PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Interior, C.X, C.Y, C.Z - 1));
		const FBuildingPieceDef* BelowDef = FindPieceDef(Below);
		return BelowDef && BelowDef->Socket == EBuildSocket::Stairs;
	}
	case EBuildSocket::Roof:
		return PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X, C.Y, C.Z + 1)) != INDEX_NONE;
	case EBuildSocket::Stairs:
	{
		const FIntPoint Dir = BMDetail::StairsDirection(Placement.Rotation);
		return PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X, C.Y, C.Z + 1)) != INDEX_NONE ||
			PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X + Dir.X, C.Y + Dir.Y, C.Z + 1)) != INDEX_NONE;
	}
	case EBuildSocket::GroundOnly:
		// Un fuego o un bancal no van sobre un suelo construido.
		return PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X, C.Y, C.Z)) != INDEX_NONE;
	default:
		return false;
	}
}

void FBuildingModel::CollectSupports(EBuildSocket Socket, const FBuildingPlacement& Placement, TArray<FSupportLink>& OutLinks) const
{
	const int32 B = Placement.BaseId;
	const FIntVector& C = Placement.Cell;

	auto AddIf = [this, &OutLinks](uint64 Slot, bool bVertical, std::initializer_list<EBuildSocket> Accepted)
	{
		const int32 Owner = PieceAtSlot(Slot);
		const FBuildingPieceDef* Def = FindPieceDef(Owner);
		if (!Def)
		{
			return;
		}
		for (const EBuildSocket Candidate : Accepted)
		{
			if (Def->Socket == Candidate)
			{
				OutLinks.Add({Owner, bVertical});
				return;
			}
		}
	};

	switch (Socket)
	{
	case EBuildSocket::Floor:
	{
		// Pilotes en sus esquinas, paredes del piso de abajo en sus lados, suelos vecinos en voladizo.
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, C.X, C.Y, C.Z), true, {EBuildSocket::Pillar});
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, C.X + 1, C.Y, C.Z), true, {EBuildSocket::Pillar});
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, C.X, C.Y + 1, C.Z), true, {EBuildSocket::Pillar});
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, C.X + 1, C.Y + 1, C.Z), true, {EBuildSocket::Pillar});
		BMDetail::FEdgeRef Edges[4];
		BMDetail::EdgesOfCell(C.X, C.Y, Edges);
		for (const BMDetail::FEdgeRef& Edge : Edges)
		{
			AddIf(BMDetail::MakeSlot(B, Edge.Kind, Edge.X, Edge.Y, C.Z - 1), true, {EBuildSocket::Wall, EBuildSocket::Door});
		}
		for (const FIntPoint& N : BMDetail::CellNeighbours)
		{
			AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X + N.X, C.Y + N.Y, C.Z), false, {EBuildSocket::Floor});
		}
		break;
	}
	case EBuildSocket::Wall:
	case EBuildSocket::Door:
	{
		BMDetail::ESlotKind Kind;
		int32 X, Y;
		BMDetail::EdgeOf(Placement, Kind, X, Y);
		FIntPoint CellA, CellB, CornerA, CornerB;
		BMDetail::CellsBesideEdge(Kind, X, Y, CellA, CellB);
		BMDetail::CornersOfEdge(Kind, X, Y, CornerA, CornerB);
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, CellA.X, CellA.Y, C.Z), true, {EBuildSocket::Floor});
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, CellB.X, CellB.Y, C.Z), true, {EBuildSocket::Floor});
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, CornerA.X, CornerA.Y, C.Z), true, {EBuildSocket::Pillar});
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Corner, CornerB.X, CornerB.Y, C.Z), true, {EBuildSocket::Pillar});
		AddIf(BMDetail::MakeSlot(B, Kind, X, Y, C.Z - 1), true, {EBuildSocket::Wall, EBuildSocket::Door});
		BMDetail::FEdgeRef Touching[6];
		BMDetail::EdgesTouchingEdge(Kind, X, Y, Touching);
		for (const BMDetail::FEdgeRef& Edge : Touching)
		{
			AddIf(BMDetail::MakeSlot(B, Edge.Kind, Edge.X, Edge.Y, C.Z), false, {EBuildSocket::Wall, EBuildSocket::Door});
		}
		break;
	}
	case EBuildSocket::Roof:
	{
		BMDetail::FEdgeRef Edges[4];
		BMDetail::EdgesOfCell(C.X, C.Y, Edges);
		for (const BMDetail::FEdgeRef& Edge : Edges)
		{
			AddIf(BMDetail::MakeSlot(B, Edge.Kind, Edge.X, Edge.Y, C.Z), true, {EBuildSocket::Wall, EBuildSocket::Door});
		}
		for (const FIntPoint& N : BMDetail::CellNeighbours)
		{
			AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Roof, C.X + N.X, C.Y + N.Y, C.Z), false, {EBuildSocket::Roof});
		}
		break;
	}
	case EBuildSocket::Stairs:
	case EBuildSocket::Furniture:
		AddIf(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, C.X, C.Y, C.Z), true, {EBuildSocket::Floor});
		break;
	case EBuildSocket::Pillar:
	case EBuildSocket::GroundOnly:
	default:
		break;
	}
}

float FBuildingModel::EvaluateStability(const FBuildingPieceDef& Def, const FBuildingPlacement& Placement, bool bGroundContact,
	bool& bOutHasSupport) const
{
	bOutHasSupport = false;
	float Best = -1.0f;
	if (bGroundContact && BMDetail::CanTouchGround(Def.Socket))
	{
		bOutHasSupport = true;
		Best = 1.0f;
	}
	if (BMDetail::NeedsGround(Def.Socket))
	{
		return Best;
	}
	const FBuildingTierTuning Tuning = TuningForTierOrder(Catalog.TierOrderOf(Def));
	TArray<FSupportLink> Links;
	CollectSupports(Def.Socket, Placement, Links);
	for (const FSupportLink& Link : Links)
	{
		const float* Support = Stability.Find(Link.PieceId);
		if (!Support || *Support <= MinStability)
		{
			continue;
		}
		bOutHasSupport = true;
		const float Candidate = *Support - (Link.bVertical ? Tuning.VerticalLoss : Tuning.HorizontalLoss);
		Best = FMath::Max(Best, Candidate);
	}
	return Best;
}

bool FBuildingModel::HasPieceOfDefInBase(int32 BaseId, FName DefId) const
{
	return Pieces.ContainsByPredicate([BaseId, &DefId](const FBuildingPieceState& Piece)
	{
		return Piece.Placement.BaseId == BaseId && Piece.DefId == DefId;
	});
}

TArray<FName> FBuildingModel::RequiredTools(const FBuildingPieceDef& Def) const
{
	TArray<FName> Tools;
	if (const FBuildingTierDef* Tier = Catalog.FindTier(Def.Tier))
	{
		BMDetail::AddUniqueTools(Tier->RequiresTools, Tools);
	}
	BMDetail::AddUniqueTools(Def.Tools, Tools);
	return Tools;
}

bool FBuildingModel::HasRoofOver(EBuildSocket Socket, const FBuildingPlacement& Placement) const
{
	const int32 B = Placement.BaseId;
	const FIntVector& C = Placement.Cell;
	// A cubierto: un techo del mismo piso o un suelo del piso de arriba sobre la celda.
	auto Covered = [this, B](int32 X, int32 Y, int32 Z)
	{
		return PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Roof, X, Y, Z)) != INDEX_NONE ||
			PieceAtSlot(BMDetail::MakeSlot(B, BMDetail::ESlotKind::Center, X, Y, Z + 1)) != INDEX_NONE;
	};
	switch (Socket)
	{
	case EBuildSocket::Roof:
	case EBuildSocket::Pillar:
		return false;
	case EBuildSocket::Wall:
	case EBuildSocket::Door:
	{
		BMDetail::ESlotKind Kind;
		int32 X, Y;
		BMDetail::EdgeOf(Placement, Kind, X, Y);
		FIntPoint CellA, CellB;
		BMDetail::CellsBesideEdge(Kind, X, Y, CellA, CellB);
		return Covered(CellA.X, CellA.Y, C.Z) || Covered(CellB.X, CellB.Y, C.Z);
	}
	default:
		return Covered(C.X, C.Y, C.Z);
	}
}

// --- Colocación -------------------------------------------------------------

EBuildFailReason FBuildingModel::CanPlace(const FBuildingPlaceRequest& Request, float* OutStability) const
{
	if (OutStability)
	{
		*OutStability = 0.0f;
	}
	const FBuildingPieceDef* Def = Catalog.FindPiece(Request.DefId);
	if (!Def)
	{
		return EBuildFailReason::UnknownPiece;
	}
	if (Request.Placement.Rotation < 0 || Request.Placement.Rotation > 3)
	{
		return EBuildFailReason::InvalidRotation;
	}
	if (!FindBase(Request.Placement.BaseId))
	{
		return EBuildFailReason::UnknownBase;
	}
	if (FMath::Abs(Request.Placement.Cell.Z) > MaxLevels)
	{
		return EBuildFailReason::NoSupport;
	}

	TArray<uint64> Slots;
	CollectOccupiedSlots(Def->Socket, Request.Placement, Slots);
	for (const uint64 Slot : Slots)
	{
		if (PieceAtSlot(Slot) != INDEX_NONE)
		{
			return EBuildFailReason::Occupied;
		}
	}
	if (IsBlocked(Def->Socket, Request.Placement))
	{
		return EBuildFailReason::Occupied;
	}
	if (BMDetail::NeedsGround(Def->Socket) && !Request.bGroundContact)
	{
		return EBuildFailReason::NeedsGround;
	}

	bool bHasSupport = false;
	const float Candidate = EvaluateStability(*Def, Request.Placement, Request.bGroundContact, bHasSupport);
	if (!bHasSupport)
	{
		return EBuildFailReason::NoSupport;
	}
	if (Candidate <= MinStability + BMDetail::StabilityEpsilon)
	{
		return EBuildFailReason::Unstable;
	}

	for (const FName& Required : Def->RequiresPieces)
	{
		if (!HasPieceOfDefInBase(Request.Placement.BaseId, Required))
		{
			return EBuildFailReason::MissingRequiredPiece;
		}
	}

	if (OutStability)
	{
		*OutStability = Candidate;
	}
	return EBuildFailReason::None;
}

EBuildFailReason FBuildingModel::CanAfford(FName DefId, const TMap<FName, int32>& Inventory, const TSet<FName>& HeldTools,
	TArray<FBuildingCost>* OutMissing, TArray<FName>* OutMissingTools) const
{
	const FBuildingPieceDef* Def = Catalog.FindPiece(DefId);
	if (!Def)
	{
		return EBuildFailReason::UnknownPiece;
	}
	bool bToolsOk = true;
	for (const FName& Tool : RequiredTools(*Def))
	{
		if (!HeldTools.Contains(Tool))
		{
			bToolsOk = false;
			if (OutMissingTools)
			{
				OutMissingTools->Add(Tool);
			}
		}
	}
	const bool bMaterialsOk = BMDetail::HasMaterials(Def->Cost, Inventory, OutMissing);
	if (!bToolsOk)
	{
		return EBuildFailReason::MissingTools;
	}
	return bMaterialsOk ? EBuildFailReason::None : EBuildFailReason::MissingMaterials;
}

EBuildFailReason FBuildingModel::TryPlace(const FBuildingPlaceRequest& Request, TMap<FName, int32>& Inventory,
	const TSet<FName>& HeldTools, int32& OutPieceId)
{
	OutPieceId = INDEX_NONE;
	const EBuildFailReason PlaceResult = CanPlace(Request);
	if (PlaceResult != EBuildFailReason::None)
	{
		return PlaceResult;
	}
	const EBuildFailReason AffordResult = CanAfford(Request.DefId, Inventory, HeldTools);
	if (AffordResult != EBuildFailReason::None)
	{
		return AffordResult;
	}

	const FBuildingPieceDef& Def = *Catalog.FindPiece(Request.DefId);
	BMDetail::ConsumeMaterials(Def.Cost, Inventory);

	FBuildingPieceState Piece;
	Piece.Id = NextPieceId++;
	Piece.DefId = Def.Id;
	Piece.Placement = Request.Placement;
	Piece.Integrity = Def.Integrity;
	Piece.bGroundContact = Request.bGroundContact && BMDetail::CanTouchGround(Def.Socket);
	AddPieceInternal(Piece, Def);
	RecomputeStability();
	OutPieceId = Piece.Id;
	return EBuildFailReason::None;
}

float FBuildingModel::GetBuildMinutes(FName DefId, float WorkEfficiency) const
{
	const FBuildingPieceDef* Def = Catalog.FindPiece(DefId);
	return Def ? Def->BuildMinutes / FMath::Max(WorkEfficiency, 0.1f) : 0.0f;
}

void FBuildingModel::AddPieceInternal(const FBuildingPieceState& Piece, const FBuildingPieceDef& Def)
{
	PieceIndex.Add(Piece.Id, Pieces.Add(Piece));
	TArray<uint64> Slots;
	CollectOccupiedSlots(Def.Socket, Piece.Placement, Slots);
	for (const uint64 Slot : Slots)
	{
		SlotOwners.Add(Slot, Piece.Id);
	}
}

void FBuildingModel::RemovePieceInternal(int32 PieceId)
{
	const int32* Index = PieceIndex.Find(PieceId);
	if (!Index)
	{
		return;
	}
	const FBuildingPieceState Piece = Pieces[*Index];
	if (const FBuildingPieceDef* Def = Catalog.FindPiece(Piece.DefId))
	{
		TArray<uint64> Slots;
		CollectOccupiedSlots(Def->Socket, Piece.Placement, Slots);
		for (const uint64 Slot : Slots)
		{
			SlotOwners.Remove(Slot);
		}
	}
	// RemoveAt conserva el orden por id (determinismo al recorrer).
	Pieces.RemoveAt(*Index);
	Stability.Remove(PieceId);
	RebuildIndex();
}

void FBuildingModel::RebuildIndex()
{
	PieceIndex.Empty();
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		PieceIndex.Add(Pieces[Index].Id, Index);
	}
}

void FBuildingModel::RecomputeStability()
{
	// Relajación en orden de id: se parte de lo que toca el terreno (1) y cada
	// pieza toma el mejor apoyo menos su pérdida. Como toda pérdida es positiva,
	// los ciclos (suelos vecinos que se apoyan entre sí) no pueden inventarse
	// estabilidad y el bucle termina en como mucho N pasadas.
	Stability.Empty();
	for (const FBuildingPieceState& Piece : Pieces)
	{
		const FBuildingPieceDef* Def = FindPieceDef(Piece.Id);
		const bool bGrounded = Def && Piece.bGroundContact && BMDetail::CanTouchGround(Def->Socket);
		Stability.Add(Piece.Id, bGrounded ? 1.0f : -1.0f);
	}
	TArray<FSupportLink> Links;
	for (int32 Pass = 0; Pass <= Pieces.Num(); ++Pass)
	{
		bool bChanged = false;
		for (const FBuildingPieceState& Piece : Pieces)
		{
			const FBuildingPieceDef* Def = FindPieceDef(Piece.Id);
			if (!Def || BMDetail::NeedsGround(Def->Socket))
			{
				continue;
			}
			const FBuildingTierTuning Tuning = TuningForTierOrder(Catalog.TierOrderOf(*Def));
			float& Current = Stability.FindChecked(Piece.Id);
			Links.Reset();
			CollectSupports(Def->Socket, Piece.Placement, Links);
			for (const FSupportLink& Link : Links)
			{
				const float Support = Stability.FindRef(Link.PieceId);
				if (Support <= MinStability)
				{
					continue;
				}
				const float Candidate = Support - (Link.bVertical ? Tuning.VerticalLoss : Tuning.HorizontalLoss);
				if (Candidate > Current + BMDetail::StabilityEpsilon)
				{
					Current = Candidate;
					bChanged = true;
				}
			}
		}
		if (!bChanged)
		{
			break;
		}
	}
}

TArray<int32> FBuildingModel::CollapseUnsupported()
{
	TArray<int32> Fallen;
	for (const FBuildingPieceState& Piece : Pieces)
	{
		if (Stability.FindRef(Piece.Id) <= MinStability + BMDetail::StabilityEpsilon)
		{
			Fallen.Add(Piece.Id);
		}
	}
	for (const int32 Id : Fallen)
	{
		RemovePieceInternal(Id);
	}
	if (Fallen.Num() > 0)
	{
		RecomputeStability();
	}
	return Fallen;
}

TArray<int32> FBuildingModel::RemovePiece(int32 PieceId)
{
	if (!PieceIndex.Contains(PieceId))
	{
		return {};
	}
	RemovePieceInternal(PieceId);
	RecomputeStability();
	return CollapseUnsupported();
}

// --- Integridad y temporales ------------------------------------------------

FBuildingChangeResult FBuildingModel::Tick(float DeltaHours, const FBuildingWeather& Weather)
{
	FBuildingChangeResult Result;
	// Un paso NaN (de un guardado o un reloj roto) dejaría Max(NaN, 0) = 0 y rompería todas las piezas.
	if (!FMath::IsFinite(DeltaHours) || DeltaHours <= 0.0f)
	{
		return Result;
	}
	const float Rain = FMath::Clamp(Weather.Rain, 0.0f, 1.0f);
	const float Storm = FMath::Max(Weather.StormCategory, 0.0f);

	for (FBuildingPieceState& Piece : Pieces)
	{
		const FBuildingPieceDef* Def = Catalog.FindPiece(Piece.DefId);
		if (!Def)
		{
			continue;
		}
		const FBuildingTierTuning Tuning = TuningForTierOrder(Catalog.TierOrderOf(*Def));
		const bool bSheltered = HasRoofOver(Def->Socket, Piece.Placement);

		// Desgaste lento: la lluvia pudre lo orgánico que no está a cubierto.
		const float RainFactor = bSheltered ? 1.0f : 1.0f + (Tuning.RainDecayMultiplier - 1.0f) * Rain;
		float Damage = Def->Integrity * Tuning.DecayPerDay * RainFactor * (DeltaHours / 24.0f);

		// Temporal: lo bien apoyado aguanta su categoría sin daño; lo mal apoyado, una menos.
		if (Storm > 0.0f)
		{
			const float StabilityNow = Stability.FindRef(Piece.Id);
			const int32 Tolerated = Def->MaxCycloneCategory - (StabilityNow < WellSupportedStability ? 1 : 0);
			const float Excess = Storm - static_cast<float>(Tolerated);
			if (Excess > 0.0f)
			{
				Damage += Def->Integrity * StormDamagePerExcessHour * Excess * DeltaHours *
					(bSheltered ? ShelteredStormFactor : 1.0f);
			}
		}

		Piece.Integrity = FMath::Max(Piece.Integrity - Damage, 0.0f);
		if (Piece.Integrity <= 0.0f)
		{
			Result.Destroyed.Add(Piece.Id);
		}
	}

	for (const int32 Id : Result.Destroyed)
	{
		RemovePieceInternal(Id);
	}
	if (Result.Destroyed.Num() > 0)
	{
		RecomputeStability();
		Result.Collapsed = CollapseUnsupported();
	}
	return Result;
}

FBuildingChangeResult FBuildingModel::ApplyDamage(int32 PieceId, float Points)
{
	FBuildingChangeResult Result;
	const int32* Index = PieceIndex.Find(PieceId);
	// Daño NaN: Max(NaN, 0) = 0 rompería la pieza de un golpe.
	if (!Index || !FMath::IsFinite(Points) || Points <= 0.0f)
	{
		return Result;
	}
	FBuildingPieceState& Piece = Pieces[*Index];
	Piece.Integrity = FMath::Max(Piece.Integrity - Points, 0.0f);
	if (Piece.Integrity <= 0.0f)
	{
		Result.Destroyed.Add(PieceId);
		Result.Collapsed = RemovePiece(PieceId);
	}
	return Result;
}

TArray<FBuildingCost> FBuildingModel::GetRepairCost(int32 PieceId) const
{
	TArray<FBuildingCost> Cost;
	const FBuildingPieceState* Piece = FindPiece(PieceId);
	const FBuildingPieceDef* Def = FindPieceDef(PieceId);
	if (!Piece || !Def || Def->Integrity <= 0.0f)
	{
		return Cost;
	}
	const float Missing = (Def->Integrity - Piece->Integrity) / Def->Integrity;
	if (Missing <= 0.01f)
	{
		return Cost;
	}
	// Reparar es volver a atar o reemplazar lo roto (biblia §2.4): una fracción del coste
	// proporcional a lo que falta, al menos una unidad de cada material.
	const float Fraction = RepairCostFraction * Missing;
	for (const FBuildingCost& Entry : Def->Cost)
	{
		const int32 Count = FMath::Max(1, FMath::CeilToInt32(Entry.Count * Fraction - 1.0e-4f));
		Cost.Add({Entry.Item, Count});
	}
	return Cost;
}

EBuildFailReason FBuildingModel::TryRepair(int32 PieceId, TMap<FName, int32>& Inventory, const TSet<FName>& HeldTools)
{
	const int32* Index = PieceIndex.Find(PieceId);
	const FBuildingPieceDef* Def = FindPieceDef(PieceId);
	if (!Index || !Def)
	{
		return EBuildFailReason::UnknownInstance;
	}
	const TArray<FBuildingCost> Cost = GetRepairCost(PieceId);
	if (Cost.Num() == 0)
	{
		return EBuildFailReason::NothingToRepair;
	}
	for (const FName& Tool : RequiredTools(*Def))
	{
		if (!HeldTools.Contains(Tool))
		{
			return EBuildFailReason::MissingTools;
		}
	}
	if (!BMDetail::HasMaterials(Cost, Inventory, nullptr))
	{
		return EBuildFailReason::MissingMaterials;
	}
	BMDetail::ConsumeMaterials(Cost, Inventory);
	Pieces[*Index].Integrity = Def->Integrity;
	return EBuildFailReason::None;
}

// --- Reaparición ------------------------------------------------------------

bool FBuildingModel::SetLit(int32 PieceId, bool bLit)
{
	const int32* Index = PieceIndex.Find(PieceId);
	if (!Index)
	{
		return false;
	}
	Pieces[*Index].bLit = bLit;
	return true;
}

TArray<int32> FBuildingModel::GetRespawnPoints() const
{
	TArray<int32> Out;
	for (const FBuildingPieceState& Piece : Pieces)
	{
		const FBuildingPieceDef* Def = Catalog.FindPiece(Piece.DefId);
		if (Def && Def->bRespawnPoint && Piece.bLit && Piece.Integrity > 0.0f)
		{
			Out.Add(Piece.Id);
		}
	}
	return Out;
}

bool FBuildingModel::FindNearestRespawnPoint(const FVector& From, int32& OutPieceId, FVector& OutLocation) const
{
	OutPieceId = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (const int32 Id : GetRespawnPoints())
	{
		const FBuildingPieceState* Piece = FindPiece(Id);
		const FBuildingPieceDef* Def = FindPieceDef(Id);
		FVector Location;
		float Yaw = 0.0f;
		if (!Piece || !Def || !PlacementToWorld(Piece->Placement, Def->Socket, Location, Yaw))
		{
			continue;
		}
		const double Distance = FVector::DistSquared(Location, From);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			OutPieceId = Id;
			OutLocation = Location;
		}
	}
	return OutPieceId != INDEX_NONE;
}

// --- Consultas --------------------------------------------------------------

const FBuildingPieceState* FBuildingModel::FindPiece(int32 PieceId) const
{
	const int32* Index = PieceIndex.Find(PieceId);
	return Index ? &Pieces[*Index] : nullptr;
}

const FBuildingPieceDef* FBuildingModel::FindPieceDef(int32 PieceId) const
{
	const FBuildingPieceState* Piece = FindPiece(PieceId);
	if (!Piece)
	{
		return nullptr;
	}
	const int32* DefIndex = DefLookup.Find(Piece->DefId);
	return DefIndex ? &Catalog.Pieces[*DefIndex] : nullptr;
}

float FBuildingModel::GetStability(int32 PieceId) const
{
	return FMath::Max(Stability.FindRef(PieceId), 0.0f);
}

bool FBuildingModel::IsSheltered(int32 PieceId) const
{
	const FBuildingPieceState* Piece = FindPiece(PieceId);
	const FBuildingPieceDef* Def = FindPieceDef(PieceId);
	return Piece && Def && HasRoofOver(Def->Socket, Piece->Placement);
}

FString FBuildingModel::ResolveMeshPath(const FBuildingPieceDef& Def) const
{
	if (!Def.Mesh.IsNone())
	{
		// Mallas propias de Tools/Blender/props/building_kit.py (grupo «Construccion»).
		const FString Name = Def.Mesh.ToString();
		return FString::Printf(TEXT("/Game/Generated/Meshes/Construccion/%s.%s"), *Name, *Name);
	}

	// Kit modular (Tools/Blender/props/kit_construccion.py): SM_Kit_<Material>_<Pieza> en Kit<Material>.
	static const TCHAR* Materials[] = {TEXT("Palm"), TEXT("Bamboo"), TEXT("Wood"), TEXT("Stone")};
	static const TCHAR* Groups[] = {TEXT("KitPalma"), TEXT("KitBambu"), TEXT("KitMadera"), TEXT("KitPiedra")};
	const TCHAR* KitPiece = nullptr;
	switch (Def.Socket)
	{
	case EBuildSocket::Pillar: KitPiece = TEXT("Foundation"); break;
	case EBuildSocket::Floor: KitPiece = TEXT("Floor"); break;
	case EBuildSocket::Wall: KitPiece = TEXT("Wall"); break;
	case EBuildSocket::Door: KitPiece = TEXT("WallDoor"); break;
	case EBuildSocket::Roof: KitPiece = TEXT("RoofGable"); break;
	case EBuildSocket::Stairs: KitPiece = TEXT("Stairs"); break;
	default: break;
	}
	const FBuildingTierDef* Tier = Catalog.FindTier(Def.Tier);
	if (!KitPiece || !Tier || Tier->Order < 0 || Tier->Order > 3)
	{
		return FString();
	}
	const FString Name = FString::Printf(TEXT("SM_Kit_%s_%s"), Materials[Tier->Order], KitPiece);
	return FString::Printf(TEXT("/Game/Generated/Meshes/%s/%s.%s"), Groups[Tier->Order], *Name, *Name);
}

// --- Guardado ---------------------------------------------------------------

FBuildingSaveState FBuildingModel::SaveState() const
{
	FBuildingSaveState State;
	State.NextPieceId = NextPieceId;
	State.NextBaseId = NextBaseId;
	State.Bases = Bases;
	State.Pieces = Pieces;
	return State;
}

bool FBuildingModel::LoadState(const FBuildingSaveState& State, int32* OutDropped)
{
	Bases.Reset();
	Pieces.Reset();
	PieceIndex.Empty();
	SlotOwners.Empty();
	Stability.Empty();
	NextPieceId = FMath::Clamp(State.NextPieceId, 1, MaxPieceId + 1);
	NextBaseId = FMath::Clamp(State.NextBaseId, 1, MaxBaseId + 1);

	for (const FBuildingBaseState& Base : State.Bases)
	{
		// La clave de hueco guarda la base en 16 bits: una base 65537 compartiría huecos con la 1.
		if (Base.Id >= 1 && Base.Id <= MaxBaseId && !FindBase(Base.Id))
		{
			Bases.Add(Base);
			NextBaseId = FMath::Max(NextBaseId, Base.Id + 1);
		}
	}

	TArray<FBuildingPieceState> Sorted = State.Pieces;
	Sorted.StableSort([](const FBuildingPieceState& A, const FBuildingPieceState& B) { return A.Id < B.Id; });

	int32 Dropped = 0;
	TArray<uint64> Slots;
	for (const FBuildingPieceState& Piece : Sorted)
	{
		// Ni siquiera las descartadas deben reutilizar su id (otros sistemas pueden recordarlo).
		if (Piece.Id >= 1 && Piece.Id <= MaxPieceId)
		{
			NextPieceId = FMath::Max(NextPieceId, Piece.Id + 1);
		}
	}
	for (const FBuildingPieceState& Piece : Sorted)
	{
		const FBuildingPieceDef* Def = Catalog.FindPiece(Piece.DefId);
		// Celdas fuera de la clave de hueco (X = 65536 compartía hueco con X = 0) o de los pisos
		// que admite CanPlace, e ids con los que Id + 1 desbordaría.
		const FIntVector& Cell = Piece.Placement.Cell;
		bool bValid = Def && FindBase(Piece.Placement.BaseId) && !PieceIndex.Contains(Piece.Id) &&
			Piece.Placement.Rotation >= 0 && Piece.Placement.Rotation <= 3 &&
			Piece.Id >= 1 && Piece.Id <= MaxPieceId &&
			Cell.X >= -MaxCellCoord && Cell.X <= MaxCellCoord && Cell.Y >= -MaxCellCoord && Cell.Y <= MaxCellCoord &&
			Cell.Z >= -MaxLevels && Cell.Z <= MaxLevels;
		if (bValid)
		{
			Slots.Reset();
			CollectOccupiedSlots(Def->Socket, Piece.Placement, Slots);
			for (const uint64 Slot : Slots)
			{
				bValid &= PieceAtSlot(Slot) == INDEX_NONE;
			}
		}
		if (!bValid)
		{
			++Dropped;
			continue;
		}
		FBuildingPieceState Copy = Piece;
		Copy.Integrity = FMath::Clamp(Copy.Integrity, 0.0f, Def->Integrity);
		AddPieceInternal(Copy, *Def);
	}
	RecomputeStability();
	if (OutDropped)
	{
		*OutDropped = Dropped;
	}
	return Dropped == 0;
}
