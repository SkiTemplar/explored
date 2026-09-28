#include "Misc/AutomationTest.h"

#include <limits>

#include "Debug/NetBudgetModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNetBudgetModelSpec, "Explored.NetBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	/** Serie de un cliente con los kbps de terreno dados, uno por segundo, desde el segundo First. */
	TArray<FNetBudgetModel::FSecondSummary> TerrainSeries(const TArray<double>& Kbps, int64 First = 0, const TCHAR* Client = TEXT("Jugador1"))
	{
		TArray<FNetBudgetModel::FSecondSummary> Rows;
		for (int32 I = 0; I < Kbps.Num(); ++I)
		{
			FNetBudgetModel::FSecondSummary Row;
			Row.Client = FName(Client);
			Row.SecondIndex = First + I;
			Row.Channels.Add(FNetBudgetModel::FChannelRow{ FNetBudgetModel::TerrainChannel(), Kbps[I] });
			Row.TotalKbps = Kbps[I];
			Rows.Add(Row);
		}
		return Rows;
	}
END_DEFINE_SPEC(FNetBudgetModelSpec)

void FNetBudgetModelSpec::Define()
{
	Describe("Acumulación por segundo", [this]()
	{
		It("acumula bytes por canal dentro del mismo segundo y solo cierra al pasar al siguiente", [this]()
		{
			FNetBudgetModel Model;
			const FName Client(TEXT("Jugador1"));

			Model.RecordBytes(Client, FName(TEXT("Terreno")), 1000, 0.2);
			Model.RecordBytes(Client, FName(TEXT("Terreno")), 500, 0.9);
			Model.RecordBytes(Client, FName(TEXT("Fauna")), 200, 0.95);

			TArray<FNetBudgetModel::FSecondSummary> Rows;
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("El segundo 0 sigue abierto: nada que drenar todavía"), Rows.Num(), 0);

			// Un byte en el segundo 1 cierra el segundo 0.
			Model.RecordBytes(Client, FName(TEXT("Terreno")), 10, 1.1);
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("Ahora sí: una fila cerrada"), Rows.Num(), 1);
			if (Rows.Num() == 1)
			{
				const FNetBudgetModel::FSecondSummary& Row = Rows[0];
				TestTrue(TEXT("Cliente correcto"), Row.Client == Client);
				TestEqual(TEXT("Segundo 0"), Row.SecondIndex, (int64)0);
				TestEqual(TEXT("Dos canales"), Row.Channels.Num(), 2);
				if (Row.Channels.Num() == 2)
				{
					// Orden alfabético: Fauna antes que Terreno.
					TestTrue(TEXT("Primero Fauna"), Row.Channels[0].Channel == FName(TEXT("Fauna")));
					TestEqual(TEXT("Fauna: 200 B = 1.6 kbps"), Row.Channels[0].Kbps, 1.6, 1e-9);
					TestTrue(TEXT("Luego Terreno"), Row.Channels[1].Channel == FName(TEXT("Terreno")));
					TestEqual(TEXT("Terreno: 1500 B = 12.0 kbps"), Row.Channels[1].Kbps, 12.0, 1e-9);
				}
				TestEqual(TEXT("Total: 13.6 kbps"), Row.TotalKbps, 13.6, 1e-9);
			}

			// Drenar dos veces no repite la misma fila.
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("Un segundo cerrado solo se drena una vez"), Rows.Num(), 0);
		});

		It("cada cliente cierra sus segundos de forma independiente, y las filas salen ordenadas", [this]()
		{
			FNetBudgetModel Model;
			const FName Alice(TEXT("Alice"));
			const FName Bob(TEXT("Bob"));

			Model.RecordBytes(Bob, FName(TEXT("Terreno")), 100, 0.0);
			Model.RecordBytes(Alice, FName(TEXT("Terreno")), 100, 0.0);
			// Cierra el segundo 0 de los dos.
			Model.RecordBytes(Bob, FName(TEXT("Terreno")), 100, 1.0);
			Model.RecordBytes(Alice, FName(TEXT("Terreno")), 100, 1.0);

			TArray<FNetBudgetModel::FSecondSummary> Rows;
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("Dos filas cerradas (una por cliente)"), Rows.Num(), 2);
			if (Rows.Num() == 2)
			{
				TestTrue(TEXT("Alice va primero (orden alfabético de cliente)"), Rows[0].Client == Alice);
				TestTrue(TEXT("Bob va segundo"), Rows[1].Client == Bob);
			}
		});

		It("CloseAllOpenSeconds cierra el segundo en curso sin esperar más tráfico", [this]()
		{
			FNetBudgetModel Model;
			const FName Client(TEXT("Jugador1"));
			Model.RecordBytes(Client, FName(TEXT("Terreno")), 1000, 5.0);

			TArray<FNetBudgetModel::FSecondSummary> Rows;
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("Nada cerrado todavía"), Rows.Num(), 0);

			Model.CloseAllOpenSeconds();
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("El cierre forzado libera la fila"), Rows.Num(), 1);
		});
	});

	Describe("Validate", [this]()
	{
		It("compara el total contra el tope de reposo y el de pico, con motivo en el fallo", [this]()
		{
			FNetBudgetModel::FSecondSummary Row;
			Row.Client = FName(TEXT("Jugador1"));
			Row.SecondIndex = 3;

			Row.TotalKbps = 50.0;
			FString Reason;
			TestTrue(TEXT("50 kbps cumple el tope de reposo (64)"), FNetBudgetModel::Validate(Row, FNetBudgetModel::RestKbpsLimit, Reason));

			Row.TotalKbps = 70.0;
			TestFalse(TEXT("70 kbps supera el tope de reposo"), FNetBudgetModel::Validate(Row, FNetBudgetModel::RestKbpsLimit, Reason));
			TestFalse(TEXT("El motivo no viene vacío"), Reason.IsEmpty());

			Row.TotalKbps = 200.0;
			TestTrue(TEXT("200 kbps cumple el tope de pico (256)"), FNetBudgetModel::Validate(Row, FNetBudgetModel::PeakKbpsLimit, Reason));

			Row.TotalKbps = 300.0;
			TestFalse(TEXT("300 kbps supera incluso el tope de pico"), FNetBudgetModel::Validate(Row, FNetBudgetModel::PeakKbpsLimit, Reason));
		});
	});

	Describe("Entradas rotas", [this]()
	{
		It("ignora un instante no finito o absurdo sin comportamiento indefinido", [this]()
		{
			FNetBudgetModel Model;
			const FName Client(TEXT("Jugador1"));
			Model.RecordBytes(Client, FName(TEXT("Fauna")), 100, std::numeric_limits<double>::quiet_NaN());
			Model.RecordBytes(Client, FName(TEXT("Fauna")), 100, std::numeric_limits<double>::infinity());
			Model.RecordBytes(Client, FName(TEXT("Fauna")), 100, 1e300);
			Model.CloseAllOpenSeconds();
			TArray<FNetBudgetModel::FSecondSummary> Rows;
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("Nada registrado"), Rows.Num(), 0);
		});

		It("no desborda al sumar muchos bytes en el mismo segundo", [this]()
		{
			FNetBudgetModel Model;
			const FName Client(TEXT("Jugador1"));
			for (int32 I = 0; I < 4; ++I)
			{
				Model.RecordBytes(Client, FName(TEXT("Terreno")), 2000000000, 0.5);
			}
			Model.CloseAllOpenSeconds();
			TArray<FNetBudgetModel::FSecondSummary> Rows;
			Model.DrainClosedSeconds(Rows);
			TestEqual(TEXT("Una fila"), Rows.Num(), 1);
			if (Rows.Num() == 1)
			{
				TestEqual(TEXT("8e9 B = 64e6 kbps, sin dar la vuelta"), Rows[0].TotalKbps, 64000000.0, 1e-3);
			}
		});
	});

	Describe("ValidateSeries", [this]()
	{
		It("cinco segundos seguidos a 128 kbps de terreno pasan y el sexto no", [this]()
		{
			TArray<FString> Violations;
			TestTrue(TEXT("5 s de ráfaga"), FNetBudgetModel::ValidateSeries(TerrainSeries({ 128, 128, 128, 128, 128, 64, 64 }), FNetBudgetModel::EScenario::Peak, Violations));
			TestFalse(TEXT("6 s de ráfaga"), FNetBudgetModel::ValidateSeries(TerrainSeries({ 128, 128, 128, 128, 128, 128 }), FNetBudgetModel::EScenario::Peak, Violations));
			TestEqual(TEXT("Una violación, en el segundo 5"), Violations.Num(), 1);
			if (Violations.Num() == 1)
			{
				TestTrue(TEXT("Nombra el segundo"), Violations[0].Contains(TEXT("segundo 5")));
			}
		});

		It("la ráfaga se recupera al ritmo sostenido, también con segundos sin tráfico (sin fila)", [this]()
		{
			TArray<FString> Violations;
			// 5 s de ráfaga, 5 s a 0 y otros 5 de ráfaga: vale.
			TArray<FNetBudgetModel::FSecondSummary> Rows = TerrainSeries({ 128, 128, 128, 128, 128 });
			Rows.Append(TerrainSeries({ 128, 128, 128, 128, 128 }, 10));
			TestTrue(TEXT("Con 5 s de hueco se recupera entera"), FNetBudgetModel::ValidateSeries(Rows, FNetBudgetModel::EScenario::Peak, Violations));

			// Con solo 1 s de hueco, la segunda ráfaga se queda corta.
			Rows = TerrainSeries({ 128, 128, 128, 128, 128 });
			Rows.Append(TerrainSeries({ 128, 128, 128 }, 6));
			TestFalse(TEXT("Con 1 s de hueco no"), FNetBudgetModel::ValidateSeries(Rows, FNetBudgetModel::EScenario::Peak, Violations));
		});

		It("más de 128 kbps de terreno en un segundo es violación aunque haya crédito", [this]()
		{
			TArray<FString> Violations;
			TestFalse(TEXT("129 kbps"), FNetBudgetModel::ValidateSeries(TerrainSeries({ 129 }), FNetBudgetModel::EScenario::Peak, Violations));
			TestTrue(TEXT("64 kbps sostenidos para siempre"), FNetBudgetModel::ValidateSeries(TerrainSeries({ 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 }), FNetBudgetModel::EScenario::Peak, Violations));
		});

		It("aplica el techo del escenario al total: 64 kbps en reposo, 256 en pico", [this]()
		{
			FNetBudgetModel::FSecondSummary Row;
			Row.Client = FName(TEXT("Jugador2"));
			Row.Channels.Add(FNetBudgetModel::FChannelRow{ FName(TEXT("Fauna")), 80.0 });
			Row.TotalKbps = 80.0;
			TArray<FString> Violations;
			TestFalse(TEXT("80 kbps en reposo"), FNetBudgetModel::ValidateSeries({ Row }, FNetBudgetModel::EScenario::Rest, Violations));
			TestTrue(TEXT("Dice que es el tope de reposo"), Violations.Num() == 1 && Violations[0].Contains(TEXT("reposo")));
			TestTrue(TEXT("80 kbps en pico"), FNetBudgetModel::ValidateSeries({ Row }, FNetBudgetModel::EScenario::Peak, Violations));
			TestTrue(TEXT("Y deja la lista vacía"), Violations.IsEmpty());
			Row.TotalKbps = 256.0;
			TestTrue(TEXT("256 justos pasan"), FNetBudgetModel::ValidateSeries({ Row }, FNetBudgetModel::EScenario::Peak, Violations));
			Row.TotalKbps = 256.01;
			TestFalse(TEXT("256,01 no"), FNetBudgetModel::ValidateSeries({ Row }, FNetBudgetModel::EScenario::Peak, Violations));
		});

		It("cada cliente lleva su propia ráfaga y el orden de entrada no importa", [this]()
		{
			TArray<FNetBudgetModel::FSecondSummary> Rows = TerrainSeries({ 128, 128, 128, 128, 128 }, 0, TEXT("A"));
			Rows.Append(TerrainSeries({ 128, 128, 128, 128, 128 }, 0, TEXT("B")));
			TArray<FNetBudgetModel::FSecondSummary> Reversed;
			for (int32 I = Rows.Num() - 1; I >= 0; --I) { Reversed.Add(Rows[I]); }
			Rows = Reversed;
			TArray<FString> Violations;
			TestTrue(TEXT("Dos clientes con 5 s de ráfaga cada uno, al revés"), FNetBudgetModel::ValidateSeries(Rows, FNetBudgetModel::EScenario::Peak, Violations));
		});

		It("un total o un canal no finito siempre es violación", [this]()
		{
			TArray<FNetBudgetModel::FSecondSummary> Rows = TerrainSeries({ 10 });
			Rows[0].TotalKbps = std::numeric_limits<double>::quiet_NaN();
			TArray<FString> Violations;
			TestFalse(TEXT("Total NaN"), FNetBudgetModel::ValidateSeries(Rows, FNetBudgetModel::EScenario::Peak, Violations));
			Rows = TerrainSeries({ std::numeric_limits<double>::infinity() });
			TestFalse(TEXT("Canal infinito"), FNetBudgetModel::ValidateSeries(Rows, FNetBudgetModel::EScenario::Peak, Violations));
		});
	});

	Describe("ToCsv", [this]()
	{
		It("escribe una línea por canal y una de total, con punto decimal y comillas donde hacen falta", [this]()
		{
			FNetBudgetModel::FSecondSummary Row;
			Row.Client = FName(TEXT("Ana, la del barco"));
			Row.SecondIndex = 12;
			Row.Channels.Add(FNetBudgetModel::FChannelRow{ FName(TEXT("Fauna")), 1.6 });
			Row.Channels.Add(FNetBudgetModel::FChannelRow{ FName(TEXT("Terreno")), 12.0 });
			Row.TotalKbps = 13.6;
			const FString Csv = FNetBudgetModel::ToCsv({ Row });
			const FString Expected = FString(TEXT("cliente,segundo,canal,kbps\n"))
				+ TEXT("\"Ana, la del barco\",12,Fauna,1.600\n")
				+ TEXT("\"Ana, la del barco\",12,Terreno,12.000\n")
				+ TEXT("\"Ana, la del barco\",12,total,13.600\n");
			TestEqual(TEXT("CSV exacto"), Csv, Expected);
			TestEqual(TEXT("Sin filas, solo cabecera"), FNetBudgetModel::ToCsv({}), FString(TEXT("cliente,segundo,canal,kbps\n")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
