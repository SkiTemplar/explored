#include "Misc/AutomationTest.h"

#include "Carry/InventoryModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace InventoryTest
{
	/**
	 * Registros con los mismos números que Content/Data/items.json: el modelo
	 * no lee el JSON (eso lo hace UItemRegistrySubsystem), así que el test fija
	 * los datos que necesita y no depende de que alguien retoque el catálogo.
	 */
	FInventoryItem Make(FInventoryModel& Model, const TCHAR* Id, float WeightKg, float VolumeLiters, EInventorySize Size, TArray<FName> Tags)
	{
		FInventoryItem Item;
		Item.InstanceId = Model.AllocateInstanceId();
		Item.DefinitionId = FName(Id);
		Item.WeightKg = WeightKg;
		Item.VolumeLiters = VolumeLiters;
		Item.Size = Size;
		Item.Tags = MoveTemp(Tags);
		return Item;
	}

	FInventoryItem Coco(FInventoryModel& M) { return Make(M, TEXT("coco_maduro"), 0.6f, 0.6f, EInventorySize::Pequeno, { FName(TEXT("comida")), FName(TEXT("coco")) }); }
	FInventoryItem Piedra(FInventoryModel& M) { return Make(M, TEXT("basalto"), 1.0f, 0.4f, EInventorySize::Pequeno, { FName(TEXT("piedra")) }); }
	FInventoryItem Tronco(FInventoryModel& M) { return Make(M, TEXT("tronco_pequeno"), 8.0f, 6.0f, EInventorySize::DosManos, { FName(TEXT("madera")) }); }
	FInventoryItem Hacha(FInventoryModel& M) { return Make(M, TEXT("hacha"), 1.0f, 1.0f, EInventorySize::Mediano, { FName(TEXT("herramienta")), FName(TEXT("corte")) }); }
	FInventoryItem Cuchillo(FInventoryModel& M) { return Make(M, TEXT("cuchillo"), 0.3f, 0.2f, EInventorySize::Pequeno, { FName(TEXT("herramienta")), FName(TEXT("corte")) }); }
	FInventoryItem Palo(FInventoryModel& M) { return Make(M, TEXT("palo_recto"), 0.5f, 0.6f, EInventorySize::Mediano, { FName(TEXT("madera")), FName(TEXT("mango")) }); }
	FInventoryItem Brujula(FInventoryModel& M) { return Make(M, TEXT("brujula"), 0.1f, 0.05f, EInventorySize::Pequeno, { FName(TEXT("rescatado")), FName(TEXT("instrumento")), FName(TEXT("brujula")) }); }
	FInventoryItem Cerillas(FInventoryModel& M) { return Make(M, TEXT("cerillas"), 0.02f, 0.02f, EInventorySize::Pequeno, { FName(TEXT("rescatado")), FName(TEXT("fuego")) }); }
	FInventoryItem Bolsa(FInventoryModel& M) { return Make(M, TEXT("bolsa_impermeable"), 0.1f, 0.3f, EInventorySize::Pequeno, { FName(TEXT("contenedor")), FName(TEXT("impermeable")) }); }
	FInventoryItem MochilaAlbatros(FInventoryModel& M) { return Make(M, TEXT("mochila"), 0.8f, 2.0f, EInventorySize::Grande, { FName(TEXT("rescatado")), FName(TEXT("mochila")) }); }
	FInventoryItem MochilaFibra(FInventoryModel& M) { return Make(M, TEXT("mochila_fibra"), 0.9f, 2.0f, EInventorySize::Grande, { FName(TEXT("mochila")), FName(TEXT("fibra")) }); }
	FInventoryItem MochilaCuero(FInventoryModel& M) { return Make(M, TEXT("mochila_cuero_bambu"), 1.6f, 2.5f, EInventorySize::Grande, { FName(TEXT("mochila")), FName(TEXT("piel")) }); }
	FInventoryItem CinturonCuero(FInventoryModel& M) { return Make(M, TEXT("cinturon_cuero"), 0.4f, 0.3f, EInventorySize::Pequeno, { FName(TEXT("cinturon")), FName(TEXT("piel")) }); }
	FInventoryItem Angarillas(FInventoryModel& M) { return Make(M, TEXT("angarillas"), 6.0f, 20.0f, EInventorySize::DosManos, { FName(TEXT("angarillas")), FName(TEXT("madera")) }); }

	FInventoryItem Cantimplora(FInventoryModel& M)
	{
		FInventoryItem Item = Make(M, TEXT("cantimplora"), 0.3f, 1.0f, EInventorySize::Pequeno, { FName(TEXT("rescatado")), FName(TEXT("recipiente")), FName(TEXT("cantimplora")) });
		Item.LiquidCapacityLiters = FInventoryModel::LiquidCapacityFromRecipiente(4.0f);
		return Item;
	}

	/** Coge a la mano y lo guarda; devuelve si ha entrado. */
	bool PickAndStore(FInventoryModel& Model, const FInventoryItem& Item, EInventorySlot To, EInventoryFail& OutFail)
	{
		if (!Model.PickUp(Item, OutFail))
		{
			return false;
		}
		return Model.Move(Item.InstanceId, To, OutFail);
	}

	void EquipBackpack(FInventoryModel& Model, const FInventoryItem& Backpack)
	{
		EInventoryFail Fail = EInventoryFail::None;
		Model.PickUp(Backpack, Fail);
		Model.EquipFromHand(Model.FindItem(Backpack.InstanceId), Fail);
	}
}

BEGIN_DEFINE_SPEC(FInventorySpec, "Explored.Inventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FInventorySpec)

void FInventorySpec::Define()
{
	using namespace InventoryTest;

	Describe("Las manos", [this]()
	{
		It("llevan un objeto en cada una y rechazan el tercero", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem A = Coco(Model);
			const FInventoryItem B = Piedra(Model);
			TestTrue(TEXT("Primero a la izquierda"), Model.PickUp(A, Fail));
			TestTrue(TEXT("Está en la izquierda"), Model.FindItem(A.InstanceId) == EInventorySlot::HandLeft);
			TestTrue(TEXT("Segundo a la derecha"), Model.PickUp(B, Fail));
			TestTrue(TEXT("Está en la derecha"), Model.FindItem(B.InstanceId) == EInventorySlot::HandRight);
			TestFalse(TEXT("No hay tercera mano"), Model.PickUp(Coco(Model), Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::HandsFull);
		});

		It("exigen las dos libres para un DosManos y lo cuentan una sola vez", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Log = Tronco(Model);
			TestTrue(TEXT("Se coge el tronco"), Model.PickUp(Log, Fail));
			TestTrue(TEXT("Ocupa las dos manos"), !Model.IsHandEmpty(EInventorySlot::HandLeft) && !Model.IsHandEmpty(EInventorySlot::HandRight));
			TestTrue(TEXT("Es el mismo objeto"), Model.IsHoldingTwoHanded());
			TestEqual(TEXT("Pesa una vez (revisión: GetTotalWeight con DosManos)"), Model.GetBodyWeightKg(), 8.0f);
			TestFalse(TEXT("No se cambia de mano"), Model.SwapHands());

			FInventoryModel Other;
			Other.PickUp(Coco(Other), Fail);
			TestFalse(TEXT("Con una mano ocupada no se coge"), Other.PickUp(Tronco(Other), Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NeedBothHands);
		});

		It("no combinan un DosManos consigo mismo (H3)", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.PickUp(Tronco(Model), Fail);
			TestFalse(TEXT("No se puede combinar"), Model.CanCombineHands(Fail));
			TestTrue(TEXT("Motivo: es el mismo objeto"), Fail == EInventoryFail::SameItem);

			FInventoryModel Pair;
			Pair.PickUp(Cuchillo(Pair), Fail);
			TestFalse(TEXT("Con una sola pieza no se combina"), Pair.CanCombineHands(Fail));
			Pair.PickUp(Palo(Pair), Fail);
			TestTrue(TEXT("Dos piezas distintas sí"), Pair.CanCombineHands(Fail));
		});

		It("se vacían al fabricar y reciben el resultado", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.PickUp(Cuchillo(Model), Fail);
			Model.PickUp(Palo(Model), Fail);
			const TArray<FInventoryItem> Removed = Model.ClearHands();
			TestEqual(TEXT("Salen las dos piezas"), Removed.Num(), 2);
			TestTrue(TEXT("Manos vacías"), Model.IsHandEmpty(EInventorySlot::HandLeft) && Model.IsHandEmpty(EInventorySlot::HandRight));
			TestTrue(TEXT("El resultado entra"), Model.PickUp(Hacha(Model), Fail));

			FInventoryModel TwoHanded;
			TwoHanded.PickUp(Tronco(TwoHanded), Fail);
			TestEqual(TEXT("Un DosManos sale una vez"), TwoHanded.ClearHands().Num(), 1);
		});
	});

	Describe("Los bolsillos", [this]()
	{
		It("guardan cuatro objetos pequeños y ni uno más", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			for (int32 Index = 0; Index < FInventoryModel::PocketSlots; ++Index)
			{
				TestTrue(FString::Printf(TEXT("Bolsillo %d"), Index), PickAndStore(Model, Piedra(Model), EInventorySlot::Pockets, Fail));
			}
			const FInventoryItem Fifth = Piedra(Model);
			TestFalse(TEXT("El quinto no entra"), PickAndStore(Model, Fifth, EInventorySlot::Pockets, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerFull);
			TestTrue(TEXT("Sigue en la mano"), Model.FindItem(Fifth.InstanceId) == EInventorySlot::HandLeft);
		});

		It("rechazan lo que no es pequeño", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestFalse(TEXT("El hacha no cabe"), PickAndStore(Model, Hacha(Model), EInventorySlot::Pockets, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::TooBig);
			FInventoryModel Other;
			TestFalse(TEXT("Un tronco tampoco"), PickAndStore(Other, Tronco(Other), EInventorySlot::Pockets, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::TooBig);
		});
	});

	Describe("El cinturón", [this]()
	{
		It("solo lleva herramientas, recipientes e instrumentos", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestTrue(TEXT("Hacha"), PickAndStore(Model, Hacha(Model), EInventorySlot::Belt, Fail));
			TestTrue(TEXT("Cantimplora"), PickAndStore(Model, Cantimplora(Model), EInventorySlot::Belt, Fail));
			TestFalse(TEXT("Un coco no se cuelga"), PickAndStore(Model, Coco(Model), EInventorySlot::Belt, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::WrongKind);
		});

		It("tiene tres enganches y cinco con el cinturón de cuero", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestEqual(TEXT("Tres de serie"), Model.GetBeltHooks(), FInventoryModel::BaseBeltHooks);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestTrue(TEXT("Enganche libre"), PickAndStore(Model, Cuchillo(Model), EInventorySlot::Belt, Fail));
			}
			const FInventoryItem Fourth = Cuchillo(Model);
			TestFalse(TEXT("El cuarto no cabe"), PickAndStore(Model, Fourth, EInventorySlot::Belt, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerFull);

			const FInventoryItem Belt = CinturonCuero(Model);
			Model.PickUp(Belt, Fail);
			TestTrue(TEXT("Se pone el cinturón de cuero"), Model.EquipFromHand(Model.FindItem(Belt.InstanceId), Fail));
			TestEqual(TEXT("Cinco enganches"), Model.GetBeltHooks(), 5);
			TestTrue(TEXT("Ahora el cuarto sí"), Model.Move(Fourth.InstanceId, EInventorySlot::Belt, Fail));

			TestFalse(TEXT("No se quita con cuatro colgados"), Model.UnequipBelt(EInventorySlot::HandLeft, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerNotEmpty);
			TestEqual(TEXT("El cinturón sigue puesto"), Model.GetBeltHooks(), 5);
		});
	});

	Describe("La mochila", [this]()
	{
		It("no guarda nada si no se lleva", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestFalse(TEXT("Sin mochila"), PickAndStore(Model, Coco(Model), EInventorySlot::Backpack, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NoBackpack);
		});

		It("respeta volumen y peso", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			// Como CarrySpec: dos cocos por peso aunque el volumen dé para diez.
			TestTrue(TEXT("Mochila a medida"), Model.SetCustomBackpack(true, 0.6f * 10.0f, 0.6f * 2.5f, Fail));
			TestTrue(TEXT("Coco 1"), PickAndStore(Model, Coco(Model), EInventorySlot::Backpack, Fail));
			TestTrue(TEXT("Coco 2"), PickAndStore(Model, Coco(Model), EInventorySlot::Backpack, Fail));
			TestFalse(TEXT("Coco 3 pesa demasiado"), PickAndStore(Model, Coco(Model), EInventorySlot::Backpack, Fail));
			TestTrue(TEXT("Motivo peso"), Fail == EInventoryFail::TooHeavy);

			FInventoryModel ByVolume;
			ByVolume.SetCustomBackpack(true, 1.0f, 50.0f, Fail);
			TestTrue(TEXT("Un palo entra"), PickAndStore(ByVolume, Palo(ByVolume), EInventorySlot::Backpack, Fail));
			TestFalse(TEXT("Otro coco no cabe"), PickAndStore(ByVolume, Coco(ByVolume), EInventorySlot::Backpack, Fail));
			TestTrue(TEXT("Motivo volumen"), Fail == EInventoryFail::NoRoom);
		});

		It("se mejora de la del Albatros a la de fibra y a la de cuero conservando el contenido", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Albatros = MochilaAlbatros(Model);
			EquipBackpack(Model, Albatros);
			TestTrue(TEXT("Lleva mochila"), Model.HasBackpack());
			TestTrue(TEXT("La del Albatros trae bolsillo impermeable"), Model.HasPouch());
			const float AlbatrosLiters = Model.GetContainer(EInventorySlot::Backpack)->Spec.MaxVolumeLiters;

			const FInventoryItem Stone = Piedra(Model);
			PickAndStore(Model, Stone, EInventorySlot::Backpack, Fail);

			const FInventoryItem Fibra = MochilaFibra(Model);
			Model.PickUp(Fibra, Fail);
			const EInventorySlot Hand = Model.FindItem(Fibra.InstanceId);
			TestTrue(TEXT("Se cambia a la de fibra"), Model.EquipFromHand(Hand, Fail));
			TestTrue(TEXT("Más volumen"), Model.GetContainer(EInventorySlot::Backpack)->Spec.MaxVolumeLiters > AlbatrosLiters);
			TestTrue(TEXT("La piedra pasa a la nueva"), Model.FindItem(Stone.InstanceId) == EInventorySlot::Backpack);
			TestTrue(TEXT("La vieja queda en la mano"), Model.FindItem(Albatros.InstanceId) == Hand);

			const float ComfortBefore = Model.GetComfortableCapacityKg();
			const float WeightBefore = Model.GetContainer(EInventorySlot::Backpack)->Spec.MaxWeightKg;
			TestFalse(TEXT("Sin angarillas no hay dónde arrastrarla"), Model.Move(Albatros.InstanceId, EInventorySlot::Sledge, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NoSledge);
			FInventoryItem Dropped;
			Model.RemoveFromHand(Hand, Dropped, Fail);
			EquipBackpack(Model, MochilaCuero(Model));
			TestTrue(TEXT("El armazón aguanta más peso"), Model.GetContainer(EInventorySlot::Backpack)->Spec.MaxWeightKg > WeightBefore);
			TestTrue(TEXT("Y se lleva más cómodo"), Model.GetComfortableCapacityKg() > ComfortBefore);
		});

		It("no se cambia si el contenido no cabe en la nueva, y no se quita llena", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			EquipBackpack(Model, MochilaCuero(Model));
			for (int32 Index = 0; Index < 15; ++Index)
			{
				TestTrue(TEXT("Piedra a la mochila de cuero"), PickAndStore(Model, Piedra(Model), EInventorySlot::Backpack, Fail)); // 15 kg
			}
			const FInventoryItem Small = MochilaAlbatros(Model);
			Model.PickUp(Small, Fail);
			const FInventoryState Before = Model.GetState();
			TestFalse(TEXT("15 kg no caben en la del Albatros (10 kg)"), Model.EquipFromHand(Model.FindItem(Small.InstanceId), Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::TooHeavy);
			TestTrue(TEXT("Nada ha cambiado"), Model.GetState() == Before);

			TestFalse(TEXT("Llena no se quita"), Model.UnequipBackpack(EInventorySlot::HandRight, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerNotEmpty);
			TestTrue(TEXT("Nada ha cambiado"), Model.GetState() == Before);
		});
	});

	Describe("La bolsa estanca", [this]()
	{
		It("guarda seco lo que se cuelga con la bolsa y no se quita con cosas dentro", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Matches = Cerillas(Model);
			TestFalse(TEXT("Sin bolsa no hay bolsa"), PickAndStore(Model, Matches, EInventorySlot::Pouch, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NoPouch);

			const FInventoryItem Pouch = Bolsa(Model);
			TestTrue(TEXT("La bolsa se cuelga del cinturón"), PickAndStore(Model, Pouch, EInventorySlot::Belt, Fail));
			TestTrue(TEXT("Ya hay bolsa"), Model.HasPouch());
			TestTrue(TEXT("Lo que no debe mojarse se propone para la bolsa"), Model.SuggestStowSlot(Matches) == EInventorySlot::Pouch);
			TestTrue(TEXT("Las cerillas entran"), Model.Move(Matches.InstanceId, EInventorySlot::Pouch, Fail));
			TestTrue(TEXT("Van secas"), Model.IsStoredDry(Matches.InstanceId));
			TestTrue(TEXT("Todo lo de fuego va seco"), Model.AreTaggedItemsDry(FName(TEXT("fuego"))));

			TestFalse(TEXT("La bolsa no se descuelga con cosas dentro"), Model.Move(Pouch.InstanceId, EInventorySlot::HandLeft, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerNotEmpty);

			TestTrue(TEXT("Las cerillas salen a la mano"), Model.Move(Matches.InstanceId, EInventorySlot::HandLeft, Fail));
			TestFalse(TEXT("En la mano se mojan"), Model.AreTaggedItemsDry(FName(TEXT("fuego"))));
			TestTrue(TEXT("Vacía, la bolsa se descuelga"), Model.Move(Pouch.InstanceId, EInventorySlot::HandRight, Fail));
		});
	});

	Describe("Las angarillas", [this]()
	{
		It("cargan troncos y piedra, no comida, y liberan las manos", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Sledge = Angarillas(Model);
			Model.PickUp(Sledge, Fail);
			TestTrue(TEXT("Se enganchan"), Model.EquipFromHand(EInventorySlot::HandLeft, Fail));
			TestTrue(TEXT("Manos libres"), Model.IsHandEmpty(EInventorySlot::HandLeft) && Model.IsHandEmpty(EInventorySlot::HandRight));

			for (int32 Index = 0; Index < 8; ++Index)
			{
				TestTrue(FString::Printf(TEXT("Tronco %d"), Index), PickAndStore(Model, Tronco(Model), EInventorySlot::Sledge, Fail));
			}
			TestFalse(TEXT("No caben más de ocho"), PickAndStore(Model, Tronco(Model), EInventorySlot::Sledge, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::ContainerFull);

			FInventoryModel Food;
			Food.PickUp(Angarillas(Food), Fail);
			Food.EquipFromHand(EInventorySlot::HandLeft, Fail);
			TestFalse(TEXT("La comida no va en las angarillas"), PickAndStore(Food, Coco(Food), EInventorySlot::Sledge, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::WrongKind);
		});

		It("frenan, impiden nadar y trepar y se sueltan al entrar en el agua", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Sledge = Angarillas(Model);
			Model.PickUp(Sledge, Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			TestFalse(TEXT("No se nada"), Model.CanSwim());
			TestFalse(TEXT("No se trepa"), Model.CanClimb());
			const float EmptySpeed = Model.GetMoveSpeedMultiplier();
			TestTrue(TEXT("Frenan vacías"), EmptySpeed < 1.0f);
			for (int32 Index = 0; Index < 8; ++Index)
			{
				PickAndStore(Model, Tronco(Model), EInventorySlot::Sledge, Fail);
			}
			TestTrue(TEXT("Frenan más cargadas"), Model.GetMoveSpeedMultiplier() < EmptySpeed);

			FInventorySledgeDrop Drop;
			TestTrue(TEXT("Al agua se sueltan"), Model.ApplyEnterWater(Drop));
			TestEqual(TEXT("Con toda su carga"), Drop.Load.Num(), 8);
			TestEqual(TEXT("Y son las mismas angarillas"), Drop.SledgeItem.InstanceId, Sledge.InstanceId);
			TestTrue(TEXT("Ya se puede nadar"), Model.CanSwim());
			FInventorySledgeDrop Again;
			TestFalse(TEXT("No se suelta dos veces"), Model.ApplyEnterWater(Again));

			TestTrue(TEXT("Se vuelven a enganchar con la carga"), Model.AttachSledge(Drop.SledgeItem, Drop.Load, Fail));
			TestTrue(TEXT("La carga vuelve"), Model.GetContainer(EInventorySlot::Sledge)->Num() == 8);
			TestFalse(TEXT("No hay dos angarillas"), Model.AttachSledge(Angarillas(Model), FInventoryContainer(), Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::SledgeAttached);
		});

		It("arrastran mucho peso contando solo una parte en la carga", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.PickUp(Angarillas(Model), Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			for (int32 Index = 0; Index < 8; ++Index)
			{
				PickAndStore(Model, Tronco(Model), EInventorySlot::Sledge, Fail);
			}
			const float Hauled = Model.GetSledgeWeightKg();
			TestEqual(TEXT("64 kg de troncos más las angarillas"), Hauled, 70.0f);
			TestEqual(TEXT("Cuenta el arrastre, no el peso entero"),
				Model.GetCarriedWeightRatio(), Hauled * FInventoryModel::SledgeDragFactor / FInventoryModel::BaseComfortableKg, 1.0e-4f);
			TestEqual(TEXT("Nada encima del cuerpo"), Model.GetSwimLoadRatio(), 0.0f);
		});
	});

	Describe("El peso", [this]()
	{
		It("da la proporción sobre la capacidad cómoda e impide pasarse del doble", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestEqual(TEXT("Sin nada"), Model.GetCarriedWeightRatio(), 0.0f);
			Model.PickUp(Tronco(Model), Fail);
			TestEqual(TEXT("8 kg de 15"), Model.GetCarriedWeightRatio(), 8.0f / 15.0f, 1.0e-4f);

			FInventoryModel Heavy;
			Heavy.SetCustomBackpack(true, 200.0f, 200.0f, Fail);
			int32 Stored = 0;
			while (PickAndStore(Heavy, Piedra(Heavy), EInventorySlot::Backpack, Fail))
			{
				++Stored;
			}
			TestTrue(TEXT("Se para en el doble de lo cómodo"), Fail == EInventoryFail::OverCarryLimit);
			TestEqual(TEXT("30 piedras de 1 kg = 30 kg"), Stored, 30);
			TestTrue(TEXT("Sobrecargado anda más lento"), Heavy.GetMoveSpeedMultiplier() < 1.0f);
			TestTrue(TEXT("Y hace más ruido"), Heavy.GetNoiseLevel() > Model.GetNoiseLevel());
		});

		It("incluye el agua de la cantimplora", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Canteen = Cantimplora(Model);
			PickAndStore(Model, Canteen, EInventorySlot::Belt, Fail);
			TestEqual(TEXT("Vacía"), Model.GetCarriedWaterLiters(), 0.0f);
			TestEqual(TEXT("Se llena hasta su capacidad (1 l)"), Model.FillLiquid(Canteen.InstanceId, 5.0f, Fail), 1.0f);
			TestEqual(TEXT("Lleva un litro"), Model.GetCarriedWaterLiters(), 1.0f);
			TestEqual(TEXT("Pesa lo suyo más el agua"), Model.GetBodyWeightKg(), 1.3f, 1.0e-4f);
			TestEqual(TEXT("Se bebe medio litro"), Model.DrinkFrom(Canteen.InstanceId, 0.5f), 0.5f);
			TestEqual(TEXT("Queda medio"), Model.GetCarriedWaterLiters(), 0.5f);

			const FInventoryItem Stone = Piedra(Model);
			Model.PickUp(Stone, Fail);
			Model.FillLiquid(Stone.InstanceId, 1.0f, Fail);
			TestTrue(TEXT("Una piedra no guarda agua"), Fail == EInventoryFail::NotALiquidContainer);
		});

		It("no llena ni vacía con litros no finitos", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Inf = std::numeric_limits<float>::infinity();
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Canteen = Cantimplora(Model);
			PickAndStore(Model, Canteen, EInventorySlot::Belt, Fail);
			TestEqual(TEXT("NaN litros no llenan"), Model.FillLiquid(Canteen.InstanceId, NaN, Fail), 0.0f);
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NoRoom);
			TestEqual(TEXT("Sigue vacía"), Model.GetCarriedWaterLiters(), 0.0f);
			TestEqual(TEXT("Infinitos litros tampoco"), Model.FillLiquid(Canteen.InstanceId, Inf, Fail), 0.0f);
			TestEqual(TEXT("Se llena medio litro"), Model.FillLiquid(Canteen.InstanceId, 0.5f, Fail), 0.5f);
			TestEqual(TEXT("NaN litros no se beben"), Model.DrinkFrom(Canteen.InstanceId, NaN), 0.0f);
			TestEqual(TEXT("Queda medio"), Model.GetCarriedWaterLiters(), 0.5f);
		});
	});

	Describe("Las etiquetas", [this]()
	{
		It("dicen si hay brújula a mano (bolsillo o cinturón, no en la mochila)", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			EquipBackpack(Model, MochilaFibra(Model));
			const FInventoryItem Compass = Brujula(Model);
			PickAndStore(Model, Compass, EInventorySlot::Backpack, Fail);
			TestTrue(TEXT("La lleva"), Model.HasItemWithTag(FName(TEXT("brujula"))));
			TestFalse(TEXT("Pero no a mano"), Model.HasCompassAtHand());
			TestTrue(TEXT("Al bolsillo"), Model.Move(Compass.InstanceId, EInventorySlot::Pockets, Fail));
			TestTrue(TEXT("Ahora sí"), Model.HasCompassAtHand());
			TestTrue(TEXT("Al cinturón"), Model.Move(Compass.InstanceId, EInventorySlot::Belt, Fail));
			TestTrue(TEXT("También"), Model.HasCompassAtHand());
			TestFalse(TEXT("Nada de mapas"), Model.HasItemWithTag(FName(TEXT("mapa"))));
			TestTrue(TEXT("Sin mapa, el mapa no se moja"), Model.AreTaggedItemsDry(FName(TEXT("mapa"))));
		});

		It("proponen dónde guardar cada cosa", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestTrue(TEXT("Herramienta al cinturón"), Model.SuggestStowSlot(Hacha(Model)) == EInventorySlot::Belt);
			TestTrue(TEXT("Piedra al bolsillo"), Model.SuggestStowSlot(Piedra(Model)) == EInventorySlot::Pockets);
			TestTrue(TEXT("Un tronco sin angarillas no cabe en ningún sitio"), Model.SuggestStowSlot(Tronco(Model)) == EInventorySlot::None);
			Model.PickUp(Angarillas(Model), Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			const FInventoryItem Log = Tronco(Model);
			Model.PickUp(Log, Fail);
			EInventorySlot Where = EInventorySlot::None;
			TestTrue(TEXT("Guardado automático"), Model.AutoStowFromHand(EInventorySlot::HandLeft, Where, Fail));
			TestTrue(TEXT("El tronco va a las angarillas"), Where == EInventorySlot::Sledge);
		});
	});

	Describe("Los contenedores del mundo", [this]()
	{
		It("tienen huecos visibles estables y límites propios", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			FInventoryContainer Basket;
			Basket.Id = FName(TEXT("cesta_base_01"));
			Basket.Spec = FInventoryContainerSpec::Basket();

			const FInventoryItem A = Coco(Model);
			const FInventoryItem B = Coco(Model);
			const FInventoryItem C = Coco(Model);
			for (const FInventoryItem& Item : { A, B })
			{
				Model.PickUp(Item, Fail);
				TestTrue(TEXT("A la cesta"), Model.StoreInWorld(Item.InstanceId, Basket, Fail));
			}
			TestEqual(TEXT("A en el hueco 0"), Basket.GetSlotIndexOf(A.InstanceId), 0);
			TestEqual(TEXT("B en el hueco 1"), Basket.GetSlotIndexOf(B.InstanceId), 1);
			TestTrue(TEXT("Se saca A"), Model.TakeFromWorld(Basket, A.InstanceId, EInventorySlot::HandLeft, Fail));
			TestEqual(TEXT("B no se mueve"), Basket.GetSlotIndexOf(B.InstanceId), 1);
			Model.PickUp(C, Fail);
			Model.StoreInWorld(C.InstanceId, Basket, Fail);
			TestEqual(TEXT("C ocupa el hueco libre de A"), Basket.GetSlotIndexOf(C.InstanceId), 0);
			TestTrue(TEXT("Se encuentra por hueco"), Basket.FindBySlotIndex(0) && Basket.FindBySlotIndex(0)->InstanceId == C.InstanceId);

			FInventoryModel Logs;
			const FInventoryItem Log = Tronco(Logs);
			Logs.PickUp(Log, Fail);
			TestFalse(TEXT("Un tronco no cabe en la cesta"), Logs.StoreInWorld(Log.InstanceId, Basket, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::TooBig);
			FInventoryContainer Shelf;
			Shelf.Spec = FInventoryContainerSpec::Shelf();
			TestFalse(TEXT("Ni en el estante (es DosManos)"), Logs.StoreInWorld(Log.InstanceId, Shelf, Fail));
			TestTrue(TEXT("El arcón tiene 12 huecos"), FInventoryContainerSpec::Chest().MaxSlots == 12);
		});

		It("no cambian nada si el traslado falla", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			FInventoryContainer Full;
			Full.Spec = FInventoryContainerSpec::Basket();
			Full.Spec.MaxSlots = 1;
			FInventoryModel Filler;
			const FInventoryItem Other = Coco(Filler);
			Full.Add(Other, Fail);

			const FInventoryItem Item = Coco(Model);
			Model.PickUp(Item, Fail);
			const FInventoryState Before = Model.GetState();
			const FInventoryContainer WorldBefore = Full;
			TestFalse(TEXT("La cesta está llena"), Model.StoreInWorld(Item.InstanceId, Full, Fail));
			TestTrue(TEXT("El jugador no cambia"), Model.GetState() == Before);
			TestTrue(TEXT("La cesta no cambia"), Full == WorldBefore);

			// Sacar a una mano ocupada tampoco toca nada.
			Model.PickUp(Coco(Model), Fail);
			FInventoryContainer Source;
			Source.Spec = FInventoryContainerSpec::Chest();
			// Lo que hay en el mundo salió antes del mismo inventario: ids del mismo modelo.
			Source.Add(Hacha(Model), Fail);
			const FInventoryState Before2 = Model.GetState();
			const FInventoryContainer SourceBefore = Source;
			TestFalse(TEXT("Mano ocupada"), Model.TakeFromWorld(Source, Source.Entries[0].Item.InstanceId, EInventorySlot::HandLeft, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::HandOccupied);
			TestTrue(TEXT("El jugador no cambia"), Model.GetState() == Before2);
			TestTrue(TEXT("El arcón no cambia"), Source == SourceBefore);

			// Entre contenedores del cuerpo, igual.
			const FInventoryItem Axe = Hacha(Model);
			FInventoryModel Pocketless;
			Pocketless.PickUp(Axe, Fail);
			const FInventoryState Before3 = Pocketless.GetState();
			TestFalse(TEXT("El hacha no va al bolsillo"), Pocketless.Move(Axe.InstanceId, EInventorySlot::Pockets, Fail));
			TestTrue(TEXT("Sigue todo igual"), Pocketless.GetState() == Before3);
		});
	});

	Describe("El guardado", [this]()
	{
		It("recupera exactamente el mismo estado", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			EquipBackpack(Model, MochilaAlbatros(Model));
			const FInventoryItem Belt = CinturonCuero(Model);
			Model.PickUp(Belt, Fail);
			Model.EquipFromHand(Model.FindItem(Belt.InstanceId), Fail);
			PickAndStore(Model, Hacha(Model), EInventorySlot::Belt, Fail);
			const FInventoryItem Canteen = Cantimplora(Model);
			PickAndStore(Model, Canteen, EInventorySlot::Belt, Fail);
			Model.FillLiquid(Canteen.InstanceId, 0.7f, Fail);
			PickAndStore(Model, Cerillas(Model), EInventorySlot::Pouch, Fail);
			PickAndStore(Model, Piedra(Model), EInventorySlot::Pockets, Fail);
			PickAndStore(Model, Coco(Model), EInventorySlot::Backpack, Fail);
			Model.PickUp(Angarillas(Model), Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			PickAndStore(Model, Tronco(Model), EInventorySlot::Sledge, Fail);
			Model.PickUp(Tronco(Model), Fail);

			const FInventoryState Saved = Model.GetState();
			FInventoryModel Loaded;
			TestTrue(TEXT("Se carga"), Loaded.LoadState(Saved, Fail));
			TestTrue(TEXT("Mismo estado"), Loaded.GetState() == Saved);
			TestEqual(TEXT("Mismo peso"), Loaded.GetCarriedWeightRatio(), Model.GetCarriedWeightRatio());
			TestEqual(TEXT("Misma agua"), Loaded.GetCarriedWaterLiters(), 0.7f, 1.0e-4f);
			TestEqual(TEXT("Mismos enganches"), Loaded.GetBeltHooks(), 5);
			TestTrue(TEXT("Sigue con el DosManos en las manos"), Loaded.IsHoldingTwoHanded());
			TestTrue(TEXT("Los ids nuevos no repiten"), Loaded.AllocateInstanceId() == Model.AllocateInstanceId());
		});

		It("rechaza estados incoherentes sin tocar el actual", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Stone = Piedra(Model);
			PickAndStore(Model, Stone, EInventorySlot::Pockets, Fail);
			const FInventoryState Good = Model.GetState();
			// Los objetos de los estados falsos salen de otro modelo para no gastar ids de este.
			FInventoryModel Ids;
			for (int32 Index = 0; Index < 50; ++Index)
			{
				Ids.AllocateInstanceId();
			}

			FInventoryState Duplicated = Good;
			Duplicated.HandLeft = Stone;
			TestFalse(TEXT("Id repetido"), Model.LoadState(Duplicated, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::DuplicateId);

			FInventoryState TooMany = Good;
			for (int32 Index = 0; Index < 5; ++Index)
			{
				FInventoryEntry Entry;
				Entry.Item = Piedra(Ids);
				Entry.SlotIndex = Index + 1;
				TooMany.Pockets.Entries.Add(Entry);
			}
			TooMany.NextInstanceId = 100;
			TestFalse(TEXT("Más bolsillos de la cuenta"), Model.LoadState(TooMany, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::CorruptState);

			FInventoryState OrphanPouch = Good;
			FInventoryEntry Wet;
			Wet.Item = Cerillas(Ids);
			OrphanPouch.Pouch.Entries.Add(Wet);
			OrphanPouch.NextInstanceId = 100;
			TestFalse(TEXT("Bolsa sin bolsa"), Model.LoadState(OrphanPouch, Fail));

			FInventoryState StaleCounter = Good;
			StaleCounter.NextInstanceId = 1;
			TestFalse(TEXT("El contador de ids no puede quedarse atrás"), Model.LoadState(StaleCounter, Fail));

			TestTrue(TEXT("El estado bueno sigue ahí"), Model.GetState() == Good);
		});

		It("rechaza pesos, volúmenes y líquidos no finitos o negativos", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Inf = std::numeric_limits<float>::infinity();
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestTrue(TEXT("Mochila a medida"), Model.SetCustomBackpack(true, 20.0f, 10.0f, Fail));
			const FInventoryItem Stone = Piedra(Model);
			PickAndStore(Model, Stone, EInventorySlot::Pockets, Fail);
			const FInventoryItem Canteen = Cantimplora(Model);
			Model.PickUp(Canteen, Fail);
			const FInventoryState Good = Model.GetState();
			TestTrue(TEXT("El estado bueno se carga"), FInventoryModel::ValidateState(Good, Fail));

			// Una roca de -1000 kg en el bolsillo dejaría coger otra de 500 kg.
			FInventoryState Negative = Good;
			Negative.Pockets.Entries[0].Item.WeightKg = -1000.0f;
			TestFalse(TEXT("Peso negativo"), Model.LoadState(Negative, Fail));

			FInventoryState NaNWeight = Good;
			NaNWeight.HandLeft.WeightKg = NaN;
			TestFalse(TEXT("Peso NaN"), Model.LoadState(NaNWeight, Fail));

			FInventoryState InfVolume = Good;
			InfVolume.Pockets.Entries[0].Item.VolumeLiters = Inf;
			TestFalse(TEXT("Volumen infinito"), Model.LoadState(InfVolume, Fail));

			FInventoryState NaNLiquid = Good;
			NaNLiquid.HandLeft.LiquidLiters = NaN;
			TestFalse(TEXT("Líquido NaN"), Model.LoadState(NaNLiquid, Fail));

			FInventoryState Overfull = Good;
			Overfull.HandLeft.LiquidLiters = Overfull.HandLeft.LiquidCapacityLiters + 5.0f;
			TestFalse(TEXT("Más líquido del que cabe"), Model.LoadState(Overfull, Fail));

			FInventoryState NegativeCapacity = Good;
			NegativeCapacity.HandLeft.LiquidCapacityLiters = -1.0f;
			NegativeCapacity.HandLeft.LiquidLiters = -2.0f;
			TestFalse(TEXT("Capacidad de líquido negativa"), Model.LoadState(NegativeCapacity, Fail));

			FInventoryState BadBackpack = Good;
			BadBackpack.Backpack.Spec.MaxWeightKg = NaN;
			TestFalse(TEXT("Mochila a medida con capacidad NaN"), Model.LoadState(BadBackpack, Fail));
			BadBackpack.Backpack.Spec.MaxWeightKg = 10.0f;
			BadBackpack.Backpack.Spec.MaxVolumeLiters = -Inf;
			TestFalse(TEXT("Mochila a medida con volumen -inf"), Model.LoadState(BadBackpack, Fail));

			TestTrue(TEXT("El estado bueno sigue ahí"), Model.GetState() == Good);
			TestTrue(TEXT("El peso sigue siendo finito"), FMath::IsFinite(Model.GetBodyWeightKg()));
		});

		It("rechaza una comodidad de mochila negativa o no finita", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			TestTrue(TEXT("Mochila a medida"), Model.SetCustomBackpack(true, 20.0f, 10.0f, Fail));
			const FInventoryState Good = Model.GetState();

			// -15 kg deja la capacidad cómoda en 0: la proporción de carga sería 0/0.
			FInventoryState Negative = Good;
			Negative.BackpackComfortBonusKg = -FInventoryModel::BaseComfortableKg;
			TestFalse(TEXT("Comodidad negativa"), Model.LoadState(Negative, Fail));
			FInventoryState NaNBonus = Good;
			NaNBonus.BackpackComfortBonusKg = std::numeric_limits<float>::quiet_NaN();
			TestFalse(TEXT("Comodidad NaN"), Model.LoadState(NaNBonus, Fail));

			TestTrue(TEXT("El estado bueno sigue ahí"), Model.GetState() == Good);
			TestTrue(TEXT("Proporción de carga finita"), FMath::IsFinite(Model.GetCarriedWeightRatio()));
			TestTrue(TEXT("Proporción al nadar finita"), FMath::IsFinite(Model.GetSwimLoadRatio()));
		});
	});

	Describe("Gastar materiales", [this]()
	{
		It("quita un objeto de las angarillas o de la mano", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Sledge = Angarillas(Model);
			TestTrue(TEXT("Engancha"), Model.AttachSledge(Sledge, FInventoryContainer(), Fail));
			const FInventoryItem Log = Tronco(Model);
			TestTrue(TEXT("Coge el tronco"), Model.PickUp(Log, Fail));
			TestTrue(TEXT("A las angarillas"), Model.Move(Log.InstanceId, EInventorySlot::Sledge, Fail));
			const FInventoryItem Stone = Piedra(Model);
			TestTrue(TEXT("Piedra en la mano"), Model.PickUp(Stone, Fail));

			FInventoryItem Out;
			TestTrue(TEXT("Gasta el tronco"), Model.ConsumeItem(Log.InstanceId, Out, Fail));
			TestEqual(TEXT("Es el tronco"), Out.InstanceId, Log.InstanceId);
			TestTrue(TEXT("Angarillas vacías"), Model.GetContainer(EInventorySlot::Sledge)->IsEmpty());
			TestTrue(TEXT("Gasta la piedra"), Model.ConsumeItem(Stone.InstanceId, Out, Fail));
			TestTrue(TEXT("Manos vacías"), Model.IsHandEmpty(EInventorySlot::HandLeft) && Model.IsHandEmpty(EInventorySlot::HandRight));
			TestFalse(TEXT("Ya no está"), Model.ConsumeItem(Stone.InstanceId, Out, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::NotFound);
		});

		It("mengua una pila sin moverla y no deja que crezca", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			FInventoryItem Stones = Piedra(Model);
			Stones.WeightKg = 3.0f;
			TestTrue(TEXT("Al bolsillo"), PickAndStore(Model, Stones, EInventorySlot::Pockets, Fail));

			FInventoryItem Fewer = Stones;
			Fewer.WeightKg = 1.0f;
			TestTrue(TEXT("Mengua"), Model.ShrinkItem(Fewer, Fail));
			TestEqual(TEXT("Sigue en el bolsillo"), Model.FindItem(Stones.InstanceId), EInventorySlot::Pockets);
			TestEqual(TEXT("Pesa menos"), Model.FindItemById(Stones.InstanceId)->WeightKg, 1.0f);

			FInventoryItem More = Stones;
			More.WeightKg = 5.0f;
			TestFalse(TEXT("No crece"), Model.ShrinkItem(More, Fail));
			FInventoryItem Other = Fewer;
			Other.DefinitionId = FName(TEXT("coral"));
			TestFalse(TEXT("No cambia de objeto"), Model.ShrinkItem(Other, Fail));
		});

		It("no mengua a cantidades no finitas o negativas", [this]()
		{
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Stones = Piedra(Model);
			TestTrue(TEXT("Al bolsillo"), PickAndStore(Model, Stones, EInventorySlot::Pockets, Fail));

			FInventoryItem NaNWeight = Stones;
			NaNWeight.WeightKg = std::numeric_limits<float>::quiet_NaN();
			TestFalse(TEXT("Peso NaN"), Model.ShrinkItem(NaNWeight, Fail));
			TestTrue(TEXT("Motivo"), Fail == EInventoryFail::InvalidItem);
			FInventoryItem NaNVolume = Stones;
			NaNVolume.VolumeLiters = std::numeric_limits<float>::quiet_NaN();
			TestFalse(TEXT("Volumen NaN"), Model.ShrinkItem(NaNVolume, Fail));
			FInventoryItem Negative = Stones;
			Negative.WeightKg = -1000.0f;
			TestFalse(TEXT("Peso negativo"), Model.ShrinkItem(Negative, Fail));
			TestEqual(TEXT("Pesa lo mismo"), Model.GetBodyWeightKg(), 1.0f);
		});
	});
}

#endif
