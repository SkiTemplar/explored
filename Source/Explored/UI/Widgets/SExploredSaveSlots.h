#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "UI/ScreensLogic.h"

class SBox;
class UExploredSaveSubsystem;

/** Se eligió una ranura para cargarla (id canónico: «auto», «manual1»…). */
DECLARE_DELEGATE_OneParam(FOnExploredSlotChosen, const FString& /*SlotId*/);

/**
 * Selector de ranura (GDD §15: 3 ranuras manuales, autoguardado y copia de
 * seguridad). Siempre enseña las cuatro ranuras (automática y manuales 1–3)
 * con fecha, tiempo jugado y estado: vacía, recuperada de la copia, dañada o
 * de una versión más nueva, y si tiene copia «.bak».
 *
 * - Guardar (pausa → «Guardar»): solo las manuales; pisar una con partida
 *   pide confirmación (con el foco en «Cancelar»). Guarda con
 *   UExploredSaveSubsystem::RequestSave, que enciende el indicador de guardado.
 * - Cargar (menú principal → «Cargar»): solo las legibles; OnLoadSlot hace el
 *   resto (el PlayerController carga y funde a jugar).
 *
 * «Volver», Escape o B: en la confirmación, cancelan; si no, vuelven a donde
 * se abrió. Las reglas están en ExploredScreens::BuildSaveSlotRows (ScreensLogicSpec).
 */
class EXPLORED_API SExploredSaveSlots : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredSaveSlots)
		: _Mode(ExploredScreens::ESaveSlotsMode::Save)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UExploredSaveSubsystem>, SaveSubsystem)
		SLATE_ARGUMENT(ExploredScreens::ESaveSlotsMode, Mode)
		SLATE_EVENT(FOnExploredSlotChosen, OnLoadSlot)
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Primera ranura que se puede elegir (o «Volver»). */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	void RefreshRows();
	TSharedRef<SWidget> BuildRow(const ExploredScreens::FSaveSlotRow& Row);
	TSharedRef<SWidget> BuildConfirm();
	void FocusWidget(const TSharedPtr<SWidget>& Widget) const;
	void SaveInto(const FString& SlotId);

	FReply HandleRowClicked(FString SlotId);
	FReply HandleConfirmOverwrite();
	FReply HandleCancelOverwrite();
	FReply HandleBack();

	static FText SlotTitle(const ExploredScreens::FSaveSlotRow& Row);
	static FText SlotDetail(const ExploredScreens::FSaveSlotRow& Row);

	TWeakObjectPtr<UExploredSaveSubsystem> SaveSubsystem;
	ExploredScreens::ESaveSlotsMode Mode = ExploredScreens::ESaveSlotsMode::Save;
	FOnExploredSlotChosen OnLoadSlot;
	FSimpleDelegate OnBack;

	TArray<ExploredScreens::FSaveSlotRow> Rows;
	/** Ranura que espera confirmación para sobrescribirse (vacío si ninguna). */
	FString PendingOverwrite;
	FText StatusText;

	TSharedPtr<SBox> ListContainer;
	TSharedPtr<SWidget> InitialFocus;
	TSharedPtr<SWidget> BackButton;
	TSharedPtr<SWidget> CancelButton;
	/** Botón de cada ranura de la lista actual, para devolverle el foco tras guardar o cancelar. */
	TMap<FString, TSharedPtr<SWidget>> RowWidgets;
};
