#include "Debug/NetBudgetTableModel.h"

#include "Carry/ContainerReplicationModel.h"
#include "Core/SystemLinks.h"
#include "Fauna/FaunaAnchorNetModel.h"
#include "Sky/WorldClockNetModel.h"
#include "WorldGen/VegetationNetStateModel.h"

namespace NetBudgetTableDetail
{
	FNetBudgetChannel Rate(const TCHAR* Name, double Bytes, double Hz, double Count = 1.0)
	{
		FNetBudgetChannel C;
		C.Name = Name;
		C.BytesPerMessage = Bytes;
		C.MessagesPerSecond = Hz;
		C.Count = Count;
		return C;
	}

	FNetBudgetChannel Fixed(const TCHAR* Name, double Kbps)
	{
		FNetBudgetChannel C;
		C.Name = Name;
		C.FixedKbps = Kbps;
		return C;
	}
}

TArray<FNetBudgetChannel> FNetBudgetTableModel::RestTable(int32 Players)
{
	using namespace NetBudgetTableDetail;
	const double Others = ExploredLinks::CoopPlayersClamped(Players) - 1;
	TArray<FNetBudgetChannel> T;
	T.Add(Rate(TEXT("personajes_ajenos"), CharacterBytes, CharacterHz, Others));
	T.Add(Rate(TEXT("fauna_terrestre_cerca"), TerrestrialAnimalBytes, NearAnimalsHz, NearAnimalsCap));
	T.Add(Rate(TEXT("fauna_terrestre_lejos"), TerrestrialAnimalBytes, FarAnimalsHz, FarAnimalsCap));
	T.Add(Rate(TEXT("anclas_fauna_ambiente"), FFaunaAnchorNetModel::PacketBytes, 1.0 / FFaunaAnchorNetModel::SendIntervalSeconds, FFaunaAnchorNetModel::MaxGroupsPerClient));
	T.Add(Rate(TEXT("cuerpo_propio"), OwnBodyBytes, OwnBodyHz));
	T.Add(Rate(TEXT("cuerpo_otros"), OtherBodyBytes, OtherBodyHz, Others));
	T.Add(Rate(TEXT("reloj_y_clima"), FWorldClockNetModel::PacketBytes, 1.0 / FWorldClockNetModel::SendIntervalSeconds));
	T.Add(Rate(TEXT("verificacion_chunks"), ChunkChecksBytesPerSecond, 1.0));
	T.Add(Fixed(TEXT("canales_de_actor_y_acuses"), ActorChannelsReserveKbps));
	return T;
}

TArray<FNetBudgetChannel> FNetBudgetTableModel::PeakTable(int32 Players)
{
	using namespace NetBudgetTableDetail;
	TArray<FNetBudgetChannel> T = RestTable(Players);
	// Terreno: cifras de la cola de deltas de 08 §2.2 (modelo propio en la rama de H0 de red).
	T.Add(Fixed(TEXT("deltas_pala"), 2.8));
	T.Add(Fixed(TEXT("deltas_pico_segundo_jugador"), 0.5));
	T.Add(Fixed(TEXT("arena_viva"), 0.6));
	// Dos talas a la vez a menos de 60 m: un golpe por segundo cada una.
	T.Add(Rate(TEXT("tala_dos_arboles"), FVegetationNetStateModel::BytesPerChange, 1.0, 2.0));
	T.Add(Rate(TEXT("barcos_ocupados"), BoatStateBytes, BoatStateHz, 2.0));
	T.Add(Rate(TEXT("objetos_sueltos_despiertos"), LooseObjectBytes, LooseObjectHz, LooseObjectsAwakeCap));
	// Fabricar mueve 2–3 huecos por operación, coalescido a 10 Hz (08 §2.4).
	T.Add(Rate(TEXT("inventario_propio_fabricando"), 3.0 * FContainerReplicationModel::EntryBytes, 10.0));
	// Cofre abierto con otro jugador sacando cosas: un hueco cambiado por segundo.
	T.Add(Rate(TEXT("cofre_abierto"), FContainerReplicationModel::MessageHeaderBytes + FContainerReplicationModel::EntryBytes, 1.0)); // loc: ignorar
	T.Add(Fixed(TEXT("cartografia_explorando"), 0.3));
	T.Add(Fixed(TEXT("multicast_cosmeticos"), 2.0));
	return T;
}

TArray<FNetBudgetChannel> FNetBudgetTableModel::PeakWithTerrainBurstTable(int32 Players)
{
	TArray<FNetBudgetChannel> T = PeakTable(Players);
	T.Add(NetBudgetTableDetail::Fixed(TEXT("rafaga_terreno"), TerrainBurstKbps));
	return T;
}

double FNetBudgetTableModel::TotalKbps(const TArray<FNetBudgetChannel>& Channels)
{
	double Total = 0.0;
	for (const FNetBudgetChannel& C : Channels)
	{
		Total += C.Kbps();
	}
	return Total;
}

double FNetBudgetTableModel::Margin01(const TArray<FNetBudgetChannel>& Channels, double TargetKbps)
{
	if (!(TargetKbps > 0.0) || !FMath::IsFinite(TargetKbps))
	{
		return -1.0;
	}
	return 1.0 - TotalKbps(Channels) / TargetKbps;
}

bool FNetBudgetTableModel::FitsTarget(const TArray<FNetBudgetChannel>& Channels, double TargetKbps, FString& OutReason)
{
	const double Total = TotalKbps(Channels);
	// Un canal con NaN no puede dar un presupuesto «bueno» por comparar en positivo.
	if (FMath::IsFinite(Total) && FMath::IsFinite(TargetKbps) && Total < TargetKbps)
	{
		OutReason.Empty();
		return true;
	}
	TArray<FNetBudgetChannel> Sorted = Channels;
	Sorted.StableSort([](const FNetBudgetChannel& A, const FNetBudgetChannel& B) { return A.Kbps() > B.Kbps(); });
	OutReason = FString::Printf(TEXT("%.2f kbps no cabe en %.2f kbps; los más caros:"), Total, TargetKbps); // loc: ignorar
	for (int32 i = 0; i < FMath::Min(3, Sorted.Num()); ++i)
	{
		OutReason += FString::Printf(TEXT(" %s %.2f"), *Sorted[i].Name, Sorted[i].Kbps());
	}
	return false;
}

FString FNetBudgetTableModel::ToCsv(const TArray<FNetBudgetChannel>& Channels)
{
	FString Out = TEXT("canal;kbps\n"); // loc: ignorar
	for (const FNetBudgetChannel& C : Channels)
	{
		Out += FString::Printf(TEXT("%s;%.3f\n"), *C.Name, C.Kbps());
	}
	Out += FString::Printf(TEXT("total;%.3f\n"), TotalKbps(Channels)); // loc: ignorar
	return Out;
}
