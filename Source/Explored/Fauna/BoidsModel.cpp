#include "Fauna/BoidsModel.h"

namespace BoidsDetail
{
	/** 21 bits por eje con signo desplazado: ±1 048 576 celdas (más de 3000 km con celdas de 3 m). */
	constexpr int64 AxisBias = 1 << 20;
	constexpr int64 AxisMask = (1 << 21) - 1;

	int64 PackCell(int64 X, int64 Y, int64 Z)
	{
		return (((X + AxisBias) & AxisMask) << 42) | (((Y + AxisBias) & AxisMask) << 21) | ((Z + AxisBias) & AxisMask);
	}

	uint32 HashCell(int64 Key)
	{
		const uint64 H = static_cast<uint64>(Key) * 0x9E3779B97F4A7C15ULL;
		return static_cast<uint32>(H >> 32);
	}

	/** Clave vacía de la tabla (ninguna celda empaquetada la produce: los 64 bits a 1). */
	constexpr int64 EmptyKey = -1;

	FVector SafeDirection(const FVector& D, double Distance, bool bFlip)
	{
		// Dos agentes en el mismo punto: dirección fija y opuesta para cada uno (determinista, sin NaN).
		if (Distance < 1.0e-3)
		{
			return bFlip ? FVector(-1.0, 0.0, 0.0) : FVector(1.0, 0.0, 0.0);
		}
		return D / Distance;
	}

	FVector ClampSize(const FVector& V, double Max)
	{
		const double S2 = V.SizeSquared();
		if (S2 > Max * Max && S2 > 0.0)
		{
			return V * (Max / FMath::Sqrt(S2));
		}
		return V;
	}
}

int32 FBoidsModel::AddAgent(const FVector& Position, const FVector& Velocity)
{
	FBoidAgent Agent;
	Agent.Position = Position;
	Agent.Velocity = BoidsDetail::ClampSize(Velocity, Params.MaxSpeedCmS);
	Agent.Id = NextId++;
	return Agents.Add(Agent);
}

void FBoidsModel::RemoveAgent(int32 Index)
{
	if (Agents.IsValidIndex(Index))
	{
		Agents.RemoveAt(Index);
	}
}

FBoidBand FBoidsModel::BandAt(const FVector& P, const FBoidsEnvironment& Environment) const
{
	return Environment.Band ? Environment.Band(P) : FBoidBand();
}

void FBoidsModel::Step(float DeltaSeconds, const FBoidsEnvironment& Environment)
{
	// NaN pasaría el guarda `<= 0` y CeilToInt(NaN) es indefinido.
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f || Agents.Num() == 0)
	{
		return;
	}
	const float Simulated = FMath::Min(DeltaSeconds, MaxSubstep * MaxSubstepsPerStep);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Simulated / MaxSubstep), 1, MaxSubstepsPerStep);
	const float Dt = Simulated / Steps;
	for (int32 I = 0; I < Steps; ++I)
	{
		SubStep(Dt, Environment);
	}
}

void FBoidsModel::SubStep(float Dt, const FBoidsEnvironment& Env)
{
	using namespace BoidsDetail;
	const int32 N = Agents.Num();
	const double Cell = FMath::Max(1.0f, Params.NeighborRadiusCm);
	const double InvCell = 1.0 / Cell;
	const double R2 = static_cast<double>(Params.NeighborRadiusCm) * Params.NeighborRadiusCm;
	const double SepR = FMath::Max(1.0f, Params.SeparationRadiusCm);
	const double MaxSpeed = Params.MaxSpeedCmS;

	// 1. Hash espacial: índices ordenados por (celda, índice) y tabla abierta celda → rango.
	CellKeys.SetNum(N);
	SortedIndices.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		const FVector& P = Agents[I].Position;
		CellKeys[I] = PackCell(FMath::FloorToInt64(P.X * InvCell), FMath::FloorToInt64(P.Y * InvCell), FMath::FloorToInt64(P.Z * InvCell));
		SortedIndices[I] = I;
	}
	const TArray<int64>& Keys = CellKeys;
	SortedIndices.Sort([&Keys](const int32 A, const int32 B)
	{
		return Keys[A] != Keys[B] ? Keys[A] < Keys[B] : A < B;
	});

	int32 TableSize = 16;
	while (TableSize < N * 2)
	{
		TableSize *= 2;
	}
	const uint32 TableMask = static_cast<uint32>(TableSize - 1);
	TableKeys.Init(EmptyKey, TableSize);
	TableStart.Init(0, TableSize);
	TableCount.Init(0, TableSize);
	for (int32 S = 0; S < N;)
	{
		const int64 Key = CellKeys[SortedIndices[S]];
		int32 E = S + 1;
		while (E < N && CellKeys[SortedIndices[E]] == Key)
		{
			++E;
		}
		uint32 Slot = HashCell(Key) & TableMask;
		while (TableKeys[Slot] != EmptyKey)
		{
			Slot = (Slot + 1) & TableMask;
		}
		TableKeys[Slot] = Key;
		TableStart[Slot] = S;
		TableCount[Slot] = E - S;
		S = E;
	}

	// 2. Aceleraciones sobre la foto actual.
	Accelerations.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		const FBoidAgent& Self = Agents[I];
		const FVector& P = Self.Position;
		const FVector& V = Self.Velocity;
		const int64 CX = FMath::FloorToInt64(P.X * InvCell);
		const int64 CY = FMath::FloorToInt64(P.Y * InvCell);
		const int64 CZ = FMath::FloorToInt64(P.Z * InvCell);

		FVector Separation = FVector::ZeroVector;
		FVector SumP = FVector::ZeroVector;
		FVector SumV = FVector::ZeroVector;
		int32 Count = 0;
		// La separación mira a todos los vecinos cercanos (nunca se recorta: evita que dos peces se
		// superpongan); alineación y cohesión solo a los primeros MaxNeighbors, en orden determinista.
		const double SepR2 = SepR * SepR;
		for (int64 DX = -1; DX <= 1; ++DX)
		{
			for (int64 DY = -1; DY <= 1; ++DY)
			{
				for (int64 DZ = -1; DZ <= 1; ++DZ)
				{
					const int64 Key = PackCell(CX + DX, CY + DY, CZ + DZ);
					uint32 Slot = HashCell(Key) & TableMask;
					while (TableKeys[Slot] != EmptyKey && TableKeys[Slot] != Key)
					{
						Slot = (Slot + 1) & TableMask;
					}
					if (TableKeys[Slot] != Key)
					{
						continue;
					}
					const int32 Start = TableStart[Slot];
					const int32 End = Start + TableCount[Slot];
					for (int32 S = Start; S < End; ++S)
					{
						const int32 J = SortedIndices[S];
						if (J == I)
						{
							continue;
						}
						const FBoidAgent& Other = Agents[J];
						const FVector D = P - Other.Position;
						const double D2 = D.SizeSquared();
						if (D2 > R2)
						{
							continue;
						}
						if (D2 < SepR2)
						{
							const double Dist = FMath::Sqrt(D2);
							Separation += SafeDirection(D, Dist, I < J) * (1.0 - Dist / SepR);
						}
						if (Count < Params.MaxNeighbors)
						{
							++Count;
							SumP += Other.Position;
							SumV += Other.Velocity;
						}
					}
				}
			}
		}

		FVector Steer = FVector::ZeroVector;
		Steer += Separation * (MaxSpeed * Params.SeparationWeight * Env.SeparationScale);
		if (Count > 0)
		{
			const double Inv = 1.0 / Count;
			Steer += (SumV * Inv - V) * (Params.AlignmentWeight * Env.AlignmentScale);
			// Cell es el radio con el mismo mínimo que el hash: un radio 0 daría 0/0 = NaN.
			Steer += ((SumP * Inv - P) / Cell) * (MaxSpeed * Params.CohesionWeight * Env.CohesionScale);
		}

		if (Env.bHasTarget)
		{
			const FVector Desired = (Env.Target - P).GetSafeNormal() * Params.CruiseSpeedCmS;
			Steer += (Desired - V) * (Params.TargetWeight * Env.TargetScale);
		}

		const double Speed = V.Size();
		if (Speed > 1.0)
		{
			Steer += (V / Speed) * ((Params.CruiseSpeedCmS - Speed) * Params.CruiseWeight);
		}

		for (const FBoidThreat& Threat : Env.Threats)
		{
			const FVector D = P - Threat.Position;
			const double Dist = D.Size();
			if (Dist < Threat.RadiusCm)
			{
				Steer += SafeDirection(D, Dist, (Self.Id & 1u) != 0) * ((1.0 - Dist / Threat.RadiusCm) * MaxSpeed * Params.FleeWeight * Threat.Weight);
			}
		}

		for (const FBoidObstacle& Obstacle : Env.Obstacles)
		{
			const FVector D = P - Obstacle.Center;
			const double Dist = D.Size();
			const double Reach = Obstacle.RadiusCm + SepR;
			if (Dist < Reach)
			{
				const double Push = FMath::Clamp(1.0 - (Dist - Obstacle.RadiusCm) / SepR, 0.0, 1.0);
				Steer += SafeDirection(D, Dist, (Self.Id & 1u) != 0) * (Push * MaxSpeed * Params.AvoidWeight);
			}
		}

		// Franja vertical: empuje suave hacia dentro al acercarse a los bordes.
		const double Margin = FMath::Max(1.0f, Params.BandMarginCm);
		const FBoidBand Band = BandAt(P, Env);
		if (Band.MaxZ - Band.MinZ < 2.0 * Margin)
		{
			const double Mid = 0.5 * (Band.MinZ + Band.MaxZ);
			Steer.Z += (Mid - P.Z) / Margin * MaxSpeed * Params.BandWeight;
		}
		else if (P.Z < Band.MinZ + Margin)
		{
			Steer.Z += (Band.MinZ + Margin - P.Z) / Margin * MaxSpeed * Params.BandWeight;
		}
		else if (P.Z > Band.MaxZ - Margin)
		{
			Steer.Z -= (P.Z - (Band.MaxZ - Margin)) / Margin * MaxSpeed * Params.BandWeight;
		}

		// Anticipación: si por delante no hay franja (tierra, orilla seca) se gira; si la franja cambia, se ajusta la altura.
		const FVector2D Horizontal(V.X, V.Y);
		const double HSpeed = Horizontal.Size();
		if (HSpeed > 1.0 && Params.LookAheadSeconds > 0.0f)
		{
			const FVector2D H = Horizontal / HSpeed;
			const FVector Ahead = P + FVector(H.X, H.Y, 0.0) * FMath::Max(HSpeed * Params.LookAheadSeconds, Params.NeighborRadiusCm * 0.5);
			const FBoidBand AheadBand = BandAt(Ahead, Env);
			if (!AheadBand.IsValid(Params.MinBandThicknessCm))
			{
				const double Side = (Self.Id & 1u) != 0 ? 1.0 : -1.0;
				const FVector2D Turn = -H + FVector2D(-H.Y, H.X) * Side;
				Steer += FVector(Turn.X, Turn.Y, 0.0) * (MaxSpeed * Params.AvoidWeight);
			}
			else if (P.Z < AheadBand.MinZ + Margin)
			{
				Steer.Z += (AheadBand.MinZ + Margin - P.Z) / Margin * MaxSpeed * Params.BandWeight * 0.5;
			}
			else if (P.Z > AheadBand.MaxZ - Margin)
			{
				Steer.Z -= (P.Z - (AheadBand.MaxZ - Margin)) / Margin * MaxSpeed * Params.BandWeight * 0.5;
			}
		}

		if (Params.bUseBounds)
		{
			const double BoundsMargin = FMath::Max(1.0f, Params.NeighborRadiusCm);
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				const double Lo = Params.BoundsMin[Axis] + BoundsMargin;
				const double Hi = Params.BoundsMax[Axis] - BoundsMargin;
				if (P[Axis] < Lo)
				{
					Steer[Axis] += (Lo - P[Axis]) / BoundsMargin * MaxSpeed * Params.AvoidWeight;
				}
				else if (P[Axis] > Hi)
				{
					Steer[Axis] -= (P[Axis] - Hi) / BoundsMargin * MaxSpeed * Params.AvoidWeight;
				}
			}
		}

		FVector Accel = Steer * Params.SteeringRate;
		Accel.Z *= Params.VerticalAgility;
		Accelerations[I] = ClampSize(Accel, Params.MaxAccelCmS2);
	}

	// 3. Integración con las garantías duras (velocidad, franja, tierra y límites).
	for (int32 I = 0; I < N; ++I)
	{
		FBoidAgent& Agent = Agents[I];
		const FVector Old = Agent.Position;
		FVector V = ClampSize(Agent.Velocity + Accelerations[I] * Dt, MaxSpeed);
		FVector P = Old + V * Dt;

		FBoidBand Band = BandAt(P, Env);
		if (!Band.IsValid(Params.MinBandThicknessCm))
		{
			// Sin sitio (tierra para los peces): no se avanza en horizontal y se da la vuelta.
			P.X = Old.X;
			P.Y = Old.Y;
			V.X = -V.X;
			V.Y = -V.Y;
			Band = BandAt(P, Env);
		}
		if (Band.MinZ <= Band.MaxZ)
		{
			if (P.Z < Band.MinZ)
			{
				P.Z = Band.MinZ;
				V.Z = FMath::Max(V.Z, 0.0);
			}
			else if (P.Z > Band.MaxZ)
			{
				P.Z = Band.MaxZ;
				V.Z = FMath::Min(V.Z, 0.0);
			}
		}

		if (Params.bUseBounds)
		{
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				if (P[Axis] < Params.BoundsMin[Axis])
				{
					P[Axis] = Params.BoundsMin[Axis];
					V[Axis] = FMath::Max(V[Axis], 0.0);
				}
				else if (P[Axis] > Params.BoundsMax[Axis])
				{
					P[Axis] = Params.BoundsMax[Axis];
					V[Axis] = FMath::Min(V[Axis], 0.0);
				}
			}
		}

		if (Params.MinSpeedCmS > 0.0f && V.SizeSquared() < static_cast<double>(Params.MinSpeedCmS) * Params.MinSpeedCmS)
		{
			// Siempre en vuelo: nunca por debajo de la velocidad mínima; se conserva el rumbo horizontal.
			FVector2D H(V.X, V.Y);
			if (H.SizeSquared() < 1.0e-6)
			{
				H = FVector2D(Agent.Velocity.X, Agent.Velocity.Y);
			}
			H = H.GetSafeNormal();
			if (H.IsNearlyZero())
			{
				H = FVector2D(1.0, 0.0);
			}
			V = FVector(H.X * Params.MinSpeedCmS, H.Y * Params.MinSpeedCmS, 0.0);
		}

		Agent.Position = P;
		Agent.Velocity = V;
	}
}

FVector FBoidsModel::Centroid() const
{
	FVector Sum = FVector::ZeroVector;
	for (const FBoidAgent& Agent : Agents)
	{
		Sum += Agent.Position;
	}
	return Agents.Num() > 0 ? Sum / static_cast<double>(Agents.Num()) : Sum;
}

FVector FBoidsModel::AverageVelocity() const
{
	FVector Sum = FVector::ZeroVector;
	for (const FBoidAgent& Agent : Agents)
	{
		Sum += Agent.Velocity;
	}
	return Agents.Num() > 0 ? Sum / static_cast<double>(Agents.Num()) : Sum;
}

float FBoidsModel::Spread() const
{
	if (Agents.Num() == 0)
	{
		return 0.0f;
	}
	const FVector C = Centroid();
	double Sum = 0.0;
	for (const FBoidAgent& Agent : Agents)
	{
		Sum += FVector::Distance(Agent.Position, C);
	}
	return static_cast<float>(Sum / Agents.Num());
}

float FBoidsModel::Polarization() const
{
	if (Agents.Num() == 0)
	{
		return 0.0f;
	}
	FVector Sum = FVector::ZeroVector;
	for (const FBoidAgent& Agent : Agents)
	{
		Sum += Agent.Velocity.GetSafeNormal();
	}
	return static_cast<float>(Sum.Size() / Agents.Num());
}

void FBoidsModel::QueryNeighbors(const FVector& Point, float RadiusCm, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	const double R2 = static_cast<double>(RadiusCm) * RadiusCm;
	for (int32 I = 0; I < Agents.Num(); ++I)
	{
		if (FVector::DistSquared(Agents[I].Position, Point) <= R2)
		{
			OutIndices.Add(I);
		}
	}
}
