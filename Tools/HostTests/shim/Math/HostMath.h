// Tipos matemáticos del shim (ver CoreMinimal.h). FVector/FVector2D en double como en UE5;
// FVector3f/FVector2f en float. Solo lo que usan los modelos puros.
#pragma once

template <typename T>
struct TVector2
{
	T X = 0, Y = 0;

	static const TVector2 ZeroVector;
	static const TVector2 UnitVector;

	constexpr TVector2() = default;
	constexpr TVector2(T InX, T InY) : X(InX), Y(InY) {}
	explicit constexpr TVector2(T V) : X(V), Y(V) {}
	explicit TVector2(EForceInit) : X(0), Y(0) {}
	template <typename U, typename = std::enable_if_t<!std::is_same_v<T, U>>>
	explicit TVector2(const TVector2<U>& O) : X((T)O.X), Y((T)O.Y) {}

	TVector2 operator+(const TVector2& O) const { return {X + O.X, Y + O.Y}; }
	TVector2 operator-(const TVector2& O) const { return {X - O.X, Y - O.Y}; }
	TVector2 operator*(const TVector2& O) const { return {X * O.X, Y * O.Y}; }
	TVector2 operator/(const TVector2& O) const { return {X / O.X, Y / O.Y}; }
	TVector2 operator*(T S) const { return {X * S, Y * S}; }
	TVector2 operator/(T S) const { return {X / S, Y / S}; }
	TVector2 operator+(T S) const { return {X + S, Y + S}; }
	TVector2 operator-(T S) const { return {X - S, Y - S}; }
	TVector2 operator-() const { return {-X, -Y}; }
	TVector2& operator+=(const TVector2& O) { X += O.X; Y += O.Y; return *this; }
	TVector2& operator-=(const TVector2& O) { X -= O.X; Y -= O.Y; return *this; }
	TVector2& operator*=(T S) { X *= S; Y *= S; return *this; }
	TVector2& operator/=(T S) { X /= S; Y /= S; return *this; }
	TVector2& operator*=(const TVector2& O) { X *= O.X; Y *= O.Y; return *this; }
	bool operator==(const TVector2& O) const { return X == O.X && Y == O.Y; }
	bool operator!=(const TVector2& O) const { return !(*this == O); }
	T operator|(const TVector2& O) const { return X * O.X + Y * O.Y; }
	T operator^(const TVector2& O) const { return X * O.Y - Y * O.X; }
	T& operator[](int32 I) { return I == 0 ? X : Y; }
	T operator[](int32 I) const { return I == 0 ? X : Y; }

	T Size() const { return std::sqrt(X * X + Y * Y); }
	T Length() const { return Size(); }
	T SizeSquared() const { return X * X + Y * Y; }
	T SquaredLength() const { return SizeSquared(); }
	bool IsNearlyZero(T Tol = (T)UE_KINDA_SMALL_NUMBER) const { return FMath::Abs(X) <= Tol && FMath::Abs(Y) <= Tol; }
	bool IsZero() const { return X == 0 && Y == 0; }
	bool Equals(const TVector2& O, T Tol = (T)UE_KINDA_SMALL_NUMBER) const { return FMath::Abs(X - O.X) <= Tol && FMath::Abs(Y - O.Y) <= Tol; }
	TVector2 GetSafeNormal(T Tol = (T)UE_SMALL_NUMBER) const
	{
		const T S = SizeSquared();
		if (S > Tol) { const T Inv = (T)1 / std::sqrt(S); return {X * Inv, Y * Inv}; }
		return {0, 0};
	}
	void Normalize(T Tol = (T)UE_SMALL_NUMBER) { *this = GetSafeNormal(Tol); }
	TVector2 GetRotated(T AngleDeg) const
	{
		const T R = FMath::DegreesToRadians(AngleDeg);
		const T C = std::cos(R), S = std::sin(R);
		return {C * X - S * Y, S * X + C * Y};
	}
	TVector2 GetAbs() const { return {FMath::Abs(X), FMath::Abs(Y)}; }
	T GetMax() const { return FMath::Max(X, Y); }
	T GetMin() const { return FMath::Min(X, Y); }
	TVector2 ClampAxes(T Lo, T Hi) const { return {FMath::Clamp(X, Lo, Hi), FMath::Clamp(Y, Lo, Hi)}; }
	TVector2 ComponentMin(const TVector2& O) const { return {FMath::Min(X, O.X), FMath::Min(Y, O.Y)}; }
	TVector2 ComponentMax(const TVector2& O) const { return {FMath::Max(X, O.X), FMath::Max(Y, O.Y)}; }

	static T DotProduct(const TVector2& A, const TVector2& B) { return A | B; }
	static T CrossProduct(const TVector2& A, const TVector2& B) { return A ^ B; }
	static T Distance(const TVector2& A, const TVector2& B) { return (A - B).Size(); }
	static T DistSquared(const TVector2& A, const TVector2& B) { return (A - B).SizeSquared(); }
	static TVector2 Min(const TVector2& A, const TVector2& B) { return A.ComponentMin(B); }
	static TVector2 Max(const TVector2& A, const TVector2& B) { return A.ComponentMax(B); }
};
template <typename T> const TVector2<T> TVector2<T>::ZeroVector(0, 0);
template <typename T> const TVector2<T> TVector2<T>::UnitVector(1, 1);
template <typename T> inline TVector2<T> operator*(T S, const TVector2<T>& V) { return V * S; }
template <typename T> inline TVector2<T> operator*(float S, const TVector2<T>& V) requires(!std::is_same_v<T, float>) { return V * (T)S; }
template <typename T> inline TVector2<T> operator*(const TVector2<T>& V, float S) requires(!std::is_same_v<T, float>) { return V * (T)S; }
template <typename T> inline TVector2<T> operator/(const TVector2<T>& V, float S) requires(!std::is_same_v<T, float>) { return V / (T)S; }

template <typename T>
struct TVector
{
	T X = 0, Y = 0, Z = 0;

	static const TVector ZeroVector;
	static const TVector OneVector;
	static const TVector UpVector;
	static const TVector DownVector;
	static const TVector ForwardVector;
	static const TVector RightVector;
	static const TVector XAxisVector;
	static const TVector YAxisVector;
	static const TVector ZAxisVector;

	constexpr TVector() = default;
	constexpr TVector(T InX, T InY, T InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit constexpr TVector(T V) : X(V), Y(V), Z(V) {}
	explicit TVector(EForceInit) : X(0), Y(0), Z(0) {}
	TVector(const TVector2<T>& V, T InZ) : X(V.X), Y(V.Y), Z(InZ) {}
	explicit TVector(const struct FIntVector& V);
	template <typename U, typename = std::enable_if_t<!std::is_same_v<T, U>>>
	explicit TVector(const TVector<U>& O) : X((T)O.X), Y((T)O.Y), Z((T)O.Z) {}

	TVector operator+(const TVector& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
	TVector operator-(const TVector& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
	TVector operator*(const TVector& O) const { return {X * O.X, Y * O.Y, Z * O.Z}; }
	TVector operator/(const TVector& O) const { return {X / O.X, Y / O.Y, Z / O.Z}; }
	TVector operator*(T S) const { return {X * S, Y * S, Z * S}; }
	TVector operator/(T S) const { return {X / S, Y / S, Z / S}; }
	TVector operator+(T S) const { return {X + S, Y + S, Z + S}; }
	TVector operator-(T S) const { return {X - S, Y - S, Z - S}; }
	TVector operator-() const { return {-X, -Y, -Z}; }
	TVector& operator+=(const TVector& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	TVector& operator-=(const TVector& O) { X -= O.X; Y -= O.Y; Z -= O.Z; return *this; }
	TVector& operator*=(T S) { X *= S; Y *= S; Z *= S; return *this; }
	TVector& operator/=(T S) { X /= S; Y /= S; Z /= S; return *this; }
	TVector& operator*=(const TVector& O) { X *= O.X; Y *= O.Y; Z *= O.Z; return *this; }
	bool operator==(const TVector& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
	bool operator!=(const TVector& O) const { return !(*this == O); }
	T operator|(const TVector& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
	TVector operator^(const TVector& O) const { return {Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X}; }
	T& operator[](int32 I) { return I == 0 ? X : (I == 1 ? Y : Z); }
	T operator[](int32 I) const { return I == 0 ? X : (I == 1 ? Y : Z); }

	T Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	T Length() const { return Size(); }
	T SizeSquared() const { return X * X + Y * Y + Z * Z; }
	T SquaredLength() const { return SizeSquared(); }
	T Size2D() const { return std::sqrt(X * X + Y * Y); }
	T SizeSquared2D() const { return X * X + Y * Y; }
	bool IsNearlyZero(T Tol = (T)UE_KINDA_SMALL_NUMBER) const { return FMath::Abs(X) <= Tol && FMath::Abs(Y) <= Tol && FMath::Abs(Z) <= Tol; }
	bool IsZero() const { return X == 0 && Y == 0 && Z == 0; }
	bool IsNormalized() const { return FMath::Abs((T)1 - SizeSquared()) < (T)0.01; }
	bool Equals(const TVector& O, T Tol = (T)UE_KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(X - O.X) <= Tol && FMath::Abs(Y - O.Y) <= Tol && FMath::Abs(Z - O.Z) <= Tol;
	}
	TVector GetSafeNormal(T Tol = (T)UE_SMALL_NUMBER, const TVector& ResultIfZero = ZeroVector) const
	{
		const T S = SizeSquared();
		if (S == (T)1) { return *this; }
		if (S < Tol) { return ResultIfZero; }
		const T Inv = (T)1 / std::sqrt(S);
		return {X * Inv, Y * Inv, Z * Inv};
	}
	TVector GetSafeNormal2D(T Tol = (T)UE_SMALL_NUMBER) const
	{
		const T S = X * X + Y * Y;
		if (S < Tol) { return ZeroVector; }
		const T Inv = (T)1 / std::sqrt(S);
		return {X * Inv, Y * Inv, 0};
	}
	TVector GetUnsafeNormal() const { const T Inv = (T)1 / Size(); return {X * Inv, Y * Inv, Z * Inv}; }
	bool Normalize(T Tol = (T)UE_SMALL_NUMBER)
	{
		const T S = SizeSquared();
		if (S > Tol) { *this = *this * ((T)1 / std::sqrt(S)); return true; }
		return false;
	}
	TVector GetAbs() const { return {FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)}; }
	T GetMax() const { return FMath::Max3(X, Y, Z); }
	T GetMin() const { return FMath::Min3(X, Y, Z); }
	T GetAbsMax() const { return FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)); }
	TVector ComponentMin(const TVector& O) const { return {FMath::Min(X, O.X), FMath::Min(Y, O.Y), FMath::Min(Z, O.Z)}; }
	TVector ComponentMax(const TVector& O) const { return {FMath::Max(X, O.X), FMath::Max(Y, O.Y), FMath::Max(Z, O.Z)}; }
	TVector GetClampedToMaxSize(T Max) const
	{
		const T S = SizeSquared();
		return S > Max * Max ? GetSafeNormal() * Max : *this;
	}
	TVector2<T> ToVector2D() const { return {X, Y}; }
	explicit operator TVector2<T>() const { return {X, Y}; }

	static T DotProduct(const TVector& A, const TVector& B) { return A | B; }
	static TVector CrossProduct(const TVector& A, const TVector& B) { return A ^ B; }
	static T Dist(const TVector& A, const TVector& B) { return (A - B).Size(); }
	static T Distance(const TVector& A, const TVector& B) { return (A - B).Size(); }
	static T DistSquared(const TVector& A, const TVector& B) { return (A - B).SizeSquared(); }
	static T Dist2D(const TVector& A, const TVector& B) { return (A - B).Size2D(); }
	static T DistSquared2D(const TVector& A, const TVector& B) { return (A - B).SizeSquared2D(); }
	static TVector Min(const TVector& A, const TVector& B) { return A.ComponentMin(B); }
	static TVector Max(const TVector& A, const TVector& B) { return A.ComponentMax(B); }
};
template <typename T> const TVector<T> TVector<T>::ZeroVector(0, 0, 0);
template <typename T> const TVector<T> TVector<T>::OneVector(1, 1, 1);
template <typename T> const TVector<T> TVector<T>::UpVector(0, 0, 1);
template <typename T> const TVector<T> TVector<T>::DownVector(0, 0, -1);
template <typename T> const TVector<T> TVector<T>::ForwardVector(1, 0, 0);
template <typename T> const TVector<T> TVector<T>::RightVector(0, 1, 0);
template <typename T> const TVector<T> TVector<T>::XAxisVector(1, 0, 0);
template <typename T> const TVector<T> TVector<T>::YAxisVector(0, 1, 0);
template <typename T> const TVector<T> TVector<T>::ZAxisVector(0, 0, 1);
template <typename T> inline TVector<T> operator*(T S, const TVector<T>& V) { return V * S; }
template <typename T> inline TVector<T> operator*(float S, const TVector<T>& V) requires(!std::is_same_v<T, float>) { return V * (T)S; }
template <typename T> inline TVector<T> operator*(const TVector<T>& V, float S) requires(!std::is_same_v<T, float>) { return V * (T)S; }
template <typename T> inline TVector<T> operator/(const TVector<T>& V, float S) requires(!std::is_same_v<T, float>) { return V / (T)S; }

using FVector = TVector<double>;
using FVector3d = TVector<double>;
using FVector3f = TVector<float>;
using FVector2D = TVector2<double>;
using FVector2d = TVector2<double>;
using FVector2f = TVector2<float>;

struct FVector4f
{
	float X = 0, Y = 0, Z = 0, W = 0;
	FVector4f() = default;
	FVector4f(float InX, float InY, float InZ, float InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
};

struct FIntVector
{
	int32 X = 0, Y = 0, Z = 0;
	static const FIntVector ZeroValue;
	static const FIntVector NoneValue;
	constexpr FIntVector() = default;
	constexpr FIntVector(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit constexpr FIntVector(int32 V) : X(V), Y(V), Z(V) {}
	FIntVector operator+(const FIntVector& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
	FIntVector operator-(const FIntVector& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
	FIntVector operator*(int32 S) const { return {X * S, Y * S, Z * S}; }
	FIntVector operator/(int32 S) const { return {X / S, Y / S, Z / S}; }
	FIntVector& operator+=(const FIntVector& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	bool operator==(const FIntVector& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
	bool operator!=(const FIntVector& O) const { return !(*this == O); }
	int32& operator[](int32 I) { return I == 0 ? X : (I == 1 ? Y : Z); }
	int32 operator[](int32 I) const { return I == 0 ? X : (I == 1 ? Y : Z); }
};
inline const FIntVector FIntVector::ZeroValue(0, 0, 0);
template <typename T> TVector<T>::TVector(const FIntVector& V) : X((T)V.X), Y((T)V.Y), Z((T)V.Z) {}
inline const FIntVector FIntVector::NoneValue(INDEX_NONE, INDEX_NONE, INDEX_NONE);
inline uint32 GetTypeHash(const FIntVector& V) { return (uint32)V.X * 73856093u ^ (uint32)V.Y * 19349663u ^ (uint32)V.Z * 83492791u; }

struct FIntPoint
{
	int32 X = 0, Y = 0;
	static const FIntPoint ZeroValue;
	constexpr FIntPoint() = default;
	constexpr FIntPoint(int32 InX, int32 InY) : X(InX), Y(InY) {}
	explicit constexpr FIntPoint(int32 V) : X(V), Y(V) {}
	FIntPoint operator+(const FIntPoint& O) const { return {X + O.X, Y + O.Y}; }
	FIntPoint operator-(const FIntPoint& O) const { return {X - O.X, Y - O.Y}; }
	FIntPoint operator*(int32 S) const { return {X * S, Y * S}; }
	bool operator==(const FIntPoint& O) const { return X == O.X && Y == O.Y; }
	bool operator!=(const FIntPoint& O) const { return !(*this == O); }
	int32 SizeSquared() const { return X * X + Y * Y; }
};
inline const FIntPoint FIntPoint::ZeroValue(0, 0);
inline uint32 GetTypeHash(const FIntPoint& V) { return (uint32)V.X * 73856093u ^ (uint32)V.Y * 19349663u; }

struct FColor
{
	uint8 B = 0, G = 0, R = 0, A = 255;
	constexpr FColor() = default;
	constexpr FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : B(InB), G(InG), R(InR), A(InA) {}
	bool operator==(const FColor& O) const { return R == O.R && G == O.G && B == O.B && A == O.A; }
};

struct FLinearColor
{
	float R = 0, G = 0, B = 0, A = 1;
	static const FLinearColor White;
	static const FLinearColor Black;
	static const FLinearColor Transparent;
	constexpr FLinearColor() = default;
	constexpr FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	explicit FLinearColor(const FVector3f& V) : R(V.X), G(V.Y), B(V.Z), A(1) {}
	/** Como en Unreal: FColor se interpreta en sRGB y se pasa a lineal con la curva exacta. */
	FLinearColor(const FColor& C) : R(FromSRGB(C.R)), G(FromSRGB(C.G)), B(FromSRGB(C.B)), A(C.A / 255.0f) {}
	static float FromSRGB(uint8 V)
	{
		const float F = V / 255.0f;
		return F <= 0.04045f ? F / 12.92f : std::pow((F + 0.055f) / 1.055f, 2.4f);
	}
	FLinearColor operator+(const FLinearColor& O) const { return {R + O.R, G + O.G, B + O.B, A + O.A}; }
	FLinearColor operator-(const FLinearColor& O) const { return {R - O.R, G - O.G, B - O.B, A - O.A}; }
	FLinearColor operator*(const FLinearColor& O) const { return {R * O.R, G * O.G, B * O.B, A * O.A}; }
	FLinearColor operator*(float S) const { return {R * S, G * S, B * S, A * S}; }
	FLinearColor operator/(float S) const { return {R / S, G / S, B / S, A / S}; }
	FLinearColor& operator+=(const FLinearColor& O) { R += O.R; G += O.G; B += O.B; A += O.A; return *this; }
	FLinearColor& operator*=(float S) { R *= S; G *= S; B *= S; A *= S; return *this; }
	bool operator==(const FLinearColor& O) const { return R == O.R && G == O.G && B == O.B && A == O.A; }
	bool Equals(const FLinearColor& O, float Tol = UE_KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(R - O.R) <= Tol && FMath::Abs(G - O.G) <= Tol && FMath::Abs(B - O.B) <= Tol && FMath::Abs(A - O.A) <= Tol;
	}
	static FLinearColor LerpUsingHSV(const FLinearColor& A, const FLinearColor& B, float T) { return A + (B - A) * T; }
	float GetLuminance() const { return R * 0.3f + G * 0.59f + B * 0.11f; }
	FColor ToFColor(bool) const
	{
		auto Q = [](float V) { return (uint8)FMath::Clamp(FMath::RoundToInt(V * 255.0f), 0, 255); };
		return FColor(Q(R), Q(G), Q(B), Q(A));
	}
};
inline const FLinearColor FLinearColor::White(1, 1, 1, 1);
inline const FLinearColor FLinearColor::Black(0, 0, 0, 1);
inline const FLinearColor FLinearColor::Transparent(0, 0, 0, 0);
inline FLinearColor operator*(float S, const FLinearColor& C) { return C * S; }

template <typename T>
struct TBox2
{
	TVector2<T> Min, Max;
	bool bIsValid = false;
	TBox2() = default;
	explicit TBox2(EForceInit) {}
	TBox2(const TVector2<T>& InMin, const TVector2<T>& InMax) : Min(InMin), Max(InMax), bIsValid(true) {}
	TBox2& operator+=(const TVector2<T>& P)
	{
		if (bIsValid) { Min = Min.ComponentMin(P); Max = Max.ComponentMax(P); }
		else { Min = Max = P; bIsValid = true; }
		return *this;
	}
	TBox2& operator+=(const TBox2& B) { if (B.bIsValid) { *this += B.Min; *this += B.Max; } return *this; }
	bool IsInside(const TVector2<T>& P) const { return P.X > Min.X && P.X < Max.X && P.Y > Min.Y && P.Y < Max.Y; }
	bool IsInsideOrOn(const TVector2<T>& P) const { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y; }
	bool Intersect(const TBox2& O) const { return !(Min.X > O.Max.X || O.Min.X > Max.X || Min.Y > O.Max.Y || O.Min.Y > Max.Y); }
	TVector2<T> GetCenter() const { return (Min + Max) * (T)0.5; }
	TVector2<T> GetExtent() const { return (Max - Min) * (T)0.5; }
	TVector2<T> GetSize() const { return Max - Min; }
	TBox2 ExpandBy(T W) const { return TBox2(Min - TVector2<T>(W, W), Max + TVector2<T>(W, W)); }
};
using FBox2D = TBox2<double>;
using FBox2f = TBox2<float>;

template <typename T>
struct TBox
{
	TVector<T> Min, Max;
	bool IsValid = false;
	TBox() = default;
	explicit TBox(EForceInit) {}
	TBox(const TVector<T>& InMin, const TVector<T>& InMax) : Min(InMin), Max(InMax), IsValid(true) {}
	TBox& operator+=(const TVector<T>& P)
	{
		if (IsValid) { Min = Min.ComponentMin(P); Max = Max.ComponentMax(P); }
		else { Min = Max = P; IsValid = true; }
		return *this;
	}
	TBox& operator+=(const TBox& B) { if (B.IsValid) { *this += B.Min; *this += B.Max; } return *this; }
	bool IsInside(const TVector<T>& P) const { return P.X > Min.X && P.X < Max.X && P.Y > Min.Y && P.Y < Max.Y && P.Z > Min.Z && P.Z < Max.Z; }
	bool IsInsideOrOn(const TVector<T>& P) const { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y && P.Z >= Min.Z && P.Z <= Max.Z; }
	bool Intersect(const TBox& O) const
	{
		return !(Min.X > O.Max.X || O.Min.X > Max.X || Min.Y > O.Max.Y || O.Min.Y > Max.Y || Min.Z > O.Max.Z || O.Min.Z > Max.Z);
	}
	TVector<T> GetCenter() const { return (Min + Max) * (T)0.5; }
	TVector<T> GetExtent() const { return (Max - Min) * (T)0.5; }
	TVector<T> GetSize() const { return Max - Min; }
	TBox ExpandBy(T W) const { return TBox(Min - TVector<T>(W), Max + TVector<T>(W)); }
	static TBox BuildAABB(const TVector<T>& Origin, const TVector<T>& Extent) { return TBox(Origin - Extent, Origin + Extent); }
};
using FBox = TBox<double>;
using FBox3f = TBox<float>;

/** Cuaternión (X, Y, Z, W) con la convención de Unreal. */
struct FQuat
{
	double X = 0, Y = 0, Z = 0, W = 1;
	static const FQuat Identity;
	FQuat() = default;
	FQuat(double InX, double InY, double InZ, double InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
	FQuat(const FVector& Axis, double Angle)
	{
		const double H = 0.5 * Angle, S = std::sin(H);
		X = S * Axis.X; Y = S * Axis.Y; Z = S * Axis.Z; W = std::cos(H);
	}
	FQuat operator*(const FQuat& Q) const
	{
		return FQuat(W * Q.X + X * Q.W + Y * Q.Z - Z * Q.Y,
			W * Q.Y - X * Q.Z + Y * Q.W + Z * Q.X,
			W * Q.Z + X * Q.Y - Y * Q.X + Z * Q.W,
			W * Q.W - X * Q.X - Y * Q.Y - Z * Q.Z);
	}
	FVector RotateVector(const FVector& V) const
	{
		const FVector Q(X, Y, Z);
		const FVector T = FVector::CrossProduct(Q, V) * 2.0;
		return V + T * W + FVector::CrossProduct(Q, T);
	}
	FVector UnrotateVector(const FVector& V) const { return Inverse().RotateVector(V); }
	FQuat Inverse() const { return FQuat(-X, -Y, -Z, W); }
	FVector GetUpVector() const { return RotateVector(FVector::UpVector); }
	FVector GetForwardVector() const { return RotateVector(FVector::ForwardVector); }
	FVector GetRightVector() const { return RotateVector(FVector::RightVector); }
	void Normalize()
	{
		const double S = std::sqrt(X * X + Y * Y + Z * Z + W * W);
		if (S > 1e-12) { X /= S; Y /= S; Z /= S; W /= S; } else { *this = Identity; }
	}
	bool Equals(const FQuat& Q, double Tol = 1e-4) const
	{
		const bool Same = FMath::Abs(X - Q.X) <= Tol && FMath::Abs(Y - Q.Y) <= Tol && FMath::Abs(Z - Q.Z) <= Tol && FMath::Abs(W - Q.W) <= Tol;
		const bool Neg = FMath::Abs(X + Q.X) <= Tol && FMath::Abs(Y + Q.Y) <= Tol && FMath::Abs(Z + Q.Z) <= Tol && FMath::Abs(W + Q.W) <= Tol;
		return Same || Neg;
	}
	static FQuat FindBetweenNormals(const FVector& A, const FVector& B)
	{
		const double NormAB = 1.0;
		double Wv = NormAB + FVector::DotProduct(A, B);
		FQuat Result;
		if (Wv >= 1e-6 * NormAB)
		{
			const FVector C = FVector::CrossProduct(A, B);
			Result = FQuat(C.X, C.Y, C.Z, Wv);
		}
		else
		{
			Wv = 0.0;
			Result = FMath::Abs(A.X) > FMath::Abs(A.Y) ? FQuat(-A.Z, 0.0, A.X, Wv) : FQuat(0.0, -A.Z, A.Y, Wv);
		}
		Result.Normalize();
		return Result;
	}
};
inline const FQuat FQuat::Identity(0, 0, 0, 1);

struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	static const FRotator ZeroRotator;
	FRotator() = default;
	FRotator(double InPitch, double InYaw, double InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
	/** Solo guiñada y cabeceo (lo que usan los modelos). */
	FVector Vector() const
	{
		const double P = FMath::DegreesToRadians(Pitch), Y = FMath::DegreesToRadians(Yaw);
		return FVector(std::cos(P) * std::cos(Y), std::cos(P) * std::sin(Y), std::sin(P));
	}
	FQuat Quaternion() const
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * FQuat(FVector(0, -1, 0), FMath::DegreesToRadians(Pitch))
			* FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Roll));
	}
	FVector RotateVector(const FVector& V) const { return Quaternion().RotateVector(V); }
	bool operator==(const FRotator& O) const { return Pitch == O.Pitch && Yaw == O.Yaw && Roll == O.Roll; }
};
inline const FRotator FRotator::ZeroRotator(0, 0, 0);

struct FTransform
{
	FQuat Rotation;
	FVector Translation;
	FVector Scale3D = FVector(1.0);
	static const FTransform Identity;
	FTransform() = default;
	FTransform(const FQuat& R, const FVector& T, const FVector& S = FVector(1.0)) : Rotation(R), Translation(T), Scale3D(S) {}
	explicit FTransform(const FVector& T) : Translation(T) {}
	FTransform(const FRotator& R, const FVector& T, const FVector& S = FVector(1.0)) : Rotation(R.Quaternion()), Translation(T), Scale3D(S) {}
	FVector GetLocation() const { return Translation; }
	FVector GetTranslation() const { return Translation; }
	FQuat GetRotation() const { return Rotation; }
	FVector GetScale3D() const { return Scale3D; }
	void SetLocation(const FVector& V) { Translation = V; }
	void SetTranslation(const FVector& V) { Translation = V; }
	void SetRotation(const FQuat& Q) { Rotation = Q; }
	void SetScale3D(const FVector& S) { Scale3D = S; }
	FVector TransformPosition(const FVector& V) const { return Rotation.RotateVector(V * Scale3D) + Translation; }
	FVector TransformVector(const FVector& V) const { return Rotation.RotateVector(V * Scale3D); }
	FVector InverseTransformPosition(const FVector& V) const { return Rotation.UnrotateVector(V - Translation) / Scale3D; }
	bool Equals(const FTransform& O, double Tol = 1e-4) const
	{
		return Rotation.Equals(O.Rotation, Tol) && Translation.Equals(O.Translation, Tol) && Scale3D.Equals(O.Scale3D, Tol);
	}
};
inline const FTransform FTransform::Identity;
