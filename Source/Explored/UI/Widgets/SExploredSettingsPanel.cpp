#include "UI/Widgets/SExploredSettingsPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/ExploredUIStyle.h"
#include "UI/SettingsLogic.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "UI/Widgets/SExploredKeyCaptureButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// Espacio de nombres con nombre (no anónimo) para no chocar en el Unity build.
namespace ExploredSettingsPanelDetail
{
	const FExploredUIStyle& S() { return FExploredUIStyle::Get(); }

	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Control)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Label).Font(S().FontBody()).ColorAndOpacity(FSlateColor(S().ColorInk()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
			[
				Control
			];
	}

	TSharedRef<SWidget> MakeSectionTitle(const FText& Title)
	{
		return SNew(STextBlock)
			.Text(Title)
			.Font(S().FontHeading())
			.ColorAndOpacity(FSlateColor(S().ColorAccent()));
	}

	TSharedRef<SWidget> MakeToggle(TFunction<bool()> GetValue, TFunction<void(bool)> SetValue)
	{
		return SNew(SCheckBox)
			.Style(&S().CheckBoxStyle())
			.IsChecked_Lambda([GetValue]() { return GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([SetValue](ECheckBoxState NewState) { SetValue(NewState == ECheckBoxState::Checked); });
	}

	/** Slider continuo sobre un rango de ExploredSettingsLogic (la conversión es pura y está testeada). */
	TSharedRef<SWidget> MakeRangeSlider(const ExploredSettingsLogic::FSettingRange& Range, TFunction<float()> GetValue, TFunction<void(float)> SetValue)
	{
		return SNew(SBox).WidthOverride(220.0f)
			[
				SNew(SSlider).Style(&S().SliderStyle())
				.Value_Lambda([GetValue, Range]() { return ExploredSettingsLogic::ValueToSlider(GetValue(), Range); })
				.OnValueChanged_Lambda([SetValue, Range](float V) { SetValue(ExploredSettingsLogic::SliderToValue(V, Range)); })
			];
	}

	/** Selector de opción discreta con flechas «<  Texto  >», cómodo con mando y ratón. */
	TSharedRef<SWidget> MakeCycler(TArray<FText> Labels, TFunction<int32()> GetIndex, TFunction<void(int32)> SetIndex)
	{
		const int32 Count = FMath::Max(Labels.Num(), 1);
		TSharedRef<STextBlock> Text = SNew(STextBlock)
			.Font(S().FontBody())
			.ColorAndOpacity(FSlateColor(S().ColorInk()))
			.MinDesiredWidth(160.0f)
			.Justification(ETextJustify::Center)
			.Text_Lambda([GetIndex, Labels, Count]() { return Labels.IsValidIndex(((GetIndex() % Count) + Count) % Count) ? Labels[((GetIndex() % Count) + Count) % Count] : FText::GetEmpty(); });

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(&S().ButtonStyle())
				.Text(FText::FromString(TEXT("<")))
				.OnClicked_Lambda([GetIndex, SetIndex, Count]() { SetIndex(((GetIndex() - 1) % Count + Count) % Count); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.0f, 0.0f))
			[
				Text
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(&S().ButtonStyle())
				.Text(FText::FromString(TEXT(">")))
				.OnClicked_Lambda([GetIndex, SetIndex, Count]() { SetIndex(((GetIndex() + 1) % Count + Count) % Count); return FReply::Handled(); })
			];
	}

	/** Etiqueta de cada acción remapeable (ExploredSettingsLogic::GetRemappableActions). */
	FText ActionLabel(FName ActionName)
	{
		static const FName Jump(TEXT("IA_Jump"));
		static const FName Sprint(TEXT("IA_Sprint"));
		static const FName Dive(TEXT("IA_Dive"));
		static const FName Interact(TEXT("IA_Interact"));
		static const FName UsePrimary(TEXT("IA_UsePrimary"));
		static const FName UseSecondary(TEXT("IA_UseSecondary"));
		static const FName Drop(TEXT("IA_Drop"));
		static const FName Combine(TEXT("IA_Combine"));
		static const FName Backpack(TEXT("IA_ToggleBackpack"));

		if (ActionName == Jump) { return NSLOCTEXT("ExploredUI", "ActionJump", "Saltar"); }
		if (ActionName == Sprint) { return NSLOCTEXT("ExploredUI", "ActionSprint", "Correr"); }
		if (ActionName == Dive) { return NSLOCTEXT("ExploredUI", "ActionDive", "Agacharse / bucear"); }
		if (ActionName == Interact) { return NSLOCTEXT("ExploredUI", "ActionInteract", "Interactuar / coger"); }
		if (ActionName == UsePrimary) { return NSLOCTEXT("ExploredUI", "ActionUsePrimary", "Usar mano derecha"); }
		if (ActionName == UseSecondary) { return NSLOCTEXT("ExploredUI", "ActionUseSecondary", "Usar mano izquierda"); }
		if (ActionName == Drop) { return NSLOCTEXT("ExploredUI", "ActionDrop", "Soltar"); }
		if (ActionName == Combine) { return NSLOCTEXT("ExploredUI", "ActionCombine", "Combinar"); }
		if (ActionName == Backpack) { return NSLOCTEXT("ExploredUI", "ActionBackpack", "Mochila"); }
		return FText::FromName(ActionName);
	}

	TArray<FText> QualityLabels()
	{
		return {
			NSLOCTEXT("ExploredUI", "QualityLow", "Baja"),
			NSLOCTEXT("ExploredUI", "QualityMedium", "Media"),
			NSLOCTEXT("ExploredUI", "QualityHigh", "Alta"),
			NSLOCTEXT("ExploredUI", "QualityEpic", "Épica"),
			NSLOCTEXT("ExploredUI", "QualityCinematic", "Cinemática")
		};
	}
}

using namespace ExploredSettingsPanelDetail;

void SExploredSettingsPanel::Construct(const FArguments& InArgs)
{
	Settings = InArgs._Settings;
	InputSettings = InArgs._InputSettings;
	WorldContextObject = InArgs._WorldContextObject;
	OnBack = InArgs._OnBack;

	Switcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot() [ BuildGraphicsTab() ]
		+ SWidgetSwitcher::Slot() [ BuildAudioTab() ]
		+ SWidgetSwitcher::Slot() [ BuildControlsTab() ]
		+ SWidgetSwitcher::Slot() [ BuildGameTab() ]
		+ SWidgetSwitcher::Slot() [ BuildAccessibilityTab() ];

	TabBarContainer = SNew(SBox) [ BuildTabBar() ];

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(S().BrushPanel())
			.Padding(FMargin(36.0f, 28.0f))
			[
				SNew(SBox)
				.WidthOverride(760.0f)
				.HeightOverride(560.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
					[
						TabBarContainer.ToSharedRef()
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SScrollBox)
						.ScrollBarStyle(&S().ScrollBarStyle())
						+ SScrollBox::Slot() [ Switcher.ToSharedRef() ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
					[
						BuildFooter()
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildTabBar()
{
	TabButtons.Reset();
	auto MakeTab = [this](ETab Tab, const FText& Label)
	{
		TSharedRef<SWidget> Button = ExploredUIWidgets::MakeTabButton(Label, CurrentTab == Tab, FOnClicked::CreateSP(this, &SExploredSettingsPanel::SelectTab, Tab));
		TabButtons.Add(Button);
		return SNew(SBox).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			Button
		];
	};

	// Se construyen en el orden de ETab: TabButtons[Tab] es el botón de esa pestaña.
	TSharedRef<SWidget> Graphics = MakeTab(ETab::Graphics, NSLOCTEXT("ExploredUI", "TabGraphics", "Gráficos"));
	TSharedRef<SWidget> Audio = MakeTab(ETab::Audio, NSLOCTEXT("ExploredUI", "TabAudio", "Audio"));
	TSharedRef<SWidget> Controls = MakeTab(ETab::Controls, NSLOCTEXT("ExploredUI", "TabControls", "Controles"));
	TSharedRef<SWidget> Game = MakeTab(ETab::Game, NSLOCTEXT("ExploredUI", "TabGame", "Juego"));
	TSharedRef<SWidget> Accessibility = MakeTab(ETab::Accessibility, NSLOCTEXT("ExploredUI", "TabAccessibility", "Accesibilidad"));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth() [ Graphics ]
		+ SHorizontalBox::Slot().AutoWidth() [ Audio ]
		+ SHorizontalBox::Slot().AutoWidth() [ Controls ]
		+ SHorizontalBox::Slot().AutoWidth() [ Game ]
		+ SHorizontalBox::Slot().AutoWidth() [ Accessibility ];
}

TSharedPtr<SWidget> SExploredSettingsPanel::GetInitialFocus() const
{
	const int32 Index = static_cast<int32>(CurrentTab);
	return TabButtons.IsValidIndex(Index) ? TabButtons[Index] : TSharedPtr<SWidget>();
}

FReply SExploredSettingsPanel::SelectTab(ETab Tab)
{
	CurrentTab = Tab;
	Switcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	// Se reconstruye entera porque el estilo activo/inactivo de cada botón se fija al crearlo.
	TabBarContainer->SetContent(BuildTabBar());
	// El botón que tenía el foco acaba de destruirse: sin devolver el foco a la
	// pestaña nueva, la navegación con teclado/mando (y Escape) se quedaría sin destino.
	const TSharedPtr<SWidget> Focus = GetInitialFocus();
	return Focus.IsValid() ? FReply::Handled().SetUserFocus(Focus.ToSharedRef(), EFocusCause::Navigation) : FReply::Handled();
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildGraphicsTab()
{
	// Los ajustes viven lo que GEngine (más que cualquier widget): las lambdas
	// de los controles pueden capturar el puntero crudo.
	UExploredGameUserSettings* GS = Settings.Get();
	if (!GS)
	{
		return SNullWidget::NullWidget;
	}

	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.Num() == 0)
	{
		Resolutions.Add(FIntPoint(1920, 1080));
	}
	TArray<FText> ResolutionLabels;
	for (const FIntPoint& R : Resolutions)
	{
		ResolutionLabels.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));
	}

	TArray<FText> WindowModeLabels = {
		NSLOCTEXT("ExploredUI", "WindowFullscreen", "Pantalla completa"),
		NSLOCTEXT("ExploredUI", "WindowBorderless", "Ventana sin bordes"),
		NSLOCTEXT("ExploredUI", "WindowWindowed", "Ventana")
	};

	TArray<float> FpsLimits = { 30.0f, 60.0f, 120.0f, 144.0f, 0.0f };
	TArray<FText> FpsLabels = {
		FText::FromString(TEXT("30")), FText::FromString(TEXT("60")), FText::FromString(TEXT("120")),
		FText::FromString(TEXT("144")), NSLOCTEXT("ExploredUI", "Unlimited", "Sin límite")
	};

	auto QualityRow = [](const FText& Label, TFunction<int32()> Get, TFunction<void(int32)> Set)
	{
		return MakeRow(Label, MakeCycler(QualityLabels(), MoveTemp(Get), MoveTemp(Set)));
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f)) [ MakeSectionTitle(NSLOCTEXT("ExploredUI", "SectionDisplay", "Pantalla")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Resolution", "Resolución"),
				MakeCycler(ResolutionLabels,
					[GS, Resolutions]() { const FIntPoint Cur = GS->GetScreenResolution(); return FMath::Max(0, Resolutions.IndexOfByPredicate([Cur](const FIntPoint& R) { return R == Cur; })); },
					[GS, Resolutions](int32 Index) { if (Resolutions.IsValidIndex(Index)) { GS->SetScreenResolution(Resolutions[Index]); } }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "WindowMode", "Modo de ventana"),
				MakeCycler(WindowModeLabels,
					[GS]() { switch (GS->GetFullscreenMode()) { case EWindowMode::Fullscreen: return 0; case EWindowMode::WindowedFullscreen: return 1; default: return 2; } },
					[GS](int32 Index) { GS->SetFullscreenMode(Index == 0 ? EWindowMode::Fullscreen : Index == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "VSync", "VSync"), MakeToggle([GS]() { return GS->IsVSyncEnabled(); }, [GS](bool bOn) { GS->SetVSyncEnabled(bOn); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "FpsLimit", "Límite de FPS"),
				MakeCycler(FpsLabels,
					[GS, FpsLimits]() { const float Cur = GS->GetFrameRateLimit(); int32 Best = 0; for (int32 I = 0; I < FpsLimits.Num(); ++I) { if (FMath::IsNearlyEqual(Cur, FpsLimits[I], 0.5f)) { Best = I; } } return Best; },
					[GS, FpsLimits](int32 Index) { if (FpsLimits.IsValidIndex(Index)) { GS->SetFrameRateLimit(FpsLimits[Index]); } }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Brightness", "Brillo"),
				MakeRangeSlider(ExploredSettingsLogic::BrightnessRange, [GS]() { return GS->GetBrightness(); }, [GS](float V) { GS->SetBrightness(V); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 8.0f)) [ MakeSectionTitle(NSLOCTEXT("ExploredUI", "SectionQuality", "Calidad")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "OverallQuality", "Calidad general"),
				MakeCycler(QualityLabels(), [GS]() { return GS->GetOverallScalabilityLevel(); }, [GS](int32 V) { GS->SetOverallScalabilityLevel(V); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "ViewDistance", "Vista"), [GS]() { return GS->GetViewDistanceQuality(); }, [GS](int32 V) { GS->SetViewDistanceQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "Shadows", "Sombras"), [GS]() { return GS->GetShadowQuality(); }, [GS](int32 V) { GS->SetShadowQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "GI", "Iluminación global"), [GS]() { return GS->GetGlobalIlluminationQuality(); }, [GS](int32 V) { GS->SetGlobalIlluminationQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "Reflections", "Reflejos"), [GS]() { return GS->GetReflectionQuality(); }, [GS](int32 V) { GS->SetReflectionQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "PostProcess", "Postproceso"), [GS]() { return GS->GetPostProcessingQuality(); }, [GS](int32 V) { GS->SetPostProcessingQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "Textures", "Texturas"), [GS]() { return GS->GetTextureQuality(); }, [GS](int32 V) { GS->SetTextureQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "Effects", "Efectos"), [GS]() { return GS->GetVisualEffectQuality(); }, [GS](int32 V) { GS->SetVisualEffectQuality(V); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ QualityRow(NSLOCTEXT("ExploredUI", "Foliage", "Vegetación"), [GS]() { return GS->GetFoliageQuality(); }, [GS](int32 V) { GS->SetFoliageQuality(V); }) ];
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildAudioTab()
{
	UExploredGameUserSettings* GS = Settings.Get();
	if (!GS)
	{
		return SNullWidget::NullWidget;
	}

	auto VolumeRow = [GS](const FText& Label, EExploredAudioChannel Channel)
	{
		return MakeRow(Label, MakeRangeSlider(ExploredSettingsLogic::VolumeRange,
			[GS, Channel]() { return GS->GetVolume(Channel); },
			[GS, Channel](float V) { GS->SetVolume(Channel, V); }));
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ VolumeRow(NSLOCTEXT("ExploredUI", "VolMaster", "Maestro"), EExploredAudioChannel::Master) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ VolumeRow(NSLOCTEXT("ExploredUI", "VolMusic", "Música"), EExploredAudioChannel::Music) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ VolumeRow(NSLOCTEXT("ExploredUI", "VolEffects", "Efectos"), EExploredAudioChannel::Effects) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ VolumeRow(NSLOCTEXT("ExploredUI", "VolAmbient", "Ambiente"), EExploredAudioChannel::Ambient) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ VolumeRow(NSLOCTEXT("ExploredUI", "VolUI", "Interfaz"), EExploredAudioChannel::Interface) ];
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildControlsTab()
{
	UExploredGameUserSettings* GS = Settings.Get();
	if (!GS)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "MouseSensitivity", "Sensibilidad del ratón"),
				MakeRangeSlider(ExploredSettingsLogic::SensitivityRange, [GS]() { return GS->GetMouseSensitivity(); }, [GS](float V) { GS->SetMouseSensitivity(V); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "GamepadSensitivity", "Sensibilidad del mando"),
				MakeRangeSlider(ExploredSettingsLogic::SensitivityRange, [GS]() { return GS->GetGamepadSensitivity(); }, [GS](float V) { GS->SetGamepadSensitivity(V); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "InvertY", "Invertir eje Y"), MakeToggle([GS]() { return GS->GetInvertY(); }, [GS](bool B) { GS->SetInvertY(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "FOV", "Campo de visión"),
				MakeRangeSlider(ExploredSettingsLogic::FieldOfViewRange, [GS]() { return GS->GetFOV(); }, [GS](float V) { GS->SetFOV(V); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "CameraBob", "Balanceo de cámara"), MakeToggle([GS]() { return GS->GetCameraBobEnabled(); }, [GS](bool B) { GS->SetCameraBobEnabled(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "HoldCrouch", "Agacharse manteniendo pulsado"), MakeToggle([GS]() { return GS->GetHoldToCrouch(); }, [GS](bool B) { GS->SetHoldToCrouch(B); }))
		];

	if (InputSettings.IsValid())
	{
		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 8.0f)) [ MakeSectionTitle(NSLOCTEXT("ExploredUI", "SectionRemap", "Remapeo de teclas")) ];

		// Una fila por acción remapeable, de la misma tabla con la que el
		// personaje construye sus mapeos y el subsistema detecta conflictos (M12).
		const TWeakObjectPtr<UExploredInputSettingsSubsystem> WeakInput = InputSettings;
		for (const ExploredSettingsLogic::FRemappableAction& Action : ExploredSettingsLogic::GetRemappableActions())
		{
			const FName ActionName = Action.ActionName;
			const FKey DefaultKey(Action.DefaultKey);
			TAttribute<FKey> KeyAttr = TAttribute<FKey>::CreateLambda([WeakInput, ActionName, DefaultKey]()
			{
				const UExploredInputSettingsSubsystem* IS = WeakInput.Get();
				return IS ? IS->GetKeyFor(ActionName, DefaultKey) : DefaultKey;
			});
			Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
			[
				MakeRow(ActionLabel(ActionName),
					SNew(SExploredKeyCaptureButton)
					.CurrentKey(KeyAttr)
					.OnKeyPicked(FOnExploredKeyPicked::CreateSP(this, &SExploredSettingsPanel::HandleKeyPicked, ActionName)))
			];
		}

		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Font(S().FontBody())
			.ColorAndOpacity(FSlateColor(S().ColorAccent()))
			.AutoWrapText(true)
			.Text_Lambda([this]() { return RemapMessage; })
		];
	}

	return Box;
}

void SExploredSettingsPanel::HandleKeyPicked(FKey NewKey, FName ActionName)
{
	UExploredInputSettingsSubsystem* IS = InputSettings.Get();
	if (!IS)
	{
		return;
	}

	const FName Conflict = IS->GetConflictFor(ActionName, NewKey);
	if (NewKey.IsGamepadKey())
	{
		RemapMessage = NSLOCTEXT("ExploredUI", "RemapGamepad", "Los botones del mando no se pueden remapear.");
	}
	else if (Conflict == UExploredInputSettingsSubsystem::ReservedConflictName)
	{
		RemapMessage = FText::Format(NSLOCTEXT("ExploredUI", "RemapReserved", "{0} está reservada (movimiento o menú)."), NewKey.GetDisplayName());
	}
	else if (!Conflict.IsNone())
	{
		RemapMessage = FText::Format(NSLOCTEXT("ExploredUI", "RemapInUse", "{0} ya se usa para «{1}»."), NewKey.GetDisplayName(), ActionLabel(Conflict));
	}
	else
	{
		IS->SetKeyFor(ActionName, NewKey);
		RemapMessage = FText::GetEmpty();
	}
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildGameTab()
{
	UExploredGameUserSettings* GS = Settings.Get();
	if (!GS)
	{
		return SNullWidget::NullWidget;
	}

	TArray<FText> DayLengthLabels;
	for (const float Minutes : UExploredGameUserSettings::GetSupportedDayLengths())
	{
		DayLengthLabels.Add(FText::FromString(FString::Printf(TEXT("%d min"), static_cast<int32>(Minutes))));
	}

	TArray<FText> TextSizeLabels = {
		NSLOCTEXT("ExploredUI", "TextSmall", "Pequeño"),
		NSLOCTEXT("ExploredUI", "TextMedium", "Mediano"),
		NSLOCTEXT("ExploredUI", "TextLarge", "Grande")
	};
	TArray<FText> LanguageLabels = {
		NSLOCTEXT("ExploredUI", "LangEs", "Español"),
		NSLOCTEXT("ExploredUI", "LangEn", "English")
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "DayLength", "Duración del día"),
				MakeCycler(DayLengthLabels,
					// Índice de la duración soportada más cercana: nunca -1 aunque el ini traiga un valor raro (M14).
					[GS]() { return ExploredSettingsLogic::DayLengthIndex(GS->GetDayLengthMinutes()); },
					[GS](int32 Index) { const TArray<float>& Lengths = UExploredGameUserSettings::GetSupportedDayLengths(); if (Lengths.IsValidIndex(Index)) { GS->SetDayLengthMinutes(Lengths[Index]); } }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Subtitles", "Subtítulos"), MakeToggle([GS]() { return GS->GetSubtitlesEnabled(); }, [GS](bool B) { GS->SetSubtitlesEnabled(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "TextSize", "Tamaño del texto"),
				MakeCycler(TextSizeLabels,
					[GS]() { return static_cast<int32>(GS->GetTextSize()); },
					[GS](int32 Index) { GS->SetTextSize(static_cast<EExploredTextSize>(Index)); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Language", "Idioma"),
				MakeCycler(LanguageLabels,
					[GS]() { return static_cast<int32>(GS->GetLanguage()); },
					[GS](int32 Index) { GS->SetLanguage(static_cast<EExploredLanguage>(Index)); }))
		];
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildAccessibilityTab()
{
	UExploredGameUserSettings* GS = Settings.Get();
	if (!GS)
	{
		return SNullWidget::NullWidget;
	}

	TArray<FText> ColorblindLabels = {
		NSLOCTEXT("ExploredUI", "CVDNone", "Ninguno"),
		NSLOCTEXT("ExploredUI", "CVDProtanopia", "Protanopía"),
		NSLOCTEXT("ExploredUI", "CVDDeuteranopia", "Deuteranopía"),
		NSLOCTEXT("ExploredUI", "CVDTritanopia", "Tritanopía")
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Colorblind", "Modo daltónico"),
				MakeCycler(ColorblindLabels,
					[GS]() { return static_cast<int32>(GS->GetColorblindMode()); },
					[GS](int32 Index) { GS->SetColorblindMode(static_cast<EExploredColorblindMode>(Index)); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "ReduceMotion", "Reducir movimiento"), MakeToggle([GS]() { return GS->GetReduceMotion(); }, [GS](bool B) { GS->SetReduceMotion(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "NoFlashing", "Desactivar destellos"), MakeToggle([GS]() { return GS->GetDisableFlashing(); }, [GS](bool B) { GS->SetDisableFlashing(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "SoundCues", "Avisos visuales de sonido"), MakeToggle([GS]() { return GS->GetSoundVisualCues(); }, [GS](bool B) { GS->SetSoundVisualCues(B); }))
		];
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildFooter()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
		[
			ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Apply", "Aplicar"), FOnClicked::CreateSP(this, &SExploredSettingsPanel::HandleApply), false)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
		[
			ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "RestoreDefaults", "Restaurar valores por defecto"), FOnClicked::CreateSP(this, &SExploredSettingsPanel::HandleRestoreDefaults), false)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			ExploredUIWidgets::MakeMenuButton(NSLOCTEXT("ExploredUI", "Back", "Volver"), FOnClicked::CreateSP(this, &SExploredSettingsPanel::HandleBack), false)
		];
}

FReply SExploredSettingsPanel::HandleApply()
{
	if (UExploredGameUserSettings* GS = Settings.Get())
	{
		// ApplySettings → ApplyNonResolutionSettings (override): brillo, idioma,
		// daltonismo, audio, duración del día y aviso al personaje; y guarda el ini.
		GS->ApplySettings(false);
		// Redundante con lo anterior salvo si el mundo del panel no figura entre
		// los mundos de juego del motor; ApplyToWorld es idempotente.
		GS->ApplyToWorld(WorldContextObject.Get());
	}
	return FReply::Handled();
}

FReply SExploredSettingsPanel::HandleRestoreDefaults()
{
	if (UExploredGameUserSettings* GS = Settings.Get())
	{
		GS->SetToDefaults();
		// M11: SetToDefaults solo cambia los valores; lo que se previsualiza al
		// instante (brillo, daltonismo, idioma) se re-aplica para que la pantalla
		// coincida con lo que muestra el panel. El resto espera a «Aplicar».
		GS->ApplyPreviewSettings();
	}
	return FReply::Handled();
}

FReply SExploredSettingsPanel::HandleBack()
{
	RequestBack();
	return FReply::Handled();
}

void SExploredSettingsPanel::RequestBack()
{
	if (UExploredGameUserSettings* GS = Settings.Get())
	{
		// M11: «Volver» sin «Aplicar» descarta. Se recarga el ini del disco y se
		// re-aplica todo lo que no es resolución (que no cambia hasta
		// ApplyResolutionSettings), incluidos brillo, idioma y daltonismo.
		// El remapeo de teclas no entra aquí: se guarda al elegir cada tecla.
		GS->LoadSettings(true);
		GS->ApplyNonResolutionSettings();
		// En el editor ApplyNonResolutionSettings no toca la cultura; en PIE el
		// selector sí la cambió al instante, así que se deshace aquí explícitamente.
		GS->ApplyPreviewSettings();
	}
	OnBack.ExecuteIfBound();
}

FReply SExploredSettingsPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Llega por burbujeo desde el control con foco (botón, slider...) si ese
	// control no la ha consumido. El botón de remapeo que está capturando sí la
	// consume, así que Escape allí solo cancela la captura.
	if (ExploredSettingsLogic::IsMenuBackKey(InKeyEvent.GetKey().GetFName()))
	{
		RequestBack();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
