#include "Misc/AutomationTest.h"

#include "WorldGen/SeabedScatterModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Fondo sintético para el spec, independiente de FTerrainDensity (a propósito: el modelo
	 * no depende de la rama worldgen/realismo-terreno, así que el test tampoco).
	 *
	 * Rejilla de zonas en metros:
	 *  - Reef somero (coral denso): X en [0, 15), Y en [0, 15), profundidad 2 m, pendiente 10°.
	 *  - Reef borde (abanicos/esponjas): X en [15, 35), Y en [0, 15), profundidad 5 m, pendiente 15°.
	 *  - Rompiente: X en [0, 15), Y en [55, 60): profundidad 0,3 m (< SurfBreakDepthM), dentro
	 *    de la zona Reef; nada debería sembrarse ahí.
	 *  - Sand (praderas y conchas): X en [0, 35), Y en [15, 35), profundidad 5 m, pendiente 5°.
	 *  - Rock (algas y erizos): X en [0, 35), Y en [35, 55), profundidad 5 m, pendiente 5°.
	 */
	FSeabedColumn TestColumn(float X, float Y)
	{
		FSeabedColumn Out;
		if (Y < 15.0f)
		{
			Out.Zone = ESeabedZone::Reef;
			if (X < 15.0f)
			{
				Out.DepthM = 2.0f;
				Out.SlopeDeg = 10.0f;
			}
			else
			{
				Out.DepthM = 5.0f;
				Out.SlopeDeg = 15.0f;
			}
		}
		else if (Y < 35.0f)
		{
			Out.Zone = ESeabedZone::Sand;
			Out.DepthM = 5.0f;
			Out.SlopeDeg = 5.0f;
		}
		else if (Y < 55.0f)
		{
			Out.Zone = ESeabedZone::Rock;
			Out.DepthM = 5.0f;
			Out.SlopeDeg = 5.0f;
		}
		else
		{
			// Franja de rompiente al fondo de la celda, dentro de la zona Reef.
			Out.Zone = ESeabedZone::Reef;
			Out.DepthM = 0.3f;
			Out.SlopeDeg = 5.0f;
		}
		return Out;
	}

	constexpr float RegionAArea = 15.0f * 15.0f;  // Reef somero.
	constexpr float RegionBArea = 20.0f * 15.0f;  // Reef borde.
}

BEGIN_DEFINE_SPEC(FSeabedScatterModelSpec, "Explored.WorldGen.SeabedScatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FBox2D CellBounds;
	TArray<FVector2D> AvoidPoints;
	static constexpr uint32 OfficialSeed = 20260928;
END_DEFINE_SPEC(FSeabedScatterModelSpec)

void FSeabedScatterModelSpec::Define()
{
	BeforeEach([this]()
	{
		CellBounds = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(35.0f, 60.0f));
		AvoidPoints.Reset();
		AvoidPoints.Add(FVector2D(5.0f, 20.0f)); // canal de balsa dentro de la zona de arena.
	});

	It("es determinista", [this]()
	{
		const TArray<FSeabedScatterRule> Rules = FSeabedScatterModel::DefaultRules();
		const TArray<FSeabedScatterInstance> A = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, AvoidPoints);
		const TArray<FSeabedScatterInstance> B = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, AvoidPoints);
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

	It("nunca siembra por encima del nivel del mar ni en la rompiente", [this]()
	{
		const TArray<FSeabedScatterRule> Rules = FSeabedScatterModel::DefaultRules();
		const TArray<FSeabedScatterInstance> Result = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, AvoidPoints);
		TestTrue(TEXT("Hay instancias"), Result.Num() > 0);
		for (const FSeabedScatterInstance& Instance : Result)
		{
			const FVector LocationM = Instance.Transform.GetLocation() / 100.0;
			if (LocationM.Z > 0.0)
			{
				AddError(FString::Printf(TEXT("%s por encima del nivel del mar (Z=%.2f m)"),
					*Instance.Species.ToString(), LocationM.Z));
				return;
			}
			const FSeabedColumn Column = TestColumn(LocationM.X, LocationM.Y);
			if (Column.DepthM < FSeabedScatterModel::SurfBreakDepthM - KINDA_SMALL_NUMBER)
			{
				AddError(FString::Printf(TEXT("%s en la rompiente (profundidad %.2f m)"),
					*Instance.Species.ToString(), Column.DepthM));
				return;
			}
		}
	});

	It("respeta el radio de los canales de navegación de las balsas", [this]()
	{
		const TArray<FSeabedScatterRule> Rules = FSeabedScatterModel::DefaultRules();
		constexpr float AvoidRadiusM = 8.0f;
		const TArray<FSeabedScatterInstance> Result = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, AvoidPoints, AvoidRadiusM);
		for (const FSeabedScatterInstance& Instance : Result)
		{
			const FVector2D LocationM(Instance.Transform.GetLocation() / 100.0);
			for (const FVector2D& Avoid : AvoidPoints)
			{
				const float Dist = FVector2D::Distance(LocationM, Avoid);
				if (Dist < AvoidRadiusM - 0.01f)
				{
					AddError(FString::Printf(TEXT("%s a %.1f m de un canal protegido"), *Instance.Species.ToString(), Dist));
					return;
				}
			}
		}
	});

	It("respeta el presupuesto de instancias por celda", [this]()
	{
		const TArray<FSeabedScatterRule> Rules = FSeabedScatterModel::DefaultRules();
		constexpr int32 Budget = 15;
		const TArray<FSeabedScatterInstance> Result = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, AvoidPoints, 8.0f, Budget);
		TestTrue(TEXT("No pasa del presupuesto"), Result.Num() <= Budget);

		// Con un presupuesto amplio, la celda entera no debería generar una cantidad
		// desorbitada (cota generosa: ninguna categoría pasa de la rejilla más densa posible).
		const TArray<FSeabedScatterInstance> Unbudgeted = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, AvoidPoints, 8.0f, 100000);
		const float CellArea = (CellBounds.Max.X - CellBounds.Min.X) * (CellBounds.Max.Y - CellBounds.Min.Y);
		const float DensestSpacing = 1.2f; // PraderaMarina, la regla con el paso más corto.
		const int32 MaxPlausible = FMath::CeilToInt32(CellArea / (DensestSpacing * DensestSpacing)) * Rules.Num();
		TestTrue(TEXT("Densidad total plausible"), Unbudgeted.Num() < MaxPlausible);
	});

	It("la densidad por zona está en el orden esperado (arrecife denso > borde del arrecife)", [this]()
	{
		const TArray<FSeabedScatterRule> Rules = FSeabedScatterModel::DefaultRules();
		const TArray<FSeabedScatterInstance> Result = FSeabedScatterModel::Generate(
			CellBounds, &TestColumn, Rules, 7, EIslandArchetype::WhiteSands, TArray<FVector2D>());

		int32 CountReefCoral = 0, CountReefEdge = 0, CountSandMeadow = 0, CountSandShell = 0;
		int32 CountRockKelp = 0, CountRockUrchin = 0, CountRockFormation = 0;
		for (const FSeabedScatterInstance& Instance : Result)
		{
			switch (Instance.Category)
			{
			case ESeabedCategory::ReefCoral: ++CountReefCoral; break;
			case ESeabedCategory::ReefEdge: ++CountReefEdge; break;
			case ESeabedCategory::SandMeadow: ++CountSandMeadow; break;
			case ESeabedCategory::SandShell: ++CountSandShell; break;
			case ESeabedCategory::RockKelp: ++CountRockKelp; break;
			case ESeabedCategory::RockUrchin: ++CountRockUrchin; break;
			case ESeabedCategory::RockFormation: ++CountRockFormation; break;
			}
		}

		TestTrue(TEXT("Hay coral denso en el arrecife somero"), CountReefCoral > 0);
		TestTrue(TEXT("Hay praderas o conchas en arena"), CountSandMeadow + CountSandShell > 0);
		TestTrue(TEXT("Hay algas, erizos o roca en la zona de roca"), CountRockKelp + CountRockUrchin + CountRockFormation > 0);

		const float DensityReefCoral = CountReefCoral / RegionAArea;
		const float DensityReefEdge = CountReefEdge / RegionBArea;
		if (!(DensityReefCoral > DensityReefEdge))
		{
			AddError(FString::Printf(TEXT("Coral denso (%.4f/m²) no supera al borde del arrecife (%.4f/m²)"),
				DensityReefCoral, DensityReefEdge));
		}
	});
}

#endif
