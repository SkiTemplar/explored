#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Pantalla de créditos (GDD §10): desplazable, sobre el mundo. */
class EXPLORED_API SExploredCredits : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredCredits) {}
		SLATE_EVENT(FSimpleDelegate, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FSimpleDelegate OnBack;
	FReply HandleBack();
};
