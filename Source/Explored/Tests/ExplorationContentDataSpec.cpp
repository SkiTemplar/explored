#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Exploration/ExplorationContentLoader.h"
#include "Exploration/ExplorationContentModel.h"

#if WITH_DEV_AUTOMATION_TESTS

// Solo en el editor (lee Content/Data con el parser de Unreal); las reglas puras están en ExplorationContentSpec.
BEGIN_DEFINE_SPEC(FExplorationContentDataSpec, "Explored.Exploration.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExplorationContentDataSpec)

void FExplorationContentDataSpec::Define()
{
	It("carga exploration.json con al menos tres lugares en la isla del Amaraje", [this]()
	{
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data/exploration.json");
		if (!TestTrue(TEXT("Existe exploration.json"), FFileHelper::LoadFileToString(Text, *Path)))
		{
			return;
		}
		FExplorationCatalog Catalog;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("Se parsea: %s"), *Error), ExplorationContentLoader::ParseJson(Text, Catalog, Error)))
		{
			return;
		}
		// Priorización de docs/diseno/exploracion.md: la porción vertical empieza en Landing.
		TestTrue(TEXT("Al menos 3 lugares en Landing"), Catalog.CountForIsland(TEXT("landing")) >= 3);
		for (const FExplorationLandmark& L : Catalog.Landmarks)
		{
			TestTrue(FString::Printf(TEXT("%s: isla válida"), *L.Id.ToString()), FExplorationCatalog::IsValidIslandId(L.IslandId));
			TestTrue(FString::Printf(TEXT("%s: tipo válido"), *L.Id.ToString()), FExplorationCatalog::IsValidKind(L.Kind));
		}
	});
}

#endif
