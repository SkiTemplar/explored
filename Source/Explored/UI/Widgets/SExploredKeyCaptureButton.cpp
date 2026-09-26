#include "UI/Widgets/SExploredKeyCaptureButton.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

void SExploredKeyCaptureButton::Construct(const FArguments& InArgs)
{
	CurrentKey = InArgs._CurrentKey;
	OnKeyPicked = InArgs._OnKeyPicked;

	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle())
		.OnClicked(this, &SExploredKeyCaptureButton::HandleClicked)
		[
			SNew(STextBlock)
			.Text(this, &SExploredKeyCaptureButton::GetLabel)
			.Font(Style.FontBody())
			.ColorAndOpacity(FSlateColor(Style.ColorInk()))
		]
	];
}

FText SExploredKeyCaptureButton::GetLabel() const
{
	if (bCapturing)
	{
		return NSLOCTEXT("ExploredUI", "PressAnyKey", "Pulsa una tecla...");
	}
	const FKey Key = CurrentKey.Get(EKeys::Invalid);
	return Key.IsValid() ? Key.GetDisplayName() : NSLOCTEXT("ExploredUI", "NoKey", "(sin asignar)");
}

FReply SExploredKeyCaptureButton::HandleClicked()
{
	bCapturing = true;
	// El clic que abre la captura llega en el mouse-up del SButton; al pedir
	// aquí la captura del ratón, el SButton no la suelta (respeta el nuevo
	// captor) y el siguiente clic, esté donde esté el cursor, llega a este widget.
	return FReply::Handled()
		.SetUserFocus(SharedThis(this), EFocusCause::SetDirectly)
		.CaptureMouse(SharedThis(this));
}

FReply SExploredKeyCaptureButton::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled();
}

void SExploredKeyCaptureButton::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	bCapturing = false;
	if (HasMouseCapture() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
}

void SExploredKeyCaptureButton::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SCompoundWidget::OnMouseCaptureLost(CaptureLostEvent);
	bCapturing = false;
}

FReply SExploredKeyCaptureButton::ReportKey(FKey Key)
{
	bCapturing = false;
	if (Key.IsValid() && !ExploredSettingsLogic::IsMenuBackKey(Key.GetFName()))
	{
		OnKeyPicked.ExecuteIfBound(Key);
	}
	FReply Reply = FReply::Handled();
	if (HasMouseCapture())
	{
		Reply.ReleaseMouseCapture();
	}
	return Reply;
}

FReply SExploredKeyCaptureButton::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (!bCapturing)
	{
		return FReply::Unhandled();
	}
	return ReportKey(KeyEvent.GetKey());
}

FReply SExploredKeyCaptureButton::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bCapturing)
	{
		return FReply::Unhandled();
	}
	return ReportKey(MouseEvent.GetEffectingButton());
}

FReply SExploredKeyCaptureButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bCapturing)
	{
		return ReportKey(MouseEvent.GetEffectingButton());
	}
	// Captura huérfana (p. ej. se perdió el foco sin evento de captura): se suelta.
	if (HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}
