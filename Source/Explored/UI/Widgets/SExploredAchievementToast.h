#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

/**
 * Aviso discreto de logro conseguido (GDD §16), en la esquina superior
 * derecha con el mismo papel y tinta que el resto del frontend. Si llegan
 * varios a la vez se muestran en cola, uno detrás de otro.
 */
class EXPLORED_API SExploredAchievementToast : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredAchievementToast) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Encola un aviso con el nombre y la descripción ya en el idioma de la interfaz. */
	void Enqueue(const FText& Name, const FText& Description, float Seconds = 4.0f);

private:
	struct FPendingToast
	{
		FText Name;
		FText Description;
		float Seconds = 4.0f;
	};

	void ShowNext();

	// Nombre distinto de SWidget::Tick a propósito (ver SExploredFade): es el
	// callback de RegisterActiveTimer, no un override.
	EActiveTimerReturnType HandleToastTick(double InCurrentTime, float InDeltaTime);

	TArray<FPendingToast> Queue;
	TSharedPtr<STextBlock> NameText;
	TSharedPtr<STextBlock> DescriptionText;
	float RemainingSeconds = 0.0f;
	bool bTimerActive = false;
};
