#pragma once

#include "CoreMinimal.h"
#include "Boats/RaftYardModel.h"
#include "WorldGen/SandModel.h"

/** Lo que ha hecho un arrastre sobre la arena. */
struct EXPLORED_API FRaftFurrowResult
{
	/** Traslados aplicados a la arena: chunks sucios, columnas cambiadas y masa movida. */
	FSandResult Sand;
	/** Columnas de arena bajo el fondo en algún punto del tramo arrastrado. */
	int32 FootprintColumns = 0;
	/** Presión media del fondo contra el suelo (kPa). */
	float PressureKPa = 0.0f;
	/** Hundimiento del surco en arena seca y en arena húmeda (mm) con esa presión. */
	int32 DrySinkMm = 0;
	int32 WetSinkMm = 0;
	/** Arena que no ha podido salir del surco (cordones llenos o estructuras a los dos lados). */
	int64 BlockedMass = 0;
};

/**
 * Surco de una balsa arrastrada por la arena viva (GDD v2 §3.17, «Surco en la arena»). Enlaza
 * FRaftYardModel con FSandModel sin que ninguno de los dos sepa del otro.
 *
 * - Solo marca el suelo una balsa en tierra que se arrastra sin rodillos por un
 *   tramo de arena (seca o mojada). Sobre rodillos, hierba, roca o la rampa de
 *   tablones no hay surco.
 * - Huella: las piezas del fondo (FRaftYardModel::IsBottomPiece), cada una con su
 *   caja en planta. Dos flotadores separados abren dos surcos.
 * - Hundimiento: la presión del fondo es el peso que no sostiene el agua entre el
 *   área de la huella. Se hunde DrySinkMmPerKPa por kPa en arena seca y
 *   WetSinkMmPerKPa en arena húmeda (bajo la pleamar o con lluvia), con un tope de
 *   MaxSinkMm. Por debajo de MinSinkMm no queda marca.
 * - El fondo rasa cada columna de la huella hasta −hundimiento respecto a la arena
 *   original: un montón en el camino se aplana y un hoyo más hondo que el surco no se
 *   toca. Por eso arrastrar dos veces por el mismo sitio no ahonda el surco.
 * - Masa: lo que sale del surco va a la primera columna fuera de la huella hacia el
 *   costado más cercano (el cordón). Si ese costado no admite más (tope del montón o
 *   huella de una estructura), prueba el otro; si ninguno puede, la arena se queda.
 *   Todo pasa por FSandModel::Transfer: la masa total no cambia nunca.
 * - La avalancha de FSandModel alisa después los cordones que pasen del ángulo de
 *   reposo, y el oleaje rellena el surco en la franja intermareal.
 * - Determinista: el tramo se muestrea en pasos de media celda, las columnas se
 *   recorren en orden (Y, X) y el costado de una columna centrada sale de su paridad.
 */
class EXPLORED_API FRaftFurrowModel
{
public:
	/** Hundimiento por kPa de presión (mm/kPa). La arena seca y suelta cede el triple que la mojada. */
	static constexpr float DrySinkMmPerKPa = 12.0f;
	static constexpr float WetSinkMmPerKPa = 4.0f;
	/** Tope del surco: con más carga la madera ya no se hunde, empuja la arena por delante. */
	static constexpr int32 MaxSinkMm = 60;
	/** Por debajo de esto no queda marca visible. */
	static constexpr int32 MinSinkMm = 3;
	/** Tope de muestras del tramo por llamada: un salto mayor se muestrea más espaciado. */
	static constexpr int32 MaxStations = 512;

	/** Hundimiento (mm) para una presión y un suelo. 0 si no deja marca. */
	static int32 SinkMm(float PressureKPa, bool bWet);
	/** Arena del camino de botadura que se deja marcar. */
	static bool IsSandSurface(ELaunchSurface Surface);

	/**
	 * Aplica a la arena el arrastre de la balsa de SFrom a STo (cm a lo largo del camino,
	 * los de antes y después de FRaftYardModel::Push). Llamar solo con el informe de un Push
	 * que no ha ido sobre rodillos: con rodillos debajo ahora, tampoco marca.
	 * La pleamar (m) y la lluvia deciden qué columnas son de arena húmeda.
	 */
	static FRaftFurrowResult Drag(const FRaftYardModel& Yard, float SFrom, float STo, double HighTide, bool bRaining,
		FSandModel& Sand, FSandModel::FBaseHeight Base);
};
