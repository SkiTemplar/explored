#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Menú principal (GDD §10, §15): título, subtítulo, la dedicatoria «Para
 * Almudena, mi Limón» y Continuar, Nueva partida, Cargar, Ajustes, Logros,
 * Créditos y Salir sobre el mundo real.
 * Escape no hace nada aquí (ver ExploredSettingsLogic::ScreenAfterBack).
 */
class EXPLORED_API SExploredMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExploredMainMenu)
		: _bCanContinue(false)
	{}
		SLATE_ARGUMENT(bool, bCanContinue)
		SLATE_EVENT(FSimpleDelegate, OnContinue)
		SLATE_EVENT(FSimpleDelegate, OnNewGame)
		SLATE_EVENT(FSimpleDelegate, OnLoad)
		SLATE_EVENT(FSimpleDelegate, OnSettings)
		SLATE_EVENT(FSimpleDelegate, OnAchievements)
		SLATE_EVENT(FSimpleDelegate, OnCredits)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }

	/** «Continuar» si hay partida guardada; si no, «Nueva partida». */
	TSharedPtr<SWidget> GetInitialFocus() const { return InitialFocus; }

private:
	FSimpleDelegate OnContinue;
	FSimpleDelegate OnNewGame;
	FSimpleDelegate OnLoad;
	FSimpleDelegate OnSettings;
	FSimpleDelegate OnAchievements;
	FSimpleDelegate OnCredits;
	FSimpleDelegate OnQuit;
	TSharedPtr<SWidget> InitialFocus;

	FReply HandleContinue();
	FReply HandleNewGame();
	FReply HandleLoad();
	FReply HandleSettings();
	FReply HandleAchievements();
	FReply HandleCredits();
	FReply HandleQuit();
};
