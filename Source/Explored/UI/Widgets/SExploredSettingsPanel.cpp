#include "UI/Widgets/SExploredSettingsPanel.h"

#include "Kismet/KismetSystemLibrary.h"
#include "UI/ExploredGameUserSettings.h"
#include "UI/ExploredInputSettingsSubsystem.h"
#include "UI/Widgets/ExploredUIWidgets.h"
#include "UI/Widgets/SExploredKeyCaptureButton.h"
#include "UI/ExploredUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
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

	/** Selector de opción discreta con flechas «<  Texto  >», cómodo con mando y ratón. */
	TSharedRef<SWidget> MakeCycler(TArray<FText> Labels, TFunction<int32()> GetIndex, TFunction<void(int32)> SetIndex)
	{
		const int32 Count = FMath::Max(Labels.Num(), 1);
		TSharedRef<STextBlock> Text = SNew(STextBlock)
			.Font(S().FontBody())
			.ColorAndOpacity(FSlateColor(S().ColorInk()))
			.MinDesiredWidth(160.0f)
			.Justification(ETextJustify::Center)
			.Text_Lambda([GetIndex, Labels, Count]() { return Labels[((GetIndex() % Count) + Count) % Count]; });

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
				.OnClicked_Lambda([GetIndex, SetIndex, Count]() { SetIndex((GetIndex() + 1) % Count); return FReply::Handled(); })
			];
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
	auto MakeTab = [this](ETab Tab, const FText& Label)
	{
		return SNew(SBox).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			ExploredUIWidgets::MakeTabButton(Label, CurrentTab == Tab, FOnClicked::CreateSP(this, &SExploredSettingsPanel::SelectTab, Tab))
		];
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth() [ MakeTab(ETab::Graphics, NSLOCTEXT("ExploredUI", "TabGraphics", "Gráficos")) ]
		+ SHorizontalBox::Slot().AutoWidth() [ MakeTab(ETab::Audio, NSLOCTEXT("ExploredUI", "TabAudio", "Audio")) ]
		+ SHorizontalBox::Slot().AutoWidth() [ MakeTab(ETab::Controls, NSLOCTEXT("ExploredUI", "TabControls", "Controles")) ]
		+ SHorizontalBox::Slot().AutoWidth() [ MakeTab(ETab::Game, NSLOCTEXT("ExploredUI", "TabGame", "Juego")) ]
		+ SHorizontalBox::Slot().AutoWidth() [ MakeTab(ETab::Accessibility, NSLOCTEXT("ExploredUI", "TabAccessibility", "Accesibilidad")) ];
}

FReply SExploredSettingsPanel::SelectTab(ETab Tab)
{
	CurrentTab = Tab;
	Switcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	// Se reconstruye entera porque el estilo activo/inactivo de cada botón se fija al crearlo.
	TabBarContainer->SetContent(BuildTabBar());
	return FReply::Handled();
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildGraphicsTab()
{
	UExploredGameUserSettings* GS = Settings;
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

	auto QualityRow = [this](const FText& Label, TFunction<int32()> Get, TFunction<void(int32)> Set)
	{
		return MakeRow(Label, MakeCycler(QualityLabels(), MoveTemp(Get), MoveTemp(Set)));
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f)) [ MakeSectionTitle(NSLOCTEXT("ExploredUI", "SectionDisplay", "Pantalla")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Resolution", "Resolución"),
				MakeCycler(ResolutionLabels,
					[GS, Resolutions]() { const FIntPoint Cur = GS->GetScreenResolution(); return Resolutions.IndexOfByPredicate([Cur](const FIntPoint& R) { return R == Cur; }); },
					[GS, Resolutions](int32 Index) { GS->SetScreenResolution(Resolutions[Index]); }))
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
					[GS, FpsLimits](int32 Index) { GS->SetFrameRateLimit(FpsLimits[Index]); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "Brightness", "Brillo"),
				SNew(SBox).WidthOverride(220.0f)
				[
					SNew(SSlider).Style(&S().SliderStyle())
					.Value_Lambda([GS]() { return (GS->GetBrightness() - 1.7f) / 1.0f; })
					.OnValueChanged_Lambda([GS](float V) { GS->SetBrightness(1.7f + V * 1.0f); })
				])
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
	UExploredGameUserSettings* GS = Settings;
	auto VolumeRow = [GS](const FText& Label, EExploredAudioChannel Channel)
	{
		return MakeRow(Label, SNew(SBox).WidthOverride(220.0f)
			[
				SNew(SSlider).Style(&S().SliderStyle())
				.Value_Lambda([GS, Channel]() { return GS->GetVolume(Channel) / 100.0f; })
				.OnValueChanged_Lambda([GS, Channel](float V) { GS->SetVolume(Channel, V * 100.0f); })
			]);
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
	UExploredGameUserSettings* GS = Settings;
	UExploredInputSettingsSubsystem* IS = InputSettings;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "MouseSensitivity", "Sensibilidad del ratón"),
				SNew(SBox).WidthOverride(220.0f)
				[
					SNew(SSlider).Style(&S().SliderStyle())
					.Value_Lambda([GS]() { return GS->GetMouseSensitivity() / 5.0f; })
					.OnValueChanged_Lambda([GS](float V) { GS->SetMouseSensitivity(V * 5.0f); })
				])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "GamepadSensitivity", "Sensibilidad del mando"),
				SNew(SBox).WidthOverride(220.0f)
				[
					SNew(SSlider).Style(&S().SliderStyle())
					.Value_Lambda([GS]() { return GS->GetGamepadSensitivity() / 5.0f; })
					.OnValueChanged_Lambda([GS](float V) { GS->SetGamepadSensitivity(V * 5.0f); })
				])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "InvertY", "Invertir eje Y"), MakeToggle([GS]() { return GS->GetInvertY(); }, [GS](bool B) { GS->SetInvertY(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "FOV", "Campo de visión"),
				SNew(SBox).WidthOverride(220.0f)
				[
					SNew(SSlider).Style(&S().SliderStyle())
					.Value_Lambda([GS]() { return (GS->GetFOV() - 70.0f) / 40.0f; })
					.OnValueChanged_Lambda([GS](float V) { GS->SetFOV(70.0f + V * 40.0f); })
				])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "CameraBob", "Balanceo de cámara"), MakeToggle([GS]() { return GS->GetCameraBobEnabled(); }, [GS](bool B) { GS->SetCameraBobEnabled(B); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			MakeRow(NSLOCTEXT("ExploredUI", "HoldCrouch", "Agacharse manteniendo pulsado"), MakeToggle([GS]() { return GS->GetHoldToCrouch(); }, [GS](bool B) { GS->SetHoldToCrouch(B); }))
		];

	if (IS)
	{
		IS->RegisterAction(ExploredInputActionNames::Jump, EKeys::SpaceBar);
		IS->RegisterAction(ExploredInputActionNames::Sprint, EKeys::LeftShift);

		Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 8.0f)) [ MakeSectionTitle(NSLOCTEXT("ExploredUI", "SectionRemap", "Remapeo de teclas")) ];

		auto AddRemapRow = [&Box, IS](const FName ActionName, FKey DefaultKey, const FText& Label)
		{
			TAttribute<FKey> KeyAttr = TAttribute<FKey>::CreateLambda([IS, ActionName, DefaultKey]() { return IS->GetKeyFor(ActionName, DefaultKey); });
			Box->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
			[
				MakeRow(Label,
					SNew(SExploredKeyCaptureButton)
					.CurrentKey(KeyAttr)
					.OnKeyPicked_Lambda([IS, ActionName](FKey NewKey) { IS->SetKeyFor(ActionName, NewKey); }))
			];
		};
		AddRemapRow(ExploredInputActionNames::Jump, EKeys::SpaceBar, NSLOCTEXT("ExploredUI", "ActionJump", "Saltar"));
		AddRemapRow(ExploredInputActionNames::Sprint, EKeys::LeftShift, NSLOCTEXT("ExploredUI", "ActionSprint", "Correr"));
	}

	return Box;
}

TSharedRef<SWidget> SExploredSettingsPanel::BuildGameTab()
{
	UExploredGameUserSettings* GS = Settings;

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
					[GS]() { const TArray<float>& Lengths = UExploredGameUserSettings::GetSupportedDayLengths(); return Lengths.IndexOfByPredicate([GS](float M) { return FMath::IsNearlyEqual(M, GS->GetDayLengthMinutes(), 0.1f); }); },
					[GS](int32 Index) { GS->SetDayLengthMinutes(UExploredGameUserSettings::GetSupportedDayLengths()[Index]); }))
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
	UExploredGameUserSettings* GS = Settings;

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
	if (Settings)
	{
		Settings->ApplySettings(false);
		Settings->ApplyAudioSettings(WorldContextObject);
	}
	return FReply::Handled();
}

FReply SExploredSettingsPanel::HandleRestoreDefaults()
{
	if (Settings)
	{
		Settings->SetToDefaults();
	}
	return FReply::Handled();
}

FReply SExploredSettingsPanel::HandleBack()
{
	OnBack.ExecuteIfBound();
	return FReply::Handled();
}
