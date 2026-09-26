#include "UI/SettingsLogic.h"

namespace ExploredSettingsLogic
{
	float ClampToRange(float Value, const FSettingRange& Range)
	{
		if (!FMath::IsFinite(Value))
		{
			return Range.Default;
		}
		return FMath::Clamp(Value, Range.Min, Range.Max);
	}

	float ValueToSlider(float Value, const FSettingRange& Range)
	{
		const float Span = Range.Max - Range.Min;
		if (Span <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::Clamp((ClampToRange(Value, Range) - Range.Min) / Span, 0.0f, 1.0f);
	}

	float SliderToValue(float Slider01, const FSettingRange& Range)
	{
		const float Alpha = FMath::IsFinite(Slider01) ? FMath::Clamp(Slider01, 0.0f, 1.0f) : 0.0f;
		return ClampToRange(Range.Min + Alpha * (Range.Max - Range.Min), Range);
	}

	float VolumeToGain(float Volume0To100)
	{
		return ClampToRange(Volume0To100, VolumeRange) / VolumeRange.Max;
	}

	float AmbienceLayerGain(float AmbientVolume0To100, bool bRoutedThroughSoundClass)
	{
		return bRoutedThroughSoundClass ? 1.0f : VolumeToGain(AmbientVolume0To100);
	}

	const TArray<float>& GetSupportedDayLengths()
	{
		static const TArray<float> Lengths = { 20.0f, 40.0f, 60.0f, 90.0f };
		return Lengths;
	}

	int32 DayLengthIndex(float Minutes)
	{
		const TArray<float>& Lengths = GetSupportedDayLengths();
		const float Target = FMath::IsFinite(Minutes) ? Minutes : DefaultDayLengthMinutes;
		int32 Best = 0;
		float BestDist = FMath::Abs(Target - Lengths[0]);
		for (int32 I = 1; I < Lengths.Num(); ++I)
		{
			const float Dist = FMath::Abs(Target - Lengths[I]);
			if (Dist < BestDist)
			{
				Best = I;
				BestDist = Dist;
			}
		}
		return Best;
	}

	float SnapDayLength(float Minutes)
	{
		return GetSupportedDayLengths()[DayLengthIndex(Minutes)];
	}

	int32 SanitizeEnumIndex(int32 Value, int32 Count, int32 Default)
	{
		return (Value >= 0 && Value < Count) ? Value : Default;
	}

	const TArray<FRemappableAction>& GetRemappableActions()
	{
		// Nombres de acción = nombres de los UInputAction que crea AExploredCharacter;
		// nombres de tecla = los de EKeys.
		static const TArray<FRemappableAction> Actions = {
			{ FName(TEXT("IA_Jump")), FName(TEXT("SpaceBar")) },
			{ FName(TEXT("IA_Sprint")), FName(TEXT("LeftShift")) },
			{ FName(TEXT("IA_Dive")), FName(TEXT("LeftControl")) },
			{ FName(TEXT("IA_Interact")), FName(TEXT("E")) },
			{ FName(TEXT("IA_UsePrimary")), FName(TEXT("LeftMouseButton")) },
			{ FName(TEXT("IA_UseSecondary")), FName(TEXT("RightMouseButton")) },
			{ FName(TEXT("IA_Drop")), FName(TEXT("G")) },
			{ FName(TEXT("IA_Combine")), FName(TEXT("C")) },
			{ FName(TEXT("IA_ToggleBackpack")), FName(TEXT("Tab")) },
		};
		return Actions;
	}

	FName GetDefaultKeyFor(FName ActionName)
	{
		for (const FRemappableAction& Action : GetRemappableActions())
		{
			if (Action.ActionName == ActionName)
			{
				return Action.DefaultKey;
			}
		}
		return NAME_None;
	}

	bool IsReservedKey(FName KeyName)
	{
		static const TArray<FName> Reserved = {
			FName(TEXT("W")), FName(TEXT("A")), FName(TEXT("S")), FName(TEXT("D")),
			FName(TEXT("Escape")), FName(TEXT("M")), FName(TEXT("F8"))
		};
		return Reserved.Contains(KeyName);
	}

	FRemapCheckResult CheckRemap(const TArray<FKeyBinding>& EffectiveBindings, FName ActionName, FName NewKey)
	{
		FRemapCheckResult Out;
		if (NewKey.IsNone() || ActionName.IsNone())
		{
			Out.Result = ERemapCheck::InvalidKey;
			return Out;
		}
		if (IsReservedKey(NewKey))
		{
			Out.Result = ERemapCheck::ReservedKey;
			return Out;
		}
		for (const FKeyBinding& Binding : EffectiveBindings)
		{
			if (Binding.ActionName != ActionName && Binding.Key == NewKey)
			{
				Out.Result = ERemapCheck::InUse;
				Out.ConflictingAction = Binding.ActionName;
				return Out;
			}
		}
		return Out;
	}

	EMenuScreen ScreenAfterBack(EMenuScreen Current, bool bOpenedFromPause)
	{
		switch (Current)
		{
		case EMenuScreen::ModeSelect:
		case EMenuScreen::Credits:
			return EMenuScreen::MainMenu;
		case EMenuScreen::Settings:
		case EMenuScreen::Achievements:
		case EMenuScreen::SaveSlots:
			return bOpenedFromPause ? EMenuScreen::Pause : EMenuScreen::MainMenu;
		case EMenuScreen::Map:
			return bOpenedFromPause ? EMenuScreen::Pause : EMenuScreen::None;
		case EMenuScreen::Museum:
			return bOpenedFromPause ? EMenuScreen::Pause : EMenuScreen::Map;
		case EMenuScreen::Pause:
			return EMenuScreen::None;
		case EMenuScreen::MainMenu:
		case EMenuScreen::None:
		default:
			return Current;
		}
	}

	EMenuScreen ScreenAfterEscape(EMenuScreen Current, bool bOpenedFromPause)
	{
		return Current == EMenuScreen::None ? EMenuScreen::Pause : ScreenAfterBack(Current, bOpenedFromPause);
	}

	bool CanOpenScreenFrom(EMenuScreen From, EMenuScreen To)
	{
		switch (From)
		{
		case EMenuScreen::MainMenu:
			return To == EMenuScreen::ModeSelect || To == EMenuScreen::Settings || To == EMenuScreen::Credits
				|| To == EMenuScreen::Achievements || To == EMenuScreen::SaveSlots;
		case EMenuScreen::None:
			return To == EMenuScreen::Pause || To == EMenuScreen::Map;
		case EMenuScreen::Pause:
			return To == EMenuScreen::Settings || To == EMenuScreen::Map || To == EMenuScreen::Museum
				|| To == EMenuScreen::Achievements || To == EMenuScreen::SaveSlots;
		case EMenuScreen::Map:
			return To == EMenuScreen::Museum;
		default:
			return false;
		}
	}

	void FMenuNavigation::Reset(EMenuScreen Root)
	{
		Stack.Reset();
		Stack.Add(Root);
	}

	bool FMenuNavigation::Open(EMenuScreen Screen)
	{
		if (Screen == Current())
		{
			return true;
		}
		const int32 Existing = Stack.Find(Screen);
		if (Existing != INDEX_NONE)
		{
			Stack.SetNum(Existing + 1);
			return true;
		}
		if (!CanOpenScreenFrom(Current(), Screen))
		{
			return false;
		}
		Stack.Add(Screen);
		return true;
	}

	EMenuScreen FMenuNavigation::BackTarget() const
	{
		if (Stack.Num() > 1)
		{
			return Stack[Stack.Num() - 2];
		}
		return ScreenAfterBack(Current(), false);
	}

	EMenuScreen FMenuNavigation::EscapeTarget() const
	{
		return Current() == EMenuScreen::None ? EMenuScreen::Pause : BackTarget();
	}

	bool IsMenuBackKey(FName KeyName)
	{
		return KeyName == FName(TEXT("Escape")) || KeyName == FName(TEXT("Gamepad_FaceButton_Right"));
	}

	bool IsPauseToggleKey(FName KeyName)
	{
		return KeyName == FName(TEXT("Escape")) || KeyName == FName(TEXT("Gamepad_Special_Right"));
	}

	bool IsMapToggleKey(FName KeyName)
	{
		return KeyName == FName(TEXT("M")) || KeyName == FName(TEXT("Gamepad_Special_Left"));
	}
}
