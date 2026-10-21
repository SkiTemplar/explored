#include "Misc/AutomationTest.h"

#include "Items/BackpackModel.h"
#include "Items/IslandConsumablesModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BackpackTest
{
	int32 Num(const FBackpackState& State, const TCHAR* Id)
	{
		return FBackpackModel::Count(State, FName(Id));
	}
}

BEGIN_DEFINE_SPEC(FBackpackSpec, "Explored.Backpack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FBackpackRules Rules;
END_DEFINE_SPEC(FBackpackSpec)

void FBackpackSpec::Define()
{
	using namespace BackpackTest;

	BeforeEach([this]()
	{
		Rules = FBackpackRules::Default();
	});

	Describe(TEXT("las reglas del GDD §5.3 y §7"), [this]()
	{
		It(TEXT("tienen 17 objetos, 8 huecos y las pilas del GDD"), [this]()
		{
			TestEqual(TEXT("Objetos"), Rules.Items.Num(), 17);
			TestEqual(TEXT("Huecos"), Rules.Slots, 8);
			TestEqual(TEXT("Cocos"), Rules.Find(TEXT("coco"))->Stack, 4);
			TestEqual(TEXT("Leña"), Rules.Find(TEXT("lena"))->Stack, 6);
			TestEqual(TEXT("Fibra"), Rules.Find(TEXT("fibra"))->Stack, 6);
			TestEqual(TEXT("Cuerdas"), Rules.Find(TEXT("cuerda"))->Stack, 4);
			TestEqual(TEXT("Peces"), Rules.Find(TEXT("pez_crudo"))->Stack, 3);
			TestTrue(TEXT("El tronco pesa"), Rules.Find(TEXT("tronco"))->bHeavy);
			TestTrue(TEXT("La lente pesa"), Rules.Find(TEXT("lente"))->bHeavy);
		});

		It(TEXT("el kit inicial es cuchillo, cantimplora con 2 sorbos y barómetro"), [this]()
		{
			const FBackpackState Kit = FBackpackModel::StarterKit(Rules);
			TestEqual(TEXT("Huecos usados"), FBackpackModel::UsedSlots(Kit), 3);
			TestEqual(TEXT("Cuchillo"), Num(Kit, TEXT("cuchillo")), 1);
			TestEqual(TEXT("Barómetro"), Num(Kit, TEXT("barometro")), 1);
			TestEqual(TEXT("Sorbos"), Kit.CanteenSips, 2);
			TestTrue(TEXT("Válido"), FBackpackModel::IsValid(Kit, Rules));
		});
	});

	Describe(TEXT("las pilas"), [this]()
	{
		It(TEXT("completan la pila antes de abrir otro hueco"), [this]()
		{
			FBackpackState S;
			FBackpackModel::Add(S, Rules, TEXT("lena"), 4, S);
			FBackpackModel::Add(S, Rules, TEXT("lena"), 4, S);
			TestEqual(TEXT("Leña"), Num(S, TEXT("lena")), 8);
			TestEqual(TEXT("Dos huecos (6 + 2)"), FBackpackModel::UsedSlots(S), 2);
			TestEqual(TEXT("Primera pila llena"), S.Stacks[0].Count, 6);
		});

		It(TEXT("con la mochila llena no recoge nada y no cambia el estado"), [this]()
		{
			FBackpackState S;
			for (int32 I = 0; I < 8; ++I)
			{
				FBackpackModel::Add(S, Rules, TEXT("cuerda"), 4, S);
			}
			const FBackpackState Before = S;
			FBackpackState After;
			TestEqual(TEXT("Llena"), FBackpackModel::Add(S, Rules, TEXT("fibra"), 1, After), EBackpackResult::Full);
			TestTrue(TEXT("Sin cambios"), After == Before);
			TestTrue(TEXT("La entrada no se toca"), S == Before);
		});

		It(TEXT("recoge todo o nada: 2 leñas con un solo hueco de sitio para 1 no entran"), [this]()
		{
			FBackpackState S;
			for (int32 I = 0; I < 7; ++I)
			{
				FBackpackModel::Add(S, Rules, TEXT("cuerda"), 4, S);
			}
			FBackpackModel::Add(S, Rules, TEXT("lena"), 5, S);
			TestEqual(TEXT("Cabe 1"), FBackpackModel::CanAdd(S, Rules, TEXT("lena"), 1), EBackpackResult::Ok);
			TestEqual(TEXT("No caben 2"), FBackpackModel::CanAdd(S, Rules, TEXT("lena"), 2), EBackpackResult::Full);
		});

		It(TEXT("rechaza ids desconocidos y cantidades no positivas"), [this]()
		{
			FBackpackState Out;
			TestEqual(TEXT("Desconocido"), FBackpackModel::Add(FBackpackState(), Rules, TEXT("rama_seca"), 1, Out), EBackpackResult::UnknownItem);
			TestEqual(TEXT("Cero"), FBackpackModel::Add(FBackpackState(), Rules, TEXT("lena"), 0, Out), EBackpackResult::InvalidCount);
			TestEqual(TEXT("Negativo"), FBackpackModel::Remove(FBackpackState(), TEXT("lena"), -1, Out), EBackpackResult::InvalidCount);
		});

		It(TEXT("quitar más de lo que hay falla sin tocar nada"), [this]()
		{
			FBackpackState S;
			FBackpackModel::Add(S, Rules, TEXT("fibra"), 2, S);
			FBackpackState Out;
			TestEqual(TEXT("No hay 3"), FBackpackModel::Remove(S, TEXT("fibra"), 3, Out), EBackpackResult::NotEnough);
			TestEqual(TEXT("Siguen 2"), Num(Out, TEXT("fibra")), 2);
		});

		It(TEXT("quitar una pila entera libera su hueco"), [this]()
		{
			FBackpackState S;
			FBackpackModel::Add(S, Rules, TEXT("fibra"), 2, S);
			FBackpackModel::Remove(S, TEXT("fibra"), 2, S);
			TestEqual(TEXT("Sin huecos"), FBackpackModel::UsedSlots(S), 0);
		});
	});

	Describe(TEXT("la carga pesada"), [this]()
	{
		It(TEXT("el tronco va en las manos y no gasta hueco"), [this]()
		{
			FBackpackState S = FBackpackModel::StarterKit(Rules);
			TestEqual(TEXT("Coge el tronco"), FBackpackModel::Add(S, Rules, TEXT("tronco"), 1, S), EBackpackResult::Ok);
			TestEqual(TEXT("Huecos"), FBackpackModel::UsedSlots(S), 3);
			TestEqual(TEXT("En las manos"), S.HeldHeavy, FName(TEXT("tronco")));
		});

		It(TEXT("con las manos ocupadas no coge otro tronco ni la lente"), [this]()
		{
			FBackpackState S;
			FBackpackModel::Add(S, Rules, TEXT("tronco"), 1, S);
			FBackpackState Out;
			TestEqual(TEXT("Otro tronco"), FBackpackModel::Add(S, Rules, TEXT("tronco"), 1, Out), EBackpackResult::HandsBusy);
			TestEqual(TEXT("La lente"), FBackpackModel::Add(S, Rules, TEXT("lente"), 1, Out), EBackpackResult::HandsBusy);
			TestEqual(TEXT("Dos troncos a la vez"), FBackpackModel::Add(FBackpackState(), Rules, TEXT("tronco"), 2, Out), EBackpackResult::HandsBusy);
		});

		It(TEXT("un estado con un tronco en un hueco no es válido"), [this]()
		{
			FBackpackState S;
			S.Stacks.Add({ TEXT("tronco"), 1 });
			TestFalse(TEXT("Inválido"), FBackpackModel::IsValid(S, Rules));
		});
	});

	Describe(TEXT("el intercambio atómico"), [this]()
	{
		It(TEXT("quita antes de añadir: el hueco liberado sirve para la salida"), [this]()
		{
			FBackpackState S;
			for (int32 I = 0; I < 7; ++I)
			{
				FBackpackModel::Add(S, Rules, TEXT("cuerda"), 4, S);
			}
			FBackpackModel::Add(S, Rules, TEXT("fibra"), 2, S);
			FBackpackState Out;
			const EBackpackResult R = FBackpackModel::Exchange(S, Rules, { { TEXT("fibra"), 2 } }, { { TEXT("mecha"), 1 } }, Out);
			TestEqual(TEXT("Ok"), R, EBackpackResult::Ok);
			TestEqual(TEXT("Mecha"), Num(Out, TEXT("mecha")), 1);
		});

		It(TEXT("si la salida no cabe no gasta nada"), [this]()
		{
			FBackpackState S;
			for (int32 I = 0; I < 7; ++I)
			{
				FBackpackModel::Add(S, Rules, TEXT("cuerda"), 4, S);
			}
			FBackpackModel::Add(S, Rules, TEXT("fibra"), 3, S);
			FBackpackState Out;
			const EBackpackResult R = FBackpackModel::Exchange(S, Rules, { { TEXT("fibra"), 1 } }, { { TEXT("mecha"), 1 } }, Out);
			TestEqual(TEXT("Llena"), R, EBackpackResult::Full);
			TestEqual(TEXT("Fibras intactas"), Num(Out, TEXT("fibra")), 3);
		});
	});

	Describe(TEXT("los consumibles"), [this]()
	{
		It(TEXT("beber un coco da +25 de sed y +5 de hambre y deja un coco vacío"), [this]()
		{
			FBackpackState S;
			FBackpackModel::Add(S, Rules, TEXT("coco"), 1, S);
			FBackpackState Out;
			const FConsumeOutcome O = FIslandConsumables::Consume(FIslandConsumables::Default(), S, Rules, TEXT("coco"), Out);
			TestEqual(TEXT("Ok"), O.Result, EConsumeResult::Ok);
			TestEqual(TEXT("Sed"), O.Effect.Thirst, 25.0f);
			TestEqual(TEXT("Hambre"), O.Effect.Hunger, 5.0f);
			TestEqual(TEXT("Coco vacío"), Num(Out, TEXT("coco_vacio")), 1);
			TestEqual(TEXT("Sin coco"), Num(Out, TEXT("coco")), 0);
		});

		It(TEXT("si el coco vacío no cabe se bebe igual y hay que soltarlo"), [this]()
		{
			FBackpackState S;
			for (int32 I = 0; I < 7; ++I)
			{
				FBackpackModel::Add(S, Rules, TEXT("cuerda"), 4, S);
			}
			FBackpackModel::Add(S, Rules, TEXT("coco"), 2, S);
			FBackpackState Out;
			const FConsumeOutcome O = FIslandConsumables::Consume(FIslandConsumables::Default(), S, Rules, TEXT("coco"), Out);
			TestEqual(TEXT("Ok"), O.Result, EConsumeResult::Ok);
			TestEqual(TEXT("Se suelta"), O.Dropped, FName(TEXT("coco_vacio")));
			TestEqual(TEXT("Queda 1 coco"), Num(Out, TEXT("coco")), 1);
		});

		It(TEXT("la cantimplora gasta un sorbo de +20 y vacía no da nada"), [this]()
		{
			FBackpackState S = FBackpackModel::StarterKit(Rules);
			const FIslandConsumables Table = FIslandConsumables::Default();
			FConsumeOutcome O = FIslandConsumables::Consume(Table, S, Rules, TEXT("cantimplora"), S);
			TestEqual(TEXT("Sorbo"), O.Effect.Thirst, 20.0f);
			O = FIslandConsumables::Consume(Table, S, Rules, TEXT("cantimplora"), S);
			TestEqual(TEXT("Sin sorbos"), S.CanteenSips, 0);
			TestEqual(TEXT("Cantimplora sigue"), Num(S, TEXT("cantimplora")), 1);
			O = FIslandConsumables::Consume(Table, S, Rules, TEXT("cantimplora"), S);
			TestEqual(TEXT("Vacía"), O.Result, EConsumeResult::Empty);
		});

		It(TEXT("el pez crudo da +20 de hambre y -10 de energía; la leña no se come"), [this]()
		{
			FBackpackState S;
			FBackpackModel::Add(S, Rules, TEXT("pez_crudo"), 1, S);
			FBackpackModel::Add(S, Rules, TEXT("lena"), 1, S);
			FBackpackState Out;
			const FIslandConsumables Table = FIslandConsumables::Default();
			const FConsumeOutcome Fish = FIslandConsumables::Consume(Table, S, Rules, TEXT("pez_crudo"), Out);
			TestEqual(TEXT("Hambre"), Fish.Effect.Hunger, 20.0f);
			TestEqual(TEXT("Energía"), Fish.Effect.Energy, -10.0f);
			TestEqual(TEXT("Leña"), FIslandConsumables::Consume(Table, S, Rules, TEXT("lena"), Out).Result, EConsumeResult::NotConsumable);
			TestEqual(TEXT("Sin pez asado"), FIslandConsumables::Consume(Table, S, Rules, TEXT("pez_asado"), Out).Result, EConsumeResult::Missing);
		});
	});
	Describe(TEXT("soltar con la tecla G"), [this]()
	{
		It(TEXT("suelta la última pila que no sea del kit; con solo el kit, nada (caso negativo)"), [this]()
		{
			FBackpackState Pack;
			Pack.Stacks.Add({ TEXT("cuchillo"), 1 });
			Pack.Stacks.Add({ TEXT("cantimplora"), 1 });
			TestTrue(TEXT("Solo kit"), FBackpackModel::DropCandidate(Pack).IsNone());
			Pack.Stacks.Add({ TEXT("remo"), 1 });
			Pack.Stacks.Add({ TEXT("barometro"), 1 });
			TestEqual(TEXT("El remo, no el barómetro"), FBackpackModel::DropCandidate(Pack), FName(TEXT("remo")));
			Pack.Stacks.Add({ TEXT("coco_vacio"), 3 });
			TestEqual(TEXT("La última"), FBackpackModel::DropCandidate(Pack), FName(TEXT("coco_vacio")));
			TestTrue(TEXT("Kit"), FBackpackModel::IsStarterKit(TEXT("cantimplora")));
			TestFalse(TEXT("No kit"), FBackpackModel::IsStarterKit(TEXT("mecha")));
		});
	});

}

#endif
