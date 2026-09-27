#include "Misc/AutomationTest.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/BeachDebrisModel.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FBeachDebrisSpec, "Explored.WorldGen.BeachDebris",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TUniquePtr<FTerrainDensity> Density;
	TArray<FVector> AvoidPoints;
	static constexpr uint32 OfficialSeed = 20260926;
END_DEFINE_SPEC(FBeachDebrisSpec)

void FBeachDebrisSpec::Define()
{
	BeforeEach([this]()
	{
		if (!Density)
		{
			Density = MakeUnique<FTerrainDensity>(FArchipelagoLayout::Generate(OfficialSeed));
		}
		AvoidPoints.Reset();
		const FIslandDesc* Landing = Density->GetLayout().FindIsland(EIslandArchetype::Landing);
		if (Landing)
		{
			const FVector2D Dir(FMath::Cos(Landing->Rotation), FMath::Sin(Landing->Rotation));
			AvoidPoints.Add(FVector(Landing->Center - Dir * Landing->Radius, 0.0));
		}
	});

	It("es determinista", [this]()
	{
		const TArray<FBeachDebrisRule> Rules = FBeachDebrisModel::DefaultRules();
		const TArray<FBeachDebrisInstance> A = FBeachDebrisModel::Generate(*Density, Rules, 3, AvoidPoints);
		const TArray<FBeachDebrisInstance> B = FBeachDebrisModel::Generate(*Density, Rules, 3, AvoidPoints);
		if (!TestEqual(TEXT("Mismo número de instancias"), A.Num(), B.Num()))
		{
			return;
		}
		for (int32 I = 0; I < A.Num(); ++I)
		{
			if (!A[I].Transform.Equals(B[I].Transform) || A[I].Species != B[I].Species)
			{
				AddError(FString::Printf(TEXT("Instancia %d distinta entre pasadas"), I));
				return;
			}
		}
	});

	It("coloca microdetalle en la franja de marea, sin invadir el spawn", [this]()
	{
		const TArray<FBeachDebrisRule> Rules = FBeachDebrisModel::DefaultRules();
		const TArray<FBeachDebrisInstance> Result = FBeachDebrisModel::Generate(*Density, Rules, 3, AvoidPoints, 15.0f);
		TestTrue(TEXT("Hay instancias"), Result.Num() > 0);
		for (const FBeachDebrisInstance& Instance : Result)
		{
			const FVector Location = Instance.Transform.GetLocation() / 100.0;
			const float Height = Density->SampleColumn(Location.X, Location.Y).Height;
			if (Height < -1.0f || Height > 2.5f)
			{
				AddError(FString::Printf(TEXT("%s fuera de la franja de marea (altura %.2f m)"),
					*Instance.Species.ToString(), Height));
				return;
			}
			for (const FVector& Avoid : AvoidPoints)
			{
				const float Dist = FVector2D::Distance(FVector2D(Location), FVector2D(Avoid));
				if (Dist < 15.0f - 0.01f)
				{
					AddError(FString::Printf(TEXT("%s a %.1f m del spawn protegido"), *Instance.Species.ToString(), Dist));
					return;
				}
			}
		}
	});

	It("es escaso y agrupado, no uniforme", [this]()
	{
		// Con el mismo umbral de agrupación para toda la franja de marea, una distribución
		// uniforme cubriría una fracción alta de las celdas de la rejilla; agrupado deja huecos.
		const TArray<FBeachDebrisRule> Rules = FBeachDebrisModel::DefaultRules();
		const TArray<FBeachDebrisInstance> Result = FBeachDebrisModel::Generate(*Density, Rules, 3, AvoidPoints, 15.0f);
		const FIslandDesc* Landing = Density->GetLayout().FindIsland(EIslandArchetype::Landing);
		if (!Landing)
		{
			return;
		}
		// Cota generosa: nada que se acerque a "una instancia por celda de 13 m" en toda la costa.
		const float ApproxShoreline = 2.0f * UE_PI * Landing->Radius;
		const int32 MaxPlausible = FMath::CeilToInt32(ApproxShoreline / 13.0f) * Rules.Num();
		int32 OnLanding = 0;
		for (const FBeachDebrisInstance& Instance : Result)
		{
			const FVector Location = Instance.Transform.GetLocation() / 100.0;
			if (Density->SampleColumn(Location.X, Location.Y).IslandIndex ==
				Density->GetLayout().Islands.IndexOfByPredicate([](const FIslandDesc& I) { return I.Archetype == EIslandArchetype::Landing; }))
			{
				++OnLanding;
			}
		}
		TestTrue(TEXT("Densidad baja"), OnLanding < MaxPlausible);
	});
}

#endif
