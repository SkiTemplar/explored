#include "UI/Widgets/SExploredSaveSlots.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/DateTime.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Save/SaveSlots.h"
#include "UI/ExploredSaveSubsystem.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"

void SExploredSaveSlots::Construct(const FArguments& InArgs)
{
	SaveSubsystem = InArgs._SaveSubsystem;
	Mode = InArgs._Mode;
	OnLoadSlot = InArgs._OnLoadSlot;
	OnBack = InArgs._OnBack;

	const bool bSaving = Mode == ExploredScreens::ESaveSlotsMode::Save;
	StatusText = bSaving
		? NSLOCTEXT("ExploredUI", "SlotsSaveHint", "Elige una ranura manual. La automática se guarda al dormir y en las hogueras.")
		: NSLOCTEXT("ExploredUI", "SlotsLoadHint", "Elige la partida que quieres cargar.");

	BackButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Back", "Volver"),
		FOnClicked::CreateSP(this, &SExploredSaveSlots::HandleBack), false);

	const TWeakPtr<SExploredSaveSlots> WeakSelf = SharedThis(this);
	TSharedRef<SWidget> Body = SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([WeakSelf]()
		{
			const TSharedPtr<SExploredSaveSlots> Pinned = WeakSelf.Pin();
			return Pinned.IsValid() && !Pinned->PendingOverwrite.IsEmpty() ? 1 : 0;
		})
		+ SWidgetSwitcher::Slot()
		[
			SNew(SScrollBox)
			.ScrollBarStyle(&FExploredUIStyle::Get().ScrollBarStyle())
			.ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
			+ SScrollBox::Slot()
			[
				SAssignNew(ListContainer, SBox)
			]
		]
		+ SWidgetSwitcher::Slot()
		[
			BuildConfirm()
		];

	const TAttribute<FText> Subtitle = TAttribute<FText>::CreateLambda([WeakSelf]()
	{
		const TSharedPtr<SExploredSaveSlots> Pinned = WeakSelf.Pin();
		return Pinned.IsValid() ? Pinned->StatusText : FText::GetEmpty();
	});

	ChildSlot
	[
		ExploredUIWidgets::MakeScreenPanel(
			bSaving ? NSLOCTEXT("ExploredUI", "SlotsSaveTitle", "Guardar partida") : NSLOCTEXT("ExploredUI", "SlotsLoadTitle", "Cargar partida"),
			Subtitle, Body, SNew(SBox).HAlign(HAlign_Center) [ BackButton.ToSharedRef() ], 620.0f, 460.0f)
	];

	RefreshRows();
}

void SExploredSaveSlots::RefreshRows()
{
	const UExploredSaveSubsystem* Subsystem = SaveSubsystem.Get();
	Rows = Subsystem
		? ExploredScreens::BuildSaveSlotRows(Subsystem->ListSlots(), Subsystem->GetSlotsWithBackup(), Mode)
		: TArray<ExploredScreens::FSaveSlotRow>();

	RowWidgets.Reset();
	InitialFocus = BackButton;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const ExploredScreens::FSaveSlotRow& Row : Rows)
	{
		TSharedRef<SWidget> RowWidget = BuildRow(Row);
		RowWidgets.Add(Row.SlotId, RowWidget);
		if (Row.bSelectable && InitialFocus == BackButton)
		{
			InitialFocus = RowWidget;
		}
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 3.0f)) [ RowWidget ];
	}
	if (Rows.Num() == 0)
	{
		List->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExploredUI", "SlotsUnavailable", "El guardado no está disponible."))
			.Font(FExploredUIStyle::Get().FontBody())
			.ColorAndOpacity(FSlateColor(FExploredUIStyle::Get().ColorInkDim()))
		];
	}
	if (ListContainer.IsValid())
	{
		ListContainer->SetContent(List);
	}
}

FText SExploredSaveSlots::SlotTitle(const ExploredScreens::FSaveSlotRow& Row)
{
	return Row.bIsAuto
		? NSLOCTEXT("ExploredUI", "SlotAuto", "Autoguardado")
		: FText::Format(NSLOCTEXT("ExploredUI", "SlotManual", "Ranura {0}"), FText::AsNumber(Row.ManualIndex));
}

FText SExploredSaveSlots::SlotDetail(const ExploredScreens::FSaveSlotRow& Row)
{
	using ExploredScreens::ESaveSlotRowState;
	switch (Row.State)
	{
	case ESaveSlotRowState::Empty:
		return NSLOCTEXT("ExploredUI", "SlotEmpty", "Vacía");
	case ESaveSlotRowState::Damaged:
		return NSLOCTEXT("ExploredUI", "SlotDamaged", "Dañada: no se puede leer");
	case ESaveSlotRowState::FutureVersion:
		return NSLOCTEXT("ExploredUI", "SlotFuture", "Guardada con una versión más nueva del juego");
	default:
		break;
	}
	// Fecha local: la cabecera guarda segundos Unix en UTC.
	const FDateTime Utc = FDateTime::FromUnixTimestamp(Row.Header.TimestampUnix);
	const FDateTime Local = Utc + (FDateTime::Now() - FDateTime::UtcNow());
	const ExploredScreens::FPlayTime PlayTime = ExploredScreens::SplitPlayTime(Row.Header.PlayTimeSeconds);
	const FText Saved = FText::Format(NSLOCTEXT("ExploredUI", "SlotSavedAt", "{0} · {1} h {2} min jugados"),
		FText::AsDateTime(Local, EDateTimeStyle::Medium, EDateTimeStyle::Short, FText::GetInvariantTimeZone()),
		FText::AsNumber(PlayTime.Hours), FText::AsNumber(PlayTime.Minutes));
	if (Row.State == ESaveSlotRowState::Recovered)
	{
		return FText::Format(NSLOCTEXT("ExploredUI", "SlotRecovered", "{0} · recuperada de la copia de seguridad (la principal estaba dañada)"), Saved);
	}
	return Saved;
}

TSharedRef<SWidget> SExploredSaveSlots::BuildRow(const ExploredScreens::FSaveSlotRow& Row)
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	using ExploredScreens::ESaveSlotRowState;
	const bool bProblem = Row.State == ESaveSlotRowState::Damaged || Row.State == ESaveSlotRowState::FutureVersion
		|| Row.State == ESaveSlotRowState::Recovered;
	const FText Backup = Row.State == ESaveSlotRowState::Empty && !Row.bHasBackup
		? FText::GetEmpty()
		: (Row.bHasBackup ? NSLOCTEXT("ExploredUI", "SlotHasBackup", "Con copia de seguridad") : NSLOCTEXT("ExploredUI", "SlotNoBackup", "Sin copia de seguridad"));

	return SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle())
		.IsEnabled(Row.bSelectable)
		.OnClicked(this, &SExploredSaveSlots::HandleRowClicked, Row.SlotId)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(SlotTitle(Row))
				.Font(Style.FontBody())
				.ColorAndOpacity(FSlateColor(Row.bSelectable ? Style.ColorInk() : Style.ColorInkDim()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(SlotDetail(Row))
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(bProblem ? Style.ColorWarning() : Style.ColorInkDim()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Backup)
				.Font(Style.FontSmall())
				.ColorAndOpacity(FSlateColor(Style.ColorInkDim()))
			]
		];
}

TSharedRef<SWidget> SExploredSaveSlots::BuildConfirm()
{
	const FExploredUIStyle& Style = FExploredUIStyle::Get();
	const TWeakPtr<SExploredSaveSlots> WeakSelf = SharedThis(this);

	CancelButton = ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "SlotsCancel", "Cancelar"),
		FOnClicked::CreateSP(this, &SExploredSaveSlots::HandleCancelOverwrite), false);

	return SNew(SBox)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 20.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakSelf]()
				{
					const TSharedPtr<SExploredSaveSlots> Pinned = WeakSelf.Pin();
					if (!Pinned.IsValid())
					{
						return FText::GetEmpty();
					}
					const ExploredScreens::FSaveSlotRow* Row = Pinned->Rows.FindByPredicate(
						[&Pinned](const ExploredScreens::FSaveSlotRow& Candidate) { return Candidate.SlotId == Pinned->PendingOverwrite; });
					return Row
						? FText::Format(NSLOCTEXT("ExploredUI", "SlotsOverwrite", "¿Sobrescribir la {0}? La partida anterior queda como copia de seguridad."), SlotTitle(*Row))
						: FText::GetEmpty();
				})
				.Font(Style.FontBody())
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(Style.ColorInk()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 24.0f, 0.0f))
				[
					ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "SlotsConfirmOverwrite", "Sobrescribir"),
						FOnClicked::CreateSP(this, &SExploredSaveSlots::HandleConfirmOverwrite), false)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					CancelButton.ToSharedRef()
				]
			]
		];
}

void SExploredSaveSlots::FocusWidget(const TSharedPtr<SWidget>& Widget) const
{
	if (Widget.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocus(Widget, EFocusCause::SetDirectly);
	}
}

void SExploredSaveSlots::SaveInto(const FString& SlotId)
{
	UExploredSaveSubsystem* Subsystem = SaveSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}
	const int64 Before = FDateTime::UtcNow().ToUnixTimestamp();
	// RequestSave dispara OnSaveCompleted (indicador de guardado) u OnSaveFailed.
	Subsystem->RequestSave(FName(*SlotId));
	RefreshRows();

	const ExploredScreens::FSaveSlotRow* Row = Rows.FindByPredicate([&SlotId](const ExploredScreens::FSaveSlotRow& Candidate) { return Candidate.SlotId == SlotId; });
	const bool bSaved = Row && Row->State == ExploredScreens::ESaveSlotRowState::Ok && Row->Header.TimestampUnix >= Before - 1;
	StatusText = bSaved
		? FText::Format(NSLOCTEXT("ExploredUI", "SlotsSaved", "Partida guardada en la {0}."), SlotTitle(*Row))
		: NSLOCTEXT("ExploredUI", "SlotsSaveFailed", "No se pudo guardar la partida.");
	if (const TSharedPtr<SWidget>* RowWidget = RowWidgets.Find(SlotId))
	{
		FocusWidget(*RowWidget);
	}
}

FReply SExploredSaveSlots::HandleRowClicked(FString SlotId)
{
	const ExploredScreens::FSaveSlotRow* Row = Rows.FindByPredicate([&SlotId](const ExploredScreens::FSaveSlotRow& Candidate) { return Candidate.SlotId == SlotId; });
	if (!Row || !Row->bSelectable)
	{
		return FReply::Handled();
	}
	if (Mode == ExploredScreens::ESaveSlotsMode::Load)
	{
		OnLoadSlot.ExecuteIfBound(SlotId);
		return FReply::Handled();
	}
	if (Row->bNeedsOverwriteConfirm)
	{
		PendingOverwrite = SlotId;
		// Por seguridad el foco va a «Cancelar».
		FocusWidget(CancelButton);
		return FReply::Handled();
	}
	SaveInto(SlotId);
	return FReply::Handled();
}

FReply SExploredSaveSlots::HandleConfirmOverwrite()
{
	const FString SlotId = PendingOverwrite;
	PendingOverwrite.Reset();
	if (!SlotId.IsEmpty())
	{
		SaveInto(SlotId);
	}
	return FReply::Handled();
}

FReply SExploredSaveSlots::HandleCancelOverwrite()
{
	const FString SlotId = PendingOverwrite;
	PendingOverwrite.Reset();
	if (const TSharedPtr<SWidget>* RowWidget = RowWidgets.Find(SlotId))
	{
		FocusWidget(*RowWidget);
	}
	return FReply::Handled();
}

FReply SExploredSaveSlots::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (ExploredSettingsLogic::IsMenuBackKey(InKeyEvent.GetKey().GetFName()))
	{
		return PendingOverwrite.IsEmpty() ? HandleBack() : HandleCancelOverwrite();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SExploredSaveSlots::HandleBack()
{
	OnBack.ExecuteIfBound();
	return FReply::Handled();
}
