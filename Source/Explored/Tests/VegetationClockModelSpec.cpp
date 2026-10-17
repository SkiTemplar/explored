#include "Misc/AutomationTest.h"

#include "Save/SaveValue.h"
#include "Save/SaveWorldDeltas.h"
#include "WorldGen/FellingModel.h"
#include "WorldGen/VegetationClockModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VegetationClockSpec
{
	FVegetationStumpKey Key(int32 X, int32 Y, const TCHAR* Component, int32 Index)
	{
		FVegetationStumpKey K;
		K.Cell = FIntPoint(X, Y);
		K.Component = FName(Component);
		K.Index = Index;
		return K;
	}

	/** Guarda, pasa por texto compacto (lo que va al disco) y vuelve a cargar. */
	bool RoundTrip(const FVegetationClockModel& In, FVegetationClockModel& Out, FString* OutText = nullptr)
	{
		const FString Text = FSaveText::Write(In.Save(), ESaveTextStyle::Compact);
		if (OutText)
		{
			*OutText = Text;
		}
		FSaveValue Parsed;
		FString Error;
		return FSaveText::Parse(Text, Parsed, Error) && Out.Load(Parsed);
	}

	FSaveValue Row(int64 X, int64 Y, const TCHAR* Component, int64 Index, int64 FelledAt, int64 UprootWork)
	{
		FSaveValue R = FSaveValue::MakeArray();
		R.Add(FSaveValue::MakeInt(X));
		R.Add(FSaveValue::MakeInt(Y));
		R.Add(FSaveValue::MakeString(Component));
		R.Add(FSaveValue::MakeInt(Index));
		R.Add(FSaveValue::MakeInt(FelledAt));
		R.Add(FSaveValue::MakeInt(UprootWork));
		return R;
	}

	FSaveValue Section(const TArray<FSaveValue>& Rows, int64 Version = FVegetationClockModel::SaveVersion)
	{
		FSaveValue Root = FSaveValue::MakeObject();
		Root.Set(TEXT("version"), FSaveValue::MakeInt(Version));
		FSaveValue& List = Root.Set(TEXT("stumps"), FSaveValue::MakeArray());
		for (const FSaveValue& R : Rows)
		{
			List.Add(R);
		}
		return Root;
	}
}

BEGIN_DEFINE_SPEC(FVegetationClockModelSpec, "Explored.VegetationClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TArray<FFellingProfile> Profiles;
	const FFellingProfile* Palm = nullptr;
	const FFellingProfile* Giant = nullptr;
	static constexpr int64 Day = FFellingModel::MinutesPerDay;
	/** Componentes de prueba: «PalmHISM» es palmera, «GiantHISM» gigante; cualquier otro no tiene perfil. */
	const FFellingProfile* ProfileOf(FName Component) const
	{
		if (Component == FName(TEXT("PalmHISM")))
		{
			return Palm;
		}
		if (Component == FName(TEXT("GiantHISM")))
		{
			return Giant;
		}
		return nullptr;
	}
	int64 MatureAfter(const FFellingProfile& P) const
	{
		return ((int64)P.StumpRegrowDays + FMath::Max(1, P.SaplingToMatureDays)) * Day;
	}
END_DEFINE_SPEC(FVegetationClockModelSpec)

void FVegetationClockModelSpec::Define()
{
	using namespace VegetationClockSpec;

	BeforeEach([this]()
	{
		Profiles = FFellingModel::DefaultProfiles();
		Palm = FFellingModel::FindProfile(Profiles, FName(TEXT("Palm")));
		Giant = FFellingModel::FindProfile(Profiles, FName(TEXT("JungleGiant")));
	});

	Describe("guardado", [this]()
	{
		It("el texto no depende del orden de tala (celdas negativas, componentes e índices mezclados)", [this]()
		{
			TArray<FVegetationStumpKey> Keys = {
				Key(0, 0, TEXT("PalmHISM"), 7), Key(-1, 0, TEXT("PalmHISM"), 3), Key(0, -1, TEXT("GiantHISM"), 0),
				Key(0, 0, TEXT("GiantHISM"), 7), Key(0, 0, TEXT("PalmHISM"), 2), Key(5, -1, TEXT("PalmHISM"), FSaveIndexSet::MaxIndex) };
			FVegetationClockModel A, B;
			for (int32 i = 0; i < Keys.Num(); ++i)
			{
				A.RecordFelled(Keys[i], 100 + i);
			}
			for (int32 i = Keys.Num() - 1; i >= 0; --i)
			{
				B.RecordFelled(Keys[i], 100 + i);
			}
			const FString TextA = FSaveText::Write(A.Save(), ESaveTextStyle::Compact);
			TestEqual(TEXT("mismo texto"), TextA, FSaveText::Write(B.Save(), ESaveTextStyle::Compact));
			TestEqual(TEXT("seis entradas"), A.Num(), 6);
			// (Y, X): primero la fila y = −1, con x = 0 antes que x = 5.
			const TArray<FVegetationStumpEntry>& E = A.GetEntries();
			TestTrue(TEXT("primero (0, −1)"), E[0].Key == Key(0, -1, TEXT("GiantHISM"), 0));
			TestTrue(TEXT("luego (5, −1)"), E[1].Key == Key(5, -1, TEXT("PalmHISM"), FSaveIndexSet::MaxIndex));
			TestTrue(TEXT("luego (−1, 0)"), E[2].Key == Key(-1, 0, TEXT("PalmHISM"), 3));
			TestTrue(TEXT("dentro de la celda, componente y luego índice"), E[3].Key == Key(0, 0, TEXT("GiantHISM"), 7) && E[4].Key == Key(0, 0, TEXT("PalmHISM"), 2) && E[5].Key == Key(0, 0, TEXT("PalmHISM"), 7));
		});

		It("guardar y cargar no adelanta ni retrasa el rebrote", [this]()
		{
			FVegetationClockModel Clock;
			const FVegetationStumpKey K = Key(3, -2, TEXT("PalmHISM"), 11);
			const int64 FelledAt = 17 * Day + 613;
			Clock.RecordFelled(K, FelledAt);

			FVegetationClockModel Loaded;
			if (!TestTrue(TEXT("carga"), RoundTrip(Clock, Loaded))) { return; }
			const FStumpState* Before = Clock.Find(K);
			const FStumpState* After = Loaded.Find(K);
			if (!TestNotNull(TEXT("sigue el tocón"), After)) { return; }
			// Cada minuto alrededor de los dos cambios de etapa da lo mismo con y sin el viaje por disco.
			const int64 Sprout = FelledAt + (int64)Palm->StumpRegrowDays * Day;
			const int64 Mature = FelledAt + MatureAfter(*Palm);
			for (const int64 T : { Sprout - 1, Sprout, Sprout + 1, Mature - 1, Mature, Mature + 1 })
			{
				TestTrue(TEXT("misma etapa"), FFellingModel::StageAt(*Palm, *Before, T) == FFellingModel::StageAt(*Palm, *After, T));
				TestEqual(TEXT("misma escala"), FFellingModel::GrowthScaleAt(*Palm, *Before, T), FFellingModel::GrowthScaleAt(*Palm, *After, T));
			}
			TestTrue(TEXT("tocón justo antes"), FFellingModel::StageAt(*Palm, *After, Sprout - 1) == EStumpStage::Stump);
			TestTrue(TEXT("brote en su minuto"), FFellingModel::StageAt(*Palm, *After, Sprout) == EStumpStage::Sapling);
		});

		It("el trabajo de pala a medias sobrevive a la carga", [this]()
		{
			FVegetationClockModel Clock;
			const FVegetationStumpKey K = Key(0, 0, TEXT("GiantHISM"), 4);
			Clock.RecordFelled(K, 0);
			const int32 Hits = Giant->UprootShovelHits;
			for (int32 i = 0; i < Hits / 2; ++i)
			{
				FFellingModel::ApplyUprootHit(*Giant, *Clock.FindMutable(K), EFellingTool::Shovel, Day);
			}
			FVegetationClockModel Loaded;
			if (!TestTrue(TEXT("carga"), RoundTrip(Clock, Loaded))) { return; }
			FStumpState* Stump = Loaded.FindMutable(K);
			if (!TestNotNull(TEXT("tocón"), Stump)) { return; }
			TestFalse(TEXT("aún en pie"), Stump->bUprooted);
			int32 Needed = 0;
			while (!FFellingModel::ApplyUprootHit(*Giant, *Stump, EFellingTool::Shovel, Day) && Needed < 100)
			{
				++Needed;
			}
			TestEqual(TEXT("faltan los golpes que faltaban"), Needed + 1, Hits - Hits / 2);
		});

		It("un tocón arrancado se guarda como arrancado y no madura nunca", [this]()
		{
			FVegetationClockModel Clock;
			const FVegetationStumpKey K = Key(1, 1, TEXT("PalmHISM"), 0);
			Clock.RecordFelled(K, 0);
			while (!FFellingModel::ApplyUprootHit(*Palm, *Clock.FindMutable(K), EFellingTool::Shovel, 10)) {}
			FVegetationClockModel Loaded;
			if (!TestTrue(TEXT("carga"), RoundTrip(Clock, Loaded))) { return; }
			TestTrue(TEXT("arrancado"), Loaded.Find(K) && Loaded.Find(K)->bUprooted);
			TestEqual(TEXT("no madura ni en un siglo"), Loaded.PruneMature([this](FName C) { return ProfileOf(C); }, 36500 * Day), 0);
		});

		It("acepta filas reordenadas a mano y las vuelve a escribir en orden", [this]()
		{
			const FSaveValue Section = VegetationClockSpec::Section({ Row(0, 0, TEXT("PalmHISM"), 9, 5, 0), Row(0, -1, TEXT("PalmHISM"), 1, 6, 0) });
			FVegetationClockModel Clock;
			if (!TestTrue(TEXT("carga"), Clock.Load(Section))) { return; }
			TestTrue(TEXT("primero la fila y = −1"), Clock.GetEntries()[0].Key == Key(0, -1, TEXT("PalmHISM"), 1));
		});

		It("rechaza entero lo que no es válido y deja el reloj vacío", [this]()
		{
			const int64 Big = (int64)FVegetationClockModel::MaxAbsCell + 1;
			const TArray<FSaveValue> Bad = {
				Section({ Row(0, 0, TEXT("PalmHISM"), 0, 0, 0) }, 2),
				Section({ Row(Big, 0, TEXT("PalmHISM"), 0, 0, 0) }),
				Section({ Row(0, -Big, TEXT("PalmHISM"), 0, 0, 0) }),
				Section({ Row(0, 0, TEXT("PalmHISM"), -1, 0, 0) }),
				Section({ Row(0, 0, TEXT("PalmHISM"), (int64)FSaveIndexSet::MaxIndex + 1, 0, 0) }),
				Section({ Row(0, 0, TEXT("PalmHISM"), 0, TNumericLimits<int64>::Min(), 0) }),
				Section({ Row(0, 0, TEXT("PalmHISM"), 0, 0, -1) }),
				Section({ Row(0, 0, TEXT("PalmHISM"), 0, 0, (int64)FFellingModel::WorkToFell + 1) }),
				Section({ Row(0, 0, TEXT(""), 0, 0, 0) }),
				// Repetidas, también si solo cambia la caja del componente (FName no la distingue).
				Section({ Row(0, 0, TEXT("PalmHISM"), 3, 0, 0), Row(0, 0, TEXT("PalmHISM"), 3, 9, 0) }),
				Section({ Row(0, 0, TEXT("PalmHISM"), 3, 0, 0), Row(0, 0, TEXT("palmhism"), 3, 9, 0) }),
			};
			for (int32 i = 0; i < Bad.Num(); ++i)
			{
				FVegetationClockModel Clock;
				Clock.RecordFelled(Key(9, 9, TEXT("PalmHISM"), 1), 0);
				TestFalse(*FString::Printf(TEXT("caso %d rechazado"), i), Clock.Load(Bad[i]));
				TestTrue(*FString::Printf(TEXT("caso %d vacío"), i), Clock.IsEmpty());
			}

			FSaveValue ShortRow = Section({});
			FSaveValue R = FSaveValue::MakeArray();
			R.Add(FSaveValue::MakeInt(0));
			ShortRow.Find(TEXT("stumps"))->Add(R);
			FVegetationClockModel Clock;
			TestFalse(TEXT("fila corta"), Clock.Load(ShortRow));
			TestFalse(TEXT("sin lista"), Clock.Load(FSaveValue::MakeObject()));
			TestFalse(TEXT("nulo"), Clock.Load(FSaveValue()));
		});
	});

	Describe("PruneMature", [this]()
	{
		It("saca justo las maduras, en orden, y deja los componentes sin perfil", [this]()
		{
			FVegetationClockModel Clock;
			const int64 Now = 1000 * Day;
			const FVegetationStumpKey Old = Key(0, 0, TEXT("PalmHISM"), 1);
			const FVegetationStumpKey Edge = Key(0, 0, TEXT("PalmHISM"), 2);
			const FVegetationStumpKey Young = Key(0, 0, TEXT("PalmHISM"), 3);
			const FVegetationStumpKey Unknown = Key(0, 0, TEXT("RockHISM"), 1);
			Clock.RecordFelled(Old, Now - MatureAfter(*Palm) - 1);
			Clock.RecordFelled(Edge, Now - MatureAfter(*Palm));      // adulto en este mismo minuto
			Clock.RecordFelled(Young, Now - MatureAfter(*Palm) + 1); // le falta un minuto
			Clock.RecordFelled(Unknown, 0);

			TArray<FVegetationStumpKey> Matured;
			TestEqual(TEXT("dos maduras"), Clock.PruneMature([this](FName C) { return ProfileOf(C); }, Now, &Matured), 2);
			TestTrue(TEXT("en orden de guardado"), Matured.Num() == 2 && Matured[0] == Old && Matured[1] == Edge);
			TestNotNull(TEXT("la joven sigue"), Clock.Find(Young));
			TestNotNull(TEXT("sin perfil no se toca"), Clock.Find(Unknown));
			TestEqual(TEXT("quedan dos"), Clock.Num(), 2);
		});

		It("en una partida larga el reloj no crece sin límite", [this]()
		{
			// Se tala una palmera nueva cada hora durante un año y se poda una vez al día.
			FVegetationClockModel Clock;
			int32 MaxSeen = 0;
			for (int64 Hour = 0; Hour < 365 * 24; ++Hour)
			{
				Clock.RecordFelled(Key((int32)(Hour % 7) - 3, (int32)(Hour / 7 % 5), TEXT("PalmHISM"), (int32)Hour), Hour * 60);
				if (Hour % 24 == 23)
				{
					Clock.PruneMature([this](FName C) { return ProfileOf(C); }, Hour * 60);
				}
				MaxSeen = FMath::Max(MaxSeen, Clock.Num());
			}
			// Como mucho las taladas en (rebrote + crecimiento) días más un día sin podar.
			const int64 Window = (MatureAfter(*Palm) / Day + 1) * 24;
			TestTrue(TEXT("acotado por la ventana de rebrote"), MaxSeen <= Window);
			TestTrue(TEXT("y no vacío"), Clock.Num() > 0);
		});
	});

	Describe("Reconcile", [this]()
	{
		It("un talado sin hora rebrota más tarde, nunca antes", [this]()
		{
			FSaveScatterDeltas Felled;
			Felled.Add(FIntPoint(-4, 2), 6);
			Felled.Add(FIntPoint(-4, 2), 0);
			FVegetationClockModel Clock;
			const int64 LoadMinute = 50 * Day;
			TestEqual(TEXT("dos añadidas"), Clock.Reconcile(FName(TEXT("PalmHISM")), Felled, LoadMinute), 2);
			const FStumpState* Stump = Clock.Find(Key(-4, 2, TEXT("PalmHISM"), 6));
			if (!TestNotNull(TEXT("con hora"), Stump)) { return; }
			TestEqual(TEXT("talado al cargar"), Stump->FelledAtMinute, LoadMinute);
			TestTrue(TEXT("sigue siendo tocón un minuto antes del rebrote"),
				FFellingModel::StageAt(*Palm, *Stump, LoadMinute + (int64)Palm->StumpRegrowDays * Day - 1) == EStumpStage::Stump);
			TestEqual(TEXT("idempotente"), Clock.Reconcile(FName(TEXT("PalmHISM")), Felled, LoadMinute + 5), 0);
		});

		It("no pasa de MaxEntries aunque la capa «felled» tenga más: el guardado sigue cargando", [this]()
		{
			FSaveScatterDeltas Felled;
			for (int32 I = 0; I <= FSaveIndexSet::MaxIndex; ++I)
			{
				Felled.Add(FIntPoint(0, 0), I);
			}
			Felled.Add(FIntPoint(1, 0), 3);
			FVegetationClockModel Clock;
			Clock.RecordFelled(Key(-1, 0, TEXT("GiantHISM"), 0), 5);
			TestEqual(TEXT("se llena hasta el tope"), Clock.Reconcile(FName(TEXT("PalmHISM")), Felled, 7), FVegetationClockModel::MaxEntries - 1);
			TestEqual(TEXT("tope"), Clock.Num(), FVegetationClockModel::MaxEntries);
			Clock.RecordFelled(Key(2, 0, TEXT("PalmHISM"), 0), 9);
			TestEqual(TEXT("una tala más no entra"), Clock.Num(), FVegetationClockModel::MaxEntries);
			TestNull(TEXT("ni queda a medias"), Clock.Find(Key(2, 0, TEXT("PalmHISM"), 0)));
			Clock.RecordFelled(Key(-1, 0, TEXT("GiantHISM"), 0), 11);
			const FStumpState* Again = Clock.Find(Key(-1, 0, TEXT("GiantHISM"), 0));
			TestTrue(TEXT("volver a talar uno que ya está sí reinicia su hora"), Again && Again->FelledAtMinute == 11);
			TestEqual(TEXT("reconciliar otra vez no añade"), Clock.Reconcile(FName(TEXT("PalmHISM")), Felled, 8), 0);
		});

		It("tras una sección ilegible, todos los tocones vuelven con la hora de carga", [this]()
		{
			FSaveScatterDeltas PalmFelled, GiantFelled;
			PalmFelled.Add(FIntPoint(0, 0), 1);
			GiantFelled.Add(FIntPoint(0, 0), 1);
			GiantFelled.Add(FIntPoint(31, -31), 900);
			FVegetationClockModel Clock;
			TestFalse(TEXT("sección rota"), Clock.Load(Section({ Row(0, 0, TEXT("PalmHISM"), 1, 5, 99999999) })));
			Clock.Reconcile(FName(TEXT("PalmHISM")), PalmFelled, 7);
			Clock.Reconcile(FName(TEXT("GiantHISM")), GiantFelled, 7);
			TestEqual(TEXT("tres tocones"), Clock.Num(), 3);
			for (const FVegetationStumpEntry& E : Clock.GetEntries())
			{
				TestEqual(TEXT("hora de carga"), E.Stump.FelledAtMinute, (int64)7);
			}
		});

		It("descarta huérfanas de su componente, acota horas futuras y no toca otros componentes", [this]()
		{
			FVegetationClockModel Clock;
			Clock.RecordFelled(Key(0, 0, TEXT("PalmHISM"), 1), 10);      // talada: se queda
			Clock.RecordFelled(Key(0, 0, TEXT("PalmHISM"), 2), 10);      // ya no está talada: huérfana
			Clock.RecordFelled(Key(0, 0, TEXT("PalmHISM"), 3), 9 * Day); // hora futura
			Clock.RecordFelled(Key(0, 0, TEXT("GiantHISM"), 2), 10);     // otro componente
			FSaveScatterDeltas Felled;
			Felled.Add(FIntPoint(0, 0), 1);
			Felled.Add(FIntPoint(0, 0), 3);
			TestEqual(TEXT("una fuera y una acotada"), Clock.Reconcile(FName(TEXT("PalmHISM")), Felled, Day), 2);
			TestNull(TEXT("huérfana fuera"), Clock.Find(Key(0, 0, TEXT("PalmHISM"), 2)));
			TestEqual(TEXT("hora intacta"), Clock.Find(Key(0, 0, TEXT("PalmHISM"), 1))->FelledAtMinute, (int64)10);
			TestEqual(TEXT("futura acotada"), Clock.Find(Key(0, 0, TEXT("PalmHISM"), 3))->FelledAtMinute, Day);
			TestNotNull(TEXT("otro componente intacto"), Clock.Find(Key(0, 0, TEXT("GiantHISM"), 2)));
		});
	});

	Describe("RecordFelled", [this]()
	{
		It("talar el brote reinicia la hora y el trabajo de pala", [this]()
		{
			FVegetationClockModel Clock;
			const FVegetationStumpKey K = Key(0, 0, TEXT("PalmHISM"), 5);
			Clock.RecordFelled(K, 0);
			FFellingModel::ApplyUprootHit(*Palm, *Clock.FindMutable(K), EFellingTool::Shovel, 1);
			Clock.RecordFelled(Key(0, 0, TEXT("palmhism"), 5), 20 * Day);
			TestEqual(TEXT("una sola entrada (FName sin caja)"), Clock.Num(), 1);
			TestEqual(TEXT("hora nueva"), Clock.Find(K)->FelledAtMinute, 20 * Day);
			TestEqual(TEXT("sin trabajo de pala"), Clock.Find(K)->UprootWork, 0);
		});

		It("ignora claves y horas fuera de rango", [this]()
		{
			FVegetationClockModel Clock;
			Clock.RecordFelled(Key(0, 0, TEXT("PalmHISM"), -1), 0);
			Clock.RecordFelled(Key(FVegetationClockModel::MaxAbsCell + 1, 0, TEXT("PalmHISM"), 0), 0);
			Clock.RecordFelled(Key(0, 0, TEXT("PalmHISM"), 0), TNumericLimits<int64>::Max());
			FVegetationStumpKey NoName = Key(0, 0, TEXT("PalmHISM"), 0);
			NoName.Component = NAME_None;
			Clock.RecordFelled(NoName, 0);
			TestTrue(TEXT("nada"), Clock.IsEmpty());
			TestFalse(TEXT("quitar lo que no está"), Clock.Remove(Key(0, 0, TEXT("PalmHISM"), 0)));
		});
	});
}

#endif
