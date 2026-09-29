#include "Boats/HullAssemblyModel.h"

#include "Boats/BoatModel.h"

const TCHAR* LexToString(EHullPieceType Type)
{
	switch (Type)
	{
	case EHullPieceType::Log: return TEXT("Log");
	case EHullPieceType::Plank: return TEXT("Plank");
	case EHullPieceType::Bamboo: return TEXT("Bamboo");
	case EHullPieceType::Float: return TEXT("Float");
	case EHullPieceType::Mast: return TEXT("Mast");
	case EHullPieceType::Sail: return TEXT("Sail");
	case EHullPieceType::Oars: return TEXT("Oars");
	case EHullPieceType::Paddle: return TEXT("Paddle");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(EHullVerdict Verdict)
{
	switch (Verdict)
	{
	case EHullVerdict::Floats: return TEXT("Floats");
	case EHullVerdict::Lists: return TEXT("Lists");
	case EHullVerdict::Swamps: return TEXT("Swamps");
	case EHullVerdict::Capsizes: return TEXT("Capsizes");
	case EHullVerdict::Sinks: return TEXT("Sinks");
	case EHullVerdict::Empty: return TEXT("Empty");
	default: return TEXT("Unknown");
	}
}

const TCHAR* LexToString(EHullPropulsion Propulsion)
{
	switch (Propulsion)
	{
	case EHullPropulsion::None: return TEXT("None");
	case EHullPropulsion::Paddle: return TEXT("Paddle");
	case EHullPropulsion::Oars: return TEXT("Oars");
	case EHullPropulsion::Sail: return TEXT("Sail");
	default: return TEXT("Unknown");
	}
}

namespace HullAssemblyDetail
{
	/** cm³ → m³. */
	constexpr double CubicCmToM3 = 1.0e-6;
	/** Paso del barrido de la curva de estabilidad (°). */
	constexpr double ScanStepDeg = 0.5;
	/** Límite del barrido de escora y de asiento (°). */
	constexpr double MaxHeelScanDeg = 89.5;
	constexpr double MaxTrimScanDeg = 45.0;
	/** Semiintervalo de la diferencia central para GM (°). */
	constexpr double SlopeStepDeg = 0.25;
	constexpr int32 BisectionSteps = 64;

	/** Rectángulo de la sección de una caja: (U, V) en el plano de la sección y Depth a lo largo del otro eje (cm). */
	struct FRect
	{
		double U0 = 0.0, U1 = 0.0, V0 = 0.0, V1 = 0.0, Depth = 0.0;
	};

	/**
	 * Sección del casco para girar en un plano: escora (U = Y, V = Z) o asiento
	 * (U = X, V = Z). G es el centro de masas en (U, V).
	 */
	struct FSection
	{
		TArray<FRect> Rects;
		double MassKg = 0.0;
		double GU = 0.0;
		double GV = 0.0;
	};

	struct FPoint
	{
		double U = 0.0, V = 0.0;
	};

	/** Altura sobre el plano horizontal de un punto del casco girado Alpha (el lado +U baja). */
	inline double Height(double U, double V, double C, double S) { return V * C - U * S; }
	/** Posición horizontal de un punto del casco girado Alpha. */
	inline double Horizontal(double U, double V, double C, double S) { return U * C + V * S; }

	/** Volumen sumergido (cm³) y su centroide (U, V) bajo la flotación Height ≤ D. */
	struct FSubmerged
	{
		double Volume = 0.0;
		double CU = 0.0;
		double CV = 0.0;
	};

	/** Recorta el rectángulo con el semiplano Height ≤ D y acumula área × fondo y su momento (exacto). */
	void AccumulateClipped(const FRect& R, double C, double S, double D, FSubmerged& Out)
	{
		const FPoint In[4] = { { R.U0, R.V0 }, { R.U1, R.V0 }, { R.U1, R.V1 }, { R.U0, R.V1 } };
		FPoint Poly[8];
		int32 Count = 0;
		for (int32 I = 0; I < 4; ++I)
		{
			const FPoint& A = In[I];
			const FPoint& B = In[(I + 1) % 4];
			const double HA = Height(A.U, A.V, C, S) - D;
			const double HB = Height(B.U, B.V, C, S) - D;
			const bool bAIn = HA <= 0.0;
			const bool bBIn = HB <= 0.0;
			if (bAIn)
			{
				Poly[Count++] = A;
			}
			if (bAIn != bBIn)
			{
				const double T = HA / (HA - HB);
				Poly[Count++] = { A.U + (B.U - A.U) * T, A.V + (B.V - A.V) * T };
			}
		}
		if (Count < 3)
		{
			return;
		}
		double Area2 = 0.0, MU = 0.0, MV = 0.0;
		for (int32 I = 0; I < Count; ++I)
		{
			const FPoint& A = Poly[I];
			const FPoint& B = Poly[(I + 1) % Count];
			const double Cross = A.U * B.V - B.U * A.V;
			Area2 += Cross;
			MU += (A.U + B.U) * Cross;
			MV += (A.V + B.V) * Cross;
		}
		if (FMath::Abs(Area2) <= 0.0)
		{
			return;
		}
		const double Area = 0.5 * Area2;
		const double CU = MU / (3.0 * Area2);
		const double CV = MV / (3.0 * Area2);
		const double Volume = FMath::Abs(Area) * R.Depth;
		Out.CU += CU * Volume;
		Out.CV += CV * Volume;
		Out.Volume += Volume;
	}

	FSubmerged Submerged(const FSection& Section, double C, double S, double D)
	{
		FSubmerged Out;
		for (const FRect& R : Section.Rects)
		{
			AccumulateClipped(R, C, S, D, Out);
		}
		if (Out.Volume > 0.0)
		{
			Out.CU /= Out.Volume;
			Out.CV /= Out.Volume;
		}
		return Out;
	}

	/** Flotación a una escora: la altura D del agua que desplaza el peso. False si ni sumergido entero lo aguanta. */
	struct FFloat
	{
		bool bFloats = false;
		double D = 0.0;
		FSubmerged Sub;
	};

	FFloat SolveWaterline(const FSection& Section, double AlphaDeg)
	{
		FFloat Out;
		const double A = FMath::DegreesToRadians(AlphaDeg);
		const double C = FMath::Cos(A);
		const double S = FMath::Sin(A);
		double Lo = TNumericLimits<double>::Max();
		double Hi = -TNumericLimits<double>::Max();
		for (const FRect& R : Section.Rects)
		{
			for (const FPoint& P : { FPoint{ R.U0, R.V0 }, FPoint{ R.U1, R.V0 }, FPoint{ R.U1, R.V1 }, FPoint{ R.U0, R.V1 } })
			{
				const double H = Height(P.U, P.V, C, S);
				Lo = FMath::Min(Lo, H);
				Hi = FMath::Max(Hi, H);
			}
		}
		if (Section.Rects.Num() == 0)
		{
			return Out;
		}
		const double Target = Section.MassKg / FHullAssemblyModel::WaterDensity / CubicCmToM3;
		if (Submerged(Section, C, S, Hi).Volume < Target)
		{
			return Out;
		}
		for (int32 I = 0; I < BisectionSteps; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			if (Submerged(Section, C, S, Mid).Volume < Target)
			{
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}
		Out.bFloats = true;
		Out.D = 0.5 * (Lo + Hi);
		Out.Sub = Submerged(Section, C, S, Out.D);
		return Out;
	}

	/** Brazo adrizante (cm): positivo = el empuje devuelve el lado +U hacia arriba. */
	double RightingArm(const FSection& Section, double AlphaDeg)
	{
		const FFloat F = SolveWaterline(Section, AlphaDeg);
		if (!F.bFloats)
		{
			return 0.0;
		}
		const double A = FMath::DegreesToRadians(AlphaDeg);
		const double C = FMath::Cos(A);
		const double S = FMath::Sin(A);
		return Horizontal(F.Sub.CU, F.Sub.CV, C, S) - Horizontal(Section.GU, Section.GV, C, S);
	}

	/** Pendiente de GZ (cm/rad) en un ángulo: la altura metacéntrica en el equilibrio. */
	double RightingSlope(const FSection& Section, double AlphaDeg)
	{
		const double Plus = RightingArm(Section, AlphaDeg + SlopeStepDeg);
		const double Minus = RightingArm(Section, AlphaDeg - SlopeStepDeg);
		return (Plus - Minus) / FMath::DegreesToRadians(2.0 * SlopeStepDeg);
	}

	struct FEquilibrium
	{
		bool bFound = false;
		double AngleDeg = 0.0;
		/** Ángulo (valor absoluto) en el que GZ vuelve a cambiar de signo por el lado más débil. */
		double VanishingDeg = 0.0;
	};

	/**
	 * Equilibrio estable más cercano a 0: un cero de GZ en el que GZ pasa de
	 * negativo a positivo al crecer el ángulo (una escora mayor lo devuelve).
	 */
	FEquilibrium FindEquilibrium(const FSection& Section, double MaxDeg)
	{
		FEquilibrium Out;
		const int32 Steps = FMath::RoundToInt32(2.0 * MaxDeg / ScanStepDeg);
		TArray<double> Angles;
		TArray<double> Arms;
		Angles.Reserve(Steps + 1);
		Arms.Reserve(Steps + 1);
		for (int32 I = 0; I <= Steps; ++I)
		{
			const double Alpha = -MaxDeg + I * ScanStepDeg;
			Angles.Add(Alpha);
			Arms.Add(RightingArm(Section, Alpha));
		}

		int32 Best = INDEX_NONE;
		double BestDistance = TNumericLimits<double>::Max();
		for (int32 I = 0; I < Steps; ++I)
		{
			const bool bStable = (Arms[I] < 0.0 && Arms[I + 1] >= 0.0) || (Arms[I] <= 0.0 && Arms[I + 1] > 0.0);
			if (!bStable)
			{
				continue;
			}
			const double Distance = FMath::Abs(0.5 * (Angles[I] + Angles[I + 1]));
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = I;
			}
		}
		if (Best == INDEX_NONE)
		{
			return Out;
		}

		double Lo = Angles[Best];
		double Hi = Angles[Best + 1];
		for (int32 I = 0; I < 40; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			if (RightingArm(Section, Mid) < 0.0)
			{
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}
		Out.bFound = true;
		Out.AngleDeg = 0.5 * (Lo + Hi);

		// Hacia ángulos mayores GZ debe seguir positivo; donde deja de serlo, la escora ya no vuelve.
		double Vanishing = MaxDeg;
		for (int32 I = Best + 1; I <= Steps; ++I)
		{
			if (Arms[I] <= 0.0 && Angles[I] > Out.AngleDeg + ScanStepDeg)
			{
				Vanishing = FMath::Min(Vanishing, FMath::Abs(Angles[I]));
				break;
			}
		}
		for (int32 I = Best; I >= 0; --I)
		{
			if (Arms[I] >= 0.0 && Angles[I] < Out.AngleDeg - ScanStepDeg)
			{
				Vanishing = FMath::Min(Vanishing, FMath::Abs(Angles[I]));
				break;
			}
		}
		Out.VanishingDeg = Vanishing;
		return Out;
	}

	/** Caja 3D de una pieza (cm). */
	struct FPieceBox
	{
		FVector Min;
		FVector Max;
	};

	FPieceBox BoxOf(const FHullPiece& Piece)
	{
		const FVector Half = FHullAssemblyModel::EffectiveSizeCm(Piece) * 0.5;
		return { Piece.CenterCm - Half, Piece.CenterCm + Half };
	}

	bool IsBuoyant(const FHullPiece& Piece)
	{
		const FVector Size = FHullAssemblyModel::EffectiveSizeCm(Piece);
		return FHullAssemblyModel::Spec(Piece.Type).bBuoyant && Size.X > 0.0 && Size.Y > 0.0 && Size.Z > 0.0;
	}

	/** Velocidad sostenida (m/s) con un empuje contra la resistencia de forma y la pared de la velocidad de casco. */
	double SustainedSpeedMS(double ThrustN, double DragCoefficient, double FrontalAreaM2, double HullSpeedMS)
	{
		if (ThrustN <= 0.0 || FrontalAreaM2 <= 0.0)
		{
			return 0.0;
		}
		const double Free = FMath::Sqrt(2.0 * ThrustN / (FHullAssemblyModel::WaterDensity * DragCoefficient * FrontalAreaM2));
		return Free <= HullSpeedMS ? Free : HullSpeedMS + 0.25 * (Free - HullSpeedMS);
	}
}

const FHullPieceSpec& FHullAssemblyModel::Spec(EHullPieceType Type)
{
	static const FHullPieceSpec Specs[] = {
		// Tronco de 3 m y ~25 cm de diámetro (caja de igual sección), madera ligera secada.
		{ EHullPieceType::Log, FVector(300.0, 22.0, 22.0), 500.0f, 0.0f, true },
		// Tablón de 2 m × 25 × 4 cm.
		{ EHullPieceType::Plank, FVector(200.0, 25.0, 4.0), 550.0f, 0.0f, true },
		// Haz de cañas gruesas de 3 m: huecas, densidad efectiva baja.
		{ EHullPieceType::Bamboo, FVector(300.0, 10.0, 10.0), 300.0f, 0.0f, true },
		// Flotador sellado (calabaza grande o barril) de 60 × 40 × 40 cm.
		{ EHullPieceType::Float, FVector(60.0, 40.0, 40.0), 80.0f, 0.0f, true },
		// Mástil de 4 m de alto.
		{ EHullPieceType::Mast, FVector(10.0, 10.0, 400.0), 550.0f, 0.0f, true },
		// Vela de 2 × 2 m: 1,2 kg por m² de lona o esterilla.
		{ EHullPieceType::Sail, FVector(5.0, 200.0, 200.0), 0.0f, 1.2f, false },
		// Par de remos.
		{ EHullPieceType::Oars, FVector(20.0, 20.0, 10.0), 0.0f, 6.0f, false },
		// Pala (canalete).
		{ EHullPieceType::Paddle, FVector(20.0, 10.0, 10.0), 0.0f, 2.5f, false },
	};
	static_assert(UE_ARRAY_COUNT(Specs) == static_cast<int32>(EHullPieceType::Count), "Falta la ficha de algún tipo de pieza");
	const int32 Index = FMath::Clamp(static_cast<int32>(Type), 0, static_cast<int32>(EHullPieceType::Count) - 1);
	return Specs[Index];
}

namespace HullAssemblyDetail
{
	bool IsFiniteVector(const FVector& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
	}
}

int32 FHullAssemblyModel::AddPiece(const FHullPiece& Piece)
{
	// Un NaN en una pieza lo contagiaría a toda la hidrostática del casco.
	if (!HullAssemblyDetail::IsFiniteVector(Piece.CenterCm) || !HullAssemblyDetail::IsFiniteVector(Piece.SizeCm))
	{
		return INDEX_NONE;
	}
	// Dos piezas con volumen no pueden ocupar el mismo sitio: cada caja suma su volumen y su
	// flotación, y cinco troncos apilados en el hueco de uno flotarían como cinco.
	if (HullAssemblyDetail::IsBuoyant(Piece))
	{
		const HullAssemblyDetail::FPieceBox Box = HullAssemblyDetail::BoxOf(Piece);
		for (const FHullPiece& Other : Pieces)
		{
			if (!HullAssemblyDetail::IsBuoyant(Other))
			{
				continue;
			}
			const HullAssemblyDetail::FPieceBox OtherBox = HullAssemblyDetail::BoxOf(Other);
			const FVector Overlap(
				FMath::Min(Box.Max.X, OtherBox.Max.X) - FMath::Max(Box.Min.X, OtherBox.Min.X),
				FMath::Min(Box.Max.Y, OtherBox.Max.Y) - FMath::Max(Box.Min.Y, OtherBox.Min.Y),
				FMath::Min(Box.Max.Z, OtherBox.Max.Z) - FMath::Max(Box.Min.Z, OtherBox.Min.Z));
			if (Overlap.X > MaxOverlapCm && Overlap.Y > MaxOverlapCm && Overlap.Z > MaxOverlapCm)
			{
				return INDEX_NONE;
			}
		}
	}
	return Pieces.Add(Piece);
}

void FHullAssemblyModel::AddLoad(const FHullLoad& Load)
{
	if (FMath::IsFinite(Load.MassKg) && HullAssemblyDetail::IsFiniteVector(Load.CenterCm))
	{
		Loads.Add(Load);
	}
}

bool FHullAssemblyModel::RemovePiece(int32 Index)
{
	if (!Pieces.IsValidIndex(Index))
	{
		return false;
	}
	Pieces.RemoveAt(Index);
	return true;
}

FVector FHullAssemblyModel::EffectiveSizeCm(const FHullPiece& Piece)
{
	const FVector Default = Spec(Piece.Type).DefaultSizeCm;
	return FVector(
		Piece.SizeCm.X > 0.0 ? Piece.SizeCm.X : Default.X,
		Piece.SizeCm.Y > 0.0 ? Piece.SizeCm.Y : Default.Y,
		Piece.SizeCm.Z > 0.0 ? Piece.SizeCm.Z : Default.Z);
}

float FHullAssemblyModel::PieceMassKg(const FHullPiece& Piece)
{
	const FHullPieceSpec& S = Spec(Piece.Type);
	const FVector Size = EffectiveSizeCm(Piece);
	if (Piece.Type == EHullPieceType::Sail)
	{
		return S.FixedMassKg * static_cast<float>(Size.Y * Size.Z / 10000.0);
	}
	if (S.DensityKgM3 > 0.0f)
	{
		return S.DensityKgM3 * static_cast<float>(Size.X * Size.Y * Size.Z * HullAssemblyDetail::CubicCmToM3);
	}
	return S.FixedMassKg;
}

namespace HullAssemblyDetail
{
	/** Sección para girar sobre el eje X (escora: U = Y) o sobre el eje Y (asiento: U = X). */
	FSection BuildSection(const TArray<FHullPiece>& Pieces, const TArray<FHullLoad>& Loads, bool bHeel)
	{
		FSection Out;
		double MU = 0.0, MV = 0.0;
		for (const FHullPiece& Piece : Pieces)
		{
			const double Mass = FHullAssemblyModel::PieceMassKg(Piece);
			Out.MassKg += Mass;
			MU += Mass * (bHeel ? Piece.CenterCm.Y : Piece.CenterCm.X);
			MV += Mass * Piece.CenterCm.Z;
			if (IsBuoyant(Piece))
			{
				const FPieceBox Box = BoxOf(Piece);
				FRect R;
				R.U0 = bHeel ? Box.Min.Y : Box.Min.X;
				R.U1 = bHeel ? Box.Max.Y : Box.Max.X;
				R.V0 = Box.Min.Z;
				R.V1 = Box.Max.Z;
				R.Depth = bHeel ? Box.Max.X - Box.Min.X : Box.Max.Y - Box.Min.Y;
				Out.Rects.Add(R);
			}
		}
		for (const FHullLoad& Load : Loads)
		{
			const double Mass = FMath::Max(0.0f, Load.MassKg);
			Out.MassKg += Mass;
			MU += Mass * (bHeel ? Load.CenterCm.Y : Load.CenterCm.X);
			MV += Mass * Load.CenterCm.Z;
		}
		if (Out.MassKg > 0.0)
		{
			Out.GU = MU / Out.MassKg;
			Out.GV = MV / Out.MassKg;
		}
		return Out;
	}
}

float FHullAssemblyModel::RightingArmCm(float HeelDeg) const
{
	using namespace HullAssemblyDetail;
	return static_cast<float>(RightingArm(BuildSection(Pieces, Loads, true), HeelDeg));
}

FHullHydrostatics FHullAssemblyModel::Evaluate() const
{
	using namespace HullAssemblyDetail;
	FHullHydrostatics H;

	double KeelZ = TNumericLimits<double>::Max();
	// La cubierta es lo más alto del casco sin contar el mástil (si solo hay mástil, el mástil).
	double DeckZ = -TNumericLimits<double>::Max();
	double MastTopZ = -TNumericLimits<double>::Max();
	double BuoyantVolumeCm3 = 0.0;
	FVector MassMoment = FVector::ZeroVector;
	for (const FHullPiece& Piece : Pieces)
	{
		const float Mass = PieceMassKg(Piece);
		H.StructureMassKg += Mass;
		MassMoment += Piece.CenterCm * Mass;
		if (IsBuoyant(Piece))
		{
			const FPieceBox Box = BoxOf(Piece);
			KeelZ = FMath::Min(KeelZ, Box.Min.Z);
			if (Piece.Type == EHullPieceType::Mast)
			{
				MastTopZ = FMath::Max(MastTopZ, Box.Max.Z);
			}
			else
			{
				DeckZ = FMath::Max(DeckZ, Box.Max.Z);
			}
			const FVector Size = Box.Max - Box.Min;
			BuoyantVolumeCm3 += Size.X * Size.Y * Size.Z;
		}
	}
	H.TotalMassKg = H.StructureMassKg;
	for (const FHullLoad& Load : Loads)
	{
		const float Mass = FMath::Max(0.0f, Load.MassKg);
		H.TotalMassKg += Mass;
		MassMoment += Load.CenterCm * Mass;
	}
	if (H.TotalMassKg > 0.0f)
	{
		H.CenterOfMassCm = MassMoment / H.TotalMassKg;
	}
	if (BuoyantVolumeCm3 <= 0.0)
	{
		H.Verdict = EHullVerdict::Empty;
		return H;
	}
	if (DeckZ < KeelZ)
	{
		DeckZ = MastTopZ;
	}

	H.MaxBuoyancyKg = static_cast<float>(BuoyantVolumeCm3 * CubicCmToM3 * WaterDensity);
	H.LoadRatio = H.TotalMassKg / H.MaxBuoyancyKg;
	H.DeckHeightCm = static_cast<float>(DeckZ - KeelZ);
	H.KGCm = static_cast<float>(H.CenterOfMassCm.Z - KeelZ);

	const FSection Heel = BuildSection(Pieces, Loads, true);
	const FSection Trim = BuildSection(Pieces, Loads, false);

	const FFloat Upright = SolveWaterline(Heel, 0.0);
	if (!Upright.bFloats)
	{
		H.Verdict = EHullVerdict::Sinks;
		return H;
	}

	// Flotación adrizada: calado, carena y plano de flotación.
	H.DisplacementM3 = static_cast<float>(Upright.Sub.Volume * CubicCmToM3);
	H.DraftCm = static_cast<float>(Upright.D - KeelZ);
	H.KBCm = static_cast<float>(Upright.Sub.CV - KeelZ);
	{
		const FFloat UprightTrim = SolveWaterline(Trim, 0.0);
		H.CenterOfBuoyancyCm = FVector(UprightTrim.Sub.CU, Upright.Sub.CU, Upright.Sub.CV);
	}
	double MinX = TNumericLimits<double>::Max(), MaxX = -MinX, MinY = MinX, MaxY = -MinX;
	double WaterplaneCm2 = 0.0;
	for (const FHullPiece& Piece : Pieces)
	{
		if (!IsBuoyant(Piece))
		{
			continue;
		}
		const FPieceBox Box = BoxOf(Piece);
		if (Box.Min.Z < Upright.D && Box.Max.Z > Upright.D)
		{
			WaterplaneCm2 += (Box.Max.X - Box.Min.X) * (Box.Max.Y - Box.Min.Y);
			MinX = FMath::Min(MinX, Box.Min.X);
			MaxX = FMath::Max(MaxX, Box.Max.X);
			MinY = FMath::Min(MinY, Box.Min.Y);
			MaxY = FMath::Max(MaxY, Box.Max.Y);
		}
	}
	{
		// Manga efectiva y área frontal sumergida: la unión de las franjas en Y de lo que está bajo el agua,
		// con el calado mayor de cada franja (dos troncos en fila no empujan el doble de agua).
		TArray<double> Breaks;
		for (const FHullPiece& Piece : Pieces)
		{
			if (IsBuoyant(Piece))
			{
				const FPieceBox Box = BoxOf(Piece);
				if (Box.Min.Z < Upright.D)
				{
					Breaks.Add(Box.Min.Y);
					Breaks.Add(Box.Max.Y);
				}
			}
		}
		Breaks.Sort();
		double FrontalCm2 = 0.0, EffectiveBeam = 0.0;
		for (int32 I = 0; I + 1 < Breaks.Num(); ++I)
		{
			const double Y0 = Breaks[I], Y1 = Breaks[I + 1];
			if (Y1 <= Y0)
			{
				continue;
			}
			const double YMid = 0.5 * (Y0 + Y1);
			double Depth = 0.0;
			for (const FHullPiece& Piece : Pieces)
			{
				if (IsBuoyant(Piece))
				{
					const FPieceBox Box = BoxOf(Piece);
					if (Box.Min.Y <= YMid && Box.Max.Y >= YMid)
					{
						Depth = FMath::Max(Depth, FMath::Min(Upright.D, Box.Max.Z) - Box.Min.Z);
					}
				}
			}
			if (Depth > 0.0)
			{
				FrontalCm2 += (Y1 - Y0) * Depth;
				EffectiveBeam += Y1 - Y0;
			}
		}
		H.FrontalAreaM2 = static_cast<float>(FrontalCm2 / 10000.0);
		H.EffectiveBeamCm = static_cast<float>(EffectiveBeam);
	}
	if (WaterplaneCm2 > 0.0)
	{
		H.WaterplaneAreaM2 = static_cast<float>(WaterplaneCm2 / 10000.0);
		H.WaterlineLengthCm = static_cast<float>(MaxX - MinX);
		H.WaterlineBeamCm = static_cast<float>(MaxY - MinY);
	}

	// Estabilidad: GM adrizada (pendiente de GZ en 0) y equilibrio con las cargas actuales.
	H.GMCm = static_cast<float>(RightingSlope(Heel, 0.0));
	H.GMLongCm = static_cast<float>(RightingSlope(Trim, 0.0));
	H.KMCm = H.KGCm + H.GMCm;

	const FEquilibrium HeelEq = FindEquilibrium(Heel, MaxHeelScanDeg);
	const FEquilibrium TrimEq = FindEquilibrium(Trim, MaxTrimScanDeg);
	if (!HeelEq.bFound || FMath::Abs(HeelEq.AngleDeg) >= CapsizeHeelDeg || !TrimEq.bFound)
	{
		H.HeelDeg = HeelEq.bFound ? static_cast<float>(HeelEq.AngleDeg) : 180.0f;
		H.TrimDeg = static_cast<float>(TrimEq.AngleDeg);
		H.Verdict = EHullVerdict::Capsizes;
		return H;
	}
	H.HeelDeg = static_cast<float>(HeelEq.AngleDeg);
	H.TrimDeg = static_cast<float>(TrimEq.AngleDeg);
	H.VanishingStabilityDeg = static_cast<float>(HeelEq.VanishingDeg);

	// Francobordo: los cantos de la cubierta (piezas que llegan a lo más alto) frente a la flotación escorada y asentada.
	double DeckMinX = TNumericLimits<double>::Max(), DeckMaxX = -DeckMinX, DeckMinY = DeckMinX, DeckMaxY = -DeckMinX;
	for (const FHullPiece& Piece : Pieces)
	{
		if (!IsBuoyant(Piece))
		{
			continue;
		}
		const FPieceBox Box = BoxOf(Piece);
		if (Box.Max.Z >= DeckZ - 1.0 && Piece.Type != EHullPieceType::Mast)
		{
			DeckMinX = FMath::Min(DeckMinX, Box.Min.X);
			DeckMaxX = FMath::Max(DeckMaxX, Box.Max.X);
			DeckMinY = FMath::Min(DeckMinY, Box.Min.Y);
			DeckMaxY = FMath::Max(DeckMaxY, Box.Max.Y);
		}
	}
	if (DeckMinX > DeckMaxX)
	{
		// Solo hay mástil arriba: la cubierta es lo más alto de lo demás (o el propio mástil si no hay nada más).
		DeckMinX = MinX; DeckMaxX = MaxX; DeckMinY = MinY; DeckMaxY = MaxY;
	}
	auto EdgeFreeboard = [DeckZ](const FSection& Section, double AngleDeg, double UMin, double UMax)
	{
		const FFloat F = SolveWaterline(Section, AngleDeg);
		const double A = FMath::DegreesToRadians(AngleDeg);
		const double C = FMath::Cos(A);
		const double S = FMath::Sin(A);
		return FMath::Min(Height(UMin, DeckZ, C, S), Height(UMax, DeckZ, C, S)) - F.D;
	};
	H.FreeboardCm = static_cast<float>(FMath::Min(
		EdgeFreeboard(Heel, HeelEq.AngleDeg, DeckMinY, DeckMaxY),
		EdgeFreeboard(Trim, TrimEq.AngleDeg, DeckMinX, DeckMaxX)));

	if (H.FreeboardCm < MinFreeboardCm)
	{
		H.Verdict = EHullVerdict::Swamps;
	}
	else if (FMath::Abs(H.HeelDeg) > LevelToleranceDeg || FMath::Abs(H.TrimDeg) > LevelToleranceDeg)
	{
		H.Verdict = EHullVerdict::Lists;
	}
	else
	{
		H.Verdict = EHullVerdict::Floats;
	}
	return H;
}

FHullPerformance FHullAssemblyModel::Performance(const FHullHydrostatics& Hydro, float WindSpeedMS) const
{
	using namespace HullAssemblyDetail;
	FHullPerformance P;

	int32 Crew = 0;
	for (const FHullLoad& Load : Loads)
	{
		Crew += Load.bPassenger ? 1 : 0;
	}
	int32 OarPairs = 0, Paddles = 0, Masts = 0;
	double SailArea = 0.0;
	for (const FHullPiece& Piece : Pieces)
	{
		switch (Piece.Type)
		{
		case EHullPieceType::Oars: ++OarPairs; break;
		case EHullPieceType::Paddle: ++Paddles; break;
		case EHullPieceType::Mast: ++Masts; break;
		case EHullPieceType::Sail:
		{
			const FVector Size = EffectiveSizeCm(Piece);
			SailArea += Size.Y * Size.Z / 10000.0;
			break;
		}
		default: break;
		}
	}

	// Cada tripulante coge unos remos si quedan y, si no, una pala.
	const int32 Rowers = FMath::Min(Crew, OarPairs);
	const int32 Paddlers = FMath::Min(Crew - Rowers, Paddles);
	const double HumanThrust = Rowers * OarsThrustN + Paddlers * PaddleThrustN;
	const EHullPropulsion Human = Rowers > 0 ? EHullPropulsion::Oars : (Paddlers > 0 ? EHullPropulsion::Paddle : EHullPropulsion::None);

	// La vela necesita mástil y alguien que lleve la escota.
	P.SailAreaM2 = (Masts > 0 && Crew > 0) ? static_cast<float>(FMath::Min(SailArea, Masts * static_cast<double>(MaxSailAreaPerMastM2))) : 0.0f;
	const double Wind = FMath::Max(0.0f, WindSpeedMS);
	const double SailThrust = 0.5 * AirDensity * SailThrustCoefficient * P.SailAreaM2 * Wind * Wind;

	if (SailThrust > HumanThrust)
	{
		P.Propulsion = EHullPropulsion::Sail;
		P.ThrustN = static_cast<float>(SailThrust);
	}
	else
	{
		P.Propulsion = Human;
		P.ThrustN = static_cast<float>(HumanThrust);
	}

	const double LengthM = Hydro.WaterlineLengthCm / 100.0;
	const double BeamM = Hydro.EffectiveBeamCm / 100.0;
	if (LengthM <= 0.0 || BeamM <= 0.0)
	{
		return P;
	}
	P.Slenderness = static_cast<float>(LengthM / BeamM);
	P.DragCoefficient = 0.3f + 1.2f / FMath::Max(P.Slenderness, 0.25f);
	const double HullSpeed = HullSpeedFroude * FMath::Sqrt(Gravity * LengthM);
	P.HullSpeedCmS = static_cast<float>(HullSpeed * 100.0);
	P.CourseStability01 = FMath::Clamp((P.Slenderness - 1.0f) / 7.0f, 0.0f, 1.0f);

	if (!Hydro.IsAfloat())
	{
		return P;
	}
	double Speed = SustainedSpeedMS(P.ThrustN, P.DragCoefficient, Hydro.FrontalAreaM2, HullSpeed);
	// Escorado empuja agua de lado; anegado arrastra el agua embarcada.
	Speed *= FMath::Cos(FMath::DegreesToRadians(static_cast<double>(Hydro.HeelDeg)));
	if (Hydro.Verdict == EHullVerdict::Swamps)
	{
		Speed *= SwampedSpeedFactor;
	}
	P.MaxSpeedCmS = static_cast<float>(Speed * 100.0);
	P.TurnRateDegS = static_cast<float>(FMath::RadiansToDegrees(Speed / (TurnRadiusLengths * LengthM)));
	return P;
}

FBoatDefinition FHullAssemblyModel::ToBoatDefinition(const FHullHydrostatics& Hydro) const
{
	using namespace HullAssemblyDetail;
	FBoatDefinition D = FBoatModel::Definition(EBoatType::Raft);
	D.MeshName = TEXT("");
	D.LengthCm = FMath::Max(Hydro.WaterlineLengthCm, 1.0f);
	D.BeamCm = FMath::Max(Hydro.WaterlineBeamCm, 1.0f);
	D.HullDepthCm = FMath::Max(Hydro.DeckHeightCm, 1.0f);
	D.HullMassKg = Hydro.StructureMassKg;
	D.WaterplaneCoefficient = FMath::Clamp(Hydro.WaterplaneAreaM2 / (D.LengthCm / 100.0f * D.BeamCm / 100.0f), 0.05f, 1.0f);
	// Carga hasta el 95 % de la flotación (biblia 02 §8.2) con un tripulante a bordo.
	D.MaxCargoKg = FMath::Max(0.0f, 0.95f * Hydro.MaxBuoyancyKg - Hydro.StructureMassKg - PassengerMassKg);
	D.MetacentricHeightCm = FMath::Max(Hydro.GMCm, 1.0f);
	D.CapsizeRollDeg = FMath::Clamp(Hydro.VanishingStabilityDeg, 5.0f, CapsizeHeelDeg);
	D.bSelfDraining = true;

	// Remo o pala sin viento: el número con el que FBoatModel reparte su resistencia.
	const FHullPerformance Rowing = Performance(Hydro, 0.0f);
	D.PaddleThrustN = FMath::Max(Rowing.ThrustN, 1.0f);
	D.MaxPaddleSpeedCmS = FMath::Max(Rowing.MaxSpeedCmS, 10.0f);

	// Vela: la superficie útil y la altura de su centro sobre la flotación.
	const FHullPerformance Sailing = Performance(Hydro, 1.0f);
	D.SailAreaM2 = Sailing.SailAreaM2;
	D.SailCenterOfEffortCm = 0.0f;
	if (D.SailAreaM2 > 0.0f)
	{
		double AreaSum = 0.0, ZSum = 0.0;
		for (const FHullPiece& Piece : Pieces)
		{
			if (Piece.Type == EHullPieceType::Sail)
			{
				const FVector Size = EffectiveSizeCm(Piece);
				const double Area = Size.Y * Size.Z;
				AreaSum += Area;
				ZSum += Area * Piece.CenterCm.Z;
			}
		}
		const double KeelZ = Hydro.CenterOfMassCm.Z - Hydro.KGCm;
		D.SailCenterOfEffortCm = static_cast<float>(FMath::Max(0.0, ZSum / AreaSum - (KeelZ + Hydro.DraftCm)));
	}
	return D;
}
