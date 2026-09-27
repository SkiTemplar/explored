#include "Misc/AutomationTest.h"

#include "Debug/NetBudgetModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNetBudgetModelSpec, "Explored.NetBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
