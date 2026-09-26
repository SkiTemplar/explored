#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Achievements/AchievementsModel.h"
#include "UI/ExploredGameplayMode.h"

#include "AchievementsSubsystem.generated.h"

/**
 * Capa fina de Unreal sobre FAchievementsModel (GDD §16): carga
 * Content/Data/achievements.json, recibe los eventos de estadística de
 * cualquier sistema y anuncia los logros nuevos.
 *
 * Qué informar y con qué ids: docs/tecnico/estadisticas.md.
 *
 * Contrato para el equipo de guardado (P-SAVE): GetSaveState() devuelve un
 * struct plano; el perfil guarda Profile y Unlocked, cada partida guarda Run y
 * RunMode. Al cargar, RestoreSaveState() y después EvaluatePending().
 */
UCLASS()
class EXPLORED_API UAchievementsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Atajo desde cualquier UObject con mundo; nullptr sin GameInstance (tests con mundo temporal). */
	static UAchievementsSubsystem* Get(const UObject* WorldContextObject);

	/** Vuelve a leer achievements.json. Conserva el estado (estadísticas y logros conseguidos). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Logros")
	bool ReloadFromDisk();

	/** Evento genérico: contador (+Amount), máximo (Amount) o marca. Ver docs/tecnico/estadisticas.md. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Logros")
	void ReportStat(FName Stat, double Amount = 1.0);

	/** Evento de conjunto: añade Item a la estadística Stat. */
	UFUNCTION(BlueprintCallable, Category = "Explored|Logros")
	void ReportStatItem(FName Stat, FName Item);

	/** Nueva partida: vacía las estadísticas de partida y fija el modo (restricciones como «Náufrago de verdad»). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Logros")
	void BeginRun(EExploredGameplayMode Mode);

	/** Revisa los logros pendientes (tras cargar una partida o actualizar los datos). */
	UFUNCTION(BlueprintCallable, Category = "Explored|Logros")
	void EvaluatePending();

	UFUNCTION(BlueprintPure, Category = "Explored|Logros")
	bool IsUnlocked(FName AchievementId) const;

	/** 0–1 para la barra de la lista de logros. */
	UFUNCTION(BlueprintPure, Category = "Explored|Logros")
	float GetProgress(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Logros")
	bool IsHidden(FName AchievementId) const;

	/** Nombre en el idioma de la interfaz (ES/EN). */
	UFUNCTION(BlueprintPure, Category = "Explored|Logros")
	FText GetDisplayName(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Explored|Logros")
	FText GetDescription(FName AchievementId) const;

	/** Ids en el orden de achievements.json. */
	UFUNCTION(BlueprintPure, Category = "Explored|Logros")
	TArray<FName> GetAchievementIds() const;

	const FAchievementsModel& GetModel() const { return Model; }
	const FAchievementsState& GetSaveState() const { return Model.GetState(); }
	void RestoreSaveState(const FAchievementsState& InState) { Model.RestoreState(InState); }

	/** Id de modo que usa achievements.json («Explorer», «Survivor», «Castaway»). */
	static FName ModeId(EExploredGameplayMode Mode);

	/** Parseo separado para poder validar el JSON en un Automation Spec sin GameInstance. */
	static bool ParseAchievementsJson(const FString& JsonText, TArray<FAchievementStatDef>& OutStats,
		TArray<FAchievementDef>& OutAchievements, FString& OutError);

	/** Se dispara una vez por logro, en el orden en que se consiguen. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAchievementUnlocked, FName /*AchievementId*/);
	FOnAchievementUnlocked OnAchievementUnlocked;

protected:
	/**
	 * Gancho de plataforma. La integración con Steam queda fuera de alcance
	 * (GDD §20): cuando exista, este es el único punto donde llamar a
	 * SetAchievement/StoreStats. Hoy no hace nada.
	 */
	virtual void NotifyPlatformAchievementUnlocked(FName AchievementId);

private:
	void Announce(const TArray<FName>& Unlocked);
	void WarnUnknownStat(FName Stat);
	bool UseEnglish() const;

	FAchievementsModel Model;
	TSet<FName> WarnedUnknownStats;
};
