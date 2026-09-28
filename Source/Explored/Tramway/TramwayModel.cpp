#include "Tramway/TramwayModel.h"

namespace TramwayDetail
{
	/** Coordenadas admitidas al cargar: acotan ficheros manipulados y evitan desbordes. */
	constexpr int64 MaxCoord = 1000000;

	int32 Sign(double V)
	{
		return V > 0.0 ? 1 : (V < 0.0 ? -1 : 0);
	}

	double DistToSegment(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector AB = B - A;
		const double Len2 = FVector::DotProduct(AB, AB);
		const double T = Len2 > 0.0 ? FMath::Clamp(FVector::DotProduct(P - A, AB) / Len2, 0.0, 1.0) : 0.0;
		return FVector::Dist(P, A + AB * T);
	}

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

	bool ReadNode(const FSaveValue& List, int32 Index, FIntVector& Out)
	{
		return ReadInt(List, Index, -MaxCoord, MaxCoord, Out.X)
			&& ReadInt(List, Index + 1, -MaxCoord, MaxCoord, Out.Y)
			&& ReadInt(List, Index + 2, -MaxCoord, MaxCoord, Out.Z);
	}

	void AddNode(FSaveValue& List, const FIntVector& Node)
	{
		List.Add(FSaveValue::MakeInt(Node.X));
		List.Add(FSaveValue::MakeInt(Node.Y));
		List.Add(FSaveValue::MakeInt(Node.Z));
	}
}

bool FMineCart::operator==(const FMineCart& Other) const
{
	return From == Other.From && To == Other.To && S == Other.S && V == Other.V && LoadKg == Other.LoadKg
		&& Derailed == Other.Derailed;
}

int32 FTramwayModel::FNode::NumEdges() const
{
	int32 N = 0;
	for (int32 D = 0; D < 4; ++D)
	{
		N += HasEdge(D) ? 1 : 0;
	}
	return N;
}

bool FTramwayModel::FNode::operator==(const FNode& O) const
{
	for (int32 D = 0; D < 4; ++D)
	{
		if (Rise[D] != O.Rise[D])
		{
			return false;
		}
	}
	return DamagedMask == O.DamagedMask && Lever == O.Lever && bWinch == O.bWinch;
}

FTramwayModel::FTramwayModel(const FTramwaySettings& InSettings)
	: Settings(InSettings)
{
}

FIntVector FTramwayModel::DirOffset(ERailDir Dir)
{
	switch (Dir)
	{
	case ERailDir::PosX: return FIntVector(1, 0, 0);
	case ERailDir::PosY: return FIntVector(0, 1, 0);
	case ERailDir::NegX: return FIntVector(-1, 0, 0);
	case ERailDir::NegY: return FIntVector(0, -1, 0);
	default: return FIntVector(0, 0, 0);
	}
}

ERailDir FTramwayModel::Opposite(ERailDir Dir)
{
	return static_cast<ERailDir>((static_cast<int32>(Dir) + 2) % 4);
}

FVector FTramwayModel::NodePosition(const FIntVector& Node) const
{
	return Settings.Origin + FVector(Node.X * static_cast<double>(Settings.CellSize), Node.Y * static_cast<double>(Settings.CellSize),
		Node.Z * static_cast<double>(Settings.HeightStep));
}

FIntVector FTramwayModel::SnapNode(const FVector& World) const
{
	const FVector L = World - Settings.Origin;
	return FIntVector(FMath::RoundToInt32(L.X / Settings.CellSize), FMath::RoundToInt32(L.Y / Settings.CellSize),
		FMath::RoundToInt32(L.Z / Settings.HeightStep));
}

float FTramwayModel::MaxSlopeDegrees() const
{
	return FMath::RadiansToDegrees(FMath::Atan2(Settings.MaxRiseSteps * Settings.HeightStep, Settings.CellSize));
}

bool FTramwayModel::DirBetween(const FIntVector& A, const FIntVector& B, ERailDir& OutDir)
{
	for (int32 D = 0; D < 4; ++D)
	{
		const FIntVector O = DirOffset(static_cast<ERailDir>(D));
		if (B.X - A.X == O.X && B.Y - A.Y == O.Y)
		{
			OutDir = static_cast<ERailDir>(D);
			return true;
		}
	}
	return false;
}

bool FTramwayModel::LessNode(const FIntVector& A, const FIntVector& B)
{
	if (A.Z != B.Z)
	{
		return A.Z < B.Z;
	}
	if (A.Y != B.Y)
	{
		return A.Y < B.Y;
	}
	return A.X < B.X;
}

TArray<FIntVector> FTramwayModel::SortedNodes() const
{
	TArray<FIntVector> Keys;
	Nodes.GetKeys(Keys);
	Keys.Sort([](const FIntVector& A, const FIntVector& B) { return LessNode(A, B); });
	return Keys;
}

ERailPlaceResult FTramwayModel::CanPlace(const FIntVector& A, const FIntVector& B) const
{
	if (A == B)
	{
		return ERailPlaceResult::Degenerate;
	}
	ERailDir Dir;
	if (!DirBetween(A, B, Dir))
	{
		return ERailPlaceResult::NotAdjacent;
	}
	if (FMath::Abs(B.Z - A.Z) > Settings.MaxRiseSteps)
	{
		return ERailPlaceResult::TooSteep;
	}
	const FNode* NA = Nodes.Find(A);
	const FNode* NB = Nodes.Find(B);
	if ((NA && NA->HasEdge(static_cast<int32>(Dir))) || (NB && NB->HasEdge(static_cast<int32>(Opposite(Dir)))))
	{
		return ERailPlaceResult::Occupied;
	}
	return ERailPlaceResult::Ok;
}

ERailPlaceResult FTramwayModel::Place(const FIntVector& A, const FIntVector& B)
{
	const ERailPlaceResult Result = CanPlace(A, B);
	if (Result != ERailPlaceResult::Ok)
	{
		return Result;
	}
	ERailDir Dir;
	DirBetween(A, B, Dir);
	const int8 Rise = static_cast<int8>(B.Z - A.Z);
	Nodes.FindOrAdd(A).Rise[static_cast<int32>(Dir)] = Rise;
	Nodes.FindOrAdd(B).Rise[static_cast<int32>(Opposite(Dir))] = static_cast<int8>(-Rise);
	return ERailPlaceResult::Ok;
}

bool FTramwayModel::Remove(const FIntVector& A, const FIntVector& B)
{
	if (!HasSegment(A, B))
	{
		return false;
	}
	ERailDir Dir;
	DirBetween(A, B, Dir);
	const int32 Ends[2] = { static_cast<int32>(Dir), static_cast<int32>(Opposite(Dir)) };
	const FIntVector Keys[2] = { A, B };
	for (int32 I = 0; I < 2; ++I)
	{
		FNode& Node = Nodes.FindChecked(Keys[I]);
		Node.Rise[Ends[I]] = NoEdge;
		Node.DamagedMask &= static_cast<uint8>(~(1u << Ends[I]));
		// La palanca solo tiene sentido en un cambio y apuntando a un tramo que exista.
		if (Node.NumEdges() < 3 || Node.Lever == Ends[I])
		{
			Node.Lever = -1;
		}
		if (Node.IsUnused())
		{
			Nodes.Remove(Keys[I]);
		}
	}
	return true;
}

bool FTramwayModel::HasSegment(const FIntVector& A, const FIntVector& B) const
{
	ERailDir Dir;
	if (!DirBetween(A, B, Dir))
	{
		return false;
	}
	const FNode* Node = Nodes.Find(A);
	return Node && Node->HasEdge(static_cast<int32>(Dir)) && Node->Rise[static_cast<int32>(Dir)] == B.Z - A.Z;
}

bool FTramwayModel::Neighbor(const FIntVector& Node, ERailDir Dir, FIntVector& OutOther) const
{
	const FNode* N = Nodes.Find(Node);
	if (!N || Dir >= ERailDir::Count || !N->HasEdge(static_cast<int32>(Dir)))
	{
		return false;
	}
	OutOther = Node + DirOffset(Dir) + FIntVector(0, 0, N->Rise[static_cast<int32>(Dir)]);
	return true;
}

int32 FTramwayModel::NumSegments() const
{
	int32 Total = 0;
	for (const auto& Pair : Nodes)
	{
		Total += Pair.Value.NumEdges();
	}
	return Total / 2;
}

int32 FTramwayModel::NumEdges(const FIntVector& Node) const
{
	const FNode* N = Nodes.Find(Node);
	return N ? N->NumEdges() : 0;
}

ERailNodeKind FTramwayModel::NodeKind(const FIntVector& Node) const
{
	const FNode* N = Nodes.Find(Node);
	const int32 Count = N ? N->NumEdges() : 0;
	if (Count == 0)
	{
		return ERailNodeKind::None;
	}
	if (Count == 1)
	{
		return ERailNodeKind::End;
	}
	if (Count >= 3)
	{
		return ERailNodeKind::Switch;
	}
	const bool bStraightX = N->HasEdge(0) && N->HasEdge(2);
	const bool bStraightY = N->HasEdge(1) && N->HasEdge(3);
	return (bStraightX || bStraightY) ? ERailNodeKind::Straight : ERailNodeKind::Curve;
}

double FTramwayModel::SegmentLength(const FIntVector& A, const FIntVector& B) const
{
	if (!HasSegment(A, B))
	{
		return 0.0;
	}
	const double Rise = (B.Z - A.Z) * static_cast<double>(Settings.HeightStep);
	return FMath::Sqrt(static_cast<double>(Settings.CellSize) * Settings.CellSize + Rise * Rise);
}

double FTramwayModel::SegmentSin(const FIntVector& A, const FIntVector& B) const
{
	const double Length = SegmentLength(A, B);
	return Length > 0.0 ? (B.Z - A.Z) * static_cast<double>(Settings.HeightStep) / Length : 0.0;
}

bool FTramwayModel::SetSwitch(const FIntVector& Node, ERailDir Dir)
{
	FNode* N = Nodes.Find(Node);
	if (!N || Dir >= ERailDir::Count || N->NumEdges() < 3 || !N->HasEdge(static_cast<int32>(Dir)))
	{
		return false;
	}
	N->Lever = static_cast<int8>(Dir);
	return true;
}

ERailDir FTramwayModel::GetSwitch(const FIntVector& Node) const
{
	const FNode* N = Nodes.Find(Node);
	return (N && N->Lever >= 0) ? static_cast<ERailDir>(N->Lever) : ERailDir::Count;
}

bool FTramwayModel::ExitFor(const FIntVector& Node, ERailDir Travel, ERailDir& OutExit) const
{
	const FNode* N = Nodes.Find(Node);
	if (!N || Travel >= ERailDir::Count)
	{
		return false;
	}
	const int32 Entry = static_cast<int32>(Opposite(Travel));
	if (N->Lever >= 0 && N->Lever != Entry && N->HasEdge(N->Lever))
	{
		OutExit = static_cast<ERailDir>(N->Lever);
		return true;
	}
	if (N->HasEdge(static_cast<int32>(Travel)))
	{
		OutExit = Travel;
		return true;
	}
	for (int32 D = 0; D < 4; ++D)
	{
		if (D != Entry && N->HasEdge(D))
		{
			OutExit = static_cast<ERailDir>(D);
			return true;
		}
	}
	return false;
}

bool FTramwayModel::AddWinch(const FIntVector& Node)
{
	FNode* N = Nodes.Find(Node);
	if (!N || N->NumEdges() == 0 || N->bWinch)
	{
		return false;
	}
	N->bWinch = true;
	return true;
}

bool FTramwayModel::RemoveWinch(const FIntVector& Node)
{
	FNode* N = Nodes.Find(Node);
	if (!N || !N->bWinch)
	{
		return false;
	}
	N->bWinch = false;
	if (N->IsUnused())
	{
		Nodes.Remove(Node);
	}
	return true;
}

bool FTramwayModel::HasWinch(const FIntVector& Node) const
{
	const FNode* N = Nodes.Find(Node);
	return N && N->bWinch;
}

void FTramwayModel::SetDamaged(const FIntVector& A, const FIntVector& B, bool bDamaged)
{
	ERailDir Dir;
	DirBetween(A, B, Dir);
	const int32 Ends[2] = { static_cast<int32>(Dir), static_cast<int32>(Opposite(Dir)) };
	const FIntVector Keys[2] = { A, B };
	for (int32 I = 0; I < 2; ++I)
	{
		FNode& Node = Nodes.FindChecked(Keys[I]);
		const uint8 Bit = static_cast<uint8>(1u << Ends[I]);
		Node.DamagedMask = bDamaged ? (Node.DamagedMask | Bit) : (Node.DamagedMask & static_cast<uint8>(~Bit));
	}
}

TArray<TPair<FIntVector, FIntVector>> FTramwayModel::DamageInSphere(const FVector& Center, float Radius)
{
	TArray<TPair<FIntVector, FIntVector>> Out;
	if (!(Radius > 0.0f) || !FMath::IsFinite(Center.X) || !FMath::IsFinite(Center.Y) || !FMath::IsFinite(Center.Z))
	{
		return Out;
	}
	for (const FIntVector& Key : SortedNodes())
	{
		const FNode& Node = Nodes.FindChecked(Key);
		// Cada tramo una vez: desde el nodo del que sale hacia +X o +Y.
		for (int32 D = 0; D < 2; ++D)
		{
			if (!Node.HasEdge(D) || (Node.DamagedMask & (1u << D)) != 0)
			{
				continue;
			}
			FIntVector Other;
			Neighbor(Key, static_cast<ERailDir>(D), Other);
			if (TramwayDetail::DistToSegment(Center, NodePosition(Key), NodePosition(Other)) < Radius)
			{
				Out.Add(TPair<FIntVector, FIntVector>(Key, Other));
			}
		}
	}
	for (const auto& Pair : Out)
	{
		SetDamaged(Pair.Key, Pair.Value, true);
	}
	return Out;
}

bool FTramwayModel::IsDamaged(const FIntVector& A, const FIntVector& B) const
{
	ERailDir Dir;
	if (!HasSegment(A, B) || !DirBetween(A, B, Dir))
	{
		return false;
	}
	return (Nodes.FindChecked(A).DamagedMask & (1u << static_cast<int32>(Dir))) != 0;
}

bool FTramwayModel::Repair(const FIntVector& A, const FIntVector& B)
{
	if (!IsDamaged(A, B))
	{
		return false;
	}
	SetDamaged(A, B, false);
	return true;
}

int32 FTramwayModel::NumDamaged() const
{
	int32 Total = 0;
	for (const auto& Pair : Nodes)
	{
		for (int32 D = 0; D < 4; ++D)
		{
			Total += (Pair.Value.DamagedMask & (1u << D)) != 0 ? 1 : 0;
		}
	}
	return Total / 2;
}

double FTramwayModel::CartMass(const FMineCart& Cart) const
{
	return static_cast<double>(Settings.CartMassKg) + FMath::Max(0.0f, Cart.LoadKg);
}

bool FTramwayModel::SetLoad(FMineCart& Cart, float LoadKg) const
{
	if (!(LoadKg >= 0.0f) || LoadKg > Settings.CapacityKg)
	{
		return false;
	}
	Cart.LoadKg = LoadKg;
	return true;
}

bool FTramwayModel::PlaceCart(FMineCart& Cart, const FIntVector& From, const FIntVector& To, double S) const
{
	const double Length = SegmentLength(From, To);
	if (Length <= 0.0 || !(S >= 0.0) || S > Length)
	{
		return false;
	}
	Cart.From = From;
	Cart.To = To;
	Cart.S = S;
	Cart.V = 0.0;
	Cart.Derailed = ECartDerailCause::None;
	return true;
}

double FTramwayModel::CurveSpeedLimit(const FMineCart& Cart) const
{
	const double Fill = Settings.CapacityKg > 0.0f ? FMath::Clamp(Cart.LoadKg / Settings.CapacityKg, 0.0f, 1.0f) : 0.0;
	const double CogHeight = Settings.EmptyCogHeight + (Settings.FullCogHeight - Settings.EmptyCogHeight) * Fill;
	const double LateralAccel = Settings.Gravity * Settings.HalfGauge / CogHeight;
	return FMath::Sqrt(LateralAccel * Settings.CurveRadius);
}

FVector FTramwayModel::CartPosition(const FMineCart& Cart) const
{
	const double Length = SegmentLength(Cart.From, Cart.To);
	const FVector A = NodePosition(Cart.From);
	if (Length <= 0.0)
	{
		return A;
	}
	return A + (NodePosition(Cart.To) - A) * FMath::Clamp(Cart.S / Length, 0.0, 1.0);
}

bool FTramwayModel::WinchDirection(const FMineCart& Cart, double& OutDistance, int32& OutSign) const
{
	const double Length = SegmentLength(Cart.From, Cart.To);
	if (Length <= 0.0)
	{
		return false;
	}
	// Dijkstra desde los dos extremos del tramo, acotado por la cuerda. Pocas decenas de nodos.
	struct FOpen
	{
		FIntVector Node;
		double Dist;
		int32 Sign;
	};
	TArray<FOpen> Open;
	TMap<FIntVector, double> Best;
	Open.Add({ Cart.To, Length - Cart.S, 1 });
	Open.Add({ Cart.From, Cart.S, -1 });
	while (Open.Num() > 0)
	{
		int32 Pick = 0;
		for (int32 I = 1; I < Open.Num(); ++I)
		{
			// Desempate determinista: menor distancia, luego delante antes que detrás.
			if (Open[I].Dist < Open[Pick].Dist || (Open[I].Dist == Open[Pick].Dist && Open[I].Sign > Open[Pick].Sign))
			{
				Pick = I;
			}
		}
		const FOpen Cur = Open[Pick];
		Open.RemoveAt(Pick);
		if (Cur.Dist > Settings.RopeLength)
		{
			return false;
		}
		const double* Seen = Best.Find(Cur.Node);
		if (Seen && *Seen <= Cur.Dist)
		{
			continue;
		}
		Best.Add(Cur.Node, Cur.Dist);
		if (HasWinch(Cur.Node))
		{
			OutDistance = Cur.Dist;
			OutSign = Cur.Sign;
			return true;
		}
		for (int32 D = 0; D < 4; ++D)
		{
			FIntVector Next;
			if (!Neighbor(Cur.Node, static_cast<ERailDir>(D), Next) || Best.Contains(Next))
			{
				continue;
			}
			// La cuerda no vuelve por el tramo del propio vagón: eso es el otro sentido.
			const bool bOwnSegment = (Cur.Node == Cart.From && Next == Cart.To) || (Cur.Node == Cart.To && Next == Cart.From);
			if (!bOwnSegment)
			{
				Open.Add({ Next, Cur.Dist + SegmentLength(Cur.Node, Next), Cur.Sign });
			}
		}
	}
	return false;
}

double FTramwayModel::Integrate(double V, double Mass, double SinSlope, double Mu, double Force) const
{
	const double Dt = Settings.SubstepSeconds();
	const double Cos = FMath::Sqrt(FMath::Max(0.0, 1.0 - SinSlope * SinSlope));
	const double External = -Mass * Settings.Gravity * SinSlope + Force;
	const double Friction = Mu * Mass * Settings.Gravity * Cos;
	if (V == 0.0)
	{
		// Rozamiento estático: si no vence, no se mueve (ni repta).
		if (FMath::Abs(External) <= Friction)
		{
			return 0.0;
		}
		return (External - TramwayDetail::Sign(External) * Friction) / Mass * Dt;
	}
	const double Next = V + (External - TramwayDetail::Sign(V) * Friction) / Mass * Dt;
	// Cambiar de sentido pasa siempre por el reposo: el rozamiento no puede empujar.
	return TramwayDetail::Sign(Next) != TramwayDetail::Sign(V) ? 0.0 : Next;
}

FCartStepResult FTramwayModel::Substep(FMineCart& Cart, const FCartControl& Control) const
{
	FCartStepResult R;
	if (Cart.IsDerailed())
	{
		return R;
	}
	const auto Derail = [&](ECartDerailCause Cause)
	{
		Cart.Derailed = Cause;
		Cart.V = 0.0;
		R.Derailed = Cause;
	};
	if (SegmentLength(Cart.From, Cart.To) <= 0.0)
	{
		Derail(ECartDerailCause::MissingTrack);
		return R;
	}
	if (IsDamaged(Cart.From, Cart.To))
	{
		Derail(ECartDerailCause::DamagedTrack);
		return R;
	}

	const double Mass = CartMass(Cart);
	const double Mu = Control.bBrake ? Settings.BrakeFriction : Settings.RollingResistance;
	const double SinSlope = SegmentSin(Cart.From, Cart.To);

	int32 PropSign = 0;
	double PropForce = 0.0;
	double PropTarget = 0.0;
	if (Control.Propulsion == ECartPropulsion::Push)
	{
		PropSign = Control.PushSign >= 0 ? 1 : -1;
		PropForce = Settings.PushForce;
		PropTarget = Settings.PushSpeed;
	}
	else if (Control.Propulsion == ECartPropulsion::Winch)
	{
		double Distance = 0.0;
		int32 Sign = 0;
		if (!WinchDirection(Cart, Distance, Sign))
		{
			R.bNoWinchInReach = true;
		}
		else if (Distance <= Settings.WinchStopDistance)
		{
			Cart.V = 0.0;
			R.bArrivedAtWinch = true;
			return R;
		}
		else
		{
			PropSign = Sign;
			PropForce = Settings.WinchForce;
			PropTarget = Settings.WinchSpeed;
		}
	}

	double V = Integrate(Cart.V, Mass, SinSlope, Mu, 0.0);
	if (PropSign != 0 && PropSign * V < PropTarget)
	{
		// Empujar o tirar solo acelera hasta la velocidad objetivo; la cuerda y los brazos no frenan.
		V = Integrate(Cart.V, Mass, SinSlope, Mu, PropSign * PropForce);
		if (PropSign * V > PropTarget)
		{
			V = PropSign * PropTarget;
		}
	}
	Cart.V = V;
	Cart.S += V * Settings.SubstepSeconds();
	R.Distance = FMath::Abs(V) * Settings.SubstepSeconds();

	for (;;)
	{
		const double Length = SegmentLength(Cart.From, Cart.To);
		const bool bForward = Cart.S > Length;
		if (!bForward && Cart.S >= 0.0)
		{
			break;
		}
		const FIntVector Node = bForward ? Cart.To : Cart.From;
		const FIntVector Behind = bForward ? Cart.From : Cart.To;
		ERailDir Travel;
		DirBetween(Behind, Node, Travel);
		ERailDir Exit;
		if (!ExitFor(Node, Travel, Exit))
		{
			Cart.S = bForward ? Length : 0.0;
			R.ImpactSpeed = FMath::Abs(Cart.V);
			if (R.ImpactSpeed > Settings.BufferMaxSpeed)
			{
				Derail(ECartDerailCause::Buffer);
			}
			else
			{
				Cart.V = 0.0;
				R.bStoppedAtBuffer = true;
			}
			break;
		}
		FIntVector Next;
		Neighbor(Node, Exit, Next);
		if (Exit != Travel && FMath::Abs(Cart.V) > CurveSpeedLimit(Cart))
		{
			Cart.S = bForward ? Length : 0.0;
			R.ImpactSpeed = FMath::Abs(Cart.V);
			Derail(ECartDerailCause::Curve);
			break;
		}
		if (IsDamaged(Node, Next))
		{
			Cart.S = bForward ? Length : 0.0;
			Derail(ECartDerailCause::DamagedTrack);
			break;
		}
		const double Excess = bForward ? Cart.S - Length : -Cart.S;
		if (bForward)
		{
			Cart.From = Node;
			Cart.To = Next;
			Cart.S = Excess;
		}
		else
		{
			// Marcha atrás: la parte delantera sigue mirando hacia el nodo que deja atrás.
			Cart.From = Next;
			Cart.To = Node;
			Cart.S = SegmentLength(Next, Node) - Excess;
		}
		++R.NodesCrossed;
	}
	return R;
}

FCartStepResult FTramwayModel::Step(FMineCart& Cart, const FCartControl& Control, double Dt, double& Accumulator) const
{
	FCartStepResult Total;
	if (!(Dt > 0.0) || !FMath::IsFinite(Dt))
	{
		return Total;
	}
	Accumulator += Dt * FMath::Max(1, Settings.SubstepsPerSecond);
	// Tolerancia de redondeo: 0,03 s × 120 = 3,5999… pasos no debe perder un paso por el camino.
	while (Accumulator >= 1.0 - 1e-6)
	{
		Accumulator -= 1.0;
		const FCartStepResult R = Substep(Cart, Control);
		Total.Distance += R.Distance;
		Total.NodesCrossed += R.NodesCrossed;
		Total.bStoppedAtBuffer |= R.bStoppedAtBuffer;
		Total.bNoWinchInReach |= R.bNoWinchInReach;
		Total.bArrivedAtWinch |= R.bArrivedAtWinch;
		if (R.ImpactSpeed > 0.0)
		{
			Total.ImpactSpeed = R.ImpactSpeed;
		}
		if (R.Derailed != ECartDerailCause::None)
		{
			Total.Derailed = R.Derailed;
		}
		if (Cart.IsDerailed())
		{
			Accumulator = 0.0;
			break;
		}
	}
	if (Accumulator < 0.0)
	{
		Accumulator = 0.0;
	}
	return Total;
}

FSaveValue FTramwayModel::ToValue() const
{
	FSaveValue Root = FSaveValue::MakeObject();
	Root.Set(TEXT("v"), FSaveValue::MakeInt(1));
	FSaveValue Segments = FSaveValue::MakeArray();
	FSaveValue Switches = FSaveValue::MakeArray();
	FSaveValue Winches = FSaveValue::MakeArray();
	for (const FIntVector& Key : SortedNodes())
	{
		const FNode& Node = Nodes.FindChecked(Key);
		for (int32 D = 0; D < 2; ++D)
		{
			if (Node.HasEdge(D))
			{
				TramwayDetail::AddNode(Segments, Key);
				Segments.Add(FSaveValue::MakeInt(D));
				Segments.Add(FSaveValue::MakeInt(Node.Rise[D]));
				Segments.Add(FSaveValue::MakeInt((Node.DamagedMask & (1u << D)) != 0 ? 1 : 0));
			}
		}
		if (Node.Lever >= 0)
		{
			TramwayDetail::AddNode(Switches, Key);
			Switches.Add(FSaveValue::MakeInt(Node.Lever));
		}
		if (Node.bWinch)
		{
			TramwayDetail::AddNode(Winches, Key);
		}
	}
	Root.Set(TEXT("seg"), MoveTemp(Segments));
	Root.Set(TEXT("sw"), MoveTemp(Switches));
	Root.Set(TEXT("winch"), MoveTemp(Winches));
	return Root;
}

bool FTramwayModel::FromValue(const FSaveValue& Value)
{
	using namespace TramwayDetail;
	Reset();
	const auto Fail = [this]()
	{
		Reset();
		return false;
	};
	const FSaveValue* Version = Value.Find(TEXT("v"));
	const FSaveValue* Segments = Value.Find(TEXT("seg"));
	const FSaveValue* Switches = Value.Find(TEXT("sw"));
	const FSaveValue* Winches = Value.Find(TEXT("winch"));
	if (!Value.IsObject() || !Version || Version->AsInt(-1) != 1 || !Segments || !Segments->IsArray()
		|| !Switches || !Switches->IsArray() || !Winches || !Winches->IsArray()
		|| Segments->Num() % 6 != 0 || Switches->Num() % 4 != 0 || Winches->Num() % 3 != 0)
	{
		return Fail();
	}
	for (int32 I = 0; I < Segments->Num(); I += 6)
	{
		FIntVector A;
		int32 Dir = 0;
		int32 Rise = 0;
		int32 Damaged = 0;
		if (!ReadNode(*Segments, I, A) || !ReadInt(*Segments, I + 3, 0, 1, Dir)
			|| !ReadInt(*Segments, I + 4, -Settings.MaxRiseSteps, Settings.MaxRiseSteps, Rise)
			|| !ReadInt(*Segments, I + 5, 0, 1, Damaged))
		{
			return Fail();
		}
		const FIntVector B = A + DirOffset(static_cast<ERailDir>(Dir)) + FIntVector(0, 0, Rise);
		if (Place(A, B) != ERailPlaceResult::Ok)
		{
			return Fail();
		}
		if (Damaged != 0)
		{
			SetDamaged(A, B, true);
		}
	}
	for (int32 I = 0; I < Switches->Num(); I += 4)
	{
		FIntVector Node;
		int32 Dir = 0;
		if (!ReadNode(*Switches, I, Node) || !ReadInt(*Switches, I + 3, 0, 3, Dir) || !SetSwitch(Node, static_cast<ERailDir>(Dir)))
		{
			return Fail();
		}
	}
	for (int32 I = 0; I < Winches->Num(); I += 3)
	{
		FIntVector Node;
		if (!ReadNode(*Winches, I, Node))
		{
			return Fail();
		}
		// No se pasa por AddWinch: un torno cuyo último tramo se quitó sigue en su nodo
		// (Remove no lo borra) y tiene que volver a cargarse igual.
		FNode& Target = Nodes.FindOrAdd(Node);
		if (Target.bWinch)
		{
			return Fail();
		}
		Target.bWinch = true;
	}
	return true;
}

FSaveValue FTramwayModel::CartToValue(const FMineCart& Cart)
{
	FSaveValue Root = FSaveValue::MakeObject();
	FSaveValue Ends = FSaveValue::MakeArray();
	TramwayDetail::AddNode(Ends, Cart.From);
	TramwayDetail::AddNode(Ends, Cart.To);
	Root.Set(TEXT("seg"), MoveTemp(Ends));
	Root.Set(TEXT("s"), FSaveValue::MakeDouble(Cart.S));
	Root.Set(TEXT("vel"), FSaveValue::MakeDouble(Cart.V));
	Root.Set(TEXT("load"), FSaveValue::MakeFloat(Cart.LoadKg));
	Root.Set(TEXT("derail"), FSaveValue::MakeInt(static_cast<int32>(Cart.Derailed)));
	return Root;
}

bool FTramwayModel::CartFromValue(const FSaveValue& Value, FMineCart& OutCart)
{
	using namespace TramwayDetail;
	const FSaveValue* Ends = Value.Find(TEXT("seg"));
	const FSaveValue* S = Value.Find(TEXT("s"));
	const FSaveValue* V = Value.Find(TEXT("vel"));
	const FSaveValue* Load = Value.Find(TEXT("load"));
	const FSaveValue* Derail = Value.Find(TEXT("derail"));
	FMineCart Cart;
	double Load64 = 0.0;
	int64 Cause = 0;
	if (!Value.IsObject() || !Ends || !Ends->IsArray() || Ends->Num() != 6 || !S || !V || !Load || !Derail
		|| !ReadNode(*Ends, 0, Cart.From) || !ReadNode(*Ends, 3, Cart.To)
		|| !S->TryGetDouble(Cart.S) || !V->TryGetDouble(Cart.V) || !Load->TryGetDouble(Load64)
		|| !FMath::IsFinite(Cart.S) || !FMath::IsFinite(Cart.V) || !(Load64 >= 0.0) || Load64 > 1.0e6
		|| !Derail->TryGetInt(Cause) || Cause < 0 || Cause > static_cast<int64>(ECartDerailCause::MissingTrack))
	{
		return false;
	}
	Cart.LoadKg = static_cast<float>(Load64);
	Cart.Derailed = static_cast<ECartDerailCause>(Cause);
	OutCart = Cart;
	return true;
}

bool FTramwayModel::operator==(const FTramwayModel& Other) const
{
	if (Nodes.Num() != Other.Nodes.Num())
	{
		return false;
	}
	for (const auto& Pair : Nodes)
	{
		const FNode* Match = Other.Nodes.Find(Pair.Key);
		if (!Match || !(*Match == Pair.Value))
		{
			return false;
		}
	}
	return true;
}
