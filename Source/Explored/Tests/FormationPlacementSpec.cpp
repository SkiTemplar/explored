#include "Misc/AutomationTest.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/FormationPlacementModel.h"
#include "WorldGen/TerrainDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FFormationPlacementSpec, "Explored.WorldGen.Formations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TUniquePtr<FTerrainDensity> Density;
	TArray<FVector> AvoidPoints;
	static constexpr uint32 OfficialSeed = 20260926;
END_DEFINE_SPEC(FFormationPlacementSpec)

void FFormationPlacementSpec::Define()
{
	BeforeEach([this]()
	{
		if (!Density)
		{
			Density = MakeUnique<FTerrainDensity>(FArchipelagoLayout::Generate(OfficialSeed));
		}
		AvoidPoints.Reset();
		// Aproxima el spawn de Landing (playa frente a la laguna) y algún POI típico, sin
		// depender de FindSpawnPoint (vive en WorldGenCommandlet.cpp, fuera del host).
		const FIslandDesc* Landing = Density->GetLayout().FindIsland(EIslandArchetype::Landing);
		if (Landing)
		{
			const FVector2D Dir(FMath::Cos(Landing->Rotation), FMath::Sin(Landing->Rotation));
			AvoidPoints.Add(FVector(Landing->Center - Dir * Landing->Radius, 0.0));
			AvoidPoints.Add(FVector(Landing->Center, 0.0));
		}
	});

	It("es determinista", [this]()
	{
		const TArray<FFormationInstance> A = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		const TArray<FFormationInstance> B = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		if (!TestEqual(TEXT("Mismo número de instancias"), A.Num(), B.Num()))
		{
			return;
		}
		for (int32 I = 0; I < A.Num(); ++I)
		{
			if (A[I].Kind != B[I].Kind || !A[I].Transform.Equals(B[I].Transform) || A[I].MeshFilter != B[I].MeshFilter)
			{
				AddError(FString::Printf(TEXT("Instancia %d distinta entre pasadas"), I));
				return;
			}
		}
	});

	It("no coloca ninguna formación sobre los puntos protegidos", [this]()
	{
		const TArray<FFormationInstance> Result = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		const FFormationPlacementParams Params;
		for (const FFormationInstance& Instance : Result)
		{
			const FVector2D P(Instance.Transform.GetLocation() / 100.0);
			for (const FVector& Avoid : AvoidPoints)
			{
				const float Dist = FVector2D::Distance(P, FVector2D(Avoid));
				if (Dist < Params.AvoidRadius - 0.01f)
				{
					AddError(FString::Printf(TEXT("%s a %.1f m de un punto protegido (radio %.1f)"),
						LexToString(Instance.Kind), Dist, Params.AvoidRadius));
					return;
				}
			}
		}
	});

	It("respeta la pendiente mínima en paredes y espolones", [this]()
	{
		const TArray<FFormationInstance> Result = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		const FFormationPlacementParams Params;
		// El modelo revalida la pendiente en el punto ya hundido (y recorta el hundimiento si
		// hace falta, ver MakeWallInstance), así que el margen aquí es solo por redondeo.
		const float ToleranceDeg = 2.0f;
		for (const FFormationInstance& Instance : Result)
		{
			if (Instance.Kind != EFormationKind::CliffWall && Instance.Kind != EFormationKind::CliffSpur)
			{
				continue;
			}
			const FVector Location = Instance.Transform.GetLocation() / 100.0;
			const FVector Normal = Density->Normal(Location, 1.0f);
			const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.Z, -1.0f, 1.0f)));
			if (SlopeDeg < Params.MinCliffSlopeDeg - ToleranceDeg)
			{
				AddError(FString::Printf(TEXT("%s con pendiente %.1f° (mínimo %.1f°)"),
					LexToString(Instance.Kind), SlopeDeg, Params.MinCliffSlopeDeg));
				return;
			}
		}
	});

	It("no coloca ninguna pieza en agua profunda, salvo los farallones en su franja", [this]()
	{
		const TArray<FFormationInstance> Result = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		const FFormationPlacementParams Params;
		for (const FFormationInstance& Instance : Result)
		{
			const FVector Location = Instance.Transform.GetLocation() / 100.0;
			const float Height = Density->SampleColumn(Location.X, Location.Y).Height;
			if (Instance.Kind == EFormationKind::SeaStack)
			{
				if (Height < Params.StackMinDepth - 0.5f || Height > Params.StackMaxDepth + 0.5f)
				{
					AddError(FString::Printf(TEXT("Farallón fuera de la franja de agua somera (altura %.1f m)"), Height));
					return;
				}
				continue;
			}
			if (Instance.Kind == EFormationKind::SeaArch)
			{
				// Un arco es, por definición, roca sobre un hueco de agua: el terreno bajo su vano
				// (ignorando el propio túnel tallado) es agua somera de costa, no mar profundo.
				if (Height < -15.0f)
				{
					AddError(FString::Printf(TEXT("Arco marino sobre mar demasiado profundo (altura %.1f m)"), Height));
					return;
				}
				continue;
			}
			if (Height < -2.0f)
			{
				AddError(FString::Printf(TEXT("%s en agua profunda (altura %.1f m)"), LexToString(Instance.Kind), Height));
				return;
			}
		}
	});

	It("respeta el tope de paredes y espolones por isla", [this]()
	{
		const TArray<FFormationInstance> Result = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		const FFormationPlacementParams Params;
		TMap<int32, int32> CountPerIsland;
		for (const FFormationInstance& Instance : Result)
		{
			if (Instance.Kind != EFormationKind::CliffWall && Instance.Kind != EFormationKind::CliffSpur)
			{
				continue;
			}
			const FVector Location = Instance.Transform.GetLocation() / 100.0;
			const FTerrainColumn Column = Density->SampleColumn(Location.X, Location.Y);
			CountPerIsland.FindOrAdd(Column.IslandIndex)++;
		}
		for (const auto& Pair : CountPerIsland)
		{
			TestTrue(TEXT("Dentro del tope por isla"), Pair.Value <= Params.MaxWallsPerIsland);
		}
	});

	It("coloca como mucho un arco marino, en Los Dientes", [this]()
	{
		const TArray<FFormationInstance> Result = FFormationPlacementModel::Generate(*Density, 11, AvoidPoints);
		int32 ArchCount = 0;
		const FIslandDesc* Teeth = Density->GetLayout().FindIsland(EIslandArchetype::Teeth);
		for (const FFormationInstance& Instance : Result)
		{
			if (Instance.Kind == EFormationKind::SeaArch)
			{
				++ArchCount;
				const FVector Location = Instance.Transform.GetLocation() / 100.0;
				if (Teeth)
				{
					TestTrue(TEXT("Cerca de Los Dientes"),
						FVector2D::Distance(FVector2D(Location), Teeth->Center) < Teeth->Radius * 1.5f);
				}
			}
		}
		TestTrue(TEXT("Como mucho un arco"), ArchCount <= 1);
	});
}

#endif
