#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#include "WorldGen/IslandLayout.h"
#include "WorldGen/IslandLayoutLoader.h"
#include "WorldGen/IslandPropsLoader.h"

#if WITH_DEV_AUTOMATION_TESTS

// Datos reales de Content/Data (necesita el motor: Json y rutas del proyecto; no va al host).
BEGIN_DEFINE_SPEC(FIslandDataSpec, "Explored.WorldGen.IslandData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandDataSpec)

void FIslandDataSpec::Define()
{
	It("island.json coincide con la copia en código (Provisional)", [this]()
	{
		FString Json;
		FIslandLayout Parsed;
		FString Error;
		if (!TestTrue(TEXT("se lee"), FFileHelper::LoadFileToString(Json, *ExploredIsland::IslandJsonPath()))
			|| !TestTrue(FString::Printf(TEXT("es válido: %s"), *Error), ExploredIsland::ParseIslandJson(Json, Parsed, Error)))
		{
			return;
		}
		const FIslandLayout Code = FIslandLayout::Provisional();
		TestEqual(TEXT("rocas"), Parsed.Rocks.Num(), Code.Rocks.Num());
		TestEqual(TEXT("caminos"), Parsed.Paths.Num(), Code.Paths.Num());
		for (float X = -180.0f; X <= 290.0f; X += 23.0f)
		{
			for (float Y = -210.0f; Y <= 210.0f; Y += 19.0f)
			{
				TestEqual(FString::Printf(TEXT("altura en (%.0f, %.0f)"), X, Y), Parsed.SampleColumn(X, Y).Height, Code.SampleColumn(X, Y).Height, 0.01f);
			}
		}
		const FIslandPoint* Raft = Parsed.FindPoint(TEXT("raft_site"));
		const FIslandPoint* RaftCode = Code.FindPoint(TEXT("raft_site"));
		if (TestNotNull(TEXT("raft_site"), Raft) && TestNotNull(TEXT("raft_site en código"), RaftCode))
		{
			TestEqual(TEXT("mismo sitio de la balsa"), Raft->Location, RaftCode->Location);
		}
	});

	It("island_props.json es válido y cumple las cantidades del GDD", [this]()
	{
		FString Json;
		FIslandProps Props;
		FString Error;
		if (!TestTrue(TEXT("se lee"), FFileHelper::LoadFileToString(Json, *ExploredIslandProps::IslandPropsJsonPath()))
			|| !TestTrue(FString::Printf(TEXT("es válido: %s"), *Error), ExploredIslandProps::ParseIslandPropsJson(Json, Props, Error)))
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("GDD: %s"), *Error), Props.MatchesGdd(Error));
		const FIslandProp* Lantern = Props.FindProp(TEXT("faro_linterna"));
		if (TestNotNull(TEXT("linterna"), Lantern))
		{
			TestEqual(TEXT("anclada a la torre"), Lantern->Anchor, FName(TEXT("faro_torre")));
			// SM_FaroLinterna_01 se apoya en la galería de SM_FaroTorre_01, a 11 m (manifest de K2).
			TestEqual(TEXT("a 11 m"), Lantern->Elevation, 11.0f);
		}
		const FIslandProp* Bridge = Props.FindProp(TEXT("carguero_puente"));
		if (TestNotNull(TEXT("puente"), Bridge))
		{
			// El casco se exporta escorado 8° a estribor (+Y en Unreal) y con la proa 2° arriba.
			TestEqual(TEXT("escora del casco"), Bridge->Roll, 8.0f);
			TestEqual(TEXT("proa arriba"), Bridge->Pitch, 2.0f);
		}
	});

	It("rechaza un island_props.json con un tipo sin definir (caso negativo)", [this]()
	{
		FIslandProps Props;
		FString Error;
		const FString Bad = TEXT("{\"types\": {}, \"props\": [{\"id\": \"a\", \"type\": \"palma\", \"at\": [0, 0]}]}");
		TestFalse(TEXT("inválido"), ExploredIslandProps::ParseIslandPropsJson(Bad, Props, Error));
		TestTrue(TEXT("dice por qué"), Error.Contains(TEXT("tipo sin definir")));
		const FString Scaled = TEXT("{\"types\": {\"roca\": {\"shape\": \"cube\", \"collision\": \"solid\", \"size\": [1, 1, 1], \"color\": 0, \"meshScale\": [1.5, 1.5, 2]}}, \"props\": []}");
		TestTrue(TEXT("meshScale válido"), ExploredIslandProps::ParseIslandPropsJson(Scaled, Props, Error));
		const FIslandPropType* Rock = Props.FindType(TEXT("roca"));
		TestTrue(TEXT("escala leída"), Rock && Rock->MeshScale.Equals(FVector(1.5, 1.5, 2.0)));
		const FString BadScale = Scaled.Replace(TEXT("[1.5, 1.5, 2]"), TEXT("[1.5, 0, 2]"));
		TestFalse(TEXT("meshScale con un cero"), ExploredIslandProps::ParseIslandPropsJson(BadScale, Props, Error));
	});
}

#endif
