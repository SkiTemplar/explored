#include "UI/Widgets/SExploredFade.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

void SExploredFade::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(0.0f, 0.0f, 0.0f, Opacity); })
			.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
		]
	];
}

TOptional<EMouseCursor::Type> SExploredFade::GetCursor() const
{
	return TOptional<EMouseCursor::Type>();
}

void SExploredFade::FadeToBlack(float Seconds, TFunction<void()> OnDone)
{
	StartOpacity = Opacity;
	TargetOpacity = 1.0f;
	Duration = FMath::Max(Seconds, 0.05f);
	Elapsed = 0.0f;
	bAnimating = true;
	PendingCallback = MoveTemp(OnDone);
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SExploredFade::Tick));
}

void SExploredFade::FadeFromBlack(float Seconds, TFunction<void()> OnDone)
{
	StartOpacity = Opacity;
	TargetOpacity = 0.0f;
	Duration = FMath::Max(Seconds, 0.05f);
	Elapsed = 0.0f;
	bAnimating = true;
	PendingCallback = MoveTemp(OnDone);
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SExploredFade::Tick));
}

EActiveTimerReturnType SExploredFade::Tick(double InCurrentTime, float InDeltaTime)
{
	if (!bAnimating)
	{
		return EActiveTimerReturnType::Stop;
	}

	Elapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
	Opacity = FMath::Lerp(StartOpacity, TargetOpacity, Alpha);

	if (Alpha >= 1.0f)
	{
		bAnimating = false;
		if (PendingCallback)
		{
			TFunction<void()> Callback = MoveTemp(PendingCallback);
			PendingCallback = nullptr;
			Callback();
		}
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}
