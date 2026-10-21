#include "Narrative/BeaconDirectorModel.h"

namespace BeaconDirectorDetail
{
	constexpr double HoursPerDay = 24.0;

	const TCHAR* const StageNames[] = {
		TEXT("Prologo"), TEXT("Orilla"), TEXT("Faro"), TEXT("Balsa"), TEXT("Carguero"),
		TEXT("LenteEnMano"), TEXT("Ciclon"), TEXT("Preparacion"), TEXT("LinternaMontada"), TEXT("Final"),
	};
	static_assert(UE_ARRAY_COUNT(StageNames) == static_cast<int32>(EBeaconStage::Count), "Un nombre por etapa");

	const TCHAR* const ThoughtIds[] = {
		TEXT("pensamiento_primer_movimiento"), TEXT("pensamiento_ver_faro"), TEXT("pensamiento_primera_hoguera"),
		TEXT("pensamiento_entrar_casa"), TEXT("pensamiento_primera_noche"), TEXT("pensamiento_subir_balsa"),
		TEXT("pensamiento_ver_carguero"), TEXT("pensamiento_coger_lente"), TEXT("pensamiento_barometro_bajo"),
		TEXT("pensamiento_ojo_ciclon"), TEXT("pensamiento_montar_linterna"), TEXT("pensamiento_encender"),
	};
	static_assert(UE_ARRAY_COUNT(ThoughtIds) == static_cast<int32>(EIriaThought::Count), "Un texto por pensamiento");

	bool Within(float Meters, float Limit)
	{
		return Meters >= 0.0f && Meters <= Limit;
	}

	uint16 ThoughtBit(EIriaThought Thought)
	{
		return static_cast<uint16>(1u << static_cast<uint32>(Thought));
	}

	bool IsBefore(EBeaconStage Stage, EBeaconStage Target)
	{
		return static_cast<uint8>(Stage) < static_cast<uint8>(Target);
	}

	/** Añade el pensamiento al resultado si no se había mostrado. */
	void Think(FBeaconStepResult& Result, EIriaThought Thought)
	{
		if (Thought >= EIriaThought::Count || (Result.State.ThoughtsShown & ThoughtBit(Thought)) != 0)
		{
			return;
		}
		Result.State.ThoughtsShown |= ThoughtBit(Thought);
		Result.Thoughts.Add(Thought);
	}

	void Record(FBeaconStepResult& Result, EBeaconMilestone Milestone)
	{
		const FBeaconProgressResult Next = FBeaconProgressModel::RecordMilestone(Result.State.Progress, Milestone);
		if (Next.bOk)
		{
			Result.bStageChanged |= Next.State.Stage != Result.State.Progress.Stage;
			Result.State.Progress = Next.State;
		}
	}

	void AdvanceTo(FBeaconStepResult& Result, EBeaconStage Target)
	{
		const FBeaconProgressResult Next = FBeaconProgressModel::Advance(Result.State.Progress, Target);
		if (Next.bOk)
		{
			Result.State.Progress = Next.State;
			Result.bStageChanged = true;
		}
	}

	void ApplyMilestones(FBeaconStepResult& Result, const FBeaconFacts& Facts)
	{
		FBeaconProgressState& Progress = Result.State.Progress;
		if (Facts.bFireLit && !Progress.bFirstFireLit)
		{
			Record(Result, EBeaconMilestone::FirstFireLit);
			Think(Result, EIriaThought::FirstFire);
		}
		if (Facts.bHasCookedFish && !Progress.bFirstFishCooked)
		{
			Record(Result, EBeaconMilestone::FirstFishCooked);
		}
		const int32 Jars = FMath::Min(Facts.OilJarsInPack, FBeaconProgressModel::RequiredOilJars);
		while (Result.State.Progress.OilJars < Jars && !FBeaconProgressModel::IsSlotFilled(Result.State.Progress, ELanternSlot::Oil))
		{
			const int32 Before = Result.State.Progress.OilJars;
			Record(Result, EBeaconMilestone::OilJar);
			if (Result.State.Progress.OilJars == Before)
			{
				break;
			}
		}
		if (Facts.bHasWick && !Result.State.Progress.bWickMade)
		{
			Record(Result, EBeaconMilestone::WickMade);
		}
		if (Facts.bHasLens && !Result.State.Progress.bLensCollected)
		{
			Record(Result, EBeaconMilestone::LensCollected);
			Think(Result, EIriaThought::TakeLens);
		}
	}

	/** Una pasada de las reglas de etapa; devuelve true si ha avanzado. */
	bool AdvanceOnce(FBeaconStepResult& Result, const FBeaconFacts& Facts, const FBeaconDirectorTuning& Tuning)
	{
		const EBeaconStage Stage = Result.State.Progress.Stage;
		const bool bPageOne = FNotebookModel::IsRead(Result.State.Notebook, 1);
		EBeaconStage Target = Stage;
		if (Facts.AbsHours >= Tuning.CycloneEndHour && IsBefore(Stage, EBeaconStage::Preparacion))
		{
			Target = EBeaconStage::Preparacion;
		}
		else if (Facts.AbsHours >= Tuning.CycloneStartHour && IsBefore(Stage, EBeaconStage::Ciclon))
		{
			Target = EBeaconStage::Ciclon;
		}
		else if (Stage == EBeaconStage::Prologo && (Facts.bDrankCoconut || Facts.AbsHours >= Tuning.PrologueEndHour))
		{
			Target = EBeaconStage::Orilla;
		}
		else if (Stage == EBeaconStage::Orilla && (bPageOne || Within(Facts.MetersToTower, Tuning.ReachLighthouseMeters)
			|| Within(Facts.MetersToHouse, Tuning.ReachLighthouseMeters)))
		{
			Target = EBeaconStage::Faro;
		}
		else if (Stage == EBeaconStage::Faro && bPageOne && Facts.AbsHours >= Tuning.RaftDayHour)
		{
			Target = EBeaconStage::Balsa;
		}
		else if (Stage == EBeaconStage::Balsa && Within(Facts.MetersToFreighter, Tuning.OnIsletMeters))
		{
			Target = EBeaconStage::Carguero;
		}
		if (Target == Stage)
		{
			return false;
		}
		AdvanceTo(Result, Target);
		return Result.State.Progress.Stage != Stage;
	}

	void ApplyThoughts(FBeaconStepResult& Result, const FBeaconFacts& Facts, const FBeaconDirectorTuning& Tuning)
	{
		if (Facts.MovedMeters >= Tuning.FirstMoveMeters)
		{
			Think(Result, EIriaThought::FirstMove);
		}
		if (Within(Facts.MetersToTower, Tuning.SeeLighthouseMeters))
		{
			Think(Result, EIriaThought::SeeLighthouse);
		}
		if (Within(Facts.MetersToHouse, Tuning.EnterHouseMeters))
		{
			Think(Result, EIriaThought::EnterHouse);
		}
		if (Within(Facts.MetersToFreighter, Tuning.SeeFreighterMeters))
		{
			Think(Result, EIriaThought::SeeFreighter);
		}
		if (Facts.BarometerHpa > 0.0f && Facts.BarometerHpa < Tuning.BarometerLowHpa)
		{
			Think(Result, EIriaThought::BarometerLow);
		}
		if (Facts.bCycloneEye)
		{
			Think(Result, EIriaThought::CycloneEye);
		}
	}

	FBeaconStepResult Begin(const FBeaconRunState& State)
	{
		FBeaconStepResult Result;
		Result.State = State;
		return Result;
	}
}

bool FBeaconRunState::operator==(const FBeaconRunState& Other) const
{
	return Progress == Other.Progress && Notebook == Other.Notebook && ThoughtsShown == Other.ThoughtsShown
		&& LastAbsHours == Other.LastAbsHours && LitAtHours == Other.LitAtHours && bMistralArrived == Other.bMistralArrived;
}

FBeaconStepResult FBeaconDirectorModel::Step(const FBeaconRunState& State, const FBeaconFacts& Facts, const FBeaconDirectorTuning& Tuning)
{
	using namespace BeaconDirectorDetail;
	FBeaconStepResult Result = Begin(State);
	if (!FMath::IsFinite(Facts.AbsHours))
	{
		return Result;
	}
	ApplyMilestones(Result, Facts);
	for (int32 Pass = 0; Pass < static_cast<int32>(EBeaconStage::Count) && AdvanceOnce(Result, Facts, Tuning); ++Pass)
	{
	}
	ApplyThoughts(Result, Facts, Tuning);

	if (State.LastAbsHours >= 0.0)
	{
		Result.bAutosave = CrossesAutosave(State.LastAbsHours, Facts.AbsHours, Tuning);
	}
	Result.State.LastAbsHours = Facts.AbsHours;

	if (Result.State.Progress.bBeaconLit && !Result.State.bMistralArrived && Result.State.LitAtHours >= 0.0
		&& Facts.AbsHours >= MistralArrivalHours(Result.State.LitAtHours, Tuning))
	{
		Result.State.bMistralArrived = true;
		Result.bMistralDue = true;
	}
	Result.bOk = true;
	return Result;
}

FBeaconStepResult FBeaconDirectorModel::TriggerThought(const FBeaconRunState& State, EIriaThought Thought)
{
	FBeaconStepResult Result = BeaconDirectorDetail::Begin(State);
	BeaconDirectorDetail::Think(Result, Thought);
	Result.bOk = Result.Thoughts.Num() > 0;
	return Result;
}

FBeaconStepResult FBeaconDirectorModel::ReadPage(const FBeaconRunState& State, int32 Page)
{
	using namespace BeaconDirectorDetail;
	FBeaconStepResult Result = Begin(State);
	if (!FNotebookModel::IsValidPage(Page))
	{
		return Result;
	}
	Result.State.Notebook = FNotebookModel::MarkRead(State.Notebook, Page).State;
	if (Page == 1 && IsBefore(Result.State.Progress.Stage, EBeaconStage::Faro))
	{
		AdvanceTo(Result, EBeaconStage::Faro);
	}
	Result.bOk = true;
	return Result;
}

FBeaconStepResult FBeaconDirectorModel::FillSlot(const FBeaconRunState& State, ELanternSlot Slot)
{
	FBeaconStepResult Result = BeaconDirectorDetail::Begin(State);
	const FBeaconProgressResult Next = FBeaconProgressModel::FillSlot(State.Progress, Slot);
	if (!Next.bOk)
	{
		return Result;
	}
	Result.bStageChanged = Next.State.Stage != State.Progress.Stage;
	Result.State.Progress = Next.State;
	if (FBeaconProgressModel::IsLanternMounted(Next.State))
	{
		BeaconDirectorDetail::Think(Result, EIriaThought::MountLantern);
	}
	Result.bOk = true;
	return Result;
}

FBeaconStepResult FBeaconDirectorModel::Light(const FBeaconRunState& State, double AbsHours)
{
	FBeaconStepResult Result = BeaconDirectorDetail::Begin(State);
	const FBeaconProgressResult Next = FBeaconProgressModel::Light(State.Progress);
	if (!Next.bOk || !FMath::IsFinite(AbsHours))
	{
		return Result;
	}
	Result.State.Progress = Next.State;
	Result.State.LitAtHours = FMath::Max(0.0, AbsHours);
	Result.bStageChanged = true;
	BeaconDirectorDetail::Think(Result, EIriaThought::LightBeacon);
	Result.bOk = true;
	return Result;
}

bool FBeaconDirectorModel::IsNight(double AbsHours, const FBeaconDirectorTuning& Tuning)
{
	if (!FMath::IsFinite(AbsHours))
	{
		return false;
	}
	const double Hour = FMath::Fmod(FMath::Max(0.0, AbsHours), BeaconDirectorDetail::HoursPerDay);
	return Hour >= Tuning.NightStartHour || Hour < Tuning.NightEndHour;
}

double FBeaconDirectorModel::MistralArrivalHours(double LitAtHours, const FBeaconDirectorTuning& Tuning)
{
	if (IsNight(LitAtHours, Tuning))
	{
		return LitAtHours;
	}
	const double DayStart = FMath::FloorToDouble(LitAtHours / BeaconDirectorDetail::HoursPerDay) * BeaconDirectorDetail::HoursPerDay;
	return DayStart + Tuning.MistralDayArrivalHour;
}

bool FBeaconDirectorModel::CrossesAutosave(double FromHours, double ToHours, const FBeaconDirectorTuning& Tuning)
{
	if (!FMath::IsFinite(FromHours) || !FMath::IsFinite(ToHours) || ToHours <= FromHours)
	{
		return false;
	}
	for (const double Hour : Tuning.AutosaveHours)
	{
		if (Hour > FromHours && Hour <= ToHours)
		{
			return true;
		}
	}
	return false;
}

bool FBeaconDirectorModel::WasThoughtShown(const FBeaconRunState& State, EIriaThought Thought)
{
	return Thought < EIriaThought::Count && (State.ThoughtsShown & BeaconDirectorDetail::ThoughtBit(Thought)) != 0;
}

FName FBeaconDirectorModel::ThoughtTextId(EIriaThought Thought)
{
	return Thought < EIriaThought::Count ? FName(BeaconDirectorDetail::ThoughtIds[static_cast<int32>(Thought)]) : FName(NAME_None);
}

FBeaconRunState FBeaconDirectorModel::StateForStage(EBeaconStage Stage)
{
	using namespace BeaconDirectorDetail;
	FBeaconRunState Run;
	if (Stage >= EBeaconStage::Count)
	{
		return Run;
	}
	const auto AtLeast = [Stage](EBeaconStage Target) { return !IsBefore(Stage, Target); };
	FBeaconProgressState& P = Run.Progress;
	P.bFirstFireLit = AtLeast(EBeaconStage::Faro);
	P.bFirstFishCooked = AtLeast(EBeaconStage::Faro);
	if (AtLeast(EBeaconStage::Balsa))
	{
		Run.Notebook = FNotebookModel::MarkRead(Run.Notebook, 1).State;
	}
	P.bRaftBuilt = AtLeast(EBeaconStage::Carguero);
	P.bLensCollected = AtLeast(EBeaconStage::LenteEnMano);
	if (AtLeast(EBeaconStage::LinternaMontada))
	{
		P.OilJars = FBeaconProgressModel::RequiredOilJars;
		P.bWickMade = true;
		P.LanternSlots = FBeaconProgressModel::AllSlotsMask;
	}
	P.bBeaconLit = AtLeast(EBeaconStage::Final);
	P.Stage = Stage;
	return Run;
}

bool FBeaconDirectorModel::ParseStage(const FString& Text, EBeaconStage& OutStage)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	bool bDigits = !Trimmed.IsEmpty();
	for (int32 Index = 0; Index < Trimmed.Len() && bDigits; ++Index)
	{
		bDigits = Trimmed[Index] >= TEXT('0') && Trimmed[Index] <= TEXT('9');
	}
	if (bDigits)
	{
		const int32 Index = FCString::Atoi(*Trimmed);
		if (Index >= 0 && Index < static_cast<int32>(EBeaconStage::Count))
		{
			OutStage = static_cast<EBeaconStage>(Index);
			return true;
		}
		return false;
	}
	for (int32 Index = 0; Index < static_cast<int32>(EBeaconStage::Count); ++Index)
	{
		if (Trimmed.Equals(BeaconDirectorDetail::StageNames[Index], ESearchCase::IgnoreCase))
		{
			OutStage = static_cast<EBeaconStage>(Index);
			return true;
		}
	}
	return false;
}

const TCHAR* FBeaconDirectorModel::StageName(EBeaconStage Stage)
{
	return Stage < EBeaconStage::Count ? BeaconDirectorDetail::StageNames[static_cast<int32>(Stage)] : TEXT("?");
}
