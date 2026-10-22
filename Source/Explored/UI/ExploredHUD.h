#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "ExploredHUD.generated.h"

class AExploredCharacter;
class FExploredHudPainter;
class SExploredHudOverlay;
class UExploredSurvivorComponent;

/** Un aviso flotante (recogida, evento del cuerpo) con el tiempo que le queda en pantalla. */
struct FExploredHUDNotification
{
	FText Text;
	FLinearColor Color = FLinearColor::White;
	float RemainingSeconds = 0.0f;
	float TotalSeconds = 1.0f;
	/** Cartel de desmayo: velo negro a pantalla completa debajo del texto. */
	bool bVeil = false;
	/** Objeto de la recogida (icono del aviso); None si no hay. */
	FName ItemId;
};

/**
 * HUD de Baliza Siete (GDD v3 §9.1). Pinta en Slate (SExploredHudOverlay) con el estilo del cuaderno:
 * 4 barras del cuerpo con su icono abajo a la izquierda (sin números; parpadean bajo 25), 3 ranuras
 * rápidas con el icono del objeto abajo en el centro, aviso «E · Verbo objeto» bajo la retícula de
 * punto, objetivo arriba a la izquierda, «Día N · HH:MM» y el barómetro con su flecha (si se lleva)
 * arriba a la derecha, subtítulo de pensamiento sobre las ranuras, barra de progreso de acciones
 * largas y el cartel «Día N». Las reglas y los textos están en FIslandHudModel. Nada de UMG.
 *
 * Blueprint: «Push Notification» y «Show Subtitle».
 */
UCLASS()
class EXPLORED_API AExploredHUD : public AHUD
{
	GENERATED_BODY()

public:
	AExploredHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Aviso flotante breve (recogida, hito). Lo puede llamar cualquier sistema del juego. */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void PushNotification(const FText& Text, FLinearColor Color = FLinearColor::White, float DurationSeconds = 2.5f);

	/**
	 * Pensamiento de Iria o aviso de la historia (GDD v3 §9.1): subtítulo abajo, sobre las ranuras.
	 * Si ya hay uno en pantalla, el nuevo espera su turno.
	 */
	UFUNCTION(BlueprintCallable, Category = "Explored|UI")
	void ShowSubtitle(const FText& Text, float DurationSeconds = 4.0f);

	/** Página del cuaderno: título, texto de Roig y nota de Iria en una hoja central. */
	void ShowPage(const FText& Title, const FText& Body, const FText& Note, float DurationSeconds);

	/** Cartel centrado (inicio de partida, «Día N», desmayo). Si ya hay uno, espera su turno. */
	void ShowCard(const FText& Text, float DurationSeconds);

	/** Desmayo (GDD v3 §9.2): negro con el texto «Despiertas en el refugio…» encima de todo el HUD. */
	void ShowBlackout(const FText& Text, float DurationSeconds);

	/** Pinta el HUD entero; lo llama SExploredHudOverlay en cada fotograma. */
	void PaintOverlay(FExploredHudPainter& Painter);

	/** Quita lo transitorio (avisos, subtítulos, página, carteles): al cargar o empezar partida. */
	void ClearTransient();

	/** Nombre visible de la tecla asignada a una acción (la remapeada, o la de fábrica). */
	FString KeyNameFor(FName ActionName) const;

private:
	AExploredCharacter* GetExploredCharacter() const;
	bool ShouldPaint() const;
	void PaintReticle(FExploredHudPainter& P);
	void PaintObjective(FExploredHudPainter& P);
	void PaintClock(FExploredHudPainter& P, const AExploredCharacter* Character);
	void PaintPrompt(FExploredHudPainter& P, const AExploredCharacter& Character);
	void PaintBodyBars(FExploredHudPainter& P, const AExploredCharacter& Character);
	void PaintQuickSlots(FExploredHudPainter& P, const AExploredCharacter& Character);
	void PaintActionBar(FExploredHudPainter& P, const AExploredCharacter& Character);
	void PaintNotifications(FExploredHudPainter& P);
	void PaintSubtitle(FExploredHudPainter& P);
	void PaintPage(FExploredHudPainter& P);
	void PaintCard(FExploredHudPainter& P);
	float CurrentActionProgress(const AExploredCharacter& Character) const;
	/** Cartel «Día N» cuando cambia el día. */
	void UpdateDayCard();
	void EnsureBound(const AExploredCharacter& Character);
	static float AgeItem(FExploredHUDNotification& Item, float DeltaSeconds);
	static float AlphaOf(const FExploredHUDNotification& Item);

	UFUNCTION()
	void HandleItemPickedUp(FText DisplayName);

	UFUNCTION()
	void HandleSurvivorMessage(const FText& Message);

	TSharedPtr<SExploredHudOverlay> Overlay;
	TArray<FExploredHUDNotification> Notifications;
	/** Subtítulos en cola: se muestra el primero. */
	TArray<FExploredHUDNotification> Subtitles;
	FExploredHUDNotification Page;
	FText PageTitle;
	FText PageNote;
	FExploredHUDNotification Card;
	/** Carteles en espera mientras se ve el actual. */
	TArray<FExploredHUDNotification> PendingCards;
	int32 LastCardDay = 0;
	bool bBoundToCarry = false;
	TWeakObjectPtr<UExploredSurvivorComponent> BoundSurvivor;
	/** Alto de la tarjeta de barras (para apilar encima los avisos). */
	float BarsTop = 0.0f;
};
