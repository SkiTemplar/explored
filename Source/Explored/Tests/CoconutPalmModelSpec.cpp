#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "WorldGen/CoconutPalmModel.h"
#include "WorldGen/FellingModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FCoconutPalmModelSpec, "Explored.CoconutPalm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	static constexpr int64 Day = FCoconutPalmModel::MinutesPerDay;
	FCoconutPalmProfile Profile;
	/** Un minuto cualquiera lejos del cero, para que los ciclos no empiecen alineados con el reloj. */
	static constexpr int64 Start = 37 * Day + 611;

	bool SameState(const FCoconutPalmState& A, const FCoconutPalmState& B)
	{
		if (A.LastUpdateMinute != B.LastUpdateMinute || A.Slots.Num() != B.Slots.Num() || A.Ground.Num() != B.Ground.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Slots.Num(); ++i)
		{
			if (A.Slots[i].Generation != B.Slots[i].Generation || A.Slots[i].CycleStartMinute != B.Slots[i].CycleStartMinute)
			{
				return false;
			}
		}
		for (int32 i = 0; i < A.Ground.Num(); ++i)
		{
			if (A.Ground[i].Id != B.Ground[i].Id || A.Ground[i].LandedMinute != B.Ground[i].LandedMinute ||
				A.Ground[i].Position != B.Ground[i].Position)
			{
				return false;
			}
		}
		const FCoconutCounters& X = A.Counters;
		const FCoconutCounters& Y = B.Counters;
		return X.NaturalFalls == Y.NaturalFalls && X.Shaken == Y.Shaken && X.GustFalls == Y.GustFalls && X.Climbed == Y.Climbed &&
			X.Felled == Y.Felled && X.PickedFromGround == Y.PickedFromGround && X.Rotted == Y.Rotted;
	}

	/** Cada coco que sale de un hueco sube su generación; los que caen al suelo están, se recogieron o se pudrieron. */
	bool Conserved(const FCoconutPalmState& S)
	{
		int64 Departures = 0;
		for (const FCoconutSlot& Slot : S.Slots)
		{
			Departures += Slot.Generation;
		}
		const FCoconutCounters& C = S.Counters;
		const int64 Out = (int64)C.NaturalFalls + C.Shaken + C.GustFalls + C.Climbed + C.Felled;
		const int64 Landed = (int64)C.NaturalFalls + C.Shaken + C.GustFalls;
		return Departures == Out && Landed == (int64)S.Ground.Num() + C.PickedFromGround + C.Rotted;
	}

	/** Palmera con al menos un maduro colgando, buscada por semilla (determinista). */
	FCoconutPalmState PalmWithMature(uint32& Seed)
	{
		for (;; ++Seed)
		{
			FCoconutPalmState S = FCoconutPalmModel::Initialize(Seed, FVector2D(1000.0, -2000.0), Profile, Start, true);
			if (FCoconutPalmModel::CountOnTree(S, Profile, ECoconutStage::Mature) > 0 &&
				FCoconutPalmModel::CountOnTree(S, Profile, ECoconutStage::Green) > 0)
			{
				return S;
			}
		}
	}
END_DEFINE_SPEC(FCoconutPalmModelSpec)

void FCoconutPalmModelSpec::Define()
{
	BeforeEach([this]()
	{
		Profile = FCoconutPalmModel::DefaultProfile();
	});

	Describe("perfil", [this]()
	{
		It("la palmera mide y abarca lo mismo que la Palm de la tala", [this]()
		{
			const TArray<FFellingProfile> Profiles = FFellingModel::DefaultProfiles();
			const FFellingProfile* Palm = FFellingModel::FindProfile(Profiles, FName(TEXT("Palm")));
			if (!TestNotNull(TEXT("Palm existe"), Palm)) { return; }
			TestEqual(TEXT("altura"), Profile.TrunkHeightMeters, Palm->HeightMeters);
			TestEqual(TEXT("copa"), Profile.CrownRadiusMeters, Palm->CrownRadiusMeters);
		});

		It("a mano una palmera de 9 m se sacude con fuerza 0,5 y una baja con fuerza 1", [this]()
		{
			TestTrue(TEXT("9 m → 0,5"), FMath::IsNearlyEqual(FCoconutPalmModel::HandShakeStrength(Profile), 0.5f, 1.0e-6f));
			FCoconutPalmProfile Low = Profile;
			Low.TrunkHeightMeters = 3.0f;
			TestEqual(TEXT("3 m → 1"), FCoconutPalmModel::HandShakeStrength(Low), 1.0f);
			FCoconutPalmProfile Huge = Profile;
			Huge.TrunkHeightMeters = 400.0f;
			TestEqual(TEXT("nunca por debajo del mínimo"), FCoconutPalmModel::HandShakeStrength(Huge), FCoconutPalmModel::MinHandShakeStrength);
			Huge.TrunkHeightMeters = NAN;
			TestEqual(TEXT("NaN cuenta como palmera baja"), FCoconutPalmModel::HandShakeStrength(Huge), 1.0f);
		});

		It("las rachas solo sueltan cocos por encima de viento 0,6 y como mucho con fuerza 0,5", [this]()
		{
			TestEqual(TEXT("brisa"), FCoconutPalmModel::GustStrength(0.2f), 0.0f);
			TestEqual(TEXT("umbral"), FCoconutPalmModel::GustStrength(0.6f), 0.0f);
			TestTrue(TEXT("temporal"), FCoconutPalmModel::GustStrength(0.8f) > 0.2f);
			TestEqual(TEXT("ciclón"), FCoconutPalmModel::GustStrength(1.0f), 0.5f);
			TestEqual(TEXT("más de 1 es ciclón"), FCoconutPalmModel::GustStrength(9.0f), 0.5f);
			TestEqual(TEXT("NaN no sopla"), FCoconutPalmModel::GustStrength(NAN), 0.0f);
		});

		It("un perfil degenerado se sanea y no deja a Advance dando vueltas de un minuto", [this]()
		{
			FCoconutPalmProfile Bad;
			Bad.Slots = 9999;
			Bad.RefillMinDays = -3;
			Bad.RefillMaxDays = -9;
			Bad.GreenDays = 0;
			Bad.HangMinDays = 0;
			Bad.HangMaxDays = -1;
			Bad.GroundLifeDays = 0;
			Bad.CrownRadiusMeters = NAN;
			Bad.CrackChanceOnFell = 7.0f;
			const FCoconutPalmProfile S = FCoconutPalmModel::Sanitize(Bad);
			TestEqual(TEXT("huecos acotados"), S.Slots, 64);
			TestTrue(TEXT("rango de cuajado ordenado"), S.RefillMinDays >= 0 && S.RefillMaxDays >= S.RefillMinDays);
			TestTrue(TEXT("verde y colgando al menos un día"), S.GreenDays >= 1 && S.HangMinDays >= 1 && S.HangMaxDays >= S.HangMinDays);
			TestEqual(TEXT("copa NaN a 0"), S.CrownRadiusMeters, 0.0f);
			TestEqual(TEXT("probabilidad acotada"), S.CrackChanceOnFell, 1.0f);

			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(5, FVector2D::ZeroVector, Bad, 0, true);
			const int32 Fallen = FCoconutPalmModel::Advance(Palm, Bad, 365 * Day);
			// Ciclo mínimo de 2 días: como mucho 183 vueltas por hueco en un año.
			TestTrue(TEXT("caídas acotadas por el ciclo mínimo"), Fallen > 0 && Fallen <= 64 * 183);
			TestTrue(TEXT("se conserva"), Conserved(Palm));
			for (const FFallenCoconut& C : Palm.Ground)
			{
				TestTrue(TEXT("con copa 0 caen a 0,5 m del tronco"), FMath::IsNearlyEqual(C.Position.Size(), 50.0, 1.0e-6));
			}
		});

		It("sin huecos no hay cocos y nada falla", [this]()
		{
			Profile.Slots = 0;
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(1, FVector2D::ZeroVector, Profile, Start, true);
			TArray<FCoconutDrop> Drops;
			TestEqual(TEXT("sin caídas"), FCoconutPalmModel::Advance(Palm, Profile, Start + 100 * Day), 0);
			TestEqual(TEXT("sin sacudida"), FCoconutPalmModel::Shake(Palm, Profile, 1.0f, FVector2D::ZeroVector, Start + 100 * Day, Drops), 0);
			TestEqual(TEXT("sin trepar"), FCoconutPalmModel::PickFromCrown(Palm, Profile, true, Start + 100 * Day), NAME_None);
			TestEqual(TEXT("sin tala"), FCoconutPalmModel::Fell(Palm, Profile, FVector2D(0.0, 1.0), Start + 100 * Day, Drops), 0);
			TestEqual(TEXT("sin drops"), Drops.Num(), 0);
			TestFalse(TEXT("etapa de un hueco inexistente"), FCoconutPalmModel::StageOf(Palm, Profile, 3) != ECoconutStage::Empty);
		});
	});

	Describe("ciclo natural", [this]()
	{
		It("al generar el mundo hay verdes y maduros, pero ninguno cae en el minuto de la creación", [this]()
		{
			int32 Green = 0, Mature = 0, Empty = 0;
			for (uint32 Seed = 1; Seed <= 200; ++Seed)
			{
				FCoconutPalmState Palm = FCoconutPalmModel::Initialize(Seed, FVector2D::ZeroVector, Profile, Start, true);
				TestEqual(TEXT("nada cae al crear"), FCoconutPalmModel::Advance(Palm, Profile, Start), 0);
				Green += FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green);
				Mature += FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Mature);
				Empty += FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Empty);
			}
			TestEqual(TEXT("todos los huecos contados"), Green + Mature + Empty, 200 * Profile.Slots);
			// Ciclo medio de 3 + 5 + 5,5 días: ~37 % verde, ~41 % maduro, ~22 % vacío.
			TestTrue(TEXT("hay verdes"), Green > 200 * Profile.Slots / 4);
			TestTrue(TEXT("hay maduros"), Mature > 200 * Profile.Slots / 4);
			TestTrue(TEXT("hay huecos vacíos"), Empty > 200 * Profile.Slots / 10);
		});

		It("una palmera recién adulta empieza vacía y tarda al menos RefillMinDays en cuajar", [this]()
		{
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(7, FVector2D::ZeroVector, Profile, Start, false);
			TestEqual(TEXT("vacía"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Empty), Profile.Slots);
			FCoconutPalmModel::Advance(Palm, Profile, Start + Profile.RefillMinDays * Day - 1);
			TestEqual(TEXT("sigue vacía un minuto antes"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Empty), Profile.Slots);
			FCoconutPalmModel::Advance(Palm, Profile, Start + Profile.RefillMaxDays * Day);
			TestEqual(TEXT("al máximo todos han cuajado en verde"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green), Profile.Slots);
		});

		It("avanzar 30 días de golpe, por horas o minuto a minuto da lo mismo", [this]()
		{
			const FCoconutPalmState Initial = FCoconutPalmModel::Initialize(42, FVector2D(-512.0, 77.0), Profile, Start, true);
			FCoconutPalmState Once = Initial;
			FCoconutPalmState Hourly = Initial;
			FCoconutPalmState Minutely = Initial;
			const int64 End = Start + 30 * Day;
			FCoconutPalmModel::Advance(Once, Profile, End);
			for (int64 T = Start; T <= End; T += 60)
			{
				FCoconutPalmModel::Advance(Hourly, Profile, T);
			}
			FCoconutPalmModel::Advance(Hourly, Profile, End);
			for (int64 T = Start; T <= End; ++T)
			{
				FCoconutPalmModel::Advance(Minutely, Profile, T);
			}
			TestTrue(TEXT("han caído cocos"), Once.Counters.NaturalFalls > 0);
			TestTrue(TEXT("de golpe = por horas"), SameState(Once, Hourly));
			TestTrue(TEXT("de golpe = minuto a minuto"), SameState(Once, Minutely));
		});

		It("los caídos se pudren a los GroundLifeDays y se quedan en la lista hasta entonces", [this]()
		{
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(3, FVector2D::ZeroVector, Profile, Start, true);
			FCoconutPalmModel::Advance(Palm, Profile, Start + 20 * Day);
			if (!TestTrue(TEXT("hay alguno en el suelo"), Palm.Ground.Num() > 0)) { return; }
			const FFallenCoconut First = Palm.Ground[0];
			const int64 Life = Profile.GroundLifeDays * Day;
			FCoconutPalmModel::Advance(Palm, Profile, First.LandedMinute + Life - 1);
			TestTrue(TEXT("un minuto antes sigue"), Palm.Ground.ContainsByPredicate([&](const FFallenCoconut& C) { return C.Id == First.Id; }));
			FCoconutPalmModel::Advance(Palm, Profile, First.LandedMinute + Life);
			TestFalse(TEXT("a su hora se pudre"), Palm.Ground.ContainsByPredicate([&](const FFallenCoconut& C) { return C.Id == First.Id; }));
			TestTrue(TEXT("se conserva"), Conserved(Palm));
		});

		It("diez años sin nadie cerca terminan, conservan los cocos y solo dejan en el suelo los recientes", [this]()
		{
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(11, FVector2D::ZeroVector, Profile, 0, true);
			const int64 End = 3650 * Day;
			FCoconutPalmModel::Advance(Palm, Profile, End);
			TestTrue(TEXT("se conserva"), Conserved(Palm));
			TestTrue(TEXT("muchos se pudrieron sin entrar en la lista"), Palm.Counters.Rotted > 1000);
			// Cada hueco suelta como mucho un coco cada 2 + 5 + 3 = 10 días: ≤ 1 vivo por hueco en 6 días.
			TestTrue(TEXT("lista acotada"), Palm.Ground.Num() <= Profile.Slots);
			for (const FFallenCoconut& C : Palm.Ground)
			{
				TestTrue(TEXT("solo recientes"), C.LandedMinute > End - Profile.GroundLifeDays * Day);
			}
		});

		It("un reloj que va hacia atrás no deshace nada", [this]()
		{
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(9, FVector2D::ZeroVector, Profile, Start, true);
			FCoconutPalmModel::Advance(Palm, Profile, Start + 10 * Day);
			const FCoconutPalmState Before = Palm;
			TestEqual(TEXT("Advance hacia atrás"), FCoconutPalmModel::Advance(Palm, Profile, Start), 0);
			TestTrue(TEXT("igual"), SameState(Before, Palm));
			TArray<FCoconutDrop> Drops;
			FCoconutPalmModel::Shake(Palm, Profile, 0.0f, FVector2D::ZeroVector, Start, Drops);
			TestEqual(TEXT("sacudir en el pasado se hace en el último minuto conocido"), Palm.LastUpdateMinute, Start + 10 * Day);
		});
	});

	Describe("sacudir", [this]()
	{
		It("el verde no cae nunca, ni con fuerza 1, y el maduro acaba cayendo", [this]()
		{
			uint32 Seed = 1;
			FCoconutPalmState Palm = PalmWithMature(Seed);
			const int32 Green = FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green);
			const int32 Mature = FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Mature);
			TArray<FCoconutDrop> Drops;
			int32 Fallen = 0;
			for (int32 i = 0; i < 40; ++i)
			{
				Fallen += FCoconutPalmModel::Shake(Palm, Profile, 1.0f, FVector2D(5000.0, 5000.0), Start, Drops);
			}
			TestEqual(TEXT("caen todos los maduros"), Fallen, Mature);
			TestEqual(TEXT("no queda ninguno maduro"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Mature), 0);
			TestEqual(TEXT("los verdes siguen"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green), Green);
			for (const FCoconutDrop& D : Drops)
			{
				TestEqual(TEXT("solo maduros"), D.ItemId, FCoconutPalmModel::MatureItem);
				TestFalse(TEXT("quien sacude a 50 m no recibe"), D.bHitsShaker);
			}
			TestEqual(TEXT("contados como sacudidos"), Palm.Counters.Shaken, Mature);
			TestTrue(TEXT("se conserva"), Conserved(Palm));
		});

		It("fuerza 0 no suelta nada y a mano una palmera alta suelta menos que una baja", [this]()
		{
			FCoconutPalmProfile Low = Profile;
			Low.TrunkHeightMeters = 3.0f;
			int32 Tall = 0, Short = 0, None = 0, Available = 0;
			for (uint32 Seed = 1; Seed <= 300; ++Seed)
			{
				FCoconutPalmState A = FCoconutPalmModel::Initialize(Seed, FVector2D::ZeroVector, Profile, Start, true);
				FCoconutPalmState B = A;
				FCoconutPalmState C = A;
				Available += FCoconutPalmModel::CountOnTree(A, Profile, ECoconutStage::Mature);
				TArray<FCoconutDrop> Drops;
				Tall += FCoconutPalmModel::Shake(A, Profile, FCoconutPalmModel::HandShakeStrength(Profile), FVector2D::ZeroVector, Start, Drops);
				Short += FCoconutPalmModel::Shake(B, Low, FCoconutPalmModel::HandShakeStrength(Low), FVector2D::ZeroVector, Start, Drops);
				None += FCoconutPalmModel::Shake(C, Profile, 0.0f, FVector2D::ZeroVector, Start, Drops);
			}
			TestEqual(TEXT("fuerza 0"), None, 0);
			TestTrue(TEXT("alta < baja"), Tall < Short);
			// Media de la flojera ≈ 0,5: con fuerza 1 cae ~67 % y con 0,5 ~34 %.
			TestTrue(TEXT("baja: más de la mitad"), Short > Available / 2);
			TestTrue(TEXT("alta: entre un 20 % y un 50 %"), Tall > Available / 5 && Tall < Available / 2);
		});

		It("los cocos caen en el anillo de 0,5 a 1,8 m del tronco y pueden pasar a la celda vecina", [this]()
		{
			// Tronco justo en el borde de una celda de vegetación, con coordenadas negativas.
			const double CellSize = 6400.0;
			const FVector2D Trunk(-CellSize, -3.0 * CellSize + 20.0);
			const FIntPoint Home = FFellingModel::CellOf(Trunk, CellSize);
			bool bNeighbour = false;
			for (uint32 Seed = 1; Seed <= 100; ++Seed)
			{
				FCoconutPalmState Palm = FCoconutPalmModel::Initialize(Seed, Trunk, Profile, Start, true);
				FCoconutPalmModel::Advance(Palm, Profile, Start + 15 * Day);
				for (const FFallenCoconut& C : Palm.Ground)
				{
					const double D = FVector2D::Distance(C.Position, Trunk);
					if (!TestTrue(TEXT("dentro del anillo"), D >= 50.0 - 1.0e-6 && D <= 180.0 + 1.0e-6)) { return; }
					bNeighbour |= FFellingModel::CellOf(C.Position, CellSize) != Home;
				}
			}
			TestTrue(TEXT("alguno cae en la celda vecina y sigue siendo de su palmera"), bNeighbour);
		});

		It("bHitsShaker marca exactamente los que caen a menos de 35 cm de quien sacude", [this]()
		{
			int32 Hits = 0, Total = 0;
			for (uint32 Seed = 1; Seed <= 400; ++Seed)
			{
				FCoconutPalmState Palm = FCoconutPalmModel::Initialize(Seed, FVector2D::ZeroVector, Profile, Start, true);
				const FVector2D Shaker(60.0, 0.0); // pegado al tronco
				TArray<FCoconutDrop> Drops;
				FCoconutPalmModel::Shake(Palm, Profile, 1.0f, Shaker, Start, Drops);
				for (const FCoconutDrop& D : Drops)
				{
					++Total;
					Hits += D.bHitsShaker ? 1 : 0;
					TestEqual(TEXT("marca coherente con la distancia"), D.bHitsShaker, FVector2D::Distance(D.Position, Shaker) < 35.0);
				}
			}
			TestTrue(TEXT("hay sacudidas"), Total > 200);
			// El círculo de la cabeza es ~4 % del anillo: pasa, pero poco.
			TestTrue(TEXT("a veces da en la cabeza"), Hits > 0);
			TestTrue(TEXT("pero poco"), Hits * 10 < Total);
		});

		It("repetir la misma secuencia de sacudidas da los mismos cocos, y otra semilla no", [this]()
		{
			auto Run = [this](uint32 Seed)
			{
				FCoconutPalmState Palm = FCoconutPalmModel::Initialize(Seed, FVector2D(300.0, 300.0), Profile, Start, true);
				TArray<FCoconutDrop> Drops;
				for (int32 i = 0; i < 12; ++i)
				{
					FCoconutPalmModel::Shake(Palm, Profile, 0.5f, FVector2D(360.0, 300.0), Start + i * 7 * 60, Drops);
				}
				return Palm;
			};
			TestTrue(TEXT("misma semilla"), SameState(Run(77), Run(77)));
			TestFalse(TEXT("otra semilla"), SameState(Run(77), Run(78)));
		});
	});

	Describe("rachas", [this]()
	{
		It("sin temporal no sueltan nada; una hora se aplica una sola vez y nunca hacia atrás", [this]()
		{
			uint32 Seed = 1;
			FCoconutPalmState Palm = PalmWithMature(Seed);
			const int64 Hour = Start / 60 + 1;
			TArray<FCoconutDrop> Drops;
			TestEqual(TEXT("brisa"), FCoconutPalmModel::ApplyGust(Palm, Profile, 0.3f, Hour, Drops), 0);
			TestEqual(TEXT("la misma hora con ciclón ya no cuenta"), FCoconutPalmModel::ApplyGust(Palm, Profile, 1.0f, Hour, Drops), 0);
			TestEqual(TEXT("una hora pasada tampoco"), FCoconutPalmModel::ApplyGust(Palm, Profile, 1.0f, Hour - 5, Drops), 0);
			int32 Fallen = 0;
			for (int64 H = Hour + 1; H <= Hour + 48; ++H)
			{
				Fallen += FCoconutPalmModel::ApplyGust(Palm, Profile, 1.0f, H, Drops);
			}
			TestTrue(TEXT("dos días de ciclón tiran maduros"), Fallen > 0);
			TestEqual(TEXT("contados como racha"), Palm.Counters.GustFalls, Fallen);
			for (const FCoconutDrop& D : Drops)
			{
				TestFalse(TEXT("nadie sacudía"), D.bHitsShaker);
			}
			TestTrue(TEXT("se conserva"), Conserved(Palm));
		});
	});

	Describe("trepar y recoger", [this]()
	{
		It("trepando se coge el verde que no cae y el hueco queda vacío", [this]()
		{
			uint32 Seed = 1;
			FCoconutPalmState Palm = PalmWithMature(Seed);
			const int32 Green = FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green);
			TestEqual(TEXT("coge un verde"), FCoconutPalmModel::PickFromCrown(Palm, Profile, true, Start), FCoconutPalmModel::GreenItem);
			TestEqual(TEXT("uno menos"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green), Green - 1);
			for (int32 i = 1; i < Green; ++i)
			{
				FCoconutPalmModel::PickFromCrown(Palm, Profile, true, Start);
			}
			TestEqual(TEXT("sin verdes no da nada"), FCoconutPalmModel::PickFromCrown(Palm, Profile, true, Start), NAME_None);
			TestEqual(TEXT("contados"), Palm.Counters.Climbed, Green);
			TestTrue(TEXT("se conserva"), Conserved(Palm));
		});

		It("un coco del suelo se recoge una sola vez; el Id 0 no existe", [this]()
		{
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(21, FVector2D::ZeroVector, Profile, Start, true);
			FCoconutPalmModel::Advance(Palm, Profile, Start + 12 * Day);
			if (!TestTrue(TEXT("hay alguno"), Palm.Ground.Num() > 0)) { return; }
			const uint32 Id = Palm.Ground.Last().Id;
			FFallenCoconut Out;
			TestTrue(TEXT("primera vez"), FCoconutPalmModel::PickFromGround(Palm, Id, &Out));
			TestEqual(TEXT("el mismo"), Out.Id, Id);
			TestFalse(TEXT("segunda vez"), FCoconutPalmModel::PickFromGround(Palm, Id));
			TestFalse(TEXT("Id 0"), FCoconutPalmModel::PickFromGround(Palm, 0));
			TestTrue(TEXT("se conserva"), Conserved(Palm));
		});

		It("los Id del suelo no se repiten en toda la vida de la palmera", [this]()
		{
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(13, FVector2D::ZeroVector, Profile, 0, true);
			TSet<uint32> Seen;
			for (int64 T = 0; T <= 400 * Day; T += Day)
			{
				FCoconutPalmModel::Advance(Palm, Profile, T);
				for (const FFallenCoconut& C : Palm.Ground)
				{
					TestNotEqual(TEXT("nunca 0"), C.Id, (uint32)0);
				}
				while (Palm.Ground.Num() > 0)
				{
					const uint32 Id = Palm.Ground[0].Id;
					if (!TestFalse(TEXT("Id nuevo"), Seen.Contains(Id))) { return; }
					Seen.Add(Id);
					FCoconutPalmModel::PickFromGround(Palm, Id);
				}
			}
			TestTrue(TEXT("muchos cocos"), Seen.Num() > 100);
		});
	});

	Describe("talar", [this]()
	{
		It("suelta lo que queda en la copa alrededor de la copa caída y la copa deja de dar cocos", [this]()
		{
			uint32 Seed = 1;
			FCoconutPalmState Palm = PalmWithMature(Seed);
			const int32 OnTree = Profile.Slots - FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Empty);
			const int32 Green = FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Green);
			TArray<FCoconutDrop> Drops;
			const FVector2D Dir(0.0, -3.0); // sin normalizar
			TestEqual(TEXT("todos los de la copa"), FCoconutPalmModel::Fell(Palm, Profile, Dir, Start, Drops), OnTree);
			TestEqual(TEXT("un drop por coco"), Drops.Num(), OnTree);
			const FVector2D Crown = Palm.TrunkPosition + FVector2D(0.0, -1.0) * (Profile.TrunkHeightMeters * 0.85 * 100.0);
			int32 GreenDrops = 0;
			for (const FCoconutDrop& D : Drops)
			{
				TestTrue(TEXT("en la copa caída"), FVector2D::Distance(D.Position, Crown) <= Profile.CrownRadiusMeters * 100.0 + 1.0e-6);
				TestEqual(TEXT("no quedan en la lista del suelo"), D.Id, (uint32)0);
				GreenDrops += D.ItemId == FCoconutPalmModel::GreenItem ? 1 : 0;
			}
			TestEqual(TEXT("los verdes aguantan el golpe"), GreenDrops, Green);
			TestEqual(TEXT("copa vacía"), FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Empty), Profile.Slots);
			TestEqual(TEXT("no cae nada más"), FCoconutPalmModel::Advance(Palm, Profile, Start + 60 * Day), 0);
			TestEqual(TEXT("talar dos veces no da más"), FCoconutPalmModel::Fell(Palm, Profile, Dir, Start + 60 * Day, Drops), 0);
			TestEqual(TEXT("sacudir un tocón no da nada"), FCoconutPalmModel::Shake(Palm, Profile, 1.0f, FVector2D::ZeroVector, Start + 60 * Day, Drops), 0);
			TestEqual(TEXT("trepar un tocón no da nada"), FCoconutPalmModel::PickFromCrown(Palm, Profile, false, Start + 60 * Day), NAME_None);
			TestEqual(TEXT("los del suelo se pudrieron"), Palm.Ground.Num(), 0);
			TestTrue(TEXT("se conserva"), Conserved(Palm));
		});

		It("sacudir y después talar no da cocos maduros de más", [this]()
		{
			uint32 Seed = 1;
			FCoconutPalmState Palm = PalmWithMature(Seed);
			const int32 Mature = FCoconutPalmModel::CountOnTree(Palm, Profile, ECoconutStage::Mature);
			TArray<FCoconutDrop> Shaken;
			for (int32 i = 0; i < 40; ++i)
			{
				FCoconutPalmModel::Shake(Palm, Profile, 1.0f, FVector2D(5000.0, 0.0), Start, Shaken);
			}
			TArray<FCoconutDrop> Felled;
			FCoconutPalmModel::Fell(Palm, Profile, FVector2D(1.0, 0.0), Start, Felled);
			int32 MatureTotal = Shaken.Num();
			for (const FCoconutDrop& D : Felled)
			{
				MatureTotal += D.ItemId != FCoconutPalmModel::GreenItem ? 1 : 0;
			}
			TestEqual(TEXT("los mismos maduros que tenía la copa"), MatureTotal, Mature);
		});

		It("parte de los maduros se abre al caer, pero nunca todos en conjunto", [this]()
		{
			int32 Cracked = 0, Mature = 0;
			for (uint32 Seed = 1; Seed <= 300; ++Seed)
			{
				FCoconutPalmState Palm = FCoconutPalmModel::Initialize(Seed, FVector2D::ZeroVector, Profile, Start, true);
				TArray<FCoconutDrop> Drops;
				FCoconutPalmModel::Fell(Palm, Profile, FVector2D(NAN, 1.0), Start, Drops);
				for (const FCoconutDrop& D : Drops)
				{
					TestTrue(TEXT("dirección NaN cae hacia +X"), D.Position.X > 0.0);
					Cracked += D.ItemId == FCoconutPalmModel::ShellItem ? 1 : 0;
					Mature += D.ItemId == FCoconutPalmModel::MatureItem ? 1 : 0;
				}
			}
			const float Ratio = (float)Cracked / (float)FMath::Max(1, Cracked + Mature);
			TestTrue(TEXT("~30 % se abre"), Ratio > 0.2f && Ratio < 0.4f);
		});
	});

	Describe("conservación", [this]()
	{
		It("una secuencia larga de todo mezclado no crea ni pierde cocos", [this]()
		{
			FExploredRandom Random(2026);
			FCoconutPalmState Palm = FCoconutPalmModel::Initialize(99, FVector2D(-40.0, 12000.0), Profile, Start, true);
			int64 T = Start;
			TArray<FCoconutDrop> Drops;
			for (int32 Step = 0; Step < 2000; ++Step)
			{
				T += Random.RangeInt(0, 600);
				switch (Random.RangeInt(0, 5))
				{
				case 0: FCoconutPalmModel::Advance(Palm, Profile, T); break;
				case 1: FCoconutPalmModel::Shake(Palm, Profile, Random.NextFloat(), FVector2D(0.0, 12000.0), T, Drops); break;
				case 2: FCoconutPalmModel::ApplyGust(Palm, Profile, Random.NextFloat(), T / 60, Drops); break;
				case 3: FCoconutPalmModel::PickFromCrown(Palm, Profile, Random.Chance(0.5f), T); break;
				case 4:
					if (Palm.Ground.Num() > 0)
					{
						FCoconutPalmModel::PickFromGround(Palm, Palm.Ground[Random.RangeInt(0, Palm.Ground.Num() - 1)].Id);
					}
					break;
				default: FCoconutPalmModel::Advance(Palm, Profile, T - 30); break; // reloj que retrocede
				}
				if (!TestTrue(TEXT("se conserva en cada paso"), Conserved(Palm))) { return; }
			}
			TestTrue(TEXT("ha pasado de todo"), Palm.Counters.NaturalFalls > 0 && Palm.Counters.Shaken > 0 &&
				Palm.Counters.Climbed > 0 && Palm.Counters.PickedFromGround > 0 && Palm.Counters.Rotted > 0);
		});
	});
}

#endif
