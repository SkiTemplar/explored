#include "Misc/AutomationTest.h"

#include "Audio/SoundtrackModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SoundtrackTest
{
	/** Catálogo mínimo: dos piezas de día de 100 s, una por estado en el resto. */
	FSoundtrackCatalog MakeCatalog()
	{
		FSoundtrackCatalog Catalog;
		const TCHAR* Ids[] = { TEXT("dia_a"), TEXT("dia_b"), TEXT("noche"), TEXT("ciclon"), TEXT("ojo"), TEXT("final"), TEXT("creditos"), TEXT("menu") };
		for (const TCHAR* Id : Ids)
		{
			FSoundtrackPiece Piece;
			Piece.Id = FName(Id);
			Piece.Asset = FString::Printf(TEXT("/Game/Audio/Musica/%s.%s"), Id, Id);
			Piece.Seconds = 100.0f;
			Catalog.Pieces.Add(Piece);
		}
		auto Set = [&Catalog](ESoundtrackState State, TArray<int32> Pieces, float Gain, float RestMin, float RestMax, bool bLoop)
		{
			FSoundtrackStateConfig& Config = Catalog.Config(State);
			Config.Pieces = MoveTemp(Pieces);
			Config.Gain = Gain;
			Config.FadeIn = 4.0f;
			Config.FadeOut = 5.0f;
			Config.RestMin = RestMin;
			Config.RestMax = RestMax;
			Config.bLoop = bLoop;
		};
		Set(ESoundtrackState::Day, { 0, 1 }, 0.5f, 40.0f, 40.0f, false);
		Set(ESoundtrackState::Night, { 2 }, 0.4f, 30.0f, 60.0f, false);
		Set(ESoundtrackState::Cyclone, { 3 }, 0.55f, 0.0f, 0.0f, false);
		Set(ESoundtrackState::Eye, { 4 }, 0.15f, 0.0f, 0.0f, false);
		Set(ESoundtrackState::Finale, { 5 }, 0.9f, 0.0f, 0.0f, true);
		Set(ESoundtrackState::Credits, { 6 }, 0.9f, 0.0f, 0.0f, true);
		Set(ESoundtrackState::Menu, { 7 }, 0.8f, 0.0f, 0.0f, true);
		return Catalog;
	}

	int32 CountAction(const TArray<FSoundtrackCommand>& Commands, ESoundtrackAction Action)
	{
		return Commands.FilterByPredicate([Action](const FSoundtrackCommand& C) { return C.Action == Action; }).Num();
	}
}

BEGIN_DEFINE_SPEC(FSoundtrackModelSpec, "Explored.Audio.Soundtrack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSoundtrackModelSpec)

void FSoundtrackModelSpec::Define()
{
	using namespace SoundtrackTest;

	Describe(TEXT("ChooseState"), [this]()
	{
		It(TEXT("respeta la prioridad menú > créditos > final > ojo > ciclón > noche > día"), [this]()
		{
			FSoundtrackContext C;
			C.bInMenu = C.bCredits = C.bFinale = C.bCycloneEye = C.bCycloneWall = true;
			C.Night = 1.0f;
			TestEqual(TEXT("menú"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Menu);
			C.bInMenu = false;
			TestEqual(TEXT("créditos"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Credits);
			C.bCredits = false;
			TestEqual(TEXT("final"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Finale);
			C.bFinale = false;
			TestEqual(TEXT("ojo"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Eye);
			C.bCycloneEye = false;
			TestEqual(TEXT("ciclón"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Cyclone);
			C.bCycloneWall = false;
			TestEqual(TEXT("noche"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Night);
			C.Night = 0.0f;
			TestEqual(TEXT("día"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Night), ESoundtrackState::Day);
		});

		It(TEXT("la noche tiene histéresis: 0,5 no cambia de estado en ningún sentido"), [this]()
		{
			FSoundtrackContext C;
			C.Night = 0.5f;
			TestEqual(TEXT("desde el día"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Day), ESoundtrackState::Day);
			TestEqual(TEXT("desde la noche"), FSoundtrackModel::ChooseState(C, ESoundtrackState::Night), ESoundtrackState::Night);
		});
	});

	Describe(TEXT("Validate"), [this]()
	{
		It(TEXT("acepta el catálogo completo"), [this]()
		{
			FString Error;
			TestTrue(TEXT("válido"), MakeCatalog().Validate(Error));
		});

		It(TEXT("rechaza un estado sin piezas, una pieza inexistente y un volumen fuera de rango"), [this]()
		{
			FString Error;
			FSoundtrackCatalog Empty = MakeCatalog();
			Empty.Config(ESoundtrackState::Eye).Pieces.Reset();
			TestFalse(TEXT("ojo vacío"), Empty.Validate(Error));
			TestTrue(TEXT("el motivo nombra el estado"), Error.Contains(TEXT("eye")));

			FSoundtrackCatalog Missing = MakeCatalog();
			Missing.Config(ESoundtrackState::Day).Pieces.Add(99);
			TestFalse(TEXT("pieza inexistente"), Missing.Validate(Error));

			FSoundtrackCatalog Loud = MakeCatalog();
			Loud.Config(ESoundtrackState::Night).Gain = 1.5f;
			TestFalse(TEXT("volumen > 1"), Loud.Validate(Error));

			FSoundtrackCatalog Rest = MakeCatalog();
			Rest.Config(ESoundtrackState::Night).RestMin = 80.0f;
			TestFalse(TEXT("silencio mínimo > máximo"), Rest.Validate(Error));
		});

		It(TEXT("traduce las claves del JSON y rechaza una desconocida"), [this]()
		{
			ESoundtrackState State = ESoundtrackState::Silence;
			TestTrue(TEXT("eye"), SoundtrackStateFromKey(TEXT("eye"), State));
			TestEqual(TEXT("eye = Eye"), State, ESoundtrackState::Eye);
			TestFalse(TEXT("sea no existe"), SoundtrackStateFromKey(TEXT("sea"), State));
		});
	});

	Describe(TEXT("Update"), [this]()
	{
		It(TEXT("de día empieza una pieza con el volumen y el fundido del estado"), [this]()
		{
			FSoundtrackModel Model(MakeCatalog(), 7u);
			TArray<FSoundtrackCommand> Out;
			Model.Update(0.0, FSoundtrackContext(), Out);
			TestEqual(TEXT("una orden"), Out.Num(), 1);
			TestEqual(TEXT("empieza"), Out[0].Action, ESoundtrackAction::Start);
			TestEqual(TEXT("volumen del día"), Out[0].Volume, 0.5f);
			TestEqual(TEXT("fundido de entrada"), Out[0].FadeSeconds, 4.0f);
			TestEqual(TEXT("estado"), Model.GetState(), ESoundtrackState::Day);
		});

		It(TEXT("al acabar una pieza calla el silencio fijado y luego toca la siguiente de la lista"), [this]()
		{
			FSoundtrackModel Model(MakeCatalog(), 7u);
			TArray<FSoundtrackCommand> Out;
			Model.Update(0.0, FSoundtrackContext(), Out);
			const FName First = Out[0].PieceId;
			Out.Reset();
			Model.Update(94.0, FSoundtrackContext(), Out);
			TestEqual(TEXT("sin órdenes antes del fundido de salida"), Out.Num(), 0);
			Model.Update(95.0, FSoundtrackContext(), Out);
			TestEqual(TEXT("para"), CountAction(Out, ESoundtrackAction::Stop), 1);
			TestEqual(TEXT("no empieza otra en el silencio"), CountAction(Out, ESoundtrackAction::Start), 0);
			TestEqual(TEXT("la próxima empieza a los 100 + 40 s"), Model.GetNextStartAt(), 140.0);
			Out.Reset();
			Model.Update(139.0, FSoundtrackContext(), Out);
			TestEqual(TEXT("silencio hasta los 140 s"), Out.Num(), 0);
			Model.Update(140.0, FSoundtrackContext(), Out);
			TestEqual(TEXT("empieza la siguiente"), CountAction(Out, ESoundtrackAction::Start), 1);
			TestNotEqual(TEXT("no repite la pieza"), Out.Last().PieceId, First);
		});

		It(TEXT("el ciclón corta la noche en fundido cruzado y el ojo baja casi a silencio"), [this]()
		{
			FSoundtrackModel Model(MakeCatalog(), 3u);
			TArray<FSoundtrackCommand> Out;
			FSoundtrackContext C;
			C.Night = 1.0f;
			Model.Update(0.0, C, Out);
			Out.Reset();
			C.bCycloneWall = true;
			Model.Update(10.0, C, Out);
			TestEqual(TEXT("dos órdenes"), Out.Num(), 2);
			TestEqual(TEXT("primero para la noche"), Out[0].Action, ESoundtrackAction::Stop);
			TestEqual(TEXT("con el fundido de salida de la noche"), Out[0].FadeSeconds, 5.0f);
			TestEqual(TEXT("luego empieza el ciclón"), Out[1].PieceId, FName(TEXT("ciclon")));
			Out.Reset();
			C.bCycloneEye = true;
			Model.Update(20.0, C, Out);
			TestEqual(TEXT("ojo"), Model.GetState(), ESoundtrackState::Eye);
			TestEqual(TEXT("volumen del ojo"), Out.Last().Volume, 0.15f);
			TestTrue(TEXT("más bajo que el ciclón"), Out.Last().Volume < 0.55f);
		});

		It(TEXT("el final suena en bucle y no se corta al acabar la pieza"), [this]()
		{
			FSoundtrackModel Model(MakeCatalog(), 1u);
			TArray<FSoundtrackCommand> Out;
			FSoundtrackContext C;
			C.bFinale = true;
			Model.Update(0.0, C, Out);
			TestTrue(TEXT("bucle"), Out.Last().bLoop);
			Out.Reset();
			Model.Update(500.0, C, Out);
			TestEqual(TEXT("sin órdenes"), Out.Num(), 0);
			TestEqual(TEXT("sigue la pieza del final"), Model.GetCurrentPiece(), FName(TEXT("final")));
		});

		It(TEXT("con la misma semilla da las mismas órdenes"), [this]()
		{
			FSoundtrackModel A(MakeCatalog(), 11u);
			FSoundtrackModel B(MakeCatalog(), 11u);
			TArray<FSoundtrackCommand> OutA;
			TArray<FSoundtrackCommand> OutB;
			for (double T = 0.0; T < 600.0; T += 5.0)
			{
				A.Update(T, FSoundtrackContext(), OutA);
				B.Update(T, FSoundtrackContext(), OutB);
			}
			TestEqual(TEXT("mismo número"), OutA.Num(), OutB.Num());
			for (int32 I = 0; I < FMath::Min(OutA.Num(), OutB.Num()); ++I)
			{
				TestEqual(FString::Printf(TEXT("pieza %d"), I), OutA[I].PieceId, OutB[I].PieceId);
			}
		});
	});
}

#endif
