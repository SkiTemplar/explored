#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

#include "Carry/CarryComponent.h"
#include "Items/ExploredItemActor.h"
#include "Items/ItemRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CarryTest
{
	/**
	 * UWorld temporal para los tests que necesitan mundo (encargo, punto 8):
	 * no toca el mapa abierto en el editor ni arranca un GameInstance real.
	 * UCarryComponent encuentra el registro de objetos por el camino de
	 * repuesto de UItemRegistrySubsystem::Resolve (ver su comentario).
	 */
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), NAME_None);
			WorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
			World->AddToRoot();
			WorldContext->SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World->RemoveFromRoot();
				World = nullptr;
			}
		}

		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
		FWorldContext* WorldContext = nullptr;
	};

	FItemInstance MakeInstance(FName DefinitionId, int32 Count = 1)
	{
		FItemInstance Instance;
		Instance.DefinitionId = DefinitionId;
		Instance.Quality = 3;
		Instance.Durability = 1.0f;
		Instance.Count = Count;
		return Instance;
	}

	/** Deja un objeto en el mundo y lo coge con el componente. */
	bool Pick(UWorld* World, UCarryComponent* Carry, FName DefinitionId, int32 Count = 1)
	{
		AExploredItemActor* Actor = World->SpawnActor<AExploredItemActor>();
		Actor->InitializeFromInstance(MakeInstance(DefinitionId, Count));
		FText FailReason;
		return Carry->TryPickUp(Actor, FailReason);
	}

	UCarryComponent* NewCarry(UWorld* World)
	{
		AActor* Owner = World->SpawnActor<AActor>();
		UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
		Carry->RegisterComponent();
		return Carry;
	}

	/** rama_seca: Pequeno, sin durabilidad ni líquido; apila hasta 10 (biblia 03 §1.3). */
	const TCHAR* const Branch = TEXT("rama_seca");
}

BEGIN_DEFINE_SPEC(FCarrySpec, "Explored.Carry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	TMap<FName, FItemDefinition> Items;
	// UItemRegistrySubsystem hereda de UGameInstanceSubsystem, cuya UCLASS
	// exige Within = GameInstance: NewObject<UItemRegistrySubsystem>() sin ese
	// outer crea el objeto dentro de un Package y dispara un ensure (ClassWithin
	// inválido). Este GameInstance nunca se inicializa (Init()); solo existe
	// para darle al registro un outer del tipo que su reflection exige.
	TStrongObjectPtr<UGameInstance> DummyGameInstance;
	TStrongObjectPtr<UItemRegistrySubsystem> Registry;

END_DEFINE_SPEC(FCarrySpec)

void FCarrySpec::Define()
{
	using namespace CarryTest;

	BeforeEach([this]()
	{
		FString ItemsJson, Error;
		FFileHelper::LoadFileToString(ItemsJson, *UItemRegistrySubsystem::GetDataFilePath(TEXT("items.json")));
		if (!UItemRegistrySubsystem::ParseItemsJson(ItemsJson, Items, Error))
		{
			AddError(FString::Printf(TEXT("items.json: %s"), *Error));
		}

		// SetLoadedDataForTests enlaza este registro en UCraftingLibrary, que
		// es el camino de repuesto que usa UCarryComponent sin GameInstance.
		DummyGameInstance = TStrongObjectPtr<UGameInstance>(NewObject<UGameInstance>());
		Registry = TStrongObjectPtr<UItemRegistrySubsystem>(NewObject<UItemRegistrySubsystem>(DummyGameInstance.Get()));
		Registry->SetLoadedDataForTests(Items, {}, {});
	});

	AfterEach([this]()
	{
		Registry.Reset();
		DummyGameInstance.Reset();
	});

	Describe("Las manos", [this]()
	{
		It("exigen las dos manos vacías para un objeto DosManos", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			// tronco_pequeno es DosManos en items.json (GDD §4.2: tronco, balsa, animal cazado).
			AExploredItemActor* Tronco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			Tronco->InitializeFromInstance(MakeInstance(TEXT("tronco_pequeno")));
			TestEqual(TEXT("El tronco es DosManos"), Tronco->GetEffectiveSize(), EItemSize::DosManos);

			FText FailReason;
			TestTrue(TEXT("Se coge con las dos manos vacías"), Carry->TryPickUp(Tronco, FailReason));
			TestFalse(TEXT("La mano izquierda queda ocupada"), Carry->IsHandEmpty(EHand::Left));
			TestFalse(TEXT("La mano derecha queda ocupada"), Carry->IsHandEmpty(EHand::Right));

			AExploredItemActor* Coco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			Coco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
			TestFalse(TEXT("No se puede coger nada más con las manos llenas"), Carry->TryPickUp(Coco, FailReason));
			TestFalse(TEXT("Da un motivo"), FailReason.IsEmpty());
		});

		It("rechazan un objeto DosManos si una mano ya está ocupada", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			AExploredItemActor* Coco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			Coco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
			FText FailReason;
			TestTrue(TEXT("Coco a la mano libre"), Carry->TryPickUp(Coco, FailReason));

			AExploredItemActor* Tronco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			Tronco->InitializeFromInstance(MakeInstance(TEXT("tronco_pequeno")));
			TestFalse(TEXT("El tronco no se puede coger con una mano ocupada"), Carry->TryPickUp(Tronco, FailReason));
		});

		It("sueltan y vuelven a coger conservando la instancia (calidad, durabilidad, cuenta)", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			FItemInstance Original = MakeInstance(TEXT("cuchillo"));
			Original.Quality = 4;
			Original.Durability = 0.6f;

			AExploredItemActor* ItemActor = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			ItemActor->InitializeFromInstance(Original);

			FText FailReason;
			TestTrue(TEXT("Se coge"), Carry->TryPickUp(ItemActor, FailReason));

			FItemInstance InHand;
			TestTrue(TEXT("Está en la mano izquierda"), Carry->GetHandItem(EHand::Left, InHand));
			TestEqual(TEXT("Misma calidad en la mano"), InHand.Quality, Original.Quality);
			TestEqual(TEXT("Misma durabilidad en la mano"), InHand.Durability, Original.Durability);

			TestTrue(TEXT("Se suelta"), Carry->Drop(EHand::Left, FailReason));
			TestTrue(TEXT("La mano queda vacía"), Carry->IsHandEmpty(EHand::Left));

			// Revisión (huecos de tests de Carry): volver a coger el actor soltado.
			AExploredItemActor* Dropped = nullptr;
			for (TActorIterator<AExploredItemActor> It(TestWorld.Get()); It; ++It)
			{
				if (It->GetItemInstance().DefinitionId == Original.DefinitionId)
				{
					Dropped = *It;
				}
			}
			if (!TestNotNull(TEXT("Hay un actor soltado en el mundo"), Dropped))
			{
				return;
			}
			TestTrue(TEXT("Se vuelve a coger"), Carry->TryPickUp(Dropped, FailReason));
			FItemInstance Again;
			TestTrue(TEXT("Vuelve a la mano"), Carry->GetHandItem(EHand::Left, Again));
			TestEqual(TEXT("Misma calidad tras soltar y coger"), Again.Quality, Original.Quality);
			TestEqual(TEXT("Misma durabilidad tras soltar y coger"), Again.Durability, Original.Durability);
		});

		It("cuentan una sola vez el peso de un DosManos y no lo combinan consigo mismo (H3)", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			AExploredItemActor* Tronco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			Tronco->InitializeFromInstance(MakeInstance(TEXT("tronco_pequeno")));
			FText FailReason;
			TestTrue(TEXT("Se coge"), Carry->TryPickUp(Tronco, FailReason));
			TestTrue(TEXT("Es un único objeto en las dos manos"), Carry->IsHoldingTwoHandedItem());
			TestEqual(TEXT("Pesa una vez"), Carry->GetTotalWeight(), Items[TEXT("tronco_pequeno")].WeightKg);
			EInventoryFail Fail = EInventoryFail::None;
			TestFalse(TEXT("El modelo no deja combinarlo"), Carry->GetInventoryModel().CanCombineHands(Fail));
			TestTrue(TEXT("Porque es el mismo objeto"), Fail == EInventoryFail::SameItem);
		});

		It("sustituyen las piezas por el resultado de fabricar", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			FText FailReason;
			for (const TCHAR* Id : { TEXT("cuchillo"), TEXT("palo_recto") })
			{
				AExploredItemActor* Piece = TestWorld.Get()->SpawnActor<AExploredItemActor>();
				Piece->InitializeFromInstance(MakeInstance(Id));
				TestTrue(TEXT("Pieza a la mano"), Carry->TryPickUp(Piece, FailReason));
			}
			TestTrue(TEXT("El resultado entra"), Carry->ReplaceHandsWithCraftResult(MakeInstance(TEXT("hacha")), FailReason));
			FItemInstance InHand;
			TestTrue(TEXT("En la mano izquierda"), Carry->GetHandItem(EHand::Left, InHand));
			TestTrue(TEXT("Es el hacha"), InHand.DefinitionId == FName(TEXT("hacha")));
			TestTrue(TEXT("La derecha queda libre"), Carry->IsHandEmpty(EHand::Right));
			TestEqual(TEXT("Solo pesa el hacha"), Carry->GetTotalWeight(), Items[TEXT("hacha")].WeightKg);
		});
	});

	Describe("Los bolsillos", [this]()
	{
		It("solo aceptan objetos pequeños", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			AExploredItemActor* Coco = TestWorld.Get()->SpawnActor<AExploredItemActor>(); // Pequeño
			Coco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
			FText FailReason;
			TestTrue(TEXT("Coco a la mano"), Carry->TryPickUp(Coco, FailReason));
			TestTrue(TEXT("Coco al bolsillo"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));

			AExploredItemActor* Hacha = TestWorld.Get()->SpawnActor<AExploredItemActor>(); // Mediano
			Hacha->InitializeFromInstance(MakeInstance(TEXT("hacha")));
			TestTrue(TEXT("Hacha a la mano"), Carry->TryPickUp(Hacha, FailReason));
			TestFalse(TEXT("El hacha no cabe en el bolsillo"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
			TestFalse(TEXT("Da un motivo"), FailReason.IsEmpty());
		});

		It("no aceptan más de cuatro objetos", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();
			FText FailReason;

			// Cuchillos: tienen durabilidad y no apilan, así que cada uno ocupa su hueco
			// (los cocos se juntarían en una sola pila, biblia 03 §1.3).
			for (int32 Index = 0; Index < UCarryComponent::MaxPocketSlots; ++Index)
			{
				AExploredItemActor* Knife = TestWorld.Get()->SpawnActor<AExploredItemActor>();
				Knife->InitializeFromInstance(MakeInstance(TEXT("cuchillo")));
				TestTrue(TEXT("Se coge"), Carry->TryPickUp(Knife, FailReason));
				TestTrue(FString::Printf(TEXT("Bolsillo %d acepta el cuchillo"), Index), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
			}

			AExploredItemActor* OneMore = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			OneMore->InitializeFromInstance(MakeInstance(TEXT("cuchillo")));
			TestTrue(TEXT("Se coge el quinto"), Carry->TryPickUp(OneMore, FailReason));
			TestFalse(TEXT("El quinto bolsillo no existe"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
		});
	});

	Describe("La mochila", [this]()
	{
		It("respeta el volumen y el peso disponibles", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			const float CocoWeight = Items[TEXT("coco_maduro")].WeightKg;
			const float CocoVolume = Items[TEXT("coco_maduro")].VolumeLiters;

			// Solo caben dos cocos por peso, aunque el volumen permitiría más.
			Carry->SetBackpack(true, /*VolumeLiters=*/CocoVolume * 10.0f, /*WeightKg=*/CocoWeight * 2.5f);

			FText FailReason;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				AExploredItemActor* Coco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
				Coco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
				TestTrue(TEXT("Se coge"), Carry->TryPickUp(Coco, FailReason));
				TestTrue(FString::Printf(TEXT("Coco %d entra por peso"), Index), Carry->StoreFromHand(EHand::Left, ECarrySlot::Backpack, FailReason));
			}

			AExploredItemActor* ThirdCoco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			ThirdCoco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
			TestTrue(TEXT("Se coge el tercero"), Carry->TryPickUp(ThirdCoco, FailReason));
			TestFalse(TEXT("El tercer coco no entra por peso"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Backpack, FailReason));
			TestFalse(TEXT("Da un motivo"), FailReason.IsEmpty());
		});

		It("no guarda nada sin mochila", [this]()
		{
			FScopedTestWorld TestWorld;
			AActor* Owner = TestWorld.Get()->SpawnActor<AActor>();
			UCarryComponent* Carry = NewObject<UCarryComponent>(Owner);
			Carry->RegisterComponent();

			AExploredItemActor* Coco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			Coco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
			FText FailReason;
			Carry->TryPickUp(Coco, FailReason);
			TestFalse(TEXT("Sin mochila no se guarda nada"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Backpack, FailReason));
		});
	});

	Describe("Las pilas (biblia 03 §1.3)", [this]()
	{
		It("juntan en una mano lo que se recoge del mismo tipo", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestTrue(TEXT("Se coge una rama"), Pick(TestWorld.Get(), Carry, Branch));
			}
			FItemInstance InHand;
			TestTrue(TEXT("En la mano izquierda"), Carry->GetHandItem(EHand::Left, InHand));
			TestEqual(TEXT("Tres ramas en una pila"), InHand.Count, 3);
			TestTrue(TEXT("La derecha sigue libre"), Carry->IsHandEmpty(EHand::Right));
			TestEqual(TEXT("Pesan las tres"), Carry->GetTotalWeight(), Items[Branch].WeightKg * 3.0f, 1.0e-4f);
		});

		It("completan la pila del bolsillo al guardar y abren otra al llenarse", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			FText FailReason;
			TestTrue(TEXT("Siete ramas"), Pick(TestWorld.Get(), Carry, Branch, 7));
			TestTrue(TEXT("Al bolsillo"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
			TestTrue(TEXT("Cinco más"), Pick(TestWorld.Get(), Carry, Branch, 5));
			TestTrue(TEXT("Al bolsillo también"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
			const TArray<FItemInstance>& Pockets = Carry->GetPocketItems();
			if (TestEqual(TEXT("Dos huecos: una pila llena y el resto"), Pockets.Num(), 2))
			{
				TestEqual(TEXT("La primera llega a 10"), Pockets[0].Count, 10);
				TestEqual(TEXT("La segunda lleva 2"), Pockets[1].Count, 2);
			}
			TestTrue(TEXT("La mano queda libre"), Carry->IsHandEmpty(EHand::Left));
			TMap<FName, int32> Counts;
			TSet<FName> Tools;
			Carry->CountMaterials(Counts, Tools);
			TestEqual(TEXT("Doce ramas para construir"), Counts.FindRef(FName(Branch)), 12);
		});

		It("se parten entre las dos manos y se vuelven a juntar", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			FText FailReason;
			TestTrue(TEXT("Seis ramas"), Pick(TestWorld.Get(), Carry, Branch, 6));
			TestTrue(TEXT("Dos a la derecha"), Carry->SplitFromHand(EHand::Left, 2, FailReason));
			FItemInstance Left, Right;
			Carry->GetHandItem(EHand::Left, Left);
			Carry->GetHandItem(EHand::Right, Right);
			TestEqual(TEXT("Cuatro en la izquierda"), Left.Count, 4);
			TestEqual(TEXT("Dos en la derecha"), Right.Count, 2);
			TestTrue(TEXT("Misma calidad en la parte nueva"), Right.Quality == Left.Quality);
			TestFalse(TEXT("No se parten las seis"), Carry->SplitFromHand(EHand::Left, 4, FailReason));
			TestFalse(TEXT("Da un motivo"), FailReason.IsEmpty());

			TestTrue(TEXT("Se juntan"), Carry->MergeHands(FailReason));
			Carry->GetHandItem(EHand::Left, Left);
			TestEqual(TEXT("Seis otra vez"), Left.Count, 6);
			TestTrue(TEXT("La derecha queda libre"), Carry->IsHandEmpty(EHand::Right));

			TestTrue(TEXT("Un cuchillo a la derecha"), Pick(TestWorld.Get(), Carry, TEXT("cuchillo")));
			TestFalse(TEXT("Ramas y cuchillo no se juntan"), Carry->MergeHands(FailReason));
		});

		It("gastan unidades sin mover la pila de su hueco", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			FText FailReason;
			TestTrue(TEXT("Cinco ramas"), Pick(TestWorld.Get(), Carry, Branch, 5));
			FItemInstance Taken;
			TestTrue(TEXT("Una al fuego"), Carry->TakeOneFromHand(EHand::Left, Taken));
			TestEqual(TEXT("Sale una"), Taken.Count, 1);
			FItemInstance Left;
			Carry->GetHandItem(EHand::Left, Left);
			TestEqual(TEXT("Quedan cuatro"), Left.Count, 4);
			TestTrue(TEXT("Al bolsillo"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
			TestTrue(TEXT("Se gastan tres para construir"), Carry->ConsumeMaterials({ FBuildingCost{ FName(Branch), 3 } }));
			if (TestEqual(TEXT("La pila sigue en el bolsillo"), Carry->GetPocketItems().Num(), 1))
			{
				TestEqual(TEXT("Con una"), Carry->GetPocketItems()[0].Count, 1);
			}
			TestFalse(TEXT("No hay dos más"), Carry->ConsumeMaterials({ FBuildingCost{ FName(Branch), 2 } }));
		});

		It("llenan la mochila hasta su peso y dejan el resto en la mano", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			const float BranchKg = Items[Branch].WeightKg;
			Carry->SetBackpack(true, /*VolumeLiters=*/100.0f, /*WeightKg=*/BranchKg * 2.5f);
			FText FailReason;
			TestTrue(TEXT("Cuatro ramas"), Pick(TestWorld.Get(), Carry, Branch, 4));
			TestTrue(TEXT("Guarda lo que cabe"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Backpack, FailReason));
			if (TestEqual(TEXT("Una pila en la mochila"), Carry->GetBackpackItems().Num(), 1))
			{
				TestEqual(TEXT("Con dos ramas"), Carry->GetBackpackItems()[0].Count, 2);
			}
			FItemInstance Left;
			TestTrue(TEXT("El resto sigue en la mano"), Carry->GetHandItem(EHand::Left, Left));
			TestEqual(TEXT("Dos"), Left.Count, 2);
			TestFalse(TEXT("Ya no cabe ninguna"), Carry->StoreFromHand(EHand::Left, ECarrySlot::Backpack, FailReason));
			TestFalse(TEXT("Da un motivo"), FailReason.IsEmpty());
		});

		It("se sueltan enteras y se recogen con su cuenta", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			FText FailReason;
			TestTrue(TEXT("Ocho ramas"), Pick(TestWorld.Get(), Carry, Branch, 8));
			TestTrue(TEXT("Se sueltan"), Carry->Drop(EHand::Left, FailReason));
			AExploredItemActor* Dropped = nullptr;
			for (TActorIterator<AExploredItemActor> It(TestWorld.Get()); It; ++It)
			{
				Dropped = *It;
			}
			if (!TestNotNull(TEXT("Hay un actor en el suelo"), Dropped))
			{
				return;
			}
			TestEqual(TEXT("Con las ocho"), Dropped->GetItemInstance().Count, 8);
			TestTrue(TEXT("Se recogen"), Carry->TryPickUp(Dropped, FailReason));
			FItemInstance Left;
			Carry->GetHandItem(EHand::Left, Left);
			TestEqual(TEXT("Ocho en la mano"), Left.Count, 8);
		});

		It("al cargar una partida quitan lo que ya no existe y pasan a unidades las pilas antiguas", [this]()
		{
			FScopedTestWorld TestWorld;
			UCarryComponent* Carry = NewCarry(TestWorld.Get());
			TestTrue(TEXT("Tres ramas"), Pick(TestWorld.Get(), Carry, Branch, 3));
			FInventoryState State;
			TMap<int64, FItemInstance> Instances;
			Carry->ExportState(State, Instances);
			TestEqual(TEXT("El guardado lleva la cuenta al día"), Instances.FindRef(State.HandLeft.InstanceId).Count, 3);

			// Un objeto retirado de items.json en el bolsillo.
			FInventoryEntry Retired;
			Retired.Item.InstanceId = State.NextInstanceId++;
			Retired.Item.DefinitionId = FName(TEXT("objeto_retirado"));
			Retired.Item.WeightKg = 0.1f;
			State.Pockets.Entries.Add(Retired);
			Instances.Add(Retired.Item.InstanceId, MakeInstance(TEXT("objeto_retirado")));
			// Una pila de antes de las pilas: el registro pesa las cinco y dice Count 1.
			FInventoryEntry Legacy;
			Legacy.Item.InstanceId = State.NextInstanceId++;
			Legacy.Item.DefinitionId = FName(Branch);
			Legacy.Item.WeightKg = Items[Branch].WeightKg * 5.0f;
			Legacy.Item.VolumeLiters = Items[Branch].VolumeLiters * 5.0f;
			Legacy.Item.Tags = Items[Branch].Tags.Array();
			Legacy.SlotIndex = 1;
			State.Pockets.Entries.Add(Legacy);
			Instances.Add(Legacy.Item.InstanceId, MakeInstance(Branch, 5));

			UCarryComponent* Loaded = NewCarry(TestWorld.Get());
			TestTrue(TEXT("La partida carga"), Loaded->ImportState(State, Instances));
			const TArray<FItemInstance>& Pockets = Loaded->GetPocketItems();
			if (TestEqual(TEXT("Solo queda la pila antigua en los bolsillos"), Pockets.Num(), 1))
			{
				TestEqual(TEXT("Con sus cinco ramas"), Pockets[0].Count, 5);
			}
			TestNull(TEXT("Lo retirado ya no está"), Loaded->FindInstance(Retired.Item.InstanceId));
			TestEqual(TEXT("El peso es el de 3 + 5 ramas"), Loaded->GetTotalWeight(), Items[Branch].WeightKg * 8.0f, 1.0e-3f);
		});
	});
}

#endif
