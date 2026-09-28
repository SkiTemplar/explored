#include "Misc/AutomationTest.h"

#include "Carry/ContainerReplicationModel.h"
#include "Core/ExploredRandom.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FContainerReplicationModelSpec, "Explored.Net.Containers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FContainerReplicationModelSpec)

namespace ContainerReplicationSpecDetail
{
	FInventoryItem Item(int64 Id, const TCHAR* Def = TEXT("coco_maduro"))
	{
		FInventoryItem I;
		I.InstanceId = Id;
		I.DefinitionId = FName(Def);
		I.WeightKg = 0.6f;
		I.VolumeLiters = 0.6f;
		I.Size = EInventorySize::Pequeno;
		return I;
	}

	FInventoryContainer Chest(const TCHAR* Id, int32 Items)
	{
		FInventoryContainer C;
		C.Id = FName(Id);
		C.Spec = FInventoryContainerSpec::Chest();
		for (int32 i = 0; i < Items; ++i)
		{
			EInventoryFail Fail;
			C.Add(Item(100 + i), Fail);
		}
		return C;
	}

	/** Mensajes que recibe un cliente en concreto. */
	TArray<FContainerMessage> For(const TArray<FContainerMessage>& All, int32 ClientId)
	{
		TArray<FContainerMessage> Out;
		for (const FContainerMessage& M : All)
		{
			if (M.ClientId == ClientId)
			{
				Out.Add(M);
			}
		}
		return Out;
	}
}

void FContainerReplicationModelSpec::Define()
{
	using namespace ContainerReplicationSpecDetail;
	const FName ChestId(TEXT("arcon_base_01"));

	Describe("Cerrado no se replica", [this, ChestId]()
	{
		It("un cofre cerrado no manda nada aunque cambie su contenido", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			Model.MarkSlotChanged(ChestId, 0);
			Model.MarkSlotChanged(ChestId, 3);
			Model.MarkSlotStale(7, ChestId, 1);
			TArray<FContainerMessage> Out;
			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Nada"), Out.Num(), 0);
		});

		It("al abrir llega el contenido entero, luego solo los huecos que cambian, y al cerrar nada", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			FInventoryContainer C = Chest(TEXT("arcon_base_01"), 5);
			TestTrue(TEXT("Abre"), Model.Subscribe(1, ChestId, 120.0));
			Model.MarkSlotChanged(ChestId, 2);
			TArray<FContainerMessage> Out;
			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Un mensaje"), Out.Num(), 1);
			TestTrue(TEXT("Contenido entero (el cambio previo va dentro)"), Out[0].Kind == EContainerMessageKind::Snapshot);
			TestEqual(TEXT("5 × 12 B + 4 B"), FContainerReplicationModel::MessageBytes(Out[0], C), 64);

			Model.MarkSlotChanged(ChestId, 4);
			Model.MarkSlotChanged(ChestId, 1);
			Model.MarkSlotChanged(ChestId, 4);
			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Un mensaje"), Out.Num(), 1);
			TestTrue(TEXT("Solo huecos"), Out[0].Kind == EContainerMessageKind::Update);
			TestTrue(TEXT("Huecos 1 y 4, ordenados y sin repetir"), Out[0].Slots == TArray<int32>({1, 4}));
			TestEqual(TEXT("2 × 12 B + 4 B"), FContainerReplicationModel::MessageBytes(Out[0], C), 28);

			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Sin cambios, sin mensajes"), Out.Num(), 0);

			TestTrue(TEXT("Cierra"), Model.Unsubscribe(1, ChestId));
			Model.MarkSlotChanged(ChestId, 0);
			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Cerrado: nada"), Out.Num(), 0);

			TestTrue(TEXT("Reabre"), Model.Subscribe(1, ChestId, 100.0));
			Model.CollectOutgoing(Out);
			TestTrue(TEXT("Contenido entero otra vez"), Out.Num() == 1 && Out[0].Kind == EContainerMessageKind::Snapshot);
		});

		It("solo abre al alcance de la mano y cierra al alejarse", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			TestFalse(TEXT("A 3,5 m"), Model.Subscribe(1, ChestId, 350.0));
			TestFalse(TEXT("Distancia NaN"), Model.Subscribe(1, ChestId, std::numeric_limits<double>::quiet_NaN()));
			TestFalse(TEXT("Distancia negativa"), Model.Subscribe(1, ChestId, -1.0));
			TestFalse(TEXT("Sin id"), Model.Subscribe(1, NAME_None, 10.0));
			TestFalse(TEXT("Cliente inválido"), Model.Subscribe(INDEX_NONE, ChestId, 10.0));
			TestTrue(TEXT("A 3 m justos"), Model.Subscribe(1, ChestId, 300.0));
			TestFalse(TEXT("Dos veces no"), Model.Subscribe(1, ChestId, 10.0));
			TestTrue(TEXT("Otro cliente sí"), Model.Subscribe(2, ChestId, 10.0));
			TestTrue(TEXT("Y un tercero"), Model.Subscribe(3, ChestId, 10.0));

			TMap<int32, double> Distances;
			Distances.Add(1, 290.0);
			Distances.Add(2, 450.0);
			const TArray<int32> Closed = Model.CloseOutOfReach(ChestId, Distances);
			TestTrue(TEXT("Se cierra el que se aleja y el que no tiene distancia"), Closed == TArray<int32>({2, 3}));
			TestTrue(TEXT("Queda el cercano"), Model.Subscribers(ChestId) == TArray<int32>({1}));
		});

		It("desconectarse o romperse el cofre da de baja", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			const FName Other(TEXT("cesta_02"));
			Model.Subscribe(1, ChestId, 10.0);
			Model.Subscribe(1, Other, 10.0);
			Model.Subscribe(2, ChestId, 10.0);
			Model.DropClient(1);
			TestFalse(TEXT("Cliente 1 fuera del arcón"), Model.IsSubscribed(1, ChestId));
			TestFalse(TEXT("Y de la cesta"), Model.IsSubscribed(1, Other));
			TestTrue(TEXT("El 2 sigue"), Model.IsSubscribed(2, ChestId));
			Model.RemoveContainer(ChestId);
			TestEqual(TEXT("Nadie con el arcón"), Model.Subscribers(ChestId).Num(), 0);
		});
	});

	Describe("Autoridad del servidor", [this, ChestId]()
	{
		It("sin abrirlo no se puede sacar ni meter nada", [this]()
		{
			FContainerReplicationModel Model;
			FInventoryContainer C = Chest(TEXT("arcon_base_01"), 2);
			FInventoryItem Out;
			TestTrue(TEXT("Sacar"), Model.ServerTake(1, C, 0, 100, Out) == EInventoryFail::NotFound);
			TestTrue(TEXT("Meter"), Model.ServerPut(1, C, Item(900)) == EInventoryFail::NotFound);
			TestEqual(TEXT("El cofre no cambia"), C.Num(), 2);
		});

		It("dos jugadores a por el mismo hueco: el segundo recibe NotFound y el hueco refrescado", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			FInventoryContainer C = Chest(TEXT("arcon_base_01"), 3);
			Model.Subscribe(1, ChestId, 50.0);
			Model.Subscribe(2, ChestId, 80.0);
			TArray<FContainerMessage> Out;
			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Dos contenidos enteros"), Out.Num(), 2);

			FInventoryItem Got;
			TestTrue(TEXT("El primero lo saca"), Model.ServerTake(1, C, 1, 101, Got) == EInventoryFail::None);
			TestEqual(TEXT("Es el suyo"), Got.InstanceId, static_cast<int64>(101));
			FInventoryItem Lost;
			TestTrue(TEXT("El segundo llega tarde"), Model.ServerTake(2, C, 1, 101, Lost) == EInventoryFail::NotFound);
			TestEqual(TEXT("El objeto no se duplica"), C.Num(), 2);

			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Un aviso a cada uno"), Out.Num(), 2);
			TestTrue(TEXT("Al 1, el hueco 1"), For(Out, 1).Num() == 1 && For(Out, 1)[0].Slots == TArray<int32>({1}));
			TestTrue(TEXT("Al 2, el hueco 1 (una vez, aunque se marcó dos)"), For(Out, 2).Num() == 1 && For(Out, 2)[0].Slots == TArray<int32>({1}));
		});

		It("un objeto que ya no está en el hueco que ve el cliente es NotFound", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			FInventoryContainer C = Chest(TEXT("arcon_base_01"), 3);
			Model.Subscribe(1, ChestId, 50.0);
			TArray<FContainerMessage> Out;
			Model.CollectOutgoing(Out);
			FInventoryItem Got;
			TestTrue(TEXT("Id que no está en ese hueco"), Model.ServerTake(1, C, 0, 102, Got) == EInventoryFail::NotFound);
			TestTrue(TEXT("Hueco vacío"), Model.ServerTake(1, C, 9, 100, Got) == EInventoryFail::NotFound);
			TestTrue(TEXT("Hueco negativo"), Model.ServerTake(1, C, -1, 100, Got) == EInventoryFail::NotFound);
			TestEqual(TEXT("Nada sale"), C.Num(), 3);
			Model.CollectOutgoing(Out);
			TestTrue(TEXT("Se le refrescan los huecos 0 y 9"), Out.Num() == 1 && Out[0].Slots == TArray<int32>({0, 9}));
		});

		It("meter avisa a los que lo tienen abierto y respeta la capacidad", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			FInventoryContainer C = Chest(TEXT("arcon_base_01"), 11);
			Model.Subscribe(1, ChestId, 50.0);
			Model.Subscribe(2, ChestId, 50.0);
			TArray<FContainerMessage> Out;
			Model.CollectOutgoing(Out);
			TestTrue(TEXT("El hueco 12"), Model.ServerPut(2, C, Item(500)) == EInventoryFail::None);
			TestTrue(TEXT("Lleno"), Model.ServerPut(1, C, Item(501)) != EInventoryFail::None);
			Model.CollectOutgoing(Out);
			TestEqual(TEXT("Aviso a los dos"), Out.Num(), 2);
			TestTrue(TEXT("Hueco 11"), Out[0].Slots == TArray<int32>({11}));
		});

		It("con cuatro jugadores sacando y metiendo al azar nunca se duplica ni se pierde nada", [this, ChestId]()
		{
			FContainerReplicationModel Model;
			FInventoryContainer C = Chest(TEXT("arcon_base_01"), 6);
			for (int32 Client = 1; Client <= 4; ++Client)
			{
				Model.Subscribe(Client, ChestId, 100.0);
			}
			TArray<FInventoryItem> InHands[5];
			FExploredRandom Rng(4242);
			const int32 Total = C.Num();
			for (int32 Step = 0; Step < 5000; ++Step)
			{
				const int32 Client = Rng.RangeInt(1, 4);
				if (Rng.Chance(0.5f) || InHands[Client].Num() == 0)
				{
					// Pide lo que cree ver en un hueco al azar (a veces ya no está).
					const int32 Slot = Rng.RangeInt(0, 11);
					const FInventoryItem* Seen = C.FindBySlotIndex(Slot);
					const int64 Id = (Seen && Rng.Chance(0.8f)) ? Seen->InstanceId : 100 + Rng.RangeInt(0, 5);
					FInventoryItem Got;
					if (Model.ServerTake(Client, C, Slot, Id, Got) == EInventoryFail::None)
					{
						InHands[Client].Add(Got);
					}
				}
				else
				{
					const FInventoryItem Put = InHands[Client].Pop();
					if (Model.ServerPut(Client, C, Put) != EInventoryFail::None)
					{
						InHands[Client].Add(Put);
					}
				}
				int32 Now = C.Num();
				for (int32 i = 1; i <= 4; ++i)
				{
					Now += InHands[i].Num();
				}
				if (Now != Total)
				{
					AddError(FString::Printf(TEXT("Paso %d: hay %d objetos y debería haber %d"), Step, Now, Total));
					return;
				}
			}
			TestTrue(TEXT("Conservación"), true);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
