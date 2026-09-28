#include "Misc/AutomationTest.h"

#include "Carry/InventoryModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace InventoryStackTest
{
	/** Registros con los números de Content/Data/items.json (el modelo no lee el JSON). */
	FInventoryItem Make(FInventoryModel& Model, const TCHAR* Id, float WeightKg, float VolumeLiters, EInventorySize Size, TArray<FName> Tags, int32 Count = 1)
	{
		FInventoryItem Item;
		Item.InstanceId = Model.AllocateInstanceId();
		Item.DefinitionId = FName(Id);
		Item.WeightKg = WeightKg;
		Item.VolumeLiters = VolumeLiters;
		Item.Size = Size;
		Item.Tags = MoveTemp(Tags);
		Item.MaxStack = FInventoryModel::ComputeMaxStack(0.0f, 0.0f, Size, Item.Tags, /*bComposite=*/false);
		Item.Count = Count;
		return Item;
	}

	/** basalto: 1 kg y 0,4 l por unidad, apila. */
	FInventoryItem Piedras(FInventoryModel& M, int32 Count = 1) { return Make(M, TEXT("basalto"), 1.0f, 0.4f, EInventorySize::Pequeno, { FName(TEXT("piedra")) }, Count); }
	/** rama_seca: 0,3 kg y 0,5 l por unidad, apila. */
	FInventoryItem Ramas(FInventoryModel& M, int32 Count = 1) { return Make(M, TEXT("rama_seca"), 0.3f, 0.5f, EInventorySize::Pequeno, { FName(TEXT("madera")), FName(TEXT("combustible")) }, Count); }

	FInventoryItem Cuchillo(FInventoryModel& M)
	{
		FInventoryItem Item = Make(M, TEXT("cuchillo"), 0.3f, 0.2f, EInventorySize::Pequeno, { FName(TEXT("herramienta")), FName(TEXT("corte")) });
		Item.MaxStack = FInventoryModel::ComputeMaxStack(40.0f, 0.0f, Item.Size, Item.Tags, false);
		return Item;
	}

	FInventoryItem Cantimplora(FInventoryModel& M)
	{
		FInventoryItem Item = Make(M, TEXT("cantimplora"), 0.3f, 1.0f, EInventorySize::Pequeno, { FName(TEXT("recipiente")), FName(TEXT("cantimplora")) });
		Item.LiquidCapacityLiters = FInventoryModel::LiquidCapacityFromRecipiente(4.0f);
		Item.MaxStack = FInventoryModel::ComputeMaxStack(0.0f, Item.LiquidCapacityLiters, Item.Size, Item.Tags, false);
		return Item;
	}

	FInventoryItem MochilaAlbatros(FInventoryModel& M)
	{
		return Make(M, TEXT("mochila"), 0.8f, 2.0f, EInventorySize::Grande, { FName(TEXT("rescatado")), FName(TEXT("mochila")) });
	}

	FInventoryItem CinturonCuero(FInventoryModel& M)
	{
		return Make(M, TEXT("cinturon_cuero"), 0.4f, 0.3f, EInventorySize::Pequeno, { FName(TEXT("cinturon")), FName(TEXT("piel")) });
	}

	FInventoryItem Angarillas(FInventoryModel& M)
	{
		return Make(M, TEXT("angarillas"), 6.0f, 20.0f, EInventorySize::DosManos, { FName(TEXT("angarillas")), FName(TEXT("madera")) });
	}

	/** Unidades de una definición en todo lo que lleva el jugador (manos, cuerpo y angarillas). */
	int32 CountUnits(const FInventoryModel& Model, FName DefinitionId)
	{
		const FInventoryState& S = Model.GetState();
		int32 Total = 0;
		auto Add = [&Total, DefinitionId](const FInventoryItem& Item)
		{
			Total += (Item.IsValid() && Item.DefinitionId == DefinitionId) ? Item.Count : 0;
		};
		Add(S.HandLeft);
		if (!S.bHandsHoldTwoHanded)
		{
			Add(S.HandRight);
		}
		for (const FInventoryContainer* Container : { &S.Pockets, &S.Belt, &S.Pouch, &S.Backpack, &S.Sledge })
		{
			for (const FInventoryEntry& Entry : Container->Entries)
			{
				Add(Entry.Item);
			}
		}
		return Total;
	}

	/** Mete una pila en los bolsillos pasando por la mano (sin fundir: hueco propio). */
	int64 PutInPockets(FInventoryModel& Model, const FInventoryItem& Item)
	{
		EInventoryFail Fail = EInventoryFail::None;
		Model.PickUp(Item, Fail);
		Model.Move(Item.InstanceId, EInventorySlot::Pockets, Fail);
		return Item.InstanceId;
	}

	/** Generador lineal congruente propio: la secuencia no depende de la plataforma. */
	struct FLcg
	{
		uint32 State;
		uint32 Next() { State = State * 1664525u + 1013904223u; return State >> 8; }
		int32 Range(int32 Max) { return static_cast<int32>(Next() % static_cast<uint32>(Max)); }
	};
}

BEGIN_DEFINE_SPEC(FInventoryStackModelSpec, "Explored.Inventory.Stacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FInventoryStackModelSpec)

void FInventoryStackModelSpec::Define()
{
	using namespace InventoryStackTest;

	Describe("La regla de qué apila (biblia 03 §1.3)", [this]()
	{
		It("apila hasta 10 los recursos y deja en 1 lo que se gasta, guarda líquido o es único", [this]()
		{
			const TArray<FName> Piedra = { FName(TEXT("piedra")) };
			TestEqual(TEXT("El tope es 10"), FInventoryModel::MaxStackSize, 10);
			TestEqual(TEXT("Recurso en bruto"), FInventoryModel::ComputeMaxStack(0.0f, 0.0f, EInventorySize::Pequeno, Piedra, false), 10);
			TestEqual(TEXT("Recurso mediano (pescado grande)"), FInventoryModel::ComputeMaxStack(0.0f, 0.0f, EInventorySize::Grande, { FName(TEXT("comida")) }, false), 10);
			TestEqual(TEXT("Con durabilidad (herramienta)"), FInventoryModel::ComputeMaxStack(40.0f, 0.0f, EInventorySize::Pequeno, Piedra, false), 1);
			TestEqual(TEXT("Con líquido (cantimplora)"), FInventoryModel::ComputeMaxStack(0.0f, 1.0f, EInventorySize::Pequeno, Piedra, false), 1);
			TestEqual(TEXT("DosManos"), FInventoryModel::ComputeMaxStack(0.0f, 0.0f, EInventorySize::DosManos, Piedra, false), 1);
			TestEqual(TEXT("Compuesto (lleva sus piezas)"), FInventoryModel::ComputeMaxStack(0.0f, 0.0f, EInventorySize::Pequeno, Piedra, true), 1);
			for (const FName& Tag : FInventoryModel::GetNonStackableTags())
			{
				TestEqual(FString::Printf(TEXT("Etiqueta %s"), *Tag.ToString()),
					FInventoryModel::ComputeMaxStack(0.0f, 0.0f, EInventorySize::Pequeno, { Tag }, false), 1);
			}
			TestTrue(TEXT("La mochila del Albatros no tiene durabilidad y aun así no apila"),
				FInventoryModel::GetNonStackableTags().Contains(FName(TEXT("mochila"))));
		});

		It("solo funde pilas de la misma definición, calidad y peso por unidad", [this]()
		{
			FInventoryModel Model;
			const FInventoryItem A = Piedras(Model, 3);
			FInventoryItem B = Piedras(Model, 2);
			TestTrue(TEXT("Iguales"), A.CanStackWith(B));
			B.Quality = 5;
			TestFalse(TEXT("Otra calidad"), A.CanStackWith(B));
			TestFalse(TEXT("Otra definición"), A.CanStackWith(Ramas(Model)));
			FInventoryItem Heavier = Piedras(Model);
			Heavier.WeightKg = 1.5f;
			TestFalse(TEXT("Otro peso por unidad"), A.CanStackWith(Heavier));
			TestFalse(TEXT("Dos cantimploras no se funden"), Cantimplora(Model).CanStackWith(Cantimplora(Model)));
			TestFalse(TEXT("Dos cuchillos no se funden"), Cuchillo(Model).CanStackWith(Cuchillo(Model)));
		});

		It("cuenta el peso y el volumen de cada unidad", [this]()
		{
			FInventoryModel Model;
			const FInventoryItem Stack = Piedras(Model, 7);
			TestEqual(TEXT("Peso de la pila"), Stack.GetTotalWeightKg(), 7.0f);
			TestEqual(TEXT("Volumen de la pila"), Stack.GetTotalVolumeLiters(), 2.8f, 1.0e-4f);
			PutInPockets(Model, Stack);
			TestEqual(TEXT("Los bolsillos suman las 7"), Model.GetState().Pockets.GetUsedWeightKg(), 7.0f);
			TestEqual(TEXT("Una sola pila ocupa un hueco"), Model.GetState().Pockets.Num(), 1);
			TestEqual(TEXT("El cuerpo carga las 7"), Model.GetBodyWeightKg(), 7.0f);
		});
	});

	Describe("Apilar", [this]()
	{
		It("funde en la mano lo que se recoge del mismo tipo", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			int64 MergedInto = 0;
			const FInventoryItem First = Piedras(Model);
			TestTrue(TEXT("La primera a la mano"), Model.PickUpMerging(First, MergedInto, Fail));
			TestEqual(TEXT("Sin fundir"), MergedInto, (int64)0);
			for (int32 Index = 0; Index < 4; ++Index)
			{
				TestTrue(TEXT("Otra piedra"), Model.PickUpMerging(Piedras(Model), MergedInto, Fail));
				TestEqual(TEXT("Se funde en la primera"), MergedInto, First.InstanceId);
			}
			TestEqual(TEXT("Cinco en la mano izquierda"), Model.GetHandItem(EInventorySlot::HandLeft)->Count, 5);
			TestTrue(TEXT("La derecha sigue libre"), Model.IsHandEmpty(EInventorySlot::HandRight));
			TestEqual(TEXT("Pesa 5 kg"), Model.GetBodyWeightKg(), 5.0f);
		});

		It("guarda en los bolsillos completando primero la pila que ya hay", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const int64 InPocket = PutInPockets(Model, Piedras(Model, 7));
			const FInventoryItem Hand = Piedras(Model, 5);
			Model.PickUp(Hand, Fail);

			FInventoryStowResult Result;
			TestTrue(TEXT("Se guarda"), Model.StowMerging(Hand.InstanceId, EInventorySlot::Pockets, Result, Fail));
			TestEqual(TEXT("La pila del bolsillo llega a 10"), Model.FindItemById(InPocket)->Count, 10);
			TestEqual(TEXT("Crecieron una pila"), Result.ToppedUp.Num(), 1);
			TestEqual(TEXT("Con 3 unidades"), Result.ToppedUp[0].Value, 3);
			TestTrue(TEXT("El resto ocupa un hueco nuevo con su id"), Result.bMovedWhole && Model.FindItem(Hand.InstanceId) == EInventorySlot::Pockets);
			TestEqual(TEXT("Con 2"), Model.FindItemById(Hand.InstanceId)->Count, 2);
			TestEqual(TEXT("Movidas las 5"), Result.MovedUnits, 5);
			TestEqual(TEXT("No queda nada en la mano"), Result.LeftAtSource, 0);
			TestTrue(TEXT("La mano queda libre"), Model.IsHandEmpty(EInventorySlot::HandLeft));
			TestEqual(TEXT("Dos huecos de bolsillo"), Model.GetState().Pockets.Num(), 2);
			TestEqual(TEXT("Ninguna piedra se pierde"), CountUnits(Model, TEXT("basalto")), 12);
		});

		It("hace desaparecer la pila de origen si cabe entera en las que ya hay", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const int64 A = PutInPockets(Model, Piedras(Model, 6));
			const int64 B = PutInPockets(Model, Piedras(Model, 8));
			const FInventoryItem Hand = Piedras(Model, 5);
			Model.PickUp(Hand, Fail);
			FInventoryStowResult Result;
			TestTrue(TEXT("Se guarda"), Model.StowMerging(Hand.InstanceId, EInventorySlot::Pockets, Result, Fail));
			TestTrue(TEXT("El registro de la mano ya no existe"), Result.bSourceRemoved && Model.FindItem(Hand.InstanceId) == EInventorySlot::None);
			TestEqual(TEXT("La primera pila (hueco 0) se llena antes"), Model.FindItemById(A)->Count, 10);
			TestEqual(TEXT("La segunda recibe el resto"), Model.FindItemById(B)->Count, 9);
			TestEqual(TEXT("Siguen dos huecos"), Model.GetState().Pockets.Num(), 2);
		});

		It("guarda sin fundir lo que no apila, como un traslado", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Knife = Cuchillo(Model);
			PutInPockets(Model, Cuchillo(Model));
			Model.PickUp(Knife, Fail);
			FInventoryStowResult Result;
			TestTrue(TEXT("Se guarda"), Model.StowMerging(Knife.InstanceId, EInventorySlot::Pockets, Result, Fail));
			TestTrue(TEXT("Entero"), Result.bMovedWhole && Result.MovedUnits == 1);
			TestEqual(TEXT("Dos huecos: los cuchillos no se funden"), Model.GetState().Pockets.Num(), 2);
		});

		It("propone guardar donde ya hay una pila con sitio", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.SetCustomBackpack(true, 20.0f, 10.0f, Fail);
			const FInventoryItem InBackpack = Piedras(Model, 2);
			Model.PickUp(InBackpack, Fail);
			Model.Move(InBackpack.InstanceId, EInventorySlot::Backpack, Fail);
			const FInventoryItem Hand = Piedras(Model, 3);
			Model.PickUp(Hand, Fail);

			EInventorySlot Where = EInventorySlot::None;
			FInventoryStowResult Result;
			TestTrue(TEXT("Se guarda"), Model.AutoStowMergingFromHand(EInventorySlot::HandLeft, Where, Result, Fail));
			TestTrue(TEXT("En la mochila, no en un bolsillo libre"), Where == EInventorySlot::Backpack);
			TestEqual(TEXT("Una sola pila de 5"), Model.FindItemById(InBackpack.InstanceId)->Count, 5);
			TestTrue(TEXT("Bolsillos intactos"), Model.GetState().Pockets.IsEmpty());
		});
	});

	Describe("Partir", [this]()
	{
		It("separa unidades a la otra mano con un id nuevo sin cambiar el peso", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Stack = Piedras(Model, 10);
			Model.PickUp(Stack, Fail);
			const float Before = Model.GetBodyWeightKg();
			int64 NewId = 0;
			TestTrue(TEXT("Se parte"), Model.SplitStack(Stack.InstanceId, 4, EInventorySlot::HandRight, NewId, Fail));
			TestTrue(TEXT("Id nuevo"), NewId != 0 && NewId != Stack.InstanceId);
			TestEqual(TEXT("Quedan 6 en la izquierda"), Model.GetHandItem(EInventorySlot::HandLeft)->Count, 6);
			TestEqual(TEXT("4 en la derecha"), Model.GetHandItem(EInventorySlot::HandRight)->Count, 4);
			TestEqual(TEXT("Mismo peso"), Model.GetBodyWeightKg(), Before);
			TestTrue(TEXT("El siguiente id no repite"), Model.AllocateInstanceId() > NewId);
			EInventoryFail ValidateFail = EInventoryFail::None;
			TestTrue(TEXT("Estado válido"), FInventoryModel::ValidateState(Model.GetState(), ValidateFail));
		});

		It("rechaza cantidades imposibles sin tocar nada", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Stack = Piedras(Model, 5);
			const int64 NotCarried = Cuchillo(Model).InstanceId;
			Model.PickUp(Stack, Fail);
			const FInventoryState Before = Model.GetState();
			int64 NewId = 0;
			for (int32 Bad : { 0, -1, 5, 6, TNumericLimits<int32>::Max(), TNumericLimits<int32>::Min() })
			{
				TestFalse(FString::Printf(TEXT("No se parte en %d"), Bad), Model.SplitStack(Stack.InstanceId, Bad, EInventorySlot::HandRight, NewId, Fail));
				TestTrue(TEXT("Motivo"), Fail == EInventoryFail::InvalidCount);
			}
			TestFalse(TEXT("Un objeto suelto no se parte"), Model.SplitStack(NotCarried, 1, EInventorySlot::HandRight, NewId, Fail));
			TestTrue(TEXT("Motivo: no lo lleva"), Fail == EInventoryFail::NotFound);
			TestTrue(TEXT("Estado intacto"), Model.GetState() == Before);
		});

		It("respeta el destino: mano ocupada, bolsillos llenos o un hueco libre del mismo contenedor", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const int64 Stack = PutInPockets(Model, Piedras(Model, 8));
			PutInPockets(Model, Ramas(Model));
			Model.PickUp(Cuchillo(Model), Fail);
			Model.PickUp(Cuchillo(Model), Fail);
			int64 NewId = 0;
			TestFalse(TEXT("A una mano ocupada no"), Model.SplitStack(Stack, 3, EInventorySlot::HandLeft, NewId, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::HandOccupied);

			TestTrue(TEXT("Al mismo bolsillo, en otro hueco"), Model.SplitStack(Stack, 3, EInventorySlot::Pockets, NewId, Fail));
			TestEqual(TEXT("Tres pilas en los bolsillos"), Model.GetState().Pockets.Num(), 3);
			TestEqual(TEXT("Mismo total"), CountUnits(Model, TEXT("basalto")), 8);

			TestTrue(TEXT("Otra más al último hueco"), Model.SplitStack(Stack, 1, EInventorySlot::Pockets, NewId, Fail));
			TestEqual(TEXT("Bolsillos llenos"), Model.GetState().Pockets.Num(), FInventoryModel::PocketSlots);
			const FInventoryState Before = Model.GetState();
			TestFalse(TEXT("No hay hueco para otra pila"), Model.SplitStack(Stack, 1, EInventorySlot::Pockets, NewId, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerFull);
			TestTrue(TEXT("Estado intacto"), Model.GetState() == Before);
		});
	});

	Describe("Fusionar", [this]()
	{
		It("pasa lo que cabe y deja el resto en la pila de origen", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Into = Piedras(Model, 8);
			const FInventoryItem From = Piedras(Model, 5);
			Model.PickUp(Into, Fail);
			Model.PickUp(From, Fail);
			int32 Moved = 0;
			TestTrue(TEXT("Se funden"), Model.MergeStacks(From.InstanceId, Into.InstanceId, Moved, Fail));
			TestEqual(TEXT("Pasan 2"), Moved, 2);
			TestEqual(TEXT("Destino lleno"), Model.FindItemById(Into.InstanceId)->Count, 10);
			TestEqual(TEXT("Origen con 3"), Model.FindItemById(From.InstanceId)->Count, 3);
		});

		It("borra la pila de origen cuando pasa entera", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Into = Piedras(Model, 4);
			const FInventoryItem From = Piedras(Model, 6);
			Model.PickUp(Into, Fail);
			Model.PickUp(From, Fail);
			int32 Moved = 0;
			TestTrue(TEXT("Se funden"), Model.MergeStacks(From.InstanceId, Into.InstanceId, Moved, Fail));
			TestEqual(TEXT("Pasan las 6"), Moved, 6);
			TestTrue(TEXT("La mano derecha queda libre"), Model.IsHandEmpty(EInventorySlot::HandRight));
			TestTrue(TEXT("El id de origen desaparece"), Model.FindItem(From.InstanceId) == EInventorySlot::None);
		});

		It("no funde objetos distintos, consigo mismos ni lo que no apila", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			int32 Moved = 0;
			const FInventoryItem Stones = Piedras(Model, 2);
			const FInventoryItem Branches = Ramas(Model, 2);
			Model.PickUp(Stones, Fail);
			Model.PickUp(Branches, Fail);
			TestFalse(TEXT("Piedras con ramas no"), Model.MergeStacks(Branches.InstanceId, Stones.InstanceId, Moved, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NotStackable);
			TestFalse(TEXT("Consigo misma no"), Model.MergeStacks(Stones.InstanceId, Stones.InstanceId, Moved, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::AlreadyThere);

			FInventoryModel Canteens;
			const FInventoryItem A = Cantimplora(Canteens);
			const FInventoryItem B = Cantimplora(Canteens);
			Canteens.PickUp(A, Fail);
			Canteens.PickUp(B, Fail);
			TestFalse(TEXT("Dos cantimploras no"), Canteens.MergeStacks(B.InstanceId, A.InstanceId, Moved, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NotStackable);
			TestEqual(TEXT("Nada movido"), Moved, 0);
		});
	});

	Describe("Pila llena", [this]()
	{
		It("no admite la unidad número 11 por ningún camino", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Full = Piedras(Model, 10);
			const FInventoryItem One = Piedras(Model, 1);
			Model.PickUp(Full, Fail);
			Model.PickUp(One, Fail);
			const FInventoryState Before = Model.GetState();
			int32 Moved = 0;
			TestFalse(TEXT("Fundir en una pila llena"), Model.MergeStacks(One.InstanceId, Full.InstanceId, Moved, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::StackFull);
			TestTrue(TEXT("Sin cambios"), Model.GetState() == Before);

			FInventoryModel Hand;
			Hand.PickUp(Piedras(Hand, 10), Fail);
			int64 MergedInto = 0;
			const FInventoryItem Extra = Piedras(Hand);
			TestTrue(TEXT("Recoger otra con la pila llena"), Hand.PickUpMerging(Extra, MergedInto, Fail));
			TestEqual(TEXT("No se funde"), MergedInto, (int64)0);
			TestTrue(TEXT("Va a la otra mano"), Hand.FindItem(Extra.InstanceId) == EInventorySlot::HandRight);

			FInventoryItem Eleven = Piedras(Model, 11);
			TestTrue(TEXT("Un registro de 11 no entra en ningún contenedor"), FInventoryContainer().CanAccept(Eleven) == EInventoryFail::InvalidItem);
		});

		It("deja los bolsillos como estaban si están llenos de pilas completas", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			for (int32 Index = 0; Index < FInventoryModel::PocketSlots; ++Index)
			{
				// Ramas: 4 pilas de 10 pesan 12 kg, lejos del límite de carga.
				PutInPockets(Model, Ramas(Model, 10));
			}
			const FInventoryItem One = Ramas(Model);
			Model.PickUp(One, Fail);
			const FInventoryState Before = Model.GetState();
			FInventoryStowResult Result;
			TestFalse(TEXT("No cabe"), Model.StowMerging(One.InstanceId, EInventorySlot::Pockets, Result, Fail));
			TestTrue(TEXT("Motivo: sin huecos"), Fail == EInventoryFail::ContainerFull);
			TestTrue(TEXT("Sin cambios"), Model.GetState() == Before);
			TestEqual(TEXT("El resultado va vacío"), Result.MovedUnits, 0);
		});
	});

	Describe("Peso en el límite", [this]()
	{
		It("completa la pila de la mochila hasta su peso y deja el resto en la mano", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.SetCustomBackpack(true, 50.0f, 5.0f, Fail);
			const FInventoryItem InBackpack = Piedras(Model, 3);
			Model.PickUp(InBackpack, Fail);
			Model.Move(InBackpack.InstanceId, EInventorySlot::Backpack, Fail);
			const FInventoryItem Hand = Piedras(Model, 4);
			Model.PickUp(Hand, Fail);

			FInventoryStowResult Result;
			TestTrue(TEXT("Guarda lo que cabe"), Model.StowMerging(Hand.InstanceId, EInventorySlot::Backpack, Result, Fail));
			TestEqual(TEXT("Solo 2 por peso"), Result.MovedUnits, 2);
			TestEqual(TEXT("La mochila llega justo a 5 kg"), Model.GetState().Backpack.GetUsedWeightKg(), 5.0f);
			TestEqual(TEXT("Quedan 2 en la mano"), Model.GetHandItem(EInventorySlot::HandLeft)->Count, 2);
			TestEqual(TEXT("El resultado lo dice"), Result.LeftAtSource, 2);
			TestEqual(TEXT("Nada se pierde"), CountUnits(Model, TEXT("basalto")), 7);
		});

		It("abre un hueco nuevo solo con la parte que cabe", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.SetCustomBackpack(true, 50.0f, 2.5f, Fail);
			const FInventoryItem Hand = Piedras(Model, 4);
			Model.PickUp(Hand, Fail);
			FInventoryStowResult Result;
			TestTrue(TEXT("Guarda lo que cabe"), Model.StowMerging(Hand.InstanceId, EInventorySlot::Backpack, Result, Fail));
			TestTrue(TEXT("Con un id nuevo"), Result.NewStackId != 0 && Result.NewStackId != Hand.InstanceId);
			TestEqual(TEXT("2 piedras en la mochila (la tercera pasaría de 2,5 kg)"), Model.FindItemById(Result.NewStackId)->Count, 2);
			TestEqual(TEXT("2 siguen en la mano"), Model.FindItemById(Hand.InstanceId)->Count, 2);
		});

		It("llena exactamente el límite pese al redondeo de 10 × 0,1 kg", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.SetCustomBackpack(true, 50.0f, 1.0f, Fail);
			const FInventoryItem Seeds = Make(Model, TEXT("semilla"), 0.1f, 0.01f, EInventorySize::Pequeno, { FName(TEXT("semilla")) }, 10);
			Model.PickUp(Seeds, Fail);
			TestTrue(TEXT("Caben las 10"), Model.Move(Seeds.InstanceId, EInventorySlot::Backpack, Fail));

			FInventoryModel Merged;
			Merged.SetCustomBackpack(true, 50.0f, 1.0f, Fail);
			const FInventoryItem Base = Make(Merged, TEXT("semilla"), 0.1f, 0.01f, EInventorySize::Pequeno, { FName(TEXT("semilla")) }, 1);
			Merged.PickUp(Base, Fail);
			Merged.Move(Base.InstanceId, EInventorySlot::Backpack, Fail);
			for (int32 Index = 1; Index < 10; ++Index)
			{
				const FInventoryItem One = Make(Merged, TEXT("semilla"), 0.1f, 0.01f, EInventorySize::Pequeno, { FName(TEXT("semilla")) }, 1);
				Merged.PickUp(One, Fail);
				FInventoryStowResult Result;
				TestTrue(FString::Printf(TEXT("Semilla %d de una en una"), Index + 1), Merged.StowMerging(One.InstanceId, EInventorySlot::Backpack, Result, Fail));
			}
			TestEqual(TEXT("Una pila de 10"), Merged.FindItemById(Base.InstanceId)->Count, 10);
		});

		It("no deja pasar del doble de la capacidad cómoda al recoger, partir o sacar de las angarillas", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			// Sin mochila: 15 kg cómodos, 30 kg de límite. 29 kg en la mano izquierda.
			const FInventoryItem Heavy = Make(Model, TEXT("mero"), 2.9f, 2.5f, EInventorySize::Grande, { FName(TEXT("comida")) }, 10);
			Model.PickUp(Heavy, Fail);
			TestEqual(TEXT("29 kg encima"), Model.GetBodyWeightKg(), 29.0f, 1.0e-3f);

			int64 MergedInto = 0;
			const FInventoryItem Stone = Piedras(Model);
			TestTrue(TEXT("1 kg más cabe justo"), Model.PickUpMerging(Stone, MergedInto, Fail));
			const FInventoryItem Branch = Ramas(Model);
			TestFalse(TEXT("Otra rama ya no"), Model.PickUpMerging(Branch, MergedInto, Fail));
			TestTrue(TEXT("Motivo: el límite de carga o las manos"), Fail == EInventoryFail::OverCarryLimit || Fail == EInventoryFail::HandsFull);

			// De las angarillas al cuerpo: cuenta como peso nuevo.
			FInventoryModel Sledge;
			const FInventoryItem Frame = Angarillas(Sledge);
			FInventoryContainer Load;
			Load.Spec = FInventoryContainerSpec::Sledge();
			const FInventoryItem Stones = Piedras(Sledge, 10);
			Load.Add(Stones, Fail);
			TestTrue(TEXT("Se enganchan"), Sledge.AttachSledge(Frame, Load, Fail));
			Sledge.PickUp(Make(Sledge, TEXT("mero"), 2.5f, 2.5f, EInventorySize::Grande, { FName(TEXT("comida")) }, 10), Fail);
			int64 NewId = 0;
			TestFalse(TEXT("6 kg de piedra no caben con 25 kg encima"), Sledge.SplitStack(Stones.InstanceId, 6, EInventorySlot::HandRight, NewId, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::OverCarryLimit);
			TestTrue(TEXT("5 kg sí"), Sledge.SplitStack(Stones.InstanceId, 5, EInventorySlot::HandRight, NewId, Fail));
			TestEqual(TEXT("30 kg encima"), Sledge.GetBodyWeightKg(), 30.0f, 1.0e-3f);
		});
	});

	Describe("Gastar unidades", [this]()
	{
		It("mengua la pila y la quita con la última", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const int64 Stack = PutInPockets(Model, Ramas(Model, 3));
			TestTrue(TEXT("Una al fuego"), Model.RemoveUnits(Stack, 1, Fail));
			TestEqual(TEXT("Quedan 2"), Model.FindItemById(Stack)->Count, 2);
			TestEqual(TEXT("En el mismo hueco"), Model.GetState().Pockets.GetSlotIndexOf(Stack), 0);
			TestFalse(TEXT("No se gastan 3 de 2"), Model.RemoveUnits(Stack, 3, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::InvalidCount);
			TestFalse(TEXT("Ni 0"), Model.RemoveUnits(Stack, 0, Fail));
			TestTrue(TEXT("Las 2 últimas"), Model.RemoveUnits(Stack, 2, Fail));
			TestTrue(TEXT("La pila desaparece"), Model.FindItem(Stack) == EInventorySlot::None);
		});
	});

	Describe("El guardado", [this]()
	{
		It("conserva las pilas y rechaza las corruptas", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			PutInPockets(Model, Piedras(Model, 7));
			Model.PickUp(Ramas(Model, 10), Fail);
			FInventoryModel Loaded;
			TestTrue(TEXT("Se carga"), Loaded.LoadState(Model.GetState(), Fail));
			TestTrue(TEXT("Igual"), Loaded.GetState() == Model.GetState());

			auto Corrupt = [this, &Model](const TCHAR* What, TFunctionRef<void(FInventoryItem&)> Break)
			{
				FInventoryState State = Model.GetState();
				Break(State.Pockets.Entries[0].Item);
				EInventoryFail Why = EInventoryFail::None;
				FInventoryModel Target;
				const FInventoryState Before = Target.GetState();
				TestFalse(What, Target.LoadState(State, Why));
				TestTrue(FString(What) + TEXT(" (sin cambios)"), Target.GetState() == Before);
			};
			Corrupt(TEXT("Pila de 11"), [](FInventoryItem& I) { I.Count = 11; I.MaxStack = 11; });
			Corrupt(TEXT("Pila de 0"), [](FInventoryItem& I) { I.Count = 0; });
			Corrupt(TEXT("Pila negativa"), [](FInventoryItem& I) { I.Count = -3; });
			Corrupt(TEXT("Más unidades que su tope"), [](FInventoryItem& I) { I.MaxStack = 1; });
			Corrupt(TEXT("Pila con líquido"), [](FInventoryItem& I) { I.LiquidCapacityLiters = 1.0f; });
		});

		It("no acepta equipo puesto en pila", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Backpack = MochilaAlbatros(Model);
			Model.PickUp(Backpack, Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			FInventoryState State = Model.GetState();
			State.BackpackItem.Count = 2;
			State.BackpackItem.MaxStack = 10;
			FInventoryModel Target;
			TestFalse(TEXT("Dos mochilas puestas en un registro"), Target.LoadState(State, Fail));
		});
	});

	Describe("Ids desconocidos al cargar una partida", [this]()
	{
		It("quita lo que ya no existe, suelta lo que se queda sin sitio y carga el resto", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			// Mochila del Albatros con piedras y cerillas en su bolsillo impermeable.
			const FInventoryItem Backpack = MochilaAlbatros(Model);
			Model.PickUp(Backpack, Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			const FInventoryItem InBackpack = Piedras(Model, 6);
			Model.PickUp(InBackpack, Fail);
			Model.Move(InBackpack.InstanceId, EInventorySlot::Backpack, Fail);
			const FInventoryItem Matches = Make(Model, TEXT("cerillas"), 0.02f, 0.02f, EInventorySize::Pequeno, { FName(TEXT("fuego")) });
			Model.PickUp(Matches, Fail);
			Model.Move(Matches.InstanceId, EInventorySlot::Pouch, Fail);
			const FInventoryItem Retired = Make(Model, TEXT("objeto_retirado"), 0.2f, 0.2f, EInventorySize::Pequeno, {}, 3);
			const int64 RetiredId = PutInPockets(Model, Retired);
			const int64 KeptId = PutInPockets(Model, Ramas(Model, 4));
			const FInventoryItem RetiredInHand = Make(Model, TEXT("otro_retirado"), 0.2f, 0.2f, EInventorySize::Pequeno, {});
			Model.PickUp(RetiredInHand, Fail);

			// Una actualización quita «objeto_retirado», «otro_retirado» y la mochila.
			const TArray<FName> Removed = { FName(TEXT("objeto_retirado")), FName(TEXT("otro_retirado")), FName(TEXT("mochila")) };
			auto IsKnown = [&Removed](FName Id) { return !Removed.Contains(Id); };

			FInventoryState Saved = Model.GetState();
			FInventoryLoadReport Report;
			TestTrue(TEXT("Hay cambios"), FInventoryModel::SanitizeUnknownDefinitions(Saved, IsKnown, Report));
			TestEqual(TEXT("Tres desconocidos"), Report.Unknown.Num(), 3);
			TestTrue(TEXT("La pila retirada es uno"), Report.Unknown.ContainsByPredicate([RetiredId](const FInventoryItem& I) { return I.InstanceId == RetiredId && I.Count == 3; }));
			TestEqual(TEXT("Dos huérfanos: las piedras de la mochila y las cerillas de su bolsillo"), Report.Orphaned.Num(), 2);
			TestTrue(TEXT("Las piedras conservan su pila"), Report.Orphaned.ContainsByPredicate([&InBackpack](const FInventoryItem& I) { return I.InstanceId == InBackpack.InstanceId && I.Count == 6; }));
			TestFalse(TEXT("Sin mochila"), Saved.bHasBackpack);
			TestTrue(TEXT("La mano queda libre"), !Saved.HandLeft.IsValid());
			TestEqual(TEXT("Las ramas conocidas siguen en su hueco"), Saved.Pockets.GetSlotIndexOf(KeptId), 1);

			FInventoryModel Loaded;
			TestTrue(TEXT("El resto carga"), Loaded.LoadState(Saved, Fail));
			TestEqual(TEXT("Con las ramas"), Loaded.FindItemById(KeptId)->Count, 4);
			TestTrue(TEXT("Los ids siguientes no chocan con los quitados"), Loaded.AllocateInstanceId() > RetiredInHand.InstanceId);

			FInventoryLoadReport Again;
			TestFalse(TEXT("Sanear dos veces no cambia nada más"), FInventoryModel::SanitizeUnknownDefinitions(Saved, IsKnown, Again));
			TestTrue(TEXT("Informe limpio"), Again.IsClean());
		});

		It("con un DosManos desconocido libera las dos manos", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.PickUp(Make(Model, TEXT("tronco_retirado"), 8.0f, 6.0f, EInventorySize::DosManos, { FName(TEXT("madera")) }), Fail);
			FInventoryState Saved = Model.GetState();
			FInventoryLoadReport Report;
			FInventoryModel::SanitizeUnknownDefinitions(Saved, [](FName Id) { return Id != FName(TEXT("tronco_retirado")); }, Report);
			TestEqual(TEXT("Un solo desconocido"), Report.Unknown.Num(), 1);
			TestTrue(TEXT("Manos libres"), !Saved.HandLeft.IsValid() && !Saved.HandRight.IsValid() && !Saved.bHandsHoldTwoHanded);
			FInventoryModel Loaded;
			TestTrue(TEXT("Carga"), Loaded.LoadState(Saved, Fail));
		});

		It("con un cinturón de cuero desconocido suelta lo que no cabe en tres enganches", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.PickUp(CinturonCuero(Model), Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			TArray<int64> Knives;
			for (int32 Index = 0; Index < 5; ++Index)
			{
				const FInventoryItem Knife = Cuchillo(Model);
				Model.PickUp(Knife, Fail);
				TestTrue(TEXT("Al cinturón"), Model.Move(Knife.InstanceId, EInventorySlot::Belt, Fail));
				Knives.Add(Knife.InstanceId);
			}
			FInventoryState Saved = Model.GetState();
			FInventoryLoadReport Report;
			FInventoryModel::SanitizeUnknownDefinitions(Saved, [](FName Id) { return Id != FName(TEXT("cinturon_cuero")); }, Report);
			TestEqual(TEXT("Tres enganches"), Saved.Belt.Num(), FInventoryModel::BaseBeltHooks);
			TestEqual(TEXT("Dos cuchillos al suelo"), Report.Orphaned.Num(), 2);
			TestTrue(TEXT("Los de los últimos enganches"), Report.Orphaned.ContainsByPredicate([&Knives](const FInventoryItem& I) { return I.InstanceId == Knives[4]; }));
			FInventoryModel Loaded;
			TestTrue(TEXT("Carga"), Loaded.LoadState(Saved, Fail));
		});

		It("no toca una partida en la que todo existe", [this]()
		{
			FInventoryModel Model;
			PutInPockets(Model, Piedras(Model, 3));
			FInventoryState Saved = Model.GetState();
			FInventoryLoadReport Report;
			TestFalse(TEXT("Sin cambios"), FInventoryModel::SanitizeUnknownDefinitions(Saved, [](FName) { return true; }, Report));
			TestTrue(TEXT("Mismo estado"), Saved == Model.GetState());
		});
	});

	Describe("Invariantes", [this]()
	{
		It("se mantienen con miles de operaciones al azar (mismo resultado con la misma semilla)", [this]()
		{
			auto Run = [this](uint32 Seed)
			{
				FInventoryModel Model;
				EInventoryFail Fail = EInventoryFail::None;
				Model.SetCustomBackpack(true, 6.0f, 7.5f, Fail);
				FLcg Rng{ Seed };
				int32 Spawned = 0;
				int32 Spent = 0;
				const EInventorySlot Slots[] = { EInventorySlot::HandLeft, EInventorySlot::HandRight, EInventorySlot::Pockets, EInventorySlot::Backpack };
				for (int32 Step = 0; Step < 3000; ++Step)
				{
					TArray<int64> Ids;
					for (const FInventoryItem* Hand : { &Model.GetState().HandLeft, &Model.GetState().HandRight })
					{
						if (Hand->IsValid())
						{
							Ids.Add(Hand->InstanceId);
						}
					}
					for (const FInventoryContainer* C : { &Model.GetState().Pockets, &Model.GetState().Backpack })
					{
						for (const FInventoryEntry& E : C->Entries)
						{
							Ids.Add(E.Item.InstanceId);
						}
					}
					const int64 AnyId = Ids.Num() > 0 ? Ids[Rng.Range(Ids.Num())] : 0;
					const int64 OtherId = Ids.Num() > 0 ? Ids[Rng.Range(Ids.Num())] : 0;
					const FInventoryItem* Any = Model.FindItemById(AnyId);
					switch (Rng.Range(6))
					{
					case 0:
					{
						const int32 Count = 1 + Rng.Range(4);
						FInventoryItem New = (Rng.Range(2) == 0) ? Piedras(Model, Count) : Ramas(Model, Count);
						int64 MergedInto = 0;
						if (Model.PickUpMerging(New, MergedInto, Fail))
						{
							Spawned += Count;
						}
						break;
					}
					case 1:
					{
						int64 NewId = 0;
						Model.SplitStack(AnyId, 1 + Rng.Range(9), Slots[Rng.Range(4)], NewId, Fail);
						break;
					}
					case 2:
					{
						int32 Moved = 0;
						Model.MergeStacks(AnyId, OtherId, Moved, Fail);
						break;
					}
					case 3:
					{
						FInventoryStowResult Result;
						Model.StowMerging(AnyId, Slots[2 + Rng.Range(2)], Result, Fail);
						break;
					}
					case 4:
					{
						const int32 Count = Any ? 1 + Rng.Range(Any->Count) : 1;
						if (Model.RemoveUnits(AnyId, Count, Fail))
						{
							Spent += Count;
						}
						break;
					}
					default:
						Model.Move(AnyId, Slots[Rng.Range(4)], Fail);
						break;
					}

					EInventoryFail Why = EInventoryFail::None;
					if (!FInventoryModel::ValidateState(Model.GetState(), Why))
					{
						AddError(FString::Printf(TEXT("Estado inválido en el paso %d (semilla %u): %s"), Step, Seed, LexToString(Why)));
						return FInventoryState();
					}
					const int32 Carried = CountUnits(Model, TEXT("basalto")) + CountUnits(Model, TEXT("rama_seca"));
					if (Carried != Spawned - Spent)
					{
						AddError(FString::Printf(TEXT("Paso %d: se llevan %d unidades y deberían ser %d"), Step, Carried, Spawned - Spent));
						return FInventoryState();
					}
					if (Model.GetBodyWeightKg() > FInventoryModel::BaseComfortableKg * FInventoryModel::MaxLoadRatio + 1.0e-3f)
					{
						AddError(FString::Printf(TEXT("Paso %d: %.3f kg encima, por encima del límite"), Step, Model.GetBodyWeightKg()));
						return FInventoryState();
					}
				}
				return Model.GetState();
			};
			for (uint32 Seed : { 1u, 7u, 1234u, 0xC0FFEEu })
			{
				const FInventoryState A = Run(Seed);
				const FInventoryState B = Run(Seed);
				TestTrue(FString::Printf(TEXT("Determinista con la semilla %u"), Seed), A == B);
			}
		});
	});
}

#endif
