#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/FelledDriftModel.h"
#include "WorldGen/FellingModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace FelledDriftSpec
{
	/** Playa recta: seco con X < ShoreX; la profundidad crece 1 cm por cada cm mar adentro (+X). */
	struct FBeach
	{
		double ShoreX = 0.0;
		double WaterRiseCm = 0.0;
		float operator()(const FVector2D& P) const { return (float)(P.X - ShoreX + WaterRiseCm); }
	};

	FFellingDrop MakeDrop(const TCHAR* ItemId, double X, double Y = 0.0)
	{
		FFellingDrop Drop;
		Drop.ItemId = FName(ItemId);
		Drop.Position = FVector2D(X, Y);
		return Drop;
	}

	int32 CountState(const FFelledDriftModel& Model, EFelledPieceState State)
	{
		int32 N = 0;
		for (const FFelledPiece& Piece : Model.GetPieces())
		{
			N += Piece.State == State ? 1 : 0;
		}
		return N;
	}
}

BEGIN_DEFINE_SPEC(FFelledDriftModelSpec, "Explored.FelledDrift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FFelledDriftModelSpec)

void FFelledDriftModelSpec::Define()
{
	using namespace FelledDriftSpec;

	Describe("la tabla de flotación", [this]()
	{
		It("tiene ficha para todo lo que suelta la tala, sin repetidos", [this]()
		{
			const TArray<FDriftFloatSpec> Specs = FFelledDriftModel::DefaultFloatSpecs();
			for (int32 i = 0; i < Specs.Num(); ++i)
			{
				for (int32 j = i + 1; j < Specs.Num(); ++j)
				{
					TestNotEqual(TEXT("sin repetidos"), Specs[i].ItemId, Specs[j].ItemId);
				}
				TestTrue(*FString::Printf(TEXT("%s: calado ≥ 0"), *Specs[i].ItemId.ToString()), Specs[i].DraftCm >= 0.0f);
			}
			for (const FFellingProfile& Profile : FFellingModel::DefaultProfiles())
			{
				for (const FFellingYield& Yield : Profile.Yields)
				{
					bool bFound = false;
					for (const FDriftFloatSpec& Spec : Specs)
					{
						bFound |= Spec.ItemId == Yield.ItemId;
					}
					TestTrue(*FString::Printf(TEXT("ficha de %s"), *Yield.ItemId.ToString()), bFound);
				}
			}
		});

		It("los troncos pasan a madera flotante, los cocos siguen siendo cocos y la madera dura y la resina se hunden", [this]()
		{
			const TArray<FDriftFloatSpec> Specs = FFelledDriftModel::DefaultFloatSpecs();
			TestEqual(TEXT("tronco"), FFelledDriftModel::FindFloatSpec(Specs, TEXT("tronco_pequeno")).HandoverItemId, FName(TEXT("madera_flotante")));
			TestEqual(TEXT("coco"), FFelledDriftModel::FindFloatSpec(Specs, TEXT("coco_maduro")).HandoverItemId, FName(TEXT("coco_maduro")));
			TestTrue(TEXT("la hoja se deshace"), FFelledDriftModel::FindFloatSpec(Specs, TEXT("hoja_palma")).HandoverItemId.IsNone());
			TestFalse(TEXT("madera dura"), FFelledDriftModel::FindFloatSpec(Specs, TEXT("madera_dura")).bFloats);
			TestFalse(TEXT("resina"), FFelledDriftModel::FindFloatSpec(Specs, TEXT("resina")).bFloats);
			TestFalse(TEXT("un objeto desconocido se hunde"), FFelledDriftModel::FindFloatSpec(Specs, TEXT("no_existe")).bFloats);
		});

		It("solo sigue lo que flota y tendría agua bastante con la pleamar", [this]()
		{
			const FDriftFloatSpec Log = FFelledDriftModel::FindFloatSpec(FFelledDriftModel::DefaultFloatSpecs(), TEXT("tronco_pequeno"));
			const FDriftFloatSpec Hard = FFelledDriftModel::FindFloatSpec(FFelledDriftModel::DefaultFloatSpecs(), TEXT("madera_dura"));
			TestTrue(TEXT("tronco en la franja intermareal"), FFelledDriftModel::NeedsTracking(Log, 21.0f));
			TestFalse(TEXT("tronco justo en su calado"), FFelledDriftModel::NeedsTracking(Log, 20.0f));
			TestFalse(TEXT("tronco en seco"), FFelledDriftModel::NeedsTracking(Log, -300.0f));
			TestFalse(TEXT("madera dura en el agua"), FFelledDriftModel::NeedsTracking(Hard, 500.0f));
			TestFalse(TEXT("profundidad no finita"), FFelledDriftModel::NeedsTracking(Log, std::numeric_limits<float>::quiet_NaN()));
		});

		It("sanea un calado negativo o no finito de una tabla externa", [this]()
		{
			FDriftFloatSpec Bad;
			Bad.ItemId = TEXT("raro");
			Bad.bFloats = true;
			Bad.DraftCm = std::numeric_limits<float>::quiet_NaN();
			TestEqual(TEXT("NaN → 0"), FFelledDriftModel::FindFloatSpec({ Bad }, TEXT("raro")).DraftCm, 0.0f);
			Bad.DraftCm = -5.0f;
			TestEqual(TEXT("negativo → 0"), FFelledDriftModel::FindFloatSpec({ Bad }, TEXT("raro")).DraftCm, 0.0f);
		});
	});

	Describe("al caer", [this]()
	{
		It("flota con más agua que su calado; con el agua justo en el calado se queda quieto", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Deep = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 20.5), Beach);
			const int32 Edge = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 20.0), Beach);
			const int32 Leaf = Model.AddPiece(MakeDrop(TEXT("hoja_palma"), 2.0), Beach);
			const int32 Sunk = Model.AddPiece(MakeDrop(TEXT("madera_dura"), 400.0), Beach);
			const int32 Dry = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), -300.0), Beach);
			TestEqual(TEXT("hondo"), Model.GetPieces()[Deep].State, EFelledPieceState::Floating);
			TestEqual(TEXT("en el calado"), Model.GetPieces()[Edge].State, EFelledPieceState::Resting);
			TestEqual(TEXT("la hoja flota con 2 cm"), Model.GetPieces()[Leaf].State, EFelledPieceState::Floating);
			TestEqual(TEXT("la madera dura se va al fondo"), Model.GetPieces()[Sunk].State, EFelledPieceState::Resting);
			TestEqual(TEXT("en seco"), Model.GetPieces()[Dry].State, EFelledPieceState::Resting);
		});

		It("rechaza posiciones no finitas y pasa de MaxPieces", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			TestEqual(TEXT("NaN"), Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), std::numeric_limits<double>::quiet_NaN()), Beach), INDEX_NONE);
			TestEqual(TEXT("inf"), Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 0.0, std::numeric_limits<double>::infinity()), Beach), INDEX_NONE);
			for (int32 i = 0; i < FFelledDriftModel::MaxPieces; ++i)
			{
				Model.AddPiece(MakeDrop(TEXT("rama_seca"), 100.0), Beach);
			}
			TestEqual(TEXT("lleno"), Model.AddPiece(MakeDrop(TEXT("rama_seca"), 100.0), Beach), INDEX_NONE);
			TestEqual(TEXT("piezas"), Model.GetPieces().Num(), FFelledDriftModel::MaxPieces);
		});

		It("una sesión larga no lo agota: lo resuelto deja su hueco y lo activo conserva el índice", [this]()
		{
			// Unos 10 objetos por árbol talado al agua: 1024 son unos 100 árboles en una sesión.
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Kept = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), -300.0), Beach);
			for (int32 i = 1; i < FFelledDriftModel::MaxPieces; ++i)
			{
				const int32 Index = Model.AddPiece(MakeDrop(TEXT("rama_seca"), 100.0), Beach);
				Model.Collect(Index);
			}
			const FVector2D KeptAt = Model.GetPieces()[Kept].Drop.Position;
			for (int32 i = 0; i < 3 * FFelledDriftModel::MaxPieces; ++i)
			{
				const int32 Index = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 300.0), Beach);
				if (Index == INDEX_NONE)
				{
					AddError(FString::Printf(TEXT("la pieza %d no entra con %d activas"), i, Model.NumActive()));
					return;
				}
				TestNotEqual(TEXT("no pisa la pieza activa"), Index, Kept);
				TestEqual(TEXT("entra flotando"), Model.GetPieces()[Index].State, EFelledPieceState::Floating);
				TestEqual(TEXT("sin pasos heredados"), Model.GetPieces()[Index].FloatingSteps, 0);
				Model.HandOver(Index);
			}
			TestTrue(TEXT("el array no crece"), Model.GetPieces().Num() <= FFelledDriftModel::MaxPieces);
			TestEqual(TEXT("la activa sigue en su sitio"), Model.GetPieces()[Kept].State, EFelledPieceState::Resting);
			TestEqual(TEXT("y en su posición"), Model.GetPieces()[Kept].Drop.Position, KeptAt);
			TestEqual(TEXT("una activa"), Model.NumActive(), 1);
		});

		It("reutiliza el hueco más bajo, así que dos servidores con la misma historia dan los mismos índices", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			for (int32 i = 0; i < 5; ++i)
			{
				Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 300.0), Beach);
			}
			Model.Collect(3);
			Model.HandOver(1);
			TestEqual(TEXT("primero el 1"), Model.AddPiece(MakeDrop(TEXT("coco_maduro"), 300.0), Beach), 1);
			TestEqual(TEXT("luego el 3"), Model.AddPiece(MakeDrop(TEXT("coco_maduro"), 300.0), Beach), 3);
			TestEqual(TEXT("después crece"), Model.AddPiece(MakeDrop(TEXT("coco_maduro"), 300.0), Beach), 5);
			TestEqual(TEXT("el hueco toma la ficha nueva"), Model.GetPieces()[1].Drop.ItemId, FName(TEXT("coco_maduro")));
		});

		It("una palmera talada hacia el mar deja la copa flotando y la base en tierra", [this]()
		{
			const TArray<FFellingProfile> Profiles = FFellingModel::DefaultProfiles();
			const FFellingProfile* Palm = FFellingModel::FindProfile(Profiles, TEXT("Palm"));
			if (!TestNotNull(TEXT("palmera"), Palm))
			{
				return;
			}
			// Base 2 m tierra adentro; la palmera de 9 m cae hacia el mar (+X).
			FExploredRandom Random(1234);
			const TArray<FFellingDrop> Drops = FFellingModel::ComputeFellDrops(*Palm, FVector2D(-200.0, 0.0), FVector2D(1.0, 0.0), Random);
			FFelledDriftModel Model;
			const FBeach Beach;
			for (const FFellingDrop& Drop : Drops)
			{
				Model.AddPiece(Drop, Beach);
			}
			int32 Floating = 0;
			for (const FFelledPiece& Piece : Model.GetPieces())
			{
				const bool bShouldFloat = Piece.Spec.bFloats && Beach(Piece.Drop.Position) > Piece.Spec.DraftCm;
				TestEqual(*FString::Printf(TEXT("%s en x=%.0f"), *Piece.Drop.ItemId.ToString(), Piece.Drop.Position.X),
					Piece.State == EFelledPieceState::Floating, bShouldFloat);
				Floating += Piece.State == EFelledPieceState::Floating ? 1 : 0;
			}
			TestTrue(TEXT("algo de la copa flota"), Floating > 0);
		});
	});

	Describe("la deriva", [this]()
	{
		It("sigue la corriente al centímetro", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 1000.0), Beach);
			const FFelledDriftReport Report = Model.Advance(10.0f, Beach, [](const FVector2D&) { return FVector2D(100.0, -30.0); });
			TestEqual(TEXT("pasos"), Report.StepsRun, 20);
			TestEqual(TEXT("x"), Model.GetPieces()[Log].Drop.Position.X, 2000.0, 1e-6);
			TestEqual(TEXT("y"), Model.GetPieces()[Log].Drop.Position.Y, -300.0, 1e-6);
		});

		It("no depende del ritmo de fotogramas", [this]()
		{
			auto Run = [](const TArray<float>& Frames)
			{
				FFelledDriftModel Model;
				const FBeach Beach;
				Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 800.0), Beach);
				Model.AddPiece(MakeDrop(TEXT("coco_maduro"), 300.0, 50.0), Beach);
				Model.AddPiece(MakeDrop(TEXT("hoja_palma"), 40.0, -20.0), Beach);
				// Corriente hacia la orilla que gira con la posición: varan en sitios distintos.
				auto Current = [](const FVector2D& P) { return FVector2D(-60.0, 20.0 * FMath::Sin(P.X * 0.01)); };
				for (const float Dt : Frames)
				{
					Model.Advance(Dt, Beach, Current);
				}
				return Model.GetPieces();
			};
			TArray<float> Sixtieths;
			TArray<float> Quarters;
			for (int32 i = 0; i < 60 * 30; ++i)
			{
				Sixtieths.Add(1.0f / 60.0f);
			}
			for (int32 i = 0; i < 4 * 30; ++i)
			{
				Quarters.Add(0.25f);
			}
			const TArray<FFelledPiece> A = Run(Sixtieths);
			const TArray<FFelledPiece> B = Run(Quarters);
			const TArray<FFelledPiece> C = Run({ 7.3f, 12.2f, 10.5f });
			for (int32 i = 0; i < A.Num(); ++i)
			{
				TestEqual(TEXT("1/60 = 1/4 (x)"), A[i].Drop.Position.X, B[i].Drop.Position.X);
				TestEqual(TEXT("1/60 = 1/4 (y)"), A[i].Drop.Position.Y, B[i].Drop.Position.Y);
				TestEqual(TEXT("1/60 = trozos (x)"), A[i].Drop.Position.X, C[i].Drop.Position.X);
				TestEqual(TEXT("1/60 = trozos (estado)"), A[i].State, C[i].State);
				TestEqual(TEXT("1/60 = trozos (pasos)"), A[i].FloatingSteps, C[i].FloatingSteps);
			}
		});

		It("vara en el punto de contacto, no entra en seco y no oscila", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 500.0), Beach);
			const FFelledDriftReport Report = Model.Advance(20.0f, Beach, [](const FVector2D&) { return FVector2D(-100.0, 0.0); });
			const FFelledPiece& Piece = Model.GetPieces()[Log];
			TestEqual(TEXT("varado"), Piece.State, EFelledPieceState::Resting);
			// Toca fondo: menos agua que su calado, pero a ~1 mm del punto donde aún flotaba.
			TestTrue(TEXT("toca fondo"), Beach(Piece.Drop.Position) <= Piece.Spec.DraftCm);
			TestTrue(TEXT("en el punto de contacto"), Beach(Piece.Drop.Position) > Piece.Spec.DraftCm - 0.2f);
			TestTrue(TEXT("no entra en seco"), Beach(Piece.Drop.Position) > 0.0f);
			// Una sola varada: si varase donde aún flota, cada paso se refloataría y volvería a varar.
			TestEqual(TEXT("un aviso de varada"), Report.Beached.Num(), 1);
			TestEqual(TEXT("ningún refloatado"), Report.Refloated.Num(), 0);
			// Varado ya no se mueve aunque siga la corriente.
			const FVector2D Before = Piece.Drop.Position;
			Model.Advance(10.0f, Beach, [](const FVector2D&) { return FVector2D(-100.0, 0.0); });
			TestEqual(TEXT("quieto"), Model.GetPieces()[Log].Drop.Position, Before);
		});

		It("no salta por encima de una barra de arena de una columna con la corriente más fuerte", [this]()
		{
			FFelledDriftModel Model;
			// Mar abierto de 3 m con una barra de 25 cm de ancho y 5 cm de agua entre x = 300 y 325.
			auto Bar = [](const FVector2D& P) { return (P.X >= 300.0 && P.X < 325.0) ? 5.0f : 300.0f; };
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 1000.0), Bar);
			Model.Advance(5.0f, Bar, [](const FVector2D&) { return FVector2D(-1.0e6, 0.0); });
			const FFelledPiece& Piece = Model.GetPieces()[Log];
			TestEqual(TEXT("varado en la barra"), Piece.State, EFelledPieceState::Resting);
			TestTrue(TEXT("en la barra, sin cruzarla"), Piece.Drop.Position.X >= 300.0 && Piece.Drop.Position.X < 325.0);
		});

		It("vuelve a flotar con la pleamar y vara otra vez con la bajamar", [this]()
		{
			FFelledDriftModel Model;
			FBeach Beach;
			// Caído en la franja intermareal con la bajamar: 10 cm de agua, el tronco pide 20.
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 10.0), Beach);
			TestEqual(TEXT("quieto con la bajamar"), Model.GetPieces()[Log].State, EFelledPieceState::Resting);
			auto Offshore = [](const FVector2D&) { return FVector2D(20.0, 0.0); };
			Beach.WaterRiseCm = 150.0;
			FFelledDriftReport Report = Model.Advance(1.0f, Beach, Offshore);
			TestEqual(TEXT("se refloata"), Report.Refloated.Num(), 1);
			TestTrue(TEXT("se aleja de la orilla"), Model.GetPieces()[Log].Drop.Position.X > 10.0);
			// Baja la marea: vara en el sitio, sin moverse ese paso.
			Beach.WaterRiseCm = -1000.0;
			const FVector2D Before = Model.GetPieces()[Log].Drop.Position;
			Report = Model.Advance(0.5f, Beach, Offshore);
			TestEqual(TEXT("vara"), Report.Beached.Num(), 1);
			TestEqual(TEXT("en el sitio"), Model.GetPieces()[Log].Drop.Position, Before);
		});

		It("cruza bordes de chunk en coordenadas negativas sin saltos", [this]()
		{
			FFelledDriftModel Model;
			auto Sea = [](const FVector2D&) { return 500.0f; };
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), -30.0, -810.0), Sea);
			const double Chunk = 800.0;
			TestEqual(TEXT("chunk inicial"), FFelledDriftModel::CellOf(Model.GetPieces()[Log], Chunk), FIntPoint(-1, -2));
			Model.Advance(1.0f, Sea, [](const FVector2D&) { return FVector2D(40.0, 20.0); });
			TestEqual(TEXT("posición"), Model.GetPieces()[Log].Drop.Position, FVector2D(10.0, -790.0));
			TestEqual(TEXT("chunk tras cruzar los dos bordes"), FFelledDriftModel::CellOf(Model.GetPieces()[Log], Chunk), FIntPoint(0, -1));
		});
	});

	Describe("al entregarlo", [this]()
	{
		It("tras HandoverFloatingS flotando, el tronco es madera flotante, el coco sigue y la hoja se deshace", [this]()
		{
			FFelledDriftModel Model;
			auto Sea = [](const FVector2D&) { return 500.0f; };
			auto Still = [](const FVector2D&) { return FVector2D::ZeroVector; };
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 0.0), Sea);
			const int32 Coco = Model.AddPiece(MakeDrop(TEXT("coco_maduro"), 0.0), Sea);
			const int32 Leaf = Model.AddPiece(MakeDrop(TEXT("hoja_palma"), 0.0), Sea);
			const int32 Steps = FFelledDriftModel::HandoverSteps();
			// Avances de 60 s (por debajo del tope de pasos) hasta el paso anterior al de entrega.
			int32 Run = 0;
			while (Run + 120 <= Steps - 1)
			{
				Run += Model.Advance(60.0f, Sea, Still).StepsRun;
			}
			while (Run < Steps - 1)
			{
				Run += Model.Advance(FFelledDriftModel::FixedStepS, Sea, Still).StepsRun;
			}
			TestEqual(TEXT("sigue flotando un paso antes"), Model.NumFloating(), 3);
			const FFelledDriftReport Report = Model.Advance(FFelledDriftModel::FixedStepS, Sea, Still);
			TestEqual(TEXT("tronco"), Model.GetPieces()[Log].State, EFelledPieceState::HandedOver);
			TestEqual(TEXT("coco"), Model.GetPieces()[Coco].State, EFelledPieceState::HandedOver);
			TestEqual(TEXT("hoja"), Model.GetPieces()[Leaf].State, EFelledPieceState::Decayed);
			TestEqual(TEXT("entregados"), Report.HandedOver.Num(), 2);
			TestEqual(TEXT("deshechos"), Report.Decayed.Num(), 1);
		});

		It("el tiempo varado no cuenta para la entrega", [this]()
		{
			FFelledDriftModel Model;
			auto Shallow = [](const FVector2D&) { return 5.0f; };
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 0.0), Shallow);
			for (int32 i = 0; i < 20; ++i)
			{
				Model.Advance(60.0f, Shallow, [](const FVector2D&) { return FVector2D(50.0, 0.0); });
			}
			TestEqual(TEXT("sigue en el bajío"), Model.GetPieces()[Log].State, EFelledPieceState::Resting);
			TestEqual(TEXT("sin pasos flotando"), Model.GetPieces()[Log].FloatingSteps, 0);
		});

		It("HandOver solo entrega lo que flota y Collect solo lo que sigue activo", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Floating = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 300.0), Beach);
			const int32 Sunk = Model.AddPiece(MakeDrop(TEXT("madera_dura"), 300.0), Beach);
			const int32 Dry = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), -300.0), Beach);
			TestTrue(TEXT("entrega el que flota"), Model.HandOver(Floating));
			TestEqual(TEXT("como madera flotante"), Model.GetPieces()[Floating].State, EFelledPieceState::HandedOver);
			TestFalse(TEXT("no entrega lo hundido"), Model.HandOver(Sunk));
			TestFalse(TEXT("ni lo que está en seco"), Model.HandOver(Dry));
			TestFalse(TEXT("ni dos veces"), Model.HandOver(Floating));
			TestFalse(TEXT("ni un índice fuera"), Model.HandOver(99));
			TestTrue(TEXT("se bucea lo hundido"), Model.Collect(Sunk));
			TestFalse(TEXT("no se recoge dos veces"), Model.Collect(Sunk));
			TestFalse(TEXT("no se recoge lo entregado"), Model.Collect(Floating));
			TestFalse(TEXT("índice negativo"), Model.Collect(-1));
		});

		It("Release saca lo quieto como objeto del suelo y entrega lo que flota", [this]()
		{
			// Revisión 2026-09-29 L1: al descargar su chunk, HandOver no aceptaba lo varado.
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Floating = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 300.0), Beach);
			const int32 Leaf = Model.AddPiece(MakeDrop(TEXT("hoja_palma"), 300.0), Beach);
			const int32 Sunk = Model.AddPiece(MakeDrop(TEXT("madera_dura"), 300.0), Beach);
			const int32 Dry = Model.AddPiece(MakeDrop(TEXT("coco_maduro"), -300.0, -40.0), Beach);
			TestTrue(TEXT("el que flota"), Model.Release(Floating));
			TestEqual(TEXT("se entrega"), Model.GetPieces()[Floating].State, EFelledPieceState::HandedOver);
			TestTrue(TEXT("la hoja"), Model.Release(Leaf));
			TestEqual(TEXT("se deshace"), Model.GetPieces()[Leaf].State, EFelledPieceState::Decayed);
			TestTrue(TEXT("lo hundido"), Model.Release(Sunk));
			TestEqual(TEXT("queda en el fondo"), Model.GetPieces()[Sunk].State, EFelledPieceState::Released);
			TestTrue(TEXT("lo varado"), Model.Release(Dry));
			TestEqual(TEXT("queda en el suelo"), Model.GetPieces()[Dry].State, EFelledPieceState::Released);
			TestEqual(TEXT("donde estaba"), Model.GetPieces()[Dry].Drop.Position, FVector2D(-300.0, -40.0));
			TestFalse(TEXT("no dos veces"), Model.Release(Dry));
			TestFalse(TEXT("ni recogerlo luego del modelo"), Model.Collect(Dry));
			TestFalse(TEXT("ni un índice fuera"), Model.Release(4));
			TestFalse(TEXT("ni negativo"), Model.Release(-1));
			TestEqual(TEXT("nada activo"), Model.NumActive(), 0);
			TestEqual(TEXT("nombre"), FString(LexToString(EFelledPieceState::Released)), FString(TEXT("Released")));
		});

		It("lo soltado no vuelve a flotar con la pleamar ni gasta consultas, y deja su hueco", [this]()
		{
			FFelledDriftModel Model;
			FBeach Beach;
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), -50.0), Beach);
			TestTrue(TEXT("suelta"), Model.Release(Log));
			Beach.WaterRiseCm = 500.0;
			int32 Queries = 0;
			auto Counting = [&Queries, &Beach](const FVector2D& P) { ++Queries; return Beach(P); };
			const FFelledDriftReport Report = Model.Advance(30.0f, Counting, [](const FVector2D&) { return FVector2D(40.0, 0.0); });
			TestEqual(TEXT("no se refloata"), Report.Refloated.Num(), 0);
			TestEqual(TEXT("sin consultas"), Queries, 0);
			TestEqual(TEXT("quieto"), Model.GetPieces()[Log].Drop.Position, FVector2D(-50.0, 0.0));
			TestEqual(TEXT("el hueco es para la siguiente"), Model.AddPiece(MakeDrop(TEXT("coco_verde"), 300.0), Beach), Log);
		});
	});

	Describe("las piezas", [this]()
	{
		It("nunca desaparecen: cada una está en un solo estado y el total no cambia", [this]()
		{
			FFelledDriftModel Model;
			FBeach Beach;
			FExploredRandom Random(77);
			const TArray<const TCHAR*> Items = { TEXT("tronco_pequeno"), TEXT("madera_dura"), TEXT("rama_seca"),
				TEXT("hoja_palma"), TEXT("coco_maduro"), TEXT("resina"), TEXT("corteza") };
			for (int32 i = 0; i < 200; ++i)
			{
				Model.AddPiece(MakeDrop(Items[i % Items.Num()], Random.RangeInt(-400, 1200), Random.RangeInt(-500, 500)), Beach);
			}
			// Hacia la orilla en la mitad sur y mar adentro en la norte, con remolinos.
			auto Swirl = [](const FVector2D& P) { return FVector2D((P.Y < 0.0 ? -40.0 : 35.0) + 20.0 * FMath::Cos(P.Y * 0.02), 25.0 * FMath::Sin(P.X * 0.02)); };
			for (int32 t = 0; t < 400; ++t)
			{
				Beach.WaterRiseCm = 120.0 * FMath::Sin(t * 0.05);
				Model.Advance(4.0f, Beach, Swirl);
				if (t % 37 == 0)
				{
					Model.Collect(t % Model.GetPieces().Num());
				}
				int32 Sum = 0;
				for (const EFelledPieceState S : { EFelledPieceState::Resting, EFelledPieceState::Floating, EFelledPieceState::Collected,
					EFelledPieceState::HandedOver, EFelledPieceState::Decayed })
				{
					Sum += CountState(Model, S);
				}
				if (Sum != 200 || Model.GetPieces().Num() != 200)
				{
					AddError(FString::Printf(TEXT("paso %d: %d piezas en estados de %d"), t, Sum, Model.GetPieces().Num()));
					return;
				}
			}
			for (const FFelledPiece& Piece : Model.GetPieces())
			{
				TestTrue(TEXT("posición finita"), FMath::IsFinite(Piece.Drop.Position.X) && FMath::IsFinite(Piece.Drop.Position.Y));
				if (!Piece.Spec.bFloats)
				{
					TestTrue(TEXT("lo que se hunde no flota nunca"), Piece.State == EFelledPieceState::Resting || Piece.State == EFelledPieceState::Collected);
				}
				if (Piece.State == EFelledPieceState::Floating)
				{
					TestTrue(TEXT("lo que flota tiene agua bajo ello"), Beach(Piece.Drop.Position) > Piece.Spec.DraftCm);
				}
			}
			TestTrue(TEXT("algo se ha entregado o deshecho"), CountState(Model, EFelledPieceState::HandedOver) + CountState(Model, EFelledPieceState::Decayed) > 0);
		});
	});

	Describe("el coste y los datos degenerados", [this]()
	{
		It("lo hundido y lo ya resuelto no consulta la profundidad ni la corriente", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			Model.AddPiece(MakeDrop(TEXT("madera_dura"), 500.0), Beach);
			Model.AddPiece(MakeDrop(TEXT("resina"), 500.0), Beach);
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 500.0), Beach);
			Model.Collect(Log);
			int32 DepthCalls = 0;
			int32 CurrentCalls = 0;
			auto Depth = [&DepthCalls](const FVector2D& P) { ++DepthCalls; return (float)P.X; };
			auto Current = [&CurrentCalls](const FVector2D&) { ++CurrentCalls; return FVector2D(10.0, 0.0); };
			const FFelledDriftReport Report = Model.Advance(30.0f, Depth, Current);
			TestEqual(TEXT("pasos"), Report.StepsRun, 60);
			TestEqual(TEXT("sin consultas de profundidad"), DepthCalls, 0);
			TestEqual(TEXT("sin consultas de corriente"), CurrentCalls, 0);
		});

		It("un tronco en tierra cuesta una consulta por paso y ninguna de corriente", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), -500.0), Beach);
			int32 DepthCalls = 0;
			int32 CurrentCalls = 0;
			auto Depth = [&DepthCalls](const FVector2D& P) { ++DepthCalls; return (float)P.X; };
			auto Current = [&CurrentCalls](const FVector2D&) { ++CurrentCalls; return FVector2D(10.0, 0.0); };
			Model.Advance(5.0f, Depth, Current);
			TestEqual(TEXT("una por paso"), DepthCalls, 10);
			TestEqual(TEXT("sin corriente"), CurrentCalls, 0);
		});

		It("ignora pasos de tiempo no finitos, nulos o negativos", [this]()
		{
			FFelledDriftModel Model;
			const FBeach Beach;
			const int32 Log = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 500.0), Beach);
			auto Current = [](const FVector2D&) { return FVector2D(10.0, 0.0); };
			for (const float Dt : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -5.0f, 0.0f })
			{
				TestEqual(TEXT("sin pasos"), Model.Advance(Dt, Beach, Current).StepsRun, 0);
			}
			TestEqual(TEXT("no se ha movido"), Model.GetPieces()[Log].Drop.Position.X, 500.0);
			// Y el acumulador no se ha envenenado: el siguiente paso normal avanza.
			TestEqual(TEXT("sigue sano"), Model.Advance(0.5f, Beach, Current).StepsRun, 1);
			TestEqual(TEXT("avanza"), Model.GetPieces()[Log].Drop.Position.X, 505.0, 1e-9);
		});

		It("recorta un fotograma enorme al tope de pasos y descarta el resto", [this]()
		{
			FFelledDriftModel Model;
			auto Sea = [](const FVector2D&) { return 500.0f; };
			Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 0.0), Sea);
			const FFelledDriftReport Report = Model.Advance(1.0e6f, Sea, [](const FVector2D&) { return FVector2D(1.0, 0.0); });
			TestEqual(TEXT("tope"), Report.StepsRun, FFelledDriftModel::MaxStepsPerAdvance);
			TestEqual(TEXT("descartados"), Report.StepsDropped, 2000000 - FFelledDriftModel::MaxStepsPerAdvance);
			TestEqual(TEXT("sin resto pendiente"), Model.Advance(0.25f, Sea, [](const FVector2D&) { return FVector2D(1.0, 0.0); }).StepsRun, 0);
		});

		It("una profundidad no finita cuenta como seco y una corriente no finita no mueve", [this]()
		{
			FFelledDriftModel Model;
			auto Sea = [](const FVector2D&) { return 500.0f; };
			const int32 A = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 0.0), Sea);
			Model.Advance(1.0f, Sea, [](const FVector2D&) { return FVector2D(std::numeric_limits<double>::quiet_NaN(), 1.0); });
			TestEqual(TEXT("sin moverse"), Model.GetPieces()[A].Drop.Position, FVector2D(0.0, 0.0));
			const FFelledDriftReport Report = Model.Advance(0.5f, [](const FVector2D&) { return std::numeric_limits<float>::quiet_NaN(); },
				[](const FVector2D&) { return FVector2D(10.0, 0.0); });
			TestEqual(TEXT("vara"), Report.Beached.Num(), 1);
			TestEqual(TEXT("en el sitio"), Model.GetPieces()[A].Drop.Position, FVector2D(0.0, 0.0));
		});

		It("una corriente disparatada se recorta a MaxCurrentCmS", [this]()
		{
			FFelledDriftModel Model;
			auto Sea = [](const FVector2D&) { return 500.0f; };
			const int32 A = Model.AddPiece(MakeDrop(TEXT("tronco_pequeno"), 0.0), Sea);
			Model.Advance(1.0f, Sea, [](const FVector2D&) { return FVector2D(0.0, 1.0e30); });
			TestEqual(TEXT("y"), Model.GetPieces()[A].Drop.Position.Y, FFelledDriftModel::MaxCurrentCmS, 1e-6);
		});
	});
}

#endif
