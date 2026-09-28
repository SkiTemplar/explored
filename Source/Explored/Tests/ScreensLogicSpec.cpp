#include "Misc/AutomationTest.h"

#include "Achievements/AchievementsModel.h"
#include "Carry/InventoryModel.h"
#include "Ruins/MuseumModel.h"
#include "Save/SaveSlots.h"
#include "UI/ScreensLogic.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredScreensLogicSpec, "Explored.UI.ScreensLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredScreensLogicSpec)

namespace ScreensLogicSpecDetail
{
	constexpr int32 ShelfKey = 7;

	/** Doce tesoros pequeños y un mueble con doce huecos: basta para el umbral de «Coleccionista». */
	FTreasureCatalog TestCatalog()
	{
		FTreasureCatalog C;
		for (int32 I = 0; I < 12; ++I)
		{
			FArtifactDef A;
			A.Id = FName(*FString::Printf(TEXT("t_%02d"), I));
			A.Kind = FName(TEXT("hook_bone"));
			A.NameEs = A.Id.ToString();
			A.NameEn = A.Id.ToString();
			A.Provenance = I % 2 == 0 ? EArtifactProvenance::Marae : EArtifactProvenance::Shipwreck;
			A.Rarity = I == 11 ? EArtifactRarity::Unique : EArtifactRarity::Common;
			C.Artifacts.Add(A);
		}
		FDisplayDef Shelf;
		Shelf.Id = FName(TEXT("estanteria_museo"));
		for (int32 I = 0; I < 12; ++I)
		{
			FDisplaySlotDef S;
			S.MaxSize = EArtifactSize::Medium;
			Shelf.Slots.Add(S);
		}
		C.Displays.Add(Shelf);
		return C;
	}

	FName Id(int32 I)
	{
		return FName(*FString::Printf(TEXT("t_%02d"), I));
	}

	FAchievementsModel TestAchievements(FString& OutError)
	{
		FAchievementStatDef Fires;
		Fires.Id = FName(TEXT("fires_lit"));
		FAchievementStatDef Flag;
		Flag.Id = FName(TEXT("secret_seen"));
		Flag.Kind = EAchievementStatKind::Flag;

		FAchievementDef First;
		First.Id = FName(TEXT("first_fire"));
		First.Condition = FAchievementCondition::AtLeast(Fires.Id, 1.0);
		FAchievementDef Ten;
		Ten.Id = FName(TEXT("ten_fires"));
		Ten.Condition = FAchievementCondition::AtLeast(Fires.Id, 10.0);
		FAchievementDef Secret;
		Secret.Id = FName(TEXT("secret"));
		Secret.bHidden = true;
		Secret.Condition = FAchievementCondition::Flag(Flag.Id);
		FAchievementDef Castaway;
		Castaway.Id = FName(TEXT("castaway_only"));
		Castaway.Modes = { FName(TEXT("Castaway")) };
		Castaway.Condition = FAchievementCondition::AtLeast(Fires.Id, 3.0);

		FAchievementsModel Model;
		Model.Configure({ Fires, Flag }, { First, Ten, Secret, Castaway }, OutError);
		return Model;
	}

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

	Describe("museo y catálogo", [this]()
	{
		It("cada ficha pasa de silueta a fotografiada, hallada y expuesta", [this]()
		{
			FMuseumModel Museum(TestCatalog());
			Museum.MarkPhotographed(Id(1));
			Museum.MarkFound(Id(2), FName(TEXT("ruin_emerald")), 3);
			Museum.MarkFound(Id(3), FName(TEXT("shipwreck")), 5);
			TestEqual(TEXT("Mueble"), Museum.AddDisplay(ShelfKey, FName(TEXT("estanteria_museo"))), EMuseumResult::Ok);
			TestEqual(TEXT("Expone"), Museum.Place(Id(3), ShelfKey, 0), EMuseumResult::Ok);

			const TArray<FMuseumEntry> Entries = BuildMuseumEntries(Museum);
			if (!TestEqual(TEXT("Una ficha por tesoro"), Entries.Num(), 12))
			{
				return;
			}
			TestEqual(TEXT("Orden del catálogo"), Entries[0].ArtifactId, Id(0));
			TestEqual(TEXT("Silueta"), Entries[0].State, EMuseumEntryState::Unknown);
			TestFalse(TEXT("Sin nombre"), Entries[0].bShowName);
			TestEqual(TEXT("La silueta conserva la procedencia"), Entries[1].Provenance, EArtifactProvenance::Shipwreck);
			TestEqual(TEXT("Fotografiada"), Entries[1].State, EMuseumEntryState::Photographed);
			TestTrue(TEXT("Con nombre"), Entries[1].bShowName);
			TestTrue(TEXT("Sin lugar de hallazgo"), Entries[1].FoundAt.IsNone());
			TestEqual(TEXT("Hallada"), Entries[2].State, EMuseumEntryState::Found);
			TestEqual(TEXT("Dónde"), Entries[2].FoundAt, FName(TEXT("ruin_emerald")));
			TestEqual(TEXT("Isla"), Entries[2].FoundIsland, 3);
			TestEqual(TEXT("Expuesta"), Entries[3].State, EMuseumEntryState::Exhibited);
			TestEqual(TEXT("Rareza"), Entries[11].Rarity, EArtifactRarity::Unique);
		});

		It("resume el catálogo y el progreso de Coleccionista", [this]()
		{
			FMuseumModel Museum(TestCatalog());
			FMuseumSummary Empty = SummarizeMuseum(Museum);
			TestEqual(TEXT("Total"), Empty.Total, 12);
			TestEqual(TEXT("Nada registrado"), Empty.Registered, 0);
			TestEqual(TEXT("Coleccionista a cero"), Empty.CollectorProgress, 0.0f);

			Museum.AddDisplay(ShelfKey, FName(TEXT("estanteria_museo")));
			for (int32 I = 0; I < 11; ++I)
			{
				Museum.MarkFound(Id(I), FName(TEXT("ruin_emerald")), 0);
				if (I < 5)
				{
					Museum.Place(Id(I), ShelfKey, I);
				}
			}
			const FMuseumSummary Half = SummarizeMuseum(Museum);
			TestEqual(TEXT("Hallados"), Half.Found, 11);
			TestEqual(TEXT("Expuestos"), Half.Exhibited, 5);
			TestEqual(TEXT("Mitad del objetivo"), Half.CollectorProgress, 0.5f, 1.e-6f);
			TestFalse(TEXT("Aún no"), Half.bCollectorDone);
			TestEqual(TEXT("Catálogo"), Half.CatalogProgress, 11.0f / 12.0f, 1.e-6f);

			for (int32 I = 5; I < 11; ++I)
			{
				Museum.Place(Id(I), ShelfKey, I);
			}
			const FMuseumSummary Done = SummarizeMuseum(Museum);
			TestEqual(TEXT("Once expuestos"), Done.Exhibited, 11);
			TestEqual(TEXT("La barra no pasa de 1"), Done.CollectorProgress, 1.0f);
			TestTrue(TEXT("Coleccionista"), Done.bCollectorDone);
		});
	});

	Describe("logros", [this]()
	{
		It("los ocultos salen como ??? sin barra hasta conseguirlos", [this]()
		{
			FString Error;
			FAchievementsModel Model = TestAchievements(Error);
			if (!TestTrue(TEXT("Configura"), Error.IsEmpty()))
			{
				return;
			}
			Model.Report(FName(TEXT("fires_lit")), 4.0);
			TArray<FAchievementRow> Rows = BuildAchievementRows(Model);
			if (!TestEqual(TEXT("Cuatro filas"), Rows.Num(), 4))
			{
				return;
			}
			TestTrue(TEXT("Primer fuego conseguido"), Rows[0].bUnlocked);
			TestEqual(TEXT("Barra llena"), Rows[0].Progress, 1.0f);
			TestEqual(TEXT("Diez fuegos a medias"), Rows[1].Progress, 0.4f, 1.e-4f);
			TestTrue(TEXT("Oculto enmascarado"), Rows[2].bMasked);
			TestEqual(TEXT("Oculto sin barra"), Rows[2].Progress, 0.0f);
			TestTrue(TEXT("Fuera de partida todos cuentan como disponibles"), Rows[3].bAvailableInMode);

			Model.Report(FName(TEXT("secret_seen")));
			Rows = BuildAchievementRows(Model);
			TestFalse(TEXT("Conseguido: ya no se oculta"), Rows[2].bMasked);
			TestTrue(TEXT("Conseguido"), Rows[2].bUnlocked);

			const FAchievementsSummary Summary = SummarizeAchievements(Model);
			TestEqual(TEXT("Conseguidos"), Summary.Unlocked, 2);
			TestEqual(TEXT("Total"), Summary.Total, 4);
			TestEqual(TEXT("Fracción"), Summary.Fraction, 0.5f, 1.e-6f);
		});

		It("en una partida de otro modo marca los que no se pueden conseguir", [this]()
		{
			FString Error;
			FAchievementsModel Model = TestAchievements(Error);
			Model.BeginRun(FName(TEXT("Explorer")));
			const TArray<FAchievementRow> Explorer = BuildAchievementRows(Model);
			TestFalse(TEXT("Solo en Náufrago"), Explorer.Num() == 4 && Explorer[3].bAvailableInMode);
			Model.BeginRun(FName(TEXT("Castaway")));
			const TArray<FAchievementRow> Castaway = BuildAchievementRows(Model);
			TestTrue(TEXT("En Náufrago sí"), Castaway.Num() == 4 && Castaway[3].bAvailableInMode);
		});

		It("oculta los de una fase sin publicar salvo que ya estén conseguidos", [this]()
		{
			FString Error;
			FAchievementsModel Model = TestAchievements(Error);
			TArray<FAchievementDef> Defs = Model.GetAchievements();
			Defs[1].Phase = EAchievementPhase::Phase2;
			FAchievementsModel Phased;
			if (!TestTrue(TEXT("Configura"), Phased.Configure(Model.GetStats(), Defs, Error)))
			{
				return;
			}
			Phased.Report(FName(TEXT("fires_lit")), 10.0);
			TestTrue(TEXT("Con todo publicado, diez fuegos"), Phased.IsUnlocked(Defs[1].Id));

			Phased.SetReleasedPhase(EAchievementPhase::EarlyAccess);
			TestEqual(TEXT("Ya conseguido: sigue en la lista"), BuildAchievementRows(Phased).Num(), 4);

			FAchievementsModel Fresh;
			Fresh.Configure(Model.GetStats(), Defs, Error);
			Fresh.SetReleasedPhase(EAchievementPhase::EarlyAccess);
			const TArray<FAchievementRow> Rows = BuildAchievementRows(Fresh);
			TestEqual(TEXT("Sin conseguir: fuera de la lista"), Rows.Num(), 3);
			TestFalse(TEXT("Ninguna fila es la de F2"), Rows.ContainsByPredicate([&](const FAchievementRow& Row) { return Row.Id == Defs[1].Id; }));
			TestEqual(TEXT("El total no cuenta la F2"), SummarizeAchievements(Fresh).Total, 3);
		});
	});

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
