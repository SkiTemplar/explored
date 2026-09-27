#include "Misc/AutomationTest.h"

#include "UI/SettingsLogic.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredSettingsLogicSpec, "Explored.UI.SettingsLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredSettingsLogicSpec)

namespace ExploredSettingsLogicSpecHelpers
{
	using namespace ExploredSettingsLogic;

	/** Teclas efectivas por defecto de todas las acciones remapeables. */
	TArray<FKeyBinding> DefaultBindings()
	{
		TArray<FKeyBinding> Bindings;
		for (const FRemappableAction& Action : GetRemappableActions())
		{
			Bindings.Add({ Action.ActionName, Action.DefaultKey });
		}
		return Bindings;
	}
}

void FExploredSettingsLogicSpec::Define()
{
	using namespace ExploredSettingsLogic;
	using namespace ExploredSettingsLogicSpecHelpers;

	Describe("slider ↔ valor", [this]()
	{
		It("los extremos del slider dan los extremos del rango (H2: no solo 0 o 100)", [this]()
		{
			TestEqual(TEXT("Volumen a la izquierda"), SliderToValue(0.0f, VolumeRange), 0.0f);
			TestEqual(TEXT("Volumen a la derecha"), SliderToValue(1.0f, VolumeRange), 100.0f);
			TestEqual(TEXT("Volumen a la mitad"), SliderToValue(0.5f, VolumeRange), 50.0f, 0.001f);
			TestEqual(TEXT("Volumen al 37 %"), SliderToValue(0.37f, VolumeRange), 37.0f, 0.001f);
		});

		It("ida y vuelta conserva el valor en todos los rangos", [this]()
		{
			const FSettingRange Ranges[] = { BrightnessRange, VolumeRange, FieldOfViewRange, SensitivityRange };
			for (const FSettingRange& Range : Ranges)
			{
				for (float Alpha = 0.0f; Alpha <= 1.0f; Alpha += 0.125f)
				{
					const float Value = SliderToValue(Alpha, Range);
					TestTrue(TEXT("Dentro del rango"), Value >= Range.Min && Value <= Range.Max);
					TestEqual(TEXT("Vuelve a la misma posición"), ValueToSlider(Value, Range), Alpha, 0.0001f);
				}
			}
		});

		It("el valor por defecto de la sensibilidad no queda en el extremo del slider", [this]()
		{
			const float Slider = ValueToSlider(SensitivityRange.Default, SensitivityRange);
			TestTrue(TEXT("Posición interior"), Slider > 0.0f && Slider < 1.0f);
			TestEqual(TEXT("Posición de 1,0 en [0,1; 5]"), Slider, (1.0f - 0.1f) / (5.0f - 0.1f), 0.0001f);
		});

		It("recorta posiciones fuera de [0, 1] y valores fuera de rango", [this]()
		{
			TestEqual(TEXT("Slider negativo"), SliderToValue(-3.0f, FieldOfViewRange), 70.0f);
			TestEqual(TEXT("Slider mayor que 1"), SliderToValue(7.0f, FieldOfViewRange), 110.0f);
			TestEqual(TEXT("Valor por debajo"), ValueToSlider(10.0f, FieldOfViewRange), 0.0f);
			TestEqual(TEXT("Valor por encima"), ValueToSlider(500.0f, FieldOfViewRange), 1.0f);
		});

		It("el volumen en porcentaje se convierte a ganancia lineal", [this]()
		{
			TestEqual(TEXT("100 %"), VolumeToGain(100.0f), 1.0f);
			TestEqual(TEXT("25 %"), VolumeToGain(25.0f), 0.25f, 0.0001f);
			TestEqual(TEXT("500 % (M14) se recorta"), VolumeToGain(500.0f), 1.0f);
			TestEqual(TEXT("Negativo se recorta"), VolumeToGain(-20.0f), 0.0f);
		});

		It("el ambiente no se atenúa dos veces si pasa por su SoundClass", [this]()
		{
			TestEqual(TEXT("Con SoundClass lo aplica la mezcla"), AmbienceLayerGain(30.0f, true), 1.0f);
			TestEqual(TEXT("Sin SoundClass lo aplica el subsistema"), AmbienceLayerGain(30.0f, false), 0.3f, 0.0001f);
		});
	});

	Describe("validación de valores leídos del ini (M14)", [this]()
	{
		It("recorta cada rango y respeta los valores válidos", [this]()
		{
			TestEqual(TEXT("Brillo bajo"), ClampToRange(0.2f, BrightnessRange), 1.7f);
			TestEqual(TEXT("Brillo alto"), ClampToRange(9.0f, BrightnessRange), 2.7f);
			TestEqual(TEXT("Brillo válido"), ClampToRange(2.4f, BrightnessRange), 2.4f);
			TestEqual(TEXT("Volumen 500"), ClampToRange(500.0f, VolumeRange), 100.0f);
			TestEqual(TEXT("FOV 10"), ClampToRange(10.0f, FieldOfViewRange), 70.0f);
			TestEqual(TEXT("Sensibilidad 0"), ClampToRange(0.0f, SensitivityRange), 0.1f);
		});

		It("un valor no finito vuelve al valor por defecto", [this]()
		{
			const float NaN = std::numeric_limits<float>::quiet_NaN();
			const float Inf = std::numeric_limits<float>::infinity();
			TestEqual(TEXT("FOV NaN"), ClampToRange(NaN, FieldOfViewRange), FieldOfViewRange.Default);
			TestEqual(TEXT("Volumen infinito"), ClampToRange(Inf, VolumeRange), VolumeRange.Default);
			TestEqual(TEXT("Slider NaN"), SliderToValue(NaN, SensitivityRange), SensitivityRange.Min);
		});

		It("la duración del día se ajusta a la soportada más cercana y nunca da índice -1", [this]()
		{
			TestEqual(TEXT("45 → 40"), SnapDayLength(45.0f), 40.0f);
			TestEqual(TEXT("85 → 90"), SnapDayLength(85.0f), 90.0f);
			TestEqual(TEXT("0 → 20"), SnapDayLength(0.0f), 20.0f);
			TestEqual(TEXT("1000 → 90"), SnapDayLength(1000.0f), 90.0f);
			TestEqual(TEXT("NaN → por defecto"), SnapDayLength(std::numeric_limits<float>::quiet_NaN()), DefaultDayLengthMinutes);
			TestEqual(TEXT("Índice de 45"), DayLengthIndex(45.0f), 1);
			TestTrue(TEXT("La duración por defecto está soportada"), GetSupportedDayLengths().Contains(DefaultDayLengthMinutes));
		});

		It("los enumerados fuera de rango vuelven al valor por defecto", [this]()
		{
			TestEqual(TEXT("Válido"), SanitizeEnumIndex(2, 3, 1), 2);
			TestEqual(TEXT("Demasiado grande"), SanitizeEnumIndex(3, 3, 1), 1);
			TestEqual(TEXT("Negativo"), SanitizeEnumIndex(-1, 4, 0), 0);
		});
	});

	Describe("remapeo (M12)", [this]()
	{
		It("la tabla no tiene acciones ni teclas por defecto repetidas ni reservadas", [this]()
		{
			const TArray<FRemappableAction>& Actions = GetRemappableActions();
			TestTrue(TEXT("Hay acciones"), Actions.Num() >= 9);
			for (int32 I = 0; I < Actions.Num(); ++I)
			{
				TestFalse(TEXT("Tecla por defecto no reservada"), IsReservedKey(Actions[I].DefaultKey));
				for (int32 J = I + 1; J < Actions.Num(); ++J)
				{
					TestNotEqual(TEXT("Acción única"), Actions[I].ActionName, Actions[J].ActionName);
					TestNotEqual(TEXT("Tecla por defecto única"), Actions[I].DefaultKey, Actions[J].DefaultKey);
				}
			}
			TestEqual(TEXT("Tecla por defecto de interactuar"), GetDefaultKeyFor(FName(TEXT("IA_Interact"))), FName(TEXT("E")));
			TestTrue(TEXT("Acción desconocida"), GetDefaultKeyFor(FName(TEXT("IA_NoExiste"))).IsNone());
		});

		It("detecta conflictos con cualquier acción, no solo entre saltar y correr", [this]()
		{
			const TArray<FKeyBinding> Bindings = DefaultBindings();
			const FName Jump(TEXT("IA_Jump"));
			const TCHAR* Taken[][2] = {
				{ TEXT("E"), TEXT("IA_Interact") },
				{ TEXT("C"), TEXT("IA_Combine") },
				{ TEXT("G"), TEXT("IA_Drop") },
				{ TEXT("Tab"), TEXT("IA_ToggleBackpack") },
				{ TEXT("LeftMouseButton"), TEXT("IA_UsePrimary") },
			};
			for (const auto& Pair : Taken)
			{
				const FRemapCheckResult Check = CheckRemap(Bindings, Jump, FName(Pair[0]));
				TestEqual(TEXT("En uso"), Check.Result, ERemapCheck::InUse);
				TestEqual(TEXT("Acción en conflicto"), Check.ConflictingAction, FName(Pair[1]));
			}
		});

		It("acepta una tecla libre y la tecla que la acción ya tiene", [this]()
		{
			const TArray<FKeyBinding> Bindings = DefaultBindings();
			TestEqual(TEXT("K libre"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("K"))).Result, ERemapCheck::Ok);
			TestEqual(TEXT("Misma tecla"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("SpaceBar"))).Result, ERemapCheck::Ok);
			TestEqual(TEXT("Botón lateral del ratón"), CheckRemap(Bindings, FName(TEXT("IA_Interact")), FName(TEXT("ThumbMouseButton"))).Result, ERemapCheck::Ok);
		});

		It("rechaza teclas reservadas y vacías", [this]()
		{
			const TArray<FKeyBinding> Bindings = DefaultBindings();
			TestEqual(TEXT("W es de movimiento"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("W"))).Result, ERemapCheck::ReservedKey);
			TestEqual(TEXT("Escape es del menú"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("Escape"))).Result, ERemapCheck::ReservedKey);
			TestEqual(TEXT("F es de la caña"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("F"))).Result, ERemapCheck::ReservedKey);
			TestEqual(TEXT("B es de construcción y del barco"), CheckRemap(Bindings, FName(TEXT("IA_Interact")), FName(TEXT("B"))).Result, ERemapCheck::ReservedKey);
			TestEqual(TEXT("Sin tecla"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName()).Result, ERemapCheck::InvalidKey);
		});

		It("tras un intercambio manual el conflicto sigue a la tecla efectiva", [this]()
		{
			TArray<FKeyBinding> Bindings = DefaultBindings();
			for (FKeyBinding& Binding : Bindings)
			{
				if (Binding.ActionName == FName(TEXT("IA_Interact")))
				{
					Binding.Key = FName(TEXT("K"));
				}
			}
			TestEqual(TEXT("E ya está libre"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("E"))).Result, ERemapCheck::Ok);
			TestEqual(TEXT("K la tiene interactuar"), CheckRemap(Bindings, FName(TEXT("IA_Jump")), FName(TEXT("K"))).Result, ERemapCheck::InUse);
		});
	});

	Describe("navegación con Volver y Escape (H6)", [this]()
	{
		It("Escape jugando abre la pausa y en la pausa reanuda", [this]()
		{
			TestEqual(TEXT("Jugando"), ScreenAfterEscape(EMenuScreen::None, false), EMenuScreen::Pause);
			TestEqual(TEXT("En pausa"), ScreenAfterEscape(EMenuScreen::Pause, false), EMenuScreen::None);
		});

		It("Ajustes vuelve a donde se abrió", [this]()
		{
			TestEqual(TEXT("Desde la pausa"), ScreenAfterEscape(EMenuScreen::Settings, true), EMenuScreen::Pause);
			TestEqual(TEXT("Desde el menú principal"), ScreenAfterEscape(EMenuScreen::Settings, false), EMenuScreen::MainMenu);
		});

		It("créditos y selección de modo vuelven al menú principal, que no hace nada", [this]()
		{
			TestEqual(TEXT("Créditos"), ScreenAfterBack(EMenuScreen::Credits, false), EMenuScreen::MainMenu);
			TestEqual(TEXT("Selección de modo"), ScreenAfterBack(EMenuScreen::ModeSelect, true), EMenuScreen::MainMenu);
			TestEqual(TEXT("Menú principal"), ScreenAfterEscape(EMenuScreen::MainMenu, false), EMenuScreen::MainMenu);
			TestEqual(TEXT("Volver jugando no hace nada"), ScreenAfterBack(EMenuScreen::None, false), EMenuScreen::None);
		});

		It("las pantallas de P-UI2 vuelven a donde se abrieron por defecto", [this]()
		{
			TestEqual(TEXT("Mapa jugando vuelve al juego"), ScreenAfterBack(EMenuScreen::Map, false), EMenuScreen::None);
			TestEqual(TEXT("Mapa desde la pausa"), ScreenAfterBack(EMenuScreen::Map, true), EMenuScreen::Pause);
			TestEqual(TEXT("Museo desde el mapa"), ScreenAfterBack(EMenuScreen::Museum, false), EMenuScreen::Map);
			TestEqual(TEXT("Museo desde la pausa"), ScreenAfterBack(EMenuScreen::Museum, true), EMenuScreen::Pause);
			TestEqual(TEXT("Logros desde el menú"), ScreenAfterEscape(EMenuScreen::Achievements, false), EMenuScreen::MainMenu);
			TestEqual(TEXT("Logros desde la pausa"), ScreenAfterEscape(EMenuScreen::Achievements, true), EMenuScreen::Pause);
			TestEqual(TEXT("Cargar vuelve al menú"), ScreenAfterBack(EMenuScreen::SaveSlots, false), EMenuScreen::MainMenu);
			TestEqual(TEXT("Guardar vuelve a la pausa"), ScreenAfterBack(EMenuScreen::SaveSlots, true), EMenuScreen::Pause);
		});

		It("solo existen las transiciones de los botones y teclas del frontend", [this]()
		{
			TestTrue(TEXT("Jugando: mapa"), CanOpenScreenFrom(EMenuScreen::None, EMenuScreen::Map));
			TestFalse(TEXT("Jugando: museo no"), CanOpenScreenFrom(EMenuScreen::None, EMenuScreen::Museum));
			TestTrue(TEXT("Mapa: museo"), CanOpenScreenFrom(EMenuScreen::Map, EMenuScreen::Museum));
			TestFalse(TEXT("Mapa: ajustes no"), CanOpenScreenFrom(EMenuScreen::Map, EMenuScreen::Settings));
			TestTrue(TEXT("Pausa: mapa, museo, logros y guardar"), CanOpenScreenFrom(EMenuScreen::Pause, EMenuScreen::Map)
				&& CanOpenScreenFrom(EMenuScreen::Pause, EMenuScreen::Museum) && CanOpenScreenFrom(EMenuScreen::Pause, EMenuScreen::Achievements)
				&& CanOpenScreenFrom(EMenuScreen::Pause, EMenuScreen::SaveSlots));
			TestTrue(TEXT("Menú: logros y cargar"), CanOpenScreenFrom(EMenuScreen::MainMenu, EMenuScreen::Achievements)
				&& CanOpenScreenFrom(EMenuScreen::MainMenu, EMenuScreen::SaveSlots));
			TestFalse(TEXT("Menú: mapa no"), CanOpenScreenFrom(EMenuScreen::MainMenu, EMenuScreen::Map));
			TestFalse(TEXT("Menú: museo no (sin partida)"), CanOpenScreenFrom(EMenuScreen::MainMenu, EMenuScreen::Museum));
		});

		It("la pila recuerda el camino: juego → mapa → museo → mapa → juego", [this]()
		{
			FMenuNavigation Nav;
			Nav.Reset(EMenuScreen::None);
			TestEqual(TEXT("Escape jugando abre la pausa"), Nav.EscapeTarget(), EMenuScreen::Pause);
			TestTrue(TEXT("Abre el mapa"), Nav.Open(EMenuScreen::Map));
			TestTrue(TEXT("Abre el museo"), Nav.Open(EMenuScreen::Museum));
			TestEqual(TEXT("Profundidad"), Nav.Depth(), 3);
			TestEqual(TEXT("Volver del museo va al mapa"), Nav.BackTarget(), EMenuScreen::Map);
			TestTrue(TEXT("Vuelve al mapa"), Nav.Open(Nav.BackTarget()));
			TestEqual(TEXT("Recorta la pila"), Nav.Depth(), 2);
			TestEqual(TEXT("Volver del mapa va al juego"), Nav.EscapeTarget(), EMenuScreen::None);
		});

		It("la pila recuerda el camino: pausa → mapa → museo vuelve a la pausa", [this]()
		{
			FMenuNavigation Nav;
			Nav.Reset(EMenuScreen::None);
			TestTrue(TEXT("Pausa"), Nav.Open(EMenuScreen::Pause));
			TestTrue(TEXT("Mapa"), Nav.Open(EMenuScreen::Map));
			TestTrue(TEXT("Museo"), Nav.Open(EMenuScreen::Museum));
			TestTrue(TEXT("Se abrió desde la pausa"), Nav.IsInStack(EMenuScreen::Pause));
			TestTrue(TEXT("Salta a la pausa"), Nav.Open(EMenuScreen::Pause));
			TestEqual(TEXT("Queda juego → pausa"), Nav.Depth(), 2);
			TestEqual(TEXT("Volver de la pausa reanuda"), Nav.BackTarget(), EMenuScreen::None);
		});

		It("la pila rechaza transiciones inexistentes y en la raíz Volver no hace nada", [this]()
		{
			FMenuNavigation Nav;
			Nav.Reset(EMenuScreen::MainMenu);
			TestFalse(TEXT("Menú → mapa"), Nav.Open(EMenuScreen::Map));
			TestEqual(TEXT("Sin cambios"), Nav.Current(), EMenuScreen::MainMenu);
			TestEqual(TEXT("Volver en el menú"), Nav.BackTarget(), EMenuScreen::MainMenu);
			TestTrue(TEXT("Abrir la actual no cambia nada"), Nav.Open(EMenuScreen::MainMenu));
			TestTrue(TEXT("Logros"), Nav.Open(EMenuScreen::Achievements));
			TestEqual(TEXT("Volver de logros"), Nav.BackTarget(), EMenuScreen::MainMenu);
			TestFalse(TEXT("Logros → ajustes no"), Nav.Open(EMenuScreen::Settings));
		});

		It("M y View sacan el mapa y M queda reservada para él", [this]()
		{
			TestTrue(TEXT("M"), IsMapToggleKey(FName(TEXT("M"))));
			TestTrue(TEXT("View"), IsMapToggleKey(FName(TEXT("Gamepad_Special_Left"))));
			TestFalse(TEXT("Start no"), IsMapToggleKey(FName(TEXT("Gamepad_Special_Right"))));
			TestTrue(TEXT("M reservada"), IsReservedKey(FName(TEXT("M"))));
			TestEqual(TEXT("No se puede remapear a M"), CheckRemap(DefaultBindings(), FName(TEXT("IA_Jump")), FName(TEXT("M"))).Result, ERemapCheck::ReservedKey);
		});

		It("reconoce las teclas de volver y de pausa de teclado y mando", [this]()
		{
			TestTrue(TEXT("Escape vuelve"), IsMenuBackKey(FName(TEXT("Escape"))));
			TestTrue(TEXT("B del mando vuelve"), IsMenuBackKey(FName(TEXT("Gamepad_FaceButton_Right"))));
			TestFalse(TEXT("A del mando no vuelve"), IsMenuBackKey(FName(TEXT("Gamepad_FaceButton_Bottom"))));
			TestTrue(TEXT("Start pausa"), IsPauseToggleKey(FName(TEXT("Gamepad_Special_Right"))));
			TestTrue(TEXT("Escape pausa"), IsPauseToggleKey(FName(TEXT("Escape"))));
			TestFalse(TEXT("B no pausa"), IsPauseToggleKey(FName(TEXT("Gamepad_FaceButton_Right"))));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
