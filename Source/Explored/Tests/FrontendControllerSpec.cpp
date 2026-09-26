#include "Misc/AutomationTest.h"

#include "UI/ExploredPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredFrontendControllerSpec, "Explored.Frontend.Controller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredFrontendControllerSpec)

// La máquina de estados de AExploredPlayerController vive en funciones puras
// (namespace ExploredUI, UI/ExploredPlayerController.h) precisamente para
// poder testearla sin levantar un UWorld ni un PlayerController real: aquí
// se verifica que Menú → Juego → Pausa → Juego deja el cursor y la pausa en
// el estado correcto en cada paso.
void FExploredFrontendControllerSpec::Define()
{
	Describe("ExploredUI (estado del PlayerController)", [this]()
	{
		It("el menú muestra el cursor y no pausa el juego (aún no ha empezado)", [this]()
		{
			TestTrue(TEXT("Cursor visible en el menú"), ExploredUI::ShouldShowCursor(EExploredUIMode::Menu));
			TestFalse(TEXT("El menú no marca pausa de gameplay"), ExploredUI::ShouldPauseGame(EExploredUIMode::Menu));
		});

		It("jugando oculta el cursor y no pausa", [this]()
		{
			TestFalse(TEXT("Cursor oculto jugando"), ExploredUI::ShouldShowCursor(EExploredUIMode::Playing));
			TestFalse(TEXT("Jugando no pausa"), ExploredUI::ShouldPauseGame(EExploredUIMode::Playing));
		});

		It("en pausa muestra el cursor y pausa el juego", [this]()
		{
			TestTrue(TEXT("Cursor visible en pausa"), ExploredUI::ShouldShowCursor(EExploredUIMode::Paused));
			TestTrue(TEXT("Pausa marca pausa de gameplay"), ExploredUI::ShouldPauseGame(EExploredUIMode::Paused));
		});

		It("con el mapa en las manos muestra el cursor, no pausa y Escape vuelve a jugar (P-UI2)", [this]()
		{
			TestTrue(TEXT("Cursor visible con el mapa"), ExploredUI::ShouldShowCursor(EExploredUIMode::InHands));
			TestFalse(TEXT("El mundo sigue con el mapa en las manos"), ExploredUI::ShouldPauseGame(EExploredUIMode::InHands));
			TestEqual(TEXT("Escape guarda el mapa"), ExploredUI::NextModeOnEscape(EExploredUIMode::InHands), EExploredUIMode::Playing);
		});

		It("Escape recorre Menú → Juego → Pausa → Juego correctamente", [this]()
		{
			// Menú → Juego → Pausa → Juego (GDD §10: Escape solo pausa/reanuda estando en juego).
			EExploredUIMode Mode = EExploredUIMode::Menu;

			// En el menú, Escape no hace nada: la partida empieza por el botón «Nueva partida»/«Continuar».
			TestEqual(TEXT("Escape no afecta al menú"), ExploredUI::NextModeOnEscape(Mode), EExploredUIMode::Menu);

			Mode = EExploredUIMode::Playing;
			Mode = ExploredUI::NextModeOnEscape(Mode);
			TestEqual(TEXT("Escape jugando abre la pausa"), Mode, EExploredUIMode::Paused);
			TestTrue(TEXT("Cursor visible tras pausar"), ExploredUI::ShouldShowCursor(Mode));
			TestTrue(TEXT("Gameplay pausado"), ExploredUI::ShouldPauseGame(Mode));

			Mode = ExploredUI::NextModeOnEscape(Mode);
			TestEqual(TEXT("Escape en pausa reanuda"), Mode, EExploredUIMode::Playing);
			TestFalse(TEXT("Cursor oculto tras reanudar"), ExploredUI::ShouldShowCursor(Mode));
			TestFalse(TEXT("Gameplay no pausado"), ExploredUI::ShouldPauseGame(Mode));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
