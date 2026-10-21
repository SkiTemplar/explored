#include "Narrative/BeaconProgressModel.h"

namespace BeaconDetail
{
	constexpr uint8 FlagFirstFire = 1u << 0;
	constexpr uint8 FlagFirstFish = 1u << 1;
	constexpr uint8 FlagRaft = 1u << 2;
	constexpr uint8 FlagLens = 1u << 3;
	constexpr uint8 FlagWick = 1u << 4;
	constexpr uint8 FlagLit = 1u << 5;
	constexpr uint8 KnownFlags = FlagFirstFire | FlagFirstFish | FlagRaft | FlagLens | FlagWick | FlagLit;

	uint8 SlotBit(ELanternSlot Slot)
	{
		return static_cast<uint8>(1u << static_cast<uint32>(Slot));
	}

	bool IsValidSlot(ELanternSlot Slot)
	{
		return static_cast<uint8>(Slot) < static_cast<uint8>(ELanternSlot::Count);
	}

	bool IsValidStage(EBeaconStage Stage)
	{
		return static_cast<uint8>(Stage) < static_cast<uint8>(EBeaconStage::Count);
	}

	/** La etapa más avanzada de las dos. */
	EBeaconStage Later(EBeaconStage A, EBeaconStage B)
	{
		return static_cast<uint8>(A) >= static_cast<uint8>(B) ? A : B;
	}

	FBeaconProgressResult Ok(const FBeaconProgressState& State)
	{
		FBeaconProgressResult Result;
		Result.bOk = true;
		Result.State = State;
		return Result;
	}

	FBeaconProgressResult Rejected(const FBeaconProgressState& State)
	{
		FBeaconProgressResult Result;
		Result.State = State;
		return Result;
	}

	/** Lo que tiene que haber hecho Iria para poner algo en cada ranura. */
	bool HasItemForSlot(const FBeaconProgressState& State, ELanternSlot Slot)
	{
		switch (Slot)
		{
		case ELanternSlot::Lens:
			return State.bLensCollected;
		case ELanternSlot::Oil:
			return State.OilJars >= FBeaconProgressModel::RequiredOilJars;
		case ELanternSlot::Wick:
			return State.bWickMade;
		default:
			return false;
		}
	}

	FName PreparationObjective(const FBeaconProgressState& State)
	{
		if (!State.bLensCollected)
		{
			return FName(TEXT("obj_recupera_lente"));
		}
		const bool bOilReady = State.OilJars >= FBeaconProgressModel::RequiredOilJars
			|| FBeaconProgressModel::IsSlotFilled(State, ELanternSlot::Oil);
		if (!bOilReady)
		{
			return FName(TEXT("obj_prepara_aceite"));
		}
		if (!State.bWickMade && !FBeaconProgressModel::IsSlotFilled(State, ELanternSlot::Wick))
		{
			return FName(TEXT("obj_trenza_mecha"));
		}
		return FName(TEXT("obj_monta_linterna"));
	}
}

bool FBeaconProgressState::operator==(const FBeaconProgressState& Other) const
{
	return Stage == Other.Stage && bFirstFireLit == Other.bFirstFireLit && bFirstFishCooked == Other.bFirstFishCooked
		&& bRaftBuilt == Other.bRaftBuilt && bLensCollected == Other.bLensCollected && bWickMade == Other.bWickMade
		&& bBeaconLit == Other.bBeaconLit && OilJars == Other.OilJars && LanternSlots == Other.LanternSlots;
}

bool FBeaconProgressModel::MeetsStageRequirement(const FBeaconProgressState& State, EBeaconStage Stage)
{
	switch (Stage)
	{
	case EBeaconStage::LenteEnMano:
		return State.bLensCollected;
	case EBeaconStage::LinternaMontada:
		return IsLanternMounted(State);
	case EBeaconStage::Final:
		return State.bBeaconLit;
	default:
		return BeaconDetail::IsValidStage(Stage);
	}
}

bool FBeaconProgressModel::CanAdvance(const FBeaconProgressState& State, EBeaconStage Target)
{
	if (!BeaconDetail::IsValidStage(Target) || static_cast<uint8>(Target) <= static_cast<uint8>(State.Stage))
	{
		return false;
	}
	return MeetsStageRequirement(State, Target);
}

FBeaconProgressResult FBeaconProgressModel::Advance(const FBeaconProgressState& State, EBeaconStage Target)
{
	if (!CanAdvance(State, Target))
	{
		return BeaconDetail::Rejected(State);
	}
	FBeaconProgressState Next = State;
	Next.Stage = Target;
	return BeaconDetail::Ok(Next);
}

FBeaconProgressResult FBeaconProgressModel::RecordMilestone(const FBeaconProgressState& State, EBeaconMilestone Milestone)
{
	FBeaconProgressState Next = State;
	bool* Flag = nullptr;
	switch (Milestone)
	{
	case EBeaconMilestone::FirstFireLit:
		Flag = &Next.bFirstFireLit;
		break;
	case EBeaconMilestone::FirstFishCooked:
		Flag = &Next.bFirstFishCooked;
		break;
	case EBeaconMilestone::RaftBuilt:
		Flag = &Next.bRaftBuilt;
		break;
	case EBeaconMilestone::LensCollected:
		Flag = &Next.bLensCollected;
		break;
	case EBeaconMilestone::WickMade:
		Flag = &Next.bWickMade;
		break;
	case EBeaconMilestone::OilJar:
		if (State.OilJars >= RequiredOilJars)
		{
			return BeaconDetail::Rejected(State);
		}
		Next.OilJars = FMath::Max(State.OilJars, 0) + 1;
		return BeaconDetail::Ok(Next);
	default:
		return BeaconDetail::Rejected(State);
	}

	if (*Flag)
	{
		return BeaconDetail::Rejected(State);
	}
	*Flag = true;
	if (Milestone == EBeaconMilestone::LensCollected)
	{
		Next.Stage = BeaconDetail::Later(Next.Stage, EBeaconStage::LenteEnMano);
	}
	return BeaconDetail::Ok(Next);
}

bool FBeaconProgressModel::IsSlotFilled(const FBeaconProgressState& State, ELanternSlot Slot)
{
	return BeaconDetail::IsValidSlot(Slot) && (State.LanternSlots & BeaconDetail::SlotBit(Slot)) != 0;
}

FBeaconProgressResult FBeaconProgressModel::FillSlot(const FBeaconProgressState& State, ELanternSlot Slot)
{
	if (!BeaconDetail::IsValidSlot(Slot) || IsSlotFilled(State, Slot) || !BeaconDetail::HasItemForSlot(State, Slot))
	{
		return BeaconDetail::Rejected(State);
	}
	FBeaconProgressState Next = State;
	Next.LanternSlots = static_cast<uint8>(State.LanternSlots | BeaconDetail::SlotBit(Slot));
	if (IsLanternMounted(Next))
	{
		Next.Stage = BeaconDetail::Later(Next.Stage, EBeaconStage::LinternaMontada);
	}
	return BeaconDetail::Ok(Next);
}

bool FBeaconProgressModel::IsLanternMounted(const FBeaconProgressState& State)
{
	return (State.LanternSlots & AllSlotsMask) == AllSlotsMask;
}

FBeaconProgressResult FBeaconProgressModel::Light(const FBeaconProgressState& State)
{
	if (State.bBeaconLit || !IsLanternMounted(State))
	{
		return BeaconDetail::Rejected(State);
	}
	FBeaconProgressState Next = State;
	Next.bBeaconLit = true;
	Next.Stage = EBeaconStage::Final;
	return BeaconDetail::Ok(Next);
}

FName FBeaconProgressModel::CurrentObjective(const FBeaconProgressState& State, const FBeaconObjectiveContext& Context)
{
	switch (State.Stage)
	{
	case EBeaconStage::Prologo:
		return FName(TEXT("obj_bebe_coco"));
	case EBeaconStage::Orilla:
		if (!State.bFirstFireLit)
		{
			return FName(TEXT("obj_haz_fuego"));
		}
		return State.bFirstFishCooked ? FName(TEXT("obj_sube_faro")) : FName(TEXT("obj_pesca_cocina"));
	case EBeaconStage::Faro:
		return Context.bPageOneRead ? FName(TEXT("obj_duerme_refugio")) : FName(TEXT("obj_lee_cuaderno"));
	case EBeaconStage::Balsa:
		return State.bRaftBuilt ? FName(TEXT("obj_cruza_bajo")) : FName(TEXT("obj_prepara_balsa"));
	case EBeaconStage::Carguero:
		return FName(TEXT("obj_encuentra_lente"));
	case EBeaconStage::LenteEnMano:
		return FName(TEXT("obj_vuelve_antes_cierre"));
	case EBeaconStage::Ciclon:
		return Context.bSheltered ? FName(TEXT("obj_sobrevive_noche")) : FName(TEXT("obj_refugiate_faro"));
	case EBeaconStage::Preparacion:
		return BeaconDetail::PreparationObjective(State);
	case EBeaconStage::LinternaMontada:
		return FName(TEXT("obj_enciende_baliza"));
	default:
		return FName(NAME_None);
	}
}

EBeaconEpilogue FBeaconProgressModel::EpilogueFor(const FNotebookState& Notebook)
{
	return FNotebookModel::IsComplete(Notebook) ? EBeaconEpilogue::Complete : EBeaconEpilogue::Partial;
}

FName FBeaconProgressModel::EpilogueTextId(EBeaconEpilogue Epilogue)
{
	return Epilogue == EBeaconEpilogue::Complete ? FName(TEXT("epilogo_completo")) : FName(TEXT("epilogo_parcial"));
}

FName FBeaconProgressModel::CommonEpilogueTextId()
{
	return FName(TEXT("epilogo_comun"));
}

FBeaconProgressRecord FBeaconProgressModel::ToRecord(const FBeaconProgressState& State)
{
	using namespace BeaconDetail;
	FBeaconProgressRecord Record;
	Record.Stage = static_cast<uint8>(State.Stage);
	Record.Flags = static_cast<uint8>((State.bFirstFireLit ? FlagFirstFire : 0) | (State.bFirstFishCooked ? FlagFirstFish : 0)
		| (State.bRaftBuilt ? FlagRaft : 0) | (State.bLensCollected ? FlagLens : 0) | (State.bWickMade ? FlagWick : 0)
		| (State.bBeaconLit ? FlagLit : 0));
	Record.OilJars = static_cast<uint8>(FMath::Clamp(State.OilJars, 0, RequiredOilJars));
	Record.LanternSlots = static_cast<uint8>(State.LanternSlots & AllSlotsMask);
	return Record;
}

FBeaconProgressResult FBeaconProgressModel::FromRecord(const FBeaconProgressRecord& Record)
{
	using namespace BeaconDetail;
	const FBeaconProgressState Fresh;
	const bool bInRange = Record.Stage < static_cast<uint8>(EBeaconStage::Count) && (Record.Flags & ~KnownFlags) == 0
		&& Record.OilJars <= RequiredOilJars && (Record.LanternSlots & ~AllSlotsMask) == 0;
	if (!bInRange)
	{
		return Rejected(Fresh);
	}

	FBeaconProgressState State;
	State.Stage = static_cast<EBeaconStage>(Record.Stage);
	State.bFirstFireLit = (Record.Flags & FlagFirstFire) != 0;
	State.bFirstFishCooked = (Record.Flags & FlagFirstFish) != 0;
	State.bRaftBuilt = (Record.Flags & FlagRaft) != 0;
	State.bLensCollected = (Record.Flags & FlagLens) != 0;
	State.bWickMade = (Record.Flags & FlagWick) != 0;
	State.bBeaconLit = (Record.Flags & FlagLit) != 0;
	State.OilJars = Record.OilJars;
	State.LanternSlots = Record.LanternSlots;

	for (uint8 Slot = 0; Slot < static_cast<uint8>(ELanternSlot::Count); ++Slot)
	{
		const ELanternSlot AsSlot = static_cast<ELanternSlot>(Slot);
		if (IsSlotFilled(State, AsSlot) && !HasItemForSlot(State, AsSlot))
		{
			return Rejected(Fresh);
		}
	}
	const bool bLitConsistent = !State.bBeaconLit || (IsLanternMounted(State) && State.Stage == EBeaconStage::Final);
	const bool bMountedStage = static_cast<uint8>(State.Stage) < static_cast<uint8>(EBeaconStage::LinternaMontada)
		|| IsLanternMounted(State);
	if (!bLitConsistent || !bMountedStage || !MeetsStageRequirement(State, State.Stage))
	{
		return Rejected(Fresh);
	}
	return Ok(State);
}
