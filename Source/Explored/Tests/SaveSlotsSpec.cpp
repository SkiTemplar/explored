#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "Save/SaveFormat.h"
#include "Save/SaveSlots.h"
#include "Save/SaveWorldDeltas.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SaveSlotsTest
{
	/** Sistema de ficheros en memoria; puede fallar en la operación N para simular un corte. */
	class FMemoryFileSystem : public ISaveFileSystem
	{
	public:
		TMap<FString, FString> Files;
		/** Operaciones de escritura que se permiten antes de fallar (-1 = sin límite). */
		int32 WritesBeforeFailure = -1;

		virtual bool FileExists(const FString& Path) const override { return Files.Contains(Path); }

		virtual bool ReadText(const FString& Path, FString& OutText) const override
		{
			const FString* Text = Files.Find(Path);
			if (!Text)
			{
				return false;
			}
			OutText = *Text;
			return true;
		}

		virtual bool WriteText(const FString& Path, const FString& Text) override
		{
			if (!Consume())
			{
				return false;
			}
			Files.Add(Path, Text);
			return true;
		}

		virtual bool MoveReplace(const FString& From, const FString& To) override
		{
			const FString* Text = Files.Find(From);
			if (!Text || !Consume())
			{
				return false;
			}
			const FString Copy = *Text;
			Files.Remove(From);
			Files.Add(To, Copy);
			return true;
		}

		virtual bool Delete(const FString& Path) override { return Files.Remove(Path) > 0; }

	private:
		bool Consume()
		{
			if (WritesBeforeFailure == 0)
			{
				return false;
			}
			if (WritesBeforeFailure > 0)
			{
				--WritesBeforeFailure;
			}
			return true;
		}
	};

	FSaveDocument MakeDocument(int64 Timestamp, double PlayTime, int32 Marker)
	{
		FSaveDocument Document;
		Document.Header.Seed = 20260926;
		Document.Header.TimestampUnix = Timestamp;
		Document.Header.PlayTimeSeconds = PlayTime;
		FSaveArchive Ar;
		Ar.Write(TEXT("marker"), Marker);
		Document.Sections.Set(TEXT("test"), Ar.GetRoot());
		return Document;
	}

	int32 MarkerOf(const FSaveReadOutcome& Outcome)
	{
		const FSaveValue* Section = Outcome.Document.Sections.Find(TEXT("test"));
		return Section ? FSaveArchive(*Section).ReadOr(TEXT("marker"), -1) : -1;
	}

	const TCHAR* const Dir = TEXT("Saved/SaveGames");
}

BEGIN_DEFINE_SPEC(FSaveSlotsSpec, "Explored.Save.Slots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FSaveSlotsSpec)

void FSaveSlotsSpec::Define()
{
	using namespace SaveSlotsTest;

	Describe(TEXT("la política de ranuras"), [this]()
	{
		It("tiene tres ranuras manuales y una automática con nombres estables", [this]()
		{
			const TArray<FString> All = FSaveSlotPolicy::AllSlotIds();
			TestEqual(TEXT("Cuatro ranuras"), All.Num(), 4);
			TestEqual(TEXT("Primero la automática"), All[0], FString(TEXT("auto")));
			TestEqual(TEXT("Última manual"), All[3], FString(TEXT("manual3")));
			TestEqual(TEXT("Nombre del fichero"), FSaveSlotPolicy::MainFileName(TEXT("manual2")), FString(TEXT("manual2.sav")));
			TestEqual(TEXT("Nombre de la copia"), FSaveSlotPolicy::BackupFileName(TEXT("auto")), FString(TEXT("auto.bak")));

			TestEqual(TEXT("«Auto» de la UI"), FSaveSlotPolicy::NormalizeSlotId(TEXT("Auto")), FString(TEXT("auto")));
			TestEqual(TEXT("Número de ranura"), FSaveSlotPolicy::NormalizeSlotId(TEXT("2")), FString(TEXT("manual2")));
			TestEqual(TEXT("Mayúsculas"), FSaveSlotPolicy::NormalizeSlotId(TEXT(" Manual3 ")), FString(TEXT("manual3")));
			TestTrue(TEXT("Ranura inexistente"), FSaveSlotPolicy::NormalizeSlotId(TEXT("manual4")).IsEmpty());
			TestTrue(TEXT("Ruta con separador"), FSaveSlotPolicy::JoinPath(TEXT("a/"), TEXT("b.sav")) == TEXT("a/b.sav"));
		});

		It("autoguarda al dormir y en hogueras, estas con intervalo mínimo", [this]()
		{
			TestTrue(TEXT("Dormir"), FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Sleep, 1.0));
			TestTrue(TEXT("Primera hoguera"), FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Campfire, -1.0));
			TestFalse(TEXT("Hoguera seguida"), FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Campfire, 30.0));
			TestTrue(TEXT("Hoguera tras el intervalo"),
				FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Campfire, FSaveSlotPolicy::MinSecondsBetweenCampfireSaves));
			TestEqual(TEXT("Dormir va a auto"), FSaveSlotPolicy::SlotForTrigger(ESaveTrigger::Sleep, TEXT("manual1")), FString(TEXT("auto")));
			TestEqual(TEXT("Manual va a su ranura"), FSaveSlotPolicy::SlotForTrigger(ESaveTrigger::Manual, TEXT("Manual1")), FString(TEXT("manual1")));
		});

		It("planifica la escritura atómica: temporal, copia y renombrado", [this]()
		{
			const TArray<FSaveFileOp> First = FSaveSlotPolicy::PlanWrite(Dir, TEXT("auto"), TEXT("x"), false);
			TestEqual(TEXT("Sin principal: dos pasos"), First.Num(), 2);
			const TArray<FSaveFileOp> Plan = FSaveSlotPolicy::PlanWrite(Dir, TEXT("auto"), TEXT("x"), true);
			TestEqual(TEXT("Con principal: tres pasos"), Plan.Num(), 3);
			TestTrue(TEXT("1. Escribe el temporal"), Plan[0].Kind == FSaveFileOp::EKind::Write && Plan[0].Path.EndsWith(TEXT("auto.tmp")));
			TestTrue(TEXT("2. La principal pasa a copia"), Plan[1].From.EndsWith(TEXT("auto.sav")) && Plan[1].Path.EndsWith(TEXT("auto.bak")));
			TestTrue(TEXT("3. El temporal pasa a principal"), Plan[2].From.EndsWith(TEXT("auto.tmp")) && Plan[2].Path.EndsWith(TEXT("auto.sav")));
		});
	});

	Describe(TEXT("el almacén"), [this]()
	{
		It("rota la copia de seguridad en cada guardado", [this]()
		{
			FMemoryFileSystem Fs;
			FSaveSlotStore Store(Fs, Dir, FSaveMigrations());
			FString Error;
			TestTrue(TEXT("Primer guardado"), Store.Write(TEXT("auto"), MakeDocument(100, 10.0, 1), Error));
			TestTrue(TEXT("Segundo guardado"), Store.Write(TEXT("auto"), MakeDocument(200, 20.0, 2), Error));
			TestFalse(TEXT("Sin temporal"), Fs.FileExists(TEXT("Saved/SaveGames/auto.tmp")));

			const FSaveReadOutcome Read = Store.Read(TEXT("auto"));
			TestTrue(TEXT("Lee la principal"), Read.Result == ESaveLoadResult::Ok && !Read.bFromBackup);
			TestEqual(TEXT("La más nueva"), MarkerOf(Read), 2);
			TestEqual(TEXT("Anota la ranura"), Read.Document.Header.SlotId, FString(TEXT("auto")));

			FString Backup;
			Fs.ReadText(TEXT("Saved/SaveGames/auto.bak"), Backup);
			FSaveDocument BackupDocument;
			FSaveCodec::Decode(Backup, FSaveMigrations(), ExploredSave::CurrentFormatVersion, BackupDocument, Error);
			TestEqual(TEXT("La copia es la anterior"), BackupDocument.Header.TimestampUnix, static_cast<int64>(100));
			TestFalse(TEXT("Ranura desconocida"), Store.Write(TEXT("manual9"), MakeDocument(1, 1.0, 1), Error));
		});

		It("cae a la copia si la principal está dañada", [this]()
		{
			FMemoryFileSystem Fs;
			FSaveSlotStore Store(Fs, Dir, FSaveMigrations());
			FString Error;
			Store.Write(TEXT("manual1"), MakeDocument(100, 10.0, 1), Error);
			Store.Write(TEXT("manual1"), MakeDocument(200, 20.0, 2), Error);

			FString& Main = Fs.Files.FindChecked(TEXT("Saved/SaveGames/manual1.sav"));
			Main = Main.Replace(TEXT("\"marker\": 2"), TEXT("\"marker\": 3"));
			const FSaveReadOutcome Corrupted = Store.Read(TEXT("manual1"));
			TestTrue(TEXT("Legible"), Corrupted.Result == ESaveLoadResult::Ok);
			TestTrue(TEXT("Desde la copia"), Corrupted.bFromBackup);
			TestEqual(TEXT("Contenido anterior"), MarkerOf(Corrupted), 1);

			Main = Main.Left(Main.Len() - 40);
			TestTrue(TEXT("Truncada también cae a la copia"), Store.Read(TEXT("manual1")).bFromBackup);

			Fs.Files.FindChecked(TEXT("Saved/SaveGames/manual1.bak")) = TEXT("basura");
			const FSaveReadOutcome Both = Store.Read(TEXT("manual1"));
			TestTrue(TEXT("Si las dos fallan, el error es el de la principal"), Both.Result == ESaveLoadResult::Malformed && !Both.bFromBackup);
		});

		It("sobrevive a un corte en cualquier paso de la escritura", [this]()
		{
			for (int32 FailAt = 0; FailAt < 3; ++FailAt)
			{
				FMemoryFileSystem Fs;
				FSaveSlotStore Store(Fs, Dir, FSaveMigrations());
				FString Error;
				Store.Write(TEXT("auto"), MakeDocument(100, 10.0, 1), Error);
				Fs.WritesBeforeFailure = FailAt;
				TestFalse(TEXT("La escritura falla"), Store.Write(TEXT("auto"), MakeDocument(200, 20.0, 2), Error));
				const FSaveReadOutcome Read = Store.Read(TEXT("auto"));
				TestTrue(FString::Printf(TEXT("Legible tras cortar en el paso %d"), FailAt), Read.Result == ESaveLoadResult::Ok);
				TestEqual(TEXT("Conserva la partida anterior"), MarkerOf(Read), 1);
			}
		});

		It("no sustituye por la copia una partida de una versión futura", [this]()
		{
			FMemoryFileSystem Fs;
			FSaveDocument Future = MakeDocument(300, 1.0, 9);
			Future.Header.FormatVersion = ExploredSave::CurrentFormatVersion + 1;
			Fs.Files.Add(TEXT("Saved/SaveGames/auto.sav"), FSaveCodec::Encode(Future));
			Fs.Files.Add(TEXT("Saved/SaveGames/auto.bak"), FSaveCodec::Encode(MakeDocument(100, 1.0, 1)));
			FSaveSlotStore Store(Fs, Dir, FSaveMigrations());
			const FSaveReadOutcome Read = Store.Read(TEXT("auto"));
			TestTrue(TEXT("Versión futura"), Read.Result == ESaveLoadResult::FutureVersion);
			TestFalse(TEXT("Sin caer a la copia"), Read.bFromBackup);
		});

		It("ordena «Continuar» por fecha y deja al final lo ilegible", [this]()
		{
			FMemoryFileSystem Fs;
			FSaveSlotStore Store(Fs, Dir, FSaveMigrations());
			FString Error;
			TestTrue(TEXT("Sin partidas no hay continuar"), Store.FindContinueSlot().IsEmpty());
			Store.Write(TEXT("auto"), MakeDocument(100, 50.0, 1), Error);
			Store.Write(TEXT("manual1"), MakeDocument(300, 10.0, 2), Error);
			Store.Write(TEXT("manual2"), MakeDocument(200, 10.0, 3), Error);
			Fs.Files.Add(TEXT("Saved/SaveGames/manual3.sav"), TEXT("{roto"));

			const TArray<FSaveSlotInfo> Slots = Store.List();
			TestEqual(TEXT("Cuatro ranuras con ficheros"), Slots.Num(), 4);
			if (Slots.Num() == 4)
			{
				TestEqual(TEXT("1.ª la más reciente"), Slots[0].SlotId, FString(TEXT("manual1")));
				TestEqual(TEXT("2.ª"), Slots[1].SlotId, FString(TEXT("manual2")));
				TestEqual(TEXT("3.ª"), Slots[2].SlotId, FString(TEXT("auto")));
				TestEqual(TEXT("Ilegible al final"), Slots[3].SlotId, FString(TEXT("manual3")));
				TestFalse(TEXT("No se puede cargar"), Slots[3].IsLoadable());
			}
			TestEqual(TEXT("Continuar"), Store.FindContinueSlot(), FString(TEXT("manual1")));

			TArray<FSaveSlotInfo> Tie;
			for (const TCHAR* Id : { TEXT("manual2"), TEXT("auto"), TEXT("manual1") })
			{
				FSaveSlotInfo& Info = Tie.AddDefaulted_GetRef();
				Info.SlotId = Id;
				Info.Result = ESaveLoadResult::Ok;
				Info.Header.TimestampUnix = 500;
			}
			FSaveSlotPolicy::SortForContinue(Tie);
			TestEqual(TEXT("Empate: orden fijo de ranuras"), Tie[0].SlotId, FString(TEXT("auto")));

			TestTrue(TEXT("Borra la ranura"), Store.DeleteSlot(TEXT("manual1")));
			TestEqual(TEXT("Continuar pasa a la siguiente"), Store.FindContinueSlot(), FString(TEXT("manual2")));
		});
	});

	Describe(TEXT("los deltas del mundo"), [this]()
	{
		It("guarda y lee instancias retiradas por celda e índice", [this]()
		{
			FSaveWorldDeltas World;
			World.Seed = 20260926;
			FSaveScatterDeltas& Harvested = World.Layer(TEXT("harvested"));
			TestTrue(TEXT("Añade"), Harvested.Add(FIntPoint(-3, 7), 12));
			TestFalse(TEXT("Repetida"), Harvested.Add(FIntPoint(-3, 7), 12));
			Harvested.Add(FIntPoint(-3, 7), 13);
			Harvested.Add(FIntPoint(4, -1), 0);
			Harvested.Add(FIntPoint(0, 0), FSaveIndexSet::MaxIndex);
			TestFalse(TEXT("Índice negativo"), Harvested.Add(FIntPoint(0, 0), -1));
			TestFalse(TEXT("Índice excesivo"), Harvested.Add(FIntPoint(0, 0), FSaveIndexSet::MaxIndex + 1));
			World.Layer(TEXT("destroyed")).Add(FIntPoint(1, 1), 5);

			FSaveArchive Ar;
			World.Save(Ar);
			FSaveValue Parsed;
			FString Error;
			FSaveText::Parse(FSaveText::Write(Ar.GetRoot()), Parsed, Error);
			FSaveWorldDeltas Loaded;
			Loaded.Load(FSaveArchive(Parsed));
			TestTrue(TEXT("Ida y vuelta"), Loaded == World);
			TestTrue(TEXT("Contiene"), Loaded.FindLayer(TEXT("harvested")) && Loaded.FindLayer(TEXT("harvested"))->Contains(FIntPoint(-3, 7), 13));
			TestEqual(TEXT("Total"), Loaded.FindLayer(TEXT("harvested"))->Num(), 4);

			// Añadir capas al TMap puede mover las existentes: se vuelve a pedir la referencia.
			FSaveScatterDeltas& HarvestedAgain = World.Layer(TEXT("harvested"));
			TArray<FIntPoint> Order;
			HarvestedAgain.ForEach([&Order](const FIntPoint& Cell, int32 Index) { Order.Add(Cell); });
			TestTrue(TEXT("Orden determinista (Y, X)"), Order == TArray<FIntPoint>({ FIntPoint(4, -1), FIntPoint(0, 0), FIntPoint(-3, 7), FIntPoint(-3, 7) }));

			TestTrue(TEXT("Quita"), HarvestedAgain.Remove(FIntPoint(4, -1), 0));
			TestTrue(TEXT("La celda vacía desaparece"), HarvestedAgain.FindCell(FIntPoint(4, -1)) == nullptr);
		});

		It("une deltas de dos partidas", [this]()
		{
			FSaveScatterDeltas A;
			A.Add(FIntPoint(0, 0), 1);
			A.Add(FIntPoint(2, 0), 5);
			FSaveScatterDeltas B;
			B.Add(FIntPoint(0, 0), 1);
			B.Add(FIntPoint(0, 0), 2);
			B.Add(FIntPoint(-1, 0), 9);
			A.Merge(B);
			TestEqual(TEXT("Unión sin duplicados"), A.Num(), 4);
			TestEqual(TEXT("Tres celdas"), A.NumCells(), 3);
			TestTrue(TEXT("Contiene la nueva"), A.Contains(FIntPoint(-1, 0), 9));
		});

		It("codifica en poco espacio tanto claros contiguos como patrones dispersos", [this]()
		{
			FSaveIndexSet Contiguous;
			for (int32 I = 0; I < 10000; ++I)
			{
				Contiguous.Add(I);
			}
			TestEqual(TEXT("Rango único"), Contiguous.Encode(), FString(TEXT("r:0-9999")));

			FExploredRandom Random(7);
			FSaveIndexSet Scattered;
			for (int32 I = 0; I < 4096; ++I)
			{
				if (Random.Chance(0.5f))
				{
					Scattered.Add(I);
				}
			}
			const FString Encoded = Scattered.Encode();
			TestTrue(TEXT("Mapa de bits para lo disperso"), Encoded.StartsWith(TEXT("b:")));
			TestTrue(TEXT("Cota: 6 bits por carácter"), Encoded.Len() <= 4096 / 6 + 4);
			FSaveIndexSet Back;
			TestTrue(TEXT("Se lee"), Back.Decode(Encoded));
			TestTrue(TEXT("Igual"), Back == Scattered);

			FSaveIndexSet Few;
			Few.Add(3);
			Few.Add(700);
			TestEqual(TEXT("Pocos índices como rangos"), Few.Encode(), FString(TEXT("r:3,700")));
			FSaveIndexSet FewBack;
			TestTrue(TEXT("Rangos ida y vuelta"), FewBack.Decode(Few.Encode()) && FewBack == Few);
			FSaveIndexSet Empty;
			TestTrue(TEXT("Vacío"), Empty.Decode(Empty.Encode()) && Empty.IsEmpty());

			// Peor caso: cada celda cuesta como mucho un bit por instancia posible (más la cabecera).
			FSaveIndexSet Max;
			Max.Add(FSaveIndexSet::MaxIndex);
			TestTrue(TEXT("Índice máximo en rangos"), Max.Encode().Len() < 16);
		});

		It("rechaza deltas manipulados sin reservar memoria desmedida", [this]()
		{
			const TArray<FString> Bad = {
				TEXT(""), TEXT("x:1"), TEXT("r:5-3"), TEXT("r:3,2"), TEXT("r:1,1"), TEXT("r:01"), TEXT("r:1,"),
				TEXT("r:-1"), TEXT("r:99999999999"), TEXT("r:1048576"), TEXT("b:!!"), TEXT("b:A"), TEXT("b:AB"),
			};
			for (const FString& Text : Bad)
			{
				FSaveIndexSet Set;
				Set.Add(1);
				TestFalse(FString::Printf(TEXT("Rechaza «%s»"), *Text), Set.Decode(Text));
				TestTrue(TEXT("Queda vacío"), Set.IsEmpty());
			}
			FString Huge = TEXT("b:");
			for (int32 I = 0; I < 200000; ++I)
			{
				Huge.AppendChar(TEXT('/'));
			}
			FSaveIndexSet Set;
			TestFalse(TEXT("Mapa de bits demasiado grande"), Set.Decode(Huge));

			FSaveScatterDeltas Deltas;
			FSaveValue Value;
			FString Error;
			FSaveText::Parse(TEXT("[[1, 2, \"r:0-3\"], [1, 2, \"r:5\"], [\"x\", 0, \"r:1\"]]"), Value, Error);
			TestFalse(TEXT("Celda con coordenada no entera"), Deltas.FromValue(Value));
			FSaveText::Parse(TEXT("[[1, 2, \"r:0-3\"], [1, 2, \"r:5\"]]"), Value, Error);
			TestTrue(TEXT("Celdas repetidas se unen"), Deltas.FromValue(Value) && Deltas.Num() == 5 && Deltas.NumCells() == 1);
		});

		It("acota la memoria total de muchas celdas con el índice máximo", [this]()
		{
			// "r:1048575" son 11 caracteres y 128 KiB: sin tope, 90.000 celdas pedirían ~11 GB.
			auto MakeCells = [](int32 NumCells)
			{
				FSaveValue Cells = FSaveValue::MakeArray();
				for (int32 I = 0; I < NumCells; ++I)
				{
					FSaveValue Entry = FSaveValue::MakeArray();
					Entry.Add(FSaveValue::MakeInt(I));
					Entry.Add(FSaveValue::MakeInt(0));
					Entry.Add(FSaveValue::MakeString(FString::Printf(TEXT("r:%d"), FSaveIndexSet::MaxIndex)));
					Cells.Add(MoveTemp(Entry));
				}
				return Cells;
			};
			const int32 WordsPerCell = (FSaveIndexSet::MaxIndex + 1) / 32;
			const int32 CellsAtCap = FSaveScatterDeltas::MaxLoadedWords / WordsPerCell;

			FSaveScatterDeltas Deltas;
			TestTrue(TEXT("Justo en el tope se carga"), Deltas.FromValue(MakeCells(CellsAtCap)));
			TestFalse(TEXT("Una celda más se rechaza"), Deltas.FromValue(MakeCells(CellsAtCap + 1)));
			TestTrue(TEXT("Y queda vacío"), Deltas.IsEmpty());

			// El tope vale también para la suma de capas.
			FSaveValue Layers = FSaveValue::MakeObject();
			Layers.Set(TEXT("destroyed"), MakeCells(CellsAtCap));
			Layers.Set(TEXT("harvested"), MakeCells(CellsAtCap));
			FSaveArchive Ar;
			Ar.SetValue(TEXT("layers"), Layers);
			FSaveWorldDeltas World;
			World.Load(Ar);
			TestEqual(TEXT("Solo cabe una de las dos capas"), World.Layers.Num(), 1);
		});
	});

	Describe(TEXT("el estado del jugador"), [this]()
	{
		It("viaja por el documento completo sin perder precisión", [this]()
		{
			FSavePlayerState Player;
			Player.Location = FVector(123456.789, -98765.4321, 250.0625);
			Player.ControlRotation = FRotator(-12.5, 271.25, 0.0);
			Player.Velocity = FVector(1.0 / 3.0, 0.0, -9.81);
			Player.Health = 42.5f;
			Player.Thirst = 0.1f;
			Player.Wetness = 0.7f;

			FSaveSectionRegistry Registry;
			FSavePlayerState Loaded;
			Registry.Register(TEXT("player"),
				[&Player](FSaveArchive& Ar) { Player.Save(Ar); },
				[&Loaded](const FSaveArchive& Ar) { Loaded.Load(Ar); });

			FSaveDocument Document;
			Document.Sections = Registry.Capture();
			FSaveDocument Read;
			FString Error;
			TestTrue(TEXT("Se lee"), FSaveCodec::Decode(FSaveCodec::Encode(Document), FSaveMigrations(),
				ExploredSave::CurrentFormatVersion, Read, Error) == ESaveLoadResult::Ok);
			Registry.Apply(Read.Sections);
			TestTrue(TEXT("Estado idéntico"), Loaded == Player);

			Registry.Apply(FSaveValue::MakeObject());
			TestTrue(TEXT("Sin sección: jugador por defecto"), Loaded == FSavePlayerState());
		});
	});
}

#endif
