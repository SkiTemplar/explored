#pragma once

#include "CoreMinimal.h"

/**
 * Ancla de un grupo de fauna de ambiente (banco, bandada, enjambre) que cada cliente
 * simula en local con la semilla del mundo (biblia 08 §2.7 a). La fauna de ambiente no se
 * replica pez a pez: el servidor manda, por grupo y cada 2 s, dónde está su centroide y
 * en qué estado va su cerebro, y el cliente arrastra su copia del grupo hacia ahí en 1 s.
 * `FMarineCreatureBrain` lleva `RandomCounter` interno y su historia diverge entre
 * máquinas: el ancla acota esa deriva sin exigir determinismo bit a bit.
 *
 * Formato de **10 bytes** por grupo: `uint16` id de grupo + 7 B de centroide
 * (`ExploredNet::WritePosition7`, el hueco de `FVector_NetQuantize100`) + `uint8` estado
 * del cerebro. Tope de 24 grupos por cliente dentro de 150 m: 24 × 10 B × 0,5 Hz =
 * 120 B/s = 0,96 kbps.
 */
struct EXPLORED_API FFaunaGroupAnchor
{
	uint16 GroupId = 0;
	FVector CentroidCm = FVector::ZeroVector;
	/** Estado del cerebro del grupo (EMarineState, EFishSchoolState, EBirdFlockState… según su clase). */
	uint8 BrainState = 0;

	bool operator==(const FFaunaGroupAnchor& Other) const
	{
		return GroupId == Other.GroupId && CentroidCm == Other.CentroidCm && BrainState == Other.BrainState;
	}
};

class EXPLORED_API FFaunaAnchorNetModel
{
public:
	static constexpr int32 PacketBytes = 10;
	static constexpr double SendIntervalSeconds = 2.0;
	static constexpr double RelevanceRadiusCm = 15000.0;
	static constexpr int32 MaxGroupsPerClient = 24;
	/** El cliente cierra la distancia al ancla en este tiempo. */
	static constexpr float PullSeconds = 1.0f;

	/** Añade los 10 bytes de un ancla al final de Out. */
	static void Append(const FFaunaGroupAnchor& Anchor, TArray<uint8>& Out);

	/**
	 * Lote de anclas para un cliente: sustituye Out. Devuelve false (y Out vacío) si pasa
	 * del tope de 24 o repite un id de grupo.
	 */
	static bool EncodeBatch(const TArray<FFaunaGroupAnchor>& Anchors, TArray<uint8>& Out);

	/**
	 * Lee un lote entero o nada: tamaño múltiplo de 10, como mucho 24 anclas y sin ids
	 * repetidos. Si falla, OutAnchors queda vacío.
	 */
	static bool DecodeBatch(const TArray<uint8>& In, TArray<FFaunaGroupAnchor>& OutAnchors);

	/** El ancla tal como llega al cliente. */
	static FFaunaGroupAnchor Quantize(const FFaunaGroupAnchor& Anchor);

	/**
	 * Grupos relevantes para un receptor: dentro de 150 m, del más cercano al más lejano
	 * (a igual distancia, por id de grupo), como mucho 24. Índices sobre Groups. Un
	 * centroide o un receptor no finitos no entran nunca.
	 */
	static void SelectForClient(const TArray<FFaunaGroupAnchor>& Groups, const FVector& ReceiverCm, TArray<int32>& OutIndices);

	/**
	 * ¿Toca mandar el ancla de este grupo entre PrevSeconds (excluido) y NowSeconds
	 * (incluido)? Cada grupo va cada 2 s, con una fase propia sacada de su id para que los
	 * 24 no salgan en el mismo tick.
	 */
	static bool IsDue(uint16 GroupId, double PrevSeconds, double NowSeconds);

	/** Fase de envío del grupo dentro del periodo de 2 s, en [0, 2). */
	static double SendPhaseSeconds(uint16 GroupId);

	/**
	 * Fracción de la distancia que queda hasta el ancla que el cliente cierra en este
	 * paso, para llegar justo al cumplirse PullSeconds desde que llegó el ancla. 1 si ya
	 * han pasado; 0 con un delta no válido.
	 */
	static float PullAlpha(float SecondsSinceAnchor, float DeltaSeconds);
};
