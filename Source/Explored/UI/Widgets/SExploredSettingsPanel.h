#pragma once

#include "CoreMinimal.h"
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
 */
class EXPLORED_API SExploredSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredSettingsPanel) {}
		SLATE_ARGUMENT(UExploredGameUserSettings*, Settings)
		SLATE_ARGUMENT(UExploredInputSettingsSubsystem*, InputSettings)
		SLATE_ARGUMENT(const UObject*, WorldContextObject)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

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

	UExploredGameUserSettings* Settings = nullptr;
	UExploredInputSettingsSubsystem* InputSettings = nullptr;
	const UObject* WorldContextObject = nullptr;
	FSimpleDelegate OnBack;

	ETab CurrentTab = ETab::Graphics;
	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SBox> TabBarContainer;
};
