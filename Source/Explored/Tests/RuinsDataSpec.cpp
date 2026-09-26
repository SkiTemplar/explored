#include "Misc/AutomationTest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Ruins/MuseumModel.h"
#include "Ruins/RuinsModel.h"
#include "Ruins/RuinsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// Solo en el editor (lee Content/Data con el parser de Unreal); las reglas están en RuinsSpec y MuseumSpec.
BEGIN_DEFINE_SPEC(FRuinsDataSpec, "Explored.Ruins.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FRuinsDataSpec)

void FRuinsDataSpec::Define()
{
	It("carga artifacts.json con tesoros exponibles para «Coleccionista»", [this]()
	{
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data/artifacts.json");
		if (!TestTrue(TEXT("Existe artifacts.json"), FFileHelper::LoadFileToString(Text, *Path)))
		{
			return;
		}
		FTreasureCatalog Catalog;
		TMap<FName, FString> NamesEs;
		TMap<FName, FString> NamesEn;
		FString Error;
		TestTrue(FString::Printf(TEXT("Se parsea: %s"), *Error), URuinsSubsystem::ParseArtifactsJson(Text, Catalog, NamesEs, NamesEn, Error));
		TestTrue(TEXT("Suficientes tesoros"), Catalog.Artifacts.Num() >= FMuseumModel::CollectorThreshold);
		for (const FArtifactDef& Artifact : Catalog.Artifacts)
		{
			TestTrue(FString::Printf(TEXT("%s cabe en algún mueble"), *Artifact.Id.ToString()), Catalog.CanEverDisplay(Artifact));
			TestFalse(FString::Printf(TEXT("%s tiene nombre"), *Artifact.Id.ToString()), Artifact.NameEs.IsEmpty() || Artifact.NameEn.IsEmpty());
		}
		TestNotNull(TEXT("Estantería"), Catalog.FindDisplay(TEXT("estanteria_museo")));
	});

	It("carga ruins.json con las mismas constantes que el código", [this]()
	{
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data/ruins.json");
		if (!TestTrue(TEXT("Existe ruins.json"), FFileHelper::LoadFileToString(Text, *Path)))
		{
			return;
		}
		TMap<FName, FString> NamesEs;
		TMap<FName, FString> NamesEn;
		FString Error;
		TestTrue(FString::Printf(TEXT("Se parsea: %s"), *Error), URuinsSubsystem::ParseRuinsJson(Text, NamesEs, NamesEn, Error));
		for (uint8 T = 0; T < static_cast<uint8>(EWayfindingTechnique::Count); ++T)
		{
			const FName Id(LexToString(static_cast<EWayfindingTechnique>(T)));
			TestTrue(FString::Printf(TEXT("Nombre de %s"), *Id.ToString()), NamesEs.Contains(Id) && NamesEn.Contains(Id));
		}
	});
}

#endif
