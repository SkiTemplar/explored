#include "Misc/AutomationTest.h"

#include "WorldGen/TerrainToolModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace TerrainToolSpecDetail
{
	FTerrainStrataQuery Column(EIslandArchetype Archetype, float Depth, float Sand = 0.0f, float Rock = 0.0f)
	{
		FTerrainStrataQuery Q;
		Q.bHasIsland = true;
		Q.Archetype = Archetype;
		Q.ColumnHeight = 10.0f;
		Q.Z = 10.0f - Depth;
		Q.SandWeight = Sand;
		Q.RockWeight = Rock;
		return Q;
	}

	FTerrainToolRequest ValidPick()
	{
		FTerrainToolRequest R;
		R.Action = ETerrainToolAction::Pick;
		R.Tool = ETerrainDigTool::PicoPiedra;
		R.EyeLocation = FVector(0.0, 0.0, 1.7);
		R.ImpactPoint = FVector(2.0, 0.0, 0.0);
		R.SecondsSinceLastUse = 5.0f;
		R.DensityAtImpact = 0.05f;
		return R;
	}

	int32 AsInt(ETerrainMaterial M) { return static_cast<int32>(M); }
	int32 AsInt(ETerrainToolVerdict V) { return static_cast<int32>(V); }
	int32 AsInt(EMineHitCue C) { return static_cast<int32>(C); }
}

BEGIN_DEFINE_SPEC(FTerrainToolModelSpec, "Explored.TerrainTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FTerrainToolModelSpec)

void FTerrainToolModelSpec::Define()
{
	using namespace TerrainToolSpecDetail;

	Describe("objetos y botones", [this]()
	{
		It("pico y pala de items.json y los ids de mining.json son herramientas; el resto no", [this]()
		{
			ETerrainDigTool Tool = ETerrainDigTool::Count;
			TestTrue(TEXT("pico"), FTerrainToolModel::ToolFromItem(TEXT("pico"), Tool) && Tool == ETerrainDigTool::PicoPiedra);
			TestTrue(TEXT("pala"), FTerrainToolModel::ToolFromItem(TEXT("pala"), Tool) && Tool == ETerrainDigTool::PalaTosca);
			TestTrue(TEXT("pico_obsidiana"), FTerrainToolModel::ToolFromItem(TEXT("pico_obsidiana"), Tool) && Tool == ETerrainDigTool::PicoObsidiana);
			TestFalse(TEXT("hacha"), FTerrainToolModel::ToolFromItem(TEXT("hacha"), Tool));
			TestFalse(TEXT("nada"), FTerrainToolModel::ToolFromItem(NAME_None, Tool));
		});

		It("el pico pica con los dos botones; la pala aplana y echa tierra", [this]()
		{
			TestTrue(TEXT("pico principal"), FTerrainToolModel::ActionFor(ETerrainDigTool::PicoTallado, false) == ETerrainToolAction::Pick);
			TestTrue(TEXT("pico secundario"), FTerrainToolModel::ActionFor(ETerrainDigTool::PicoTallado, true) == ETerrainToolAction::Pick);
			TestTrue(TEXT("pala principal"), FTerrainToolModel::ActionFor(ETerrainDigTool::PalaTosca, false) == ETerrainToolAction::ShovelFlatten);
			TestTrue(TEXT("pala secundario"), FTerrainToolModel::ActionFor(ETerrainDigTool::PalaTosca, true) == ETerrainToolAction::PlaceSoil);
		});
	});

	Describe("estratos (mining.json/strata)", [this]()
	{
		It("Landing: tierra arriba, arena en la playa hasta 3 m y basalto desde 4 m", [this]()
		{
			TestEqual(TEXT("tierra a 1 m"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Landing, 1.0f))), AsInt(ETerrainMaterial::Tierra));
			TestEqual(TEXT("arena a 2 m en la playa"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Landing, 2.0f, 0.9f))), AsInt(ETerrainMaterial::Arena));
			TestEqual(TEXT("tierra a 3,5 m bajo la playa"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Landing, 3.5f, 0.9f))), AsInt(ETerrainMaterial::Tierra));
			TestEqual(TEXT("basalto a 5 m"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Landing, 5.0f))), AsInt(ETerrainMaterial::Basalto));
		});

		It("La Meseta: caliza bajo 1 m y basalto a partir de 40 m", [this]()
		{
			TestEqual(TEXT("tierra a 0,5 m"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Mesa, 0.5f))), AsInt(ETerrainMaterial::Tierra));
			TestEqual(TEXT("caliza a 2 m"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Mesa, 2.0f))), AsInt(ETerrainMaterial::Caliza));
			TestEqual(TEXT("basalto a 45 m"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Mesa, 45.0f))), AsInt(ETerrainMaterial::Basalto));
		});

		It("una pared de roca desnuda es roca; por encima de la superficie cuenta como superficie", [this]()
		{
			TestEqual(TEXT("farallón de caliza"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Mesa, 0.2f, 0.0f, 0.9f))), AsInt(ETerrainMaterial::Caliza));
			TestEqual(TEXT("voladizo (Z sobre la columna)"), AsInt(FTerrainToolModel::ClassifyMaterial(Column(EIslandArchetype::Landing, -2.0f))), AsInt(ETerrainMaterial::Tierra));
			FTerrainStrataQuery Broken = Column(EIslandArchetype::Landing, 1.0f);
			Broken.Z = std::numeric_limits<float>::quiet_NaN();
			TestEqual(TEXT("NaN: tierra"), AsInt(FTerrainToolModel::ClassifyMaterial(Broken)), AsInt(ETerrainMaterial::Tierra));
		});
	});

	Describe("validación del servidor", [this]()
	{
		It("acepta un golpe normal", [this]()
		{
			TestEqual(TEXT("aceptado"), AsInt(FTerrainToolModel::Validate(ValidPick())), AsInt(ETerrainToolVerdict::Accepted));
		});

		It("descarta lo lejano, lo rápido, lo que no es superficie y lo no finito", [this]()
		{
			FTerrainToolRequest Far = ValidPick();
			Far.ImpactPoint = FVector(4.6, 0.0, 1.7);
			TestEqual(TEXT("a 4,6 m"), AsInt(FTerrainToolModel::Validate(Far)), AsInt(ETerrainToolVerdict::TooFar));

			FTerrainToolRequest Fast = ValidPick();
			const float Min = FTerrainEdits::ToolInfo(ETerrainDigTool::PicoPiedra).SecondsPerHit * (1.0f - FTerrainEdits::CadenceTolerance);
			Fast.SecondsSinceLastUse = Min - 0.01f;
			TestEqual(TEXT("antes del 85 %"), AsInt(FTerrainToolModel::Validate(Fast)), AsInt(ETerrainToolVerdict::TooSoon));
			Fast.SecondsSinceLastUse = Min + 0.01f;
			TestEqual(TEXT("justo después"), AsInt(FTerrainToolModel::Validate(Fast)), AsInt(ETerrainToolVerdict::Accepted));

			FTerrainToolRequest Air = ValidPick();
			Air.DensityAtImpact = 3.0f;
			TestEqual(TEXT("en el aire"), AsInt(FTerrainToolModel::Validate(Air)), AsInt(ETerrainToolVerdict::NotSurface));

			FTerrainToolRequest Broken = ValidPick();
			Broken.ImpactPoint.X = std::numeric_limits<double>::quiet_NaN();
			TestEqual(TEXT("NaN"), AsInt(FTerrainToolModel::Validate(Broken)), AsInt(ETerrainToolVerdict::Invalid));
			Broken = ValidPick();
			Broken.SecondsSinceLastUse = std::numeric_limits<float>::infinity();
			TestEqual(TEXT("infinito"), AsInt(FTerrainToolModel::Validate(Broken)), AsInt(ETerrainToolVerdict::Invalid));
		});

		It("la pala va al ritmo de su pasada (1,2 s)", [this]()
		{
			FTerrainToolRequest Shovel = ValidPick();
			Shovel.Action = ETerrainToolAction::ShovelFlatten;
			Shovel.Tool = ETerrainDigTool::PalaTosca;
			Shovel.SecondsSinceLastUse = 0.9f;
			TestEqual(TEXT("0,9 s es pronto"), AsInt(FTerrainToolModel::Validate(Shovel)), AsInt(ETerrainToolVerdict::TooSoon));
		});
	});

	Describe("tierra transportada y señales", [this]()
	{
		It("solo la tierra suelta se lleva; lo echado se resta y nunca pasa de los topes", [this]()
		{
			TestEqual(TEXT("tierra"), FTerrainToolModel::UpdateCarriedSoil(0.0, ETerrainMaterial::Tierra, 0.2, 0.0), 0.2, 1.0e-9);
			TestEqual(TEXT("basalto no"), FTerrainToolModel::UpdateCarriedSoil(0.5, ETerrainMaterial::Basalto, 0.2, 0.0), 0.5, 1.0e-9);
			TestEqual(TEXT("echar"), FTerrainToolModel::UpdateCarriedSoil(0.5, ETerrainMaterial::Tierra, 0.0, 0.3), 0.2, 1.0e-9);
			TestEqual(TEXT("tope"), FTerrainToolModel::UpdateCarriedSoil(1.9, ETerrainMaterial::Arena, 0.5, 0.0), FTerrainToolModel::MaxCarriedSoil, 1.0e-9);
			TestEqual(TEXT("nunca negativa"), FTerrainToolModel::UpdateCarriedSoil(0.1, ETerrainMaterial::Tierra, 0.0, 0.5), 0.0, 1.0e-9);
			TestEqual(TEXT("NaN no cambia nada"), FTerrainToolModel::UpdateCarriedSoil(0.4, ETerrainMaterial::Tierra, std::numeric_limits<double>::quiet_NaN(), 0.0), 0.4, 1.0e-9);
		});

		It("predice el rebote con la herramienta corta y con la pala en roca", [this]()
		{
			TestEqual(TEXT("pico de piedra en basalto"), AsInt(FTerrainToolModel::PredictCue(ETerrainToolAction::Pick, ETerrainDigTool::PicoPiedra, ETerrainMaterial::Basalto, 0.0)), AsInt(EMineHitCue::Rebound));
			TestEqual(TEXT("pico de piedra en tierra"), AsInt(FTerrainToolModel::PredictCue(ETerrainToolAction::Pick, ETerrainDigTool::PicoPiedra, ETerrainMaterial::Tierra, 0.0)), AsInt(EMineHitCue::Hit));
			TestEqual(TEXT("pala en caliza"), AsInt(FTerrainToolModel::PredictCue(ETerrainToolAction::ShovelFlatten, ETerrainDigTool::PalaTosca, ETerrainMaterial::Caliza, 0.0)), AsInt(EMineHitCue::Rebound));
			TestEqual(TEXT("echar sin tierra"), AsInt(FTerrainToolModel::PredictCue(ETerrainToolAction::PlaceSoil, ETerrainDigTool::PalaTosca, ETerrainMaterial::Tierra, 0.0)), AsInt(EMineHitCue::Miss));
		});

		It("la pasada de pala va al plano de los pies y con la tierra que se lleva", [this]()
		{
			const FShovelStroke Stroke = FTerrainToolModel::MakeShovelStroke(FVector(3.0, 4.0, 1.2), 0.5, ETerrainMaterial::Tierra, 1, 5.0);
			TestEqual(TEXT("centro en los pies"), Stroke.Center, FVector(3.0, 4.0, 0.5), 1.0e-6f);
			TestEqual(TEXT("presupuesto al tope"), Stroke.SoilBudget, FTerrainToolModel::MaxCarriedSoil, 1.0e-9);
			TestEqual(TEXT("radio del GDD"), Stroke.Radius, 1.0f, 1.0e-6f);
		});
	});
}

#endif
