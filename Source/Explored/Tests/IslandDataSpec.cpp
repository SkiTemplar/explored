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
			TestEqual(TEXT("a 12 m"), Lantern->Elevation, 12.0f);
		}
	});

	It("rechaza un island_props.json con un tipo sin definir (caso negativo)", [this]()
	{
		FIslandProps Props;
		FString Error;
		const FString Bad = TEXT("{\"types\": {}, \"props\": [{\"id\": \"a\", \"type\": \"palma\", \"at\": [0, 0]}]}");
		TestFalse(TEXT("inválido"), ExploredIslandProps::ParseIslandPropsJson(Bad, Props, Error));
		TestTrue(TEXT("dice por qué"), Error.Contains(TEXT("tipo sin definir")));
	});
}

#endif
