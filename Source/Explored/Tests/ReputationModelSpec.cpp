#include "Misc/AutomationTest.h"

#include "Villages/ReputationModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ReputationSpecDetail
{
	constexpr ESettlement WS = ESettlement::WhiteSands;
	constexpr ESettlement Mesa = ESettlement::Mesa;

	/** Estado con Arenas Blancas contactada y la reputación puesta a mano. */
	FReputationState WithReputation(int32 Reputation, ESettlement S = WS)
	{
		FReputationState State;
		FReputationModel::Contact(State, S);
		State.Get(S).Reputation = Reputation;
		return State;
	}
}

BEGIN_DEFINE_SPEC(FReputationModelSpec, "Explored.Villages.Reputation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FReputationModelSpec)

void FReputationModelSpec::Define()
{
	using namespace ReputationSpecDetail;

	Describe("los tramos", [this]()
	{
		It("cortan en 20, 40, 70 y 90 y cubren de 0 a 100 sin huecos", [this]()
		{
			TestEqual(TEXT("0"), FReputationModel::TierOf(0), EReputationTier::Hostile);
			TestEqual(TEXT("19"), FReputationModel::TierOf(19), EReputationTier::Hostile);
			TestEqual(TEXT("20"), FReputationModel::TierOf(20), EReputationTier::Wary);
			TestEqual(TEXT("39"), FReputationModel::TierOf(39), EReputationTier::Wary);
			TestEqual(TEXT("40"), FReputationModel::TierOf(40), EReputationTier::Neutral);
			TestEqual(TEXT("69"), FReputationModel::TierOf(69), EReputationTier::Neutral);
			TestEqual(TEXT("70"), FReputationModel::TierOf(70), EReputationTier::Good);
			TestEqual(TEXT("89"), FReputationModel::TierOf(89), EReputationTier::Good);
			TestEqual(TEXT("90"), FReputationModel::TierOf(90), EReputationTier::High);
			TestEqual(TEXT("100"), FReputationModel::TierOf(100), EReputationTier::High);
			for (int32 T = 0; T < static_cast<int32>(EReputationTier::Count); ++T)
			{
				const EReputationTier Tier = static_cast<EReputationTier>(T);
				TestEqual(FString::Printf(TEXT("TierMin(%s) cae en su tramo"), LexToString(Tier)),
					FReputationModel::TierOf(FReputationModel::TierMin(Tier)), Tier);
			}
		});

		It("dan la tasa ×0 / ×0,75 / ×1 / ×1,25 / ×1,5 en cuartos, creciente", [this]()
		{
			TestEqual(TEXT("hostil"), FReputationModel::TradeRateQuarters(EReputationTier::Hostile), 0);
			TestEqual(TEXT("cauta"), FReputationModel::TradeRateQuarters(EReputationTier::Wary), 3);
			TestEqual(TEXT("neutral"), FReputationModel::TradeRateQuarters(EReputationTier::Neutral), 4);
			TestEqual(TEXT("buena"), FReputationModel::TradeRateQuarters(EReputationTier::Good), 5);
			TestEqual(TEXT("alta"), FReputationModel::TradeRateQuarters(EReputationTier::High), 6);
		});

		It("llevan los ids de fases_futuras.json", [this]()
		{
			TestEqual(TEXT("hostil"), FString(LexToString(EReputationTier::Hostile)), FString(TEXT("hostil")));
			TestEqual(TEXT("alta"), FString(LexToString(EReputationTier::High)), FString(TEXT("alta")));
			TestEqual(TEXT("whitesands"), FString(LexToString(ESettlement::WhiteSands)), FString(TEXT("whitesands")));
			TestEqual(TEXT("mesa"), FString(LexToString(ESettlement::Mesa)), FString(TEXT("mesa")));
		});
	});

	Describe("el primer contacto", [this]()
	{
		It("crea la reputación en 50 una sola vez", [this]()
		{
			FReputationState State;
			TestFalse(TEXT("sin contacto no hay trueque"), FReputationModel::IsTradeOpenByReputation(State, WS, 4));
			TestTrue(TEXT("primer contacto"), FReputationModel::Contact(State, WS));
			TestEqual(TEXT("50"), State.Get(WS).Reputation, 50);
			State.Get(WS).Reputation = 80;
			TestFalse(TEXT("segundo contacto no hace nada"), FReputationModel::Contact(State, WS));
			TestEqual(TEXT("no vuelve a 50"), State.Get(WS).Reputation, 80);
		});

		It("una acción sobre un pueblo sin visitar hace antes el contacto", [this]()
		{
			FReputationState State;
			const FReputationChange C = FReputationModel::Apply(State, Mesa, EReputationAction::HuntNearby, 10);
			TestTrue(TEXT("contacto"), C.bFirstContact);
			TestEqual(TEXT("parte de 50"), C.Before, 50);
			TestEqual(TEXT("50 − 15"), State.Get(Mesa).Reputation, 35);
		});

		It("cada asentamiento lleva la suya: respetar a uno no compra al otro", [this]()
		{
			FReputationState State;
			FReputationModel::Contact(State, WS);
			FReputationModel::Contact(State, Mesa);
			FReputationModel::Apply(State, WS, EReputationAction::DefendRaid, 5);
			FReputationModel::Apply(State, Mesa, EReputationAction::Loot, 5);
			TestEqual(TEXT("Arenas Blancas"), State.Get(WS).Reputation, 60);
			TestEqual(TEXT("La Meseta"), State.Get(Mesa).Reputation, 30);
		});
	});

	Describe("las acciones", [this]()
	{
		It("suman y restan exactamente lo de la biblia", [this]()
		{
			struct FRow
			{
				EReputationAction Key;
				int32 Value;
			};
			const FRow Table[] = {
				{ EReputationAction::Trade, 3 }, { EReputationAction::BoardRequest, 4 },
				{ EReputationAction::RespectfulDay, 2 }, { EReputationAction::DefendRaid, 10 },
				{ EReputationAction::HuntNearby, -15 }, { EReputationAction::MineOrFellNearMarae, -10 },
				{ EReputationAction::Loot, -20 }, { EReputationAction::StrikeVillager, -25 },
			};
			for (const FRow& Row : Table)
			{
				FReputationState State = WithReputation(50);
				const FReputationChange C = FReputationModel::Apply(State, WS, Row.Key, 7);
				TestEqual(FString::Printf(TEXT("%s"), LexToString(Row.Key)), C.Delta, Row.Value);
				TestEqual(FString::Printf(TEXT("%s aplicado"), LexToString(Row.Key)), State.Get(WS).Reputation, 50 + Row.Value);
			}
			TestEqual(TEXT("devolver ritual"), FReputationModel::ActionDelta(EReputationAction::ReturnRitual), 5);
		});

		It("recortan a 0 y a 100 y el cambio devuelto es el real", [this]()
		{
			FReputationState High = WithReputation(99);
			const FReputationChange Up = FReputationModel::Apply(High, WS, EReputationAction::DefendRaid, 1);
			TestEqual(TEXT("tope 100"), High.Get(WS).Reputation, 100);
			TestEqual(TEXT("cambio real +1"), Up.Delta, 1);
			FReputationState Low = WithReputation(5);
			const FReputationChange Down = FReputationModel::Apply(Low, WS, EReputationAction::StrikeVillager, 1);
			TestEqual(TEXT("suelo 0"), Low.Get(WS).Reputation, 0);
			TestEqual(TEXT("cambio real −5"), Down.Delta, -5);
		});

		It("solo el primer trueque del día sube reputación", [this]()
		{
			FReputationState State = WithReputation(50);
			TestEqual(TEXT("primero"), FReputationModel::Apply(State, WS, EReputationAction::Trade, 9).Delta, 3);
			const FReputationChange Second = FReputationModel::Apply(State, WS, EReputationAction::Trade, 9);
			TestTrue(TEXT("segundo ignorado"), Second.bIgnored);
			TestEqual(TEXT("segundo 0"), Second.Delta, 0);
			TestEqual(TEXT("día siguiente"), FReputationModel::Apply(State, WS, EReputationAction::Trade, 10).Delta, 3);
			TestEqual(TEXT("total"), State.Get(WS).Reputation, 56);
		});

		It("la jornada respetuosa cuenta una vez al día", [this]()
		{
			FReputationState State = WithReputation(50);
			FReputationModel::Apply(State, WS, EReputationAction::RespectfulDay, 3);
			FReputationModel::Apply(State, WS, EReputationAction::RespectfulDay, 3);
			TestEqual(TEXT("una vez"), State.Get(WS).Reputation, 52);
		});

		It("ayudar en un asalto solo cuenta con reputación ≥ 40", [this]()
		{
			FReputationState Low = WithReputation(39);
			TestTrue(TEXT("39 no"), FReputationModel::Apply(Low, WS, EReputationAction::DefendRaid, 1).bIgnored);
			TestEqual(TEXT("sigue en 39"), Low.Get(WS).Reputation, 39);
			FReputationState Edge = WithReputation(40);
			TestEqual(TEXT("40 sí"), FReputationModel::Apply(Edge, WS, EReputationAction::DefendRaid, 1).Delta, 10);
		});

		It("el encargo del tablón deja un trueque gratis solo ese día", [this]()
		{
			FReputationState State = WithReputation(50);
			FReputationModel::Apply(State, WS, EReputationAction::BoardRequest, 12);
			TestTrue(TEXT("hoy"), FReputationModel::HasFreeTrade(State, WS, 12));
			TestFalse(TEXT("mañana no"), FReputationModel::HasFreeTrade(State, WS, 13));
			TestFalse(TEXT("el otro pueblo no"), FReputationModel::HasFreeTrade(State, Mesa, 12));
		});

		It("ignora acciones y asentamientos fuera de rango", [this]()
		{
			FReputationState State;
			TestTrue(TEXT("acción Count"), FReputationModel::Apply(State, WS, EReputationAction::Count, 1).bIgnored);
			TestTrue(TEXT("asentamiento Count"), FReputationModel::Apply(State, ESettlement::Count, EReputationAction::Trade, 1).bIgnored);
			TestTrue(TEXT("devolver ritual sin objeto"), FReputationModel::Apply(State, WS, EReputationAction::ReturnRitual, 1).bIgnored);
			TestFalse(TEXT("sin contacto creado"), State.Get(WS).bContacted);
		});
	});

	Describe("sin decaimiento pasivo", [this]()
	{
		It("mil días sin hacer nada no mueven la reputación ni el tramo", [this]()
		{
			FReputationState State = WithReputation(73);
			const FReputationState Before = State;
			for (int32 Day = 0; Day < 1000; ++Day)
			{
				FReputationModel::IsTradeOpenByReputation(State, WS, Day);
				FReputationModel::Tier(State, WS);
				FReputationModel::CooldownDaysLeft(State, WS, Day);
			}
			TestTrue(TEXT("estado idéntico"), State == Before);
			TestEqual(TEXT("sigue Buena"), FReputationModel::Tier(State, WS), EReputationTier::Good);
		});
	});

	Describe("el enfriamiento", [this]()
	{
		It("al caer por debajo de 20 cierra el trueque 15 días aunque se recupere antes", [this]()
		{
			FReputationState State = WithReputation(30);
			const FReputationChange C = FReputationModel::Apply(State, WS, EReputationAction::Loot, 100);
			TestEqual(TEXT("10"), State.Get(WS).Reputation, 10);
			TestTrue(TEXT("empieza"), C.bCooldownStarted);
			TestEqual(TEXT("15 días"), FReputationModel::CooldownDaysLeft(State, WS, 100), 15);

			// Recupera el número con acciones buenas el día siguiente.
			FReputationModel::Apply(State, WS, EReputationAction::DefendRaid, 101);  // 10 < 40: no cuenta
			FReputationModel::Apply(State, WS, EReputationAction::BoardRequest, 101);
			FReputationModel::Apply(State, WS, EReputationAction::RespectfulDay, 101);
			for (int32 Day = 101; Day < 106; ++Day)
			{
				FReputationModel::Apply(State, WS, EReputationAction::BoardRequest, Day);
			}
			TestTrue(TEXT("ya no es Hostil"), State.Get(WS).Reputation >= 20);
			TestFalse(TEXT("pero el trueque sigue cerrado el día 114"), FReputationModel::IsTradeOpenByReputation(State, WS, 114));
			TestTrue(TEXT("abre el día 115"), FReputationModel::IsTradeOpenByReputation(State, WS, 115));
		});

		It("justo en 20 no hay enfriamiento; en 19 sí", [this]()
		{
			FReputationState At20 = WithReputation(30);
			FReputationModel::Apply(At20, WS, EReputationAction::MineOrFellNearMarae, 5);
			TestEqual(TEXT("20"), At20.Get(WS).Reputation, 20);
			TestFalse(TEXT("sin enfriamiento"), FReputationModel::IsInCooldown(At20, WS, 5));
			TestTrue(TEXT("trueque abierto"), FReputationModel::IsTradeOpenByReputation(At20, WS, 5));

			FReputationState At19 = WithReputation(29);
			FReputationModel::Apply(At19, WS, EReputationAction::MineOrFellNearMarae, 5);
			TestTrue(TEXT("en enfriamiento"), FReputationModel::IsInCooldown(At19, WS, 5));
		});

		It("una ofensa nueva estando Hostil alarga el plazo y nunca lo acorta", [this]()
		{
			FReputationState State = WithReputation(10);
			FReputationModel::Apply(State, WS, EReputationAction::Loot, 50);
			TestEqual(TEXT("hasta el 65"), State.Get(WS).TradeCooldownUntilDay, 65);
			FReputationModel::Apply(State, WS, EReputationAction::StrikeVillager, 60);
			TestEqual(TEXT("hasta el 75"), State.Get(WS).TradeCooldownUntilDay, 75);
			// Una ofensa con un día anterior (reloj que va atrás al cargar) no lo acorta.
			FReputationModel::Apply(State, WS, EReputationAction::HuntNearby, 40);
			TestEqual(TEXT("sigue en el 75"), State.Get(WS).TradeCooldownUntilDay, 75);
		});

		It("con la reputación ya en 0 una ofensa sigue contando", [this]()
		{
			FReputationState State = WithReputation(0);
			const FReputationChange C = FReputationModel::Apply(State, WS, EReputationAction::StrikeVillager, 200);
			TestEqual(TEXT("sin cambio de número"), C.Delta, 0);
			TestTrue(TEXT("pero el plazo empieza"), C.bCooldownStarted);
			TestEqual(TEXT("15 días"), FReputationModel::CooldownDaysLeft(State, WS, 200), 15);
		});

		It("no desborda con un día al final del rango", [this]()
		{
			FReputationState State = WithReputation(10);
			const int32 Last = TNumericLimits<int32>::Max() - 3;
			FReputationModel::Apply(State, WS, EReputationAction::Loot, Last);
			TestEqual(TEXT("tope int32"), State.Get(WS).TradeCooldownUntilDay, TNumericLimits<int32>::Max());
			TestTrue(TEXT("cerrado"), FReputationModel::IsInCooldown(State, WS, Last));
		});
	});

	Describe("devolver un objeto ritual", [this]()
	{
		It("suma +5 una sola vez por objeto, a cualquier hora y con el trueque cerrado", [this]()
		{
			FReputationState State = WithReputation(10);
			State.Get(WS).TradeCooldownUntilDay = 30;
			const FReputationChange C = FReputationModel::ReturnRitualObject(State, WS, FName(TEXT("collar_conchas")),
				EArtifactProvenance::RitualCave, 20);
			TestEqual(TEXT("+5"), C.Delta, 5);
			TestEqual(TEXT("15"), State.Get(WS).Reputation, 15);
			TestFalse(TEXT("no abre ni cierra plazos"), C.bCooldownStarted);
			TestEqual(TEXT("el plazo sigue igual"), State.Get(WS).TradeCooldownUntilDay, 30);

			const FReputationChange Again = FReputationModel::ReturnRitualObject(State, Mesa, FName(TEXT("collar_conchas")),
				EArtifactProvenance::RitualCave, 21);
			TestTrue(TEXT("el mismo objeto en el otro pueblo no cuenta"), Again.bIgnored);
			TestFalse(TEXT("ni crea contacto"), State.Get(Mesa).bContacted);
		});

		It("solo los de marae y cueva ritual; los del pecio no son suyos", [this]()
		{
			TestTrue(TEXT("marae"), FReputationModel::IsRitualProvenance(EArtifactProvenance::Marae));
			TestTrue(TEXT("cueva"), FReputationModel::IsRitualProvenance(EArtifactProvenance::RitualCave));
			TestFalse(TEXT("pecio"), FReputationModel::IsRitualProvenance(EArtifactProvenance::Shipwreck));
			TestFalse(TEXT("sumergida"), FReputationModel::IsRitualProvenance(EArtifactProvenance::SunkenRuin));
			FReputationState State = WithReputation(50);
			TestTrue(TEXT("pecio ignorado"), FReputationModel::ReturnRitualObject(State, WS, FName(TEXT("brujula_laton")),
				EArtifactProvenance::Shipwreck, 3).bIgnored);
			TestTrue(TEXT("id vacío ignorado"), FReputationModel::ReturnRitualObject(State, WS, NAME_None,
				EArtifactProvenance::Marae, 3).bIgnored);
			TestEqual(TEXT("sin cambios"), State.Get(WS).Reputation, 50);
		});

		It("devolver en un pueblo sin visitar hace el primer contacto", [this]()
		{
			FReputationState State;
			const FReputationChange C = FReputationModel::ReturnRitualObject(State, Mesa, FName(TEXT("figura_gemelos")),
				EArtifactProvenance::Marae, 8);
			TestTrue(TEXT("contacto"), C.bFirstContact);
			TestEqual(TEXT("55"), State.Get(Mesa).Reputation, 55);
		});
	});

	Describe("los navegantes nunca son enemigos", [this]()
	{
		It("ningún valor de reputación los vuelve combatientes", [this]()
		{
			for (int32 R = 0; R <= 100; ++R)
			{
				const FReputationState State = WithReputation(R);
				if (FReputationModel::IsHostileCombatant(State, WS))
				{
					AddError(FString::Printf(TEXT("combatiente con reputación %d"), R));
				}
			}
			TestFalse(TEXT("sin contacto"), FReputationModel::IsHostileCombatant(FReputationState(), Mesa));
		});

		It("golpear a un aldeano no le quita vida: huye y baja la reputación", [this]()
		{
			FReputationState State = WithReputation(50);
			for (int32 Hit = 0; Hit < 10; ++Hit)
			{
				const FVillagerStrikeResult R = FReputationModel::StrikeVillager(State, WS, 30);
				TestEqual(TEXT("sin daño"), R.HealthDamage, 0.0f);
				TestTrue(TEXT("huye"), R.bFlees);
			}
			TestEqual(TEXT("reputación al suelo"), State.Get(WS).Reputation, 0);
			TestFalse(TEXT("y aun así no es enemigo"), FReputationModel::IsHostileCombatant(State, WS));
		});
	});

	Describe("los favores", [this]()
	{
		It("wayfinding desde 70 y favores altos desde 90, nunca sin contacto", [this]()
		{
			TestFalse(TEXT("69"), FReputationModel::CanTeachWayfinding(WithReputation(69), WS));
			TestTrue(TEXT("70"), FReputationModel::CanTeachWayfinding(WithReputation(70), WS));
			TestFalse(TEXT("89"), FReputationModel::GrantsHighFavors(WithReputation(89), WS));
			TestTrue(TEXT("90"), FReputationModel::GrantsHighFavors(WithReputation(90), WS));
			FReputationState NoContact;
			NoContact.Get(WS).Reputation = 100;
			TestFalse(TEXT("sin contacto"), FReputationModel::CanTeachWayfinding(NoContact, WS));
		});
	});

	Describe("el guardado", [this]()
	{
		It("vuelve idéntico", [this]()
		{
			FReputationState State;
			FReputationModel::Apply(State, WS, EReputationAction::Trade, 11);
			FReputationModel::Apply(State, WS, EReputationAction::BoardRequest, 11);
			FReputationModel::Apply(State, Mesa, EReputationAction::StrikeVillager, 12);
			FReputationModel::Apply(State, Mesa, EReputationAction::StrikeVillager, 12);
			FReputationModel::ReturnRitualObject(State, WS, FName(TEXT("anzuelo_hueso")), EArtifactProvenance::Marae, 13);
			FSaveArchive Ar;
			FReputationModel::Save(Ar, State);
			FSaveValue Parsed;
			FString Error;
			TestTrue(TEXT("texto"), FSaveText::Parse(FSaveText::Write(Ar.GetRoot()), Parsed, Error));
			FReputationState Loaded;
			FReputationModel::Load(FSaveArchive(Parsed), Loaded);
			TestTrue(TEXT("idéntico"), Loaded == State);
		});

		It("una partida sin la sección carga el estado de partida nueva", [this]()
		{
			FReputationState Loaded = WithReputation(77);
			FReputationModel::Load(FSaveArchive(), Loaded);
			TestTrue(TEXT("por defecto"), Loaded == FReputationState());
		});

		It("recorta números fuera de 0-100 y no tira un asentamiento por culpa del otro", [this]()
		{
			FSaveArchive Ar;
			FReputationModel::Save(Ar, WithReputation(60, Mesa));
			FSaveValue Root = Ar.GetRoot();
			Root.Set(TEXT("whitesands"), FSaveValue::MakeString(TEXT("roto")));
			Root.Find(TEXT("mesa"))->Set(TEXT("reputation"), FSaveValue::MakeInt(9000));
			FReputationState Loaded;
			FReputationModel::Load(FSaveArchive(Root), Loaded);
			TestFalse(TEXT("Arenas Blancas ilegible → sin contacto"), Loaded.Get(WS).bContacted);
			TestEqual(TEXT("La Meseta recortada"), Loaded.Get(Mesa).Reputation, 100);

			Root.Find(TEXT("mesa"))->Set(TEXT("reputation"), FSaveValue::MakeInt(-40));
			FReputationModel::Load(FSaveArchive(Root), Loaded);
			TestEqual(TEXT("negativa → 0"), Loaded.Get(Mesa).Reputation, 0);

			Root.Find(TEXT("mesa"))->Set(TEXT("reputation"), FSaveValue::MakeDouble(std::numeric_limits<double>::quiet_NaN()));
			FReputationModel::Load(FSaveArchive(Root), Loaded);
			TestEqual(TEXT("NaN → la del primer contacto"), Loaded.Get(Mesa).Reputation, 50);
		});

		It("descarta ids de objeto repetidos, vacíos o que no son texto", [this]()
		{
			FSaveArchive Ar;
			FSaveValue List = FSaveValue::MakeArray();
			List.Add(FSaveValue::MakeString(TEXT("collar_conchas")));
			List.Add(FSaveValue::MakeInt(4));
			List.Add(FSaveValue::MakeString(TEXT("")));
			List.Add(FSaveValue::MakeString(TEXT("collar_conchas")));
			List.Add(FSaveValue::MakeString(TEXT("remo_ceremonial")));
			Ar.SetValue(TEXT("returnedRituals"), List);
			FReputationState Loaded;
			FReputationModel::Load(Ar, Loaded);
			TestEqual(TEXT("dos únicos"), Loaded.ReturnedRituals.Num(), 2);
		});
	});
}

#endif
