#include "WorldGen/GroundField.h"

namespace GroundFieldDetail
{
	/** Cabecera: "EXGF", versión, nx, ny, min_x, min_y, cell, probes (8 × 4 bytes). */
	constexpr int32 HeaderBytes = 32;
	constexpr double BarycentricSlack = 1e-6;

	struct FReader
	{
		const TArray<uint8>& Bytes;
		int32 Offset = 0;

		bool Has(int64 Count) const { return Offset + Count <= Bytes.Num(); }

		uint32 U32()
		{
			const uint32 V = static_cast<uint32>(Bytes[Offset]) | (static_cast<uint32>(Bytes[Offset + 1]) << 8)
				| (static_cast<uint32>(Bytes[Offset + 2]) << 16) | (static_cast<uint32>(Bytes[Offset + 3]) << 24);
			Offset += 4;
			return V;
		}

		float F32()
		{
			const uint32 Bits = U32();
			float V = 0.0f;
			FMemory::Memcpy(&V, &Bits, sizeof(V));
			return V;
		}
	};
}

bool FGroundField::Init(int32 InNx, int32 InNy, const FVector2D& InMin, float InCell, TArray<FVector3f> InVerts,
	TArray<uint8> InSurface, TArray<uint8> InFlip, FString& OutError)
{
	const int64 VertCount = static_cast<int64>(InNx) * InNy;
	if (InNx < 2 || InNy < 2 || VertCount > 16 * 1024 * 1024 || !(InCell > 0.0f) || !FMath::IsFinite(InCell))
	{
		OutError = FString::Printf(TEXT("rejilla no válida (%d × %d, celda %.3f)"), InNx, InNy, InCell);
		return false;
	}
	if (InVerts.Num() != VertCount || InSurface.Num() != VertCount || InFlip.Num() != (InNx - 1) * (InNy - 1))
	{
		OutError = TEXT("tamaños de vértices, suelos o diagonales incoherentes");
		return false;
	}
	for (const FVector3f& V : InVerts)
	{
		if (!FMath::IsFinite(V.X) || !FMath::IsFinite(V.Y) || !FMath::IsFinite(V.Z))
		{
			OutError = TEXT("vértice no finito");
			return false;
		}
	}
	for (const uint8 S : InSurface)
	{
		if (S >= static_cast<uint8>(EGroundSurface::Count))
		{
			OutError = FString::Printf(TEXT("tipo de suelo %d desconocido"), S);
			return false;
		}
	}
	Nx = InNx;
	Ny = InNy;
	Min = InMin;
	Cell = InCell;
	Verts = MoveTemp(InVerts);
	Surface = MoveTemp(InSurface);
	Flip = MoveTemp(InFlip);
	return true;
}

bool FGroundField::Parse(const TArray<uint8>& Bytes, FString& OutError)
{
	using namespace GroundFieldDetail;
	FReader R{Bytes};
	if (!R.Has(HeaderBytes) || Bytes[0] != 'E' || Bytes[1] != 'X' || Bytes[2] != 'G' || Bytes[3] != 'F')
	{
		OutError = TEXT("no es un ground_field (cabecera EXGF)");
		return false;
	}
	R.Offset = 4;
	const uint32 FileVersion = R.U32();
	const int32 InNx = static_cast<int32>(R.U32());
	const int32 InNy = static_cast<int32>(R.U32());
	const float MinX = R.F32();
	const float MinY = R.F32();
	const float InCell = R.F32();
	const int32 Probes = static_cast<int32>(R.U32());
	if (FileVersion != Version || InNx < 2 || InNy < 2 || InNx > 4096 || InNy > 4096 || Probes < 0 || Probes > 65536)
	{
		OutError = FString::Printf(TEXT("versión %u o tamaños no válidos (%d × %d, %d controles)"), FileVersion, InNx, InNy, Probes);
		return false;
	}
	const int64 VertCount = static_cast<int64>(InNx) * InNy;
	const int64 CellCount = static_cast<int64>(InNx - 1) * (InNy - 1);
	if (Bytes.Num() != HeaderBytes + VertCount * 12 + VertCount + CellCount + static_cast<int64>(Probes) * 12)
	{
		OutError = FString::Printf(TEXT("%d bytes no cuadran con la cabecera"), Bytes.Num());
		return false;
	}
	TArray<FVector3f> InVerts;
	InVerts.SetNum(static_cast<int32>(VertCount));
	for (FVector3f& V : InVerts)
	{
		V.X = R.F32();
		V.Y = R.F32();
		V.Z = R.F32();
	}
	TArray<uint8> InSurface(Bytes.GetData() + R.Offset, static_cast<int32>(VertCount));
	R.Offset += static_cast<int32>(VertCount);
	TArray<uint8> InFlip(Bytes.GetData() + R.Offset, static_cast<int32>(CellCount));
	R.Offset += static_cast<int32>(CellCount);

	FGroundField Candidate;
	if (!Candidate.Init(InNx, InNy, FVector2D(MinX, MinY), InCell, MoveTemp(InVerts), MoveTemp(InSurface), MoveTemp(InFlip), OutError))
	{
		return false;
	}
	for (int32 P = 0; P < Probes; ++P)
	{
		const float X = R.F32();
		const float Y = R.F32();
		const float Z = R.F32();
		FGroundSample Hit;
		if (!Candidate.Sample(X, Y, Hit) || FMath::Abs(Hit.Height - Z) > ProbeTolerance)
		{
			OutError = FString::Printf(TEXT("el control %d (%.2f, %.2f) da %.3f m y el fichero %.3f m"), P, X, Y, Hit.Height, Z);
			return false;
		}
	}
	Candidate.ProbeCount = Probes;
	*this = MoveTemp(Candidate);
	return true;
}

bool FGroundField::Sample(double X, double Y, FGroundSample& Out) const
{
	if (!IsValid() || !FMath::IsFinite(X) || !FMath::IsFinite(Y))
	{
		return false;
	}
	const double Fx = (X - Min.X) / Cell;
	const double Fy = (Y - Min.Y) / Cell;
	if (Fx < -1.0 || Fy < -1.0 || Fx > Nx || Fy > Ny)
	{
		return false;
	}
	const int32 Ci = FMath::FloorToInt32(Fx);
	const int32 Cj = FMath::FloorToInt32(Fy);
	// Mismo orden que groundfield.sample (Python): la celda propia y luego las vecinas, porque los
	// vértices interiores están desplazados hasta un 12 % de celda.
	static constexpr int32 Order[3] = {0, -1, 1};
	for (const int32 Di : Order)
	{
		for (const int32 Dj : Order)
		{
			if (SampleCell(Ci + Di, Cj + Dj, X, Y, Out))
			{
				return true;
			}
		}
	}
	return false;
}

bool FGroundField::SampleCell(int32 Ci, int32 Cj, double X, double Y, FGroundSample& Out) const
{
	if (Ci < 0 || Cj < 0 || Ci >= Nx - 1 || Cj >= Ny - 1)
	{
		return false;
	}
	const int32 V00 = Ci * Ny + Cj;
	const int32 V10 = (Ci + 1) * Ny + Cj;
	const int32 V11 = (Ci + 1) * Ny + Cj + 1;
	const int32 V01 = Ci * Ny + Cj + 1;
	if (Flip[Ci * (Ny - 1) + Cj])
	{
		return SampleTriangle(V00, V10, V11, X, Y, Out) || SampleTriangle(V00, V11, V01, X, Y, Out);
	}
	return SampleTriangle(V00, V10, V01, X, Y, Out) || SampleTriangle(V10, V11, V01, X, Y, Out);
}

bool FGroundField::SampleTriangle(int32 A, int32 B, int32 C, double X, double Y, FGroundSample& Out) const
{
	const FVector3f& P0 = Verts[A];
	const FVector3f& P1 = Verts[B];
	const FVector3f& P2 = Verts[C];
	const double X0 = P0.X, Y0 = P0.Y, X1 = P1.X, Y1 = P1.Y, X2 = P2.X, Y2 = P2.Y;
	const double Det = (Y1 - Y2) * (X0 - X2) + (X2 - X1) * (Y0 - Y2);
	if (FMath::Abs(Det) < 1e-12)
	{
		return false;
	}
	const double L0 = ((Y1 - Y2) * (X - X2) + (X2 - X1) * (Y - Y2)) / Det;
	const double L1 = ((Y2 - Y0) * (X - X2) + (X0 - X2) * (Y - Y2)) / Det;
	const double L2 = 1.0 - L0 - L1;
	if (FMath::Min3(L0, L1, L2) < -GroundFieldDetail::BarycentricSlack)
	{
		return false;
	}
	Out.Height = static_cast<float>(L0 * P0.Z + L1 * P1.Z + L2 * P2.Z);
	FVector3f N = FVector3f::CrossProduct(P1 - P0, P2 - P0);
	if (N.Z < 0.0f)
	{
		N = -N;
	}
	Out.Normal = N.GetSafeNormal(1e-8f, FVector3f(0.0f, 0.0f, 1.0f));
	const int32 Nearest = L0 >= L1 && L0 >= L2 ? A : (L1 >= L2 ? B : C);
	Out.Surface = static_cast<EGroundSurface>(Surface[Nearest]);
	return true;
}
