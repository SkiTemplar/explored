#include "UI/Widgets/SExploredKeyCaptureButton.h"

#include "Framework/Application/SlateApplication.h"
#include "UI/ExploredUIStyle.h"
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
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::SetDirectly);
}

FReply SExploredKeyCaptureButton::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled();
}

void SExploredKeyCaptureButton::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	bCapturing = false;
}

void SExploredKeyCaptureButton::ReportKey(FKey Key)
{
	bCapturing = false;
	if (Key.IsValid() && Key != EKeys::Escape)
	{
		OnKeyPicked.ExecuteIfBound(Key);
	}
}

FReply SExploredKeyCaptureButton::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	if (!bCapturing)
	{
		return FReply::Unhandled();
	}
	ReportKey(KeyEvent.GetKey());
	return FReply::Handled();
}

FReply SExploredKeyCaptureButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bCapturing)
	{
		return FReply::Unhandled();
	}
	ReportKey(MouseEvent.GetEffectingButton());
	return FReply::Handled();
}
