#include "Audio/SoundtrackModel.h"

namespace SoundtrackModelDetail
{
	constexpr int32 NumStates = static_cast<int32>(ESoundtrackState::Count);

	const TCHAR* const StateKeys[NumStates] = {
		TEXT("silence"), TEXT("menu"), TEXT("day"), TEXT("night"), TEXT("cyclone"), TEXT("eye"), TEXT("finale"), TEXT("credits"),
	};
}

const TCHAR* LexToString(ESoundtrackState State)
{
	const int32 Index = static_cast<int32>(State);
	return Index >= 0 && Index < SoundtrackModelDetail::NumStates ? SoundtrackModelDetail::StateKeys[Index] : TEXT("?");
}

bool SoundtrackStateFromKey(const FString& Key, ESoundtrackState& OutState)
{
	for (int32 Index = 0; Index < SoundtrackModelDetail::NumStates; ++Index)
	{
		if (Key.Equals(SoundtrackModelDetail::StateKeys[Index], ESearchCase::IgnoreCase))
		{
			OutState = static_cast<ESoundtrackState>(Index);
			return true;
		}
	}
	return false;
}

int32 FSoundtrackCatalog::FindById(const FName& Id) const
{
	return Pieces.IndexOfByPredicate([&Id](const FSoundtrackPiece& Piece) { return Piece.Id == Id; });
}

bool FSoundtrackCatalog::Validate(FString& OutError) const
{
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		if (Pieces[Index].Id.IsNone() || !FMath::IsFinite(Pieces[Index].Seconds) || Pieces[Index].Seconds <= 0.0f)
		{
			OutError = FString::Printf(TEXT("pieza %d sin id o sin duración"), Index);
			return false;
		}
	}
	for (int32 StateIndex = 1; StateIndex < SoundtrackModelDetail::NumStates; ++StateIndex)
	{
		const FSoundtrackStateConfig& State = States[StateIndex];
		const TCHAR* Key = SoundtrackModelDetail::StateKeys[StateIndex];
		if (State.Pieces.Num() == 0)
		{
			OutError = FString::Printf(TEXT("el estado %s no tiene piezas"), Key);
			return false;
		}
		for (const int32 Piece : State.Pieces)
		{
			if (!Pieces.IsValidIndex(Piece))
			{
				OutError = FString::Printf(TEXT("el estado %s nombra una pieza que no existe"), Key);
				return false;
			}
		}
		const bool bGainOk = FMath::IsFinite(State.Gain) && State.Gain >= 0.0f && State.Gain <= 1.0f;
		const bool bFadesOk = State.FadeIn >= 0.0f && State.FadeOut >= 0.0f;
		const bool bRestOk = State.RestMin >= 0.0f && State.RestMax >= State.RestMin;
		if (!bGainOk || !bFadesOk || !bRestOk)
		{
			OutError = FString::Printf(TEXT("el estado %s tiene volumen, fundidos o silencios fuera de rango"), Key);
			return false;
		}
	}
	return true;
}

FSoundtrackModel::FSoundtrackModel(const FSoundtrackCatalog& InCatalog, uint32 InSeed)
	: Catalog(InCatalog)
	, Seed(InSeed)
{
	// Cada lista empieza en un punto distinto según la semilla: dos partidas no suenan igual al principio.
	for (int32 Index = 0; Index < SoundtrackModelDetail::NumStates; ++Index)
	{
		const int32 Count = Catalog.States[Index].Pieces.Num();
		Cursor[Index] = Count > 1 ? FMath::Min(static_cast<int32>(Random01() * Count), Count - 1) : 0;
	}
}

float FSoundtrackModel::Random01()
{
	// Hash entero (PCG de una ronda): reproducible en cualquier plataforma.
	uint32 X = Seed ^ (0x9E3779B9u * ++Draws);
	X ^= X >> 16;
	X *= 0x7FEB352Du;
	X ^= X >> 15;
	X *= 0x846CA68Bu;
	X ^= X >> 16;
	return static_cast<float>(X & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

ESoundtrackState FSoundtrackModel::ChooseState(const FSoundtrackContext& Context, ESoundtrackState Previous)
{
	if (Context.bInMenu)
	{
		return ESoundtrackState::Menu;
	}
	if (Context.bCredits)
	{
		return ESoundtrackState::Credits;
	}
	if (Context.bFinale)
	{
		return ESoundtrackState::Finale;
	}
	if (Context.bCycloneEye)
	{
		return ESoundtrackState::Eye;
	}
	if (Context.bCycloneWall)
	{
		return ESoundtrackState::Cyclone;
	}
	const float Threshold = Previous == ESoundtrackState::Night ? NightExit : NightEnter;
	return Context.Night >= Threshold ? ESoundtrackState::Night : ESoundtrackState::Day;
}

FName FSoundtrackModel::GetCurrentPiece() const
{
	return CurrentVoice != INDEX_NONE && Catalog.Pieces.IsValidIndex(CurrentPiece) ? Catalog.Pieces[CurrentPiece].Id : NAME_None;
}

void FSoundtrackModel::StopCurrent(float FadeSeconds, TArray<FSoundtrackCommand>& Out)
{
	if (CurrentVoice == INDEX_NONE)
	{
		return;
	}
	FSoundtrackCommand Stop;
	Stop.Voice = CurrentVoice;
	Stop.PieceId = Catalog.Pieces.IsValidIndex(CurrentPiece) ? Catalog.Pieces[CurrentPiece].Id : NAME_None;
	Stop.Action = ESoundtrackAction::Stop;
	Stop.FadeSeconds = FadeSeconds;
	Out.Add(Stop);
	CurrentVoice = INDEX_NONE;
	CurrentPiece = INDEX_NONE;
}

void FSoundtrackModel::StartNext(double Now, TArray<FSoundtrackCommand>& Out)
{
	const int32 StateIndex = static_cast<int32>(State);
	const FSoundtrackStateConfig& Config = Catalog.States[StateIndex];
	if (Config.Pieces.Num() == 0)
	{
		return;
	}
	const int32 Slot = Cursor[StateIndex] % Config.Pieces.Num();
	Cursor[StateIndex] = (Slot + 1) % Config.Pieces.Num();
	CurrentPiece = Config.Pieces[Slot];
	CurrentVoice = NextVoice++;
	PieceEndsAt = Now + Catalog.Pieces[CurrentPiece].Seconds;

	FSoundtrackCommand Start;
	Start.Voice = CurrentVoice;
	Start.PieceId = Catalog.Pieces[CurrentPiece].Id;
	Start.Action = ESoundtrackAction::Start;
	Start.Volume = Config.Gain;
	Start.FadeSeconds = Config.FadeIn;
	Start.bLoop = Config.bLoop;
	Out.Add(Start);
}

void FSoundtrackModel::Update(double Now, const FSoundtrackContext& Context, TArray<FSoundtrackCommand>& Out)
{
	const ESoundtrackState Wanted = ChooseState(Context, State);
	if (Wanted != State)
	{
		// El estado que se va decide su salida; el nuevo entra ya, en fundido cruzado.
		const float FadeOut = State == ESoundtrackState::Silence ? 0.0f : Catalog.Config(State).FadeOut;
		StopCurrent(FadeOut, Out);
		State = Wanted;
		NextStartAt = Now;
	}
	if (State == ESoundtrackState::Silence)
	{
		return;
	}
	const FSoundtrackStateConfig& Config = Catalog.Config(State);
	if (CurrentVoice != INDEX_NONE && !Config.bLoop && Now >= PieceEndsAt - Config.FadeOut)
	{
		// Fin de la pieza: se funde y se deja un silencio antes de la siguiente.
		StopCurrent(Config.FadeOut, Out);
		const float Rest = Config.RestMin + (Config.RestMax - Config.RestMin) * Random01();
		NextStartAt = FMath::Max(Now, PieceEndsAt) + Rest;
	}
	if (CurrentVoice == INDEX_NONE && Now >= NextStartAt)
	{
		StartNext(Now, Out);
	}
}
