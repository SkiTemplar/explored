#pragma once

#include <atomic>

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/WorldSubsystem.h"

#include "Debug/TrailerModel.h"

#include "ExploredTrailerSubsystem.generated.h"

class ACameraActor;

/**
 * Grabación del tráiler (no se incluye en Shipping). Con «-ExploredTrailer=<trailer.json>» en la línea de
 * comandos del juego lee la línea de tiempo (FTrailerModel), fija el paso de tiempo a 1/fps y graba un PNG
 * por fotograma en «-TrailerDir» (por defecto Saved/Trailer/frames), con el nombre del índice global del
 * fotograma (f_00000.png), no del orden de llegada. Antes de cada plano deja asentar shaders, streaming,
 * texturas y césped con la cámara quieta en su primer fotograma, sin grabar.
 *
 * Opciones: «-TrailerShots=a,b» (solo esos planos; un «finale» va solo en su proceso, ver Tools/trailer.ps1),
 * «-TrailerStride=3» (ensayo: graba uno de cada 3 fotogramas sin cambiar el paso de tiempo).
 * Planos especiales: «finale» enciende la baliza en su primer fotograma y deja correr el final real del
 * juego (UBeaconSubsystem, FFinaleSequenceModel); «beam» enciende la baliza fuera de grabación y mantiene la
 * cámara del tráiler; la acción «hud» graba desde los ojos de Iria con la interfaz de juego.
 * Al acabar escribe report.json junto a los fotogramas y cierra el juego.
 */
UCLASS()
class EXPLORED_API UExploredTrailerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickable() const override { return State != EState::Off; }
	virtual TStatId GetStatId() const override;

private:
	enum class EState : uint8
	{
		Off,
		Warmup,
		Settling,
		Leading,
		Recording,
		Flushing,
	};

	bool LoadTimeline(const FString& Path, FString& OutError);
	void ApplyTrailerSettings() const;
	void BeginShot(int32 OrderIndex);
	void PrepareGameplay(const FTrailerShot& Shot);
	void PrepareHud(const FTrailerShot& Shot);
	/** Quita avisos y pensamientos que hayan salido durante el asentamiento (plano «hud»). */
	void ClearHudTransient() const;
	void ApplySample(const FTrailerShot& Shot, const FTrailerCameraSample& Sample);
	void RecordFrame();
	void TrackFinale(int32 GlobalFrame);
	bool IsWorldSettled() const;
	void OnScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void WriteReportAndExit();
	FTrailerGroundFn GroundFn() const;
	const FTrailerShot& CurrentShot() const { return Timeline.Shots[Order[OrderIndexNow]]; }

	FTrailerTimeline Timeline;
	FTrailerLimits Limits;
	TArray<int32> Order;
	int32 OrderIndexNow = INDEX_NONE;
	int32 LocalFrame = 0;
	int32 Stride = 1;
	float SettleSeconds = 4.0f;
	float Timer = 0.0f;
	EState State = EState::Off;
	FString OutputDir;
	bool bFading = false;
	FVector LastEyeCm = FVector::ZeroVector;
	float MaxStepCm = 0.0f;

	/** Fotograma global pedido y aún sin entregar (la captura llega en el Draw del mismo fotograma). */
	int32 PendingFrame = INDEX_NONE;
	int32 RequestedFrames = 0;
	TArray<int32> LostFrames;
	FCriticalSection WrittenLock;
	TArray<int32> WrittenFrames;
	TArray<int32> FailedFrames;
	std::atomic<int32> InFlight{0};
	FDelegateHandle ScreenshotHandle;

	/** Informe por plano (asentamiento real, salto máximo) y eventos del final, para report.json. */
	TArray<TSharedPtr<class FJsonValue>> ShotReports;
	TMap<FString, int32> FinaleFrames;
	TArray<FString> MotionIssues;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
