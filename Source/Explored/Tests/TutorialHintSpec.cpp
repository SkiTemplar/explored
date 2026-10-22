#include "Misc/AutomationTest.h"

#include "UI/TutorialHintModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HintTest
{
	FTutorialHintState Must(const FTutorialHintResult& Result)
	{
		check(Result.bOk);
		return Result.State;
	}

	FTutorialHintRecord Record(uint8 Shown, std::initializer_list<uint8> Queue)
	{
		FTutorialHintRecord R;
		R.ShownMask = Shown;
		for (const uint8 Raw : Queue)
		{
			R.Queue.Add(Raw);
		}
		return R;
	}
}

BEGIN_DEFINE_SPEC(FTutorialHintSpec, "Explored.TutorialHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTutorialHintSpec)

void FTutorialHintSpec::Define()
{
	using namespace HintTest;

	It(TEXT("hay 8 pistas, cada una con su texto"), [this]()
	{
		TestEqual(TEXT("Ocho"), FTutorialHintModel::HintCount, 8);
		for (uint8 Raw = 0; Raw < static_cast<uint8>(ETutorialHint::Count); ++Raw)
		{
			TestTrue(FString::Printf(TEXT("Texto %d"), Raw), FTutorialHintModel::TextId(static_cast<ETutorialHint>(Raw)) != NAME_None);
		}
		TestEqual(TEXT("Remar"), FTutorialHintModel::TextId(ETutorialHint::Paddle), FName(TEXT("tut_remar")));
		TestTrue(TEXT("Fuera de rango"), FTutorialHintModel::TextId(ETutorialHint::Count) == NAME_None);
	});

	It(TEXT("saca en orden de llegada y marca como mostradas"), [this]()
	{
		FTutorialHintState S;
		S = Must(FTutorialHintModel::Request(S, ETutorialHint::Move));
		S = Must(FTutorialHintModel::Request(S, ETutorialHint::Drink));
		TestTrue(TEXT("En cola"), FTutorialHintModel::IsQueued(S, ETutorialHint::Drink));
		TestFalse(TEXT("Aún no mostrada"), FTutorialHintModel::WasShown(S, ETutorialHint::Move));

		const FTutorialHintResult First = FTutorialHintModel::PopNext(S);
		TestTrue(TEXT("Sale"), First.bOk);
		TestEqual(TEXT("Primero mover"), First.Hint, ETutorialHint::Move);
		TestTrue(TEXT("Mostrada"), FTutorialHintModel::WasShown(First.State, ETutorialHint::Move));
		TestEqual(TEXT("Entrada intacta"), S.Queue.Num(), 2);

		const FTutorialHintResult Second = FTutorialHintModel::PopNext(First.State);
		TestEqual(TEXT("Luego beber"), Second.Hint, ETutorialHint::Drink);
		TestEqual(TEXT("Dos mostradas"), FTutorialHintModel::ShownCount(Second.State), 2);
	});

	It(TEXT("no repite una pista que ya espera en la cola"), [this]()
	{
		const FTutorialHintState S = Must(FTutorialHintModel::Request(FTutorialHintState(), ETutorialHint::Craft));
		const FTutorialHintResult Again = FTutorialHintModel::Request(S, ETutorialHint::Craft);
		TestFalse(TEXT("Rechazada"), Again.bOk);
		TestEqual(TEXT("Una en cola"), Again.State.Queue.Num(), 1);
	});

	It(TEXT("no vuelve a encolar una pista ya mostrada"), [this]()
	{
		FTutorialHintState S = Must(FTutorialHintModel::Request(FTutorialHintState(), ETutorialHint::Sleep));
		S = FTutorialHintModel::PopNext(S).State;
		const FTutorialHintResult Again = FTutorialHintModel::Request(S, ETutorialHint::Sleep);
		TestFalse(TEXT("Rechazada"), Again.bOk);
		TestTrue(TEXT("Cola vacía"), Again.State.Queue.IsEmpty());
	});

	It(TEXT("rechaza pistas fuera de rango y sacar de la cola vacía"), [this]()
	{
		TestFalse(TEXT("Count"), FTutorialHintModel::Request(FTutorialHintState(), ETutorialHint::Count).bOk);
		TestFalse(TEXT("200"), FTutorialHintModel::Request(FTutorialHintState(), static_cast<ETutorialHint>(200)).bOk);
		const FTutorialHintResult Pop = FTutorialHintModel::PopNext(FTutorialHintState());
		TestFalse(TEXT("Cola vacía"), Pop.bOk);
		TestEqual(TEXT("Sin pista"), Pop.Hint, ETutorialHint::Count);
	});

	It(TEXT("tras las 8 están todas mostradas y no sale ninguna más"), [this]()
	{
		FTutorialHintState S;
		for (uint8 Raw = 0; Raw < static_cast<uint8>(ETutorialHint::Count); ++Raw)
		{
			S = Must(FTutorialHintModel::Request(S, static_cast<ETutorialHint>(Raw)));
		}
		while (!S.Queue.IsEmpty())
		{
			S = FTutorialHintModel::PopNext(S).State;
		}
		TestTrue(TEXT("Todas"), FTutorialHintModel::AllShown(S));
		for (uint8 Raw = 0; Raw < static_cast<uint8>(ETutorialHint::Count); ++Raw)
		{
			TestFalse(TEXT("Ninguna se repite"), FTutorialHintModel::Request(S, static_cast<ETutorialHint>(Raw)).bOk);
		}
	});

	It(TEXT("ida y vuelta del registro conserva mostradas y cola"), [this]()
	{
		FTutorialHintState S = Must(FTutorialHintModel::Request(FTutorialHintState(), ETutorialHint::Move));
		S = FTutorialHintModel::PopNext(S).State;
		S = Must(FTutorialHintModel::Request(S, ETutorialHint::Paddle));
		S = Must(FTutorialHintModel::Request(S, ETutorialHint::Barometer));
		const FTutorialHintResult Loaded = FTutorialHintModel::FromRecord(FTutorialHintModel::ToRecord(S));
		TestTrue(TEXT("Aceptado"), Loaded.bOk);
		TestTrue(TEXT("Igual"), Loaded.State == S);
	});

	It(TEXT("rechaza registros con ids fuera de rango, repetidos o ya mostrados"), [this]()
	{
		const FTutorialHintRecord Bad[] = {Record(0, {8}), Record(0, {255}), Record(0, {2, 2}), Record(0x1, {0}),
			Record(0, {0, 1, 2, 3, 4, 5, 6, 7, 0})};
		for (const FTutorialHintRecord& R : Bad)
		{
			const FTutorialHintResult Loaded = FTutorialHintModel::FromRecord(R);
			TestFalse(TEXT("Rechazado"), Loaded.bOk);
			TestTrue(TEXT("Estado vacío"), Loaded.State == FTutorialHintState());
		}
		TestTrue(TEXT("Todas mostradas y cola vacía"), FTutorialHintModel::FromRecord(Record(0xFF, {})).bOk);
	});

	It(TEXT("nada antes de jugar 2 s; luego mover sale la primera"), [this]()
	{
		FTutorialFacts Facts;
		Facts.PlayingSeconds = 1.9f;
		Facts.bHasFocus = true;
		Facts.bCarriesDrink = true;
		Facts.bOnRaft = true;
		TestEqual(TEXT("Aún nada"), FTutorialHintModel::Triggered(Facts).Num(), 0);
		Facts.PlayingSeconds = 2.0f;
		const TArray<ETutorialHint> Hints = FTutorialHintModel::Triggered(Facts);
		TestEqual(TEXT("Mover, coger, beber y remar"), Hints.Num(), 4);
		TestTrue(TEXT("Mover primero"), Hints.Num() == 4 && Hints[0] == ETutorialHint::Move && Hints[1] == ETutorialHint::Pickup);
	});

	It(TEXT("cada disparador pide su pista"), [this]()
	{
		FTutorialFacts Base;
		Base.PlayingSeconds = 5.0f;
		TestEqual(TEXT("Solo mover"), FTutorialHintModel::Triggered(Base).Num(), 1);
		auto Only = [this](const FTutorialFacts& Facts, ETutorialHint Expected, const TCHAR* What)
		{
			const TArray<ETutorialHint> Hints = FTutorialHintModel::Triggered(Facts);
			TestTrue(What, Hints.Num() == 2 && Hints[0] == ETutorialHint::Move && Hints[1] == Expected);
		};
		FTutorialFacts Drink = Base;
		Drink.bCarriesDrink = true;
		Only(Drink, ETutorialHint::Drink, TEXT("Coco en la mochila"));
		FTutorialFacts Thirsty = Base;
		Thirsty.Thirst = 45.0f;
		Only(Thirsty, ETutorialHint::Drink, TEXT("Sed a 45"));
		FTutorialFacts NotThirsty = Base;
		NotThirsty.Thirst = 55.0f;
		TestEqual(TEXT("Sed inicial (55): solo mover"), FTutorialHintModel::Triggered(NotThirsty).Num(), 1);
		FTutorialFacts Craft = Base;
		Craft.Fibres = 2;
		Only(Craft, ETutorialHint::Craft, TEXT("Dos fibras"));
		Craft.Fibres = 1;
		TestEqual(TEXT("Una fibra: solo mover"), FTutorialHintModel::Triggered(Craft).Num(), 1);
		FTutorialFacts Sleep = Base;
		Sleep.bSleepHour = true;
		Only(Sleep, ETutorialHint::Sleep, TEXT("Hora de dormir"));
		FTutorialFacts Notebook = Base;
		Notebook.PagesRead = 1;
		Only(Notebook, ETutorialHint::Notebook, TEXT("Primera página"));
		FTutorialFacts Barometer = Base;
		Barometer.bHasBarometer = true;
		Only(Barometer, ETutorialHint::Barometer, TEXT("Barómetro"));
		FTutorialFacts Raft = Base;
		Raft.bOnRaft = true;
		Only(Raft, ETutorialHint::Paddle, TEXT("En la balsa"));
	});

	It(TEXT("una pista disparada sale una sola vez aunque el disparador siga"), [this]()
	{
		FTutorialFacts Facts;
		Facts.PlayingSeconds = 5.0f;
		Facts.bOnRaft = true;
		FTutorialHintState S = FTutorialHintModel::RequestTriggered(FTutorialHintState(), Facts);
		TestEqual(TEXT("Mover y remar en cola"), S.Queue.Num(), 2);
		S = FTutorialHintModel::RequestTriggered(S, Facts);
		TestEqual(TEXT("Sin repetir en cola"), S.Queue.Num(), 2);
		S = Must(FTutorialHintModel::PopNext(S));
		S = Must(FTutorialHintModel::PopNext(S));
		S = FTutorialHintModel::RequestTriggered(S, Facts);
		TestEqual(TEXT("Ya mostradas: no vuelven"), S.Queue.Num(), 0);
		TestTrue(TEXT("Remar mostrada"), FTutorialHintModel::WasShown(S, ETutorialHint::Paddle));
	});

	It(TEXT("una partida anterior a las pistas las da todas por vistas"), [this]()
	{
		const FTutorialHintState S = FTutorialHintModel::AllShownState();
		TestTrue(TEXT("Todas"), FTutorialHintModel::AllShown(S));
		TestTrue(TEXT("Cola vacía"), S.Queue.IsEmpty());
		FTutorialFacts Facts;
		Facts.PlayingSeconds = 10.0f;
		Facts.bOnRaft = true;
		TestEqual(TEXT("Nada que pedir"), FTutorialHintModel::RequestTriggered(S, Facts).Queue.Num(), 0);
	});
}

#endif
