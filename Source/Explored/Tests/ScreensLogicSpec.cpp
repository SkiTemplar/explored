#include "Misc/AutomationTest.h"

#include "Carry/InventoryModel.h"
#include "Save/SaveSlots.h"
#include "UI/ScreensLogic.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredScreensLogicSpec, "Explored.UI.ScreensLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredScreensLogicSpec)

namespace ScreensLogicSpecDetail
{
	FSaveSlotInfo Info(const FString& SlotId, ESaveLoadResult Result, bool bFromBackup = false, int64 Timestamp = 0)
	{
		FSaveSlotInfo Out;
		Out.SlotId = SlotId;
		Out.Result = Result;
		Out.bFromBackup = bFromBackup;
		Out.Header.SlotId = SlotId;
		Out.Header.TimestampUnix = Timestamp;
		return Out;
	}

	FInventoryItem Item(int64 InstanceId, float WeightKg)
	{
		FInventoryItem Out;
		Out.InstanceId = InstanceId;
		Out.DefinitionId = FName(TEXT("piedra"));
		Out.WeightKg = WeightKg;
		Out.VolumeLiters = 0.5f;
		return Out;
	}
}

void FExploredScreensLogicSpec::Define()
{
	using namespace ExploredScreens;
	using namespace ScreensLogicSpecDetail;

	Describe("ranuras de guardado", [this]()
	{
		It("siempre muestra la automática y las tres manuales en orden fijo", [this]()
		{
			const TArray<FSaveSlotRow> Rows = BuildSaveSlotRows({ Info(TEXT("manual2"), ESaveLoadResult::Ok, false, 100) }, {}, ESaveSlotsMode::Save);
			if (!TestEqual(TEXT("Cuatro"), Rows.Num(), 4))
			{
				return;
			}
			TestTrue(TEXT("Automática primero"), Rows[0].bIsAuto);
			TestEqual(TEXT("Manual 1"), Rows[1].ManualIndex, 1);
			TestEqual(TEXT("Manual 3"), Rows[3].SlotId, FString(TEXT("manual3")));
			TestEqual(TEXT("Vacía"), Rows[1].State, ESaveSlotRowState::Empty);
			TestEqual(TEXT("Con partida"), Rows[2].State, ESaveSlotRowState::Ok);
			TestTrue(TEXT("Cabecera"), Rows[2].Header.TimestampUnix == 100);
		});

		It("guardando: la automática no se elige y pisar una partida pide confirmación", [this]()
		{
			const TArray<FSaveSlotRow> Rows = BuildSaveSlotRows({
				Info(TEXT("auto"), ESaveLoadResult::Ok),
				Info(TEXT("manual1"), ESaveLoadResult::BadChecksum),
				Info(TEXT("manual2"), ESaveLoadResult::Ok) }, {}, ESaveSlotsMode::Save);
			TestFalse(TEXT("Automática"), Rows[0].bSelectable);
			TestTrue(TEXT("Dañada: se puede pisar"), Rows[1].bSelectable && Rows[1].bNeedsOverwriteConfirm);
			TestTrue(TEXT("Con partida: confirma"), Rows[2].bSelectable && Rows[2].bNeedsOverwriteConfirm);
			TestTrue(TEXT("Vacía: sin confirmación"), Rows[3].bSelectable && !Rows[3].bNeedsOverwriteConfirm);
		});

		It("cargando: solo las legibles, también las recuperadas de la copia", [this]()
		{
			const TArray<FSaveSlotRow> Rows = BuildSaveSlotRows({
				Info(TEXT("auto"), ESaveLoadResult::Ok, true),
				Info(TEXT("manual1"), ESaveLoadResult::FutureVersion),
				Info(TEXT("manual2"), ESaveLoadResult::Malformed) }, { TEXT("auto"), TEXT("Manual2") }, ESaveSlotsMode::Load);
			TestEqual(TEXT("Recuperada"), Rows[0].State, ESaveSlotRowState::Recovered);
			TestTrue(TEXT("Recuperada se carga"), Rows[0].bSelectable);
			TestTrue(TEXT("Copia de la automática"), Rows[0].bHasBackup);
			TestEqual(TEXT("Versión futura"), Rows[1].State, ESaveSlotRowState::FutureVersion);
			TestFalse(TEXT("Versión futura no se carga"), Rows[1].bSelectable);
			TestEqual(TEXT("Dañada"), Rows[2].State, ESaveSlotRowState::Damaged);
			TestTrue(TEXT("Copia por nombre de la UI"), Rows[2].bHasBackup);
			TestFalse(TEXT("Vacía no se carga"), Rows[3].bSelectable);
			TestFalse(TEXT("Cargando nunca confirma"), Rows[0].bNeedsOverwriteConfirm);
		});

		It("parte el tiempo jugado en horas y minutos", [this]()
		{
			const FPlayTime T = SplitPlayTime(3.0 * 3600.0 + 12.0 * 60.0 + 59.0);
			TestEqual(TEXT("Horas"), T.Hours, 3);
			TestEqual(TEXT("Minutos"), T.Minutes, 12);
			TestEqual(TEXT("Negativo"), SplitPlayTime(-5.0).Hours, 0);
			TestEqual(TEXT("NaN"), SplitPlayTime(std::numeric_limits<double>::quiet_NaN()).Minutes, 0);
			const FPlayTime Huge = SplitPlayTime(1.0e300);
			TestTrue(TEXT("Enorme sin desbordar"), Huge.Hours == 1000000 && Huge.Minutes == 0);
		});
	});

	Describe("inventario", [this]()
	{
		It("clasifica la carga por la capacidad cómoda", [this]()
		{
			TestEqual(TEXT("Ligera"), LoadBandFor(0.5f), ELoadBand::Comfortable);
			TestEqual(TEXT("Justa"), LoadBandFor(1.0f), ELoadBand::Comfortable);
			TestEqual(TEXT("Pesada"), LoadBandFor(1.5f), ELoadBand::Heavy);
			TestEqual(TEXT("Al tope"), LoadBandFor(FInventoryModel::MaxLoadRatio), ELoadBand::Overloaded);
			TestEqual(TEXT("NaN"), LoadBandFor(std::numeric_limits<float>::quiet_NaN()), ELoadBand::Comfortable);
		});

		It("enseña bolsillos y cinturón siempre y el resto solo si se lleva", [this]()
		{
			FInventoryState State;
			State.Pockets.Spec = FInventoryContainerSpec::Pockets();
			State.Belt.Spec = FInventoryContainerSpec::Belt(FInventoryModel::BaseBeltHooks);
			State.Pockets.Entries.Add({ Item(11, 0.2f), 2 });
			State.Pockets.Entries.Add({ Item(12, 0.3f), 0 });
			TArray<FInventorySection> Sections = BuildInventorySections(State, false);
			if (!TestEqual(TEXT("Solo bolsillos y cinturón"), Sections.Num(), 2))
			{
				return;
			}
			TestEqual(TEXT("Bolsillos primero"), Sections[0].Slot, EInventorySlot::Pockets);
			TestEqual(TEXT("Cuatro huecos"), Sections[0].Capacity, FInventoryModel::PocketSlots);
			TestTrue(TEXT("Por hueco visible"), Sections[0].ItemIds.Num() == 2 && Sections[0].ItemIds[0] == 12 && Sections[0].ItemIds[1] == 11);
			TestEqual(TEXT("Peso"), Sections[0].UsedWeightKg, 0.5f, 1.e-5f);

			State.bHasBackpack = true;
			State.bBackpackWaterproofPocket = true;
			State.Backpack.Spec = FInventoryContainerSpec::Backpack(20.0f, 12.0f);
			State.Pouch.Spec = FInventoryContainerSpec::Pouch();
			State.bHasSledge = true;
			State.Sledge.Spec = FInventoryContainerSpec::Sledge();
			Sections = BuildInventorySections(State, true);
			if (!TestEqual(TEXT("Todas"), Sections.Num(), 5))
			{
				return;
			}
			TestEqual(TEXT("Bolsa estanca"), Sections[2].Slot, EInventorySlot::Pouch);
			TestEqual(TEXT("Mochila"), Sections[3].Slot, EInventorySlot::Backpack);
			TestEqual(TEXT("Mochila por volumen"), Sections[3].MaxVolumeLiters, 20.0f);
			TestEqual(TEXT("Angarillas al final"), Sections[4].Slot, EInventorySlot::Sledge);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
