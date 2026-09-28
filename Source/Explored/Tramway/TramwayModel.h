#pragma once

#include "CoreMinimal.h"
#include "Save/SaveValue.h"

/**
 * Raíles y vagones [F2] (GDD v2 §3.5, biblia 02 §9, biblia 03 §3.9). Modelo puro: vía
 * como grafo sobre rejilla y dinámica del vagón a lo largo de ella. Lo que no está en la
 * biblia (masa del vagón, fuerzas, radio de curva, tope) está marcado como propuesta en
 * el GDD.
 *
 * Rejilla: los nodos están en (X·2 m, Y·2 m, Z·0,125 m) sobre `Origin`. Un tramo une dos
 * nodos vecinos en una de las cuatro direcciones horizontales (2 m de avance) y puede
 * subir o bajar hasta `MaxRiseSteps` escalones de 0,125 m (17,4°). Cada nodo tiene como
 * mucho un tramo por dirección. El tipo de pieza sale de la forma del nodo: dos tramos
 * alineados son vía recta, dos perpendiculares son una curva (`rail_curvo`) y tres o
 * cuatro son un cambio de agujas (`cambio_agujas`).
 */

/** Direcciones horizontales de la rejilla: +X, +Y, −X, −Y. La opuesta de D es (D + 2) % 4. */
enum class ERailDir : uint8
{
	PosX,
	PosY,
	NegX,
	NegY,
	Count,
};

enum class ERailPlaceResult : uint8
{
	Ok,
	/** Los dos nodos son el mismo. */
	Degenerate,
	/** No son vecinos en una dirección de la rejilla. */
	NotAdjacent,
	/** Sube o baja más de MaxRiseSteps escalones. */
	TooSteep,
	/** Ya hay un tramo que sale de uno de los nodos en esa dirección. */
	Occupied,
};

enum class ERailNodeKind : uint8
{
	/** Sin tramos (o nodo inexistente). */
	None,
	/** Fin de vía: el vagón choca con el tope. */
	End,
	Straight,
	Curve,
	Switch,
};

enum class ECartPropulsion : uint8
{
	None,
	/** Un jugador empuja hacia la parte delantera (+1) o trasera (−1) del vagón. */
	Push,
	/** El torno más cercano en alcance de cuerda tira del vagón hacia él. */
	Winch,
};

enum class ECartDerailCause : uint8
{
	None,
	/** Curva tomada por encima de la velocidad de vuelco. */
	Curve,
	/** Tope de fin de vía golpeado demasiado rápido. */
	Buffer,
	/** Tramo marcado como dañado por una edición del terreno. */
	DamagedTrack,
	/** El tramo en el que estaba ya no existe. */
	MissingTrack,
};

struct EXPLORED_API FTramwaySettings
{
	FVector Origin = FVector::ZeroVector;
	float CellSize = 2.0f;
	float HeightStep = 0.125f;
	/** 5 × 0,125 m en 2 m: 17,4°. */
	int32 MaxRiseSteps = 5;
	float Gravity = 9.81f;
	/** Rodadura de ruedas de metal sobre raíl de hierro con traviesa de madera. */
	float RollingResistance = 0.02f;
	/** Coeficiente con el freno de zapata echado. */
	float BrakeFriction = 0.3f;
	float CartMassKg = 60.0f;
	/** Biblia 02 §9: 4× lo que carga un jugador a cuestas. */
	float CapacityKg = 200.0f;
	/** Biblia 02 §9: 1,2 m/s empujado a mano. */
	float PushSpeed = 1.2f;
	/** Fuerza sostenida de un jugador: sube el vagón lleno justo hasta 5°. */
	float PushForce = 280.0f;
	/** Biblia 02 §9: 2 m/s con el torno de cuerda. */
	float WinchSpeed = 2.0f;
	/** Sube el vagón lleno por la pendiente máxima (necesita 811 N). */
	float WinchForce = 900.0f;
	/** Cuerda del torno, medida por la vía. */
	float RopeLength = 60.0f;
	/** A esta distancia del torno el trinquete sujeta el vagón. */
	float WinchStopDistance = 0.5f;
	/** Radio efectivo de una curva de 90° en una celda de 2 m. */
	float CurveRadius = 1.0f;
	/** Media vía (m) y altura del centro de masas vacío y lleno: fijan la velocidad de vuelco en curva. */
	float HalfGauge = 0.3f;
	float EmptyCogHeight = 0.45f;
	float FullCogHeight = 0.7f;
	/** Por encima de esto el vagón vuelca contra el tope de fin de vía. */
	float BufferMaxSpeed = 2.5f;
	/** Pasos fijos de integración por segundo (deterministas con cualquier Dt de fotograma). */
	int32 SubstepsPerSecond = 120;

	double SubstepSeconds() const { return 1.0 / FMath::Max(1, SubstepsPerSecond); }
};

/**
 * Estado de un vagón. Está en el tramo dirigido From → To, a S metros de From; la
 * parte delantera mira hacia To. V es la velocidad con signo (+ hacia delante). Al
 * pasar de tramo se mantiene la orientación del vagón: si va marcha atrás, el nuevo
 * tramo se dirige de forma que la parte delantera siga mirando hacia donde venía.
 */
struct EXPLORED_API FMineCart
{
	FIntVector From = FIntVector(0, 0, 0);
	FIntVector To = FIntVector(0, 0, 0);
	double S = 0.0;
	double V = 0.0;
	float LoadKg = 0.0f;
	ECartDerailCause Derailed = ECartDerailCause::None;

	bool IsDerailed() const { return Derailed != ECartDerailCause::None; }
	bool operator==(const FMineCart& Other) const;
	bool operator!=(const FMineCart& Other) const { return !(*this == Other); }
};

struct EXPLORED_API FCartControl
{
	ECartPropulsion Propulsion = ECartPropulsion::None;
	/** Solo para Push: +1 empuja hacia delante, −1 hacia atrás. */
	int32 PushSign = 1;
	bool bBrake = false;
};

struct EXPLORED_API FCartStepResult
{
	/** Camino recorrido (siempre positivo). */
	double Distance = 0.0;
	/** Nodos cruzados. */
	int32 NodesCrossed = 0;
	/** Se ha parado contra un tope sin volcar. */
	bool bStoppedAtBuffer = false;
	/** Velocidad del último choque contra un tope o de la curva en la que volcó. */
	double ImpactSpeed = 0.0;
	/** Se pidió el torno, pero no hay ninguno en alcance de cuerda. */
	bool bNoWinchInReach = false;
	/** El trinquete del torno ha parado el vagón al llegar. */
	bool bArrivedAtWinch = false;
	ECartDerailCause Derailed = ECartDerailCause::None;
};

class EXPLORED_API FTramwayModel
{
public:
	explicit FTramwayModel(const FTramwaySettings& InSettings = FTramwaySettings());

	static FIntVector DirOffset(ERailDir Dir);
	static ERailDir Opposite(ERailDir Dir);

	const FTramwaySettings& GetSettings() const { return Settings; }
	FVector NodePosition(const FIntVector& Node) const;
	/** Nodo más cercano a un punto del mundo (redondeo, también en negativos). */
	FIntVector SnapNode(const FVector& World) const;
	/** Pendiente máxima de vía en grados. */
	float MaxSlopeDegrees() const;

	// --- Vía ---

	/** Comprueba sin tocar nada si se puede tender el tramo A–B. */
	ERailPlaceResult CanPlace(const FIntVector& A, const FIntVector& B) const;
	ERailPlaceResult Place(const FIntVector& A, const FIntVector& B);
	/** Quita el tramo A–B (en cualquier orden); false si no existía. */
	bool Remove(const FIntVector& A, const FIntVector& B);
	bool HasSegment(const FIntVector& A, const FIntVector& B) const;
	/** Tramo que sale de Node en Dir; false si no hay. */
	bool Neighbor(const FIntVector& Node, ERailDir Dir, FIntVector& OutOther) const;
	int32 NumSegments() const;
	int32 NumEdges(const FIntVector& Node) const;
	ERailNodeKind NodeKind(const FIntVector& Node) const;
	/** Longitud real (con la subida) del tramo A–B; 0 si no existe. */
	double SegmentLength(const FIntVector& A, const FIntVector& B) const;
	/** Seno de la pendiente al ir de A a B (positivo si sube). */
	double SegmentSin(const FIntVector& A, const FIntVector& B) const;

	// --- Cambios de agujas y tornos ---

	/** Apunta la palanca de un nodo con 3 o 4 tramos a la salida Dir. */
	bool SetSwitch(const FIntVector& Node, ERailDir Dir);
	/** Salida a la que apunta la palanca; Count si el nodo no es un cambio. */
	ERailDir GetSwitch(const FIntVector& Node) const;
	/**
	 * Salida que toma un vagón que llega a Node yendo en la dirección de avance Travel
	 * (es decir, que entra por Opposite(Travel)). Nunca da media vuelta. Si la palanca
	 * apunta a una salida válida, esa; si no, la recta; si no hay recta, la de menor índice.
	 * Devuelve false en un fin de vía.
	 */
	bool ExitFor(const FIntVector& Node, ERailDir Travel, ERailDir& OutExit) const;
	/** Pone un torno en un nodo con vía. */
	bool AddWinch(const FIntVector& Node);
	bool RemoveWinch(const FIntVector& Node);
	bool HasWinch(const FIntVector& Node) const;

	// --- Daños por edición del terreno (biblia 02 §9) ---

	/**
	 * Marca dañado todo tramo que pase a menos de Radius del centro (distancia al
	 * segmento que une sus nodos). Devuelve los tramos recién dañados como pares
	 * (A, B) con A < B, ordenados.
	 */
	TArray<TPair<FIntVector, FIntVector>> DamageInSphere(const FVector& Center, float Radius);
	bool IsDamaged(const FIntVector& A, const FIntVector& B) const;
	bool Repair(const FIntVector& A, const FIntVector& B);
	int32 NumDamaged() const;

	// --- Vagón ---

	/** Masa total con la carga. */
	double CartMass(const FMineCart& Cart) const;
	/** Cambia la carga; false (sin tocar nada) si pasa de la capacidad o es negativa. */
	bool SetLoad(FMineCart& Cart, float LoadKg) const;
	/** Pone el vagón parado sobre el tramo From → To; false si el tramo no existe. */
	bool PlaceCart(FMineCart& Cart, const FIntVector& From, const FIntVector& To, double S = 0.0) const;
	/** Velocidad a la que vuelca en curva con la carga actual. */
	double CurveSpeedLimit(const FMineCart& Cart) const;
	FVector CartPosition(const FMineCart& Cart) const;
	/**
	 * Avanza Dt segundos en pasos fijos de 1/SubstepsPerSecond. Lo que no llega a un
	 * paso se acumula en `Accumulator` (en pasos, no en segundos), así que 60 llamadas
	 * de 1/60 s dan lo mismo que una de 1 s. Se simula como mucho MaxStepSeconds por
	 * llamada, y un acumulador no finito o negativo se reinicia.
	 */
	FCartStepResult Step(FMineCart& Cart, const FCartControl& Control, double Dt, double& Accumulator) const;
	/** Un único paso fijo. */
	FCartStepResult Substep(FMineCart& Cart, const FCartControl& Control) const;
	/** Distancia por la vía hasta el torno más cercano y signo (+1 delante, −1 detrás). False si no hay en alcance. */
	bool WinchDirection(const FMineCart& Cart, double& OutDistance, int32& OutSign) const;

	// --- Guardado ---

	/**
	 * {"v":1,"seg":[AX,AY,AZ,Dir,Dz,Dañado,…],"sw":[X,Y,Z,Dir,…],"winch":[X,Y,Z,…]},
	 * en orden determinista. Solo se guarda cada tramo una vez (desde su nodo menor).
	 */
	FSaveValue ToValue() const;
	/** Sustituye el contenido; false (y queda vacío) si el valor no es válido. */
	bool FromValue(const FSaveValue& Value);
	static FSaveValue CartToValue(const FMineCart& Cart);
	static bool CartFromValue(const FSaveValue& Value, FMineCart& OutCart);

	bool IsEmpty() const { return Nodes.Num() == 0; }
	void Reset() { Nodes.Empty(); }
	bool operator==(const FTramwayModel& Other) const;
	bool operator!=(const FTramwayModel& Other) const { return !(*this == Other); }

	/** Sin tramo en esa dirección. */
	static constexpr int8 NoEdge = -128;
	/** Tope de tiempo simulado por llamada a Step. */
	static constexpr double MaxStepSeconds = 5.0;

private:
	struct FNode
	{
		/** Escalones de subida del tramo que sale en cada dirección; NoEdge si no hay. */
		int8 Rise[4] = { NoEdge, NoEdge, NoEdge, NoEdge };
		uint8 DamagedMask = 0;
		/** Salida a la que apunta la palanca (solo cuenta con 3 o 4 tramos). */
		int8 Lever = -1;
		bool bWinch = false;

		bool HasEdge(int32 D) const { return Rise[D] != NoEdge; }
		int32 NumEdges() const;
		bool IsUnused() const { return NumEdges() == 0 && !bWinch; }
		bool operator==(const FNode& O) const;
	};

	/** Dirección del tramo A → B; false si no son vecinos (sin mirar la pendiente). */
	static bool DirBetween(const FIntVector& A, const FIntVector& B, ERailDir& OutDir);
	static bool LessNode(const FIntVector& A, const FIntVector& B);
	TArray<FIntVector> SortedNodes() const;
	void SetDamaged(const FIntVector& A, const FIntVector& B, bool bDamaged);
	/** Velocidad tras un paso con la fuerza de propulsión dada (N, con signo). */
	double Integrate(double V, double Mass, double SinSlope, double Mu, double Force) const;

	FTramwaySettings Settings;
	TMap<FIntVector, FNode> Nodes;
};
