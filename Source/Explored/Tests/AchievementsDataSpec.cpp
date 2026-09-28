#include "Misc/AutomationTest.h"

#include "Achievements/AchievementsModel.h"
#include "Achievements/AchievementsSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// Solo en el editor: lee el achievements.json real con el parser de la capa de Unreal.
// La lógica del modelo se prueba en AchievementsSpec (también en el host).
BEGIN_DEFINE_SPEC(FAchievementsDataSpec, "Explored.Achievements.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FAchievementsDataSpec)

void FAchievementsDataSpec::Define()
{
	It("carga los logros de achievements.json y el modelo los acepta", [this]()
	{
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data/achievements.json");
		if (!TestTrue(TEXT("Existe el fichero"), FFileHelper::LoadFileToString(Text, *Path)))
		{
			return;
		}

		TArray<FAchievementStatDef> Stats;
		TArray<FAchievementDef> Achievements;
		FString Error;
		TestTrue(TEXT("Se parsea"), UAchievementsSubsystem::ParseAchievementsJson(Text, Stats, Achievements, Error));
		// Biblia 07 §2: los 30 del GDD §16 más los nuevos, hasta 54 (DataCheck comprueba los ids).
		TestTrue(TEXT("Entre 30 y 54 logros"), Achievements.Num() >= 30 && Achievements.Num() <= 54);
		for (const TCHAR* Id : {TEXT("primer_fuego"), TEXT("sin_mapa"), TEXT("primera_palada"), TEXT("manazas")})
		{
			TestTrue(FString::Printf(TEXT("Existe «%s»"), Id),
				Achievements.ContainsByPredicate([Id](const FAchievementDef& Def) { return Def.Id == FName(Id); }));
		}

		FAchievementsModel Model;
		TestTrue(FString::Printf(TEXT("El modelo lo acepta (%s)"), *Error), Model.Configure(Stats, Achievements, Error));

		// «Sin mapa» de principio a fin con los datos reales.
		Model.BeginRun(TEXT("Survivor"));
		const TArray<FName> Unlocked = Model.Report(TEXT("hidden_island_reached"));
		TestTrue(TEXT("Sin mapa"), Unlocked.Contains(FName(TEXT("sin_mapa"))));
		TestFalse(TEXT("Náufrago de verdad no, en Superviviente"), Unlocked.Contains(FName(TEXT("naufrago_de_verdad"))));

		// Minería (biblia 07 §2.3) con los datos reales: tres estratos y la obsidiana.
		Model.ReportItem(TEXT("strata_mined"), TEXT("tierra"));
		Model.ReportItem(TEXT("strata_mined"), TEXT("basalto"));
		const TArray<FName> Mined = Model.ReportItem(TEXT("strata_mined"), TEXT("obsidiana"));
		TestTrue(TEXT("Buscador de vetas"), Mined.Contains(FName(TEXT("buscador_de_vetas"))));
		TestTrue(TEXT("Filo de obsidiana"), Mined.Contains(FName(TEXT("filo_de_obsidiana"))));
		TestTrue(TEXT("Manazas a las veinte herramientas"),
			Model.Report(TEXT("tools_broken_on_wrong_material"), 20.0).Contains(FName(TEXT("manazas"))));
	});
}

#endif
