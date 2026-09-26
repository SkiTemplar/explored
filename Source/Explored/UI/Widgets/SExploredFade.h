#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Fundido a negro reutilizable (GDD §10: pantalla de carga y fundidos).
 * Se añade una sola vez al viewport con ZOrder alto y se reutiliza para
 * todos los fundidos del frontend (nueva partida, salir al menú...).
 */
class EXPLORED_API SExploredFade : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredFade) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Va de transparente a negro opaco en Seconds y llama a OnDone al terminar. */
	void FadeToBlack(float Seconds, TFunction<void()> OnDone);
	/** Va de negro opaco a transparente en Seconds. */
	void FadeFromBlack(float Seconds, TFunction<void()> OnDone = nullptr);

	bool IsFading() const { return bAnimating; }

private:
	EActiveTimerReturnType Tick(double InCurrentTime, float InDeltaTime);
	virtual TOptional<EMouseCursor::Type> GetCursor() const override;

	float Opacity = 0.0f;
	float StartOpacity = 0.0f;
	float TargetOpacity = 0.0f;
	float Duration = 0.5f;
	float Elapsed = 0.0f;
	bool bAnimating = false;
	TFunction<void()> PendingCallback;
};
