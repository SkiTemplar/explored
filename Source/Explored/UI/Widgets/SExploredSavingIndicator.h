#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Indicador «Guardando…» en la esquina, GDD §10. Se muestra unos segundos y se oculta sola. */
class EXPLORED_API SExploredSavingIndicator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredSavingIndicator) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void Show(float Seconds = 1.6f);

private:
	EActiveTimerReturnType Tick(double InCurrentTime, float InDeltaTime);

	float RemainingSeconds = 0.0f;
};
