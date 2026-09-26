#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"

class UExploredGameUserSettings;
class UExploredInputSettingsSubsystem;
class SWidgetSwitcher;
class SBox;

/**
 * Panel de Ajustes (GDD §10), con pestañas Gráficos / Audio / Controles /
 * Juego / Accesibilidad. Lee y escribe directamente sobre
 * UExploredGameUserSettings (y sobre UExploredInputSettingsSubsystem para el
 * remapeo), así que la UI siempre refleja el estado real: «Restaurar
 * valores por defecto» solo necesita llamar a SetToDefaults() y refrescar.
 *
 * Se abre igual desde el menú principal que desde la pausa (AExploredPlayerController
 * decide el «Volver» de cada caso).
 *
 * «Volver» (botón, Escape o B del mando) descarta lo no aplicado: recarga el
 * ini y re-aplica, para que brillo, idioma y daltonismo, que se previsualizan
 * al instante, no se queden activos sin guardar (M11).
 */
class EXPLORED_API SExploredSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredSettingsPanel)
		: _Settings(nullptr)
		, _InputSettings(nullptr)
		, _WorldContextObject(nullptr)
	{}
		SLATE_ARGUMENT(UExploredGameUserSettings*, Settings)
		SLATE_ARGUMENT(UExploredInputSettingsSubsystem*, InputSettings)
		SLATE_ARGUMENT(const UObject*, WorldContextObject)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Widget que recibe el foco al abrir el panel (pestaña activa), para navegar con teclado y mando. */
	TSharedPtr<SWidget> GetInitialFocus() const;

	/** Igual que pulsar «Volver»: descarta lo no aplicado y avisa a OnBack. */
	void RequestBack();

private:
	enum class ETab : uint8 { Graphics, Audio, Controls, Game, Accessibility };

	TSharedRef<SWidget> BuildTabBar();
	TSharedRef<SWidget> BuildGraphicsTab();
	TSharedRef<SWidget> BuildAudioTab();
	TSharedRef<SWidget> BuildControlsTab();
	TSharedRef<SWidget> BuildGameTab();
	TSharedRef<SWidget> BuildAccessibilityTab();
	TSharedRef<SWidget> BuildFooter();

	FReply SelectTab(ETab Tab);
	FReply HandleApply();
	FReply HandleRestoreDefaults();
	FReply HandleBack();
	/** Payload de FOnExploredKeyPicked::CreateSP: la tecla primero y luego la acción. */
	void HandleKeyPicked(FKey NewKey, FName ActionName);

	/** Ajustes del motor: viven lo que GEngine, pero se guardan débiles por si el panel sobrevive. */
	TWeakObjectPtr<UExploredGameUserSettings> Settings;
	/** Subsistema del LocalPlayer: puede desaparecer antes que el widget. */
	TWeakObjectPtr<UExploredInputSettingsSubsystem> InputSettings;
	TWeakObjectPtr<const UObject> WorldContextObject;
	FSimpleDelegate OnBack;

	ETab CurrentTab = ETab::Graphics;
	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SBox> TabBarContainer;
	/** Botones de pestaña de la barra actual, en el orden de ETab (se rehacen al cambiar de pestaña). */
	TArray<TSharedPtr<SWidget>> TabButtons;
	/** Motivo del último remapeo rechazado (vacío si el último fue bien). */
	FText RemapMessage;
};
