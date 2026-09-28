#include "Misc/AutomationTest.h"

#include "Debug/NetBudgetTableModel.h"
#include "Fauna/FaunaAnchorNetModel.h"
#include "Sky/WorldClockNetModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNetBudgetTableModelSpec, "Explored.Net.BudgetTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FNetBudgetTableModelSpec)

namespace NetBudgetTableSpecDetail
{
	double ChannelKbps(const TArray<FNetBudgetChannel>& Table, const TCHAR* Name)
	{
		for (const FNetBudgetChannel& C : Table)
		{
			if (C.Name == Name)
			{
				return C.Kbps();
			}
		}
		return -1.0;
	}
}

void FNetBudgetTableModelSpec::Define()
{
	using namespace NetBudgetTableSpecDetail;

	Describe("Reposo (08 §3)", [this]()
	{
		It("suma el consumo por canal y cabe en menos de 64 kbps por cliente, con 4 jugadores", [this]()
		{
			const TArray<FNetBudgetChannel> Rest = FNetBudgetTableModel::RestTable(4);
			const double Total = FNetBudgetTableModel::TotalKbps(Rest);
			TestEqual(TEXT("≈ 33 kbps, como la tabla de la biblia"), Total, 33.07, 0.05);
			FString Reason;
			TestTrue(TEXT("Por debajo de 64 kbps"), FNetBudgetTableModel::FitsTarget(Rest, FNetBudgetTableModel::RestTargetKbps, Reason));
			TestTrue(TEXT("Con el 48 % de margen"), FNetBudgetTableModel::Margin01(Rest, FNetBudgetTableModel::RestTargetKbps) >= 0.48);
		});

		It("cabe con 1, 2, 3 y 4 jugadores, y crece con cada uno", [this]()
		{
			double Previous = 0.0;
			for (int32 Players = 1; Players <= 4; ++Players)
			{
				const double Total = FNetBudgetTableModel::TotalKbps(FNetBudgetTableModel::RestTable(Players));
				TestTrue(FString::Printf(TEXT("%d jugadores: %.2f kbps < 64"), Players, Total), Total < FNetBudgetTableModel::RestTargetKbps);
				TestTrue(TEXT("Más jugadores, más tráfico"), Total > Previous);
				Previous = Total;
			}
			TestEqual(TEXT("Más de 4 se trata como 4"), FNetBudgetTableModel::TotalKbps(FNetBudgetTableModel::RestTable(9)), FNetBudgetTableModel::TotalKbps(FNetBudgetTableModel::RestTable(4)));
		});

		It("usa los tamaños reales de los paquetes: fila a fila como la biblia", [this]()
		{
			const TArray<FNetBudgetChannel> Rest = FNetBudgetTableModel::RestTable(4);
			TestEqual(TEXT("3 personajes ajenos: 11,5"), ChannelKbps(Rest, TEXT("personajes_ajenos")), 11.52, 1e-9);
			TestEqual(TEXT("Fauna terrestre: 18,8"), ChannelKbps(Rest, TEXT("fauna_terrestre_cerca")) + ChannelKbps(Rest, TEXT("fauna_terrestre_lejos")), 18.816, 1e-9);
			TestEqual(TEXT("Anclas: 24 × 10 B × 0,5 Hz = 0,96"), ChannelKbps(Rest, TEXT("anclas_fauna_ambiente")), 0.96, 1e-9);
			TestEqual(TEXT("Reloj y clima: 11 B × 0,2 Hz"), ChannelKbps(Rest, TEXT("reloj_y_clima")), 11.0 * 0.2 * 8.0 / 1000.0, 1e-9);
			TestEqual(TEXT("Cuerpo propio: 0,19"), ChannelKbps(Rest, TEXT("cuerpo_propio")), 0.192, 1e-9);
			TestEqual(TEXT("Cuerpo de los otros: 0,024"), ChannelKbps(Rest, TEXT("cuerpo_otros")), 0.024, 1e-9);
			TestEqual(TEXT("El paquete de reloj sigue midiendo 11 B"), FWorldClockNetModel::PacketBytes, 11);
			TestEqual(TEXT("El ancla sigue midiendo 10 B"), FFaunaAnchorNetModel::PacketBytes, 10);
		});

		It("la fauna terrestre es la partida más cara en reposo (por eso su tope es duro)", [this]()
		{
			const TArray<FNetBudgetChannel> Rest = FNetBudgetTableModel::RestTable(4);
			const double Fauna = ChannelKbps(Rest, TEXT("fauna_terrestre_cerca")) + ChannelKbps(Rest, TEXT("fauna_terrestre_lejos"));
			for (const FNetBudgetChannel& C : Rest)
			{
				if (!C.Name.StartsWith(TEXT("fauna_terrestre")))
				{
					TestTrue(FString::Printf(TEXT("%s por debajo de la fauna"), *C.Name), C.Kbps() < Fauna);
				}
			}
		});
	});

	Describe("Pico (08 §3)", [this]()
	{
		It("el pico realista ronda los 80 kbps y cabe en 256", [this]()
		{
			const TArray<FNetBudgetChannel> Peak = FNetBudgetTableModel::PeakTable(4);
			TestEqual(TEXT("≈ 80 kbps (79,3 con los paquetes reales: entradas de 12 B)"), FNetBudgetTableModel::TotalKbps(Peak), 79.30, 0.01);
			FString Reason;
			TestTrue(TEXT("Cabe en 256"), FNetBudgetTableModel::FitsTarget(Peak, FNetBudgetTableModel::PeakTargetKbps, Reason));
			TestFalse(TEXT("Pero no en el objetivo de reposo"), FNetBudgetTableModel::FitsTarget(Peak, FNetBudgetTableModel::RestTargetKbps, Reason));
			TestEqual(TEXT("Dos barcos: 6,08"), ChannelKbps(Peak, TEXT("barcos_ocupados")), 6.08, 1e-9);
			TestEqual(TEXT("32 objetos sueltos: 30,7"), ChannelKbps(Peak, TEXT("objetos_sueltos_despiertos")), 30.72, 1e-9);
			TestTrue(TEXT("Inventario y cofre dentro de los 4 kbps de la biblia"), ChannelKbps(Peak, TEXT("inventario_propio_fabricando")) + ChannelKbps(Peak, TEXT("cofre_abierto")) <= 4.0);
		});

		It("incluso con la ráfaga entera de terreno queda por debajo de 256 (≈ 208)", [this]()
		{
			const TArray<FNetBudgetChannel> Burst = FNetBudgetTableModel::PeakWithTerrainBurstTable(4);
			TestEqual(TEXT("≈ 208 kbps"), FNetBudgetTableModel::TotalKbps(Burst), 207.30, 0.01);
			FString Reason;
			TestTrue(TEXT("Cabe en 256"), FNetBudgetTableModel::FitsTarget(Burst, FNetBudgetTableModel::PeakTargetKbps, Reason));
		});
	});

	Describe("Informe", [this]()
	{
		It("si no cabe, dice cuánto y los tres canales más caros", [this]()
		{
			TArray<FNetBudgetChannel> T = FNetBudgetTableModel::RestTable(4);
			FNetBudgetChannel Extra;
			Extra.Name = TEXT("fauna_sin_tope");
			Extra.BytesPerMessage = 14.0;
			Extra.MessagesPerSecond = 10.0;
			Extra.Count = 40.0;
			T.Add(Extra);
			FString Reason;
			TestFalse(TEXT("No cabe"), FNetBudgetTableModel::FitsTarget(T, FNetBudgetTableModel::RestTargetKbps, Reason));
			TestTrue(TEXT("Nombra el culpable primero"), Reason.Contains(TEXT("los más caros: fauna_sin_tope")));
			TestTrue(TEXT("Margen negativo"), FNetBudgetTableModel::Margin01(T, FNetBudgetTableModel::RestTargetKbps) < 0.0);
		});

		It("un canal con NaN nunca da el presupuesto por bueno", [this]()
		{
			TArray<FNetBudgetChannel> T = FNetBudgetTableModel::RestTable(2);
			T[0].BytesPerMessage = std::numeric_limits<double>::quiet_NaN();
			FString Reason;
			TestFalse(TEXT("NaN no cabe"), FNetBudgetTableModel::FitsTarget(T, FNetBudgetTableModel::RestTargetKbps, Reason));
			TestTrue(TEXT("Objetivo 0: margen −1"), FNetBudgetTableModel::Margin01(T, 0.0) == -1.0);
		});

		It("vuelca un CSV estable con la fila de total", [this]()
		{
			const FString Csv = FNetBudgetTableModel::ToCsv(FNetBudgetTableModel::RestTable(4));
			TestTrue(TEXT("Cabecera"), Csv.StartsWith(TEXT("canal;kbps\n")));
			TestTrue(TEXT("Fila de reloj"), Csv.Contains(TEXT("reloj_y_clima;0.018\n")));
			TestTrue(TEXT("Total"), Csv.Contains(TEXT("total;33.")));
			TestEqual(TEXT("Determinista"), Csv, FNetBudgetTableModel::ToCsv(FNetBudgetTableModel::RestTable(4)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
