#include "Misc/AutomationTest.h"

#include <limits>

#include "Carry/InventoryNetModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace InventoryNetTest
{
	FContentIdTableModel MakeTable()
	{
		FContentIdTableModel Table;
		FString Error;
		TArray<FName> Ids;
		for (const TCHAR* Id : { TEXT("rama_seca"), TEXT("basalto"), TEXT("cantimplora"), TEXT("tronco_pequeno"), TEXT("mochila"), TEXT("cuchillo") })
		{
			Ids.Add(FName(Id));
		}
		Table.SetIds(EContentKind::Item, Ids, Error);
		return Table;
	}

	FInventoryItem Make(FInventoryModel& Model, const TCHAR* Id, float WeightKg, EInventorySize Size, TArray<FName> Tags, int32 Count = 1)
	{
		FInventoryItem Item;
		Item.InstanceId = Model.AllocateInstanceId();
		Item.DefinitionId = FName(Id);
		Item.WeightKg = WeightKg;
		Item.VolumeLiters = 0.4f;
		Item.Size = Size;
		Item.Tags = MoveTemp(Tags);
		Item.MaxStack = FInventoryModel::ComputeMaxStack(0.0f, 0.0f, Size, Item.Tags, false);
		Item.Count = Count;
		return Item;
	}

	FInventoryItem Piedras(FInventoryModel& M, int32 Count) { return Make(M, TEXT("basalto"), 1.0f, EInventorySize::Pequeno, { FName(TEXT("piedra")) }, Count); }
	FInventoryItem Ramas(FInventoryModel& M, int32 Count) { return Make(M, TEXT("rama_seca"), 0.3f, EInventorySize::Pequeno, { FName(TEXT("madera")) }, Count); }

	TArray<FInventoryNetEntry> Snapshot(const FInventoryModel& Model, const FContentIdTableModel& Table)
	{
		TArray<FInventoryNetEntry> Entries;
		TArray<FInventoryNetSkip> Skipped;
		FInventoryNetModel::BuildSnapshot(Model.GetState(), Table, {}, Entries, Skipped);
		return Entries;
	}

	const FInventoryNetEntry* FindEntry(const TArray<FInventoryNetEntry>& Entries, int64 InstanceId)
	{
		return Entries.FindByPredicate([InstanceId](const FInventoryNetEntry& E) { return E.InstanceId == static_cast<uint32>(InstanceId); });
	}

	struct FLcg
	{
		uint32 State;
		uint32 Next() { State = State * 1664525u + 1013904223u; return State >> 8; }
		int32 Range(int32 Max) { return static_cast<int32>(Next() % static_cast<uint32>(Max)); }
	};
}

BEGIN_DEFINE_SPEC(FInventoryNetModelSpec, "Explored.Inventory.Net",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FInventoryNetModelSpec)

void FInventoryNetModelSpec::Define()
{
	using namespace InventoryNetTest;

	Describe("La entrada", [this]()
	{
		It("ocupa 12 bytes en el orden de la biblia y vuelve igual", [this]()
		{
			FInventoryNetEntry Entry;
			Entry.Slot = static_cast<uint8>(EInventoryNetSlot::Pockets);
			Entry.SlotIndex = 3;
			Entry.DefinitionId = 0x0102;
			Entry.InstanceId = 0x0A0B0C0D;
			Entry.Quality01x255 = 128;
			Entry.Durability01x255 = 200;
			Entry.Count = 7;
			Entry.Flags = InventoryNetFlags::Wet;
			TArray<uint8> Bytes;
			FInventoryNetModel::EncodeEntry(Entry, Bytes);
			TestEqual(TEXT("12 bytes"), Bytes.Num(), FInventoryNetModel::EntryBytes);
			TestEqual(TEXT("Hueco"), (int32)Bytes[0], 3);
			TestEqual(TEXT("Hueco visible"), (int32)Bytes[1], 3);
			TestEqual(TEXT("Definición, byte bajo primero"), (int32)Bytes[2], 0x02);
			TestEqual(TEXT("Id de instancia, byte bajo primero"), (int32)Bytes[4], 0x0D);
			TestEqual(TEXT("Id de instancia, byte alto al final"), (int32)Bytes[7], 0x0A);
			TestEqual(TEXT("Cuenta"), (int32)Bytes[10], 7);
			FInventoryNetEntry Back;
			TestTrue(TEXT("Se decodifica"), FInventoryNetModel::DecodeEntry(Bytes.GetData(), Bytes.Num(), Back));
			TestTrue(TEXT("Igual"), Back == Entry);
		});

		It("cuantiza calidad, durabilidad y líquido sin sorpresas en los bordes", [this]()
		{
			for (uint8 Quality = 1; Quality <= 5; ++Quality)
			{
				TestEqual(FString::Printf(TEXT("Calidad %d ida y vuelta"), Quality),
					(int32)FInventoryNetModel::ByteToQuality(FInventoryNetModel::QualityToByte(Quality)), (int32)Quality);
			}
			TestEqual(TEXT("Calidad 1 → 0"), (int32)FInventoryNetModel::QualityToByte(1), 0);
			TestEqual(TEXT("Calidad 5 → 255"), (int32)FInventoryNetModel::QualityToByte(5), 255);
			TestEqual(TEXT("Calidad 0 se trata como 1"), (int32)FInventoryNetModel::QualityToByte(0), 0);
			TestEqual(TEXT("Calidad 9 se trata como 5"), (int32)FInventoryNetModel::QualityToByte(9), 255);
			TestEqual(TEXT("Durabilidad llena"), (int32)FInventoryNetModel::Durability01ToByte(1.0f), 255);
			TestEqual(TEXT("Durabilidad media"), (int32)FInventoryNetModel::Durability01ToByte(0.5f), 128);
			TestEqual(TEXT("Durabilidad de más"), (int32)FInventoryNetModel::Durability01ToByte(3.0f), 255);
			TestEqual(TEXT("Durabilidad negativa"), (int32)FInventoryNetModel::Durability01ToByte(-1.0f), 0);
			TestEqual(TEXT("Durabilidad NaN: rota, no nueva"), (int32)FInventoryNetModel::Durability01ToByte(std::numeric_limits<float>::quiet_NaN()), 0);
			TestEqual(TEXT("1 l de agua"), (int32)FInventoryNetModel::LitersToCentiliters(1.0f), 100);
			TestEqual(TEXT("Tope de un byte"), (int32)FInventoryNetModel::LitersToCentiliters(40.0f), 255);
			TestEqual(TEXT("Líquido NaN"), (int32)FInventoryNetModel::LitersToCentiliters(std::numeric_limits<float>::quiet_NaN()), 0);
		});

		It("rechaza bytes corruptos", [this]()
		{
			FInventoryNetEntry Good;
			Good.Slot = static_cast<uint8>(EInventoryNetSlot::Backpack);
			Good.DefinitionId = 1;
			Good.InstanceId = 5;
			Good.Count = 3;
			TArray<uint8> Bytes;
			FInventoryNetModel::EncodeEntry(Good, Bytes);
			FInventoryNetEntry Out;
			TestFalse(TEXT("Truncada"), FInventoryNetModel::DecodeEntry(Bytes.GetData(), 11, Out));
			TestFalse(TEXT("Nula"), FInventoryNetModel::DecodeEntry(nullptr, 12, Out));

			auto Broken = [this, &Bytes](const TCHAR* What, int32 Offset, uint8 Value)
			{
				TArray<uint8> Copy = Bytes;
				Copy[Offset] = Value;
				FInventoryNetEntry Ignored;
				TestFalse(What, FInventoryNetModel::DecodeEntry(Copy.GetData(), Copy.Num(), Ignored));
			};
			Broken(TEXT("Hueco 0"), 0, 0);
			Broken(TEXT("Hueco que no existe"), 0, static_cast<uint8>(EInventoryNetSlot::Count));
			Broken(TEXT("Cuenta 0"), 10, 0);
			Broken(TEXT("Cuenta 11"), 10, 11);
			Broken(TEXT("Bit de estado desconocido"), 11, 0x80);
			Broken(TEXT("DosManos fuera de la mano izquierda"), 11, InventoryNetFlags::TwoHanded);
			TArray<uint8> NoDefinition = Bytes;
			NoDefinition[2] = 0xFF;
			NoDefinition[3] = 0xFF;
			TestFalse(TEXT("Definición «ninguna»"), FInventoryNetModel::DecodeEntry(NoDefinition.GetData(), 12, Out));
			TArray<uint8> NoId = Bytes;
			NoId[4] = NoId[5] = NoId[6] = NoId[7] = 0;
			TestFalse(TEXT("Id 0"), FInventoryNetModel::DecodeEntry(NoId.GetData(), 12, Out));
		});
	});

	Describe("La foto del inventario", [this]()
	{
		It("manda una entrada por hueco, una sola por un DosManos, y el equipo puesto", [this]()
		{
			const FContentIdTableModel Table = MakeTable();
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Backpack = Make(Model, TEXT("mochila"), 0.8f, EInventorySize::Grande, { FName(TEXT("mochila")) });
			Model.PickUp(Backpack, Fail);
			Model.EquipFromHand(EInventorySlot::HandLeft, Fail);
			const FInventoryItem Stones = Piedras(Model, 7);
			Model.PickUp(Stones, Fail);
			Model.Move(Stones.InstanceId, EInventorySlot::Pockets, Fail);
			FInventoryItem Canteen = Make(Model, TEXT("cantimplora"), 0.3f, EInventorySize::Pequeno, { FName(TEXT("cantimplora")) });
			Canteen.LiquidCapacityLiters = 1.0f;
			Canteen.LiquidLiters = 0.75f;
			Canteen.MaxStack = 1;
			Model.PickUp(Canteen, Fail);
			Model.Move(Canteen.InstanceId, EInventorySlot::Backpack, Fail);
			const FInventoryItem Log = Make(Model, TEXT("tronco_pequeno"), 8.0f, EInventorySize::DosManos, { FName(TEXT("madera")) });
			Model.PickUp(Log, Fail);

			TArray<FInventoryNetEntry> Entries;
			TArray<FInventoryNetSkip> Skipped;
			TMap<int64, FInventoryNetExtras> Extras;
			FInventoryNetExtras CanteenExtras;
			CanteenExtras.Durability01 = 0.5f;
			CanteenExtras.Flags = static_cast<uint8>(InventoryNetFlags::Wet | InventoryNetFlags::TwoHanded);
			Extras.Add(Canteen.InstanceId, CanteenExtras);
			FInventoryNetModel::BuildSnapshot(Model.GetState(), Table, Extras, Entries, Skipped);
			TestEqual(TEXT("Cuatro entradas: tronco, piedras, cantimplora y mochila"), Entries.Num(), 4);
			TestEqual(TEXT("Nada se queda fuera"), Skipped.Num(), 0);
			for (int32 I = 1; I < Entries.Num(); ++I)
			{
				TestTrue(TEXT("Ordenadas por id"), Entries[I - 1].InstanceId < Entries[I].InstanceId);
			}

			const FInventoryNetEntry* LogEntry = FindEntry(Entries, Log.InstanceId);
			if (TestNotNull(TEXT("Tronco"), LogEntry))
			{
				TestEqual(TEXT("En la mano izquierda"), (int32)LogEntry->Slot, (int32)EInventoryNetSlot::HandLeft);
				TestTrue(TEXT("Marcado DosManos"), (LogEntry->Flags & InventoryNetFlags::TwoHanded) != 0);
			}
			const FInventoryNetEntry* StoneEntry = FindEntry(Entries, Stones.InstanceId);
			if (TestNotNull(TEXT("Piedras"), StoneEntry))
			{
				TestEqual(TEXT("Siete"), (int32)StoneEntry->Count, 7);
				TestTrue(TEXT("Su definición"), Table.FromNetId(EContentKind::Item, StoneEntry->DefinitionId) == FName(TEXT("basalto")));
			}
			const FInventoryNetEntry* CanteenEntry = FindEntry(Entries, Canteen.InstanceId);
			if (TestNotNull(TEXT("Cantimplora"), CanteenEntry))
			{
				TestEqual(TEXT("75 cl en la cuenta"), (int32)CanteenEntry->Count, 75);
				TestTrue(TEXT("Marcada como líquido"), (CanteenEntry->Flags & InventoryNetFlags::LiquidInCount) != 0);
				TestTrue(TEXT("Mojada, de los extras"), (CanteenEntry->Flags & InventoryNetFlags::Wet) != 0);
				TestTrue(TEXT("Los extras no cuelan el bit DosManos"), (CanteenEntry->Flags & InventoryNetFlags::TwoHanded) == 0);
				TestEqual(TEXT("Media durabilidad"), (int32)CanteenEntry->Durability01x255, 128);
			}
			const FInventoryNetEntry* BackpackEntry = FindEntry(Entries, Backpack.InstanceId);
			if (TestNotNull(TEXT("Mochila puesta"), BackpackEntry))
			{
				TestEqual(TEXT("Como equipo"), (int32)BackpackEntry->Slot, (int32)EInventoryNetSlot::EquippedBackpack);
			}

			const FInventoryNetHands Hands = FInventoryNetModel::BuildHands(Model.GetState(), Table);
			TestEqual(TEXT("El tronco en las dos manos"), (int32)Hands.LeftDefinitionId, (int32)Table.ToNetId(EContentKind::Item, TEXT("tronco_pequeno")));
			TestEqual(TEXT("Misma definición a la derecha"), (int32)Hands.RightDefinitionId, (int32)Hands.LeftDefinitionId);
			TArray<uint8> HandBytes;
			FInventoryNetModel::EncodeHands(Hands, HandBytes);
			TestEqual(TEXT("Las manos son 6 bytes"), HandBytes.Num(), FInventoryNetModel::HandsBytes);
			FInventoryNetHands HandsBack;
			TestTrue(TEXT("Vuelven"), FInventoryNetModel::DecodeHands(HandBytes.GetData(), HandBytes.Num(), HandsBack) && HandsBack == Hands);
			TestFalse(TEXT("Manos truncadas"), FInventoryNetModel::DecodeHands(HandBytes.GetData(), 5, HandsBack));
		});

		It("deja fuera, y lo dice, lo que no tiene número de red", [this]()
		{
			const FContentIdTableModel Table = MakeTable();
			FInventoryState State;
			State.HandLeft.InstanceId = 3;
			State.HandLeft.DefinitionId = FName(TEXT("objeto_retirado"));
			State.HandRight.InstanceId = static_cast<int64>(MAX_uint32) + 1;
			State.HandRight.DefinitionId = FName(TEXT("basalto"));
			TArray<FInventoryNetEntry> Entries;
			TArray<FInventoryNetSkip> Skipped;
			FInventoryNetModel::BuildSnapshot(State, Table, {}, Entries, Skipped);
			TestEqual(TEXT("Nada se manda"), Entries.Num(), 0);
			TestEqual(TEXT("Dos avisos"), Skipped.Num(), 2);
			const FInventoryNetHands Hands = FInventoryNetModel::BuildHands(FInventoryState(), Table);
			TestEqual(TEXT("Mano vacía"), (int32)Hands.LeftDefinitionId, (int32)FContentIdTableModel::InvalidNetId);
		});

		It("con 24 huecos ocupados pesa 288 bytes al unirse", [this]()
		{
			TArray<FInventoryNetEntry> Full;
			for (uint32 Id = 1; Id <= 24; ++Id)
			{
				FInventoryNetEntry Entry;
				Entry.Slot = static_cast<uint8>(EInventoryNetSlot::Backpack);
				Entry.SlotIndex = static_cast<uint8>(Id);
				Entry.DefinitionId = 0;
				Entry.InstanceId = Id;
				Full.Add(Entry);
			}
			FInventoryNetCoalescer Coalescer;
			const FInventoryNetDelta Join = Coalescer.MakeFullSnapshot(Full);
			TestEqual(TEXT("24 × 12"), Join.GetPayloadBytes(), 288);
		});
	});

	Describe("Lo que viaja en cada cambio", [this]()
	{
		It("partir una pila manda dos entradas; guardarla, una; fundirla del todo, una y un borrado", [this]()
		{
			const FContentIdTableModel Table = MakeTable();
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			const FInventoryItem Stones = Piedras(Model, 8);
			Model.PickUp(Stones, Fail);
			TArray<FInventoryNetEntry> Before = Snapshot(Model, Table);

			int64 NewId = 0;
			Model.SplitStack(Stones.InstanceId, 3, EInventorySlot::HandRight, NewId, Fail);
			TArray<FInventoryNetEntry> After = Snapshot(Model, Table);
			FInventoryNetDelta Delta = FInventoryNetModel::Diff(Before, After);
			TestEqual(TEXT("Partir: dos entradas"), Delta.Changed.Num(), 2);
			TestEqual(TEXT("24 bytes"), Delta.GetPayloadBytes(), 24);

			Before = After;
			Model.Move(NewId, EInventorySlot::Pockets, Fail);
			After = Snapshot(Model, Table);
			Delta = FInventoryNetModel::Diff(Before, After);
			TestEqual(TEXT("Guardar: una entrada (cambia de hueco)"), Delta.Changed.Num(), 1);
			TestEqual(TEXT("Sin borrados"), Delta.Removed.Num(), 0);

			Before = After;
			int32 Moved = 0;
			Model.MergeStacks(NewId, Stones.InstanceId, Moved, Fail);
			After = Snapshot(Model, Table);
			Delta = FInventoryNetModel::Diff(Before, After);
			TestEqual(TEXT("Fundir: la pila que crece"), Delta.Changed.Num(), 1);
			TestEqual(TEXT("y la que desaparece"), Delta.Removed.Num(), 1);
			TestEqual(TEXT("16 bytes"), Delta.GetPayloadBytes(), 16);

			TestTrue(TEXT("En reposo no se manda nada"), FInventoryNetModel::Diff(After, Snapshot(Model, Table)).IsEmpty());
		});

		It("reconstruye en el cliente el mismo inventario tras miles de cambios al azar, pasando por bytes", [this]()
		{
			const FContentIdTableModel Table = MakeTable();
			FInventoryModel Model;
			EInventoryFail Fail = EInventoryFail::None;
			Model.SetCustomBackpack(true, 8.0f, 8.0f, Fail);
			TArray<FInventoryNetEntry> Server = Snapshot(Model, Table);
			TArray<FInventoryNetEntry> Client;
			FInventoryNetCoalescer Coalescer;
			FLcg Rng{ 20260928u };
			int32 Sent = 0;
			const EInventorySlot Slots[] = { EInventorySlot::HandLeft, EInventorySlot::HandRight, EInventorySlot::Pockets, EInventorySlot::Backpack };
			for (int32 Step = 0; Step < 2000; ++Step)
			{
				TArray<int64> Ids;
				for (const FInventoryNetEntry& E : Server)
				{
					Ids.Add(E.InstanceId);
				}
				const int64 AnyId = Ids.Num() > 0 ? Ids[Rng.Range(Ids.Num())] : 0;
				const int64 OtherId = Ids.Num() > 0 ? Ids[Rng.Range(Ids.Num())] : 0;
				switch (Rng.Range(5))
				{
				case 0:
				{
					int64 MergedInto = 0;
					Model.PickUpMerging(Rng.Range(2) ? Piedras(Model, 1 + Rng.Range(3)) : Ramas(Model, 1 + Rng.Range(3)), MergedInto, Fail);
					break;
				}
				case 1:
				{
					int64 NewId = 0;
					Model.SplitStack(AnyId, 1 + Rng.Range(5), Slots[Rng.Range(4)], NewId, Fail);
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
				default:
					Model.RemoveUnits(AnyId, 1, Fail);
					break;
				}
				Server = Snapshot(Model, Table);

				FInventoryNetDelta Delta;
				if (Coalescer.Tick(1.0f / 30.0f, Server, Delta))
				{
					++Sent;
					FInventoryNetDelta Received;
					if (!TestTrue(TEXT("El paquete se decodifica"), FInventoryNetModel::DecodeDelta(FInventoryNetModel::EncodeDelta(Delta), Received)))
					{
						return;
					}
					if (!TestTrue(FString::Printf(TEXT("El cliente aplica el paso %d"), Step), FInventoryNetModel::ApplyDelta(Client, Received)))
					{
						return;
					}
					if (!TestTrue(FString::Printf(TEXT("Cliente igual a lo enviado en el paso %d"), Step), Client == Coalescer.GetLastSent()))
					{
						return;
					}
				}
			}
			// A 30 fps durante 2000 fotogramas (66 s) caben como mucho 667 envíos a 10 Hz.
			TestTrue(TEXT("Coalescido a 10 Hz"), Sent > 0 && Sent <= 667);
			FInventoryNetDelta Last;
			Coalescer.Tick(1.0f, Server, Last);
			FInventoryNetModel::ApplyDelta(Client, Last);
			TestTrue(TEXT("Al final el cliente ve lo mismo que el servidor"), Client == Server);
		});
	});

	Describe("Los paquetes", [this]()
	{
		It("se rechazan truncados, con bytes de más o con borrados imposibles, sin tocar al cliente", [this]()
		{
			FInventoryNetDelta Delta;
			FInventoryNetEntry Entry;
			Entry.Slot = static_cast<uint8>(EInventoryNetSlot::Pockets);
			Entry.DefinitionId = 1;
			Entry.InstanceId = 9;
			Delta.Changed.Add(Entry);
			Delta.Removed.Add(4);
			const TArray<uint8> Bytes = FInventoryNetModel::EncodeDelta(Delta);
			TestEqual(TEXT("Cabecera + 12 + 4"), Bytes.Num(), 4 + 12 + 4);
			FInventoryNetDelta Out;
			TestTrue(TEXT("Correcto"), FInventoryNetModel::DecodeDelta(Bytes, Out));
			TArray<uint8> Short = Bytes;
			Short.Pop();
			TestFalse(TEXT("Truncado"), FInventoryNetModel::DecodeDelta(Short, Out));
			TArray<uint8> Long = Bytes;
			Long.Add(0);
			TestFalse(TEXT("Un byte de más"), FInventoryNetModel::DecodeDelta(Long, Out));
			TestFalse(TEXT("Vacío"), FInventoryNetModel::DecodeDelta(TArray<uint8>(), Out));
			TArray<uint8> Lying = Bytes;
			Lying[0] = 200;
			TestFalse(TEXT("Cabecera que miente"), FInventoryNetModel::DecodeDelta(Lying, Out));

			TArray<FInventoryNetEntry> Client;
			FInventoryNetEntry Existing = Entry;
			Existing.InstanceId = 5;
			Client.Add(Existing);
			const TArray<FInventoryNetEntry> ClientBefore = Client;
			TestFalse(TEXT("Borra un id que el cliente no tiene"), FInventoryNetModel::ApplyDelta(Client, Delta));
			TestTrue(TEXT("Cliente intacto"), Client == ClientBefore);
			FInventoryNetDelta Both;
			Both.Changed.Add(Existing);
			Both.Removed.Add(5);
			TestFalse(TEXT("Cambia y borra el mismo id"), FInventoryNetModel::ApplyDelta(Client, Both));
			FInventoryNetDelta Twice;
			Twice.Changed = { Entry, Entry };
			TestFalse(TEXT("La misma entrada dos veces"), FInventoryNetModel::ApplyDelta(Client, Twice));
			TestTrue(TEXT("Cliente intacto"), Client == ClientBefore);
		});
	});

	Describe("La coalescencia a 10 Hz", [this]()
	{
		It("no manda nada en reposo ni lo que aparece y desaparece en la misma ventana", [this]()
		{
			FInventoryNetCoalescer Coalescer;
			const TArray<FInventoryNetEntry> Empty;
			FInventoryNetDelta Delta;
			int32 Sends = 0;
			for (int32 Frame = 0; Frame < 600; ++Frame)
			{
				Sends += Coalescer.Tick(1.0f / 60.0f, Empty, Delta) ? 1 : 0;
			}
			TestEqual(TEXT("10 s en reposo: nada"), Sends, 0);

			FInventoryNetEntry Flash;
			Flash.Slot = static_cast<uint8>(EInventoryNetSlot::HandLeft);
			Flash.DefinitionId = 0;
			Flash.InstanceId = 77;
			TestFalse(TEXT("Aparece a mitad de ventana"), Coalescer.Tick(0.03f, { Flash }, Delta));
			TestFalse(TEXT("Desaparece antes de enviar"), Coalescer.Tick(0.03f, Empty, Delta));
			TestFalse(TEXT("Al cerrar la ventana no hay nada que mandar"), Coalescer.Tick(0.05f, Empty, Delta));
		});

		It("con un cambio en cada fotograma manda como mucho diez veces por segundo", [this]()
		{
			FInventoryNetCoalescer Coalescer;
			FInventoryNetDelta Delta;
			int32 Sends = 0;
			FInventoryNetEntry Entry;
			Entry.Slot = static_cast<uint8>(EInventoryNetSlot::Pockets);
			Entry.DefinitionId = 0;
			Entry.InstanceId = 1;
			for (int32 Frame = 0; Frame < 144; ++Frame)
			{
				Entry.Count = static_cast<uint8>(1 + Frame % 10);
				Sends += Coalescer.Tick(1.0f / 144.0f, { Entry }, Delta) ? 1 : 0;
			}
			TestTrue(TEXT("Un segundo a 144 fps: 9 o 10 envíos"), Sends >= 9 && Sends <= 10);

			FInventoryNetCoalescer Stall;
			Entry.Count = 2;
			TestTrue(TEXT("Un fotograma de 2 s manda una vez"), Stall.Tick(2.0f, { Entry }, Delta));
			Entry.Count = 3;
			TestFalse(TEXT("y no otra en el siguiente fotograma"), Stall.Tick(1.0f / 60.0f, { Entry }, Delta));
		});
	});
}

#endif
