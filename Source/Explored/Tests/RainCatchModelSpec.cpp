#include "Misc/AutomationTest.h"

#include "Carry/InventoryModel.h"
#include "Weather/RainCatchModel.h"
#include "Weather/WeatherModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FRainCatchModelSpec, "Explored.RainCatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	static constexpr int64 Day = FRainCatchModel::MinutesPerDay;
	static constexpr int64 Liter = FRainCatchModel::MicroLPerLiter;
	/** Días del monzón (16–23): llueve casi a diario, así que los tests tienen agua de verdad. */
	static constexpr int64 MonsoonStart = 16 * Day;

	FRainCatchSpec Coconut()
	{
		return FRainCatchModel::SpecForItem(FName(TEXT("cascara_coco")), 2.0f, false);
	}
	FWeatherSample SampleWithRain(float Rain, float Cloud = 0.9f)
	{
		FWeatherSample S;
		S.Rain = Rain;
		S.CloudCover = Cloud;
		return S;
	}
	bool SameState(const FRainCatchState& A, const FRainCatchState& B)
	{
		return A.RainMicroL == B.RainMicroL && A.OtherMicroL == B.OtherMicroL && A.OtherKind == B.OtherKind &&
			A.LastUpdateMinute == B.LastUpdateMinute && A.CaughtMicroL == B.CaughtMicroL &&
			A.SpilledMicroL == B.SpilledMicroL && A.EvaporatedMicroL == B.EvaporatedMicroL;
	}
	/** Lo que entra menos lo que sale tiene que ser exactamente lo que hay dentro. */
	bool Balanced(const FRainCatchState& S, int64 Initial, int64 Poured, int64 Taken)
	{
		return Initial + S.CaughtMicroL + Poured - S.SpilledMicroL - S.EvaporatedMicroL - Taken == S.TotalMicroL();
	}
END_DEFINE_SPEC(FRainCatchModelSpec)

void FRainCatchModelSpec::Define()
{
	Describe("intensidad y ritmos", [this]()
	{
		It("sigue la tabla de mm/h en los estados del tiempo y es monótona", [this]()
		{
			TestEqual(TEXT("seco"), FRainCatchModel::RainMmPerHour(0.0f), 0.0f);
			TestEqual(TEXT("por debajo del umbral"), FRainCatchModel::RainMmPerHour(0.01f), 0.0f);
			TestTrue(TEXT("llovizna 2,5"), FMath::IsNearlyEqual(FRainCatchModel::RainMmPerHour(0.3f), 2.5f, 1.0e-4f));
			TestTrue(TEXT("chubasco 10"), FMath::IsNearlyEqual(FRainCatchModel::RainMmPerHour(0.75f), 10.0f, 1.0e-4f));
			TestTrue(TEXT("tormenta 30"), FMath::IsNearlyEqual(FRainCatchModel::RainMmPerHour(0.95f), 30.0f, 1.0e-4f));
			TestTrue(TEXT("ciclón 50"), FMath::IsNearlyEqual(FRainCatchModel::RainMmPerHour(1.0f), 50.0f, 1.0e-4f));
			TestTrue(TEXT("por encima de 1 se queda en 50"), FMath::IsNearlyEqual(FRainCatchModel::RainMmPerHour(7.0f), 50.0f, 1.0e-4f));
			TestEqual(TEXT("NaN no llueve"), FRainCatchModel::RainMmPerHour(NAN), 0.0f);
			float Prev = 0.0f;
			for (int32 i = 0; i <= 1000; ++i)
			{
				const float V = FRainCatchModel::RainMmPerHour(i / 1000.0f);
				if (!TestTrue(TEXT("monótona"), V >= Prev)) { break; }
				Prev = V;
			}
		});

		It("1 mm sobre 1 m² es 1 L: una cáscara de coco recoge 180 mL en una hora de chubasco", [this]()
		{
			const FRainCatchSpec Spec = Coconut();
			TestEqual(TEXT("capacidad del inventario"), Spec.CapacityMicroL,
				static_cast<int64>(FInventoryModel::LiquidCapacityFromRecipiente(2.0f) * Liter));
			const FRainCatchRates Rates = FRainCatchModel::RatesFor(Spec, SampleWithRain(0.75f));
			TestEqual(TEXT("3 mL/min"), Rates.InPerMinute, (int64)3000);
			TestEqual(TEXT("sin evaporación mientras llueve"), Rates.EvaporationPerMinute, (int64)0);
			FRainCatchState S;
			FRainCatchModel::StepMinutes(S, Spec, Rates, 60);
			TestEqual(TEXT("180 mL"), S.RainMicroL, (int64)180000);
			TestEqual(TEXT("lluvia limpia"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Rain);
		});

		It("a cubierto no recoge nada y evapora menos que al sol", [this]()
		{
			FRainCatchSpec Open = Coconut();
			FRainCatchSpec Roof = FRainCatchModel::SpecForItem(FName(TEXT("cascara_coco")), 2.0f, true);
			TestEqual(TEXT("bajo techo no entra lluvia"), FRainCatchModel::RatesFor(Roof, SampleWithRain(1.0f)).InPerMinute, (int64)0);
			const FWeatherSample Dry = SampleWithRain(0.0f, 0.0f);
			const int64 SunEvap = FRainCatchModel::RatesFor(Open, Dry).EvaporationPerMinute;
			const int64 ShadeEvap = FRainCatchModel::RatesFor(Roof, Dry).EvaporationPerMinute;
			TestTrue(TEXT("evapora al sol"), SunEvap > 0);
			TestTrue(TEXT("a la sombra, menos"), ShadeEvap > 0 && ShadeEvap < SunEvap);
			TestTrue(TEXT("nubes, menos evaporación"),
				FRainCatchModel::RatesFor(Open, SampleWithRain(0.0f, 1.0f)).EvaporationPerMinute < SunEvap);
		});

		It("solo recogen los recipientes abiertos del catálogo", [this]()
		{
			TestTrue(TEXT("vasija"), FRainCatchModel::MouthAreaM2ForItem(FName(TEXT("vasija_barro"))) > 0.0);
			TestTrue(TEXT("la boca de la almeja gigante es mayor que la de la cantimplora"),
				FRainCatchModel::MouthAreaM2ForItem(FName(TEXT("concha_grande"))) > FRainCatchModel::MouthAreaM2ForItem(FName(TEXT("cantimplora"))));
			for (const TCHAR* Closed : {TEXT("coco_verde"), TEXT("cesta"), TEXT("mochila"), TEXT("bolsa_impermeable"), TEXT("no_existe")})
			{
				const FRainCatchSpec Spec = FRainCatchModel::SpecForItem(FName(Closed), 4.0f, false);
				TestEqual(*FString::Printf(TEXT("%s no recoge"), Closed), FRainCatchModel::RatesFor(Spec, SampleWithRain(1.0f)).InPerMinute, (int64)0);
				TestEqual(*FString::Printf(TEXT("%s sin capacidad de lluvia"), Closed), Spec.CapacityMicroL, (int64)0);
			}
		});
	});

	Describe("rebose, evaporación y mezcla", [this]()
	{
		It("rebosa al llenarse y conserva la masa: recogido = dentro + rebosado + evaporado", [this]()
		{
			const FRainCatchSpec Spec = Coconut();
			FRainCatchState S;
			FRainCatchModel::StepMinutes(S, Spec, FRainCatchModel::RatesFor(Spec, SampleWithRain(0.95f)), 5 * 60);
			TestEqual(TEXT("lleno"), S.TotalMicroL(), Spec.CapacityMicroL);
			TestTrue(TEXT("ha rebosado"), S.SpilledMicroL > 0);
			TestTrue(TEXT("balance tras la tormenta"), Balanced(S, 0, 0, 0));
			FRainCatchModel::StepMinutes(S, Spec, FRainCatchModel::RatesFor(Spec, SampleWithRain(0.0f, 0.0f)), 3 * Day);
			TestTrue(TEXT("se evapora al sol"), S.EvaporatedMicroL > 0 && S.TotalMicroL() < Spec.CapacityMicroL);
			TestTrue(TEXT("balance tras la sequía"), Balanced(S, 0, 0, 0));
		});

		It("al sol se seca del todo y no baja de cero", [this]()
		{
			const FRainCatchSpec Spec = Coconut();
			FRainCatchState S;
			FRainCatchModel::Pour(S, Spec, Spec.CapacityMicroL, ERainCatchLiquid::None);
			FRainCatchModel::StepMinutes(S, Spec, FRainCatchModel::RatesFor(Spec, SampleWithRain(0.0f, 0.0f)), 30 * Day);
			TestEqual(TEXT("vacío"), S.TotalMicroL(), (int64)0);
			TestEqual(TEXT("se evaporó justo lo que había"), S.EvaporatedMicroL, Spec.CapacityMicroL);
			TestEqual(TEXT("vacío"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Empty);
		});

		It("evaporar al sol concentra la mezcla: un poco de agua sin tratar no se vuelve de lluvia", [this]()
		{
			const FRainCatchSpec Spec = Coconut();
			FRainCatchState S;
			FRainCatchModel::Pour(S, Spec, 499000, ERainCatchLiquid::None);
			FRainCatchModel::Pour(S, Spec, 1000, ERainCatchLiquid::Untreated);
			const FRainCatchRates Sun = FRainCatchModel::RatesFor(Spec, SampleWithRain(0.0f, 0.0f));
			TestTrue(TEXT("el sol evapora"), Sun.EvaporationPerMinute > 0);
			for (int32 Minute = 0; Minute < 30 * Day && S.TotalMicroL() > 0; ++Minute)
			{
				FRainCatchModel::StepMinutes(S, Spec, Sun, 1);
				if (S.TotalMicroL() > 0
					&& !TestEqual(TEXT("sigue sin tratar mientras quede agua"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Untreated))
				{
					return;
				}
			}
			TestEqual(TEXT("acaba seco"), S.TotalMicroL(), (int64)0);
			TestTrue(TEXT("balance"), Balanced(S, 0, 500000, 0));
		});

		It("un chaparrón acaba desplazando el agua de mar: salobre → sin tratar → lluvia", [this]()
		{
			const FRainCatchSpec Spec = FRainCatchModel::SpecForItem(FName(TEXT("vasija_barro")), 3.0f, false);
			FRainCatchState S;
			FRainCatchModel::Pour(S, Spec, Spec.CapacityMicroL, ERainCatchLiquid::Sea);
			TestEqual(TEXT("empieza salobre"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Sea);
			const FRainCatchRates Storm = FRainCatchModel::RatesFor(Spec, SampleWithRain(0.95f));
			TArray<ERainCatchQuality> Seen;
			for (int32 Minute = 0; Minute < 3 * Day; ++Minute)
			{
				FRainCatchModel::StepMinutes(S, Spec, Storm, 1);
				const ERainCatchQuality Q = FRainCatchModel::Quality(S);
				if (Seen.Num() == 0 || Seen.Last() != Q) { Seen.Add(Q); }
				if (!TestTrue(TEXT("nunca pasa de la capacidad"), S.TotalMicroL() <= Spec.CapacityMicroL)) { return; }
				if (Q == ERainCatchQuality::Rain) { break; }
			}
			TestEqual(TEXT("tres fases"), Seen.Num(), 3);
			if (Seen.Num() == 3)
			{
				TestTrue(TEXT("en orden"), Seen[0] == ERainCatchQuality::Sea && Seen[1] == ERainCatchQuality::Untreated && Seen[2] == ERainCatchQuality::Rain);
			}
			TestEqual(TEXT("sin rastro de mar"), S.OtherMicroL, (int64)0);
			TestTrue(TEXT("tipo limpio"), S.OtherKind == ERainCatchLiquid::None);
			TestTrue(TEXT("balance con lo vertido"), Balanced(S, 0, Spec.CapacityMicroL, 0));
		});

		It("el umbral salobre es el 3 % de agua de mar", [this]()
		{
			FRainCatchSpec Spec;
			Spec.CapacityMicroL = Liter;
			Spec.MouthAreaM2 = 0.01;
			FRainCatchState S;
			FRainCatchModel::Pour(S, Spec, 970000, ERainCatchLiquid::None);
			FRainCatchModel::Pour(S, Spec, 29999, ERainCatchLiquid::Sea);
			TestEqual(TEXT("justo por debajo: sin tratar"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Untreated);
			FRainCatchModel::Pour(S, Spec, 1, ERainCatchLiquid::Sea);
			TestEqual(TEXT("3 % exacto: salobre"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Sea);
			FRainCatchModel::Pour(S, Spec, 1, ERainCatchLiquid::Untreated);
			TestTrue(TEXT("el mar manda sobre lo sin tratar"), S.OtherKind == ERainCatchLiquid::Sea);
		});

		It("verter y sacar respetan la capacidad y la proporción", [this]()
		{
			FRainCatchSpec Spec;
			Spec.CapacityMicroL = Liter;
			Spec.MouthAreaM2 = 0.01;
			FRainCatchState S;
			TestEqual(TEXT("medio litro de lluvia"), FRainCatchModel::Pour(S, Spec, Liter / 2, ERainCatchLiquid::None), Liter / 2);
			TestEqual(TEXT("solo cabe medio litro más"), FRainCatchModel::Pour(S, Spec, Liter, ERainCatchLiquid::Untreated), Liter / 2);
			TestEqual(TEXT("verter negativo no hace nada"), FRainCatchModel::Pour(S, Spec, -5, ERainCatchLiquid::Sea), (int64)0);
			int64 R = 0;
			int64 O = 0;
			TestEqual(TEXT("saca 400 mL"), FRainCatchModel::Take(S, 400000, &R, &O), (int64)400000);
			TestEqual(TEXT("mitad y mitad"), R, (int64)200000);
			TestEqual(TEXT("mitad y mitad"), O, (int64)200000);
			TestEqual(TEXT("no saca más de lo que hay"), FRainCatchModel::Take(S, 10 * Liter), (int64)600000);
			TestEqual(TEXT("vacío"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Empty);
			TestTrue(TEXT("vacío vuelve a limpio"), S.OtherKind == ERainCatchLiquid::None);
		});
	});

	Describe("tiempo real del archipiélago", [this]()
	{
		It("tres días de monzón de golpe = minuto a minuto = a saltos irregulares", [this]()
		{
			const FWeatherModel Weather(424242u);
			const FRainCatchSpec Spec = FRainCatchModel::SpecForItem(FName(TEXT("vasija_barro")), 3.0f, false);
			FRainCatchState Big;
			Big.LastUpdateMinute = MonsoonStart;
			FRainCatchState Small = Big;
			FRainCatchState Jumpy = Big;
			const int64 End = MonsoonStart + 3 * Day;
			FRainCatchModel::Advance(Big, Spec, Weather, End);
			for (int64 T = MonsoonStart + 1; T <= End; ++T)
			{
				FRainCatchModel::Advance(Small, Spec, Weather, T);
			}
			uint32 Hash = 12345u;
			for (int64 T = MonsoonStart; T < End;)
			{
				Hash = Hash * 1664525u + 1013904223u;
				T = FMath::Min(End, T + 1 + static_cast<int64>(Hash % 97u));
				FRainCatchModel::Advance(Jumpy, Spec, Weather, T);
			}
			TestTrue(TEXT("ha llovido de verdad"), Big.CaughtMicroL > 0);
			TestTrue(TEXT("de golpe = minuto a minuto"), SameState(Big, Small));
			TestTrue(TEXT("de golpe = a saltos"), SameState(Big, Jumpy));
			TestTrue(TEXT("balance"), Balanced(Big, 0, 0, 0));
		});

		It("un recipiente lejano puesto al día al cargarse coincide con uno simulado en vivo", [this]()
		{
			// El motor solo simula los recipientes cerca del jugador; los demás no se tocan.
			const FWeatherModel Weather(7u);
			const FRainCatchSpec Spec = Coconut();
			FRainCatchState Near;
			FRainCatchModel::Pour(Near, Spec, 100000, ERainCatchLiquid::Untreated);
			Near.LastUpdateMinute = MonsoonStart;
			FRainCatchState Far = Near;
			for (int64 T = MonsoonStart; T <= MonsoonStart + 5 * Day; T += 2)
			{
				FRainCatchModel::Advance(Near, Spec, Weather, T);
			}
			TestEqual(TEXT("el lejano no se ha movido"), Far.LastUpdateMinute, MonsoonStart);
			FRainCatchModel::Advance(Far, Spec, Weather, MonsoonStart + 5 * Day);
			TestTrue(TEXT("idénticos"), SameState(Near, Far));
		});

		It("la caché de franjas compartida da lo mismo que sin caché, también al vaciarse", [this]()
		{
			const FWeatherModel Weather(31337u);
			const FRainCatchSky Sky(Weather);
			const FRainCatchSpec Specs[] = {Coconut(), FRainCatchModel::SpecForItem(FName(TEXT("concha_grande")), 2.0f, false),
				FRainCatchModel::SpecForItem(FName(TEXT("vasija_barro")), 3.0f, true)};
			// 70 días: más franjas de las que caben en la caché, así que se vacía por el camino.
			const int64 End = MonsoonStart + 70 * Day;
			for (const FRainCatchSpec& Spec : Specs)
			{
				FRainCatchState Shared;
				Shared.LastUpdateMinute = MonsoonStart;
				FRainCatchState Alone = Shared;
				for (int64 T = MonsoonStart; T <= End; T += 7 * Day)
				{
					FRainCatchModel::Advance(Shared, Spec, Sky, FMath::Min(T, End));
					FRainCatchModel::Advance(Alone, Spec, Weather, FMath::Min(T, End));
				}
				FRainCatchModel::Advance(Shared, Spec, Sky, End);
				FRainCatchModel::Advance(Alone, Spec, Weather, End);
				TestTrue(TEXT("idénticos"), SameState(Shared, Alone));
				TestTrue(TEXT("la caché no crece sin límite"), Sky.NumCached() <= FRainCatchSky::MaxCachedSlots);
			}
		});

		It("la franja se muestrea con el reloj absoluto: da igual cruzar su borde en uno o dos pasos", [this]()
		{
			const FWeatherModel Weather(99u);
			FRainCatchSpec Spec;
			Spec.CapacityMicroL = 1000 * Liter;
			Spec.MouthAreaM2 = 1.0;
			for (int64 Start : {MonsoonStart - 15, MonsoonStart + 9, MonsoonStart + 10, (int64)-25})
			{
				FRainCatchState A;
				A.LastUpdateMinute = Start;
				FRainCatchState B = A;
				FRainCatchModel::Advance(A, Spec, Weather, Start + 31);
				FRainCatchModel::Advance(B, Spec, Weather, Start + 1);
				FRainCatchModel::Advance(B, Spec, Weather, Start + 30);
				FRainCatchModel::Advance(B, Spec, Weather, Start + 31);
				TestTrue(*FString::Printf(TEXT("borde desde %lld"), (long long)Start), SameState(A, B));
			}
		});

		It("un reloj que retrocede no hace nada", [this]()
		{
			const FWeatherModel Weather(1u);
			FRainCatchState S;
			S.LastUpdateMinute = 10 * Day;
			FRainCatchModel::Pour(S, Coconut(), 1000, ERainCatchLiquid::None);
			const FRainCatchState Before = S;
			TestEqual(TEXT("nada recogido"), FRainCatchModel::Advance(S, Coconut(), Weather, 5 * Day), (int64)0);
			TestEqual(TEXT("mismo minuto"), FRainCatchModel::Advance(S, Coconut(), Weather, 10 * Day), (int64)0);
			TestTrue(TEXT("sin cambios"), SameState(S, Before));
		});

		It("ponerse al día tras una ausencia enorme recorre como mucho 60 días", [this]()
		{
			const FWeatherModel Weather(3u);
			const FRainCatchSpec Spec = Coconut();
			// Reloj corrupto en los dos extremos: ni desborda ni cuelga.
			FRainCatchState Ancient;
			Ancient.LastUpdateMinute = TNumericLimits<int64>::Lowest();
			FRainCatchModel::Advance(Ancient, Spec, Weather, MonsoonStart + Day);
			TestEqual(TEXT("reloj al día desde el mínimo"), Ancient.LastUpdateMinute, MonsoonStart + Day);
			TestTrue(TEXT("balance desde el mínimo"), Balanced(Ancient, 0, 0, 0));
			FRainCatchState Future;
			FRainCatchModel::Pour(Future, Spec, 1234, ERainCatchLiquid::None);
			TestEqual(TEXT("reloj absurdo no simula"), FRainCatchModel::Advance(Future, Spec, Weather, TNumericLimits<int64>::Max() / 4), (int64)0);
			TestEqual(TEXT("ni adopta la hora"), Future.LastUpdateMinute, (int64)0);
			TestEqual(TEXT("y no toca el agua"), Future.TotalMicroL(), (int64)1234);
			// Antes adoptaba la hora absurda y el reloj real quedaba siempre atrás: el recipiente se congelaba.
			TestTrue(TEXT("el reloj real sigue llenándolo"), FRainCatchModel::Advance(Future, Spec, Weather, MonsoonStart + Day) > 0);
			FRainCatchState Frozen;
			Frozen.LastUpdateMinute = TNumericLimits<int64>::Max();
			TestEqual(TEXT("reloj guardado corrupto no simula"), FRainCatchModel::Advance(Frozen, Spec, Weather, MonsoonStart), (int64)0);
			TestEqual(TEXT("pero se corrige a la hora actual"), Frozen.LastUpdateMinute, MonsoonStart);
			TestTrue(TEXT("y vuelve a llenarse"), FRainCatchModel::Advance(Frozen, Spec, Weather, MonsoonStart + Day) > 0);
			FRainCatchState Edge;
			Edge.LastUpdateMinute = FRainCatchModel::MaxSupportedMinute - Day;
			FRainCatchModel::Advance(Edge, Spec, Weather, FRainCatchModel::MaxSupportedMinute);
			TestTrue(TEXT("el último día admitido se simula"), Edge.LastUpdateMinute == FRainCatchModel::MaxSupportedMinute && Balanced(Edge, 0, 0, 0));

			// Volver tras 70 días da lo mismo que volver tras 60: lo anterior no cuenta.
			const int64 End = MonsoonStart + 90 * Day;
			FRainCatchState Seventy;
			Seventy.LastUpdateMinute = End - 70 * Day;
			FRainCatchState Sixty;
			Sixty.LastUpdateMinute = End - FRainCatchModel::MaxCatchUpMinutes;
			FRainCatchModel::Advance(Seventy, Spec, Weather, End);
			FRainCatchModel::Advance(Sixty, Spec, Weather, End);
			TestTrue(TEXT("tope de 60 días"), SameState(Seventy, Sixty));
		});

		It("en una partida entera mezclando lluvia, vertidos y tragos la masa cuadra al microlitro", [this]()
		{
			const FWeatherModel Weather(2026u);
			const FRainCatchSpec Spec = FRainCatchModel::SpecForItem(FName(TEXT("concha_grande")), 2.0f, false);
			FRainCatchState S;
			int64 Poured = 0;
			int64 Taken = 0;
			uint32 Hash = 777u;
			for (int64 T = 0; T < 32 * Day; T += 37)
			{
				FRainCatchModel::Advance(S, Spec, Weather, T);
				Hash = Hash * 1664525u + 1013904223u;
				switch (Hash % 5u)
				{
				case 0: Poured += FRainCatchModel::Pour(S, Spec, 50000, ERainCatchLiquid::Sea); break;
				case 1: Poured += FRainCatchModel::Pour(S, Spec, 70000, ERainCatchLiquid::Untreated); break;
				case 2: Taken += FRainCatchModel::Take(S, 90000); break;
				default: break;
				}
				if (!TestTrue(TEXT("balance en cada paso"), Balanced(S, 0, Poured, Taken))) { return; }
				if (!TestTrue(TEXT("sin negativos ni rebose"), S.RainMicroL >= 0 && S.OtherMicroL >= 0 && S.TotalMicroL() <= Spec.CapacityMicroL)) { return; }
			}
			TestTrue(TEXT("un año entero de estaciones ha dejado lluvia"), S.CaughtMicroL > 0);
		});
	});

	Describe("estados degenerados", [this]()
	{
		It("recipiente sin capacidad o sin boca no recoge ni rompe nada", [this]()
		{
			const FWeatherModel Weather(5u);
			FRainCatchSpec Zero;
			FRainCatchState S;
			S.LastUpdateMinute = MonsoonStart;
			TestEqual(TEXT("nada"), FRainCatchModel::Advance(S, Zero, Weather, MonsoonStart + 2 * Day), (int64)0);
			TestEqual(TEXT("vacío"), S.TotalMicroL(), (int64)0);
			FRainCatchSpec NoCap;
			NoCap.MouthAreaM2 = 1.0;
			TestEqual(TEXT("verter en capacidad 0"), FRainCatchModel::Pour(S, NoCap, Liter, ERainCatchLiquid::None), (int64)0);
			FRainCatchSpec Nan;
			Nan.CapacityMicroL = Liter;
			Nan.MouthAreaM2 = NAN;
			TestEqual(TEXT("boca NaN no recoge"), FRainCatchModel::RatesFor(Nan, SampleWithRain(1.0f)).InPerMinute, (int64)0);
			FWeatherSample Weird = SampleWithRain(NAN, NAN);
			const FRainCatchRates R = FRainCatchModel::RatesFor(Coconut(), Weird);
			TestTrue(TEXT("tiempo NaN: ritmos finitos"), R.InPerMinute == 0 && R.EvaporationPerMinute >= 0);
		});

		It("un estado cargado fuera de rango se sanea sin desbordar", [this]()
		{
			FRainCatchState S;
			S.RainMicroL = -50;
			S.OtherMicroL = TNumericLimits<int64>::Max();
			S.OtherKind = static_cast<ERainCatchLiquid>(200);
			TestEqual(TEXT("calidad sin desbordar"), (int32)FRainCatchModel::Quality(S), (int32)ERainCatchQuality::Sea);
			FRainCatchModel::Sanitize(S);
			TestEqual(TEXT("lluvia a cero"), S.RainMicroL, (int64)0);
			TestEqual(TEXT("resto al tope"), S.OtherMicroL, FRainCatchModel::MaxCapacityMicroL);
			const FRainCatchSpec Spec = Coconut();
			FRainCatchModel::StepMinutes(S, Spec, FRainCatchModel::RatesFor(Spec, SampleWithRain(0.3f)), 1);
			TestEqual(TEXT("rebosa hasta su capacidad"), S.TotalMicroL(), Spec.CapacityMicroL);

			FRainCatchState Untyped;
			Untyped.OtherMicroL = 10;
			Untyped.OtherKind = ERainCatchLiquid::None;
			TestEqual(TEXT("agua ajena sin tipo nunca pasa por limpia"), (int32)FRainCatchModel::Quality(Untyped), (int32)ERainCatchQuality::Untreated);
		});
	});
}

#endif
