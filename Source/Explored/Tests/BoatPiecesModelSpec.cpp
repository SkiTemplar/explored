#include "Misc/AutomationTest.h"

#include "Boats/BoatModel.h"
#include "Boats/BoatPiecesModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BoatPiecesSpecDetail
{
	FBoatLoadMass Mass(float Kg, double X, double Y, double Z)
	{
		FBoatLoadMass M;
		M.MassKg = Kg;
		M.CenterCm = FVector(X, Y, Z);
		return M;
	}

	/** Tripulantes sentados en el eje y una carga en el fondo. */
	FBoatLoadout Crew(int32 People, float CargoKg = 0.0f)
	{
		FBoatLoadout L;
		for (int32 I = 0; I < People; ++I)
		{
			L.Masses.Add(Mass(FBoatModel::CrewMassKg, -100.0 + 60.0 * I, 0.0, FBoatPiecesModel::CrewSeatedHeightCm));
		}
		if (CargoKg > 0.0f)
		{
			L.Masses.Add(Mass(CargoKg, 0.0, 0.0, 15.0));
		}
		return L;
	}

	/** Carga en el eje que deja el casco con una fracción dada de su flotabilidad. */
	FBoatLoadout AtLoadRatio(const FBoatPiecesModel& Model, float Ratio)
	{
		const FBoatHullReport Empty = Model.Evaluate(FBoatLoadout());
		FBoatLoadout L;
		L.Masses.Add(Mass(Ratio * Empty.MaxBuoyancyKg - Empty.StructureMassKg, 0.0, 0.0, 15.0));
		return L;
	}

	int32 IndexOf(const FBoatPiecesModel& Model, EBoatPieceType Type)
	{
		for (int32 I = 0; I < Model.GetPieces().Num(); ++I)
		{
			if (Model.GetPieces()[I].Type == Type)
			{
				return I;
			}
		}
		return INDEX_NONE;
	}

	bool Has(uint32 Issues, EBoatHullIssue Issue)
	{
		return (Issues & static_cast<uint32>(Issue)) != 0;
	}

	/** Quita todas las piezas de un tipo. */
	void RemoveAll(FBoatPiecesModel& Model, EBoatPieceType Type)
	{
		for (int32 I = IndexOf(Model, Type); I != INDEX_NONE; I = IndexOf(Model, Type))
		{
			Model.RemovePiece(I);
		}
	}
}

BEGIN_DEFINE_SPEC(FBoatPiecesModelSpec, "Explored.Boats.Pieces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBoatPiecesModelSpec)

void FBoatPiecesModelSpec::Define()
{
	using namespace BoatPiecesSpecDetail;

	Describe("Catálogo", [this]()
	{
		It("tiene las diez piezas de la biblia 02 §8.1 con id, masa e integridad", [this]()
		{
			TestEqual(TEXT("Diez tipos"), static_cast<int32>(EBoatPieceType::Count), 10);
			for (int32 I = 0; I < static_cast<int32>(EBoatPieceType::Count); ++I)
			{
				const EBoatPieceType Type = static_cast<EBoatPieceType>(I);
				const FBoatPieceSpec& S = FBoatPiecesModel::Spec(Type);
				TestTrue(TEXT("Tipo coherente"), S.Type == Type);
				TestTrue(TEXT("Con id"), FCString::Strlen(S.Id) > 0);
				TestTrue(TEXT("Con masa"), S.MassKg > 0.0f);
				TestTrue(TEXT("Integridad 1–100"), S.BaseIntegrity >= 1 && S.BaseIntegrity <= 100);
				EBoatPieceType Back = EBoatPieceType::Count;
				TestTrue(TEXT("El nombre vuelve"), LexFromString(Back, LexToString(Type)) && Back == Type);
				const bool bFloats = Type == EBoatPieceType::HullPlank || Type == EBoatPieceType::Outrigger;
				TestEqual(TEXT("Solo tablón y balancín flotan"), S.BuoyancyLiters > 0.0f, bFloats);
				TestEqual(TEXT("Solo la cubierta carga"), S.CargoKg > 0.0f, Type == EBoatPieceType::Deck);
				TestEqual(TEXT("Solo la vela tiene superficie"), S.SailAreaM2 > 0.0f, Type == EBoatPieceType::Sail);
			}
			EBoatPieceType Out = EBoatPieceType::Mast;
			TestFalse(TEXT("Nombre desconocido"), LexFromString(Out, TEXT("Anchor")));
			TestFalse(TEXT("Sin nombre"), LexFromString(Out, nullptr));
			TestTrue(TEXT("Sin tocar la salida"), Out == EBoatPieceType::Mast);
		});

		It("escala masa y litros con la dimensión principal y sanea tamaños rotos", [this]()
		{
			FBoatPiece Plank;
			Plank.Type = EBoatPieceType::HullPlank;
			Plank.SizeCm = FVector(400.0, 0.0, -3.0);
			const FBoatPieceSpec& S = FBoatPiecesModel::Spec(EBoatPieceType::HullPlank);
			TestEqual(TEXT("El doble de largo pesa el doble"), FBoatPiecesModel::PieceMassKg(Plank), 2.0f * S.MassKg, 1e-4f);
			TestEqual(TEXT("y encierra el doble"), FBoatPiecesModel::PieceBuoyancyLiters(Plank), 2.0f * S.BuoyancyLiters, 1e-3f);
			TestEqual(TEXT("Grosor por defecto"), FBoatPiecesModel::EffectiveSizeCm(Plank).Z, S.DefaultSizeCm.Z);

			Plank.SizeCm = FVector(NAN, 1.0e9, 0.0);
			const FVector Clean = FBoatPiecesModel::EffectiveSizeCm(Plank);
			TestEqual(TEXT("NaN toma el valor por defecto"), Clean.X, S.DefaultSizeCm.X);
			TestEqual(TEXT("Lo enorme se recorta"), Clean.Y, static_cast<double>(FBoatPiecesModel::MaxPieceSizeCm));

			FBoatPiece Sail;
			Sail.Type = EBoatPieceType::Sail;
			Sail.SizeCm = FVector(1.0, 400.0, 450.0);
			TestEqual(TEXT("La vela escala con su área"), FBoatPiecesModel::ScaleOf(Sail), 4.0f, 1e-4f);
		});

		It("rechaza piezas con el centro no finito, de tipo inexistente o por encima del tope", [this]()
		{
			FBoatPiecesModel Model;
			FBoatPiece Bad;
			Bad.CenterCm = FVector(INFINITY, 0.0, 0.0);
			TestEqual(TEXT("Centro infinito"), Model.AddPiece(Bad), INDEX_NONE);
			Bad.CenterCm = FVector::ZeroVector;
			Bad.Type = EBoatPieceType::Count;
			TestEqual(TEXT("Tipo inexistente"), Model.AddPiece(Bad), INDEX_NONE);
			FBoatPiece Deck;
			Deck.Type = EBoatPieceType::Deck;
			for (int32 I = 0; I < FBoatPiecesModel::MaxPieces; ++I)
			{
				Model.AddPiece(Deck);
			}
			TestEqual(TEXT("Tope de piezas"), Model.AddPiece(Deck), INDEX_NONE);
			TestEqual(TEXT("Se queda en el tope"), Model.GetPieces().Num(), FBoatPiecesModel::MaxPieces);
			TestFalse(TEXT("Quitar un índice que no existe"), Model.RemovePiece(-1));
			TestFalse(TEXT("ni uno pasado el final"), Model.RemovePiece(FBoatPiecesModel::MaxPieces));
		});
	});

	Describe("Planos canónicos", [this]()
	{
		It("están completos, flotan con su tripulante y calan menos que el calado de diseño", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				const EBoatType Type = static_cast<EBoatType>(T);
				const FBoatPiecesModel Model = FBoatPiecesModel::Blueprint(Type);
				const FBoatHullReport R = Model.EvaluateWithCrew();
				const FString Name = LexToString(Type);
				TestTrue(Name + TEXT(": completo"), R.Issues == 0u);
				TestTrue(Name + TEXT(": flota"), R.Verdict == EBoatHullVerdict::Floats);
				TestTrue(Name + TEXT(": calado < diseño"), R.EquilibriumDraftCm < R.DesignDraftCm);
				TestEqual(Name + TEXT(": diseño al 60 % del puntal"), R.DesignDraftCm, 0.6f * R.DepthCm, 1e-3f);
				TestTrue(Name + TEXT(": estable"), R.EffectiveGMCm > 0.0f && FMath::Abs(R.HeelDeg) < 5.0f);
				TestTrue(Name + TEXT(": con amarre"), Model.CanMoor());
				TestTrue(Name + TEXT(": con banco de remo"), Model.RowingSeats() >= 1);
				TestTrue(Name + TEXT(": uniones cuaderna–tablón"), Model.GetJoints().Num() > 0);
				TestEqual(Name + TEXT(": intacto"), Model.Integrity01(), 1.0f);
				for (int32 P = 0; P < static_cast<int32>(EBoatPieceType::Count); ++P)
				{
					const EBoatPieceType Piece = static_cast<EBoatPieceType>(P);
					TestEqual(Name + TEXT(": recuento por tipo"), Model.CountOf(Piece), FBoatPiecesModel::BlueprintCount(Type, Piece));
				}
			}
		});

		It("la flotabilidad es la suma de litros de tablones y flotadores × 1,025", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				const FBoatPiecesModel Model = FBoatPiecesModel::Blueprint(static_cast<EBoatType>(T));
				double Liters = 0.0;
				for (const FBoatPiece& Piece : Model.GetPieces())
				{
					Liters += FBoatPiecesModel::PieceBuoyancyLiters(Piece);
				}
				TestEqual(TEXT("Σ litros × 1,025"), Model.EvaluateWithCrew().MaxBuoyancyKg, static_cast<float>(Liters * 1.025), 0.01f);
			}
			TestEqual(TEXT("Canoa: cuatro tablones de 4 m, 560 L"),
				FBoatPiecesModel::Blueprint(EBoatType::Canoe).EvaluateWithCrew().MaxBuoyancyKg, 574.0f, 0.01f);
		});

		It("solo el balancín y el «Limón» llevan vela y solo el balancín lleva flotador", [this]()
		{
			TestFalse(TEXT("Balsa sin vela"), FBoatPiecesModel::Blueprint(EBoatType::Raft).CanRaiseSail());
			TestFalse(TEXT("Canoa sin vela"), FBoatPiecesModel::Blueprint(EBoatType::Canoe).CanRaiseSail());
			TestTrue(TEXT("Balancín con vela"), FBoatPiecesModel::Blueprint(EBoatType::Outrigger).CanRaiseSail());
			TestTrue(TEXT("«Limón» con vela"), FBoatPiecesModel::Blueprint(EBoatType::Limon).CanRaiseSail());
			TestEqual(TEXT("Un flotador"), FBoatPiecesModel::BlueprintCount(EBoatType::Outrigger, EBoatPieceType::Outrigger), 1);
			TestEqual(TEXT("Casco doble: dos quillas"), FBoatPiecesModel::BlueprintCount(EBoatType::Limon, EBoatPieceType::Keel), 2);
		});

		It("FBoatModel navega con la ficha que sale de las piezas", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				const EBoatType Type = static_cast<EBoatType>(T);
				const FBoatHullReport R = FBoatPiecesModel::Blueprint(Type).EvaluateWithCrew();
				const FBoatDefinition& D = FBoatModel::Definition(Type);
				TestEqual(TEXT("Masa del casco"), D.HullMassKg, R.StructureMassKg, 1e-3f);
				TestEqual(TEXT("Puntal = la cuaderna"), D.HullDepthCm, R.DepthCm, 1e-3f);
				TestEqual(TEXT("Área de flotación"), D.WaterplaneAreaM2(), R.WaterplaneAreaM2, 1e-3f);
				FBoatModel Boat(Type, FVector::ZeroVector, 0.0f);
				Boat.SetCrewAboard(true);
				TestEqual(TEXT("Mismo calado que las piezas"), Boat.EquilibriumDraftCm(), R.EquilibriumDraftCm, 0.01f);
				TestEqual(TEXT("Anega con el mismo agua"), Boat.SwampWaterKg(), R.SwampWaterKg, 0.5f);
				TestTrue(TEXT("La carga cabe en las cubiertas"), D.MaxCargoKg <= R.DeckCargoKg + 1e-3f);
			}
		});

		It("el «Limón» exige las cuatro piezas del Albatros y los demás ninguna", [this]()
		{
			TestTrue(TEXT("Cuatro partes"), FBoatPiecesModel::RequiredShipPartsMask(EBoatType::Limon) == FBoatPiecesModel::AllShipPartsMask);
			TestFalse(TEXT("Sin cola no"), FBoatPiecesModel::HasShipPartsFor(EBoatType::Limon, 0xBu));
			TestFalse(TEXT("Sin motor no"), FBoatPiecesModel::HasShipPartsFor(EBoatType::Limon, 0x7u));
			TestFalse(TEXT("Sin fuselaje no"), FBoatPiecesModel::HasShipPartsFor(EBoatType::Limon, 0xEu));
			TestTrue(TEXT("Con las cuatro sí"), FBoatPiecesModel::HasShipPartsFor(EBoatType::Limon, 0xFu));
			TestTrue(TEXT("Bits de más no estorban"), FBoatPiecesModel::HasShipPartsFor(EBoatType::Limon, 0xFFu));
			for (const EBoatType Type : {EBoatType::Raft, EBoatType::Canoe, EBoatType::Outrigger})
			{
				TestTrue(TEXT("Los demás sin partes"), FBoatPiecesModel::HasShipPartsFor(Type, 0u));
			}
		});
	});

	Describe("Casco incompleto", [this]()
	{
		It("sin piezas no hay quilla, cuadernas ni tablones, y no se puede botar", [this]()
		{
			const FBoatPiecesModel Empty;
			const FBoatHullReport R = Empty.Evaluate(FBoatLoadout());
			TestTrue(TEXT("Incompleto"), R.Verdict == EBoatHullVerdict::Incomplete);
			TestTrue(TEXT("Sin quilla"), Has(R.Issues, EBoatHullIssue::NoKeel));
			TestTrue(TEXT("Sin cuadernas"), Has(R.Issues, EBoatHullIssue::NoFrames));
			TestTrue(TEXT("Sin tablones"), Has(R.Issues, EBoatHullIssue::NoPlanks));
			TestFalse(TEXT("No está a flote"), R.IsAfloat());
			TestEqual(TEXT("Sin masa"), R.TotalMassKg, 0.0f);
		});

		It("a la canoa sin quilla, sin tablones o con un tablón suelto le falta algo", [this]()
		{
			FBoatPiecesModel NoKeel = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			NoKeel.RemovePiece(IndexOf(NoKeel, EBoatPieceType::Keel));
			const uint32 KeelIssues = NoKeel.FindIssues();
			TestTrue(TEXT("Sin quilla"), Has(KeelIssues, EBoatHullIssue::NoKeel));
			TestFalse(TEXT("Las cuadernas siguen unidas a los tablones"), Has(KeelIssues, EBoatHullIssue::FrameUnattached));
			TestTrue(TEXT("Incompleto aunque flotaría"), NoKeel.EvaluateWithCrew().Verdict == EBoatHullVerdict::Incomplete);

			FBoatPiecesModel NoPlanks = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			RemoveAll(NoPlanks, EBoatPieceType::HullPlank);
			TestTrue(TEXT("Sin tablones"), Has(NoPlanks.FindIssues(), EBoatHullIssue::NoPlanks));
			TestEqual(TEXT("Sin uniones"), NoPlanks.GetJoints().Num(), 0);
			TestEqual(TEXT("Sin flotabilidad"), NoPlanks.EvaluateWithCrew().MaxBuoyancyKg, 0.0f);

			FBoatPiecesModel Loose = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			FBoatPiece Plank;
			Plank.Type = EBoatPieceType::HullPlank;
			Plank.CenterCm = FVector(0.0, 300.0, 14.0);
			Loose.AddPiece(Plank);
			TestTrue(TEXT("Solo el tablón suelto"), Loose.FindIssues() == static_cast<uint32>(EBoatHullIssue::PlankUnsupported));
			TestEqual(TEXT("Sin entrada de agua por sobrecarga"), Loose.EvaluateWithCrew().OverloadIngressKgS, 0.0f);
		});

		It("una vela sin mástil, un mástil al aire o un balancín sin botalón no valen", [this]()
		{
			FBoatPiecesModel NoMast = FBoatPiecesModel::Blueprint(EBoatType::Outrigger);
			NoMast.RemovePiece(IndexOf(NoMast, EBoatPieceType::Mast));
			TestTrue(TEXT("Vela sin mástil"), NoMast.FindIssues() == static_cast<uint32>(EBoatHullIssue::SailWithoutMast));
			TestFalse(TEXT("No se iza"), NoMast.CanRaiseSail());
			TestEqual(TEXT("Sin superficie vélica útil"), NoMast.Evaluate(FBoatLoadout()).SailAreaM2, 0.0f);

			FBoatPiecesModel FloatingMast = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			FBoatPiece Mast;
			Mast.Type = EBoatPieceType::Mast;
			Mast.CenterCm = FVector(0.0, 0.0, 500.0);
			FloatingMast.AddPiece(Mast);
			TestTrue(TEXT("Mástil al aire"), FloatingMast.FindIssues() == static_cast<uint32>(EBoatHullIssue::MastUnstepped));

			FBoatPiecesModel NoBooms = FBoatPiecesModel::Blueprint(EBoatType::Outrigger);
			for (int32 I = NoBooms.GetPieces().Num() - 1; I >= 0; --I)
			{
				const FBoatPiece& P = NoBooms.GetPieces()[I];
				if (P.Type == EBoatPieceType::Frame && P.CenterCm.Y > 0.0)
				{
					NoBooms.RemovePiece(I);
				}
			}
			TestTrue(TEXT("Balancín sin botalón"), Has(NoBooms.FindIssues(), EBoatHullIssue::OutriggerUnattached));

			FBoatPiecesModel StrayFrame = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			FBoatPiece Frame;
			Frame.Type = EBoatPieceType::Frame;
			Frame.CenterCm = FVector(0.0, 0.0, -300.0);
			StrayFrame.AddPiece(Frame);
			TestTrue(TEXT("Cuaderna suelta"), StrayFrame.FindIssues() == static_cast<uint32>(EBoatHullIssue::FrameUnattached));
		});

		It("quitar una cuaderna deja el casco entero y las demás uniones con su integridad", [this]()
		{
			FBoatPiecesModel Model = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			// Golpe a popa: daña las uniones de la cuaderna de popa.
			Model.ApplyImpact(200.0f, FVector(-120.0, 34.0, 14.0));
			const float Before = Model.Integrity01();
			TestTrue(TEXT("Dañada"), Before < 1.0f);
			const int32 Joints = Model.GetJoints().Num();
			// Quita la cuaderna de proa (X = 120), que el golpe no ha tocado.
			int32 Bow = INDEX_NONE;
			for (int32 I = 0; I < Model.GetPieces().Num(); ++I)
			{
				if (Model.GetPieces()[I].Type == EBoatPieceType::Frame && Model.GetPieces()[I].CenterCm.X > 100.0)
				{
					Bow = I;
				}
			}
			TestTrue(TEXT("Se quita"), Model.RemovePiece(Bow));
			TestTrue(TEXT("Sigue completa"), Model.FindIssues() == 0u);
			TestEqual(TEXT("Cuatro uniones menos"), Model.GetJoints().Num(), Joints - 4);
			double Lost = 0.0;
			for (const FBoatHullJoint& J : Model.GetJoints())
			{
				Lost += J.MaxIntegrity - J.Integrity;
				TestTrue(TEXT("Índices renumerados válidos"), Model.GetPieces().IsValidIndex(J.Frame) && Model.GetPieces().IsValidIndex(J.Plank)
					&& Model.GetPieces()[J.Frame].Type == EBoatPieceType::Frame && Model.GetPieces()[J.Plank].Type == EBoatPieceType::HullPlank);
			}
			TestTrue(TEXT("El daño de popa se conserva"), Lost > 0.0);
		});
	});

	Describe("Sobrecarga", [this]()
	{
		It("flota hasta el 95 %, embarca agua a 2 kg/s hasta el 115 % y se hunde por encima", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			const FBoatHullReport Light = Canoe.Evaluate(AtLoadRatio(Canoe, 0.94f));
			TestTrue(TEXT("94 %: flota"), Light.Verdict == EBoatHullVerdict::Floats);
			TestEqual(TEXT("sin entrada"), Light.OverloadIngressKgS, 0.0f);

			const FBoatHullReport Heavy = Canoe.Evaluate(AtLoadRatio(Canoe, 0.96f));
			TestTrue(TEXT("96 %: embarca agua"), Heavy.Verdict == EBoatHullVerdict::TakingWater);
			TestEqual(TEXT("2 kg/s"), Heavy.OverloadIngressKgS, 2.0f);
			TestTrue(TEXT("sigue a flote"), Heavy.IsAfloat());

			TestTrue(TEXT("114 %: aún embarca"), Canoe.Evaluate(AtLoadRatio(Canoe, 1.14f)).Verdict == EBoatHullVerdict::TakingWater);
			const FBoatHullReport Sunk = Canoe.Evaluate(AtLoadRatio(Canoe, 1.16f));
			TestTrue(TEXT("116 %: se hunde"), Sunk.Verdict == EBoatHullVerdict::Sinks);
			TestFalse(TEXT("no está a flote"), Sunk.IsAfloat());
			TestEqual(TEXT("El calado no pasa del puntal"), Sunk.EquilibriumDraftCm, Sunk.DepthCm);
		});

		It("cuatro tripulantes y la carga máxima anegan la canoa, pero no el «Limón» (biblia 08 §2.5)", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			const float CanoeCargo = FBoatModel::Definition(EBoatType::Canoe).MaxCargoKg;
			TestTrue(TEXT("Uno y su carga caben"), Canoe.Evaluate(Crew(1, CanoeCargo)).Verdict == EBoatHullVerdict::Floats);
			TestTrue(TEXT("Cuatro y la carga la anegan"), Canoe.Evaluate(Crew(4, CanoeCargo)).Verdict == EBoatHullVerdict::TakingWater);

			const FBoatPiecesModel Outrigger = FBoatPiecesModel::Blueprint(EBoatType::Outrigger);
			const float Decks = Outrigger.Evaluate(FBoatLoadout()).DeckCargoKg;
			TestTrue(TEXT("Balancín: cuatro y las cubiertas llenas pasan del 95 % (biblia 08 §5.4)"),
				Outrigger.Evaluate(Crew(4, Decks)).LoadRatio > FBoatPiecesModel::TakingWaterLoadRatio);

			const FBoatPiecesModel Limon = FBoatPiecesModel::Blueprint(EBoatType::Limon);
			TestTrue(TEXT("«Limón»: cuatro y su carga"),
				Limon.Evaluate(Crew(4, FBoatModel::Definition(EBoatType::Limon).MaxCargoKg)).Verdict == EBoatHullVerdict::Floats);
		});

		It("el agua embarcada cuenta en la masa y el agua que la anega es la flotabilidad que sobra", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			const FBoatHullReport Dry = Canoe.Evaluate(Crew(1));
			FBoatLoadout Wet = Crew(1);
			Wet.WaterInHullKg = Dry.SwampWaterKg;
			const FBoatHullReport Swamped = Canoe.Evaluate(Wet);
			TestEqual(TEXT("Con el agua de anegar llega al 100 %"), Swamped.LoadRatio, 1.0f, 1e-4f);
			TestTrue(TEXT("Embarca"), Swamped.Verdict == EBoatHullVerdict::TakingWater);
			TestEqual(TEXT("La borda a ras"), Swamped.EquilibriumDraftCm, Swamped.DepthCm, 0.01f);
			FBoatLoadout Broken = Crew(1);
			Broken.WaterInHullKg = NAN;
			Broken.Masses.Add(Mass(-50.0f, 0.0, 0.0, 0.0));
			Broken.Masses.Add(Mass(INFINITY, 0.0, 0.0, 0.0));
			Broken.Masses.Add(Mass(50.0f, NAN, 0.0, 0.0));
			TestEqual(TEXT("Masas rotas se ignoran"), Canoe.Evaluate(Broken).TotalMassKg, Dry.TotalMassKg, 1e-3f);
		});
	});

	Describe("Vuelco", [this]()
	{
		It("dos de pie en la borda de la canoa la vuelcan; sentados en el eje, no", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			FBoatLoadout Standing;
			Standing.Masses.Add(Mass(75.0f, -50.0, 34.0, 140.0));
			Standing.Masses.Add(Mass(75.0f, 50.0, 34.0, 140.0));
			const FBoatHullReport R = Canoe.Evaluate(Standing);
			TestTrue(TEXT("Vuelca"), R.Verdict == EBoatHullVerdict::Capsizes);
			TestTrue(TEXT("Sin estabilidad inicial o escora de más de 55°"), R.EffectiveGMCm <= 0.0f || FMath::Abs(R.HeelDeg) > 55.0f);
			TestTrue(TEXT("Sentados flota"), Canoe.Evaluate(Crew(2)).Verdict == EBoatHullVerdict::Floats);
		});

		It("el balancín quita el 60 % del momento que escora hacia su banda y nada del otro lado", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			FBoatPiecesModel WithFloat = Canoe;
			FBoatPiece Float;
			Float.Type = EBoatPieceType::Outrigger;
			Float.CenterCm = FVector(0.0, 44.0, 25.0);
			WithFloat.AddPiece(Float);
			TestTrue(TEXT("Unido a las cuadernas"), WithFloat.FindIssues() == 0u);

			FBoatLoadout Starboard;
			Starboard.Masses.Add(Mass(75.0f, 0.0, 30.0, 30.0));
			FBoatLoadout Port;
			Port.Masses.Add(Mass(75.0f, 0.0, -30.0, 30.0));
			const float Bare = Canoe.Evaluate(Starboard).HeelingMomentNm;
			TestTrue(TEXT("Escora a estribor"), Bare > 0.0f);
			TestEqual(TEXT("Hacia el flotador: 40 %"), WithFloat.Evaluate(Starboard).HeelingMomentNm, 0.4f * Bare, 0.01f);
			TestEqual(TEXT("Hacia el otro lado: igual"), WithFloat.Evaluate(Port).HeelingMomentNm, -Bare, 0.01f);
			TestTrue(TEXT("Menos escora"), FMath::Abs(WithFloat.Evaluate(Starboard).HeelDeg) < FMath::Abs(Canoe.Evaluate(Starboard).HeelDeg));
		});

		It("una galerna de través con la vela izada vuelca el balancín por la banda sin flotador", [this]()
		{
			const FBoatPiecesModel Outrigger = FBoatPiecesModel::Blueprint(EBoatType::Outrigger);
			FBoatLoadout Calm = Crew(1);
			Calm.bSailRaised = true;
			Calm.BeamWindMS = 6.0f;
			TestTrue(TEXT("Con alisio navega"), Outrigger.Evaluate(Calm).Verdict == EBoatHullVerdict::Floats);
			FBoatLoadout Gale = Calm;
			Gale.BeamWindMS = -25.0f;
			const FBoatHullReport Away = Outrigger.Evaluate(Gale);
			TestTrue(TEXT("Galerna hacia babor: vuelca"), Away.Verdict == EBoatHullVerdict::Capsizes);
			Gale.bSailRaised = false;
			TestTrue(TEXT("Arriada aguanta"), Outrigger.Evaluate(Gale).Verdict == EBoatHullVerdict::Floats);
			Gale.bSailRaised = true;
			Gale.BeamWindMS = 25.0f;
			TestTrue(TEXT("Hacia el flotador escora menos"), FMath::Abs(Outrigger.Evaluate(Gale).HeelDeg) < FMath::Abs(Away.HeelDeg));
		});

		It("vuelca a los 4 s por encima de 55° y de inmediato por encima de 75°", [this]()
		{
			FBoatCapsizeTimer Timer;
			TestFalse(TEXT("54° no cuenta"), Timer.Step(54.0f, 10.0f));
			TestFalse(TEXT("60° durante 3,9 s"), Timer.Step(60.0f, 3.9f));
			TestFalse(TEXT("Bajar de 55° reinicia"), Timer.Step(40.0f, 0.1f));
			TestFalse(TEXT("Otra vez 3,9 s"), Timer.Step(-60.0f, 3.9f));
			TestTrue(TEXT("Pasados 4 s vuelca"), Timer.Step(-60.0f, 0.2f));
			TestTrue(TEXT("Se queda volcado"), Timer.Step(0.0f, 1.0f));
			Timer.Reset();
			TestTrue(TEXT("76° vuelca de inmediato"), Timer.Step(76.0f, 0.0f));
			Timer.Reset();
			TestFalse(TEXT("Justo 55° no cuenta"), Timer.Step(55.0f, 100.0f));
			TestFalse(TEXT("Justo 75° tampoco de inmediato"), Timer.Step(75.0f, 0.0f));
			TestFalse(TEXT("Tiempo negativo no suma"), Timer.Step(70.0f, -100.0f));
			TestFalse(TEXT("Tiempo NaN no suma"), Timer.Step(70.0f, NAN));
			TestTrue(TEXT("Escora NaN: volcado"), Timer.Step(NAN, 0.0f));
		});

		It("el casco del balancín solo no aguanta de pie: lo sostiene el peso del flotador hasta despegarse", [this]()
		{
			const FBoatPiecesModel Outrigger = FBoatPiecesModel::Blueprint(EBoatType::Outrigger);
			const FBoatHullReport Rest = Outrigger.EvaluateWithCrew();
			TestTrue(TEXT("Sin balancín sería inestable (mástil y plataforma altos)"), Rest.GMCm < 0.0f);
			TestTrue(TEXT("Con el flotador hundido, muy estable"), Rest.OutriggerBMCm > 100.0f);
			// Un tripulante asomado a babor: su momento no pasa del peso del flotador.
			FBoatLoadout Lean = Crew(1);
			Lean.Masses.Add(Mass(20.0f, 0.0, -40.0, 30.0));
			const FBoatHullReport Held = Outrigger.Evaluate(Lean);
			TestTrue(TEXT("El peso del flotador lo sujeta"), Held.Verdict == EBoatHullVerdict::Floats);
			TestTrue(TEXT("Casi adrizado"), FMath::Abs(Held.HeelDeg) < 1.0f);
			// Dos tripulantes de pie en la borda de babor sí lo levantan.
			Lean.Masses.Add(Mass(75.0f, 50.0, -40.0, 130.0));
			Lean.Masses.Add(Mass(75.0f, -50.0, -40.0, 130.0));
			TestTrue(TEXT("Despega el flotador y vuelca"), Outrigger.Evaluate(Lean).Verdict == EBoatHullVerdict::Capsizes);
		});

		It("un peso alto sin contrapeso deja el casco sin estabilidad inicial", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			FBoatLoadout High;
			High.Masses.Add(Mass(200.0f, 0.0, 0.0, 300.0));
			const FBoatHullReport R = Canoe.Evaluate(High);
			TestTrue(TEXT("GM negativa"), R.GMCm <= 0.0f);
			TestTrue(TEXT("Vuelca"), R.Verdict == EBoatHullVerdict::Capsizes);
		});
	});

	Describe("Integridad por uniones", [this]()
	{
		It("hay una unión por cada par cuaderna–tablón que se toca", [this]()
		{
			const FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			TestEqual(TEXT("Tres cuadernas × cuatro tablones"), Canoe.GetJoints().Num(), 12);
			TestEqual(TEXT("Balsa: dos travesaños × seis troncos"), FBoatPiecesModel::Blueprint(EBoatType::Raft).GetJoints().Num(), 12);
			for (const FBoatHullJoint& J : Canoe.GetJoints())
			{
				TestEqual(TEXT("Integridad de la pieza más débil"), J.Integrity, 70.0f);
			}
		});

		It("un golpe a la velocidad segura o menos no hace nada", [this]()
		{
			FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			const FVector Bow(200.0, 0.0, 10.0);
			TestEqual(TEXT("80 cm/s"), Canoe.ApplyImpact(FBoatModel::SafeImpactSpeedCmS, Bow).JointsDamaged, 0);
			TestEqual(TEXT("NaN"), Canoe.ApplyImpact(NAN, Bow).JointsDamaged, 0);
			TestEqual(TEXT("Punto infinito"), Canoe.ApplyImpact(500.0f, FVector(INFINITY, 0.0, 0.0)).JointsDamaged, 0);
			TestEqual(TEXT("Intacta"), Canoe.Integrity01(), 1.0f);
			FBoatPiecesModel Empty;
			TestEqual(TEXT("Sin uniones"), Empty.ApplyImpact(500.0f, Bow).JointsDamaged, 0);
			TestEqual(TEXT("Sin uniones está intacto"), Empty.Integrity01(), 1.0f);
		});

		It("daña la unión más cercana en proporción a la velocidad de sobra y menos las de alrededor", [this]()
		{
			const FVector Point(150.0, 34.0, 14.0);
			auto NearestLoss = [&Point](float Speed)
			{
				FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
				Canoe.ApplyImpact(Speed, Point);
				float Worst = 0.0f;
				for (const FBoatHullJoint& J : Canoe.GetJoints())
				{
					Worst = FMath::Max(Worst, J.MaxIntegrity - J.Integrity);
				}
				return Worst;
			};
			TestEqual(TEXT("0,5 m/s de sobra: 30 puntos"), NearestLoss(130.0f), 30.0f, 1e-3f);
			TestEqual(TEXT("1 m/s de sobra: 60 puntos"), NearestLoss(180.0f), 60.0f, 1e-3f);

			FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			const FBoatImpactReport R = Canoe.ApplyImpact(180.0f, Point);
			TestTrue(TEXT("Varias uniones"), R.JointsDamaged > 1);
			for (const FBoatHullJoint& J : Canoe.GetJoints())
			{
				if (J.LocationCm.X < -100.0)
				{
					TestEqual(TEXT("La popa, lejos, intacta"), J.Integrity, J.MaxIntegrity);
				}
			}
			TestEqual(TEXT("Lo perdido cuadra"), R.IntegrityLost,
				static_cast<float>(Canoe.GetJoints().Num()) * 70.0f * (1.0f - Canoe.Integrity01()), 0.01f);
		});

		It("cada unión rota es una vía de agua de 0,5 L/s y se repara con material nuevo", [this]()
		{
			FBoatPiecesModel Canoe = FBoatPiecesModel::Blueprint(EBoatType::Canoe);
			const FBoatImpactReport R = Canoe.ApplyImpact(300.0f, FVector(120.0, 32.5, 14.0));
			TestTrue(TEXT("Abre al menos una brecha"), R.NewBreaches >= 1);
			TestEqual(TEXT("Brechas contadas"), Canoe.BreachCount(), R.NewBreaches);
			TestEqual(TEXT("0,5 L/s de agua de mar por brecha"), Canoe.LeakKgS(), Canoe.BreachCount() * 0.5f * 1.025f, 1e-4f);
			// Un golpe más no vuelve a contar las rotas.
			const FBoatImpactReport Again = Canoe.ApplyImpact(300.0f, FVector(120.0, 32.5, 14.0));
			TestEqual(TEXT("Brechas nuevas solo las nuevas"), Canoe.BreachCount(), R.NewBreaches + Again.NewBreaches);

			int32 Broken = INDEX_NONE;
			for (int32 I = 0; I < Canoe.GetJoints().Num(); ++I)
			{
				if (Canoe.GetJoints()[I].IsBreached())
				{
					Broken = I;
				}
			}
			const int32 Before = Canoe.BreachCount();
			TestFalse(TEXT("Reparar con 0 puntos no"), Canoe.RepairJoint(Broken, 0.0f));
			TestFalse(TEXT("Ni con NaN"), Canoe.RepairJoint(Broken, NAN));
			TestFalse(TEXT("Ni una unión que no existe"), Canoe.RepairJoint(999, 10.0f));
			TestTrue(TEXT("Se repara"), Canoe.RepairJoint(Broken, 1000.0f));
			TestEqual(TEXT("Sin pasarse del máximo"), Canoe.GetJoints()[Broken].Integrity, Canoe.GetJoints()[Broken].MaxIntegrity);
			TestEqual(TEXT("Una brecha menos"), Canoe.BreachCount(), Before - 1);
			TestFalse(TEXT("Entera no se repara"), Canoe.RepairJoint(Broken, 10.0f));
		});

		It("no depende del orden de montaje: las mismas piezas al revés dan el mismo casco", [this]()
		{
			for (int32 T = 0; T < static_cast<int32>(EBoatType::Count); ++T)
			{
				const FBoatPiecesModel Forward = FBoatPiecesModel::Blueprint(static_cast<EBoatType>(T));
				FBoatPiecesModel Backward;
				for (int32 I = Forward.GetPieces().Num() - 1; I >= 0; --I)
				{
					Backward.AddPiece(Forward.GetPieces()[I]);
				}
				const FBoatHullReport A = Forward.EvaluateWithCrew();
				const FBoatHullReport B = Backward.EvaluateWithCrew();
				TestTrue(TEXT("Mismo veredicto"), A.Verdict == B.Verdict && A.Issues == B.Issues);
				TestEqual(TEXT("Misma masa"), A.StructureMassKg, B.StructureMassKg, 1e-3f);
				TestEqual(TEXT("Misma flotabilidad"), A.MaxBuoyancyKg, B.MaxBuoyancyKg, 1e-3f);
				TestEqual(TEXT("Misma GM"), A.EffectiveGMCm, B.EffectiveGMCm, 1e-3f);
				TestEqual(TEXT("Mismas uniones"), Backward.GetJoints().Num(), Forward.GetJoints().Num());
			}
		});

		It("es determinista: la misma secuencia de golpes da lo mismo bit a bit", [this]()
		{
			auto Run = []()
			{
				FBoatPiecesModel Limon = FBoatPiecesModel::Blueprint(EBoatType::Limon);
				for (int32 I = 0; I < 40; ++I)
				{
					const double X = -300.0 + 15.0 * I;
					Limon.ApplyImpact(90.0f + 7.0f * I, FVector(X, (I % 2 ? 1.0 : -1.0) * 125.0, 12.0));
				}
				return Limon;
			};
			const FBoatPiecesModel A = Run();
			const FBoatPiecesModel B = Run();
			TestEqual(TEXT("Mismas uniones"), A.GetJoints().Num(), B.GetJoints().Num());
			bool bSame = true;
			for (int32 I = 0; I < A.GetJoints().Num(); ++I)
			{
				bSame &= A.GetJoints()[I].Integrity == B.GetJoints()[I].Integrity;
			}
			TestTrue(TEXT("Misma integridad"), bSame);
			TestTrue(TEXT("Ha habido brechas"), A.BreachCount() > 0);
		});
	});
}

#endif
