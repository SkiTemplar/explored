#pragma once

#include "CoreMinimal.h"

#include "Survival/BodyModel.h"
#include "Survival/SurvivalModel.h"

/**
 * Medicinas de la porción vertical (biblia 03 §3.6): qué hace cada una sobre el
 * cuerpo. La tabla sale de Content/Data/survival_needs.json («medicines»), que
 * Tools/DataCheck convierte en MedicineData.inl; los objetos están en items.json.
 *
 * Red (biblia 08 §2.9): se aplica solo en el servidor, que es quien simula el cuerpo;
 * el cliente pide usar el objeto y ve el resultado en la réplica de su cuerpo.
 */
struct EXPLORED_API FMedicineDef
{
	FName ItemId;
	/** Salud inmediata y estados que cura (bits de ECondition, ver SurvivalCureBit). */
	FConsumable Effects;
	/** Si además trata los cortes abiertos (vendas). */
	bool bTreatsWounds = false;
	EWoundTreatment Treatment = EWoundTreatment::CleanWater;

	bool Cures(ECondition Condition) const { return (Effects.Cures & SurvivalCureBit(Condition)) != 0; }
};

struct EXPLORED_API FMedicineModel
{
	/** Todas las medicinas de los datos, en su orden. */
	static const TArray<FMedicineDef>& All();

	/** La medicina de ese objeto, o nullptr si no lo es. */
	static const FMedicineDef* Find(FName ItemId);

	/**
	 * Usa una medicina: aplica su salud y sus curas (FSurvivalModel::Consume, que para
	 * ContactBurn cicatriza a mitad de tiempo en vez de borrar) y, si es una venda,
	 * trata los cortes. Devuelve false, sin tocar nada, si el objeto no es medicina o si
	 * el cuerpo ya está muerto.
	 */
	static bool Apply(FSurvivalState& State, FName ItemId, TArray<ESurvivalEvent>& OutEvents);
};
