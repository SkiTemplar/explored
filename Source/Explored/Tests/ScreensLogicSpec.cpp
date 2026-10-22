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

	Describe("cuaderno", [this]()
	{
		It("enseña la página pedida si está leída y cuenta las leídas", [this]()
		{
			const uint8 Mask = 0b00010101; // páginas 1, 3 y 5
			const FNotebookView View = BuildNotebookView(Mask, 3);
			TestEqual("8 filas", View.Rows.Num(), 8);
			TestEqual("3 leídas", View.ReadCount, 3);
			TestEqual("Enseña la 3", View.Shown, 3);
			TestTrue("La 3 marcada", View.Rows[2].bSelected);
			TestFalse("La 2 sin leer", View.Rows[1].bRead);
		});

		It("con una página sin leer enseña la última leída y sin ninguna, nada", [this]()
		{
			TestEqual("Pide la 2 (sin leer): la 5", BuildNotebookView(0b00010101, 2).Shown, 5);
			const FNotebookView Empty = BuildNotebookView(0, 1);
			TestEqual("Ninguna", Empty.Shown, 0);
			TestEqual("0 leídas", Empty.ReadCount, 0);
		});

		It("pasa de página saltando las no leídas y se queda en los extremos", [this]()
		{
			const uint8 Mask = 0b00010101;
			TestEqual("1 → 3", StepNotebookPage(Mask, 1, 1), 3);
			TestEqual("5 → 3", StepNotebookPage(Mask, 5, -1), 3);
			TestEqual("5 sin más", StepNotebookPage(Mask, 5, 1), 5);
			TestEqual("1 sin menos", StepNotebookPage(Mask, 1, -1), 1);
		});
	});

	Describe(TEXT("menú de pausa"), [this]()
	{
		using namespace ExploredScreens;

		It(TEXT("«Salir del juego» con progreso sin guardar pregunta y no sale"), [this]()
		{
			const FPauseMenuStep Step = PauseRequestExit(true, true);
			TestTrue(TEXT("no sale todavía"), Step.Action == EPauseMenuAction::None);
			TestTrue(TEXT("abre la pregunta de salir del juego"), Step.Page == EPauseMenuPage::ConfirmQuit);
			TestTrue(TEXT("con el foco en «No»"), Step.Focus == EPauseMenuFocus::ConfirmNo);
		});

		It(TEXT("«Salir al menú» con progreso sin guardar abre su propia pregunta"), [this]()
		{
			const FPauseMenuStep Step = PauseRequestExit(false, true);
			TestTrue(TEXT("no sale todavía"), Step.Action == EPauseMenuAction::None);
			TestTrue(TEXT("pregunta de salir al menú"), Step.Page == EPauseMenuPage::ConfirmExit);
		});

		It(TEXT("sin nada que perder sale al momento"), [this]()
		{
			TestTrue(TEXT("sale del juego"), PauseRequestExit(true, false).Action == EPauseMenuAction::Quit);
			TestTrue(TEXT("sale al menú"), PauseRequestExit(false, false).Action == EPauseMenuAction::ExitToMenu);
			TestTrue(TEXT("sin cambiar de página"), PauseRequestExit(true, false).Page == EPauseMenuPage::Buttons);
		});

		It(TEXT("«Sí» lanza el delegado de su pregunta y no el de la otra"), [this]()
		{
			TestTrue(TEXT("salir del juego"), PauseConfirmYes(EPauseMenuPage::ConfirmQuit).Action == EPauseMenuAction::Quit);
			TestTrue(TEXT("salir al menú"), PauseConfirmYes(EPauseMenuPage::ConfirmExit).Action == EPauseMenuAction::ExitToMenu);
			TestTrue(TEXT("en los botones no hace nada"), PauseConfirmYes(EPauseMenuPage::Buttons).Action == EPauseMenuAction::None);
		});

		It(TEXT("«No» vuelve a los botones con el foco en el que preguntó"), [this]()
		{
			const FPauseMenuStep FromQuit = PauseConfirmNo(EPauseMenuPage::ConfirmQuit);
			TestTrue(TEXT("página 0"), FromQuit.Page == EPauseMenuPage::Buttons);
			TestTrue(TEXT("sin salir"), FromQuit.Action == EPauseMenuAction::None);
			TestTrue(TEXT("foco en «Salir del juego»"), FromQuit.Focus == EPauseMenuFocus::QuitButton);
			const FPauseMenuStep FromExit = PauseConfirmNo(EPauseMenuPage::ConfirmExit);
			TestTrue(TEXT("foco en «Salir al menú»"), FromExit.Focus == EPauseMenuFocus::ExitButton);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
