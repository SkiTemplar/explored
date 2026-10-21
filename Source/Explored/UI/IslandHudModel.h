#pragma once

#include "CoreMinimal.h"
#include "Items/BackpackModel.h"
#include "Weather/StormClockModel.h"

/**
 * Reglas y textos del HUD de Baliza Siete (GDD v3 §9.1) sin Canvas: reloj «Día N · HH:MM»,
 * barómetro «NNNN hPa ↓», parpadeo de las barras bajas, las 3 ranuras rápidas, el aviso de
 * interacción «E · Verbo objeto» y la barra de progreso. AExploredHUD solo dibuja.
 */

struct EXPLORED_API FHudQuickSlot
{
	FName ItemId;
	int32 Count = 0;
	bool bSelected = false;

	bool IsEmpty() const { return ItemId.IsNone(); }
};

struct EXPLORED_API FIslandHudModel
{
	static constexpr int32 QuickSlotCount = 3;
	/** Parpadeos por segundo de una barra bajo el umbral. */
	static constexpr float BlinkHz = 2.0f;

	/** «Día 2 · 15:40». Las horas se truncan al minuto; fuera de [0, 24) se ajustan. */
	static FString ClockText(int32 Day, float HourOfDay);

	/** «Día 2» (cartel al empezar cada día). */
	static FString DayCardText(int32 Day);

	/** «1008 hPa ↓» (↑ sube, ↓ baja, → estable). */
	static FString BarometerText(float Hpa, EBarometerTrend Trend);

	/** «E · Beber coco». Sin tecla o sin texto, vacío. */
	static FString PromptText(const FString& KeyName, const FString& VerbAndObject);

	/** La barra se ve: siempre por encima del umbral; por debajo, parpadea a BlinkHz. */
	static bool IsBarVisible(float Value, float LowThreshold, double TimeSeconds);

	/**
	 * Las 3 ranuras rápidas: las 3 primeras pilas de la mochila, en orden. Selected (0-2) marca la
	 * del objeto en mano; fuera de rango, ninguna.
	 */
	static TArray<FHudQuickSlot> QuickSlots(const FBackpackState& Pack, int32 Selected);

	/** Fracción 0-1 de una acción larga; 0 si el total no es válido. */
	static float Progress01(float ElapsedSeconds, float TotalSeconds);
};
