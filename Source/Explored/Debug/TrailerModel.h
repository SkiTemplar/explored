#pragma once

#include "CoreMinimal.h"

/**
 * Tráiler de Baliza Siete (Tools/Trailer/trailer.json, Tools/trailer.ps1): línea de tiempo de planos y
 * cámara de cada fotograma a tiempo fijo. Modelo puro (solo CoreMinimal): lo usa
 * UExploredTrailerSubsystem (no se incluye en Shipping) y lo prueban TrailerSpec y el host.
 *
 * Unidades: metros y grados; +X norte, +Y este (island.json). Cada plano tiene puntos de ojo (EyeM) y de
 * mirada (LookM):
 * - El ojo recorre un Catmull-Rom por sus puntos, reparametrizado por longitud de arco (velocidad
 *   constante), con un perfil de velocidad trapezoidal: acelera y frena durante EaseFraction del plano.
 * - Mirada: con un punto, la cámara lo sigue desde donde esté el ojo; con dos, la dirección pasa de la
 *   primera a la segunda por slerp con el mismo perfil; con más, Catmull-Rom de las direcciones
 *   (normalizado), repartidas a partes iguales en el plano.
 * - Con bFollowGround, la Z del ojo es altura sobre el suelo (media de la malla en un círculo de
 *   GroundSmoothRadiusM, para que el relieve no haga temblar la cámara). Siempre se respeta
 *   MinClearanceM sobre el suelo bajo el ojo.
 */

/** Tipo de plano: normal, el final del juego reproduciéndose de verdad o el haz encendido (título). */
enum class ETrailerShotKind : uint8
{
	Normal,
	Finale,
	Beam,
};

struct EXPLORED_API FTrailerShot
{
	FString Name;
	/** Bloque del guion al que pertenece (lo usa el montaje para los rótulos). */
	FString Section;
	float DurationSeconds = 1.0f;
	int32 Day = 1;
	float HoursFrom = 12.0f;
	float HoursTo = 12.0f;
	float FovFrom = 70.0f;
	float FovTo = 70.0f;
	float FadeInSeconds = 0.0f;
	float FadeOutSeconds = 0.0f;
	/** Fracción del plano (0-0,5) que dura la aceleración y, otra igual, la frenada. */
	float EaseFraction = 0.2f;
	bool bFollowGround = false;
	bool bShowUI = false;
	ETrailerShotKind Kind = ETrailerShotKind::Normal;
	/** Preparación de juego del plano («hud»: interfaz con barras y objetivo). */
	FString Action;
	TArray<FVector> EyeM;
	TArray<FVector> LookM;

	/** Los rellena BuildTimeline. */
	int32 StartFrame = 0;
	int32 FrameCount = 0;
};

struct EXPLORED_API FTrailerTimeline
{
	TArray<FTrailerShot> Shots;
	int32 Fps = 30;
	int32 TotalFrames = 0;
};

/** Cámara de un fotograma. */
struct EXPLORED_API FTrailerCameraSample
{
	FVector EyeM = FVector::ZeroVector;
	/** Dirección de la mirada (unitaria). */
	FVector LookDir = FVector(1.0, 0.0, 0.0);
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	float Fov = 70.0f;
	int32 Day = 1;
	float Hours = 12.0f;
	/** Negro del fundido: 0 imagen limpia, 1 negro total. */
	float Black = 0.0f;
};

/** Umbrales de la comprobación de movimiento (por fotograma, dentro de un plano). */
struct EXPLORED_API FTrailerLimits
{
	float MaxEyeStepM = 1.0f;
	float MaxTurnStepDeg = 3.0f;
	/** Cambio máximo de velocidad del ojo entre dos fotogramas (m por fotograma): tirones. */
	float MaxEyeAccelM = 0.08f;
	float MinClearanceM = 0.4f;
};

/** Altura del suelo (m) en (X, Y) metros. */
using FTrailerGroundFn = TFunction<float(double, double)>;

struct EXPLORED_API FTrailerModel
{
	static constexpr float GroundSmoothRadiusM = 1.5f;
	static constexpr float MinClearanceM = 0.4f;

	static ETrailerShotKind ParseKind(const FString& Text);
	static const TCHAR* KindName(ETrailerShotKind Kind);

	/** Comprueba los planos y les asigna fotogramas (Round(duración × fps)). False con el motivo. */
	static bool BuildTimeline(const TArray<FTrailerShot>& Shots, int32 Fps, FTrailerTimeline& Out, FString& OutError);
	/** Índice del plano que contiene el fotograma global; INDEX_NONE fuera de la línea de tiempo. */
	static int32 FindShot(const FTrailerTimeline& Timeline, int32 Frame);
	static int32 FindShotByName(const FTrailerTimeline& Timeline, const FString& Name);

	/** Progreso [0, 1] con perfil de velocidad trapezoidal (EaseFraction en [0, 0,5]; 0 = lineal). */
	static float EaseProgress(float Alpha, float EaseFraction);
	/** Punto del Catmull-Rom centrípeto por Points al parámetro U en [0, Num − 1]. */
	static FVector CatmullRom(const TArray<FVector>& Points, double U);
	/** Punto a la fracción S de la longitud de arco del Catmull-Rom (S en [0, 1]). */
	static FVector PathPoint(const TArray<FVector>& Points, double S);
	/** Slerp de dos direcciones (se normalizan); T en [0, 1]. */
	static FVector SlerpDirection(const FVector& A, const FVector& B, double T);
	/** Media del suelo en el punto y en 8 puntos de un círculo de RadiusM. */
	static float SmoothedGround(const FTrailerGroundFn& Ground, double X, double Y, double RadiusM);
	/** Negro del fundido del fotograma que empieza a LocalSeconds y dura FrameSeconds. */
	static float FadeBlack(const FTrailerShot& Shot, float LocalSeconds, float FrameSeconds = 0.0f);

	/** Cámara del fotograma LocalFrame (0 .. FrameCount − 1) del plano. */
	static FTrailerCameraSample Evaluate(const FTrailerShot& Shot, int32 LocalFrame, int32 Fps, const FTrailerGroundFn& Ground);

	/**
	 * Recorre todos los fotogramas del plano y devuelve los problemas: saltos del ojo o giros mayores que
	 * los límites entre fotogramas seguidos, tirones de velocidad y el ojo por debajo del suelo. Vacío = bien.
	 */
	static TArray<FString> CheckMotion(const FTrailerShot& Shot, int32 Fps, const FTrailerGroundFn& Ground, const FTrailerLimits& Limits);

	/**
	 * Orden de captura dentro de un proceso: normales, luego los de interfaz (activan la partida) y el haz
	 * el último (arranca el final del juego). Un plano «finale» solo se puede capturar solo: false si hay mezcla.
	 */
	static bool CaptureOrder(const FTrailerTimeline& Timeline, const TArray<int32>& Selected, TArray<int32>& OutOrder, FString& OutError);

	/** Nombre del fichero de un fotograma: f_00000.png. */
	static FString FrameFileName(int32 Frame);
	/** Fotogramas de [First, Last] que faltan y los que aparecen más de una vez en Captured. */
	static void FindGaps(const TArray<int32>& Captured, int32 First, int32 Last, int32 Stride, TArray<int32>& OutMissing, TArray<int32>& OutRepeated);
};
