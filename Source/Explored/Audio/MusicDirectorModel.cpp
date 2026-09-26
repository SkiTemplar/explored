#include "Audio/MusicDirectorModel.h"

#include "Core/ExploredRandom.h"

namespace MusicDirectorDetail
{
	constexpr int32 NumMoods = static_cast<int32>(EMusicMood::Count);

	/** Estados que descansan tras unas vueltas para dejar oír el ambiente. */
	bool MoodRests(EMusicMood Mood)
	{
		return Mood == EMusicMood::Explore || Mood == EMusicMood::Night || Mood == EMusicMood::Sea;
	}

	/** Estados que toman el relevo al instante (sin esperar al compás). */
	bool MoodIsImmediate(EMusicMood Mood)
	{
		return Mood == EMusicMood::Menu || Mood == EMusicMood::Credits || Mood == EMusicMood::Finale;
	}

	/** Estados en los que un motivo de descubrimiento estorbaría. */
	bool MoodBlocksStings(EMusicMood Mood)
	{
		switch (Mood)
		{
		case EMusicMood::Menu:
		case EMusicMood::Credits:
		case EMusicMood::KeyMoment:
		case EMusicMood::Tension:
		case EMusicMood::Storm:
		case EMusicMood::CycloneHush:
		case EMusicMood::Finale:
			return true;
		default:
			return false;
		}
	}

	const TCHAR* FinaleKey(EMusicFinale Finale)
	{
		switch (Finale)
		{
		case EMusicFinale::Rescue: return TEXT("rescue");
		case EMusicFinale::Stay: return TEXT("stay");
		default: return TEXT("voyage");
		}
	}

	/** Claves de isla del JSON, en el orden de EIslandArchetype. */
	const TCHAR* const IslandKeys[static_cast<int32>(EIslandArchetype::Count)] = {
		TEXT("landing"), TEXT("emerald"), TEXT("smoke"), TEXT("teeth"), TEXT("mangrove"), TEXT("whitesands"), TEXT("mesa"),
	};

	/** Claves de papel del JSON, en el orden de EMusicRole. */
	const TCHAR* const RoleKeys[static_cast<int32>(EMusicRole::Count)] = {
		TEXT("theme"), TEXT("menu"), TEXT("explore"), TEXT("night"), TEXT("tension"),
		TEXT("storm"), TEXT("sea"), TEXT("discovery"), TEXT("finale"), TEXT("credits"),
	};

	/** Pentatónica mayor: la escala de la flauta si el JSON no trae otra. */
	constexpr int32 DefaultFluteSemitones[FFluteModel::NumNotes] = {0, 2, 4, 7, 9};

	/** Interpolación logarítmica de frecuencias (un filtro se oye en octavas, no en hercios). */
	float LerpHz(float A, float B, float Alpha)
	{
		return FMath::Exp(FMath::Lerp(FMath::Loge(A), FMath::Loge(B), Alpha));
	}
}

// ---------------------------------------------------------------------------
// Claves y catálogo
// ---------------------------------------------------------------------------

const TCHAR* LexToString(EMusicRole Role)
{
	const int32 I = static_cast<int32>(Role);
	return I >= 0 && I < static_cast<int32>(EMusicRole::Count) ? MusicDirectorDetail::RoleKeys[I] : TEXT("unknown");
}

bool MusicRoleFromKey(const FString& Key, EMusicRole& OutRole)
{
	for (int32 I = 0; I < static_cast<int32>(EMusicRole::Count); ++I)
	{
		if (Key.Equals(MusicDirectorDetail::RoleKeys[I], ESearchCase::IgnoreCase))
		{
			OutRole = static_cast<EMusicRole>(I);
			return true;
		}
	}
	return false;
}

bool IslandArchetypeFromMusicKey(const FString& Key, EIslandArchetype& OutArchetype)
{
	for (int32 I = 0; I < static_cast<int32>(EIslandArchetype::Count); ++I)
	{
		if (Key.Equals(MusicDirectorDetail::IslandKeys[I], ESearchCase::IgnoreCase))
		{
			OutArchetype = static_cast<EIslandArchetype>(I);
			return true;
		}
	}
	return false;
}

const TCHAR* LexToString(EMusicMood Mood)
{
	switch (Mood)
	{
	case EMusicMood::Silence: return TEXT("Silence");
	case EMusicMood::Menu: return TEXT("Menu");
	case EMusicMood::Credits: return TEXT("Credits");
	case EMusicMood::KeyMoment: return TEXT("KeyMoment");
	case EMusicMood::Explore: return TEXT("Explore");
	case EMusicMood::Night: return TEXT("Night");
	case EMusicMood::Sea: return TEXT("Sea");
	case EMusicMood::Tension: return TEXT("Tension");
	case EMusicMood::Storm: return TEXT("Storm");
	case EMusicMood::CycloneHush: return TEXT("CycloneHush");
	case EMusicMood::Finale: return TEXT("Finale");
	default: return TEXT("Unknown");
	}
}

double FMusicPiece::SecondsPerBeat() const
{
	return 60.0 / FMath::Max(1.0, static_cast<double>(Bpm));
}

double FMusicPiece::SecondsPerBar() const
{
	return SecondsPerBeat() * FMath::Max(1, BeatsPerBar);
}

double FMusicPiece::Duration() const
{
	return SecondsPerBar() * FMath::Max(0.0, static_cast<double>(Bars));
}

int32 FMusicCatalog::Add(const FMusicPiece& Piece)
{
	return Pieces.Add(Piece);
}

int32 FMusicCatalog::FindById(const FName& Id) const
{
	return Pieces.IndexOfByPredicate([&Id](const FMusicPiece& Piece) { return Piece.Id == Id; });
}

int32 FMusicCatalog::FindByRole(EMusicRole Role, const FString& Variant) const
{
	return Pieces.IndexOfByPredicate([Role, &Variant](const FMusicPiece& Piece)
	{
		return Piece.Role == Role && (Variant.IsEmpty() || Piece.Variant.Equals(Variant, ESearchCase::IgnoreCase));
	});
}

void FMusicCatalog::PiecesWithRole(EMusicRole Role, TArray<int32>& Out) const
{
	Out.Reset();
	for (int32 I = 0; I < Pieces.Num(); ++I)
	{
		if (Pieces[I].Role == Role)
		{
			Out.Add(I);
		}
	}
}

bool FMusicCatalog::Validate(FString& OutError) const
{
	const EMusicRole Required[] = {
		EMusicRole::Theme, EMusicRole::Menu, EMusicRole::Explore, EMusicRole::Night, EMusicRole::Tension,
		EMusicRole::Storm, EMusicRole::Sea, EMusicRole::Discovery, EMusicRole::Finale,
	};
	for (const EMusicRole Role : Required)
	{
		if (FindByRole(Role) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Falta una pieza con el papel «%s»"), LexToString(Role));
			return false;
		}
	}
	for (const FMusicPiece& Piece : Pieces)
	{
		if (Piece.Id.IsNone() || Piece.Bpm <= 0.0f || Piece.BeatsPerBar <= 0 || Piece.Bars <= 0.0f)
		{
			OutError = FString::Printf(TEXT("Pieza «%s» sin tempo o compases válidos"), *Piece.Id.ToString());
			return false;
		}
	}
	for (const TArray<int32>& Variants : DayVariants)
	{
		for (const int32 Index : Variants)
		{
			if (!Pieces.IsValidIndex(Index) || Pieces[Index].Role != EMusicRole::Explore)
			{
				OutError = TEXT("Variación diurna que no es una pieza de exploración");
				return false;
			}
		}
	}
	return true;
}

bool FMusicLayerTarget::operator==(const FMusicLayerTarget& Other) const
{
	return Voice == Other.Voice && PieceId == Other.PieceId && Action == Other.Action && Volume == Other.Volume &&
		FadeSeconds == Other.FadeSeconds && AtSeconds == Other.AtSeconds && bLoop == Other.bLoop;
}

// ---------------------------------------------------------------------------
// Director
// ---------------------------------------------------------------------------

FMusicDirectorModel::FMusicDirectorModel(const FMusicCatalog& InCatalog, uint32 InSeed, const FMusicDirectorTuning& InTuning)
	: Catalog(InCatalog)
	, Tuning(InTuning)
	, Seed(InSeed)
{
	Bus.Volume = 1.0f;
	Bus.LowPassHz = Tuning.DryLowPassHz;
	Bus.FadeSeconds = Tuning.BusFadeSeconds;
}

EMusicMood FMusicDirectorModel::ChooseMood(const FMusicContext& Context, EMusicMood Previous, const FMusicDirectorTuning& Tuning)
{
	if (Context.bInMenu)
	{
		return EMusicMood::Menu;
	}
	if (Context.bCredits)
	{
		return EMusicMood::Credits;
	}
	if (Context.bFinale)
	{
		return EMusicMood::Finale;
	}
	if (Context.bCyclone)
	{
		return EMusicMood::Storm;
	}
	if (Context.CycloneEtaHours >= 0.0f && Context.CycloneEtaHours <= Tuning.CycloneHushHours)
	{
		return EMusicMood::CycloneHush;
	}

	const float DangerThreshold = Previous == EMusicMood::Tension ? Tuning.DangerExit : Tuning.DangerEnter;
	if (Context.Danger >= DangerThreshold)
	{
		return EMusicMood::Tension;
	}
	const float StormThreshold = Previous == EMusicMood::Storm ? Tuning.StormExit : Tuning.StormEnter;
	if (Context.Storm >= StormThreshold)
	{
		return EMusicMood::Storm;
	}
	if (Context.bSailing)
	{
		return EMusicMood::Sea;
	}
	const float NightThreshold = Previous == EMusicMood::Night ? Tuning.NightExit : Tuning.NightEnter;
	if (Context.Night >= NightThreshold)
	{
		return EMusicMood::Night;
	}
	return EMusicMood::Explore;
}

double FMusicDirectorModel::NextBoundary(double Origin, double Now, double Step)
{
	if (Step <= 0.0 || Now <= Origin)
	{
		return FMath::Max(Origin, Now);
	}
	// Tolerancia de un microsegundo: si Now cae justo en el límite, vale ese límite.
	const double Steps = FMath::CeilToDouble((Now - Origin) / Step - 1.0e-6);
	return Origin + Steps * Step;
}

FMusicVoice* FMusicDirectorModel::FindVoice(int32 VoiceId)
{
	return Voices.FindByPredicate([VoiceId](const FMusicVoice& Voice) { return Voice.Id == VoiceId; });
}

const FMusicVoice* FMusicDirectorModel::FindVoice(int32 VoiceId) const
{
	return Voices.FindByPredicate([VoiceId](const FMusicVoice& Voice) { return Voice.Id == VoiceId; });
}

FName FMusicDirectorModel::GetPrimaryPiece() const
{
	const FMusicVoice* Voice = FindVoice(PrimaryVoice);
	return Voice ? Catalog.Pieces[Voice->Piece].Id : FName(NAME_None);
}

FName FMusicDirectorModel::GetLastDayPiece() const
{
	return Catalog.Pieces.IsValidIndex(LastDayPiece) ? Catalog.Pieces[LastDayPiece].Id : FName(NAME_None);
}

float FMusicDirectorModel::Random01()
{
	const uint32 H = ExploredHash::Hash32(Seed ^ ExploredHash::Hash32(Draws * 0x9E3779B9u + 0x5EEDu));
	++Draws;
	return ExploredHash::ToUnitFloat(H);
}

int32 FMusicDirectorModel::PickAvoiding(const TArray<int32>& Candidates, int32 Avoid, int32 PreferredFirstWeight)
{
	if (Candidates.IsEmpty())
	{
		return INDEX_NONE;
	}
	// Pesos: la primera candidata (la pieza propia de la isla) pesa más, y la última
	// que sonó queda fuera siempre que haya otra.
	TArray<int32> Pool;
	TArray<int32> Weights;
	int32 Total = 0;
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		if (Candidates[I] == Avoid && Candidates.Num() > 1)
		{
			continue;
		}
		const int32 Weight = I == 0 ? FMath::Max(1, PreferredFirstWeight) : 1;
		Pool.Add(Candidates[I]);
		Weights.Add(Weight);
		Total += Weight;
	}
	int32 Roll = FMath::Min(Total - 1, FMath::FloorToInt(Random01() * Total));
	for (int32 I = 0; I < Pool.Num(); ++I)
	{
		Roll -= Weights[I];
		if (Roll < 0)
		{
			return Pool[I];
		}
	}
	return Pool.Last();
}

int32 FMusicDirectorModel::ChooseDayVariation(EIslandArchetype ForIsland)
{
	TArray<int32> Candidates;
	const int32 IslandIndex = static_cast<int32>(ForIsland);
	if (IslandIndex >= 0 && IslandIndex < static_cast<int32>(EIslandArchetype::Count))
	{
		Candidates = Catalog.DayVariants[IslandIndex];
	}
	if (Candidates.IsEmpty())
	{
		Catalog.PiecesWithRole(EMusicRole::Explore, Candidates);
	}
	const int32 Chosen = PickAvoiding(Candidates, LastDayPiece, 2);
	if (Chosen != INDEX_NONE)
	{
		LastDayPiece = Chosen;
	}
	return Chosen;
}

int32 FMusicDirectorModel::ChoosePieceFor(EMusicMood ForMood)
{
	switch (ForMood)
	{
	case EMusicMood::Menu: return Catalog.FindByRole(EMusicRole::Menu);
	case EMusicMood::Credits: return Catalog.FindByRole(EMusicRole::Credits);
	case EMusicMood::KeyMoment: return Catalog.FindByRole(EMusicRole::Theme);
	case EMusicMood::Explore: return ChooseDayVariation(Island);
	case EMusicMood::Night: return Catalog.FindByRole(EMusicRole::Night);
	case EMusicMood::Sea: return Catalog.FindByRole(EMusicRole::Sea);
	case EMusicMood::Tension: return Catalog.FindByRole(EMusicRole::Tension);
	case EMusicMood::Storm: return Catalog.FindByRole(EMusicRole::Storm);
	case EMusicMood::Finale:
	{
		if (bFinalePlayed)
		{
			return INDEX_NONE;
		}
		const int32 Exact = Catalog.FindByRole(EMusicRole::Finale, MusicDirectorDetail::FinaleKey(LastContext.Finale));
		return Exact != INDEX_NONE ? Exact : Catalog.FindByRole(EMusicRole::Finale);
	}
	default:
		return INDEX_NONE;
	}
}

void FMusicDirectorModel::StopVoice(FMusicVoice& Voice, double At, float Fade, TArray<FMusicLayerTarget>& Out)
{
	const FMusicPiece& Piece = Catalog.Pieces[Voice.Piece];
	// Si quedaba una restauración pendiente tras un motivo, se descarta antes de parar.
	if (Voice.DuckRestoreAt > LastNow)
	{
		FMusicLayerTarget Cancel;
		Cancel.Voice = Voice.Id;
		Cancel.PieceId = Piece.Id;
		Cancel.Action = EMusicLayerAction::Cancel;
		Cancel.AtSeconds = LastNow;
		Cancel.bLoop = Piece.bLoop;
		Out.Add(Cancel);
		Voice.DuckRestoreAt = -1.0;
	}
	Voice.StopAt = At;
	Voice.StopFade = Fade;
	Voice.RestAt = -1.0;

	FMusicLayerTarget Stop;
	Stop.Voice = Voice.Id;
	Stop.PieceId = Piece.Id;
	Stop.Action = EMusicLayerAction::Stop;
	Stop.Volume = 0.0f;
	Stop.FadeSeconds = Fade;
	Stop.AtSeconds = At;
	Stop.bLoop = Piece.bLoop;
	Out.Add(Stop);
}

void FMusicDirectorModel::StartPrimary(int32 PieceIndex, double At, TArray<FMusicLayerTarget>& Out)
{
	const FMusicPiece& Piece = Catalog.Pieces[PieceIndex];
	const int32 M = static_cast<int32>(Mood);

	FMusicVoice Voice;
	Voice.Id = NextVoiceId++;
	Voice.Piece = PieceIndex;
	Voice.Mood = Mood;
	Voice.StartAt = At;
	Voice.Volume = Tuning.MoodGain[M];
	if (MusicDirectorDetail::MoodRests(Mood) && Piece.bLoop && Tuning.LoopsBeforeRest > 0)
	{
		Voice.RestAt = At + Piece.Duration() * Tuning.LoopsBeforeRest;
	}
	Voices.Add(Voice);
	PrimaryVoice = Voice.Id;
	PrimaryIsland = Mood == EMusicMood::Explore ? Island : EIslandArchetype::Count;
	RestUntil = -1.0;

	FMusicLayerTarget Start;
	Start.Voice = Voice.Id;
	Start.PieceId = Piece.Id;
	Start.Action = EMusicLayerAction::Start;
	Start.Volume = Voice.Volume;
	Start.FadeSeconds = Tuning.MoodFadeIn[M];
	Start.AtSeconds = At;
	Start.bLoop = Piece.bLoop;
	Out.Add(Start);
}

void FMusicDirectorModel::RetireFinished(double Now)
{
	for (int32 I = Voices.Num() - 1; I >= 0; --I)
	{
		const FMusicVoice& Voice = Voices[I];
		const FMusicPiece& Piece = Catalog.Pieces[Voice.Piece];
		const bool bFadedOut = Voice.StopAt >= 0.0 && Now >= Voice.StopAt + Voice.StopFade;
		const bool bEnded = !Piece.bLoop && Now >= Voice.StartAt + Piece.Duration();
		if (!bFadedOut && !bEnded)
		{
			continue;
		}
		if (Voice.Id == PrimaryVoice)
		{
			PrimaryVoice = INDEX_NONE;
			if (Voice.Mood == EMusicMood::KeyMoment)
			{
				bKeyMomentRequested = false;
			}
			else if (Voice.Mood == EMusicMood::Finale)
			{
				bFinalePlayed = true;
			}
		}
		Voices.RemoveAt(I);
	}
}

void FMusicDirectorModel::TransitionTo(EMusicMood NewMood, double Now, TArray<FMusicLayerTarget>& Out)
{
	const EMusicMood OldMood = Mood;
	double At = Now;

	if (FMusicVoice* Primary = FindVoice(PrimaryVoice))
	{
		if (Primary->StartAt > Now)
		{
			// El relevo anterior aún no había empezado: se anula y el nuevo ocupa su hueco
			// (la pieza saliente ya tiene su fundido en ese mismo límite).
			At = Primary->StartAt;
			FMusicLayerTarget Cancel;
			Cancel.Voice = Primary->Id;
			Cancel.PieceId = Catalog.Pieces[Primary->Piece].Id;
			Cancel.Action = EMusicLayerAction::Cancel;
			Cancel.AtSeconds = Now;
			Cancel.bLoop = Catalog.Pieces[Primary->Piece].bLoop;
			Out.Add(Cancel);
			const int32 PrimaryId = Primary->Id;
			Voices.RemoveAll([PrimaryId](const FMusicVoice& Voice) { return Voice.Id == PrimaryId; });
		}
		else
		{
			const FMusicPiece& Piece = Catalog.Pieces[Primary->Piece];
			const bool bUrgent = NewMood == EMusicMood::Tension || MusicDirectorDetail::MoodIsImmediate(NewMood);
			if (MusicDirectorDetail::MoodIsImmediate(NewMood))
			{
				At = Now;
			}
			else if (NewMood == EMusicMood::Tension)
			{
				At = NextBoundary(Primary->StartAt, Now, Piece.SecondsPerBeat());
			}
			else
			{
				At = NextBoundary(Primary->StartAt, Now, Piece.SecondsPerBar());
			}
			const float Fade = NewMood == EMusicMood::CycloneHush ? Tuning.HushFadeOutSeconds
				: (bUrgent ? Tuning.UrgentFadeOutSeconds : Tuning.FadeOutSeconds);
			StopVoice(*Primary, At, Fade, Out);
		}
	}
	PrimaryVoice = INDEX_NONE;
	PrimaryIsland = EIslandArchetype::Count;

	if (OldMood == EMusicMood::KeyMoment && NewMood != EMusicMood::KeyMoment)
	{
		// Un momento clave interrumpido (peligro, temporal) no se retoma después.
		bKeyMomentRequested = false;
	}
	Mood = NewMood;
	RestUntil = -1.0;

	// Calma antes de volver a explorar: el ambiente respira tras la tensión o el temporal.
	if (MusicDirectorDetail::MoodRests(NewMood))
	{
		float Calm = 0.0f;
		if (OldMood == EMusicMood::Tension)
		{
			Calm = Tuning.CalmAfterTensionSeconds;
		}
		else if (OldMood == EMusicMood::Storm || OldMood == EMusicMood::CycloneHush)
		{
			Calm = Tuning.CalmAfterStormSeconds;
		}
		else if (OldMood == EMusicMood::KeyMoment)
		{
			Calm = Tuning.CalmAfterKeyMomentSeconds;
		}
		if (Calm > 0.0f)
		{
			RestUntil = At + Calm;
			return;
		}
	}

	const int32 PieceIndex = ChoosePieceFor(NewMood);
	if (PieceIndex != INDEX_NONE)
	{
		StartPrimary(PieceIndex, At, Out);
	}
}

void FMusicDirectorModel::MaintainMood(double Now, TArray<FMusicLayerTarget>& Out)
{
	if (FMusicVoice* Primary = FindVoice(PrimaryVoice))
	{
		// Descanso tras unas vueltas: la pieza se funde al terminar una vuelta completa
		// (siempre en un límite de compás) y la música calla un rato.
		if (Primary->RestAt >= 0.0 && Primary->StopAt < 0.0 && Now >= Primary->RestAt - Tuning.RestFadeOutSeconds)
		{
			const FMusicPiece& Piece = Catalog.Pieces[Primary->Piece];
			const double At = NextBoundary(Primary->StartAt, FMath::Max(Now, Primary->RestAt), Piece.SecondsPerBar());
			StopVoice(*Primary, At, Tuning.RestFadeOutSeconds, Out);
			PrimaryVoice = INDEX_NONE;
			PrimaryIsland = EIslandArchetype::Count;
			RestUntil = At + Tuning.RestFadeOutSeconds + FMath::Lerp(Tuning.RestMinSeconds, Tuning.RestMaxSeconds, Random01());
		}
		return;
	}

	if (Now < RestUntil)
	{
		return;
	}
	if (Mood == EMusicMood::KeyMoment && !bKeyMomentRequested)
	{
		return;
	}
	// Las piezas sin bucle del final y los créditos no se repiten al terminar.
	if (Mood == EMusicMood::Finale || Mood == EMusicMood::Credits)
	{
		return;
	}
	const int32 PieceIndex = ChoosePieceFor(Mood);
	if (PieceIndex != INDEX_NONE)
	{
		StartPrimary(PieceIndex, Now, Out);
	}
}

void FMusicDirectorModel::Update(double Now, const FMusicContext& Context, TArray<FMusicLayerTarget>& OutTargets)
{
	Now = FMath::Max(Now, LastNow);
	LastNow = Now;
	LastContext = Context;

	RetireFinished(Now);

	if (Context.Island != EIslandArchetype::Count)
	{
		Island = Context.Island;
	}

	// Bus: bajo el agua la música se atenúa y se apaga en agudos (el ambiente submarino manda).
	const float Under = FMath::Clamp(Context.Underwater, 0.0f, 1.0f);
	Bus.Volume = FMath::Lerp(1.0f, Tuning.UnderwaterVolume, Under);
	Bus.LowPassHz = MusicDirectorDetail::LerpHz(Tuning.DryLowPassHz, Tuning.UnderwaterLowPassHz, Under);
	Bus.FadeSeconds = Tuning.BusFadeSeconds;

	// La tensión se sostiene unos segundos después de que el peligro baje.
	FMusicContext Effective = Context;
	if (Context.Danger >= Tuning.DangerExit)
	{
		LastDangerAt = Now;
	}
	if (Mood == EMusicMood::Tension && Now - LastDangerAt < Tuning.DangerHoldSeconds)
	{
		Effective.Danger = FMath::Max(Effective.Danger, Tuning.DangerEnter);
	}

	const EMusicMood Base = Mood == EMusicMood::KeyMoment ? EMusicMood::Explore : Mood;
	EMusicMood Desired = ChooseMood(Effective, Base, Tuning);
	if (bKeyMomentRequested && (Desired == EMusicMood::Explore || Desired == EMusicMood::Night || Desired == EMusicMood::Sea))
	{
		Desired = EMusicMood::KeyMoment;
	}

	if (Desired != Mood)
	{
		TransitionTo(Desired, Now, OutTargets);
	}
	else if (Mood == EMusicMood::Explore && PrimaryIsland != EIslandArchetype::Count && PrimaryIsland != Island)
	{
		// Otra isla: relevo a su variación diurna en el siguiente compás.
		TransitionTo(EMusicMood::Explore, Now, OutTargets);
	}
	else
	{
		MaintainMood(Now, OutTargets);
	}
}

bool FMusicDirectorModel::NotifyDiscovery(double Now, TArray<FMusicLayerTarget>& OutTargets)
{
	Now = FMath::Max(Now, LastNow);
	if (MusicDirectorDetail::MoodBlocksStings(Mood) || Now - LastStingAt < Tuning.StingCooldownSeconds)
	{
		return false;
	}
	TArray<int32> Stings;
	Catalog.PiecesWithRole(EMusicRole::Discovery, Stings);
	const int32 StingIndex = PickAvoiding(Stings, LastStingPiece, 1);
	if (StingIndex == INDEX_NONE)
	{
		return false;
	}
	LastStingAt = Now;
	LastStingPiece = StingIndex;
	const FMusicPiece& Sting = Catalog.Pieces[StingIndex];

	// En el siguiente tiempo de la pieza que suena (o ya, si calla).
	double At = Now;
	FMusicVoice* Primary = FindVoice(PrimaryVoice);
	if (Primary && Primary->StartAt <= Now && Primary->StopAt < 0.0)
	{
		const FMusicPiece& Piece = Catalog.Pieces[Primary->Piece];
		At = NextBoundary(Primary->StartAt, Now, Piece.SecondsPerBeat());

		FMusicLayerTarget Duck;
		Duck.Voice = Primary->Id;
		Duck.PieceId = Piece.Id;
		Duck.Action = EMusicLayerAction::Fade;
		Duck.Volume = Primary->Volume * Tuning.StingDuck;
		Duck.FadeSeconds = Tuning.StingDuckFadeSeconds;
		Duck.AtSeconds = At;
		Duck.bLoop = Piece.bLoop;
		OutTargets.Add(Duck);

		FMusicLayerTarget Restore = Duck;
		Restore.Volume = Primary->Volume;
		Restore.FadeSeconds = Tuning.StingRestoreFadeSeconds;
		Restore.AtSeconds = At + Sting.Duration();
		OutTargets.Add(Restore);
		Primary->DuckRestoreAt = Restore.AtSeconds;
	}
	else
	{
		Primary = nullptr;
	}

	FMusicVoice Voice;
	Voice.Id = NextVoiceId++;
	Voice.Piece = StingIndex;
	Voice.Mood = Mood;
	Voice.bSting = true;
	Voice.StartAt = At;
	Voice.Volume = Tuning.StingGain;
	Voices.Add(Voice);

	FMusicLayerTarget Start;
	Start.Voice = Voice.Id;
	Start.PieceId = Sting.Id;
	Start.Action = EMusicLayerAction::Start;
	Start.Volume = Voice.Volume;
	Start.FadeSeconds = 0.0f;
	Start.AtSeconds = At;
	Start.bLoop = false;
	OutTargets.Add(Start);
	return true;
}

void FMusicDirectorModel::NotifyKeyMoment()
{
	if (Mood != EMusicMood::Menu && Mood != EMusicMood::Credits && Mood != EMusicMood::Finale)
	{
		bKeyMomentRequested = true;
	}
}

// ---------------------------------------------------------------------------
// Flauta
// ---------------------------------------------------------------------------

FFluteModel::FFluteModel(const FFluteTuning& InTuning)
	: Tuning(InTuning)
{
	if (Tuning.Semitones.Num() != NumNotes)
	{
		Tuning.Semitones.Reset();
		for (const int32 Semitone : MusicDirectorDetail::DefaultFluteSemitones)
		{
			Tuning.Semitones.Add(Semitone);
		}
	}
	if (Tuning.SampleHz <= 0.0f)
	{
		Tuning.SampleHz = FFluteTuning().SampleHz;
	}
	Keys = {FName(TEXT("One")), FName(TEXT("Two")), FName(TEXT("Three")), FName(TEXT("Four")), FName(TEXT("Five"))};
}

void FFluteModel::SetKeyBindings(const TArray<FName>& InKeys)
{
	if (InKeys.Num() == NumNotes)
	{
		Keys = InKeys;
	}
}

int32 FFluteModel::NoteForKey(const FName& KeyName) const
{
	return KeyName.IsNone() ? INDEX_NONE : Keys.Find(KeyName);
}

FFluteNote FFluteModel::Note(int32 Index) const
{
	FFluteNote Out;
	Out.Index = FMath::Clamp(Index, 0, NumNotes - 1);
	Out.Semitones = Tuning.Semitones[Out.Index];
	Out.PitchMultiplier = FMath::Pow(2.0f, Out.Semitones / 12.0f);
	Out.FrequencyHz = Tuning.SampleHz * Out.PitchMultiplier;
	Out.SampleId = Tuning.SampleId;
	return Out;
}

FFluteNote FFluteModel::Play(double Now, int32 Index)
{
	const FFluteNote Played = Note(Index);
	// Solo interesa la ventana reciente: se olvida lo anterior.
	while (RecentTimes.Num() > 0 && (Now - RecentTimes[0] > PerformWindowSeconds || RecentTimes.Num() >= 32))
	{
		RecentTimes.RemoveAt(0);
		RecentNotes.RemoveAt(0);
	}
	RecentTimes.Add(Now);
	RecentNotes.Add(Played.Index);
	return Played;
}

bool FFluteModel::IsPerforming(double Now) const
{
	int32 Count = 0;
	int32 First = INDEX_NONE;
	bool bVaried = false;
	for (int32 I = 0; I < RecentTimes.Num(); ++I)
	{
		if (Now - RecentTimes[I] > PerformWindowSeconds || RecentTimes[I] > Now)
		{
			continue;
		}
		++Count;
		if (First == INDEX_NONE)
		{
			First = RecentNotes[I];
		}
		else if (RecentNotes[I] != First)
		{
			bVaried = true;
		}
	}
	return Count >= MinNotes && bVaried;
}

float FFluteModel::MoraleRatePerHour(double Now, float FireHeat) const
{
	if (!IsPerforming(Now))
	{
		return 0.0f;
	}
	return MaxMoralePerHour * FMath::SmoothStep(0.15f, 0.8f, FMath::Clamp(FireHeat, 0.0f, 1.0f));
}
