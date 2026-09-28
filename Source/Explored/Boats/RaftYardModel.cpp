#include "Boats/RaftYardModel.h"

const TCHAR* LexToString(ERaftJointKind Kind)
{
	switch (Kind)
	{
	case ERaftJointKind::Fiber: return TEXT("Fiber");
	case ERaftJointKind::Rope: return TEXT("Rope");
	case ERaftJointKind::Nails: return TEXT("Nails");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ELaunchSurface Surface)
{
	switch (Surface)
	{
	case ELaunchSurface::Sand: return TEXT("Sand");
	case ELaunchSurface::WetSand: return TEXT("WetSand");
	case ELaunchSurface::Grass: return TEXT("Grass");
	case ELaunchSurface::Rock: return TEXT("Rock");
	case ELaunchSurface::PlankRamp: return TEXT("PlankRamp");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(ERaftYardState State)
{
	switch (State)
	{
	case ERaftYardState::Ashore: return TEXT("Ashore");
	case ERaftYardState::Afloat: return TEXT("Afloat");
	default: return TEXT("Unknown");
	}
}

namespace RaftYardDetail
{
	// Por bits (FMath::IsFinite): con matemáticas rápidas `V == V` no descarta los NaN.
	bool IsFiniteValue(float V) { return FMath::IsFinite(V); }

	/** Unión-búsqueda con compresión de caminos para los grupos de piezas unidas. */
	int32 Find(TArray<int32>& Parent, int32 I)
	{
		while (Parent[I] != I)
		{
			Parent[I] = Parent[Parent[I]];
			I = Parent[I];
		}
		return I;
	}

	void Box(const FHullPiece& Piece, FVector& OutMin, FVector& OutMax)
	{
		const FVector Half = FHullAssemblyModel::EffectiveSizeCm(Piece) * 0.5;
		OutMin = Piece.CenterCm - Half;
		OutMax = Piece.CenterCm + Half;
	}
}

// ----------------------------------------------------------------------------- datos

const FRaftJointSpec& FRaftYardModel::JointSpec(ERaftJointKind Kind)
{
	// Cordel de fibra: se gasta el doble que la cuerda. Clavos: aguantan el roce, pero un
	// golpe seco raja la madera alrededor (menos tenaces que la cuerda, que cede y vuelve).
	static const FRaftJointSpec Specs[] = {
		{ ERaftJointKind::Fiber, TEXT("cordel"), 1, 1, 0.6f, 0.7f, 0.5f },
		{ ERaftJointKind::Rope, TEXT("cuerda"), 1, 1, 1.0f, 1.2f, 0.5f },
		{ ERaftJointKind::Nails, TEXT("clavo"), 2, 2, 2.0f, 0.7f, 1.0f },
	};
	const int32 Index = FMath::Clamp(static_cast<int32>(Kind), 0, static_cast<int32>(ERaftJointKind::Count) - 1);
	return Specs[Index];
}

const FLaunchSurfaceSpec& FRaftYardModel::SurfaceSpec(ELaunchSurface Surface)
{
	// Madera sobre el suelo. El desgaste está calibrado para que arrastrar la balsa de 6
	// troncos 10 m por arena se lleve ~30 % de unas uniones de cuerda, y por roca las rompa.
	static const FLaunchSurfaceSpec Specs[] = {
		{ ELaunchSurface::Sand, 0.55f, 0.12f },
		{ ELaunchSurface::WetSand, 0.45f, 0.08f },
		{ ELaunchSurface::Grass, 0.40f, 0.05f },
		{ ELaunchSurface::Rock, 0.50f, 0.40f },
		{ ELaunchSurface::PlankRamp, 0.30f, 0.01f },
	};
	const int32 Index = FMath::Clamp(static_cast<int32>(Surface), 0, static_cast<int32>(ELaunchSurface::Count) - 1);
	return Specs[Index];
}

void FRaftDamageReport::Append(const FRaftDamageReport& Other)
{
	JointsDamaged += Other.JointsDamaged;
	JointsBroken += Other.JointsBroken;
	Released.Append(Other.Released);
}

// ----------------------------------------------------------------------------- camino

float FLaunchPath::TotalLengthCm() const
{
	float Total = 0.0f;
	for (const FLaunchSegment& Segment : Segments)
	{
		Total += FMath::Max(Segment.LengthCm, 0.0f);
	}
	return Total;
}

int32 FLaunchPath::SegmentIndexAt(float S) const
{
	if (Segments.Num() == 0)
	{
		return INDEX_NONE;
	}
	float Start = 0.0f;
	for (int32 I = 0; I < Segments.Num(); ++I)
	{
		const float End = Start + FMath::Max(Segments[I].LengthCm, 0.0f);
		// En un borde exacto manda el tramo siguiente (el que empieza ahí).
		if (S < End)
		{
			return I;
		}
		Start = End;
	}
	return Segments.Num() - 1;
}

float FLaunchPath::GroundZAt(float S) const
{
	float Z = static_cast<float>(StartCm.Z);
	float Start = 0.0f;
	for (const FLaunchSegment& Segment : Segments)
	{
		const float Length = FMath::Max(Segment.LengthCm, 0.0f);
		if (S <= Start + Length)
		{
			const float T = Length > 0.0f ? FMath::Clamp((S - Start) / Length, 0.0f, 1.0f) : 1.0f;
			return Z - Segment.DropCm * T;
		}
		Z -= Segment.DropCm;
		Start += Length;
	}
	return Z;
}

float FLaunchPath::SlopeRadAt(float S) const
{
	const int32 Index = SegmentIndexAt(S);
	if (Index == INDEX_NONE)
	{
		return 0.0f;
	}
	const FLaunchSegment& Segment = Segments[Index];
	return FMath::Atan2(Segment.DropCm, FMath::Max(Segment.LengthCm, 1.0f));
}

FVector FLaunchPath::WorldAt(float S) const
{
	const float Yaw = FMath::DegreesToRadians(YawDeg);
	return FVector(StartCm.X + FMath::Cos(Yaw) * S, StartCm.Y + FMath::Sin(Yaw) * S, GroundZAt(S));
}

// ----------------------------------------------------------------------------- construcción

int32 FRaftYardModel::AddPiece(const FHullPiece& Piece)
{
	Invalidate();
	return Hull.AddPiece(Piece);
}

bool FRaftYardModel::RemovePiece(int32 Index)
{
	if (!Hull.RemovePiece(Index))
	{
		return false;
	}
	Invalidate();
	for (int32 J = Joints.Num() - 1; J >= 0; --J)
	{
		FRaftJoint& Joint = Joints[J];
		if (Joint.PieceA == Index || Joint.PieceB == Index)
		{
			Joints.RemoveAt(J);
			continue;
		}
		Joint.PieceA -= Joint.PieceA > Index ? 1 : 0;
		Joint.PieceB -= Joint.PieceB > Index ? 1 : 0;
	}
	return true;
}

float FRaftYardModel::GapCm(const FHullPiece& A, const FHullPiece& B)
{
	FVector MinA, MaxA, MinB, MaxB;
	RaftYardDetail::Box(A, MinA, MaxA);
	RaftYardDetail::Box(B, MinB, MaxB);
	const FVector Gap(
		FMath::Max3(0.0, MinA.X - MaxB.X, MinB.X - MaxA.X),
		FMath::Max3(0.0, MinA.Y - MaxB.Y, MinB.Y - MaxA.Y),
		FMath::Max3(0.0, MinA.Z - MaxB.Z, MinB.Z - MaxA.Z));
	return static_cast<float>(Gap.Size());
}

int32 FRaftYardModel::AddJoint(int32 PieceA, int32 PieceB, ERaftJointKind Kind)
{
	const TArray<FHullPiece>& Pieces = Hull.GetPieces();
	if (!Pieces.IsValidIndex(PieceA) || !Pieces.IsValidIndex(PieceB) || PieceA == PieceB
		|| Kind >= ERaftJointKind::Count)
	{
		return INDEX_NONE;
	}
	for (const FRaftJoint& Joint : Joints)
	{
		if ((Joint.PieceA == PieceA && Joint.PieceB == PieceB) || (Joint.PieceA == PieceB && Joint.PieceB == PieceA))
		{
			return INDEX_NONE;
		}
	}
	if (GapCm(Pieces[PieceA], Pieces[PieceB]) > MaxJointGapCm)
	{
		return INDEX_NONE;
	}
	FRaftJoint Joint;
	Joint.PieceA = FMath::Min(PieceA, PieceB);
	Joint.PieceB = FMath::Max(PieceA, PieceB);
	Joint.Kind = Kind;
	return Joints.Add(Joint);
}

void FRaftYardModel::AddLoad(const FHullLoad& Load)
{
	Hull.AddLoad(Load);
	Invalidate();
}

void FRaftYardModel::ClearLoads()
{
	Hull.ClearLoads();
	Invalidate();
}

const FHullHydrostatics& FRaftYardModel::GetHydrostatics() const
{
	if (bHydroDirty)
	{
		CachedHydro = Hull.Evaluate();
		bHydroDirty = false;
	}
	return CachedHydro;
}

float FRaftYardModel::TotalMassKg() const
{
	double Mass = 0.0;
	for (const FHullPiece& Piece : Hull.GetPieces())
	{
		Mass += FHullAssemblyModel::PieceMassKg(Piece);
	}
	for (const FHullLoad& Load : Hull.GetLoads())
	{
		Mass += FMath::Max(Load.MassKg, 0.0f);
	}
	return static_cast<float>(Mass);
}

float FRaftYardModel::BottomZ() const
{
	// La quilla es la de las piezas con volumen, como en FHullAssemblyModel::Evaluate: unos
	// remos o una pala colgados por debajo no apoyan el casco en el suelo, y si contaran
	// ninguna unión de los troncos sufriría el roce.
	double Bottom = TNumericLimits<double>::Max();
	for (const FHullPiece& Piece : Hull.GetPieces())
	{
		if (FHullAssemblyModel::Spec(Piece.Type).bBuoyant)
		{
			FVector Min, Max;
			RaftYardDetail::Box(Piece, Min, Max);
			Bottom = FMath::Min(Bottom, Min.Z);
		}
	}
	return static_cast<float>(Bottom);
}

void FRaftYardModel::HullBoundsX(double& OutMinX, double& OutMaxX) const
{
	OutMinX = 0.0;
	OutMaxX = 0.0;
	bool bFirst = true;
	for (const FHullPiece& Piece : Hull.GetPieces())
	{
		FVector Min, Max;
		RaftYardDetail::Box(Piece, Min, Max);
		OutMinX = bFirst ? Min.X : FMath::Min(OutMinX, Min.X);
		OutMaxX = bFirst ? Max.X : FMath::Max(OutMaxX, Max.X);
		bFirst = false;
	}
}

float FRaftYardModel::HullLengthCm() const
{
	double MinX, MaxX;
	HullBoundsX(MinX, MaxX);
	return static_cast<float>(MaxX - MinX);
}

// ----------------------------------------------------------------------------- integridad

float FRaftYardModel::Integrity01() const
{
	if (Joints.Num() == 0)
	{
		return 1.0f;
	}
	double Sum = 0.0;
	for (const FRaftJoint& Joint : Joints)
	{
		Sum += FMath::Clamp(Joint.Health01, 0.0f, 1.0f);
	}
	return static_cast<float>(Sum / Joints.Num());
}

bool FRaftYardModel::IsBottomPiece(int32 Index) const
{
	const TArray<FHullPiece>& Pieces = Hull.GetPieces();
	if (!Pieces.IsValidIndex(Index) || !FHullAssemblyModel::Spec(Pieces[Index].Type).bBuoyant)
	{
		return false;
	}
	FVector Min, Max;
	RaftYardDetail::Box(Pieces[Index], Min, Max);
	return Min.Z <= BottomZ() + BottomContactToleranceCm;
}

FRaftDamageReport FRaftYardModel::ApplyJointDamage(const TArray<float>& Damage01)
{
	FRaftDamageReport Report;
	bool bBroke = false;
	for (int32 J = 0; J < Joints.Num() && J < Damage01.Num(); ++J)
	{
		FRaftJoint& Joint = Joints[J];
		const float Amount = Damage01[J];
		if (!RaftYardDetail::IsFiniteValue(Amount) || Amount <= 0.0f || Joint.IsBroken())
		{
			continue;
		}
		++Report.JointsDamaged;
		Joint.Health01 = FMath::Max(Joint.Health01 - Amount, 0.0f);
		if (Joint.IsBroken())
		{
			Joint.Health01 = 0.0f;
			++Report.JointsBroken;
			bBroke = true;
		}
	}
	if (bBroke)
	{
		Report.Released = ReleaseLoosePieces();
	}
	return Report;
}

FRaftDamageReport FRaftYardModel::ApplyScrapeWork(float WorkNm, ELaunchSurface Surface)
{
	if (!RaftYardDetail::IsFiniteValue(WorkNm) || WorkNm <= 0.0f)
	{
		return FRaftDamageReport();
	}
	// Solo sufren las uniones con alguna pieza apoyada en el suelo; se reparten el desgaste.
	TArray<int32> Bottom;
	for (int32 J = 0; J < Joints.Num(); ++J)
	{
		const FRaftJoint& Joint = Joints[J];
		if (!Joint.IsBroken() && (IsBottomPiece(Joint.PieceA) || IsBottomPiece(Joint.PieceB)))
		{
			Bottom.Add(J);
		}
	}
	if (Bottom.Num() == 0)
	{
		return FRaftDamageReport();
	}
	const float Wear = SurfaceSpec(Surface).WearPerKNm * WorkNm / 1000.0f / Bottom.Num();
	TArray<float> Damage;
	Damage.SetNumZeroed(Joints.Num());
	for (int32 J : Bottom)
	{
		Damage[J] = Wear / JointSpec(Joints[J].Kind).WearToughness;
	}
	return ApplyJointDamage(Damage);
}

FRaftDamageReport FRaftYardModel::ApplyImpact(float SpeedCmS, const FVector2D& DirectionHull)
{
	const float ExcessMS = (SpeedCmS - FBoatModel::SafeImpactSpeedCmS) / 100.0f;
	const TArray<FHullPiece>& Pieces = Hull.GetPieces();
	if (!RaftYardDetail::IsFiniteValue(SpeedCmS) || ExcessMS <= 0.0f || Pieces.Num() == 0 || Joints.Num() == 0)
	{
		return FRaftDamageReport();
	}
	FVector2D Dir = DirectionHull.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(1.0, 0.0);
	}

	// Punto de impacto: donde el rayo desde el centro de la caja del casco sale por su borde.
	FVector BoxMin(TNumericLimits<double>::Max()), BoxMax(-TNumericLimits<double>::Max());
	for (const FHullPiece& Piece : Pieces)
	{
		FVector Min, Max;
		RaftYardDetail::Box(Piece, Min, Max);
		BoxMin = BoxMin.ComponentMin(Min);
		BoxMax = BoxMax.ComponentMax(Max);
	}
	const FVector2D Center((BoxMin.X + BoxMax.X) * 0.5, (BoxMin.Y + BoxMax.Y) * 0.5);
	const FVector2D Half(FMath::Max((BoxMax.X - BoxMin.X) * 0.5, 1.0), FMath::Max((BoxMax.Y - BoxMin.Y) * 0.5, 1.0));
	const double TX = FMath::Abs(Dir.X) > UE_KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Dir.X) : TNumericLimits<double>::Max();
	const double TY = FMath::Abs(Dir.Y) > UE_KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Dir.Y) : TNumericLimits<double>::Max();
	const FVector2D Hit = Center + Dir * FMath::Min(TX, TY);
	// Alcance del golpe: media eslora (y nunca menos de medio metro).
	const double Reach = FMath::Max(FMath::Max(Half.X, Half.Y), 50.0);

	const float Budget = ExcessMS * ImpactDamagePerMS;
	TArray<float> Damage;
	Damage.SetNumZeroed(Joints.Num());
	for (int32 J = 0; J < Joints.Num(); ++J)
	{
		const FRaftJoint& Joint = Joints[J];
		const FVector Mid = (Pieces[Joint.PieceA].CenterCm + Pieces[Joint.PieceB].CenterCm) * 0.5;
		const double Distance = (FVector2D(Mid.X, Mid.Y) - Hit).Size();
		const double Weight = FMath::Max(0.0, 1.0 - Distance / Reach);
		Damage[J] = static_cast<float>(Budget * Weight) / JointSpec(Joint.Kind).ImpactToughness;
	}
	return ApplyJointDamage(Damage);
}

FRaftDamageReport FRaftYardModel::DamageJoint(int32 JointIndex, float Amount01)
{
	if (!Joints.IsValidIndex(JointIndex) || !RaftYardDetail::IsFiniteValue(Amount01) || Amount01 <= 0.0f)
	{
		return FRaftDamageReport();
	}
	TArray<float> Damage;
	Damage.SetNumZeroed(Joints.Num());
	Damage[JointIndex] = Amount01;
	return ApplyJointDamage(Damage);
}

bool FRaftYardModel::RepairJoint(int32 JointIndex)
{
	if (!Joints.IsValidIndex(JointIndex) || Joints[JointIndex].Health01 >= 1.0f)
	{
		return false;
	}
	FRaftJoint& Joint = Joints[JointIndex];
	Joint.Health01 = FMath::Min(Joint.Health01 + JointSpec(Joint.Kind).RepairPerAction, 1.0f);
	return true;
}

TArray<FHullPiece> FRaftYardModel::ReleaseLoosePieces()
{
	TArray<FHullPiece> Released;
	const TArray<FHullPiece>& Pieces = Hull.GetPieces();
	const int32 Count = Pieces.Num();
	if (Count <= 1)
	{
		return Released;
	}

	TArray<int32> Parent;
	Parent.SetNumUninitialized(Count);
	for (int32 I = 0; I < Count; ++I)
	{
		Parent[I] = I;
	}
	for (const FRaftJoint& Joint : Joints)
	{
		if (!Joint.IsBroken())
		{
			const int32 A = RaftYardDetail::Find(Parent, Joint.PieceA);
			const int32 B = RaftYardDetail::Find(Parent, Joint.PieceB);
			// La raíz es siempre el índice menor: el resultado no depende del orden de las uniones.
			Parent[FMath::Max(A, B)] = FMath::Min(A, B);
		}
	}

	// Grupo principal: el de más masa; a igualdad, el que contiene la pieza de índice menor.
	TArray<double> GroupMass;
	GroupMass.SetNumZeroed(Count);
	for (int32 I = 0; I < Count; ++I)
	{
		GroupMass[RaftYardDetail::Find(Parent, I)] += FHullAssemblyModel::PieceMassKg(Pieces[I]);
	}
	int32 Main = RaftYardDetail::Find(Parent, 0);
	for (int32 I = 0; I < Count; ++I)
	{
		if (Parent[I] == I && GroupMass[I] > GroupMass[Main] + 1e-6)
		{
			Main = I;
		}
	}

	for (int32 I = Count - 1; I >= 0; --I)
	{
		if (RaftYardDetail::Find(Parent, I) != Main)
		{
			Released.Insert(Hull.GetPieces()[I], 0);
			RemovePiece(I);
		}
	}
	return Released;
}

// ----------------------------------------------------------------------------- botadura

void FRaftYardModel::PlaceOnPath(const FLaunchPath& InPath, float InCenterS)
{
	using RaftYardDetail::IsFiniteValue;
	Path = InPath;
	// El camino llega del mundo (trazas, marea): lo no finito toma el valor por defecto
	// para no contagiar NaN a la posición y a la flotación de la balsa.
	const FLaunchPath Defaults;
	if (!FMath::IsFinite(Path.StartCm.X) || !FMath::IsFinite(Path.StartCm.Y) || !FMath::IsFinite(Path.StartCm.Z))
	{
		Path.StartCm = Defaults.StartCm;
	}
	Path.YawDeg = IsFiniteValue(Path.YawDeg) ? Path.YawDeg : Defaults.YawDeg;
	Path.WaterLevelZCm = IsFiniteValue(Path.WaterLevelZCm) ? Path.WaterLevelZCm : Defaults.WaterLevelZCm;
	for (FLaunchSegment& Segment : Path.Segments)
	{
		Segment.LengthCm = IsFiniteValue(Segment.LengthCm) ? Segment.LengthCm : 0.0f;
		Segment.DropCm = IsFiniteValue(Segment.DropCm) ? Segment.DropCm : 0.0f;
	}
	State = ERaftYardState::Ashore;
	CenterS = FMath::Clamp(RaftYardDetail::IsFiniteValue(InCenterS) ? InCenterS : 0.0f, 0.0f, Path.TotalLengthCm());
	VelocityCmS = 0.0f;
	PendingTimeS = 0.0f;
}

void FRaftYardModel::SetAfloat()
{
	State = ERaftYardState::Afloat;
	VelocityCmS = 0.0f;
	PendingTimeS = 0.0f;
}

bool FRaftYardModel::PlaceRoller(float S)
{
	if (State != ERaftYardState::Ashore || !RaftYardDetail::IsFiniteValue(S) || S < 0.0f || S > Path.TotalLengthCm())
	{
		return false;
	}
	Rollers.Add(S);
	return true;
}

bool FRaftYardModel::TakeRoller(int32 Index)
{
	if (!Rollers.IsValidIndex(Index))
	{
		return false;
	}
	const float HalfLength = HullLengthCm() * 0.5f;
	if (State == ERaftYardState::Ashore && FMath::Abs(Rollers[Index] - CenterS) <= HalfLength)
	{
		return false;
	}
	Rollers.RemoveAt(Index);
	return true;
}

bool FRaftYardModel::IsOnRollers() const
{
	if (State != ERaftYardState::Ashore || Hull.GetPieces().Num() == 0)
	{
		return false;
	}
	const float HalfLength = HullLengthCm() * 0.5f;
	bool bAft = false;
	bool bFore = false;
	for (float R : Rollers)
	{
		const float Offset = R - CenterS;
		if (FMath::Abs(Offset) <= HalfLength)
		{
			bAft |= Offset <= 0.0f;
			bFore |= Offset >= 0.0f;
		}
	}
	return bAft && bFore;
}

float FRaftYardModel::WaterSupport01() const
{
	if (State == ERaftYardState::Afloat)
	{
		return 1.0f;
	}
	const float Depth = Path.WaterDepthAt(CenterS);
	if (!RaftYardDetail::IsFiniteValue(Depth) || Depth <= 0.0f)
	{
		return 0.0f;
	}
	const FHullHydrostatics& Hydro = GetHydrostatics();
	if (Hydro.IsAfloat() || Hydro.Verdict == EHullVerdict::Capsizes)
	{
		return FMath::Clamp(Depth / FMath::Max(Hydro.DraftCm, 0.1f), 0.0f, 1.0f);
	}
	// No flota (se hunde o no hay nada que flote): el agua solo quita lo que puede desplazar.
	const float Mass = FMath::Max(TotalMassKg(), 1.0f);
	const float Depth01 = FMath::Clamp(Depth / FMath::Max(Hydro.DeckHeightCm, 1.0f), 0.0f, 1.0f);
	return FMath::Clamp(Depth01 * Hydro.MaxBuoyancyKg / Mass, 0.0f, 0.99f);
}

float FRaftYardModel::RequiredPushForceN() const
{
	if (State != ERaftYardState::Ashore || Hull.GetPieces().Num() == 0)
	{
		return 0.0f;
	}
	const float Mass = TotalMassKg();
	const float Slope = Path.SlopeRadAt(CenterS);
	const float Normal = Mass * Gravity * FMath::Cos(Slope) * (1.0f - WaterSupport01());
	const int32 Segment = Path.SegmentIndexAt(CenterS);
	const float Mu = IsOnRollers() ? RollingResistance
		: (Segment == INDEX_NONE ? SurfaceSpec(ELaunchSurface::Sand).Friction : SurfaceSpec(Path.Segments[Segment].Surface).Friction);
	return FMath::Max(0.0f, Mu * Normal - Mass * Gravity * FMath::Sin(Slope));
}

FRaftPushReport FRaftYardModel::Push(float PushForceN, float DeltaSeconds)
{
	FRaftPushReport Report;
	if (State != ERaftYardState::Ashore)
	{
		Report.WaterSupport01 = 1.0f;
		return Report;
	}
	if (!RaftYardDetail::IsFiniteValue(DeltaSeconds) || DeltaSeconds <= 0.0f || !RaftYardDetail::IsFiniteValue(PushForceN)
		|| Hull.GetPieces().Num() == 0)
	{
		Report.WaterSupport01 = WaterSupport01();
		Report.bOnRollers = IsOnRollers();
		return Report;
	}
	PendingTimeS += FMath::Min(DeltaSeconds, MaxFrameS);
	const int32 Steps = FMath::FloorToInt((PendingTimeS + 1e-6f) / FixedStepS);
	PendingTimeS = FMath::Max(PendingTimeS - Steps * FixedStepS, 0.0f);
	Report.bOnRollers = IsOnRollers();
	for (int32 I = 0; I < Steps && State == ERaftYardState::Ashore; ++I)
	{
		Substep(FixedStepS, PushForceN, Report);
	}
	Report.WaterSupport01 = WaterSupport01();
	return Report;
}

void FRaftYardModel::Substep(float H, float PushForceN, FRaftPushReport& Report)
{
	const float Mass = FMath::Max(TotalMassKg(), 1.0f);
	const float Slope = Path.SlopeRadAt(CenterS);
	const float Support = WaterSupport01();
	const float Normal = Mass * Gravity * FMath::Cos(Slope) * (1.0f - Support);
	const bool bRolling = IsOnRollers();
	const int32 SegmentIndex = Path.SegmentIndexAt(CenterS);
	const ELaunchSurface Surface = SegmentIndex == INDEX_NONE ? ELaunchSurface::Sand : Path.Segments[SegmentIndex].Surface;
	const float Mu = bRolling ? RollingResistance : SurfaceSpec(Surface).Friction;
	const float FrictionMax = Mu * Normal;
	// Fuerzas a lo largo del camino (N): empujón y peso por la pendiente.
	const float Drive = PushForceN + Mass * Gravity * FMath::Sin(Slope);
	const float V = VelocityCmS / 100.0f;

	float NewV = V;
	if (V == 0.0f)
	{
		// Rozamiento estático: no arranca si no lo vence.
		if (FMath::Abs(Drive) > FrictionMax)
		{
			NewV = (Drive - FMath::Sign(Drive) * FrictionMax) / Mass * H;
		}
	}
	else
	{
		const float WaterDrag = -ShallowWaterDragPerS * Support * Mass * V;
		NewV = V + (Drive - FMath::Sign(V) * FrictionMax + WaterDrag) / Mass * H;
		// El rozamiento puede parar la balsa, nunca darle la vuelta.
		if (NewV * V < 0.0f)
		{
			NewV = 0.0f;
		}
	}

	const float Total = Path.TotalLengthCm();
	float Moved = NewV * 100.0f * H;
	if (CenterS + Moved > Total)
	{
		Moved = Total - CenterS;
		NewV = 0.0f;
		Report.bAtPathEnd = true;
	}
	else if (CenterS + Moved < 0.0f)
	{
		Moved = -CenterS;
		NewV = 0.0f;
	}

	// Los rodillos de debajo ruedan sin deslizar: avanzan la mitad que el casco.
	if (bRolling)
	{
		const float HalfLength = HullLengthCm() * 0.5f;
		for (float& R : Rollers)
		{
			if (FMath::Abs(R - CenterS) <= HalfLength)
			{
				R = FMath::Clamp(R + Moved * 0.5f, 0.0f, Total);
			}
		}
	}
	CenterS += Moved;
	VelocityCmS = NewV * 100.0f;
	Report.MovedCm += Moved;
	Report.bOnRollers = bRolling;

	if (!bRolling && Moved != 0.0f)
	{
		Report.Damage.Append(ApplyScrapeWork(Normal * FMath::Abs(Moved) / 100.0f, Surface));
	}

	// Una balsa que vuelca también sale al agua: FBoatModel la vuelca con su ángulo de estabilidad nula.
	const FHullHydrostatics& Hydro = GetHydrostatics();
	if (WaterSupport01() >= 1.0f && (Hydro.IsAfloat() || Hydro.Verdict == EHullVerdict::Capsizes))
	{
		State = ERaftYardState::Afloat;
		Report.bLaunched = true;
		// Una pieza que nadie ató se va flotando al tocar el agua.
		Report.Damage.Released.Append(ReleaseLoosePieces());
	}
}

FRaftHullSaveData FRaftYardModel::ToHullSaveData() const
{
	FRaftHullSaveData Data;
	Data.Pieces = Hull.GetPieces();
	Data.Joints = Joints;
	return Data;
}

FRaftYardModel FRaftYardModel::FromHullSaveData(const FRaftHullSaveData& Data, int32* OutDiscarded)
{
	FRaftYardModel Yard;
	int32 Discarded = 0;

	// Índice guardado → índice en el casco rehecho (INDEX_NONE si se descarta).
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Data.Pieces.Num());
	const auto InRange = [](const FVector& V)
	{
		return FMath::Abs(V.X) <= MaxSavedExtentCm && FMath::Abs(V.Y) <= MaxSavedExtentCm && FMath::Abs(V.Z) <= MaxSavedExtentCm;
	};
	for (int32 I = 0; I < Data.Pieces.Num(); ++I)
	{
		const FHullPiece& Piece = Data.Pieces[I];
		// InRange descarta también los NaN (toda comparación con NaN es falsa), pero con
		// matemáticas rápidas no se puede contar con eso: AddPiece los filtra por bits.
		const bool bValid = Piece.Type < EHullPieceType::Count && InRange(Piece.CenterCm) && InRange(Piece.SizeCm)
			&& Yard.Hull.GetPieces().Num() < MaxSavedPieces;
		Remap[I] = bValid ? Yard.AddPiece(Piece) : INDEX_NONE;
		Discarded += Remap[I] == INDEX_NONE ? 1 : 0;
	}

	for (const FRaftJoint& Saved : Data.Joints)
	{
		const int32 A = Remap.IsValidIndex(Saved.PieceA) ? Remap[Saved.PieceA] : INDEX_NONE;
		const int32 B = Remap.IsValidIndex(Saved.PieceB) ? Remap[Saved.PieceB] : INDEX_NONE;
		// AddJoint rechaza la pieza inexistente, la repetida, consigo misma, el tipo desconocido y el hueco.
		const int32 Index = (A == INDEX_NONE || B == INDEX_NONE) ? INDEX_NONE : Yard.AddJoint(A, B, Saved.Kind);
		if (Index == INDEX_NONE)
		{
			++Discarded;
			continue;
		}
		Yard.Joints[Index].Health01 = RaftYardDetail::IsFiniteValue(Saved.Health01) ? FMath::Clamp(Saved.Health01, 0.0f, 1.0f) : 0.0f;
	}

	if (OutDiscarded)
	{
		*OutDiscarded = Discarded;
	}
	return Yard;
}

FBoatDefinition FRaftYardModel::ToBoatDefinition() const
{
	return Hull.ToBoatDefinition(GetHydrostatics());
}

FBoatModel FRaftYardModel::MakeBoat() const
{
	const FHullHydrostatics& Hydro = GetHydrostatics();
	FVector Location = Path.WorldAt(CenterS);
	if (Path.Segments.Num() > 0)
	{
		Location.Z = Path.WaterLevelZCm - Hydro.DraftCm;
	}
	FBoatModel Boat(ToBoatDefinition(), Location, Path.YawDeg);
	const float Yaw = FMath::DegreesToRadians(Path.YawDeg);
	Boat.SetVelocityCmS(FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)) * static_cast<double>(VelocityCmS));
	Boat.ApplyDamage(FMath::Min(HullDamage01(), 0.99f));
	return Boat;
}
