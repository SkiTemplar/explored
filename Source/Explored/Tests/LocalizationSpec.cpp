#include "Misc/AutomationTest.h"

#include "Core/ExploredLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLocalizationSpec, "Explored.Localization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FLocalizationSpec)

void FLocalizationSpec::Define()
{
	Describe("IsEnglishCulture", [this]()
	{
		It("reconoce el inglés con y sin región", [this]()
		{
			TestTrue(TEXT("en"), ExploredLocalization::IsEnglishCulture(TEXT("en")));
			TestTrue(TEXT("en-US"), ExploredLocalization::IsEnglishCulture(TEXT("en-US")));
			TestTrue(TEXT("EN_gb"), ExploredLocalization::IsEnglishCulture(TEXT("EN_gb")));
		});

		It("trata el resto de culturas como español, el idioma fuente", [this]()
		{
			TestFalse(TEXT("es"), ExploredLocalization::IsEnglishCulture(TEXT("es")));
			TestFalse(TEXT("es-ES"), ExploredLocalization::IsEnglishCulture(TEXT("es-ES")));
			TestFalse(TEXT("vacía"), ExploredLocalization::IsEnglishCulture(TEXT("")));
			TestFalse(TEXT("e"), ExploredLocalization::IsEnglishCulture(TEXT("e")));
			// "eng" no es una cultura de Unreal y no debe confundirse con "en".
			TestFalse(TEXT("eng"), ExploredLocalization::IsEnglishCulture(TEXT("eng")));
		});
	});

	Describe("Pick", [this]()
	{
		It("elige el texto del idioma de la cultura", [this]()
		{
			const FExploredLocalizedString Name(TEXT("Limón"), TEXT("Lemon"));
			TestEqual(TEXT("es"), Name.Get(TEXT("es")), FString(TEXT("Limón")));
			TestEqual(TEXT("en-GB"), Name.Get(TEXT("en-GB")), FString(TEXT("Lemon")));
			TestTrue(TEXT("completo"), Name.IsComplete());
		});

		It("cae al español si falta el inglés", [this]()
		{
			const FExploredLocalizedString Name(TEXT("Esqueje"), FString());
			TestEqual(TEXT("en sin traducción"), Name.Get(TEXT("en")), FString(TEXT("Esqueje")));
			TestFalse(TEXT("incompleto"), Name.IsComplete());
		});

		It("no traduce la dedicatoria: es la misma en los dos idiomas", [this]()
		{
			const FString Dedication = TEXT("Para Almudena, mi Limón");
			TestEqual(TEXT("en"), ExploredLocalization::Pick(Dedication, Dedication, TEXT("en")), Dedication);
		});
	});
}

#endif
