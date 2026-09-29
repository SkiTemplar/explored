#include "Misc/AutomationTest.h"

#include "Villages/BarterModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BarterSpecDetail
{
	constexpr ESettlement WS = ESettlement::WhiteSands;

	FReputationState WithReputation(int32 Reputation)
	{
		FReputationState State;
		FReputationModel::Contact(State, WS);
		State.Get(WS).Reputation = Reputation;
		return State;
	}

	FBarterOffer Offer(const TCHAR* Id, int32 Value, EReputationTier MinTier = EReputationTier::Neutral)
	{
		FBarterOffer O;
		O.Id = FName(Id);
		O.Value = Value;
		O.MinTier = MinTier;
		return O;
	}

	FBarterRequest Request(EBarterCategory Category, int32 Count, const FBarterOffer& Wanted, int32 WantedCount = 1,
		int32 Day = 10, float Hour = 12.0f)
	{
		FBarterRequest R;
		R.Settlement = WS;
		R.Day = Day;
		R.Hour = Hour;
		R.Given.Add({ Category, Count });
		R.Wanted = Wanted;
		R.WantedCount = WantedCount;
		return R;
	}
}

BEGIN_DEFINE_SPEC(FBarterModelSpec, "Explored.Villages.Barter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBarterModelSpec)

void FBarterModelSpec::Define()
{
	using namespace BarterSpecDetail;

	Describe("los valores", [this]()
	{
		It("van de 1 a 5 por categoría como la tabla de la biblia", [this]()
		{
			TestEqual(TEXT("fruta y pescado"), FBarterModel::CategoryValue(EBarterCategory::CommonFood), 1);
			TestEqual(TEXT("comida preparada"), FBarterModel::CategoryValue(EBarterCategory::PreparedFood), 2);
			TestEqual(TEXT("fibra y cerámica"), FBarterModel::CategoryValue(EBarterCategory::FiberCeramic), 2);
			TestEqual(TEXT("pieles"), FBarterModel::CategoryValue(EBarterCategory::Leather), 3);
			TestEqual(TEXT("herramientas"), FBarterModel::CategoryValue(EBarterCategory::GoodTool), 3);
			TestEqual(TEXT("medicina"), FBarterModel::CategoryValue(EBarterCategory::Medicine), 4);
			TestEqual(TEXT("metal"), FBarterModel::CategoryValue(EBarterCategory::WorkedMetal), 5);
			TestEqual(TEXT("no se trueca"), FBarterModel::CategoryValue(EBarterCategory::None), 0);
			for (int32 C = 1; C < static_cast<int32>(EBarterCategory::Count); ++C)
			{
				const int32 V = FBarterModel::CategoryValue(static_cast<EBarterCategory>(C));
				TestTrue(FString::Printf(TEXT("%s en 1-5"), LexToString(static_cast<EBarterCategory>(C))), V >= 1 && V <= 5);
			}
		});

		It("valor × tasa: 4 pescados compran una cuerda de valor 3 en Cauta (4×1×0,75 = 3) y 3 no", [this]()
		{
			const FReputationState Wary = WithReputation(20);
			const FBarterOffer Rope = Offer(TEXT("cuerda"), 3, EReputationTier::Wary);
			TestEqual(TEXT("4 sí"), FBarterModel::Evaluate(Wary, Request(EBarterCategory::CommonFood, 4, Rope)).Result, EBarterResult::Accepted);
			const FBarterOutcome Three = FBarterModel::Evaluate(Wary, Request(EBarterCategory::CommonFood, 3, Rope));
			TestEqual(TEXT("3 no"), Three.Result, EBarterResult::NotEnough);
			TestEqual(TEXT("ofrecido 3×1×3 cuartos"), Three.OfferedQuarters, static_cast<int64>(9));
			TestEqual(TEXT("pedido 3×4 cuartos"), Three.RequiredQuarters, static_cast<int64>(12));
		});

		It("la misma oferta vale más cuanto mejor es el tramo", [this]()
		{
			const FBarterOffer Pearl = Offer(TEXT("perla"), 3, EReputationTier::Wary);
			const FBarterRequest Hides = Request(EBarterCategory::Leather, 2, Pearl);
			int64 Last = -1;
			for (const int32 R : { 20, 40, 70, 90 })
			{
				const FBarterOutcome O = FBarterModel::Evaluate(WithReputation(R), Hides);
				TestTrue(FString::Printf(TEXT("sube con %d"), R), O.OfferedQuarters > Last);
				Last = O.OfferedQuarters;
			}
			TestEqual(TEXT("con Alta 2×3×1,5 = 9 → 36 cuartos"), Last, static_cast<int64>(36));
		});

		It("cuántas unidades cubre lo ofrecido (prompt «Ofrecer X → Y ×N»)", [this]()
		{
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2, EReputationTier::Wary);
			// 3 medicinas con Buena: 3×4×1,25 = 15 → 7 sales de valor 2.
			TestEqual(TEXT("7"), FBarterModel::AffordableCount(WithReputation(75), Request(EBarterCategory::Medicine, 3, Salt)), 7);
			TestEqual(TEXT("nada con Hostil"), FBarterModel::AffordableCount(WithReputation(5), Request(EBarterCategory::Medicine, 3, Salt)), 0);
			TestEqual(TEXT("nada si no llega a una"), FBarterModel::AffordableCount(WithReputation(50), Request(EBarterCategory::CommonFood, 1, Salt)), 0);
		});

		It("no desborda con el máximo de líneas y objetos", [this]()
		{
			FBarterRequest R = Request(EBarterCategory::WorkedMetal, FBarterModel::MaxCountPerLine, Offer(TEXT("perla"), 5, EReputationTier::Good));
			for (int32 I = 1; I < FBarterModel::MaxLines; ++I)
			{
				R.Given.Add({ EBarterCategory::WorkedMetal, FBarterModel::MaxCountPerLine });
			}
			R.WantedCount = FBarterModel::MaxCountPerLine;
			const FBarterOutcome O = FBarterModel::Evaluate(WithReputation(100), R);
			TestEqual(TEXT("aceptado"), O.Result, EBarterResult::Accepted);
			TestEqual(TEXT("16×999×5×6"), O.OfferedQuarters, static_cast<int64>(16) * 999 * 5 * 6);
			TestEqual(TEXT("tope de unidades"), FBarterModel::AffordableCount(WithReputation(100), R), FBarterModel::MaxCountPerLine);
		});
	});

	Describe("la ventana horaria", [this]()
	{
		It("abre a las 8:00 y cierra a las 18:00 en punto", [this]()
		{
			const FReputationState State = WithReputation(50);
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2);
			auto At = [&](float Hour) { return FBarterModel::Evaluate(State, Request(EBarterCategory::Medicine, 1, Salt, 1, 10, Hour)).Result; };
			TestEqual(TEXT("7:59"), At(7.99f), EBarterResult::OutsideHours);
			TestEqual(TEXT("8:00"), At(8.0f), EBarterResult::Accepted);
			TestEqual(TEXT("17:59"), At(17.99f), EBarterResult::Accepted);
			TestEqual(TEXT("18:00"), At(18.0f), EBarterResult::OutsideHours);
			TestEqual(TEXT("medianoche"), At(0.0f), EBarterResult::OutsideHours);
			TestEqual(TEXT("NaN"), At(std::numeric_limits<float>::quiet_NaN()), EBarterResult::OutsideHours);
			TestEqual(TEXT("infinito"), At(std::numeric_limits<float>::infinity()), EBarterResult::OutsideHours);
		});
	});

	Describe("los cierres", [this]()
	{
		It("sin contacto, Hostil o en enfriamiento no hay trueque", [this]()
		{
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2, EReputationTier::Wary);
			TestEqual(TEXT("sin contacto"), FBarterModel::Evaluate(FReputationState(), Request(EBarterCategory::Medicine, 1, Salt)).Result,
				EBarterResult::NotContacted);
			TestEqual(TEXT("19"), FBarterModel::Evaluate(WithReputation(19), Request(EBarterCategory::Medicine, 1, Salt)).Result,
				EBarterResult::Hostile);
			FReputationState Cooling = WithReputation(45);
			Cooling.Get(WS).TradeCooldownUntilDay = 11;
			TestEqual(TEXT("enfriamiento"), FBarterModel::Evaluate(Cooling, Request(EBarterCategory::Medicine, 1, Salt, 1, 10)).Result,
				EBarterResult::Cooldown);
			TestEqual(TEXT("el día que acaba, abierto"), FBarterModel::Evaluate(Cooling, Request(EBarterCategory::Medicine, 1, Salt, 1, 11)).Result,
				EBarterResult::Accepted);
		});

		It("lo exclusivo exige su tramo: perlas desde 70, animales desde 90", [this]()
		{
			const FBarterOffer Pearl = Offer(TEXT("perla"), 3, EReputationTier::Good);
			const FBarterOffer Hen = Offer(TEXT("gallina"), 5, EReputationTier::High);
			TestEqual(TEXT("perla con 69"), FBarterModel::Evaluate(WithReputation(69), Request(EBarterCategory::WorkedMetal, 1, Pearl)).Result,
				EBarterResult::TierTooLow);
			TestEqual(TEXT("perla con 70"), FBarterModel::Evaluate(WithReputation(70), Request(EBarterCategory::WorkedMetal, 1, Pearl)).Result,
				EBarterResult::Accepted);
			TestEqual(TEXT("gallina con 89"), FBarterModel::Evaluate(WithReputation(89), Request(EBarterCategory::WorkedMetal, 5, Hen)).Result,
				EBarterResult::TierTooLow);
			TestEqual(TEXT("gallina con 90"), FBarterModel::Evaluate(WithReputation(90), Request(EBarterCategory::WorkedMetal, 1, Hen)).Result,
				EBarterResult::Accepted);
		});

		It("en Cauta solo aceptan comida, fibra y cerámica", [this]()
		{
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2, EReputationTier::Wary);
			const FReputationState Wary = WithReputation(25);
			TestEqual(TEXT("metal"), FBarterModel::Evaluate(Wary, Request(EBarterCategory::WorkedMetal, 1, Salt)).Result,
				EBarterResult::NotBasicWhileWary);
			TestEqual(TEXT("fibra"), FBarterModel::Evaluate(Wary, Request(EBarterCategory::FiberCeramic, 2, Salt)).Result,
				EBarterResult::Accepted);
			TestEqual(TEXT("metal con Neutral"), FBarterModel::Evaluate(WithReputation(40), Request(EBarterCategory::WorkedMetal, 1, Salt)).Result,
				EBarterResult::Accepted);
		});

		It("los objetos rituales y los tesoros no se truecan", [this]()
		{
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2);
			FBarterRequest R = Request(EBarterCategory::WorkedMetal, 1, Salt);
			R.Given.Add({ EBarterCategory::None, 1 });
			TestEqual(TEXT("una línea sin categoría tumba el trueque"), FBarterModel::Evaluate(WithReputation(50), R).Result,
				EBarterResult::NotTradeable);
		});

		It("rechaza entradas imposibles sin tocar el estado", [this]()
		{
			FReputationState State = WithReputation(50);
			const FReputationState Before = State;
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2);
			TestEqual(TEXT("cantidad 0"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 0, Salt)).Result, EBarterResult::Invalid);
			TestEqual(TEXT("cantidad negativa"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, -5, Salt)).Result, EBarterResult::Invalid);
			TestEqual(TEXT("demasiados"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1000, Salt)).Result, EBarterResult::Invalid);
			TestEqual(TEXT("categoría fuera de rango"), FBarterModel::Execute(State,
				Request(static_cast<EBarterCategory>(200), 1, Salt)).Result, EBarterResult::Invalid);
			TestEqual(TEXT("valor 0"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Offer(TEXT("x"), 0))).Result, EBarterResult::Invalid);
			TestEqual(TEXT("valor 6"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 9, Offer(TEXT("x"), 6))).Result, EBarterResult::Invalid);
			TestEqual(TEXT("sin id"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Offer(TEXT(""), 1))).Result, EBarterResult::Invalid);
			TestEqual(TEXT("pedir 0"), FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Salt, 0)).Result, EBarterResult::Invalid);
			FBarterRequest Empty = Request(EBarterCategory::Medicine, 1, Salt);
			Empty.Given.Reset();
			TestEqual(TEXT("sin dar nada"), FBarterModel::Execute(State, Empty).Result, EBarterResult::NotEnough);
			FBarterRequest Bad = Request(EBarterCategory::Medicine, 1, Salt);
			Bad.Settlement = ESettlement::Count;
			TestEqual(TEXT("asentamiento"), FBarterModel::Execute(State, Bad).Result, EBarterResult::Invalid);
			TestTrue(TEXT("estado intacto"), State == Before);
		});
	});

	Describe("la reputación del trueque", [this]()
	{
		It("+3 al primero del día; los siguientes dan objetos pero no reputación", [this]()
		{
			FReputationState State = WithReputation(50);
			const FBarterOffer Salt = Offer(TEXT("sal_refinada"), 2);
			const FBarterOutcome A = FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Salt, 1, 30));
			const FBarterOutcome B = FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Salt, 1, 30));
			TestTrue(TEXT("los dos aceptados"), A.IsAccepted() && B.IsAccepted());
			TestEqual(TEXT("+3"), A.ReputationGained, 3);
			TestEqual(TEXT("+0"), B.ReputationGained, 0);
			TestEqual(TEXT("53"), State.Get(WS).Reputation, 53);
		});

		It("un trueque rechazado no gasta el del día", [this]()
		{
			FReputationState State = WithReputation(50);
			FBarterModel::Execute(State, Request(EBarterCategory::CommonFood, 1, Offer(TEXT("perla"), 3), 1, 30));
			const FBarterOutcome Ok = FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Offer(TEXT("sal_refinada"), 2), 1, 30));
			TestEqual(TEXT("el bueno suma"), Ok.ReputationGained, 3);
		});

		It("el trueque gratis del tablón no cobra nada ni gasta el límite diario", [this]()
		{
			FReputationState State = WithReputation(50);
			FReputationModel::Apply(State, WS, EReputationAction::BoardRequest, 40);  // 54
			FBarterRequest Free = Request(EBarterCategory::CommonFood, 1, Offer(TEXT("perla"), 3, EReputationTier::Good), 1, 40);
			Free.Given.Reset();
			Free.bUseFreeTrade = true;
			TestEqual(TEXT("perla exige Buena"), FBarterModel::Execute(State, Free).Result, EBarterResult::TierTooLow);
			Free.Wanted = Offer(TEXT("tabla_mareas"), 3);
			const FBarterOutcome Gift = FBarterModel::Execute(State, Free);
			TestTrue(TEXT("aceptado"), Gift.IsAccepted() && Gift.bUsedFreeTrade);
			TestEqual(TEXT("0 de reputación"), Gift.ReputationGained, 0);
			TestEqual(TEXT("otra vez no"), FBarterModel::Execute(State, Free).Result, EBarterResult::NoFreeTrade);
			const FBarterOutcome Paid = FBarterModel::Execute(State, Request(EBarterCategory::Medicine, 1, Offer(TEXT("sal_refinada"), 2), 1, 40));
			TestEqual(TEXT("el trueque normal aún suma"), Paid.ReputationGained, 3);
			TestEqual(TEXT("57"), State.Get(WS).Reputation, 57);
		});

		It("el trueque gratis es una unidad y sin entregar nada", [this]()
		{
			FReputationState State = WithReputation(50);
			FReputationModel::Apply(State, WS, EReputationAction::BoardRequest, 40);
			FBarterRequest Free = Request(EBarterCategory::CommonFood, 1, Offer(TEXT("sal_refinada"), 2), 3, 40);
			Free.bUseFreeTrade = true;
			TestEqual(TEXT("tres unidades no"), FBarterModel::Evaluate(State, Free).Result, EBarterResult::Invalid);
			TestEqual(TEXT("una unidad"), FBarterModel::AffordableCount(State, Free), 0);
			Free.Given.Reset();
			TestEqual(TEXT("una unidad sin dar nada"), FBarterModel::AffordableCount(State, Free), 1);
		});
	});

	Describe("el determinismo", [this]()
	{
		It("el mismo intercambio da lo mismo en cualquier orden de líneas", [this]()
		{
			const FReputationState State = WithReputation(77);
			FBarterRequest A = Request(EBarterCategory::Leather, 2, Offer(TEXT("perla"), 3, EReputationTier::Good), 2);
			A.Given.Add({ EBarterCategory::PreparedFood, 3 });
			A.Given.Add({ EBarterCategory::CommonFood, 1 });
			FBarterRequest B = A;
			B.Given.Swap(0, 2);
			const FBarterOutcome OA = FBarterModel::Evaluate(State, A);
			const FBarterOutcome OB = FBarterModel::Evaluate(State, B);
			TestEqual(TEXT("mismo resultado"), OA.Result, OB.Result);
			TestEqual(TEXT("mismo valor"), OA.OfferedQuarters, OB.OfferedQuarters);
			TestEqual(TEXT("(6 + 6 + 1) × 5"), OA.OfferedQuarters, static_cast<int64>(65));
		});
	});
}

#endif
