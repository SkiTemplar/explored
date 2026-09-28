#include "Misc/AutomationTest.h"

#include "Achievements/AchievementsModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace AchievementsSpecDetail
{
	using FCond = FAchievementCondition;

	FAchievementStatDef Stat(const TCHAR* Id, EAchievementStatKind Kind, EAchievementStatScope Scope)
	{
		FAchievementStatDef Def;
		Def.Id = FName(Id);
		Def.Kind = Kind;
		Def.Scope = Scope;
		return Def;
	}

	FAchievementDef Achievement(const TCHAR* Id, FAchievementCondition Condition, TArray<FName> Modes = TArray<FName>())
	{
		FAchievementDef Def;
		Def.Id = FName(Id);
		Def.NameEs = Id;
		Def.NameEn = Id;
		Def.Modes = MoveTemp(Modes);
		Def.Condition = MoveTemp(Condition);
		return Def;
	}

	/** Un subconjunto de Content/Data/achievements.json con cada tipo de estadística y de condición. */
	TArray<FAchievementStatDef> MakeStats()
	{
		using K = EAchievementStatKind;
		using S = EAchievementStatScope;
		return {
			Stat(TEXT("fires_lit"), K::Counter, S::Profile),
			Stat(TEXT("palms_climbed"), K::Counter, S::Profile),
			Stat(TEXT("coconuts_opened"), K::Counter, S::Profile),
			Stat(TEXT("max_dive_depth_m"), K::Maximum, S::Profile),
			Stat(TEXT("days_survived"), K::Maximum, S::Run),
			Stat(TEXT("islands_visited"), K::Set, S::Run),
			Stat(TEXT("crops_harvested"), K::Set, S::Run),
			Stat(TEXT("building_pieces_built"), K::Set, S::Run),
			Stat(TEXT("boats_built"), K::Set, S::Run),
			Stat(TEXT("coast_drawn"), K::Flag, S::Run),
			Stat(TEXT("hidden_island_reached"), K::Flag, S::Run),
		};
	}

	TArray<FAchievementDef> MakeAchievements()
	{
		return {
			Achievement(TEXT("primer_fuego"), FCond::AtLeast(TEXT("fires_lit"), 1)),
			Achievement(TEXT("pulmones_de_perla"), FCond::AtLeast(TEXT("max_dive_depth_m"), 20)),
			Achievement(TEXT("tierra_firme"), FCond::AtLeast(TEXT("islands_visited"), 2)),
			Achievement(TEXT("el_limonero"), FCond::Contains(TEXT("crops_harvested"), TEXT("limonero"))),
			Achievement(TEXT("rey_del_cocotero"), FCond::All({
				FCond::AtLeast(TEXT("palms_climbed"), 1),
				FCond::AtLeast(TEXT("coconuts_opened"), 50) })),
			Achievement(TEXT("primer_techo"), FCond::Any({
				FCond::Contains(TEXT("building_pieces_built"), TEXT("refugio_inclinado")),
				FCond::Contains(TEXT("building_pieces_built"), TEXT("techo_palma")) })),
			Achievement(TEXT("un_ano_de_islas"), FCond::AtLeast(TEXT("days_survived"), 32), { TEXT("Survivor"), TEXT("Castaway") }),
			Achievement(TEXT("limon_zarpa"), FCond::All({
				FCond::Contains(TEXT("boats_built"), TEXT("barco_limon")),
				FCond::Flag(TEXT("hidden_island_reached")) })),
			Achievement(TEXT("naufrago_de_verdad"), FCond::Flag(TEXT("hidden_island_reached")), { TEXT("Castaway") }),
			Achievement(TEXT("sin_mapa"), FCond::All({
				FCond::Flag(TEXT("hidden_island_reached")),
				FCond::Not(FCond::Flag(TEXT("coast_drawn"))) })),
			Achievement(TEXT("pocos_fuegos"), FCond::Compare(TEXT("fires_lit"), EAchievementCompareOp::LessEqual, 2)),
			Achievement(TEXT("tres_islas_justas"), FCond::Compare(TEXT("islands_visited"), EAchievementCompareOp::Equal, 3)),
		};
	}

	bool Has(const TArray<FName>& Ids, const TCHAR* Id)
	{
		return Ids.Contains(FName(Id));
	}
}

BEGIN_DEFINE_SPEC(FAchievementsSpec, "Explored.Achievements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	FAchievementsModel Model;
END_DEFINE_SPEC(FAchievementsSpec)

void FAchievementsSpec::Define()
{
	using namespace AchievementsSpecDetail;

	BeforeEach([this]()
	{
		Model = FAchievementsModel();
		FString Error;
		TestTrue(TEXT("El catálogo de prueba es válido"), Model.Configure(MakeStats(), MakeAchievements(), Error));
		TestTrue(TEXT("Sin error de configuración"), Error.IsEmpty());
		Model.BeginRun(TEXT("Survivor"));
	});

	Describe("Configuración", [this]()
	{
		It("rechaza estadísticas desconocidas y tipos incompatibles", [this]()
		{
			FAchievementsModel Other;
			FString Error;
			TestFalse(TEXT("Estadística desconocida"), Other.Configure(MakeStats(),
				{ Achievement(TEXT("a"), FCond::AtLeast(TEXT("no_existe"), 1)) }, Error));
			TestTrue(TEXT("Explica el error"), Error.Contains(TEXT("no_existe")));
			TestFalse(TEXT("Contains sobre un contador"), Other.Configure(MakeStats(),
				{ Achievement(TEXT("a"), FCond::Contains(TEXT("fires_lit"), TEXT("x"))) }, Error));
			TestFalse(TEXT("Flag sobre un conjunto"), Other.Configure(MakeStats(),
				{ Achievement(TEXT("a"), FCond::Flag(TEXT("islands_visited"))) }, Error));
			TestFalse(TEXT("Compare sobre una marca"), Other.Configure(MakeStats(),
				{ Achievement(TEXT("a"), FCond::AtLeast(TEXT("coast_drawn"), 1)) }, Error));
			TestFalse(TEXT("Composición vacía"), Other.Configure(MakeStats(),
				{ Achievement(TEXT("a"), FCond::All({})) }, Error));
			TestEqual(TEXT("Tras un fallo el modelo queda vacío"), Other.GetAchievements().Num(), 0);
		});

		It("rechaza ids repetidos", [this]()
		{
			FAchievementsModel Other;
			FString Error;
			TestFalse(TEXT("Logro repetido"), Other.Configure(MakeStats(), {
				Achievement(TEXT("a"), FCond::AtLeast(TEXT("fires_lit"), 1)),
				Achievement(TEXT("a"), FCond::AtLeast(TEXT("fires_lit"), 2)) }, Error));
			TArray<FAchievementStatDef> Stats = MakeStats();
			const FAchievementStatDef Duplicate = Stats[0];
			Stats.Add(Duplicate);
			TestFalse(TEXT("Estadística repetida"), Other.Configure(Stats, {}, Error));
		});

		It("traduce los operadores del JSON", [this]()
		{
			EAchievementCompareOp Op = EAchievementCompareOp::Equal;
			TestTrue(TEXT(">="), ParseAchievementCompareOp(TEXT(">="), Op) && Op == EAchievementCompareOp::GreaterEqual);
			TestTrue(TEXT(">"), ParseAchievementCompareOp(TEXT(">"), Op) && Op == EAchievementCompareOp::Greater);
			TestTrue(TEXT("<="), ParseAchievementCompareOp(TEXT("<="), Op) && Op == EAchievementCompareOp::LessEqual);
			TestTrue(TEXT("<"), ParseAchievementCompareOp(TEXT("<"), Op) && Op == EAchievementCompareOp::Less);
			TestTrue(TEXT("=="), ParseAchievementCompareOp(TEXT("=="), Op) && Op == EAchievementCompareOp::Equal);
			TestFalse(TEXT("Desconocido"), ParseAchievementCompareOp(TEXT("=>"), Op));
		});
	});

	Describe("Estadísticas", [this]()
	{
		It("suma los contadores e ignora cantidades no positivas o no finitas", [this]()
		{
			Model.Report(TEXT("coconuts_opened"), 3.0);
			Model.Report(TEXT("coconuts_opened"));
			Model.Report(TEXT("coconuts_opened"), -5.0);
			Model.Report(TEXT("coconuts_opened"), 0.0);
			Model.Report(TEXT("coconuts_opened"), FMath::Sqrt(-1.0));
			TestEqual(TEXT("Cocos"), Model.GetNumber(TEXT("coconuts_opened")), 4.0);
		});

		It("guarda solo el máximo", [this]()
		{
			Model.Report(TEXT("max_dive_depth_m"), 12.0);
			Model.Report(TEXT("max_dive_depth_m"), 7.5);
			TestEqual(TEXT("Profundidad"), Model.GetNumber(TEXT("max_dive_depth_m")), 12.0);
		});

		It("no repite ids en los conjuntos e ignora los vacíos", [this]()
		{
			Model.ReportItem(TEXT("islands_visited"), TEXT("Landing"));
			Model.ReportItem(TEXT("islands_visited"), TEXT("landing"));
			Model.ReportItem(TEXT("islands_visited"), NAME_None);
			TestEqual(TEXT("Una isla"), Model.GetSetSize(TEXT("islands_visited")), 1);
			TestTrue(TEXT("Contiene el Amaraje"), Model.SetContains(TEXT("islands_visited"), TEXT("Landing")));
		});

		It("ignora las estadísticas desconocidas", [this]()
		{
			TestEqual(TEXT("Nada desbloqueado"), Model.Report(TEXT("no_existe"), 100.0).Num(), 0);
			TestFalse(TEXT("No se crea"), Model.IsKnownStat(TEXT("no_existe")));
			TestEqual(TEXT("Valor nulo"), Model.GetNumber(TEXT("no_existe")), 0.0);
		});

		It("vacía las estadísticas de partida al empezar otra y conserva las de perfil", [this]()
		{
			Model.Report(TEXT("fires_lit"));
			Model.ReportItem(TEXT("islands_visited"), TEXT("Landing"));
			Model.Report(TEXT("coast_drawn"));
			Model.BeginRun(TEXT("Castaway"));
			TestEqual(TEXT("Perfil intacto"), Model.GetNumber(TEXT("fires_lit")), 1.0);
			TestEqual(TEXT("Islas de la partida vacías"), Model.GetSetSize(TEXT("islands_visited")), 0);
			TestFalse(TEXT("Marca de partida borrada"), Model.HasFlag(TEXT("coast_drawn")));
			TestTrue(TEXT("Logro conservado"), Model.IsUnlocked(TEXT("primer_fuego")));
			TestTrue(TEXT("Modo nuevo"), Model.GetState().RunMode == FName(TEXT("Castaway")));
		});
	});

	Describe("Condiciones", [this]()
	{
		It("compara contadores", [this]()
		{
			const TArray<FName> Unlocked = Model.Report(TEXT("fires_lit"));
			TestTrue(TEXT("Primer fuego"), Has(Unlocked, TEXT("primer_fuego")));
		});

		It("compara máximos", [this]()
		{
			TestEqual(TEXT("A 19 m aún no"), Model.Report(TEXT("max_dive_depth_m"), 19.0).Num(), 0);
			TestTrue(TEXT("A 20 m sí"), Has(Model.Report(TEXT("max_dive_depth_m"), 20.0), TEXT("pulmones_de_perla")));
		});

		It("compara el tamaño de un conjunto", [this]()
		{
			TestFalse(TEXT("Una isla no basta"), Has(Model.ReportItem(TEXT("islands_visited"), TEXT("Landing")), TEXT("tierra_firme")));
			TestFalse(TEXT("Repetir la misma tampoco"), Has(Model.ReportItem(TEXT("islands_visited"), TEXT("Landing")), TEXT("tierra_firme")));
			TestTrue(TEXT("La segunda isla"), Has(Model.ReportItem(TEXT("islands_visited"), TEXT("Emerald")), TEXT("tierra_firme")));
		});

		It("busca un id en un conjunto", [this]()
		{
			TestFalse(TEXT("Otra cosecha"), Has(Model.ReportItem(TEXT("crops_harvested"), TEXT("taro")), TEXT("el_limonero")));
			TestTrue(TEXT("El primer limón"), Has(Model.ReportItem(TEXT("crops_harvested"), TEXT("limonero")), TEXT("el_limonero")));
		});

		It("admite comparaciones de igualdad y de «como mucho»", [this]()
		{
			TArray<FName> Unlocked = Model.Evaluate();
			TestTrue(TEXT("Cero hogueras cumple <= 2"), Has(Unlocked, TEXT("pocos_fuegos")));
			Model.ReportItem(TEXT("islands_visited"), TEXT("Landing"));
			Model.ReportItem(TEXT("islands_visited"), TEXT("Emerald"));
			TestFalse(TEXT("Dos islas no son tres"), Model.IsUnlocked(TEXT("tres_islas_justas")));
			TestTrue(TEXT("Tres islas justas"), Has(Model.ReportItem(TEXT("islands_visited"), TEXT("Smoke")), TEXT("tres_islas_justas")));
		});
	});

	Describe("Composición", [this]()
	{
		It("«all» exige todas las condiciones", [this]()
		{
			Model.Report(TEXT("coconuts_opened"), 50.0);
			TestFalse(TEXT("Sin trepar"), Model.IsUnlocked(TEXT("rey_del_cocotero")));
			TestTrue(TEXT("Tras trepar"), Has(Model.Report(TEXT("palms_climbed")), TEXT("rey_del_cocotero")));
		});

		It("«any» se conforma con una", [this]()
		{
			TestTrue(TEXT("Un techo de palma basta"),
				Has(Model.ReportItem(TEXT("building_pieces_built"), TEXT("techo_palma")), TEXT("primer_techo")));
		});

		It("«Limón zarpa» necesita el barco y la isla oculta", [this]()
		{
			Model.Report(TEXT("hidden_island_reached"));
			TestFalse(TEXT("Llegar en canoa no vale"), Model.IsUnlocked(TEXT("limon_zarpa")));
			Model.BeginRun(TEXT("Survivor"));
			Model.ReportItem(TEXT("boats_built"), TEXT("barco_limon"));
			TestTrue(TEXT("Con el «Limón»"), Has(Model.Report(TEXT("hidden_island_reached")), TEXT("limon_zarpa")));
		});
	});

	Describe("Sin mapa", [this]()
	{
		It("se consigue llegando a la isla oculta sin haber dibujado costa", [this]()
		{
			TestTrue(TEXT("Sin trazos"), Has(Model.Report(TEXT("hidden_island_reached")), TEXT("sin_mapa")));
		});

		It("no se consigue si antes se dibujó alguna costa", [this]()
		{
			Model.Report(TEXT("coast_drawn"));
			TestFalse(TEXT("Con trazos"), Has(Model.Report(TEXT("hidden_island_reached")), TEXT("sin_mapa")));
			TestFalse(TEXT("Tampoco al reevaluar"), Has(Model.Evaluate(), TEXT("sin_mapa")));
		});

		It("dibujar después no lo retira y otra partida limpia lo permite", [this]()
		{
			Model.Report(TEXT("coast_drawn"));
			Model.Report(TEXT("hidden_island_reached"));
			TestFalse(TEXT("Partida con mapa"), Model.IsUnlocked(TEXT("sin_mapa")));
			Model.BeginRun(TEXT("Explorer"));
			TestTrue(TEXT("Partida nueva sin mapa"), Has(Model.Report(TEXT("hidden_island_reached")), TEXT("sin_mapa")));
			Model.Report(TEXT("coast_drawn"));
			TestTrue(TEXT("Sigue desbloqueado"), Model.IsUnlocked(TEXT("sin_mapa")));
		});
	});

	Describe("Desbloqueo", [this]()
	{
		It("anuncia cada logro una sola vez", [this]()
		{
			TestTrue(TEXT("Primera vez"), Has(Model.Report(TEXT("fires_lit")), TEXT("primer_fuego")));
			TestEqual(TEXT("Segunda hoguera"), Model.Report(TEXT("fires_lit")).Num(), 0);
			TestFalse(TEXT("Reevaluar no lo repite"), Has(Model.Evaluate(), TEXT("primer_fuego")));
			Model.BeginRun(TEXT("Castaway"));
			TestFalse(TEXT("Ni en otra partida"), Has(Model.Report(TEXT("fires_lit")), TEXT("primer_fuego")));
			int32 Count = 0;
			for (const FName& Id : Model.GetState().Unlocked)
			{
				Count += Id == FName(TEXT("primer_fuego")) ? 1 : 0;
			}
			TestEqual(TEXT("Una entrada en el estado"), Count, 1);
		});

		It("un logro que deja de cumplirse no se pierde", [this]()
		{
			TestTrue(TEXT("Al empezar"), Has(Model.Evaluate(), TEXT("pocos_fuegos")));
			Model.Report(TEXT("fires_lit"), 5.0);
			TestTrue(TEXT("Sigue desbloqueado"), Model.IsUnlocked(TEXT("pocos_fuegos")));
		});
	});

	Describe("Modos", [this]()
	{
		It("«Náufrago de verdad» solo cuenta en modo Náufrago", [this]()
		{
			TestFalse(TEXT("En Superviviente"), Has(Model.Report(TEXT("hidden_island_reached")), TEXT("naufrago_de_verdad")));
			TestFalse(TEXT("Ni al reevaluar"), Has(Model.Evaluate(), TEXT("naufrago_de_verdad")));
			Model.BeginRun(TEXT("Castaway"));
			TestTrue(TEXT("En Náufrago"), Has(Model.Report(TEXT("hidden_island_reached")), TEXT("naufrago_de_verdad")));
		});

		It("admite varios modos y ninguno fuera de partida", [this]()
		{
			Model.BeginRun(TEXT("Explorer"));
			Model.Report(TEXT("days_survived"), 40.0);
			TestFalse(TEXT("No en Explorador"), Model.IsUnlocked(TEXT("un_ano_de_islas")));
			Model.BeginRun(NAME_None);
			Model.Report(TEXT("days_survived"), 40.0);
			TestFalse(TEXT("No fuera de partida"), Model.IsUnlocked(TEXT("un_ano_de_islas")));
			Model.BeginRun(TEXT("Survivor"));
			TestTrue(TEXT("Sí en Superviviente"), Has(Model.Report(TEXT("days_survived"), 32.0), TEXT("un_ano_de_islas")));
		});

		It("los logros sin restricción valen en cualquier modo", [this]()
		{
			Model.BeginRun(TEXT("Explorer"));
			TestTrue(TEXT("Primer fuego en Explorador"), Has(Model.Report(TEXT("fires_lit")), TEXT("primer_fuego")));
		});
	});

	Describe("Fases", [this]()
	{
		It("un logro de una fase sin publicar no se desbloquea, pero su estadística cuenta", [this]()
		{
			TArray<FAchievementDef> Defs = MakeAchievements();
			FAchievementDef Eggs = Achievement(TEXT("huevos_por_docenas"), FCond::AtLeast(TEXT("coconuts_opened"), 3));
			Eggs.Phase = EAchievementPhase::Phase2;
			Defs.Add(Eggs);
			FAchievementsModel Phased;
			FString Error;
			if (!TestTrue(TEXT("Configura"), Phased.Configure(MakeStats(), Defs, Error)))
			{
				return;
			}
			Phased.BeginRun(TEXT("Survivor"));
			Phased.SetReleasedPhase(EAchievementPhase::EarlyAccess);
			TestFalse(TEXT("No se anuncia en AA"), Has(Phased.Report(TEXT("coconuts_opened"), 5.0), TEXT("huevos_por_docenas")));
			TestFalse(TEXT("No está desbloqueado"), Phased.IsUnlocked(TEXT("huevos_por_docenas")));
			TestEqual(TEXT("La estadística sí cuenta"), Phased.GetNumber(TEXT("coconuts_opened")), 5.0);
			TestTrue(TEXT("Los de AA siguen igual"), Has(Phased.Report(TEXT("fires_lit")), TEXT("primer_fuego")));

			Phased.SetReleasedPhase(EAchievementPhase::Phase2);
			TestTrue(TEXT("Al publicar F2, Evaluate lo recupera"), Has(Phased.Evaluate(), TEXT("huevos_por_docenas")));
			TestFalse(TEXT("Y no lo repite"), Has(Phased.Evaluate(), TEXT("huevos_por_docenas")));
		});

		It("por defecto todas las fases están publicadas y las fases se ordenan", [this]()
		{
			FAchievementsModel Fresh;
			TestTrue(TEXT("Por defecto, F3"), Fresh.GetReleasedPhase() == EAchievementPhase::Phase3);
			FAchievementDef Late;
			Late.Phase = EAchievementPhase::Phase3;
			TestTrue(TEXT("F3 publicado por defecto"), Fresh.IsReleased(Late));
			Fresh.SetReleasedPhase(EAchievementPhase::Phase2);
			TestFalse(TEXT("F3 no entra en F2"), Fresh.IsReleased(Late));
			Late.Phase = EAchievementPhase::EarlyAccess;
			TestTrue(TEXT("AA entra en F2"), Fresh.IsReleased(Late));
		});

		It("lee fase, rareza y alcance con la grafía exacta del JSON", [this]()
		{
			EAchievementPhase Phase = EAchievementPhase::EarlyAccess;
			TestTrue(TEXT("F2"), ParseAchievementPhase(TEXT("F2"), Phase) && Phase == EAchievementPhase::Phase2);
			TestFalse(TEXT("«f2» no"), ParseAchievementPhase(TEXT("f2"), Phase));
			TestFalse(TEXT("Vacío no"), ParseAchievementPhase(TEXT(""), Phase));
			TestTrue(TEXT("No toca la salida al fallar"), Phase == EAchievementPhase::Phase2);
			TestEqual(TEXT("Ida y vuelta"), FString(LexToString(EAchievementPhase::Phase3)), FString(TEXT("F3")));

			EAchievementRarity Rarity = EAchievementRarity::Common;
			TestTrue(TEXT("muy_raro"), ParseAchievementRarity(TEXT("muy_raro"), Rarity) && Rarity == EAchievementRarity::VeryRare);
			TestFalse(TEXT("«común» con tilde no"), ParseAchievementRarity(TEXT("común"), Rarity));
			for (EAchievementRarity R : { EAchievementRarity::Common, EAchievementRarity::Uncommon, EAchievementRarity::Rare, EAchievementRarity::VeryRare })
			{
				EAchievementRarity Back = EAchievementRarity::Common;
				TestTrue(TEXT("Rareza ida y vuelta"), ParseAchievementRarity(LexToString(R), Back) && Back == R);
			}

			EAchievementCoopScope Scope = EAchievementCoopScope::Actor;
			TestTrue(TEXT("witness"), ParseAchievementCoopScope(TEXT("witness"), Scope) && Scope == EAchievementCoopScope::Witness);
			TestFalse(TEXT("«World» no"), ParseAchievementCoopScope(TEXT("World"), Scope));
			for (EAchievementCoopScope S : { EAchievementCoopScope::Actor, EAchievementCoopScope::World, EAchievementCoopScope::Witness })
			{
				EAchievementCoopScope Back = EAchievementCoopScope::Actor;
				TestTrue(TEXT("Alcance ida y vuelta"), ParseAchievementCoopScope(LexToString(S), Back) && Back == S);
			}
		});
	});

	Describe("Cooperativo", [this]()
	{
		It("actor solo llega a quien hace la acción y world a todos", [this]()
		{
			TestTrue(TEXT("Actor"), FAchievementsModel::ReachesPlayer(EAchievementCoopScope::Actor, true, 1000.0f));
			TestFalse(TEXT("Actor, otro jugador al lado"), FAchievementsModel::ReachesPlayer(EAchievementCoopScope::Actor, false, 0.0f));
			TestTrue(TEXT("World lejos"), FAchievementsModel::ReachesPlayer(EAchievementCoopScope::World, false, 100000.0f));
			TestTrue(TEXT("World sin distancia válida"), FAchievementsModel::ReachesPlayer(EAchievementCoopScope::World, false, std::numeric_limits<float>::quiet_NaN()));
		});

		It("witness llega hasta 50 m, incluido el borde, y nunca con distancias corruptas", [this]()
		{
			using S = EAchievementCoopScope;
			TestTrue(TEXT("Quien lo hace"), FAchievementsModel::ReachesPlayer(S::Witness, true, std::numeric_limits<float>::quiet_NaN()));
			TestTrue(TEXT("A 0 m"), FAchievementsModel::ReachesPlayer(S::Witness, false, 0.0f));
			TestTrue(TEXT("A 50 m justos"), FAchievementsModel::ReachesPlayer(S::Witness, false, FAchievementsModel::WitnessRadiusMeters));
			TestFalse(TEXT("A 50,01 m"), FAchievementsModel::ReachesPlayer(S::Witness, false, 50.01f));
			TestFalse(TEXT("NaN"), FAchievementsModel::ReachesPlayer(S::Witness, false, std::numeric_limits<float>::quiet_NaN()));
			TestFalse(TEXT("Infinito"), FAchievementsModel::ReachesPlayer(S::Witness, false, std::numeric_limits<float>::infinity()));
			TestFalse(TEXT("Negativo"), FAchievementsModel::ReachesPlayer(S::Witness, false, -1.0f));
		});
	});

	Describe("Progreso", [this]()
	{
		It("avanza en proporción a la meta y llega a 1 al desbloquearse", [this]()
		{
			TestEqual(TEXT("Al principio"), Model.GetProgress(TEXT("pulmones_de_perla")), 0.0f);
			Model.Report(TEXT("max_dive_depth_m"), 5.0);
			TestEqual(TEXT("A 5 de 20 m"), Model.GetProgress(TEXT("pulmones_de_perla")), 0.25f, 1e-4f);
			Model.Report(TEXT("max_dive_depth_m"), 25.0);
			TestEqual(TEXT("Hecho"), Model.GetProgress(TEXT("pulmones_de_perla")), 1.0f);
		});

		It("cuenta el tamaño de los conjuntos", [this]()
		{
			Model.ReportItem(TEXT("islands_visited"), TEXT("Landing"));
			TestEqual(TEXT("Una de dos islas"), Model.GetProgress(TEXT("tierra_firme")), 0.5f, 1e-4f);
		});

		It("promedia «all» y toma la mejor rama de «any»", [this]()
		{
			Model.Report(TEXT("coconuts_opened"), 25.0);
			TestEqual(TEXT("Media de 0 y 0,5"), Model.GetProgress(TEXT("rey_del_cocotero")), 0.25f, 1e-4f);
			Model.Report(TEXT("palms_climbed"));
			TestEqual(TEXT("Media de 1 y 0,5"), Model.GetProgress(TEXT("rey_del_cocotero")), 0.75f, 1e-4f);
			TestEqual(TEXT("«any» sin nada"), Model.GetProgress(TEXT("primer_techo")), 0.0f);
		});

		It("no llega a 1 sin cumplirse", [this]()
		{
			Model.Report(TEXT("coconuts_opened"), 50.0);
			TestTrue(TEXT("Por debajo de 1"), Model.GetProgress(TEXT("rey_del_cocotero")) < 1.0f);
		});

		It("una restricción «not» rota deja el progreso a cero", [this]()
		{
			TestEqual(TEXT("Sin trazos ni isla"), Model.GetProgress(TEXT("sin_mapa")), 0.0f);
			Model.Report(TEXT("coast_drawn"));
			Model.Report(TEXT("hidden_island_reached"));
			TestEqual(TEXT("Imposible en esta partida"), Model.GetProgress(TEXT("sin_mapa")), 0.0f);
		});

		It("devuelve 0 para ids desconocidos", [this]()
		{
			TestEqual(TEXT("Desconocido"), Model.GetProgress(TEXT("no_existe")), 0.0f);
		});
	});

	Describe("Guardado", [this]()
	{
		It("el estado plano se copia y restaura sin anunciar de nuevo", [this]()
		{
			Model.Report(TEXT("fires_lit"), 2.0);
			Model.Report(TEXT("max_dive_depth_m"), 8.0);
			Model.ReportItem(TEXT("islands_visited"), TEXT("Landing"));
			Model.ReportItem(TEXT("crops_harvested"), TEXT("limonero"));
			Model.Report(TEXT("coast_drawn"));
			const FAchievementsState Saved = Model.GetState();

			FAchievementsModel Loaded;
			FString Error;
			TestTrue(TEXT("Configura"), Loaded.Configure(MakeStats(), MakeAchievements(), Error));
			Loaded.RestoreState(Saved);
			TestTrue(TEXT("Estado idéntico"), Loaded.GetState() == Saved);
			TestEqual(TEXT("Nada nuevo al reevaluar"), Loaded.Evaluate().Num(), 0);
			TestTrue(TEXT("Primer fuego guardado"), Loaded.IsUnlocked(TEXT("primer_fuego")));
			TestTrue(TEXT("El limonero guardado"), Loaded.IsUnlocked(TEXT("el_limonero")));
			TestEqual(TEXT("Mismo progreso"), Loaded.GetProgress(TEXT("pulmones_de_perla")), Model.GetProgress(TEXT("pulmones_de_perla")));
			TestTrue(TEXT("Mismo modo"), Loaded.GetState().RunMode == FName(TEXT("Survivor")));

			Loaded.ReportItem(TEXT("islands_visited"), TEXT("Emerald"));
			TestTrue(TEXT("Sigue desde donde estaba"), Loaded.IsUnlocked(TEXT("tierra_firme")));
			TestFalse(TEXT("Y el original no cambia"), Model.IsUnlocked(TEXT("tierra_firme")));
			TestFalse(TEXT("Los estados ya difieren"), Loaded.GetState() == Saved);
		});

		It("recupera logros pendientes al reevaluar un estado antiguo", [this]()
		{
			FAchievementsState Old;
			Old.RunMode = TEXT("Survivor");
			Old.Profile.Numbers.Add(TEXT("fires_lit"), 3.0);
			Model.RestoreState(Old);
			TestFalse(TEXT("Restaurar no anuncia"), Model.IsUnlocked(TEXT("primer_fuego")));
			TestTrue(TEXT("Evaluate lo desbloquea"), Has(Model.Evaluate(), TEXT("primer_fuego")));
		});
	});
}

#endif
