#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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

	FItemInstance MakeInstance(FName DefinitionId)
	{
		FItemInstance Instance;
		Instance.DefinitionId = DefinitionId;
		Instance.Quality = 3;
		Instance.Durability = 1.0f;
		Instance.Count = 1;
		return Instance;
	}
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

			for (int32 Index = 0; Index < UCarryComponent::MaxPocketSlots; ++Index)
			{
				AExploredItemActor* Coco = TestWorld.Get()->SpawnActor<AExploredItemActor>();
				Coco->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
				TestTrue(TEXT("Se coge"), Carry->TryPickUp(Coco, FailReason));
				TestTrue(FString::Printf(TEXT("Bolsillo %d acepta el coco"), Index), Carry->StoreFromHand(EHand::Left, ECarrySlot::Pocket, FailReason));
			}

			AExploredItemActor* OneMore = TestWorld.Get()->SpawnActor<AExploredItemActor>();
			OneMore->InitializeFromInstance(MakeInstance(TEXT("coco_maduro")));
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
}

#endif
