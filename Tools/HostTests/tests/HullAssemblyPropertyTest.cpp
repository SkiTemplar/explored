// Propiedades del casco por piezas con cascos aleatorios (semilla fija): el veredicto no
// depende del orden de las piezas ni de dónde esté el casco, un casco reflejado escora
// al otro lado lo mismo, lo que flota desplaza su peso, los veredictos cumplen sus
// umbrales y una carga en un costado nunca escora hacia el otro.
#include "HostTest.h"

#include "Boats/HullAssemblyModel.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace
{
	struct FCase
	{
		std::vector<FHullPiece> Pieces;
		std::vector<FHullLoad> Loads;
		bool bSymmetricY = false;
	};

	FHullAssemblyModel Build(const std::vector<FHullPiece>& Pieces, const std::vector<FHullLoad>& Loads)
	{
		FHullAssemblyModel Model;
		for (const FHullPiece& Piece : Pieces)
		{
			Model.AddPiece(Piece);
		}
		for (const FHullLoad& Load : Loads)
		{
			Model.AddLoad(Load);
		}
		return Model;
	}

	FHullAssemblyModel Build(const FCase& Case) { return Build(Case.Pieces, Case.Loads); }

	FHullPiece Piece(EHullPieceType Type, const FVector& Center, const FVector& Size)
	{
		FHullPiece Out;
		Out.Type = Type;
		Out.CenterCm = Center;
		Out.SizeCm = Size;
		return Out;
	}

	/**
	 * Casco aleatorio sin piezas solapadas: una fila de troncos o haces de bambú a lo largo
	 * de X, a veces tablones encima, balancines con flotador y un mástil con vela. Si
	 * bSymmetricY, todo (piezas y cargas) es simétrico respecto a Y = 0.
	 */
	FCase RandomCase(std::mt19937& Rng, bool bSymmetricY)
	{
		std::uniform_real_distribution<double> U(0.0, 1.0);
		FCase Case;
		Case.bSymmetricY = bSymmetricY;

		const EHullPieceType Base = U(Rng) < 0.6 ? EHullPieceType::Log : EHullPieceType::Bamboo;
		const FVector BaseSize = FHullAssemblyModel::Spec(Base).DefaultSizeCm;
		const double Length = 150.0 + 350.0 * U(Rng);
		const double Width = BaseSize.Y * (0.8 + 0.6 * U(Rng));
		const double Height = BaseSize.Z * (0.8 + 0.6 * U(Rng));
		const int32 Count = 2 + static_cast<int32>(U(Rng) * 8.0);
		const double Offset = bSymmetricY ? 0.0 : (U(Rng) - 0.5) * 40.0;
		for (int32 I = 0; I < Count; ++I)
		{
			// Un tronco puede ser más corto que los demás, pero en simétrico su gemelo también.
			const int32 Twin = Count - 1 - I;
			const uint32 Salt = static_cast<uint32>(bSymmetricY ? FMath::Min(I, Twin) : I);
			const double Short = ((Salt * 2654435761u) >> 28) % 3 == 0 ? 0.75 : 1.0;
			const double Y = (I - (Count - 1) * 0.5) * Width + Offset;
			Case.Pieces.push_back(Piece(Base, FVector(0.0, Y, Height * 0.5), FVector(Length * Short, Width, Height)));
		}
		const double Beam = Count * Width;
		double DeckZ = Height;
		if (U(Rng) < 0.5)
		{
			// Cubierta de tablones cruzados sobre los troncos.
			const FVector PlankSize(20.0, Beam, 3.0);
			const int32 Planks = 2 + static_cast<int32>(U(Rng) * 5.0);
			for (int32 I = 0; I < Planks; ++I)
			{
				const double X = (I - (Planks - 1) * 0.5) * (Length * 0.8 / Planks);
				Case.Pieces.push_back(Piece(EHullPieceType::Plank, FVector(X, Offset, DeckZ + 1.5), PlankSize));
			}
			DeckZ += 3.0;
		}
		if (U(Rng) < 0.4)
		{
			// Balancines: flotador a una distancia; en asimétrico solo a estribor a veces.
			const double Reach = Beam * 0.5 + 60.0 + 80.0 * U(Rng);
			const FVector FloatSize = FHullAssemblyModel::Spec(EHullPieceType::Float).DefaultSizeCm;
			Case.Pieces.push_back(Piece(EHullPieceType::Float, FVector(0.0, Offset + Reach, FloatSize.Z * 0.5), FloatSize));
			if (bSymmetricY || U(Rng) < 0.5)
			{
				Case.Pieces.push_back(Piece(EHullPieceType::Float, FVector(0.0, Offset - Reach, FloatSize.Z * 0.5), FloatSize));
			}
		}
		if (U(Rng) < 0.4)
		{
			const double MastHeight = 200.0 + 300.0 * U(Rng);
			const double MastX = (U(Rng) - 0.5) * Length * 0.4;
			Case.Pieces.push_back(Piece(EHullPieceType::Mast, FVector(MastX, Offset, DeckZ + MastHeight * 0.5), FVector(10.0, 10.0, MastHeight)));
			Case.Pieces.push_back(Piece(EHullPieceType::Sail, FVector(MastX + 10.0, Offset, DeckZ + MastHeight * 0.6), FVector(1.0, 150.0, MastHeight * 0.6)));
		}
		if (U(Rng) < 0.5)
		{
			Case.Pieces.push_back(Piece(EHullPieceType::Paddle, FVector(0.0, Offset, DeckZ + 5.0), FVector::ZeroVector));
		}

		const int32 Loads = static_cast<int32>(U(Rng) * 5.0);
		for (int32 I = 0; I < Loads; ++I)
		{
			FHullLoad Load;
			Load.bPassenger = U(Rng) < 0.5;
			Load.MassKg = Load.bPassenger ? FHullAssemblyModel::PassengerMassKg : static_cast<float>(5.0 + 60.0 * U(Rng));
			const double X = (U(Rng) - 0.5) * Length * 0.6;
			const double Y = (U(Rng) - 0.5) * Beam * 0.8;
			const double Z = DeckZ + (Load.bPassenger ? 90.0 : 20.0);
			if (bSymmetricY)
			{
				// Mitad a cada lado, o en la crujía.
				if (U(Rng) < 0.3)
				{
					Load.CenterCm = FVector(X, Offset, Z);
					Case.Loads.push_back(Load);
				}
				else
				{
					Load.MassKg *= 0.5f;
					Load.CenterCm = FVector(X, Y, Z);
					Case.Loads.push_back(Load);
					Load.CenterCm = FVector(X, -Y, Z);
					Case.Loads.push_back(Load);
				}
			}
			else
			{
				Load.CenterCm = FVector(X, Y + Offset, Z);
				Case.Loads.push_back(Load);
			}
		}
		return Case;
	}

	bool AllFinite(const FHullHydrostatics& H)
	{
		const float Values[] = { H.TotalMassKg, H.StructureMassKg, H.MaxBuoyancyKg, H.LoadRatio, H.DisplacementM3,
			H.DraftCm, H.FreeboardCm, H.HeelDeg, H.TrimDeg, H.KGCm, H.KBCm, H.KMCm, H.GMCm, H.GMLongCm,
			H.VanishingStabilityDeg, H.WaterplaneAreaM2, H.WaterlineLengthCm, H.WaterlineBeamCm,
			H.EffectiveBeamCm, H.FrontalAreaM2, H.DeckHeightCm };
		for (const float V : Values)
		{
			if (!std::isfinite(V))
			{
				return false;
			}
		}
		return std::isfinite(H.CenterOfMassCm.X) && std::isfinite(H.CenterOfMassCm.Y) && std::isfinite(H.CenterOfMassCm.Z);
	}

	/** El veredicto sale de umbrales: cerca de uno, un redondeo distinto puede cambiarlo sin que sea un error. */
	bool NearThreshold(const FHullHydrostatics& H)
	{
		const float Tol = FHullAssemblyModel::LevelToleranceDeg;
		return FMath::Abs(FMath::Abs(H.HeelDeg) - Tol) < 0.1f || FMath::Abs(FMath::Abs(H.TrimDeg) - Tol) < 0.1f
			|| FMath::Abs(H.FreeboardCm - FHullAssemblyModel::MinFreeboardCm) < 0.1f
			|| FMath::Abs(FMath::Abs(H.HeelDeg) - FHullAssemblyModel::CapsizeHeelDeg) < 1.0f
			|| FMath::Abs(H.LoadRatio - 1.0f) < 1.0e-3f;
	}

	/** Compara dos evaluaciones que deberían ser la misma (o la reflejada con los signos dados). */
	void ExpectSame(const FHullHydrostatics& A, const FHullHydrostatics& B, float HeelSign, float TrimSign, const char* What, int32 Seed)
	{
		const std::string Tag = std::string(What) + " (semilla " + std::to_string(Seed) + ")";
		if (!NearThreshold(A) && A.Verdict != B.Verdict)
		{
			HostTest::Fail(__FILE__, __LINE__, Tag + ": veredicto " + std::to_string(static_cast<int>(A.Verdict))
				+ " frente a " + std::to_string(static_cast<int>(B.Verdict)));
			return;
		}
		if (A.Verdict != B.Verdict || A.Verdict == EHullVerdict::Empty || A.Verdict == EHullVerdict::Sinks)
		{
			return;
		}
		const auto Near = [&](double X, double Y, double Tol, const char* Field)
		{
			if (!(std::fabs(X - Y) <= Tol))
			{
				HostTest::Fail(__FILE__, __LINE__, Tag + ": " + Field + " " + std::to_string(X) + " frente a " + std::to_string(Y));
			}
		};
		Near(A.DraftCm, B.DraftCm, 0.01, "calado");
		Near(A.KGCm, B.KGCm, 0.01, "KG");
		Near(A.KBCm, B.KBCm, 0.01, "KB");
		Near(A.GMCm, B.GMCm, 0.05 + 1.0e-3 * std::fabs(A.GMCm), "GM");
		Near(A.GMLongCm, B.GMLongCm, 0.5 + 1.0e-3 * std::fabs(A.GMLongCm), "GM longitudinal");
		if (A.Verdict != EHullVerdict::Capsizes)
		{
			Near(A.HeelDeg, HeelSign * B.HeelDeg, 0.05, "escora");
			Near(A.TrimDeg, TrimSign * B.TrimDeg, 0.05, "asiento");
			Near(A.FreeboardCm, B.FreeboardCm, 0.05, "francobordo");
		}
		Near(A.WaterplaneAreaM2, B.WaterplaneAreaM2, 1.0e-4, "área de flotación");
		Near(A.EffectiveBeamCm, B.EffectiveBeamCm, 0.01, "manga efectiva");
	}

	constexpr int32 Cases = 150;
}

HOST_TEST("HullAssemblyProperty.orden_y_posicion_no_cambian_el_resultado")
{
	std::mt19937 Rng(20260929u);
	for (int32 Seed = 0; Seed < Cases; ++Seed)
	{
		const FCase Case = RandomCase(Rng, Seed % 2 == 0);
		const FHullHydrostatics H = Build(Case).Evaluate();
		EXPECT_TRUE(AllFinite(H));

		// Mismo casco dos veces: determinista bit a bit.
		const FHullHydrostatics Again = Build(Case).Evaluate();
		EXPECT_TRUE(H.Verdict == Again.Verdict && H.HeelDeg == Again.HeelDeg && H.DraftCm == Again.DraftCm && H.GMCm == Again.GMCm);

		// Piezas y cargas en otro orden.
		FCase Shuffled = Case;
		std::shuffle(Shuffled.Pieces.begin(), Shuffled.Pieces.end(), Rng);
		std::shuffle(Shuffled.Loads.begin(), Shuffled.Loads.end(), Rng);
		ExpectSame(H, Build(Shuffled).Evaluate(), 1.0f, 1.0f, "orden", Seed);

		// Todo el casco desplazado: nada que se mida desde la quilla cambia.
		std::uniform_real_distribution<double> D(-400.0, 400.0);
		const FVector Shift(D(Rng), D(Rng), D(Rng) * 0.25);
		FCase Moved = Case;
		for (FHullPiece& P : Moved.Pieces) { P.CenterCm += Shift; }
		for (FHullLoad& L : Moved.Loads) { L.CenterCm += Shift; }
		const FHullHydrostatics HM = Build(Moved).Evaluate();
		ExpectSame(H, HM, 1.0f, 1.0f, "traslación", Seed);
		if (H.TotalMassKg > 0.0f)
		{
			EXPECT_NEAR((HM.CenterOfMassCm - H.CenterOfMassCm - Shift).Size(), 0.0, 0.01);
		}
	}
}

HOST_TEST("HullAssemblyProperty.reflejar_el_casco_refleja_escora_y_asiento")
{
	std::mt19937 Rng(1729u);
	for (int32 Seed = 0; Seed < Cases; ++Seed)
	{
		const FCase Case = RandomCase(Rng, false);
		const FHullHydrostatics H = Build(Case).Evaluate();

		FCase MirrorY = Case;
		for (FHullPiece& P : MirrorY.Pieces) { P.CenterCm.Y = -P.CenterCm.Y; }
		for (FHullLoad& L : MirrorY.Loads) { L.CenterCm.Y = -L.CenterCm.Y; }
		ExpectSame(H, Build(MirrorY).Evaluate(), -1.0f, 1.0f, "reflejo babor–estribor", Seed);

		FCase MirrorX = Case;
		for (FHullPiece& P : MirrorX.Pieces) { P.CenterCm.X = -P.CenterCm.X; }
		for (FHullLoad& L : MirrorX.Loads) { L.CenterCm.X = -L.CenterCm.X; }
		ExpectSame(H, Build(MirrorX).Evaluate(), 1.0f, -1.0f, "reflejo proa–popa", Seed);
	}
}

HOST_TEST("HullAssemblyProperty.veredictos_cumplen_sus_umbrales_y_arquimedes")
{
	std::mt19937 Rng(424242u);
	int32 Seen[6] = {};
	for (int32 Seed = 0; Seed < Cases * 2; ++Seed)
	{
		FCase Case = RandomCase(Rng, Seed % 3 == 0);
		// Una de cada cuatro, sobrecargada a propósito para ver hundimientos.
		if (Seed % 4 == 3)
		{
			FHullLoad Heavy;
			Heavy.MassKg = 400.0f + 200.0f * static_cast<float>(Seed % 7);
			Heavy.CenterCm = FVector(0.0, Case.bSymmetricY ? 0.0 : 5.0, 30.0);
			Case.Loads.push_back(Heavy);
		}
		const FHullHydrostatics H = Build(Case).Evaluate();
		++Seen[static_cast<int32>(H.Verdict)];
		EXPECT_TRUE(AllFinite(H));

		if (H.LoadRatio > 1.0f + 1.0e-4f)
		{
			EXPECT_TRUE(H.Verdict == EHullVerdict::Sinks);
		}
		if (H.Verdict == EHullVerdict::Sinks)
		{
			EXPECT_GE(H.LoadRatio, 1.0f - 1.0e-4f);
			continue;
		}
		EXPECT_TRUE(H.Verdict != EHullVerdict::Empty);
		// Arquímedes: lo que flota desplaza exactamente su peso.
		EXPECT_NEAR(H.DisplacementM3 * FHullAssemblyModel::WaterDensity, H.TotalMassKg, 1.0e-3 * H.TotalMassKg + 1.0e-3);
		EXPECT_GE(H.DraftCm, 0.0f);
		EXPECT_LE(H.KBCm, H.DraftCm + 1.0e-3f);
		EXPECT_NEAR(H.KMCm, H.KGCm + H.GMCm, 1.0e-3);

		switch (H.Verdict)
		{
		case EHullVerdict::Floats:
			EXPECT_LE(FMath::Abs(H.HeelDeg), FHullAssemblyModel::LevelToleranceDeg);
			EXPECT_LE(FMath::Abs(H.TrimDeg), FHullAssemblyModel::LevelToleranceDeg);
			EXPECT_GE(H.FreeboardCm, FHullAssemblyModel::MinFreeboardCm);
			EXPECT_GE(H.GMCm, 0.0f);
			break;
		case EHullVerdict::Lists:
			EXPECT_GE(H.FreeboardCm, FHullAssemblyModel::MinFreeboardCm);
			EXPECT_TRUE(FMath::Abs(H.HeelDeg) > FHullAssemblyModel::LevelToleranceDeg
				|| FMath::Abs(H.TrimDeg) > FHullAssemblyModel::LevelToleranceDeg);
			break;
		case EHullVerdict::Swamps:
			EXPECT_LT(H.FreeboardCm, FHullAssemblyModel::MinFreeboardCm);
			EXPECT_LT(FMath::Abs(H.HeelDeg), FHullAssemblyModel::CapsizeHeelDeg);
			break;
		default:
			break;
		}
		if (H.IsAfloat())
		{
			EXPECT_LT(FMath::Abs(H.HeelDeg), FHullAssemblyModel::CapsizeHeelDeg);
		}
	}
	// El generador tiene que cubrir los casos interesantes, o el test no prueba nada.
	EXPECT_GT(Seen[static_cast<int32>(EHullVerdict::Floats)], 0);
	EXPECT_GT(Seen[static_cast<int32>(EHullVerdict::Lists)], 0);
	EXPECT_GT(Seen[static_cast<int32>(EHullVerdict::Sinks)], 0);
}

HOST_TEST("HullAssemblyProperty.casco_simetrico_adrizado_y_la_carga_escora_hacia_su_lado")
{
	std::mt19937 Rng(31337u);
	for (int32 Seed = 0; Seed < Cases; ++Seed)
	{
		const FCase Case = RandomCase(Rng, true);
		const FHullAssemblyModel Model = Build(Case);
		const FHullHydrostatics H = Model.Evaluate();
		if (!H.IsAfloat())
		{
			continue;
		}
		// Simétrico en Y: brazo adrizante antisimétrico y, con GM positiva, sin escora. Con GM
		// negativa adrizado es inestable y se queda en su ángulo de apoyo (lolling) a un lado.
		EXPECT_NEAR(H.CenterOfMassCm.Y, 0.0, 1.0e-3);
		if (H.GMCm <= 0.5f)
		{
			if (H.GMCm < 0.0f)
			{
				EXPECT_GT(FMath::Abs(H.HeelDeg), 0.0f);
				EXPECT_LE(Model.RightingArmCm(0.5f * H.HeelDeg) * H.HeelDeg, 0.0f);
			}
			continue;
		}
		EXPECT_NEAR(H.HeelDeg, 0.0, 0.05);
		EXPECT_NEAR(Model.RightingArmCm(0.0f), 0.0, 1.0e-3);
		for (const float A : { 3.0f, 10.0f, 25.0f, 40.0f })
		{
			EXPECT_NEAR(Model.RightingArmCm(A), -Model.RightingArmCm(-A), 1.0e-2 + 1.0e-4 * std::fabs(Model.RightingArmCm(A)));
		}

		// Una carga pequeña a estribor escora a estribor (o vuelca), nunca a babor, y hunde más.
		FCase Loaded = Case;
		FHullLoad Crate;
		Crate.MassKg = 10.0f;
		Crate.CenterCm = FVector(H.CenterOfBuoyancyCm.X, H.WaterlineBeamCm * 0.4, H.DeckHeightCm + 20.0);
		Loaded.Loads.push_back(Crate);
		const FHullHydrostatics HL = Build(Loaded).Evaluate();
		if (HL.Verdict == EHullVerdict::Sinks)
		{
			continue;
		}
		if (HL.Verdict != EHullVerdict::Capsizes)
		{
			EXPECT_GE(HL.HeelDeg, -0.05f);
		}
		EXPECT_GE(HL.DraftCm, H.DraftCm - 1.0e-3f);
	}
}
