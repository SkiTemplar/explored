#include "Misc/AutomationTest.h"

#include "WorldGen/TerrainNetSyncModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainNetSyncSpecDetail
{
	const auto Flat = [](const FVector& P) { return static_cast<float>(P.Z); };

	FSphereDig Dig(const FVector& Center)
	{
		FSphereDig D;
		D.Center = Center;
		D.Radius = 0.5f;
		D.Material = ETerrainMaterial::Tierra;
		D.ToolTier = 2;
		return D;
	}

	/** Servidor → cable → cliente, como hace UTerrainSyncComponent (sin la cola). */
	bool Replicate(const TArray<FTerrainDeltaCodecModel::FChunkPatch>& Patches, FTerrainEditModel& Client, TArray<FIntVector>& OutDirty)
	{
		for (const FTerrainDeltaCodecModel::FChunkPatch& Patch : Patches)
		{
			for (const TArray<uint8>& Bytes : FTerrainDeltaCodecModel::Encode(Patch))
			{
				FTerrainDeltaCodecModel::FPacket Packet;
				if (!FTerrainDeltaCodecModel::Decode(Bytes, Packet) || !FTerrainNetSyncModel::ApplyPacket(Client, Packet, OutDirty))
				{
					return false;
				}
			}
		}
		return true;
	}
}

BEGIN_DEFINE_SPEC(FTerrainNetSyncModelSpec, "Explored.TerrainNetSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainNetSyncModelSpec)

void FTerrainNetSyncModelSpec::Define()
{
	using namespace TerrainNetSyncSpecDetail;

	It("un golpe del servidor llega igual al cliente y ensucia los mismos chunks", [this]()
	{
		FTerrainEditModel Server;
		FTerrainEditModel Client;
		// En la esquina de 8 chunks de edición: el parche cruza bordes, aristas y esquina.
		const FTerrainEditResult Edit = Server.DigSphere(Dig(FVector(8.0, 8.0, 0.0)), Flat);
		TestTrue(TEXT("el servidor cava"), Edit.Changed());
		TestEqual(TEXT("una muestra cambiada por escritura"), Edit.ChangedSamples.Num(), Edit.SamplesChanged);

		const TArray<FTerrainDeltaCodecModel::FChunkPatch> Patches = FTerrainNetSyncModel::PatchesForSamples(Server, Edit.ChangedSamples);
		TestTrue(TEXT("varios chunks de guardado"), Patches.Num() > 1);
		TArray<FIntVector> Dirty;
		TestTrue(TEXT("se aplica"), Replicate(Patches, Client, Dirty));
		TestTrue(TEXT("mismo terreno"), Client == Server);
		TestTrue(TEXT("mismos chunks sucios"), Dirty == Edit.DirtyChunks);
		for (const FIntVector& Chunk : Server.EditedChunks())
		{
			TestTrue(TEXT("misma suma de control"), Client.ChunkChecksum(Chunk) == Server.ChunkChecksum(Chunk));
		}

		TArray<FIntVector> Again;
		TestTrue(TEXT("reenviar"), Replicate(Patches, Client, Again));
		TestEqual(TEXT("reenviar no ensucia nada (idempotente)"), Again.Num(), 0);
	});

	It("una muestra que vuelve a estar sin tocar viaja como 0 y se borra en el cliente", [this]()
	{
		FTerrainEditModel Server;
		FTerrainEditModel Client;
		const FIntVector Sample(3, 4, 5);
		Server.SetSampleDeltaMm(Sample, 250);
		TArray<FIntVector> Dirty;
		Replicate(FTerrainNetSyncModel::PatchesForSamples(Server, {Sample}), Client, Dirty);
		TestEqual(TEXT("llega"), Client.SampleDeltaMm(Sample), 250);
		Server.SetSampleDeltaMm(Sample, 0);
		const TArray<FTerrainDeltaCodecModel::FChunkPatch> Undo = FTerrainNetSyncModel::PatchesForSamples(Server, {Sample, Sample});
		TestEqual(TEXT("un parche"), Undo.Num(), 1);
		TestEqual(TEXT("sin repetir"), Undo[0].Samples.Num(), 1);
		TestEqual(TEXT("valor 0"), Undo[0].Samples[0].DeltaMm, 0);
		Replicate(Undo, Client, Dirty);
		TestTrue(TEXT("cliente vacío"), Client.IsEmpty());
	});

	It("un cliente que entra tarde recibe el estado completo", [this]()
	{
		FTerrainEditModel Server;
		for (int32 I = 0; I < 12; ++I)
		{
			Server.DigSphere(Dig(FVector(0.4 * I, -3.0 + 0.3 * I, -0.2 * I)), Flat);
		}
		TArray<FTerrainDeltaCodecModel::FChunkPatch> Full;
		for (const FIntVector& Chunk : Server.EditedChunks())
		{
			Full.Add(FTerrainNetSyncModel::FullChunkPatch(Server, Chunk));
		}
		FTerrainEditModel Late;
		TArray<FIntVector> Dirty;
		TestTrue(TEXT("se aplica"), Replicate(Full, Late, Dirty));
		TestTrue(TEXT("mismo terreno"), Late == Server);
		TestEqual(TEXT("chunk sin ediciones: parche vacío"), FTerrainNetSyncModel::FullChunkPatch(Server, FIntVector(90, 90, 90)).Samples.Num(), 0);
	});

	It("un paquete con una muestra imposible no toca nada", [this]()
	{
		FTerrainEditModel Client;
		FTerrainDeltaCodecModel::FPacket Packet;
		Packet.Chunk = FIntVector(1, 2, 3);
		Packet.Samples.Add({10, 100});
		Packet.Samples.Add({40000, 100});
		TArray<FIntVector> Dirty;
		TestFalse(TEXT("índice fuera del chunk"), FTerrainNetSyncModel::ApplyPacket(Client, Packet, Dirty));
		TestTrue(TEXT("intacto"), Client.IsEmpty());
		Packet.Samples = {{10, FTerrainEditModel::MaxDeltaMm + 1}};
		TestFalse(TEXT("delta fuera de rango"), FTerrainNetSyncModel::ApplyPacket(Client, Packet, Dirty));
		TestTrue(TEXT("sigue intacto"), Client.IsEmpty() && Dirty.Num() == 0);
	});

	It("la caja de las muestras y el orden de chunks", [this]()
	{
		const FTerrainEditModel Model;
		const FBox Box = FTerrainNetSyncModel::SamplesBounds(Model, {FIntVector(0, 0, 0), FIntVector(4, -8, 2)});
		TestEqual(TEXT("mínimo"), Box.Min, FVector(0.0, -2.0, 0.0), 1.0e-6f);
		TestEqual(TEXT("máximo"), Box.Max, FVector(1.0, 0.0, 0.5), 1.0e-6f);
		TestFalse(TEXT("sin muestras, caja vacía"), static_cast<bool>(FTerrainNetSyncModel::SamplesBounds(Model, {}).IsValid));
		TArray<FIntVector> Chunks = {FIntVector(1, 0, 0), FIntVector(0, 0, 1), FIntVector(0, 1, 0), FIntVector(1, 0, 0)};
		FTerrainNetSyncModel::SortUnique(Chunks);
		TestEqual(TEXT("sin repetir"), Chunks.Num(), 3);
		TestTrue(TEXT("orden (Z, Y, X)"), Chunks[0] == FIntVector(1, 0, 0) && Chunks[1] == FIntVector(0, 1, 0) && Chunks[2] == FIntVector(0, 0, 1));
	});
}

#endif
