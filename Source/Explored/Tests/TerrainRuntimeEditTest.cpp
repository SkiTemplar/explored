#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ProceduralMeshComponent.h"

#include "Mining/TerrainEditSubsystem.h"
#include "Mining/TerrainRuntimeMesher.h"
#include "Save/SaveArchive.h"
#include "Save/SaveValue.h"
#include "Save/SaveWorldDeltas.h"
#include "WorldGen/TerrainDensity.h"

namespace TerrainRuntimeTestDetail
{
	/** Mundo de juego mínimo (sin mapa ni terreno horneado): las mallas finas son todo el suelo. */
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), NAME_None);
			WorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
			World->AddToRoot();
			WorldContext->SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World->RemoveFromRoot();
			}
		}

		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
		FWorldContext* WorldContext = nullptr;
	};

	/** Superficie exacta de la columna (bisección sobre el campo procedural). */
	double SurfaceZ(const FTerrainDensity& Density, double X, double Y)
	{
		const double H = Density.SampleColumn(static_cast<float>(X), static_cast<float>(Y)).Height;
		double Lo = H - 6.0;
		double Hi = H + 6.0;
		for (int32 I = 0; I < 40; ++I)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(Density.ProceduralDensity(FVector(X, Y, Mid)) > 0.0f ? Hi : Lo) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	bool AwayFromChunkBorder(double V, double Margin)
	{
		const double Local = V - 64.0 * FMath::FloorToDouble(V / 64.0);
		return Local > Margin && Local < 64.0 - Margin;
	}

	/** Suelo llano de Landing, lejos de los bordes de los chunks de 64 m. */
	bool FindLandPoint(const FTerrainDensity& Density, FVector& Out)
	{
		for (const FIslandDesc& Island : Density.GetLayout().Islands)
		{
			if (Island.Archetype != EIslandArchetype::Landing)
			{
				continue;
			}
			for (int32 Ring = 1; Ring <= 12; ++Ring)
			{
				for (int32 Step = 0; Step < 16; ++Step)
				{
					const double Angle = Step * UE_TWO_PI / 16.0;
					const double R = Island.Radius * 0.05 * Ring;
					const double X = Island.Center.X + R * FMath::Cos(Angle);
					const double Y = Island.Center.Y + R * FMath::Sin(Angle);
					const double Z = SurfaceZ(Density, X, Y);
					const FVector P(X, Y, Z);
					if (Z > 2.0 && Z < 40.0 && Density.Normal(P).Z > 0.9 && AwayFromChunkBorder(X, 10.0) && AwayFromChunkBorder(Y, 10.0)
						&& AwayFromChunkBorder(Z, 4.0))
					{
						Out = P;
						return true;
					}
				}
			}
		}
		return false;
	}

	/** Altura (cm) del primer choque bajando por la vertical del punto; NaN si no hay nada. */
	double GroundCm(UWorld* World, const FVector& Meters, float SphereRadiusCm = 0.0f)
	{
		const FVector Start = (Meters + FVector(0.0, 0.0, 5.0)) * 100.0;
		const FVector End = (Meters - FVector(0.0, 0.0, 8.0)) * 100.0;
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(ExploredTerrainTest), true);
		const bool bHit = SphereRadiusCm > 0.0f
			? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(SphereRadiusCm), Params)
			: World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		return bHit ? Hit.ImpactPoint.Z : std::numeric_limits<double>::quiet_NaN();
	}

	/** Deja que la escena de física registre los cuerpos nuevos. */
	void TickWorld(UWorld* World, int32 Frames)
	{
		for (int32 I = 0; I < Frames; ++I)
		{
			World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		}
	}

	struct FContext
	{
		FAutomationTestBase& Test;
		UWorld* World = nullptr;
		UTerrainEditSubsystem* Terrain = nullptr;
		UTerrainRuntimeMesher* Mesher = nullptr;
		FVector Surface = FVector::ZeroVector;
		double GroundBefore = 0.0;
		double GroundDug = 0.0;
	};

	int32 NumTriangles(const UProceduralMeshComponent* Component)
	{
		const FProcMeshSection* Section = Component ? const_cast<UProceduralMeshComponent*>(Component)->GetProcMeshSection(0) : nullptr;
		return Section ? Section->ProcIndexBuffer.Num() / 3 : 0;
	}

	/** 1) Sustituir el chunk horneado: mallas finas con colisión, a la altura del terreno. */
	bool ReplaceAndMeasure(FContext& C)
	{
		C.Terrain->EnsureReplacedAt(C.Surface);
		const double Start = FPlatformTime::Seconds();
		C.Test.TestTrue(TEXT("sustitución terminada"), C.Terrain->FlushRemeshing(120.0));
		const FTerrainRemeshStats Stats = C.Terrain->GetRemeshStats();
		const FIntVector RenderChunk(FMath::FloorToInt32(C.Surface.X / 64.0), FMath::FloorToInt32(C.Surface.Y / 64.0),
			FMath::FloorToInt32(C.Surface.Z / 64.0));
		C.Test.TestTrue(TEXT("chunk de render sustituido"), C.Mesher->GetReplacementState(RenderChunk) == ETerrainReplacementState::Replaced);
		const FIntVector EditChunk(FMath::FloorToInt32(C.Surface.X / 8.0), FMath::FloorToInt32(C.Surface.Y / 8.0),
			FMath::FloorToInt32(C.Surface.Z / 8.0));
		if (!C.Test.TestTrue(TEXT("malla fina con triángulos"), NumTriangles(C.Mesher->FindChunkComponent(EditChunk)) > 0))
		{
			return false;
		}
		TickWorld(C.World, 2);
		C.GroundBefore = GroundCm(C.World, C.Surface);
		C.Test.TestTrue(FString::Printf(TEXT("el suelo colisiona a la altura del terreno (%.1f cm frente a %.1f)"), C.GroundBefore, C.Surface.Z * 100.0),
			FMath::IsFinite(C.GroundBefore) && FMath::Abs(C.GroundBefore - C.Surface.Z * 100.0) < 30.0);
		C.Test.AddInfo(FString::Printf(TEXT("Sustitución de un chunk de 64 m: %.2f s, %d chunks de 8 m, tarea media %.2f ms (máx %.2f)"),
			FPlatformTime::Seconds() - Start, Stats.ChunksApplied, Stats.TaskMsAverage, Stats.TaskMsMax));
		return true;
	}

	/** 2) Cavar con el pico (autoridad: partida sola) y remallar con el presupuesto del juego. */
	void DigAndRemesh(FContext& C)
	{
		C.Mesher->ResetStats();
		double Removed = 0.0;
		for (int32 I = 0; I < 16; ++I)
		{
			FTerrainDigHit Hit;
			Hit.ImpactPoint = C.Surface - FVector(0.0, 0.0, 0.15 * I);
			Hit.Material = ETerrainMaterial::Tierra;
			Hit.Tool = ETerrainDigTool::PicoRescatado;
			Removed += C.Terrain->Dig(Hit).Edit.VolumeRemoved;
		}
		C.Test.TestTrue(FString::Printf(TEXT("el pico arranca tierra (%.2f m³)"), Removed), Removed > 0.5);
		double EndOfFrameMsMax = 0.0;
		int32 Frames = 0;
		const double Deadline = FPlatformTime::Seconds() + 30.0;
		while (C.Mesher->NeedsTick() && FPlatformTime::Seconds() < Deadline)
		{
			C.Mesher->Tick(C.Terrain->GetModel(), C.Surface, 1.5);
			const double EndOfFrameStart = FPlatformTime::Seconds();
			C.World->SendAllEndOfFrameUpdates();
			EndOfFrameMsMax = FMath::Max(EndOfFrameMsMax, (FPlatformTime::Seconds() - EndOfFrameStart) * 1000.0);
			++Frames;
			FPlatformProcess::Sleep(0.004f);
		}
		const FTerrainRemeshStats Stats = C.Terrain->GetRemeshStats();
		C.Test.TestFalse(TEXT("remallado terminado"), C.Mesher->NeedsTick());
		C.Test.TestTrue(FString::Printf(TEXT("hilo de juego por fotograma < 8 ms (máx %.3f ms)"), Stats.GameThreadMsMax), Stats.GameThreadMsMax < 8.0f);
		C.Test.AddInfo(FString::Printf(TEXT("16 golpes: %d chunks remallados en %d fotogramas; hilo de juego máx %.3f ms/fotograma, volcado máx %.3f ms/chunk, fin de fotograma máx %.3f ms; tarea media %.2f ms (máx %.2f)"),
			Stats.ChunksApplied, Frames, Stats.GameThreadMsMax, Stats.ApplyMsMax, EndOfFrameMsMax, Stats.TaskMsAverage, Stats.TaskMsMax));

		TickWorld(C.World, 2);
		C.GroundDug = GroundCm(C.World, C.Surface);
		const double Sphere = GroundCm(C.World, C.Surface, 25.0f);
		C.Test.TestTrue(FString::Printf(TEXT("la colisión tiene el hueco (%.1f cm, antes %.1f)"), C.GroundDug, C.GroundBefore),
			FMath::IsFinite(C.GroundDug) && C.GroundDug < C.GroundBefore - 80.0);
		C.Test.TestTrue(FString::Printf(TEXT("cabe una esfera de 25 cm (%.1f cm)"), Sphere), FMath::IsFinite(Sphere) && Sphere < C.GroundBefore - 50.0);
	}

	/** 3) Guardar (texto de la partida), dejar el mundo sin cavar y cargar. */
	void SaveResetLoad(FContext& C)
	{
		const FTerrainEditModel Before = C.Terrain->GetModel();
		FSaveWorldDeltas Saved;
		C.Terrain->SaveTo(Saved);
		C.Test.TestFalse(TEXT("capa «terrain» escrita"), Saved.Terrain.IsNull());
		FSaveArchive Out;
		Saved.Save(Out);
		FSaveValue Parsed;
		FString Error;
		C.Test.TestTrue(TEXT("el texto de la partida se vuelve a leer"), FSaveText::Parse(FSaveText::Write(Out.GetRoot()), Parsed, Error));

		C.Terrain->ResetEdits();
		C.Terrain->EnsureReplacedAt(C.Surface);
		C.Test.TestTrue(TEXT("sin ediciones: remallado"), C.Terrain->FlushRemeshing(120.0));
		TickWorld(C.World, 2);
		const double Reset = GroundCm(C.World, C.Surface);
		C.Test.TestTrue(FString::Printf(TEXT("sin ediciones vuelve el suelo (%.1f cm)"), Reset), FMath::IsFinite(Reset) && FMath::Abs(Reset - C.GroundBefore) < 5.0);

		FSaveWorldDeltas Loaded;
		Loaded.Load(FSaveArchive(Parsed));
		C.Test.TestTrue(TEXT("carga la capa"), C.Terrain->LoadFrom(Loaded));
		C.Test.TestTrue(TEXT("mismas ediciones"), C.Terrain->GetModel() == Before);
		C.Test.TestTrue(TEXT("carga: remallado"), C.Terrain->FlushRemeshing(120.0));
		TickWorld(C.World, 2);
		const double Reloaded = GroundCm(C.World, C.Surface);
		C.Test.TestTrue(FString::Printf(TEXT("cargar reproduce el hueco (%.1f cm frente a %.1f)"), Reloaded, C.GroundDug),
			FMath::IsFinite(Reloaded) && FMath::Abs(Reloaded - C.GroundDug) < 2.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainRuntimeEditTest, "Explored.TerrainRuntime.EditRemeshCollisionSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerrainRuntimeEditTest::RunTest(const FString& Parameters)
{
	using namespace TerrainRuntimeTestDetail;
	FScopedTestWorld TestWorld;
	FContext C{*this};
	C.World = TestWorld.Get();
	C.Terrain = C.World ? C.World->GetSubsystem<UTerrainEditSubsystem>() : nullptr;
	C.Mesher = C.Terrain ? C.Terrain->GetMesher() : nullptr;
	if (!TestNotNull(TEXT("subsistema del terreno"), C.Terrain) || !TestNotNull(TEXT("remallador"), C.Mesher))
	{
		return false;
	}
	C.Mesher->SetAsyncCollisionCooking(false);
	if (!TestTrue(TEXT("hay suelo llano en Landing"), FindLandPoint(C.Terrain->GetTerrainDensity(), C.Surface)))
	{
		return false;
	}
	if (ReplaceAndMeasure(C))
	{
		DigAndRemesh(C);
		SaveResetLoad(C);
	}
	return true;
}

#endif
