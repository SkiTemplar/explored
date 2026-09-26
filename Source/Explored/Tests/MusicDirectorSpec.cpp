#include "Misc/AutomationTest.h"

#include "Audio/MusicDirectorModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MusicDirectorSpecDetail
{
	FMusicPiece MakePiece(const TCHAR* Id, EMusicRole Role, const TCHAR* Variant, float Bpm, int32 BeatsPerBar, float Bars, bool bLoop)
	{
		FMusicPiece Piece;
		Piece.Id = FName(Id);
		Piece.Asset = FString::Printf(TEXT("/Game/Generated/Audio/Musica/%s.%s"), Id, Id);
		Piece.Role = Role;
		Piece.Variant = Variant;
		Piece.Bpm = Bpm;
		Piece.BeatsPerBar = BeatsPerBar;
		Piece.Bars = Bars;
		Piece.bLoop = bLoop;
		return Piece;
	}

	/** El mismo contenido que Content/Data/music_layers.json (lo genera Tools/Audio). */
	FMusicCatalog MakeCatalog()
	{
		FMusicCatalog C;
		C.Add(MakePiece(TEXT("mus_theme"), EMusicRole::Theme, TEXT(""), 80, 4, 40, false));
		C.Add(MakePiece(TEXT("mus_explore_landing"), EMusicRole::Explore, TEXT("landing"), 82, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_explore_emerald"), EMusicRole::Explore, TEXT("emerald"), 84, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_explore_smoke"), EMusicRole::Explore, TEXT("smoke"), 70, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_explore_teeth"), EMusicRole::Explore, TEXT("teeth"), 86, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_explore_mangrove"), EMusicRole::Explore, TEXT("mangrove"), 74, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_explore_whitesands"), EMusicRole::Explore, TEXT("whitesands"), 88, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_explore_mesa"), EMusicRole::Explore, TEXT("mesa"), 78, 4, 40, true));
		C.Add(MakePiece(TEXT("mus_night"), EMusicRole::Night, TEXT(""), 70, 4, 16, true));
		C.Add(MakePiece(TEXT("mus_tension"), EMusicRole::Tension, TEXT(""), 90, 4, 16, true));
		C.Add(MakePiece(TEXT("mus_storm"), EMusicRole::Storm, TEXT(""), 64, 4, 16, true));
		C.Add(MakePiece(TEXT("mus_sea"), EMusicRole::Sea, TEXT(""), 152, 6, 16, true));
		C.Add(MakePiece(TEXT("mus_discovery_01"), EMusicRole::Discovery, TEXT("01"), 90, 4, 0.5f, false));
		C.Add(MakePiece(TEXT("mus_discovery_02"), EMusicRole::Discovery, TEXT("02"), 88, 4, 0.6f, false));
		C.Add(MakePiece(TEXT("mus_discovery_03"), EMusicRole::Discovery, TEXT("03"), 92, 4, 0.65f, false));
		C.Add(MakePiece(TEXT("mus_discovery_04"), EMusicRole::Discovery, TEXT("04"), 76, 4, 0.65f, false));
		C.Add(MakePiece(TEXT("mus_finale_rescue"), EMusicRole::Finale, TEXT("rescue"), 84, 4, 32, false));
		C.Add(MakePiece(TEXT("mus_finale_voyage"), EMusicRole::Finale, TEXT("voyage"), 80, 4, 32, false));
		C.Add(MakePiece(TEXT("mus_finale_stay"), EMusicRole::Finale, TEXT("stay"), 72, 4, 32, false));
		C.Add(MakePiece(TEXT("mus_menu"), EMusicRole::Menu, TEXT(""), 72, 4, 16, true));
		C.Add(MakePiece(TEXT("mus_credits"), EMusicRole::Credits, TEXT(""), 76, 4, 32, false));

		const TCHAR* const Variants[][3] = {
			{TEXT("mus_explore_landing"), TEXT("mus_explore_teeth"), TEXT("mus_explore_whitesands")},
			{TEXT("mus_explore_emerald"), TEXT("mus_explore_mesa"), TEXT("mus_explore_mangrove")},
			{TEXT("mus_explore_smoke"), TEXT("mus_explore_mangrove"), TEXT("mus_explore_mesa")},
			{TEXT("mus_explore_teeth"), TEXT("mus_explore_whitesands"), TEXT("mus_explore_landing")},
			{TEXT("mus_explore_mangrove"), TEXT("mus_explore_mesa"), TEXT("mus_explore_smoke")},
			{TEXT("mus_explore_whitesands"), TEXT("mus_explore_teeth"), TEXT("mus_explore_landing")},
			{TEXT("mus_explore_mesa"), TEXT("mus_explore_mangrove"), TEXT("mus_explore_emerald")},
		};
		for (int32 Island = 0; Island < static_cast<int32>(EIslandArchetype::Count); ++Island)
		{
			for (const TCHAR* Id : Variants[Island])
			{
				C.DayVariants[Island].Add(C.FindById(FName(Id)));
			}
		}
		C.Flute.SampleId = FName(TEXT("sfx_flute_note"));
		C.Flute.SampleHz = 293.6648f;
		C.Flute.Semitones = {0, 2, 4, 7, 9};
		return C;
	}

	FMusicContext DayOn(EIslandArchetype Island)
	{
		FMusicContext Context;
		Context.Island = Island;
		return Context;
	}

	/** Múltiplo entero (con tolerancia) de Step contado desde Origin. */
	bool IsOnGrid(double Value, double Origin, double Step)
	{
		const double Steps = (Value - Origin) / Step;
		return FMath::Abs(Steps - FMath::RoundToDouble(Steps)) < 1.0e-6;
	}

	const FMusicLayerTarget* FindAction(const TArray<FMusicLayerTarget>& Targets, EMusicLayerAction Action, const FName& Piece = NAME_None)
	{
		return Targets.FindByPredicate([Action, &Piece](const FMusicLayerTarget& T)
		{
			return T.Action == Action && (Piece.IsNone() || T.PieceId == Piece);
		});
	}

	/** Avanza el director con un paso fijo y acumula todas sus órdenes. */
	void Run(FMusicDirectorModel& Director, double& Clock, double Seconds, const FMusicContext& Context, TArray<FMusicLayerTarget>& Out, double Step = 0.25)
	{
		const double End = Clock + Seconds;
		while (Clock < End - 1.0e-9)
		{
			Clock += Step;
			Director.Update(Clock, Context, Out);
		}
	}
}

BEGIN_DEFINE_SPEC(FMusicDirectorSpec, "Explored.Audio.MusicDirector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FMusicDirectorSpec)

void FMusicDirectorSpec::Define()
{
	using namespace MusicDirectorSpecDetail;

	It("valida el catálogo y reconoce las claves del JSON", [this]()
	{
		FString Error;
		TestTrue(TEXT("Catálogo completo"), MakeCatalog().Validate(Error));
		FMusicCatalog Missing = MakeCatalog();
		Missing.Pieces.RemoveAll([](const FMusicPiece& P) { return P.Role == EMusicRole::Storm; });
		TestFalse(TEXT("Sin pieza de tormenta"), Missing.Validate(Error));

		EMusicRole Role = EMusicRole::Count;
		TestTrue(TEXT("Papel conocido"), MusicRoleFromKey(TEXT("discovery"), Role));
		TestEqual(TEXT("discovery"), Role, EMusicRole::Discovery);
		TestFalse(TEXT("Papel desconocido"), MusicRoleFromKey(TEXT("jazz"), Role));
		EIslandArchetype Island = EIslandArchetype::Count;
		TestTrue(TEXT("Isla conocida"), IslandArchetypeFromMusicKey(TEXT("whitesands"), Island));
		TestEqual(TEXT("whitesands"), Island, EIslandArchetype::WhiteSands);
	});

	It("sigue la tabla del GDD §14.3 con sus prioridades e histéresis", [this]()
	{
		const FMusicDirectorTuning T;
		auto Mood = [&T](const FMusicContext& C, EMusicMood Prev = EMusicMood::Explore) { return FMusicDirectorModel::ChooseMood(C, Prev, T); };

		FMusicContext C = DayOn(EIslandArchetype::Emerald);
		TestEqual(TEXT("Día en una isla"), Mood(C), EMusicMood::Explore);
		C.Night = 0.8f;
		TestEqual(TEXT("Noche"), Mood(C), EMusicMood::Night);
		C.Night = 0.5f;
		TestEqual(TEXT("Crepúsculo desde el día"), Mood(C), EMusicMood::Explore);
		TestEqual(TEXT("Crepúsculo desde la noche (histéresis)"), Mood(C, EMusicMood::Night), EMusicMood::Night);

		C = DayOn(EIslandArchetype::Count);
		C.bSailing = true;
		TestEqual(TEXT("Mar abierto a vela"), Mood(C), EMusicMood::Sea);
		C.Night = 1.0f;
		TestEqual(TEXT("Navegar de noche sigue siendo mar abierto"), Mood(C), EMusicMood::Sea);

		C = DayOn(EIslandArchetype::Landing);
		C.Danger = 0.7f;
		TestEqual(TEXT("Depredador cerca"), Mood(C), EMusicMood::Tension);
		C.Danger = 0.4f;
		TestEqual(TEXT("Peligro medio sin tensión previa"), Mood(C), EMusicMood::Explore);
		TestEqual(TEXT("Peligro medio con tensión (histéresis)"), Mood(C, EMusicMood::Tension), EMusicMood::Tension);

		C = DayOn(EIslandArchetype::Landing);
		C.Storm = 0.8f;
		TestEqual(TEXT("Temporal"), Mood(C), EMusicMood::Storm);
		C.bCyclone = true;
		TestEqual(TEXT("Ciclón"), Mood(C), EMusicMood::Storm);
		C = DayOn(EIslandArchetype::Landing);
		C.CycloneEtaHours = 2.0f;
		TestEqual(TEXT("Silencio antes del ciclón"), Mood(C), EMusicMood::CycloneHush);
		C.Danger = 1.0f;
		TestEqual(TEXT("El silencio del ciclón manda sobre la tensión"), Mood(C), EMusicMood::CycloneHush);
		C.CycloneEtaHours = 12.0f;
		TestEqual(TEXT("Ciclón aún lejano"), Mood(C), EMusicMood::Tension);

		C = DayOn(EIslandArchetype::Landing);
		C.bFinale = true;
		C.Danger = 1.0f;
		TestEqual(TEXT("Partida del «Limón»"), Mood(C), EMusicMood::Finale);
		C.bInMenu = true;
		TestEqual(TEXT("Menú por encima de todo"), Mood(C), EMusicMood::Menu);
	});

	It("elige la pieza de cada estado desde el catálogo", [this]()
	{
		struct FCase { FMusicContext Context; const TCHAR* Expected; };
		TArray<FCase> Cases;
		FMusicContext C;
		C.bInMenu = true;
		Cases.Add({C, TEXT("mus_menu")});
		C = DayOn(EIslandArchetype::Emerald);
		Cases.Add({C, TEXT("mus_explore_emerald")});
		C.Night = 1.0f;
		Cases.Add({C, TEXT("mus_night")});
		C = DayOn(EIslandArchetype::Count);
		C.bSailing = true;
		Cases.Add({C, TEXT("mus_sea")});
		C = DayOn(EIslandArchetype::Mesa);
		C.Danger = 1.0f;
		Cases.Add({C, TEXT("mus_tension")});
		C = DayOn(EIslandArchetype::Mesa);
		C.bCyclone = true;
		Cases.Add({C, TEXT("mus_storm")});
		C = DayOn(EIslandArchetype::Landing);
		C.bFinale = true;
		C.Finale = EMusicFinale::Voyage;
		Cases.Add({C, TEXT("mus_finale_voyage")});

		for (const FCase& Case : Cases)
		{
			// Director nuevo en cada caso: la primera pieza de exploración es la propia de la isla
			// con la semilla usada (la propia pesa el doble), el resto de papeles son únicos.
			FMusicDirectorModel Director(MakeCatalog(), 3);
			TArray<FMusicLayerTarget> Out;
			Director.Update(1.0, Case.Context, Out);
			const FName Expected(Case.Expected);
			if (Expected == FName(TEXT("mus_explore_emerald")))
			{
				const TArray<int32>& Variants = Director.GetCatalog().DayVariants[static_cast<int32>(EIslandArchetype::Emerald)];
				const int32 Chosen = Director.GetCatalog().FindById(Director.GetPrimaryPiece());
				TestTrue(TEXT("Variación diurna de Esmeralda"), Variants.Contains(Chosen));
				continue;
			}
			TestEqual(FString::Printf(TEXT("Pieza de %s"), Case.Expected), Director.GetPrimaryPiece().ToString(), Expected.ToString());
			TestNotNull(TEXT("Orden de arranque"), FindAction(Out, EMusicLayerAction::Start, Expected));
		}
	});

	It("cuantiza los relevos al compás, y la tensión al tiempo, de la pieza que suena", [this]()
	{
		FMusicDirectorModel Director(MakeCatalog(), 11);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 10.3, DayOn(EIslandArchetype::Emerald), Out);
		const FMusicLayerTarget* FirstFound = FindAction(Out, EMusicLayerAction::Start);
		if (!TestNotNull(TEXT("Arranca la exploración"), FirstFound))
		{
			return;
		}
		// Copias: Out se vacía y crece más abajo, los punteros a sus elementos no sobreviven.
		const FMusicLayerTarget First = *FirstFound;
		const FMusicPiece Day = Director.GetCatalog().Pieces[Director.GetCatalog().FindById(First.PieceId)];
		TestEqual(TEXT("Desde el silencio empieza en el acto"), First.AtSeconds, 0.25);

		// Anochece: el relevo espera al siguiente compás de la pieza diurna.
		Out.Reset();
		FMusicContext Night = DayOn(EIslandArchetype::Emerald);
		Night.Night = 1.0f;
		Director.Update(Clock, Night, Out);
		const FMusicLayerTarget* Stop = FindAction(Out, EMusicLayerAction::Stop, First.PieceId);
		const FMusicLayerTarget* Start = FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_night")));
		if (!TestNotNull(TEXT("Funde la pieza diurna"), Stop) || !TestNotNull(TEXT("Entra la noche"), Start))
		{
			return;
		}
		TestTrue(TEXT("En un límite de compás"), IsOnGrid(Stop->AtSeconds, First.AtSeconds, Day.SecondsPerBar()));
		TestTrue(TEXT("No antes de ahora"), Stop->AtSeconds >= Clock);
		TestTrue(TEXT("Como mucho un compás de espera"), Stop->AtSeconds - Clock < Day.SecondsPerBar());
		TestEqual(TEXT("Fundido cruzado en el mismo límite"), Start->AtSeconds, Stop->AtSeconds);
		TestTrue(TEXT("Fundido de entrada"), Start->FadeSeconds > 1.0f);

		// Un depredador de noche: la tensión entra en el siguiente tiempo.
		const double NightStart = Start->AtSeconds;
		const FMusicPiece NightPiece = Director.GetCatalog().Pieces[Director.GetCatalog().FindById(FName(TEXT("mus_night")))];
		Clock = NightStart + 7.1;
		Director.Update(Clock, Night, Out);
		Out.Reset();
		FMusicContext Danger = Night;
		Danger.Danger = 1.0f;
		Director.Update(Clock, Danger, Out);
		const FMusicLayerTarget* TensionStart = FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_tension")));
		if (!TestNotNull(TEXT("Entra la tensión"), TensionStart))
		{
			return;
		}
		TestTrue(TEXT("En un límite de tiempo"), IsOnGrid(TensionStart->AtSeconds, NightStart, NightPiece.SecondsPerBeat()));
		TestTrue(TEXT("Como mucho un tiempo de espera"), TensionStart->AtSeconds - Clock < NightPiece.SecondsPerBeat() + 1.0e-9);
		const FMusicLayerTarget* NightStop = FindAction(Out, EMusicLayerAction::Stop, FName(TEXT("mus_night")));
		TestTrue(TEXT("La noche se va deprisa"), NightStop && NightStop->FadeSeconds <= 2.0f);
	});

	It("anula un relevo pendiente si el contexto cambia antes del compás", [this]()
	{
		FMusicDirectorModel Director(MakeCatalog(), 5);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 20.1, DayOn(EIslandArchetype::Landing), Out);
		Out.Reset();
		FMusicContext Night = DayOn(EIslandArchetype::Landing);
		Night.Night = 1.0f;
		Director.Update(Clock, Night, Out);
		const FMusicLayerTarget* NightStart = FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_night")));
		if (!TestNotNull(TEXT("Relevo programado"), NightStart) || !TestTrue(TEXT("Aún pendiente"), NightStart->AtSeconds > Clock + 0.05))
		{
			return;
		}
		const FMusicLayerTarget Pending = *NightStart;
		Out.Reset();
		FMusicContext Sailing = DayOn(EIslandArchetype::Count);
		Sailing.bSailing = true;
		Director.Update(Clock + 0.01, Sailing, Out);
		const FMusicLayerTarget* Cancel = FindAction(Out, EMusicLayerAction::Cancel, FName(TEXT("mus_night")));
		const FMusicLayerTarget* SeaStart = FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_sea")));
		TestTrue(TEXT("Anula la noche pendiente"), Cancel && Cancel->Voice == Pending.Voice);
		TestTrue(TEXT("El mar ocupa el mismo límite"), SeaStart && SeaStart->AtSeconds == Pending.AtSeconds);
		TestEqual(TEXT("Estado"), Director.GetMood(), EMusicMood::Sea);
	});

	It("espacia los motivos de descubrimiento y no repite el mismo seguido", [this]()
	{
		const FMusicDirectorTuning Tuning;
		FMusicDirectorModel Director(MakeCatalog(), 21);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 5.0, DayOn(EIslandArchetype::Teeth), Out);
		const FMusicLayerTarget* Day = FindAction(Out, EMusicLayerAction::Start);
		if (!TestNotNull(TEXT("Suena la exploración"), Day))
		{
			return;
		}
		const FMusicLayerTarget DayStart = *Day;
		const FMusicPiece& DayPiece = Director.GetCatalog().Pieces[Director.GetCatalog().FindById(DayStart.PieceId)];

		Out.Reset();
		TestTrue(TEXT("Primer motivo"), Director.NotifyDiscovery(Clock + 0.1, Out));
		const FMusicLayerTarget* Sting = Out.FindByPredicate([](const FMusicLayerTarget& T)
		{
			return T.Action == EMusicLayerAction::Start && T.PieceId.ToString().StartsWith(TEXT("mus_discovery_"));
		});
		if (!TestNotNull(TEXT("Arranca un motivo"), Sting))
		{
			return;
		}
		const FName FirstSting = Sting->PieceId;
		TestTrue(TEXT("En un tiempo de la pieza"), IsOnGrid(Sting->AtSeconds, DayStart.AtSeconds, DayPiece.SecondsPerBeat()));
		int32 Fades = 0;
		for (const FMusicLayerTarget& T : Out)
		{
			if (T.Action == EMusicLayerAction::Fade && T.Voice == DayStart.Voice)
			{
				++Fades;
				TestTrue(TEXT("Agacha y restaura la exploración"), T.Volume <= DayStart.Volume + 1.0e-6f);
			}
		}
		TestEqual(TEXT("Agachar y restaurar"), Fades, 2);

		Out.Reset();
		TestFalse(TEXT("En enfriamiento"), Director.NotifyDiscovery(Clock + 20.0, Out));
		TestEqual(TEXT("Sin órdenes"), Out.Num(), 0);
		Run(Director, Clock, Tuning.StingCooldownSeconds + 1.0, DayOn(EIslandArchetype::Teeth), Out);
		Out.Reset();
		TestTrue(TEXT("Pasado el enfriamiento"), Director.NotifyDiscovery(Clock, Out));
		const FMusicLayerTarget* Second = Out.FindByPredicate([](const FMusicLayerTarget& T)
		{
			return T.Action == EMusicLayerAction::Start && T.PieceId.ToString().StartsWith(TEXT("mus_discovery_"));
		});
		TestTrue(TEXT("Otro motivo distinto"), Second && Second->PieceId != FirstSting);

		// Con un depredador cerca no hay motivos.
		FMusicContext Danger = DayOn(EIslandArchetype::Teeth);
		Danger.Danger = 1.0f;
		Run(Director, Clock, Tuning.StingCooldownSeconds + 1.0, Danger, Out);
		Out.Reset();
		TestFalse(TEXT("Descartado en tensión"), Director.NotifyDiscovery(Clock, Out));
	});

	It("calla antes del ciclón, suena el temporal y deja calma después", [this]()
	{
		const FMusicDirectorTuning Tuning;
		FMusicDirectorModel Director(MakeCatalog(), 8);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 30.0, DayOn(EIslandArchetype::Smoke), Out);
		const FName Day = Director.GetPrimaryPiece();

		Out.Reset();
		FMusicContext Before = DayOn(EIslandArchetype::Smoke);
		Before.CycloneEtaHours = 3.0f;
		Run(Director, Clock, 60.0, Before, Out);
		TestEqual(TEXT("Silencio antes del ciclón"), Director.GetMood(), EMusicMood::CycloneHush);
		const FMusicLayerTarget* Hush = FindAction(Out, EMusicLayerAction::Stop, Day);
		TestTrue(TEXT("Fundido largo hacia el silencio"), Hush && Hush->FadeSeconds >= 8.0f);
		TestNull(TEXT("Nada arranca en el silencio"), FindAction(Out, EMusicLayerAction::Start));
		TestTrue(TEXT("Ni siquiera los motivos"), !Director.NotifyDiscovery(Clock, Out));
		TestTrue(TEXT("No queda nada sonando"), Director.GetVoices().IsEmpty());

		Out.Reset();
		FMusicContext Cyclone = DayOn(EIslandArchetype::Smoke);
		Cyclone.bCyclone = true;
		Run(Director, Clock, 120.0, Cyclone, Out);
		TestNotNull(TEXT("Llega el ciclón"), FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_storm"))));

		Out.Reset();
		Run(Director, Clock, Tuning.CalmAfterStormSeconds - 5.0, DayOn(EIslandArchetype::Smoke), Out);
		TestNull(TEXT("Calma tras el ciclón"), FindAction(Out, EMusicLayerAction::Start));
		Run(Director, Clock, 10.0, DayOn(EIslandArchetype::Smoke), Out);
		TestNotNull(TEXT("Vuelve la exploración"), FindAction(Out, EMusicLayerAction::Start));
	});

	It("atenúa y filtra la música bajo el agua sin cambiar de pieza", [this]()
	{
		const FMusicDirectorTuning Tuning;
		FMusicDirectorModel Director(MakeCatalog(), 2);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 5.0, DayOn(EIslandArchetype::WhiteSands), Out);
		TestEqual(TEXT("Seco: volumen pleno"), Director.GetBus().Volume, 1.0f);
		TestEqual(TEXT("Seco: sin filtro"), Director.GetBus().LowPassHz, Tuning.DryLowPassHz, 1.0f);
		const FName Piece = Director.GetPrimaryPiece();

		Out.Reset();
		FMusicContext Dive = DayOn(EIslandArchetype::WhiteSands);
		Dive.Underwater = 1.0f;
		Run(Director, Clock, 5.0, Dive, Out);
		TestEqual(TEXT("Sumergido: atenuada"), Director.GetBus().Volume, Tuning.UnderwaterVolume);
		TestEqual(TEXT("Sumergido: filtrada"), Director.GetBus().LowPassHz, Tuning.UnderwaterLowPassHz, 1.0f);
		TestEqual(TEXT("Sin relevos"), Out.Num(), 0);
		TestEqual(TEXT("Misma pieza"), Director.GetPrimaryPiece().ToString(), Piece.ToString());

		Dive.Underwater = 0.5f;
		Director.Update(Clock + 0.1, Dive, Out);
		const FMusicBus& Half = Director.GetBus();
		TestTrue(TEXT("Transición intermedia"), Half.Volume < 1.0f && Half.Volume > Tuning.UnderwaterVolume &&
			Half.LowPassHz < Tuning.DryLowPassHz && Half.LowPassHz > Tuning.UnderwaterLowPassHz);
	});

	It("descansa tras unas vueltas para dejar oír el ambiente", [this]()
	{
		const FMusicDirectorTuning Tuning;
		FMusicDirectorModel Director(MakeCatalog(), 17);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 1.0, DayOn(EIslandArchetype::Mangrove), Out);
		const FMusicLayerTarget First = Out[0];
		const FMusicPiece& Piece = Director.GetCatalog().Pieces[Director.GetCatalog().FindById(First.PieceId)];
		Out.Reset();
		Run(Director, Clock, Piece.Duration() * Tuning.LoopsBeforeRest + 1.0, DayOn(EIslandArchetype::Mangrove), Out);
		const FMusicLayerTarget* Rest = FindAction(Out, EMusicLayerAction::Stop, First.PieceId);
		if (!TestNotNull(TEXT("Se funde para descansar"), Rest))
		{
			return;
		}
		TestEqual(TEXT("Al terminar la segunda vuelta"), Rest->AtSeconds, First.AtSeconds + Piece.Duration() * Tuning.LoopsBeforeRest, 1.0e-6);
		TestNull(TEXT("Silencio"), FindAction(Out, EMusicLayerAction::Start));
		TestTrue(TEXT("Descanso de al menos el mínimo"), Director.GetRestUntil() >= Rest->AtSeconds + Tuning.RestMinSeconds);

		Out.Reset();
		Run(Director, Clock, Tuning.RestFadeOutSeconds + Tuning.RestMaxSeconds + 2.0, DayOn(EIslandArchetype::Mangrove), Out);
		const FMusicLayerTarget* Next = FindAction(Out, EMusicLayerAction::Start);
		TestTrue(TEXT("Vuelve con otra variación"), Next && Next->PieceId != First.PieceId);
	});

	It("no repite la misma variación diurna dos veces seguidas", [this]()
	{
		FMusicDirectorModel Director(MakeCatalog(), 99);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		const EIslandArchetype Route[] = {
			EIslandArchetype::Landing, EIslandArchetype::Landing, EIslandArchetype::Emerald, EIslandArchetype::Teeth,
			EIslandArchetype::Teeth, EIslandArchetype::WhiteSands, EIslandArchetype::Landing, EIslandArchetype::Mesa,
		};
		for (int32 Leg = 0; Leg < 24; ++Leg)
		{
			Run(Director, Clock, 600.0, DayOn(Route[Leg % UE_ARRAY_COUNT(Route)]), Out, 0.5);
		}
		TArray<FName> Played;
		for (const FMusicLayerTarget& T : Out)
		{
			if (T.Action == EMusicLayerAction::Start && T.PieceId.ToString().StartsWith(TEXT("mus_explore_")))
			{
				Played.Add(T.PieceId);
			}
		}
		TestTrue(TEXT("Muchas variaciones en cuatro horas"), Played.Num() > 30);
		TSet<FName> Distinct;
		for (int32 I = 0; I < Played.Num(); ++I)
		{
			Distinct.Add(Played[I]);
			if (I > 0 && Played[I] == Played[I - 1])
			{
				AddError(FString::Printf(TEXT("%s dos veces seguidas en la posición %d"), *Played[I].ToString(), I));
				return;
			}
		}
		TestTrue(TEXT("Recorre varias piezas"), Distinct.Num() >= 5);
	});

	It("es determinista para la misma semilla", [this]()
	{
		auto Script = [](uint32 Seed, TArray<FMusicLayerTarget>& Out)
		{
			FMusicDirectorModel Director(MakeCatalog(), Seed);
			double Clock = 0.0;
			FMusicContext C;
			C.bInMenu = true;
			Run(Director, Clock, 20.0, C, Out);
			for (int32 Hour = 0; Hour < 6; ++Hour)
			{
				C = DayOn(static_cast<EIslandArchetype>(Hour % static_cast<int32>(EIslandArchetype::Count)));
				C.Night = Hour % 3 == 2 ? 1.0f : 0.0f;
				C.Danger = Hour == 4 ? 0.9f : 0.0f;
				C.Underwater = Hour == 1 ? 1.0f : 0.0f;
				for (int32 Minute = 0; Minute < 10; ++Minute)
				{
					Run(Director, Clock, 60.0, C, Out, 0.5);
					Director.NotifyDiscovery(Clock, Out);
				}
			}
		};
		TArray<FMusicLayerTarget> A, B, Other;
		Script(1234, A);
		Script(1234, B);
		Script(4321, Other);
		TestTrue(TEXT("Hay órdenes"), A.Num() > 20);
		TestTrue(TEXT("Misma semilla, mismas órdenes"), A == B);
		TestTrue(TEXT("Otra semilla, otra música"), A != Other);
	});

	It("toca el tema en un momento clave y vuelve a explorar tras una calma", [this]()
	{
		const FMusicDirectorTuning Tuning;
		FMusicDirectorModel Director(MakeCatalog(), 4);
		TArray<FMusicLayerTarget> Out;
		double Clock = 0.0;
		Run(Director, Clock, 10.0, DayOn(EIslandArchetype::Landing), Out);
		Out.Reset();
		Director.NotifyKeyMoment();
		Run(Director, Clock, 5.0, DayOn(EIslandArchetype::Landing), Out);
		TestEqual(TEXT("Momento clave"), Director.GetMood(), EMusicMood::KeyMoment);
		TestNotNull(TEXT("Suena el tema"), FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_theme"))));
		Out.Reset();
		Run(Director, Clock, 130.0, DayOn(EIslandArchetype::Landing), Out);
		TestEqual(TEXT("Vuelve a explorar"), Director.GetMood(), EMusicMood::Explore);
		TestNull(TEXT("El tema no se repite"), FindAction(Out, EMusicLayerAction::Start, FName(TEXT("mus_theme"))));
		Run(Director, Clock, Tuning.CalmAfterKeyMomentSeconds + 1.0, DayOn(EIslandArchetype::Landing), Out);
		TestTrue(TEXT("Tras la calma, exploración"), Director.GetPrimaryPiece().ToString().StartsWith(TEXT("mus_explore_")));
	});

	It("toca cinco notas pentatónicas en las teclas 1–5", [this]()
	{
		const FFluteModel Flute(MakeCatalog().Flute);
		TestEqual(TEXT("Tecla 1"), Flute.NoteForKey(FName(TEXT("One"))), 0);
		TestEqual(TEXT("Tecla 5"), Flute.NoteForKey(FName(TEXT("Five"))), 4);
		TestEqual(TEXT("Tecla ajena"), Flute.NoteForKey(FName(TEXT("Six"))), INDEX_NONE);
		TestEqual(TEXT("Sin tecla"), Flute.NoteForKey(NAME_None), INDEX_NONE);

		const int32 Expected[] = {0, 2, 4, 7, 9};
		for (int32 I = 0; I < FFluteModel::NumNotes; ++I)
		{
			const FFluteNote Note = Flute.Note(I);
			TestEqual(TEXT("Semitonos"), Note.Semitones, Expected[I]);
			TestEqual(TEXT("Tono"), Note.PitchMultiplier, FMath::Pow(2.0f, Expected[I] / 12.0f), 1.0e-5f);
			TestEqual(TEXT("Frecuencia"), Note.FrequencyHz, 293.6648f * Note.PitchMultiplier, 1.0e-2f);
			TestEqual(TEXT("Muestra"), Note.SampleId.ToString(), FString(TEXT("sfx_flute_note")));
		}
		TestEqual(TEXT("La quinta nota es la sexta mayor (B4)"), Flute.Note(4).FrequencyHz, 493.88f, 0.05f);

		FFluteModel Rebound(FFluteTuning{});
		Rebound.SetKeyBindings({FName(TEXT("Z")), FName(TEXT("X")), FName(TEXT("C")), FName(TEXT("V")), FName(TEXT("B"))});
		TestEqual(TEXT("Teclas reasignadas"), Rebound.NoteForKey(FName(TEXT("V"))), 3);
		TestEqual(TEXT("Escala por defecto"), Rebound.Note(3).Semitones, 7);
	});

	It("sube el ánimo al tocar una melodía junto al fuego", [this]()
	{
		FFluteModel Flute(MakeCatalog().Flute);
		TestEqual(TEXT("Sin tocar"), Flute.MoraleRatePerHour(0.0, 1.0f), 0.0f);

		for (int32 I = 0; I < 6; ++I)
		{
			Flute.Play(I * 0.5, 2);
		}
		TestFalse(TEXT("Aporrear una tecla no es tocar"), Flute.IsPerforming(3.0));
		TestEqual(TEXT("Sin ánimo"), Flute.MoraleRatePerHour(3.0, 1.0f), 0.0f);

		const int32 Melody[] = {0, 2, 4, 2, 3, 4};
		for (int32 I = 0; I < 6; ++I)
		{
			Flute.Play(10.0 + I * 0.6, Melody[I]);
		}
		TestTrue(TEXT("Melodía"), Flute.IsPerforming(14.0));
		TestEqual(TEXT("Junto a la hoguera"), Flute.MoraleRatePerHour(14.0, 1.0f), Flute.MaxMoralePerHour);
		TestEqual(TEXT("Lejos del fuego"), Flute.MoraleRatePerHour(14.0, 0.0f), 0.0f);
		const float Embers = Flute.MoraleRatePerHour(14.0, 0.5f);
		TestTrue(TEXT("Brasas: algo"), Embers > 0.0f && Embers < Flute.MaxMoralePerHour);
		TestEqual(TEXT("Deja de tocar"), Flute.MoraleRatePerHour(30.0, 1.0f), 0.0f);
	});
}

#endif
