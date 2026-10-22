#include "Misc/AutomationTest.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Los .locres compilados (Content/Localization/Game/<cultura>/Game.locres) existen y traen el inglés
 * de los textos del C++. Sin ellos, Ajustes → Idioma → English deja menús y HUD en español y solo los
 * datos (Content/Data) en inglés. Necesita el motor (lector de .locres); no va al host.
 * Si falla tras cambiar un texto: docs/tecnico/localizacion.md, «Compilar en el editor».
 */
BEGIN_DEFINE_SPEC(FLocresSpec, "Explored.Localization.Locres",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	/** Traducción de Namespace/Key en el .locres de la cultura; vacía si no está. */
	FString Lookup(const FTextLocalizationResource& Res, const TCHAR* Namespace, const TCHAR* Key, const TCHAR* Source)
	{
		const FTextLocalizationResource::FEntry* Entry = Res.Entries.Find(FTextId(FTextKey(Namespace), FTextKey(Key)));
		if (!Entry || !Entry->LocalizedString)
		{
			return FString();
		}
		// El hash del texto fuente detecta un .locres compilado antes de cambiar el español.
		TestEqual(FString::Printf(TEXT("%s,%s compilado con el español actual"), Namespace, Key),
			Entry->SourceStringHash, FTextLocalizationResource::HashString(Source));
		return *Entry->LocalizedString;
	}

	FString LocresPath(const TCHAR* Culture) const
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization"), TEXT("Game"), Culture, TEXT("Game.locres"));
	}
END_DEFINE_SPEC(FLocresSpec)

void FLocresSpec::Define()
{
	It(TEXT("el inglés de los menús está compilado y al día"), [this]()
	{
		FTextLocalizationResource En;
		if (!TestTrue(TEXT("en/Game.locres se lee"), En.LoadFromFile(LocresPath(TEXT("en")), 0)))
		{
			return;
		}
		TestTrue(TEXT("trae los textos del C++"), En.Entries.Num() > 200);
		TestEqual(TEXT("Salir del juego"), Lookup(En, TEXT("ExploredUI"), TEXT("QuitGame"), TEXT("Salir del juego")), FString(TEXT("Quit game")));
		TestEqual(TEXT("Reanudar"), Lookup(En, TEXT("ExploredUI"), TEXT("Resume"), TEXT("Reanudar")), FString(TEXT("Resume")));
		TestEqual(TEXT("Pregunta de salir del juego"),
			Lookup(En, TEXT("ExploredUI"), TEXT("QuitConfirm"), TEXT("¿Salir del juego? Lo que no hayas guardado se perderá.")),
			FString(TEXT("Quit the game? Anything you have not saved will be lost.")));
	});

	It(TEXT("el español también tiene su .locres"), [this]()
	{
		FTextLocalizationResource Es;
		TestTrue(TEXT("es/Game.locres se lee"), Es.LoadFromFile(LocresPath(TEXT("es")), 0));
	});
}

#endif
