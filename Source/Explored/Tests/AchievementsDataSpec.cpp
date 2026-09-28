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
	It("carga los 54 logros de achievements.json y el modelo los acepta", [this]()
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
		TestEqual(TEXT("Cincuenta y cuatro logros (biblia 07 §2)"), Achievements.Num(), 54);
		int32 EarlyAccess = 0;
		for (const FAchievementDef& Def : Achievements)
		{
			EarlyAccess += Def.Phase == EAchievementPhase::EarlyAccess ? 1 : 0;
		}
		TestEqual(TEXT("Treinta y seis de acceso anticipado"), EarlyAccess, 36);

		FAchievementsModel Model;
		TestTrue(FString::Printf(TEXT("El modelo lo acepta (%s)"), *Error), Model.Configure(Stats, Achievements, Error));

		// «Sin mapa» de principio a fin con los datos reales.
		Model.BeginRun(TEXT("Survivor"));
		const TArray<FName> Unlocked = Model.Report(TEXT("hidden_island_reached"));
		TestTrue(TEXT("Sin mapa"), Unlocked.Contains(FName(TEXT("sin_mapa"))));
		TestFalse(TEXT("Náufrago de verdad no, en Superviviente"), Unlocked.Contains(FName(TEXT("naufrago_de_verdad"))));
	});
}

#endif
