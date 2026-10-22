#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "Save/SaveFormat.h"
#include "Save/SavePlayerState.h"
#include "Save/SaveSlots.h"

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
			TestTrue(TEXT("Amanecer de la historia"), FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Story, 1.0));
			TestEqual(TEXT("La historia va a auto"), FSaveSlotPolicy::SlotForTrigger(ESaveTrigger::Story, TEXT("manual2")), FString(TEXT("auto")));
			TestTrue(TEXT("Primera hoguera"), FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Campfire, -1.0));
			TestFalse(TEXT("Hoguera seguida"), FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Campfire, 30.0));
			TestTrue(TEXT("Hoguera tras el intervalo"),
				FSaveSlotPolicy::ShouldAutosave(ESaveTrigger::Campfire, FSaveSlotPolicy::MinSecondsBetweenCampfireSaves));
			TestEqual(TEXT("Dormir va a auto"), FSaveSlotPolicy::SlotForTrigger(ESaveTrigger::Sleep, TEXT("manual1")), FString(TEXT("auto")));
			TestEqual(TEXT("Manual va a su ranura"), FSaveSlotPolicy::SlotForTrigger(ESaveTrigger::Manual, TEXT("Manual1")), FString(TEXT("manual1")));
		});

		It("pide confirmar la salida al menú solo con progreso sin guardar", [this]()
		{
			TestFalse(TEXT("Recién guardado"), FSaveSlotPolicy::HasUnsavedProgress(600.0, 595.0));
			TestTrue(TEXT("Un rato después"), FSaveSlotPolicy::HasUnsavedProgress(600.0, 500.0));
			TestFalse(TEXT("Partida recién empezada"), FSaveSlotPolicy::HasUnsavedProgress(5.0, -1.0));
			TestTrue(TEXT("Nunca guardada"), FSaveSlotPolicy::HasUnsavedProgress(300.0, -1.0));
			TestTrue(TEXT("Reloj no válido: se pregunta"), FSaveSlotPolicy::HasUnsavedProgress(std::numeric_limits<double>::quiet_NaN(), 0.0));
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

		It("lee un tiempo de juego no finito o negativo como 0", [this]()
		{
			// Con "NaN", SortForContinue dejaría de ser un orden estricto débil.
			for (const TCHAR* Text : { TEXT("NaN"), TEXT("Infinity"), TEXT("-Infinity") })
			{
				FSaveValue Value = FSaveHeader().ToValue();
				Value.Set(TEXT("playTimeSeconds"), FSaveValue::MakeString(Text));
				FSaveHeader Header;
				TestTrue(FString::Printf(TEXT("Cabecera con «%s» legible"), Text), Header.FromValue(Value));
				TestEqual(FString::Printf(TEXT("«%s» cuenta como 0"), Text), Header.PlayTimeSeconds, 0.0);
			}
			FSaveValue Negative = FSaveHeader().ToValue();
			Negative.Set(TEXT("playTimeSeconds"), FSaveValue::MakeDouble(-50.0));
			FSaveHeader Header;
			Header.FromValue(Negative);
			TestEqual(TEXT("Negativo cuenta como 0"), Header.PlayTimeSeconds, 0.0);
		});
	});

	Describe(TEXT("el estado del jugador"), [this]()
	{
		It("rechaza posiciones no finitas o fuera de la isla y anula rotaciones no finitas", [this]()
		{
			const double NaN = std::numeric_limits<double>::quiet_NaN();
			const double Inf = std::numeric_limits<double>::infinity();
			FSavePlayerState Good;
			Good.Location = FVector(-9800.0, 4200.0, 350.0);
			TestTrue(TEXT("En la isla"), Good.HasPlausibleLocation());
			for (const FVector& Bad : { FVector(NaN, 0.0, 0.0), FVector(0.0, Inf, 0.0), FVector(0.0, 0.0, -Inf),
				FVector(200001.0, 0.0, 0.0), FVector(0.0, -5.0e7, 0.0) })
			{
				FSavePlayerState State;
				State.Location = Bad;
				TestFalse(FString::Printf(TEXT("Rechaza (%f, %f, %f)"), Bad.X, Bad.Y, Bad.Z), State.HasPlausibleLocation());
			}

			// Una partida editada a mano: el escritor guarda NaN e infinitos y el lector los devuelve.
			FSaveArchive Ar;
			Ar.Write(TEXT("location"), FVector(NaN, 0.0, 0.0));
			Ar.Write(TEXT("controlRotation"), FRotator(NaN, 90.0, 0.0));
			Ar.Write(TEXT("velocity"), FVector(Inf, 0.0, 0.0));
			FSaveValue Parsed;
			FString Error;
			TestTrue(TEXT("Texto válido"), FSaveText::Parse(FSaveText::Write(Ar.GetRoot()), Parsed, Error));
			FSavePlayerState Loaded;
			Loaded.Load(FSaveArchive(Parsed));
			TestFalse(TEXT("Posición descartada"), Loaded.HasPlausibleLocation());
			TestTrue(TEXT("Rotación a cero"), Loaded.ControlRotation == FRotator::ZeroRotator);
			TestTrue(TEXT("Velocidad a cero"), Loaded.Velocity == FVector::ZeroVector);
		});

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
