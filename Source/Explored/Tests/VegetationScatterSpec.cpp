#include "Misc/AutomationTest.h"

#include "WorldGen/ArchipelagoLayout.h"
#include "WorldGen/TerrainDensity.h"
#include "WorldGen/VegetationScatter.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FVegetationScatterSpec, "Explored.WorldGen.Scatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	TUniquePtr<FTerrainDensity> Density;
	FBox2D Region;
END_DEFINE_SPEC(FVegetationScatterSpec)

void FVegetationScatterSpec::Define()
{
	BeforeEach([this]()
	{
		if (!Density)
		{
			Density = MakeUnique<FTerrainDensity>(FArchipelagoLayout::Generate(20260926));
		}
		const FIslandDesc* Emerald = Density->GetLayout().FindIsland(EIslandArchetype::Emerald);
		Region = FBox2D(Emerald->Center - FVector2D(150.0f), Emerald->Center + FVector2D(150.0f));
	});

	It("es determinista", [this]()
	{
		const TArray<FScatterRule> Rules = FVegetationScatter::DefaultRules();
		const FScatterResult A = FVegetationScatter::Generate(*Density, Rules, Region, 7);
		const FScatterResult B = FVegetationScatter::Generate(*Density, Rules, Region, 7);
		TestEqual(TEXT("Mismo total"), A.Total(), B.Total());
		if (!TestEqual(TEXT("Mismo número de reglas"), A.PerRule.Num(), B.PerRule.Num()))
		{
			return;
		}
		for (int32 R = 0; R < A.PerRule.Num(); ++R)
		{
			if (!TestEqual(TEXT("Mismo número por regla"), A.PerRule[R].Num(), B.PerRule[R].Num()))
			{
				return;
			}
			for (int32 I = 0; I < A.PerRule[R].Num(); ++I)
			{
				if (!A.PerRule[R][I].Transform.Equals(B.PerRule[R][I].Transform))
				{
					AddError(FString::Printf(TEXT("Regla %d, instancia %d distinta"), R, I));
					return;
				}
			}
		}
	});

	It("puebla la selva de Esmeralda con árboles y sotobosque", [this]()
	{
		const TArray<FScatterRule> Rules = FVegetationScatter::DefaultRules();
		const FScatterResult Result = FVegetationScatter::Generate(*Density, Rules, Region, 7);
		for (int32 R = 0; R < Rules.Num(); ++R)
		{
			if (Rules[R].Species == TEXT("JungleWide"))
			{
				TestTrue(TEXT("Árboles"), Result.PerRule[R].Num() > 30);
			}
			if (Rules[R].Species == TEXT("Shrub"))
			{
				TestTrue(TEXT("Sotobosque"), Result.PerRule[R].Num() > 300);
			}
		}
	});

	It("no coloca vegetación terrestre bajo el agua ni en pendientes excesivas", [this]()
	{
		const TArray<FScatterRule> Rules = FVegetationScatter::DefaultRules();
		const FScatterResult Result = FVegetationScatter::Generate(*Density, Rules, Region, 7);
		for (int32 R = 0; R < Rules.Num(); ++R)
		{
			for (const FScatterInstance& I : Result.PerRule[R])
			{
				const float SurfaceZ = I.Transform.GetLocation().Z / 100.0f + Rules[R].Sink * I.Transform.GetScale3D().Z;
				if (SurfaceZ < Rules[R].MinHeight - 0.01f)
				{
					AddError(FString::Printf(TEXT("%s por debajo de su altura mínima (%.2f m)"), *Rules[R].Species.ToString(), SurfaceZ));
					return;
				}
			}
		}
	});

	It("encuentra la superficie exacta del terreno", [this]()
	{
		const FIslandDesc* Landing = Density->GetLayout().FindIsland(EIslandArchetype::Landing);
		float Z = 0.0f;
		FVector Normal;
		const bool bFound = FVegetationScatter::FindSurface(*Density, Landing->Center.X, Landing->Center.Y, Z, Normal);
		TestTrue(TEXT("Encontrada"), bFound);
		TestTrue(TEXT("En la superficie"), FMath::Abs(Density->Density(FVector(Landing->Center, Z))) < 0.05f);
		TestTrue(TEXT("Normal hacia arriba"), Normal.Z > 0.3f);
	});
}

#endif
