#include "Misc/AutomationTest.h"

#include "Save/SaveFormat.h"
#include "Save/SaveIslandSections.h"
#include "Save/SaveSystemStates.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Ida y vuelta de la partida de Baliza Siete (GDD v3 §5.10) en las 10 etapas: se compone el
 * documento con las mismas secciones que el juego (progreso, páginas, cuerpo, mochila e ids
 * recogidos, fuegos y reloj), se codifica en el formato 2, se lee y se reparte en estados
 * nuevos. Todo lo leído tiene que ser igual a lo guardado.
 */
BEGIN_DEFINE_SPEC(FBeaconSaveSpec, "Explored.Save.Beacon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	/** Estado completo de una partida, tal como lo reparten los sistemas del juego. */
	struct FGame
	{
		FBeaconRunState Run;
		FBackpackState Backpack;
		TSet<FName> Collected;
		FSurvivalState Body;
		ESurvivalMode Mode = ESurvivalMode::Survivor;
		TArray<ExploredSaveStates::FSavedFire> Fires;
		ExploredSaveStates::FSavedClock Clock;
	};

	static FGame GameAt(EBeaconStage Stage)
	{
		const int32 Index = static_cast<int32>(Stage);
		FGame Game;
		Game.Run = FBeaconDirectorModel::StateForStage(Stage);
		// Páginas, pensamientos y horas distintas en cada etapa para que nada coincida por casualidad.
		Game.Run.Notebook.PagesMask |= static_cast<uint8>((1u << FMath::Min(Index, 8)) - 1u);
		Game.Run.ThoughtsShown = static_cast<uint16>((1u << (Index + 1)) - 1u);
		Game.Run.LastAbsHours = 7.0 + Index * 6.5;
		if (Game.Run.Progress.bBeaconLit)
		{
			Game.Run.LitAtHours = 69.25;
		}
		Game.Backpack.Stacks.Add({ FName(TEXT("coco")), 1 + Index % 4 });
		Game.Backpack.Stacks.Add({ FName(TEXT("fibra")), 2 + Index % 3 });
		Game.Backpack.CanteenSips = Index % 5;
		if (Stage >= EBeaconStage::LenteEnMano && Stage < EBeaconStage::LinternaMontada)
		{
			Game.Backpack.HeldHeavy = FName(TEXT("lente"));
		}
		Game.Collected.Add(FName(TEXT("lena_cala_1")));
		for (int32 Extra = 0; Extra < Index; ++Extra)
		{
			Game.Collected.Add(FName(*FString::Printf(TEXT("fibra_palmar_%d"), Extra + 1)));
		}
		Game.Body.Thirst = 55.0f - Index * 2.5f;
		Game.Body.Hunger = 70.0f - Index * 3.0f;
		Game.Body.Energy = 40.0f + Index;
		Game.Body.BodyTemperature = 36.5f + Index * 0.05f;
		ExploredSaveStates::FSavedFire& Fire = Game.Fires.AddDefaulted_GetRef();
		Fire.Location = FVector(4020.0, 1220.0, 3510.0 + Index);
		Fire.Fire.FuelHours = 0.25f * Index;
		Game.Clock.Day = 1 + Index / 4;
		Game.Clock.Hours = 7.0f + Index * 1.5f;
		return Game;
	}

	/** Registro de secciones con los nombres del juego sobre el estado Game. */
	static void Register(FSaveSectionRegistry& Registry, FGame& Game, bool& bProgressOk, bool& bPagesOk)
	{
		using namespace ExploredSaveStates;
		Registry.Register(SectionProgress,
			[&Game](FSaveArchive& Ar) { SaveBeaconProgress(Ar, Game.Run); },
			[&Game, &bProgressOk](const FSaveArchive& Ar) { bProgressOk = LoadBeaconProgress(Ar, Game.Run); });
		Registry.Register(SectionPages,
			[&Game](FSaveArchive& Ar) { SavePages(Ar, Game.Run.Notebook); },
			[&Game, &bPagesOk](const FSaveArchive& Ar) { bPagesOk = LoadPages(Ar, Game.Run.Notebook); });
		Registry.Register(SectionIsland,
			[&Game](FSaveArchive& Ar)
			{
				SaveCollectedIds(Ar, Game.Collected);
				FSaveArchive Pack;
				SaveBackpack(Pack, Game.Backpack);
				Ar.Write(TEXT("backpack"), Pack);
			},
			[&Game](const FSaveArchive& Ar)
			{
				LoadCollectedIds(Ar, Game.Collected);
				FSaveArchive Pack;
				if (Ar.Read(TEXT("backpack"), Pack))
				{
					LoadBackpack(Pack, Game.Backpack);
				}
			});
		Registry.Register(TEXT("body"),
			[&Game](FSaveArchive& Ar) { SaveSurvival(Ar, Game.Body, Game.Mode); },
			[&Game](const FSaveArchive& Ar) { LoadSurvival(Ar, Game.Body, Game.Mode); });
		Registry.Register(TEXT("cooking"),
			[&Game](FSaveArchive& Ar)
			{
				TArray<FSaveArchive> Fires;
				for (const FSavedFire& Fire : Game.Fires)
				{
					SaveFire(Fires.AddDefaulted_GetRef(), Fire);
				}
				Ar.Write(TEXT("fires"), Fires);
			},
			[&Game](const FSaveArchive& Ar)
			{
				TArray<FSaveArchive> Fires;
				Ar.Read(TEXT("fires"), Fires);
				Game.Fires.Reset();
				for (const FSaveArchive& Entry : Fires)
				{
					LoadFire(Entry, Game.Fires.AddDefaulted_GetRef());
				}
			});
		Registry.Register(TEXT("time"),
			[&Game](FSaveArchive& Ar) { SaveClock(Ar, Game.Clock); },
			[&Game](const FSaveArchive& Ar) { LoadClock(Ar, Game.Clock); });
	}

	/** Guarda Saved, lo codifica en texto, lo lee y lo reparte en un juego nuevo. */
	FString RoundTrip(FGame& Saved, FGame& Loaded, bool& bProgressOk, bool& bPagesOk)
	{
		FSaveSectionRegistry Writer;
		bool bUnused = true;
		Register(Writer, Saved, bUnused, bUnused);
		FSaveDocument Document;
		Document.Sections = Writer.Capture();
		const FString Text = FSaveCodec::Encode(Document);

		FSaveDocument Read;
		FString Error;
		const ESaveLoadResult Result = FSaveCodec::Decode(Text, FSaveMigrations(), ExploredSave::CurrentFormatVersion, Read, Error);
		TestEqual(TEXT("Se lee"), Result, ESaveLoadResult::Ok);
		TestEqual(TEXT("Formato 2"), Read.Header.FormatVersion, 2);
		FSaveSectionRegistry Reader;
		Register(Reader, Loaded, bProgressOk, bPagesOk);
		Reader.Apply(Read.Sections);
		FSaveSectionRegistry Again;
		Register(Again, Loaded, bUnused, bUnused);
		FSaveDocument Second;
		Second.Sections = Again.Capture();
		return FSaveCodec::Encode(Second) == Text ? FString() : FSaveCodec::Encode(Second);
	}

END_DEFINE_SPEC(FBeaconSaveSpec)

void FBeaconSaveSpec::Define()
{
	It(TEXT("guardar y cargar devuelve el mismo estado en las 10 etapas"), [this]()
	{
		for (int32 Index = 0; Index < static_cast<int32>(EBeaconStage::Count); ++Index)
		{
			const EBeaconStage Stage = static_cast<EBeaconStage>(Index);
			const FString Name = FBeaconDirectorModel::StageName(Stage);
			FGame Saved = GameAt(Stage);
			FGame Loaded;
			bool bProgressOk = false;
			bool bPagesOk = false;
			const FString Mismatch = RoundTrip(Saved, Loaded, bProgressOk, bPagesOk);
			TestTrue(Name + TEXT(": progreso coherente"), bProgressOk);
			TestTrue(Name + TEXT(": páginas legibles"), bPagesOk);
			TestTrue(Name + TEXT(": historia"), Loaded.Run == Saved.Run);
			TestEqual(Name + TEXT(": etapa"), Loaded.Run.Progress.Stage, Stage);
			TestTrue(Name + TEXT(": mochila"), Loaded.Backpack == Saved.Backpack);
			bool bSameIds = Loaded.Collected.Num() == Saved.Collected.Num();
			for (const FName& Id : Saved.Collected)
			{
				bSameIds &= Loaded.Collected.Contains(Id);
			}
			TestTrue(Name + TEXT(": ids recogidos"), bSameIds);
			TestEqual(Name + TEXT(": sed"), Loaded.Body.Thirst, Saved.Body.Thirst);
			TestEqual(Name + TEXT(": hambre"), Loaded.Body.Hunger, Saved.Body.Hunger);
			TestEqual(Name + TEXT(": energía"), Loaded.Body.Energy, Saved.Body.Energy);
			TestEqual(Name + TEXT(": fuegos"), Loaded.Fires.Num(), Saved.Fires.Num());
			TestTrue(Name + TEXT(": fuego"), Loaded.Fires.Num() == 1 && Loaded.Fires[0].Location == Saved.Fires[0].Location
				&& Loaded.Fires[0].Fire == Saved.Fires[0].Fire);
			TestEqual(Name + TEXT(": día"), Loaded.Clock.Day, Saved.Clock.Day);
			TestEqual(Name + TEXT(": hora"), Loaded.Clock.Hours, Saved.Clock.Hours);
			TestTrue(Name + TEXT(": guardar otra vez da el mismo texto"), Mismatch.IsEmpty());
		}
	});

	It(TEXT("una partida del formato 1 se rechaza limpiamente como obsoleta"), [this]()
	{
		FSaveDocument Old;
		Old.Header.FormatVersion = 1;
		FSaveArchive Legacy;
		Legacy.Write(TEXT("shipParts"), 3);
		Old.Sections.Set(TEXT("progress"), Legacy.GetRoot());
		FSaveDocument Read;
		FString Error;
		TestEqual(TEXT("Obsoleta"), FSaveCodec::Decode(FSaveCodec::Encode(Old), FSaveMigrations(), ExploredSave::CurrentFormatVersion, Read, Error),
			ESaveLoadResult::ObsoleteVersion);
		TestFalse(TEXT("Con mensaje"), Error.IsEmpty());
		TestEqual(TEXT("Nombre"), FString(LexToString(ESaveLoadResult::ObsoleteVersion)), FString(TEXT("ObsoleteVersion")));
	});

	It(TEXT("un progreso incoherente o fuera de rango deja el estado por defecto"), [this]()
	{
		using namespace ExploredSaveStates;
		FBeaconRunState Run = FBeaconDirectorModel::StateForStage(EBeaconStage::Preparacion);
		FSaveArchive Ar;
		SaveBeaconProgress(Ar, FBeaconDirectorModel::StateForStage(EBeaconStage::LenteEnMano));
		Ar.Write(TEXT("flags"), 0);
		TestFalse(TEXT("Lente en mano sin lente"), LoadBeaconProgress(Ar, Run));
		TestTrue(TEXT("Queda en el prólogo"), Run.Progress == FBeaconProgressState());

		FSaveArchive Big;
		SaveBeaconProgress(Big, FBeaconRunState());
		Big.Write(TEXT("oilJars"), 300);
		TestFalse(TEXT("Tarros fuera de rango"), LoadBeaconProgress(Big, Run));

		FSaveArchive Lit;
		SaveBeaconProgress(Lit, FBeaconRunState());
		Lit.Write(TEXT("litAtHours"), 70.0);
		Lit.Write(TEXT("mistralArrived"), true);
		TestTrue(TEXT("Apagada se lee"), LoadBeaconProgress(Lit, Run));
		TestTrue(TEXT("Sin hora de encendido si está apagada"), Run.LitAtHours < 0.0 && !Run.bMistralArrived);

		FSaveArchive Pages;
		Pages.Write(TEXT("mask"), 256);
		FNotebookState Notebook;
		TestFalse(TEXT("Máscara de páginas fuera de rango"), LoadPages(Pages, Notebook));
		TestEqual(TEXT("Sin páginas"), FNotebookModel::ReadCount(Notebook), 0);
		TestTrue(TEXT("Sin sección, partida nueva"), LoadPages(FSaveArchive(), Notebook));
	});

	It(TEXT("la mochila sin pilas no se lee y los ids vacíos se descartan"), [this]()
	{
		using namespace ExploredSaveStates;
		FBackpackState Pack;
		TestFalse(TEXT("Sin pilas"), LoadBackpack(FSaveArchive(), Pack));
		FSaveArchive Ids;
		Ids.Write(TEXT("collected"), TArray<FString>({ TEXT("pagina_1"), TEXT(""), TEXT("lente") }));
		TSet<FName> Collected;
		LoadCollectedIds(Ids, Collected);
		TestEqual(TEXT("Dos ids"), Collected.Num(), 2);
	});

	It(TEXT("el cuerpo y la balsa van y vuelven; lo incoherente se rechaza"), [this]()
	{
		using namespace ExploredSaveStates;
		FBodyBars Bars;
		Bars.Thirst = 12.5f;
		Bars.Warmth = 60.0f;
		FSaveArchive BodyAr;
		SaveBodyBars(BodyAr, Bars, FName(TEXT("refugio_faro_volumen")));
		FBodyBars LoadedBars;
		FName Shelter;
		TestTrue(TEXT("Cuerpo leído"), LoadBodyBars(BodyAr, LoadedBars, Shelter));
		TestTrue(TEXT("Mismas barras"), LoadedBars == Bars);
		TestEqual(TEXT("Mismo refugio"), Shelter, FName(TEXT("refugio_faro_volumen")));
		FSaveArchive BadBody = BodyAr;
		BadBody.Write(TEXT("hunger"), 140.0f);
		TestFalse(TEXT("Barra fuera de rango"), LoadBodyBars(BadBody, LoadedBars, Shelter));
		TestTrue(TEXT("Por defecto"), LoadedBars == FBodyBars());
		TestFalse(TEXT("Vacío"), LoadBodyBars(FSaveArchive(), LoadedBars, Shelter));

		FRaftSaveData Raft;
		Raft.Site.LogsPlaced = 4;
		Raft.Site.bBuilt = true;
		Raft.bHasPose = true;
		Raft.Location = FVector(12000.0, -300.0, 15.0);
		Raft.YawDeg = 37.5f;
		FSaveArchive RaftAr;
		SaveRaft(RaftAr, Raft);
		FRaftSaveData LoadedRaft;
		TestTrue(TEXT("Balsa leída"), LoadRaft(RaftAr, LoadedRaft));
		TestTrue(TEXT("Mismo sitio"), LoadedRaft.Site == Raft.Site);
		TestTrue(TEXT("Misma posición"), LoadedRaft.bHasPose && LoadedRaft.Location.Equals(Raft.Location) && LoadedRaft.YawDeg == Raft.YawDeg);
		FSaveArchive BadRaft = RaftAr;
		BadRaft.Write(TEXT("logs"), 2);
		TestFalse(TEXT("Atada con 2 troncos"), LoadRaft(BadRaft, LoadedRaft));
		TestEqual(TEXT("Por hacer"), LoadedRaft.Site.LogsPlaced, 0);
	});
}

#endif
