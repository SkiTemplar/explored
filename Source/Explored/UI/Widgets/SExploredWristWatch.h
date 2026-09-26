#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Survival/BodySignals.h"

/**
 * Reloj de pulsera del Albatros (GDD §8.3): al levantar la muñeca se ve la
 * hora y, si el jugador lo quiere, cuatro indicadores gruesos (agua, comida,
 * sueño, calor) en palabras, sin números ni iconos. Es una vista pura: lee la
 * lectura (FWristWatchReadout) y si la muñeca está levantada por atributos.
 */
class EXPLORED_API SExploredWristWatch : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredWristWatch) {}
		SLATE_ATTRIBUTE(FWristWatchReadout, Readout)
		SLATE_ATTRIBUTE(bool, Raised)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	EVisibility GetWatchVisibility() const;
	EVisibility GetNeedsVisibility() const;
	FText GetTimeText() const;
	TSharedRef<SWidget> MakeNeedRow(const FText& Label, ENeedLevel FWristWatchReadout::* Field);

	static FText LevelText(ENeedLevel Level);
	static FSlateColor LevelColor(ENeedLevel Level);

	TAttribute<FWristWatchReadout> Readout;
	TAttribute<bool> Raised;
};
