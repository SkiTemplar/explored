#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnExploredKeyPicked, FKey);

/**
 * Botón de remapeo (Ajustes > Controles): muestra la tecla actual y, al
 * pulsarlo, espera a que el jugador pulse la tecla, botón de ratón o
 * gatillo/botón de mando siguiente y la reporta por OnKeyPicked. Escape
 * cancela la captura sin cambiar nada.
 */
class EXPLORED_API SExploredKeyCaptureButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredKeyCaptureButton) {}
		SLATE_ATTRIBUTE(FKey, CurrentKey)
		SLATE_EVENT(FOnExploredKeyPicked, OnKeyPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

private:
	FReply HandleClicked();
	FText GetLabel() const;
	void ReportKey(FKey Key);

	TAttribute<FKey> CurrentKey;
	FOnExploredKeyPicked OnKeyPicked;
	bool bCapturing = false;
};
