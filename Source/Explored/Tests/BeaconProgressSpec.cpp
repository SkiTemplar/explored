#include "Misc/AutomationTest.h"

#include "Narrative/BeaconProgressModel.h"
#include "Narrative/NotebookModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BeaconTest
{
	using FModel = FBeaconProgressModel;

	FBeaconProgressState Must(const FBeaconProgressResult& Result)
	{
		check(Result.bOk);
		return Result.State;
	}

	/** Estado legal en la etapa pedida, recorrido por el camino del GDD. */
	FBeaconProgressState ReachStage(EBeaconStage Target)
	{
		FBeaconProgressState S;
		for (uint8 Raw = 1; Raw <= static_cast<uint8>(Target); ++Raw)
		{
			const EBeaconStage Stage = static_cast<EBeaconStage>(Raw);
			switch (Stage)
			{
			case EBeaconStage::Orilla:
				S = Must(FModel::Advance(S, Stage));
				S = Must(FModel::RecordMilestone(S, EBeaconMilestone::FirstFireLit));
				break;
			case EBeaconStage::Balsa:
				S = Must(FModel::Advance(S, Stage));
				S = Must(FModel::RecordMilestone(S, EBeaconMilestone::RaftBuilt));
				break;
			case EBeaconStage::LenteEnMano:
				S = Must(FModel::RecordMilestone(S, EBeaconMilestone::LensCollected));
				break;
			case EBeaconStage::Preparacion:
				S = Must(FModel::Advance(S, Stage));
				S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
				break;
			case EBeaconStage::LinternaMontada:
				S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
				S = Must(FModel::RecordMilestone(S, EBeaconMilestone::WickMade));
				S = Must(FModel::FillSlot(S, ELanternSlot::Lens));
				S = Must(FModel::FillSlot(S, ELanternSlot::Oil));
				S = Must(FModel::FillSlot(S, ELanternSlot::Wick));
				break;
			case EBeaconStage::Final:
				S = Must(FModel::Light(S));
				break;
			default:
				S = Must(FModel::Advance(S, Stage));
				break;
			}
		}
		return S;
	}

	FBeaconProgressState ReadyToMount()
	{
		FBeaconProgressState S = ReachStage(EBeaconStage::Preparacion);
		S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
		return Must(FModel::RecordMilestone(S, EBeaconMilestone::WickMade));
	}

	bool SameRecord(const FBeaconProgressRecord& A, const FBeaconProgressRecord& B)
	{
		return A.Stage == B.Stage && A.Flags == B.Flags && A.OilJars == B.OilJars && A.LanternSlots == B.LanternSlots;
	}

	FBeaconProgressRecord Record(uint8 Stage, uint8 Flags, uint8 Oil, uint8 Slots)
	{
		FBeaconProgressRecord R;
		R.Stage = Stage;
		R.Flags = Flags;
		R.OilJars = Oil;
		R.LanternSlots = Slots;
		return R;
	}
}

BEGIN_DEFINE_SPEC(FBeaconProgressSpec, "Explored.BeaconProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBeaconProgressSpec)

void FBeaconProgressSpec::Define()
{
	using namespace BeaconTest;

	Describe(TEXT("etapas"), [this]()
	{
		It(TEXT("empieza en el prólogo sin hitos"), [this]()
		{
			const FBeaconProgressState S;
			TestEqual(TEXT("Etapa"), S.Stage, EBeaconStage::Prologo);
			TestEqual(TEXT("Tarros"), S.OilJars, 0);
			TestEqual(TEXT("Ranuras"), S.LanternSlots, static_cast<uint8>(0));
			TestFalse(TEXT("Sin lente"), S.bLensCollected);
		});

		It(TEXT("avanza una etapa y no toca el estado de entrada"), [this]()
		{
			const FBeaconProgressState S;
			const FBeaconProgressResult R = FModel::Advance(S, EBeaconStage::Orilla);
			TestTrue(TEXT("Válida"), R.bOk);
			TestEqual(TEXT("Nueva etapa"), R.State.Stage, EBeaconStage::Orilla);
			TestEqual(TEXT("Entrada intacta"), S.Stage, EBeaconStage::Prologo);
		});

		It(TEXT("se pueden saltar etapas sin requisito (no bloquea sin leer la página 1)"), [this]()
		{
			const FBeaconProgressState S = ReachStage(EBeaconStage::Faro);
			TestTrue(TEXT("Faro → Ciclón sin lente"), FModel::Advance(S, EBeaconStage::Ciclon).bOk);
			TestTrue(TEXT("Faro → Preparación"), FModel::Advance(S, EBeaconStage::Preparacion).bOk);
		});

		It(TEXT("rechaza volver atrás, repetir etapa o salirse del rango"), [this]()
		{
			const FBeaconProgressState S = ReachStage(EBeaconStage::Carguero);
			const EBeaconStage Bad[] = {EBeaconStage::Balsa, EBeaconStage::Prologo, EBeaconStage::Carguero, EBeaconStage::Count,
				static_cast<EBeaconStage>(200)};
			for (const EBeaconStage Target : Bad)
			{
				const FBeaconProgressResult R = FModel::Advance(S, Target);
				TestFalse(FString::Printf(TEXT("Rechaza %d"), static_cast<int32>(Target)), R.bOk);
				TestTrue(TEXT("Estado igual"), R.State == S);
			}
		});

		It(TEXT("rechaza las etapas con requisito si falta su hito"), [this]()
		{
			const FBeaconProgressState S = ReachStage(EBeaconStage::Carguero);
			TestFalse(TEXT("LenteEnMano sin lente"), FModel::CanAdvance(S, EBeaconStage::LenteEnMano));
			TestFalse(TEXT("LinternaMontada sin ranuras"), FModel::CanAdvance(S, EBeaconStage::LinternaMontada));
			TestFalse(TEXT("Final sin encender"), FModel::CanAdvance(S, EBeaconStage::Final));
			const FBeaconProgressState Mounted = ReachStage(EBeaconStage::LinternaMontada);
			TestFalse(TEXT("Final con la linterna montada pero apagada"), FModel::Advance(Mounted, EBeaconStage::Final).bOk);
		});

		It(TEXT("el camino del GDD recorre las 10 etapas en orden"), [this]()
		{
			for (uint8 Raw = 0; Raw < static_cast<uint8>(EBeaconStage::Count); ++Raw)
			{
				TestEqual(TEXT("Etapa alcanzada"), ReachStage(static_cast<EBeaconStage>(Raw)).Stage, static_cast<EBeaconStage>(Raw));
			}
		});
	});

	Describe(TEXT("hitos"), [this]()
	{
		It(TEXT("recoger la lente lleva a LenteEnMano"), [this]()
		{
			const FBeaconProgressState S = ReachStage(EBeaconStage::Carguero);
			const FBeaconProgressResult R = FModel::RecordMilestone(S, EBeaconMilestone::LensCollected);
			TestTrue(TEXT("Registrada"), R.bOk);
			TestEqual(TEXT("Etapa"), R.State.Stage, EBeaconStage::LenteEnMano);
		});

		It(TEXT("la lente recogida más tarde no hace retroceder la etapa"), [this]()
		{
			const FBeaconProgressState S = ReachStage(EBeaconStage::Faro);
			const FBeaconProgressState Late = Must(FModel::Advance(S, EBeaconStage::Preparacion));
			TestEqual(TEXT("Sigue en Preparación"), Must(FModel::RecordMilestone(Late, EBeaconMilestone::LensCollected)).Stage,
				EBeaconStage::Preparacion);
		});

		It(TEXT("cada hito se registra una sola vez"), [this]()
		{
			const EBeaconMilestone Once[] = {EBeaconMilestone::FirstFireLit, EBeaconMilestone::FirstFishCooked, EBeaconMilestone::RaftBuilt,
				EBeaconMilestone::LensCollected, EBeaconMilestone::WickMade};
			for (const EBeaconMilestone M : Once)
			{
				const FBeaconProgressState S = Must(FModel::RecordMilestone(FBeaconProgressState(), M));
				const FBeaconProgressResult Again = FModel::RecordMilestone(S, M);
				TestFalse(TEXT("Segunda vez rechazada"), Again.bOk);
				TestTrue(TEXT("Estado igual"), Again.State == S);
			}
			TestFalse(TEXT("Hito fuera de rango"), FModel::RecordMilestone(FBeaconProgressState(), EBeaconMilestone::Count).bOk);
		});

		It(TEXT("cuenta dos tarros y rechaza el tercero"), [this]()
		{
			FBeaconProgressState S;
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
			TestEqual(TEXT("Dos tarros"), S.OilJars, 2);
			TestFalse(TEXT("El tercero no cuenta"), FModel::RecordMilestone(S, EBeaconMilestone::OilJar).bOk);
		});
	});

	Describe(TEXT("linterna"), [this]()
	{
		It(TEXT("no acepta una ranura sin su objeto"), [this]()
		{
			FBeaconProgressState S = ReachStage(EBeaconStage::Faro);
			TestFalse(TEXT("Lente sin recoger"), FModel::FillSlot(S, ELanternSlot::Lens).bOk);
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
			TestFalse(TEXT("Aceite con un solo tarro"), FModel::FillSlot(S, ELanternSlot::Oil).bOk);
			TestFalse(TEXT("Mecha sin hacer"), FModel::FillSlot(S, ELanternSlot::Wick).bOk);
			TestFalse(TEXT("Ranura fuera de rango"), FModel::FillSlot(S, ELanternSlot::Count).bOk);
		});

		It(TEXT("no llena dos veces la misma ranura"), [this]()
		{
			const FBeaconProgressState S = Must(FModel::FillSlot(ReadyToMount(), ELanternSlot::Wick));
			const FBeaconProgressResult Again = FModel::FillSlot(S, ELanternSlot::Wick);
			TestFalse(TEXT("Rechazada"), Again.bOk);
			TestTrue(TEXT("Estado igual"), Again.State == S);
		});

		It(TEXT("la tercera ranura, en cualquier orden, monta la linterna"), [this]()
		{
			FBeaconProgressState S = ReadyToMount();
			S = Must(FModel::FillSlot(S, ELanternSlot::Wick));
			S = Must(FModel::FillSlot(S, ELanternSlot::Lens));
			TestFalse(TEXT("Con dos aún no"), FModel::IsLanternMounted(S));
			TestEqual(TEXT("Sigue en Preparación"), S.Stage, EBeaconStage::Preparacion);
			S = Must(FModel::FillSlot(S, ELanternSlot::Oil));
			TestTrue(TEXT("Montada"), FModel::IsLanternMounted(S));
			TestEqual(TEXT("Etapa"), S.Stage, EBeaconStage::LinternaMontada);
		});

		It(TEXT("encender exige la linterna montada y solo se hace una vez"), [this]()
		{
			const FBeaconProgressState NotMounted = Must(FModel::FillSlot(ReadyToMount(), ELanternSlot::Lens));
			TestFalse(TEXT("Sin montar"), FModel::Light(NotMounted).bOk);
			const FBeaconProgressState Lit = Must(FModel::Light(ReachStage(EBeaconStage::LinternaMontada)));
			TestTrue(TEXT("Encendida"), Lit.bBeaconLit);
			TestEqual(TEXT("Final"), Lit.Stage, EBeaconStage::Final);
			TestFalse(TEXT("Segunda vez"), FModel::Light(Lit).bOk);
		});
	});

	Describe(TEXT("objetivo"), [this]()
	{
		It(TEXT("cada etapa da su objetivo y el Final ninguno"), [this]()
		{
			const FBeaconObjectiveContext Ctx;
			const FName Expected[] = {TEXT("obj_bebe_coco"), TEXT("obj_pesca_cocina"), TEXT("obj_lee_cuaderno"), TEXT("obj_cruza_bajo"),
				TEXT("obj_encuentra_lente"), TEXT("obj_vuelve_antes_cierre"), TEXT("obj_refugiate_faro"), TEXT("obj_prepara_aceite"),
				TEXT("obj_enciende_baliza")};
			for (uint8 Raw = 0; Raw < UE_ARRAY_COUNT(Expected); ++Raw)
			{
				TestEqual(FString::Printf(TEXT("Etapa %d"), Raw), FModel::CurrentObjective(ReachStage(static_cast<EBeaconStage>(Raw)), Ctx),
					Expected[Raw]);
			}
			TestTrue(TEXT("Final sin objetivo"), FModel::CurrentObjective(ReachStage(EBeaconStage::Final), Ctx) == NAME_None);
		});

		It(TEXT("el día 1 encadena fuego, pez y subir al faro"), [this]()
		{
			const FBeaconObjectiveContext Ctx;
			FBeaconProgressState S = Must(FModel::Advance(FBeaconProgressState(), EBeaconStage::Orilla));
			TestEqual(TEXT("Fuego"), FModel::CurrentObjective(S, Ctx), FName(TEXT("obj_haz_fuego")));
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::FirstFireLit));
			TestEqual(TEXT("Pez"), FModel::CurrentObjective(S, Ctx), FName(TEXT("obj_pesca_cocina")));
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::FirstFishCooked));
			TestEqual(TEXT("Faro"), FModel::CurrentObjective(S, Ctx), FName(TEXT("obj_sube_faro")));
		});

		It(TEXT("el contexto cambia el objetivo del faro y del ciclón"), [this]()
		{
			FBeaconObjectiveContext Ctx;
			Ctx.bPageOneRead = true;
			Ctx.bSheltered = true;
			TestEqual(TEXT("Página leída: dormir"), FModel::CurrentObjective(ReachStage(EBeaconStage::Faro), Ctx),
				FName(TEXT("obj_duerme_refugio")));
			TestEqual(TEXT("A cubierto: aguantar"), FModel::CurrentObjective(ReachStage(EBeaconStage::Ciclon), Ctx),
				FName(TEXT("obj_sobrevive_noche")));
		});

		It(TEXT("la preparación pide lo que falte: lente, aceite, mecha y montar"), [this]()
		{
			const FBeaconObjectiveContext Ctx;
			const FBeaconProgressState NoLens = Must(FModel::Advance(ReachStage(EBeaconStage::Faro), EBeaconStage::Preparacion));
			TestEqual(TEXT("Sin lente"), FModel::CurrentObjective(NoLens, Ctx), FName(TEXT("obj_recupera_lente")));
			FBeaconProgressState S = ReachStage(EBeaconStage::Preparacion);
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::OilJar));
			TestEqual(TEXT("Mecha"), FModel::CurrentObjective(S, Ctx), FName(TEXT("obj_trenza_mecha")));
			S = Must(FModel::RecordMilestone(S, EBeaconMilestone::WickMade));
			TestEqual(TEXT("Montar"), FModel::CurrentObjective(S, Ctx), FName(TEXT("obj_monta_linterna")));
		});
	});

	Describe(TEXT("epílogo"), [this]()
	{
		It(TEXT("solo 8 de 8 da el epílogo completo"), [this]()
		{
			FNotebookState Notebook;
			TestEqual(TEXT("Sin páginas"), FModel::EpilogueFor(Notebook), EBeaconEpilogue::Partial);
			Notebook.PagesMask = 0x7F;
			TestEqual(TEXT("7 de 8"), FModel::EpilogueFor(Notebook), EBeaconEpilogue::Partial);
			Notebook.PagesMask = 0xFF;
			TestEqual(TEXT("8 de 8"), FModel::EpilogueFor(Notebook), EBeaconEpilogue::Complete);
			TestEqual(TEXT("Texto completo"), FModel::EpilogueTextId(EBeaconEpilogue::Complete), FName(TEXT("epilogo_completo")));
			TestEqual(TEXT("Texto parcial"), FModel::EpilogueTextId(EBeaconEpilogue::Partial), FName(TEXT("epilogo_parcial")));
			TestEqual(TEXT("Línea común"), FModel::CommonEpilogueTextId(), FName(TEXT("epilogo_comun")));
		});
	});

	Describe(TEXT("guardado"), [this]()
	{
		It(TEXT("ida y vuelta en las 10 etapas devuelve el mismo estado"), [this]()
		{
			for (uint8 Raw = 0; Raw < static_cast<uint8>(EBeaconStage::Count); ++Raw)
			{
				const FBeaconProgressState S = ReachStage(static_cast<EBeaconStage>(Raw));
				const FBeaconProgressResult Loaded = FModel::FromRecord(FModel::ToRecord(S));
				TestTrue(FString::Printf(TEXT("Etapa %d aceptada"), Raw), Loaded.bOk);
				TestTrue(FString::Printf(TEXT("Etapa %d igual"), Raw), Loaded.State == S);
			}
		});

		It(TEXT("rechaza registros fuera de rango"), [this]()
		{
			const FBeaconProgressRecord Bad[] = {Record(10, 0, 0, 0), Record(255, 0, 0, 0), Record(0, 0x40, 0, 0), Record(0, 0, 3, 0),
				Record(0, 0, 0, 0x8)};
			for (const FBeaconProgressRecord& R : Bad)
			{
				const FBeaconProgressResult Loaded = FModel::FromRecord(R);
				TestFalse(TEXT("Rechazado"), Loaded.bOk);
				TestTrue(TEXT("Estado nuevo"), Loaded.State == FBeaconProgressState());
			}
		});

		It(TEXT("rechaza registros incoherentes"), [this]()
		{
			constexpr uint8 Lens = 1u << 3;
			constexpr uint8 Wick = 1u << 4;
			constexpr uint8 Lit = 1u << 5;
			constexpr uint8 Final = static_cast<uint8>(EBeaconStage::Final);
			TestFalse(TEXT("Ranura de lente sin lente"), FModel::FromRecord(Record(7, 0, 0, 0x1)).bOk);
			TestFalse(TEXT("Ranura de aceite con un tarro"), FModel::FromRecord(Record(7, 0, 1, 0x2)).bOk);
			TestFalse(TEXT("LenteEnMano sin lente"), FModel::FromRecord(Record(5, 0, 0, 0)).bOk);
			TestFalse(TEXT("LinternaMontada sin ranuras"), FModel::FromRecord(Record(8, Lens | Wick, 2, 0x3)).bOk);
			TestFalse(TEXT("Final apagado"), FModel::FromRecord(Record(Final, Lens | Wick, 2, 0x7)).bOk);
			TestFalse(TEXT("Encendida fuera del Final"), FModel::FromRecord(Record(8, Lens | Wick | Lit, 2, 0x7)).bOk);
			TestTrue(TEXT("Final coherente"), FModel::FromRecord(Record(Final, Lens | Wick | Lit, 2, 0x7)).bOk);
		});

		It(TEXT("barrido: todo registro aceptado es coherente y se guarda igual"), [this]()
		{
			int32 Accepted = 0;
			for (uint32 Stage = 0; Stage <= static_cast<uint32>(EBeaconStage::Count); ++Stage)
			{
				for (uint32 Flags = 0; Flags < 0x80; ++Flags)
				{
					for (uint32 Oil = 0; Oil <= 3; ++Oil)
					{
						for (uint32 Slots = 0; Slots < 0x10; ++Slots)
						{
							const FBeaconProgressRecord R = Record(static_cast<uint8>(Stage), static_cast<uint8>(Flags), static_cast<uint8>(Oil),
								static_cast<uint8>(Slots));
							const FBeaconProgressResult Loaded = FModel::FromRecord(R);
							if (!Loaded.bOk)
							{
								continue;
							}
							++Accepted;
							const FBeaconProgressState& S = Loaded.State;
							const bool bCoherent = FModel::MeetsStageRequirement(S, S.Stage)
								&& (!S.bBeaconLit || FModel::IsLanternMounted(S)) && S.OilJars <= FModel::RequiredOilJars;
							if (!TestTrue(TEXT("Coherente"), bCoherent) || !TestTrue(TEXT("Mismo registro"), SameRecord(FModel::ToRecord(S), R)))
							{
								return;
							}
						}
					}
				}
			}
			TestTrue(TEXT("Hay registros válidos"), Accepted > 100);
		});
	});
}

#endif
