#pragma once

#include "CoreMinimal.h"

#include "Boats/BoatTypes.h"

/**
 * Viento para navegar a vela. FWeatherSample solo da la fuerza (0–1); la
 * dirección es la del alisio de cada estación, con un giro lento y
 * determinista a lo largo del día. Funciones puras.
 */
struct EXPLORED_API FBoatWind
{
	/** Velocidad del viento (m/s) para FWeatherSample::Wind: despejado ≈ 6.5, galerna ≈ 23, ciclón ≈ 26.5. */
	static float SpeedMS(float Wind01);

	/** Rumbo de brújula DESDE el que sopla (°): alisio del ESE en la seca, del oeste en el monzón. */
	static float PrevailingFromDeg(float TotalDays);

	/** Vector de viento verdadero (cm/s, mundo, hacia donde sopla). */
	static FVector2D VelocityCmS(float Wind01, float FromDeg);
};

/** Ayudas de navegación nocturna por caminos de estrellas (GDD §6.3). Rumbos de brújula: 0 norte (+X), 90 este (+Y). */
struct EXPLORED_API FBoatNavigation
{
	/** Rumbo de brújula de A hacia B (°, [0, 360)). */
	static float BearingDeg(const FVector2D& From, const FVector2D& To);

	/** Diferencia firmada Target − Heading en (-180, 180]: positiva = hay que caer a estribor. */
	static float HeadingErrorDeg(float HeadingDeg, float TargetBearingDeg);

	/** Lectura completa frente a un camino de estrellas; bOnCourse si proa y rumbo real están dentro de la tolerancia. */
	static FNightNavigationReading Evaluate(const FBoatState& State, const FStarPath& Path, float ToleranceDeg = 10.0f);
};

/**
 * Modelo puro de una embarcación (GDD §8.10): flotación sobre las olas de
 * Gerstner, remo, vela con viento aparente, corrientes, marea, varadas y
 * vuelcos. Integra con paso fijo (FixedStepS) para dar el mismo resultado
 * con cualquier ritmo de fotogramas; AExploredBoat solo copia el estado a
 * su transformación.
 *
 * Flotación: se muestrea el agua en proa, popa, babor, estribor y centro.
 * La media fija la altura de equilibrio (flotación − calado), la diferencia
 * proa-popa el cabeceo y la de babor-estribor el balance. Cada grado de
 * libertad es un oscilador amortiguado hacia su objetivo cuya rigidez es
 * la de la flotación real (ρ·g·Área de flotación / masa).
 *
 * Propulsión: las paladas empujan y hacen guiñar hacia la banda contraria
 * (alternarlas mantiene el rumbo); la vela empuja según una polar sobre el
 * ángulo del viento verdadero (sin avance por debajo de ~45°, máximo de
 * través) con la fuerza del viento aparente, y el trimado óptimo depende
 * del ángulo del viento aparente. La fuerza lateral produce abatimiento y
 * escora. El casco se frena contra el AGUA, así que la corriente arrastra.
 */
class EXPLORED_API FBoatModel
{
public:
	/** Paso fijo de integración (s). */
	static constexpr float FixedStepS = 1.0f / 60.0f;
	/** Un tirón del juego mayor que esto se recorta (evita la espiral de pasos). */
	static constexpr float MaxFrameS = 1.0f;
	static constexpr float WaterDensity = 1025.0f;
	static constexpr float AirDensity = 1.225f;
	static constexpr float Gravity = 9.81f;
	static constexpr float CrewMassKg = 75.0f;
	/** Media amplitud de la marea viva (cm) para TideOffsetCm. */
	static constexpr float TideAmplitudeCm = 90.0f;
	/** Tolerancia de la quilla sobre el fondo antes de considerar que choca (cm). */
	static constexpr float GroundingToleranceCm = 3.0f;
	/** Velocidad de choque contra el fondo a partir de la cual se daña el casco (cm/s). */
	static constexpr float SafeImpactSpeedCmS = 80.0f;
	/** Tope de la arrancada impuesta desde fuera (50 m/s, muy por encima de cualquier barco). */
	static constexpr float MaxSetSpeedCmS = 5000.0f;

	static const FBoatDefinition& Definition(EBoatType Type);

	FBoatModel() : FBoatModel(EBoatType::Raft, FVector::ZeroVector, 0.0f) {}
	FBoatModel(EBoatType InType, const FVector& InLocationCm, float InYawDeg);
	/**
	 * Con una ficha arbitraria: la de un casco armado por piezas
	 * (FHullAssemblyModel::ToBoatDefinition). Type solo decide el nombre y el
	 * guardado; la física sale de la ficha.
	 */
	FBoatModel(const FBoatDefinition& InDefinition, const FVector& InLocationCm, float InYawDeg);

	/** Cambia la ficha sin tocar el estado (una unión rota ha soltado una pieza). Recorta la carga a la nueva capacidad. */
	void SetDefinition(const FBoatDefinition& InDefinition);

	/**
	 * Velocidad sobre el fondo (cm/s): la arrancada con la que sale de la botadura. Una no finita
	 * lo deja quieto y una enorme se recorta a MaxSetSpeedCmS (la física daría NaN al paso siguiente).
	 */
	void SetVelocityCmS(const FVector2D& VelocityCmS)
	{
		if (!FMath::IsFinite(VelocityCmS.X) || !FMath::IsFinite(VelocityCmS.Y))
		{
			State.VelocityCmS = FVector2D::ZeroVector;
			return;
		}
		// Size() de un vector enorme puede ser infinito: entonces el factor es 0 y queda quieto.
		const double Speed = VelocityCmS.Size();
		State.VelocityCmS = Speed > MaxSetSpeedCmS ? VelocityCmS * (MaxSetSpeedCmS / Speed) : VelocityCmS;
	}

	/**
	 * Amarra a un poste o muelle en AnchorCm con un cabo de LengthCm. False si el
	 * punto de amarre queda más lejos que el cabo, si el cabo no tiene largo o si
	 * el barco está destrozado. Amarrado sigue cabeceando y balanceándose con las
	 * olas, pero la deriva no lo aleja más que el cabo.
	 */
	bool Moor(const FVector2D& AnchorCm, float LengthCm);
	/** Suelta el amarre. */
	void CastOff() { State.bMoored = false; State.bMooringTaut = false; }
	bool IsMoored() const { return State.bMoored; }

	/** Avanza DeltaSeconds (se acumula y se integra a paso fijo). */
	void Step(float DeltaSeconds, const FBoatControls& Controls, const FBoatEnvironment& Environment);

	/** Da una palada por una banda. False si ya está remando, no hay tripulante o el barco no navega. */
	bool TryStroke(EBoatSide Side);
	bool IsStroking() const { return State.StrokeTimeLeftS > 0.0f; }

	/** Iza o arría la vela. False si no tiene vela o está volcado/hundido. */
	bool SetSailRaised(bool bRaised);

	void SetCrewAboard(bool bAboard) { State.bCrewAboard = bAboard; }

	/** Carga (kg). False si supera MaxCargoKg: nunca se sobrecarga hasta hundirlo. */
	bool TryAddCargo(float Kg);
	/** Descarga hasta Kg; devuelve lo descargado. */
	float RemoveCargo(float Kg);

	/** Adrizar un barco volcado: queda anegado (o a flote si es la balsa). False si no está volcado. */
	bool TryRight();

	/** Repara una fracción del daño del casco (0–1). */
	void Repair(float Amount01);

	/** Daño directo (ciclón, golpe de otro sistema). */
	void ApplyDamage(float Amount01);

	const FBoatState& GetState() const { return State; }
	const FBoatDefinition& GetDefinition() const { return Def; }

	/** Masa total a bordo: casco, tripulante, carga y agua embarcada (kg). */
	float TotalMassKg() const;
	/** Calado de equilibrio en agua quieta con la carga actual (cm). */
	float EquilibriumDraftCm() const;
	/** Agua embarcada con la que la borda queda a ras y se anega (kg). */
	float SwampWaterKg() const;
	/** Velocidad sobre el fondo (cm/s). */
	float SpeedCmS() const { return State.VelocityCmS.Size(); }

	/** Ángulo del viento verdadero respecto a la proa (0 = de proa, 180 = de popa) para un rumbo. */
	static float TrueWindAngleDeg(float YawDeg, const FVector2D& WindCmS);
	/** Coeficiente de empuje de la polar: 0 en la zona muerta (< ~45°), máximo de través. */
	static float SailDriveCoefficient(float TrueWindAngleDeg);
	/** Escota óptima (0–1) para un ángulo de viento aparente. */
	static float OptimalSailTrim01(float ApparentWindAngleDeg);
	/** Rendimiento de la vela (0–1) con una escota dada y un viento aparente. */
	static float SailTrimEfficiency(float SailTrim01, float ApparentWindAngleDeg);

	/** Subida del mar por la marea (cm) para un instante y fase lunar (FOceanTide). */
	static float TideOffsetCm(float TotalDays, float MoonPhase01);

	/** Último viento aparente calculado (cm/s, mundo) y su ángulo respecto a la proa. */
	FVector2D GetApparentWindCmS() const { return LastApparentWindCmS; }
	float GetApparentWindAngleDeg() const { return LastApparentWindAngleDeg; }
	/** Abatimiento: ángulo entre la proa y el rumbo sobre el agua (°, positivo hacia estribor). */
	float GetLeewayDeg() const { return LastLeewayDeg; }
	/** Máxima escora alcanzada desde la construcción o la última carga (°, valor absoluto). */
	float GetMaxAbsRollDeg() const { return MaxAbsRollDeg; }

	FBoatSaveData ToSaveData() const;
	/** Con CustomDefinition, la ficha del casco por piezas que se reconstruye aparte (el astillero guarda las piezas). */
	static FBoatModel FromSaveData(const FBoatSaveData& Data, const FBoatDefinition* CustomDefinition = nullptr);

private:
	void Substep(float H, float WaveTime, const FBoatControls& Controls, const FBoatEnvironment& Environment);

	FBoatState State;
	FBoatDefinition Def;
	FVector2D LastApparentWindCmS = FVector2D::ZeroVector;
	float LastApparentWindAngleDeg = 0.0f;
	float LastLeewayDeg = 0.0f;
	float MaxAbsRollDeg = 0.0f;
};
