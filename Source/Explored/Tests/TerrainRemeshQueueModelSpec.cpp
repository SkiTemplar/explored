#include "Misc/AutomationTest.h"

#include "WorldGen/TerrainRemeshQueueModel.h"
#include "WorldGen/TerrainReplacementModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainRemeshQueueSpecDetail
{
	/** Prioridad: distancia al origen (el jugador). */
	double ByDistance(const FIntVector& Chunk)
	{
		return FVector(Chunk.X, Chunk.Y, Chunk.Z).Size();
	}

	TArray<FIntVector> Select(FTerrainRemeshQueueModel& Queue, double Now, int32 MaxInFlight = 8, int32 MaxStarts = 8)
	{
		TArray<FIntVector> Out;
		Queue.SelectStarts(Now, MaxInFlight, MaxStarts, [](const FIntVector& C) { return ByDistance(C); }, Out);
		return Out;
	}

	int32 AsInt(ETerrainReplacementState S) { return static_cast<int32>(S); }
}

BEGIN_DEFINE_SPEC(FTerrainRemeshQueueModelSpec, "Explored.TerrainRemeshQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainRemeshQueueModelSpec)

void FTerrainRemeshQueueModelSpec::Define()
{
	using namespace TerrainRemeshQueueSpecDetail;

	Describe("cola de remallado", [this]()
	{
		It("lanza lo sucio por cercanía y respeta los topes", [this]()
		{
			FTerrainRemeshQueueModel Queue;
			Queue.MarkDirty({FIntVector(5, 0, 0), FIntVector(1, 0, 0), FIntVector(3, 0, 0), FIntVector(1, 0, 0)});
			TestEqual(TEXT("sin repetir"), Queue.NumDirty(), 3);
			const TArray<FIntVector> First = Select(Queue, 0.0, 8, 2);
			TestTrue(TEXT("los dos más cercanos, en orden"), First.Num() == 2 && First[0] == FIntVector(1, 0, 0) && First[1] == FIntVector(3, 0, 0));
			TestEqual(TEXT("en vuelo"), Queue.NumInFlight(), 2);
			TestEqual(TEXT("tope de vuelo"), Select(Queue, 0.0, 2, 8).Num(), 0);
			TestEqual(TEXT("con hueco, el resto"), Select(Queue, 0.0, 8, 8).Num(), 1);
			TestTrue(TEXT("nada sucio"), Queue.NumDirty() == 0 && !Queue.IsIdle());
		});

		It("un chunk en vuelo que se ensucia espera a volver y al intervalo mínimo", [this]()
		{
			FTerrainRemeshQueueModel Queue;
			const FIntVector C(0, 0, 0);
			Queue.MarkDirty(C);
			TestEqual(TEXT("sale"), Select(Queue, 10.0).Num(), 1);
			Queue.MarkDirty(C);
			TestEqual(TEXT("en vuelo: no se relanza"), Select(Queue, 11.0).Num(), 0);
			Queue.MarkFinished(C);
			TestEqual(TEXT("vuelve a salir pasado el intervalo"), Select(Queue, 11.0).Num(), 1);
			Queue.MarkFinished(C);
			Queue.MarkDirty(C);
			TestEqual(TEXT("golpes seguidos se agrupan"), Select(Queue, 11.1).Num(), 0);
			TestEqual(TEXT("a los 0,15 s sale"), Select(Queue, 11.0 + FTerrainRemeshQueueModel::MinSecondsBetweenStarts).Num(), 1);
			Queue.MarkFinished(C);
			TestTrue(TEXT("en reposo"), Queue.IsIdle());
		});

		It("vaciado forzoso y relojes rotos", [this]()
		{
			FTerrainRemeshQueueModel Queue;
			const FIntVector C(2, 2, 2);
			Queue.MarkDirty(C);
			Select(Queue, 5.0);
			Queue.MarkFinished(C);
			Queue.MarkDirty(C);
			TestEqual(TEXT("NaN no arranca nada"), Select(Queue, std::numeric_limits<double>::quiet_NaN()).Num(), 0);
			TestEqual(TEXT("intervalo"), Select(Queue, 5.01).Num(), 0);
			Queue.ClearThrottle();
			TestEqual(TEXT("sin freno"), Select(Queue, 5.01).Num(), 1);
			Queue.Reset();
			TestTrue(TEXT("reset"), Queue.IsIdle());
		});
	});

	Describe("sustitución de chunks horneados", [this]()
	{
		It("Baked → Surveying → Building → Replaced cuando todos tienen malla", [this]()
		{
			FTerrainReplacementModel Model(8);
			const FIntVector R(1, 0, 0);
			TestTrue(TEXT("primera petición"), Model.Request(R));
			TestFalse(TEXT("idempotente"), Model.Request(R));
			TestEqual(TEXT("sondeando"), AsInt(Model.GetState(R)), AsInt(ETerrainReplacementState::Surveying));
			TArray<FIntVector> Done;
			Model.SetRequired(R, {FIntVector(8, 0, 0), FIntVector(9, 1, 0), FIntVector(0, 0, 0)}, Done);
			TestEqual(TEXT("construyendo"), AsInt(Model.GetState(R)), AsInt(ETerrainReplacementState::Building));
			TestEqual(TEXT("el de otro chunk de render no cuenta"), Model.NumMissing(R), 2);
			Model.OnEditChunkMeshed(FIntVector(8, 0, 0), Done);
			TestEqual(TEXT("aún no"), Done.Num(), 0);
			Model.OnEditChunkMeshed(FIntVector(9, 1, 0), Done);
			TestTrue(TEXT("sustituido"), Done.Num() == 1 && Done[0] == R);
			TestEqual(TEXT("estado"), AsInt(Model.GetState(R)), AsInt(ETerrainReplacementState::Replaced));
			TestFalse(TEXT("no vuelve a pedirse"), Model.Request(R));
		});

		It("lo ya mallado cuenta y sin superficie se sustituye al momento", [this]()
		{
			FTerrainReplacementModel Model(8);
			TArray<FIntVector> Done;
			Model.OnEditChunkMeshed(FIntVector(0, 0, 0), Done);
			Model.Request(FIntVector(0, 0, 0));
			Model.SetRequired(FIntVector(0, 0, 0), {FIntVector(0, 0, 0)}, Done);
			TestTrue(TEXT("ya tenía malla"), Done.Num() == 1);
			Model.Request(FIntVector(-1, 0, 0));
			Model.SetRequired(FIntVector(-1, 0, 0), {}, Done);
			TestEqual(TEXT("vacío: sustituido"), AsInt(Model.GetState(FIntVector(-1, 0, 0))), AsInt(ETerrainReplacementState::Replaced));
			TestEqual(TEXT("activos"), Model.ActiveRenderChunks().Num(), 2);
			TestTrue(TEXT("del chunk (-1)"), Model.RenderChunkOf(FIntVector(-1, 0, 0)) == FIntVector(-1, 0, 0));
			Model.Reset();
			TestEqual(TEXT("reset"), AsInt(Model.GetState(FIntVector(0, 0, 0))), AsInt(ETerrainReplacementState::Baked));
			TestFalse(TEXT("reset olvida las mallas"), Model.IsMeshed(FIntVector(0, 0, 0)));
		});

		It("SetRequired fuera de Surveying no hace nada", [this]()
		{
			FTerrainReplacementModel Model(8);
			TArray<FIntVector> Done;
			Model.SetRequired(FIntVector(3, 3, 3), {}, Done);
			TestEqual(TEXT("sigue horneado"), AsInt(Model.GetState(FIntVector(3, 3, 3))), AsInt(ETerrainReplacementState::Baked));
			TestEqual(TEXT("nada completado"), Done.Num(), 0);
		});
	});
}

#endif
