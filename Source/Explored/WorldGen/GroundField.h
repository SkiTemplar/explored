#pragma once

#include "CoreMinimal.h"

/** Tipo de suelo de un vértice del terreno (su color de paleta; Tools/Island/groundfield.py). */
enum class EGroundSurface : uint8
{
	Sea,
	Sand,
	Path,
	Dirt,
	Rock,
	Grass,
	Count
};

/** Resultado de FGroundField::Sample. Metros. */
struct EXPLORED_API FGroundSample
{
	float Height = 0.0f;
	/** Normal (unitaria, Z ≥ 0) del triángulo de la malla. */
	FVector3f Normal = FVector3f(0.0f, 0.0f, 1.0f);
	EGroundSurface Surface = EGroundSurface::Sea;
};

/**
 * Superficie exacta de SM_IslaSal_Terrain (Content/Data/ground_field.bin, lo escribe Tools/Island con
 * «uv run island-groundfield» o «island-terrain»): los vértices de la rejilla desplazada, la diagonal de
 * cada celda y el tipo de suelo por vértice. Sample() da la altura del triángulo que se dibuja, sin
 * trazas de física: FIslandLayout::SampleColumn es el relieve analítico y difiere hasta ~3,4 m de la
 * malla desde el relieve de F6, así que no sirve para apoyar nada en el suelo.
 *
 * Modelo puro (solo CoreMinimal): lo carga ExploredGround (GroundHeight.h) para el césped, el auditor,
 * el bot y las capturas, y lo prueban GrassFieldSpec y el host.
 */
class EXPLORED_API FGroundField
{
public:
	static constexpr uint32 Version = 1;
	/** Error admitido en los puntos de control al cargar (m): los dos lados usan float32. */
	static constexpr float ProbeTolerance = 0.005f;

	/** Lee el binario y comprueba sus puntos de control. False con el motivo si no es válido. */
	bool Parse(const TArray<uint8>& Bytes, FString& OutError);

	/**
	 * Construye el campo desde memoria (tests): Nx · Ny vértices (índice i · Ny + j), un tipo de suelo
	 * por vértice y una diagonal por celda ((Nx − 1) · (Ny − 1), índice ci · (Ny − 1) + cj).
	 */
	bool Init(int32 InNx, int32 InNy, const FVector2D& InMin, float InCell, TArray<FVector3f> InVerts,
		TArray<uint8> InSurface, TArray<uint8> InFlip, FString& OutError);

	bool IsValid() const { return Nx > 1 && Ny > 1; }
	int32 GetNx() const { return Nx; }
	int32 GetNy() const { return Ny; }
	int32 GetProbeCount() const { return ProbeCount; }

	/** Altura, normal y suelo en (X, Y) metros; false fuera de la rejilla. */
	bool Sample(double X, double Y, FGroundSample& Out) const;

private:
	bool SampleCell(int32 Ci, int32 Cj, double X, double Y, FGroundSample& Out) const;
	bool SampleTriangle(int32 A, int32 B, int32 C, double X, double Y, FGroundSample& Out) const;

	int32 Nx = 0;
	int32 Ny = 0;
	FVector2D Min = FVector2D::ZeroVector;
	float Cell = 1.0f;
	int32 ProbeCount = 0;
	TArray<FVector3f> Verts;
	TArray<uint8> Surface;
	TArray<uint8> Flip;
};
