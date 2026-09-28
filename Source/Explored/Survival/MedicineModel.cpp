#include "Survival/MedicineModel.h"

namespace MedicineModelDetail
{
	void FillDefaultMedicines(TArray<FMedicineDef>& D)
	{
#include "Survival/MedicineData.inl"
	}
}

const TArray<FMedicineDef>& FMedicineModel::All()
{
	static const TArray<FMedicineDef> Medicines = []()
	{
		TArray<FMedicineDef> D;
		MedicineModelDetail::FillDefaultMedicines(D);
		return D;
	}();
	return Medicines;
}

const FMedicineDef* FMedicineModel::Find(FName ItemId)
{
	return All().FindByPredicate([ItemId](const FMedicineDef& M) { return M.ItemId == ItemId; });
}

bool FMedicineModel::Apply(FSurvivalState& State, FName ItemId, TArray<ESurvivalEvent>& OutEvents)
{
	const FMedicineDef* Medicine = Find(ItemId);
	if (!Medicine || State.IsDead())
	{
		return false;
	}
	// Las medicinas no son tóxicas: la tirada no decide nada.
	FSurvivalModel::Consume(State, Medicine->Effects, 1.0f, OutEvents);
	if (Medicine->bTreatsWounds)
	{
		FBodyModel::TreatWounds(State, Medicine->Treatment);
	}
	return true;
}
