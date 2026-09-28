// Shim mínimo de CoreMinimal.h para compilar y probar en el host (Linux, g++/clang) los
// modelos puros del módulo Explored, sin Unreal. Reproduce SOLO la parte de la API de
// Core que usan esos modelos, con la semántica de Unreal donde importa (int32 en Num(),
// INDEX_NONE, orden de inserción en TMap, FVector en double, FVector3f en float).
//
// No es Unreal: si un test pasa aquí y falla en el editor, manda el editor. Los ficheros
// que incluyan UObject (UCLASS/USTRUCT, *.generated.h) no se compilan con este shim.
#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tipos y macros básicos
// ---------------------------------------------------------------------------

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using TCHAR = char;
using ANSICHAR = char;
using SIZE_T = std::size_t;

#define TEXT(x) x
#define FORCEINLINE inline
#define FORCENOINLINE
#define EXPLORED_API
#define EXPLOREDEDITOR_API
#define INDEX_NONE (-1)
#define UE_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define UE_NODISCARD [[nodiscard]]

struct FHostCheckFailure : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

#define check(expr) do { if (!(expr)) throw FHostCheckFailure("check falló: " #expr); } while (0)
#define checkf(expr, ...) check(expr)
#define checkNoEntry() throw FHostCheckFailure("checkNoEntry")
#define verify(expr) check(expr)
#define ensure(expr) (static_cast<bool>(expr))
#define ensureMsgf(expr, ...) (static_cast<bool>(expr))
#define ensureAlways(expr) (static_cast<bool>(expr))

#define DECLARE_LOG_CATEGORY_EXTERN(...)
#define DEFINE_LOG_CATEGORY(...)
#define DEFINE_LOG_CATEGORY_STATIC(...)
#define UE_LOG(...) do {} while (0)

constexpr double UE_DOUBLE_PI = 3.141592653589793238462643383279502884197169399;
constexpr float UE_PI = 3.1415926535897932f;
constexpr float UE_TWO_PI = 6.28318530717958647f;
constexpr float UE_HALF_PI = 1.57079632679489661f;
constexpr float UE_SMALL_NUMBER = 1.e-8f;
constexpr double UE_DOUBLE_SMALL_NUMBER = 1.e-8;
constexpr double UE_DOUBLE_KINDA_SMALL_NUMBER = 1.e-4;
constexpr float UE_KINDA_SMALL_NUMBER = 1.e-4f;
constexpr float UE_BIG_NUMBER = 3.4e+38f;
constexpr float UE_DELTA = 0.00001f;
#define PI UE_PI
#define TWO_PI UE_TWO_PI
#define HALF_PI UE_HALF_PI
#define SMALL_NUMBER UE_SMALL_NUMBER
#define KINDA_SMALL_NUMBER UE_KINDA_SMALL_NUMBER
#define BIG_NUMBER UE_BIG_NUMBER
#define MAX_flt FLT_MAX
#define MAX_int32 INT32_MAX
#define MIN_int32 INT32_MIN
#define MAX_uint32 UINT32_MAX

template <typename T>
struct TNumericLimits
{
	static constexpr T Min() { return std::numeric_limits<T>::lowest(); }
	static constexpr T Lowest() { return std::numeric_limits<T>::lowest(); }
	static constexpr T Max() { return std::numeric_limits<T>::max(); }
};
template <>
struct TNumericLimits<float>
{
	// Como en Unreal: Min() es el menor positivo normalizado, Lowest() el más negativo.
	static constexpr float Min() { return FLT_MIN; }
	static constexpr float Lowest() { return -FLT_MAX; }
	static constexpr float Max() { return FLT_MAX; }
};
template <>
struct TNumericLimits<double>
{
	static constexpr double Min() { return DBL_MIN; }
	static constexpr double Lowest() { return -DBL_MAX; }
	static constexpr double Max() { return DBL_MAX; }
};

template <typename T>
inline void Swap(T& A, T& B) { std::swap(A, B); }
template <typename T>
inline std::remove_reference_t<T>&& MoveTemp(T&& V) { return std::move(V); }
template <typename T>
inline T CopyTemp(const T& V) { return V; }
template <typename T>
inline T&& Forward(std::remove_reference_t<T>& V) { return static_cast<T&&>(V); }

// ---------------------------------------------------------------------------
// FMath
// ---------------------------------------------------------------------------

struct FMath
{
	template <typename T> static constexpr T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static constexpr T Min(T A, T B) { return A < B ? A : B; }
	template <typename A, typename B, typename = std::enable_if_t<!std::is_same_v<A, B>>>
	static constexpr auto Max(A X, B Y) -> std::common_type_t<A, B> { return X > Y ? X : Y; }
	template <typename A, typename B, typename = std::enable_if_t<!std::is_same_v<A, B>>>
	static constexpr auto Min(A X, B Y) -> std::common_type_t<A, B> { return X < Y ? X : Y; }
	template <typename T> static constexpr T Max3(T A, T B, T C) { return Max(Max(A, B), C); }
	template <typename T> static constexpr T Min3(T A, T B, T C) { return Min(Min(A, B), C); }
	template <typename T, typename U, typename V>
	static constexpr T Clamp(T X, U Lo, V Hi) { return X < T(Lo) ? T(Lo) : (X > T(Hi) ? T(Hi) : X); }
	template <typename T> static constexpr T Abs(T A) { return A < T(0) ? -A : A; }
	template <typename T> static constexpr T Sign(T A) { return A > T(0) ? T(1) : (A < T(0) ? T(-1) : T(0)); }
	template <typename T> static constexpr T Square(T A) { return A * A; }
	template <typename T> static constexpr T Cube(T A) { return A * A * A; }

	template <typename T, typename U>
	static constexpr T Lerp(const T& A, const T& B, const U& Alpha) { return (T)(A + Alpha * (B - A)); }
	template <typename T, typename U>
	static T BiLerp(const T& P00, const T& P10, const T& P01, const T& P11, const U& FracX, const U& FracY)
	{
		return Lerp(Lerp(P00, P10, FracX), Lerp(P01, P11, FracX), FracY);
	}

	static float Sqrt(float V) { return std::sqrt(V); }
	static double Sqrt(double V) { return std::sqrt(V); }
	static float InvSqrt(float V) { return 1.0f / std::sqrt(V); }
	static double InvSqrt(double V) { return 1.0 / std::sqrt(V); }
	static float Sin(float V) { return std::sin(V); }
	static double Sin(double V) { return std::sin(V); }
	static float Cos(float V) { return std::cos(V); }
	static double Cos(double V) { return std::cos(V); }
	static float Tan(float V) { return std::tan(V); }
	static double Tan(double V) { return std::tan(V); }
	static float Asin(float V) { return std::asin(Clamp(V, -1.0f, 1.0f)); }
	static double Asin(double V) { return std::asin(Clamp(V, -1.0, 1.0)); }
	static float Acos(float V) { return std::acos(Clamp(V, -1.0f, 1.0f)); }
	static double Acos(double V) { return std::acos(Clamp(V, -1.0, 1.0)); }
	static float Atan(float V) { return std::atan(V); }
	static float Atan2(float Y, float X) { return std::atan2(Y, X); }
	static double Atan2(double Y, double X) { return std::atan2(Y, X); }
	static void SinCos(float* S, float* C, float V) { *S = std::sin(V); *C = std::cos(V); }
	static void SinCos(double* S, double* C, double V) { *S = std::sin(V); *C = std::cos(V); }
	static float Exp(float V) { return std::exp(V); }
	static double Exp(double V) { return std::exp(V); }
	static float Exp2(float V) { return std::exp2(V); }
	static float Loge(float V) { return std::log(V); }
	static double Loge(double V) { return std::log(V); }
	static float Log2(float V) { return std::log2(V); }
	static float LogX(float Base, float V) { return std::log(V) / std::log(Base); }
	static float Pow(float A, float B) { return std::pow(A, B); }
	static double Pow(double A, double B) { return std::pow(A, B); }
	static float Fmod(float A, float B) { return std::fmod(A, B); }
	static double Fmod(double A, double B) { return std::fmod(A, B); }
	static float Frac(float V) { return V - std::floor(V); }
	static double Frac(double V) { return V - std::floor(V); }
	static float Fractional(float V) { return V - std::trunc(V); }

	static float FloorToFloat(float V) { return std::floor(V); }
	static double FloorToDouble(double V) { return std::floor(V); }
	static double FloorToFloat(double V) { return std::floor(V); }
	static float CeilToFloat(float V) { return std::ceil(V); }
	static double CeilToDouble(double V) { return std::ceil(V); }
	static float RoundToFloat(float V) { return std::floor(V + 0.5f); }
	static double RoundToDouble(double V) { return std::floor(V + 0.5); }
	static int32 FloorToInt32(float V) { return (int32)std::floor(V); }
	static int32 FloorToInt32(double V) { return (int32)std::floor(V); }
	static int32 FloorToInt(float V) { return (int32)std::floor(V); }
	static int32 FloorToInt(double V) { return (int32)std::floor(V); }
	static int32 CeilToInt32(float V) { return (int32)std::ceil(V); }
	static int32 CeilToInt32(double V) { return (int32)std::ceil(V); }
	static int32 CeilToInt(float V) { return (int32)std::ceil(V); }
	static int32 CeilToInt(double V) { return (int32)std::ceil(V); }
	static int32 RoundToInt32(float V) { return (int32)std::floor(V + 0.5f); }
	static int32 RoundToInt32(double V) { return (int32)std::floor(V + 0.5); }
	static int32 RoundToInt(float V) { return (int32)std::floor(V + 0.5f); }
	static int32 RoundToInt(double V) { return (int32)std::floor(V + 0.5); }
	static int32 TruncToInt32(float V) { return (int32)V; }
	static int32 TruncToInt(float V) { return (int32)V; }
	static float TruncToFloat(float V) { return std::trunc(V); }
	static int64 FloorToInt64(double V) { return (int64)std::floor(V); }

	static bool IsNearlyEqual(float A, float B, float Tol = UE_SMALL_NUMBER) { return std::fabs(A - B) <= Tol; }
	static bool IsNearlyEqual(double A, double B, double Tol = UE_SMALL_NUMBER) { return std::fabs(A - B) <= Tol; }
	static bool IsNearlyZero(float A, float Tol = UE_SMALL_NUMBER) { return std::fabs(A) <= Tol; }
	static bool IsNearlyZero(double A, double Tol = UE_SMALL_NUMBER) { return std::fabs(A) <= Tol; }
	// Por bits, como FGenericPlatformMath: con matemáticas rápidas (-ffast-math, /fp:fast) el
	// compilador puede dar por hecho que no hay NaN y plegar std::isnan/std::isfinite a una constante.
	static bool IsFinite(float A) { return (std::bit_cast<uint32>(A) & 0x7F800000u) != 0x7F800000u; }
	static bool IsFinite(double A) { return (std::bit_cast<uint64>(A) & 0x7FF0000000000000ull) != 0x7FF0000000000000ull; }
	static bool IsNaN(float A) { return (std::bit_cast<uint32>(A) & 0x7FFFFFFFu) > 0x7F800000u; }
	static bool IsNaN(double A) { return (std::bit_cast<uint64>(A) & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull; }
	template <typename T> static constexpr bool IsPowerOfTwo(T V) { return V > 0 && (V & (V - 1)) == 0; }
	template <typename T> static constexpr bool IsWithin(T V, T Lo, T Hi) { return V >= Lo && V < Hi; }
	template <typename T> static constexpr bool IsWithinInclusive(T V, T Lo, T Hi) { return V >= Lo && V <= Hi; }
	template <typename T> static constexpr T DivideAndRoundUp(T A, T B) { return (A + B - 1) / B; }

	template <typename T> static T RadiansToDegrees(T V) { return V * (T)(180.0 / UE_DOUBLE_PI); }
	template <typename T> static T DegreesToRadians(T V) { return V * (T)(UE_DOUBLE_PI / 180.0); }

	template <typename T> static T SmoothStep(T A, T B, T X)
	{
		if (X < A) { return T(0); }
		if (X >= B) { return T(1); }
		const T F = (X - A) / (B - A);
		return F * F * (T(3) - T(2) * F);
	}
	static float FInterpTo(float Current, float Target, float DeltaTime, float Speed)
	{
		if (Speed <= 0.0f) { return Target; }
		const float Dist = Target - Current;
		if (Dist * Dist < UE_SMALL_NUMBER) { return Target; }
		return Current + Dist * Clamp(DeltaTime * Speed, 0.0f, 1.0f);
	}
	static float FInterpConstantTo(float Current, float Target, float DeltaTime, float Speed)
	{
		const float Dist = Target - Current;
		if (Dist * Dist < UE_SMALL_NUMBER) { return Target; }
		const float Step = Speed * DeltaTime;
		return Current + Clamp(Dist, -Step, Step);
	}
	static float GetRangePct(float Lo, float Hi, float V) { return (Hi != Lo) ? (V - Lo) / (Hi - Lo) : Lo; }
	template <typename T> static T GetMappedRangeValueClamped(T InLo, T InHi, T OutLo, T OutHi, T V)
	{
		const T Pct = Clamp((InHi != InLo) ? (V - InLo) / (InHi - InLo) : T(0), T(0), T(1));
		return Lerp(OutLo, OutHi, Pct);
	}
	template <typename T> static T GetMappedRangeValueUnclamped(T InLo, T InHi, T OutLo, T OutHi, T V)
	{
		const T Pct = (InHi != InLo) ? (V - InLo) / (InHi - InLo) : T(0);
		return Lerp(OutLo, OutHi, Pct);
	}
	template <typename T> static T Wrap(T V, T Lo, T Hi)
	{
		const T Size = Hi - Lo;
		if (Size == T(0)) { return Hi; }
		T R = V;
		while (R > Hi) { R -= Size; }
		while (R < Lo) { R += Size; }
		return R;
	}
	static float UnwindDegrees(float A)
	{
		while (A > 180.0f) { A -= 360.0f; }
		while (A < -180.0f) { A += 360.0f; }
		return A;
	}
	static float UnwindRadians(float A)
	{
		while (A > UE_PI) { A -= UE_TWO_PI; }
		while (A < -UE_PI) { A += UE_TWO_PI; }
		return A;
	}
	static float FindDeltaAngleDegrees(float A, float B) { return UnwindDegrees(B - A); }
	static float FindDeltaAngleRadians(float A, float B) { return UnwindRadians(B - A); }
};

// ---------------------------------------------------------------------------
// Contenedores
// ---------------------------------------------------------------------------

enum EAllowShrinking { No, Yes };
enum class EAllowShrinkingE { No, Yes };
enum ENoInit { NoInit };
enum EForceInit { ForceInit, ForceInitToZero };

/** TArray sobre std::vector con la interfaz (y los índices int32) de Unreal. */
template <typename T, typename Alloc = void>
class TArray
{
public:
	using ElementType = T;

	TArray() = default;
	TArray(std::initializer_list<T> Init) : Data(Init) {}
	TArray(const T* Ptr, int32 Count) : Data(Ptr, Ptr + Count) {}

	int32 Num() const { return (int32)Data.size(); }
	int32 Max() const { return (int32)Data.capacity(); }
	bool IsEmpty() const { return Data.empty(); }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
	SIZE_T GetTypeSize() const { return sizeof(T); }
	T* GetData() { return Data.data(); }
	const T* GetData() const { return Data.data(); }

	T& operator[](int32 I) { check(IsValidIndex(I)); return Data[(size_t)I]; }
	const T& operator[](int32 I) const { check(IsValidIndex(I)); return Data[(size_t)I]; }
	T& Last(int32 FromEnd = 0) { return (*this)[Num() - 1 - FromEnd]; }
	const T& Last(int32 FromEnd = 0) const { return (*this)[Num() - 1 - FromEnd]; }
	T& Top() { return Last(); }
	const T& Top() const { return Last(); }

	// Igual que TArray::CheckAddress en UE: añadir un elemento del propio array es un
	// assert fatal en el editor (la referencia se invalida al realojar).
	void CheckAddress(const T* Addr) const
	{
		const std::less<const T*> Less;
		check(Data.empty() || Less(Addr, Data.data()) || !Less(Addr, Data.data() + Data.capacity()));
	}
	int32 Add(const T& V) { CheckAddress(&V); Data.push_back(V); return Num() - 1; }
	int32 Add(T&& V) { CheckAddress(&V); Data.push_back(std::move(V)); return Num() - 1; }
	template <typename... A> int32 Emplace(A&&... Args) { Data.emplace_back(std::forward<A>(Args)...); return Num() - 1; }
	template <typename... A> T& Emplace_GetRef(A&&... Args) { Data.emplace_back(std::forward<A>(Args)...); return Data.back(); }
	T& Add_GetRef(const T& V) { CheckAddress(&V); Data.push_back(V); return Data.back(); }
	int32 AddUnique(const T& V) { const int32 I = Find(V); return I != INDEX_NONE ? I : Add(V); }
	int32 AddUninitialized(int32 Count = 1) { const int32 I = Num(); Data.resize(Data.size() + (size_t)Count); return I; }
	int32 AddZeroed(int32 Count = 1) { const int32 I = Num(); Data.resize(Data.size() + (size_t)Count, T{}); return I; }
	int32 AddDefaulted(int32 Count = 1) { return AddZeroed(Count); }
	T& AddDefaulted_GetRef() { Data.emplace_back(); return Data.back(); }
	T& AddZeroed_GetRef() { Data.emplace_back(); return Data.back(); }
	void Push(const T& V) { Add(V); }
	T Pop(bool = true) { T V = std::move(Data.back()); Data.pop_back(); return V; }
	template <typename C> void Append(const C& Other) { for (const auto& V : Other) { Data.push_back(V); } }
	void Append(std::initializer_list<T> Other) { Data.insert(Data.end(), Other); }
	void Append(const T* Ptr, int32 Count) { Data.insert(Data.end(), Ptr, Ptr + Count); }
	int32 Insert(const T& V, int32 Index) { CheckAddress(&V); Data.insert(Data.begin() + Index, V); return Index; }
	void Init(const T& V, int32 Count) { Data.assign((size_t)Count, V); }
	void SetNum(int32 N, bool = true) { Data.resize((size_t)N); }
	void SetNumZeroed(int32 N, bool = true) { Data.resize((size_t)N, T{}); }
	void SetNumUninitialized(int32 N, bool = true) { Data.resize((size_t)N); }
	void Reserve(int32 N) { Data.reserve((size_t)N); }
	void Reset(int32 Slack = 0) { Data.clear(); Data.reserve((size_t)Slack); }
	void Empty(int32 Slack = 0) { Data.clear(); Data.shrink_to_fit(); Data.reserve((size_t)Slack); }
	void Shrink() { Data.shrink_to_fit(); }

	void RemoveAt(int32 Index, int32 Count = 1, bool = true) { Data.erase(Data.begin() + Index, Data.begin() + Index + Count); }
	void RemoveAtSwap(int32 Index, int32 Count = 1, bool = true)
	{
		for (int32 I = 0; I < Count; ++I)
		{
			std::swap(Data[(size_t)(Index + I)], Data.back());
			Data.pop_back();
		}
	}
	int32 Remove(const T& V) { const int32 Before = Num(); Data.erase(std::remove(Data.begin(), Data.end(), V), Data.end()); return Before - Num(); }
	int32 RemoveSingle(const T& V) { const int32 I = Find(V); if (I == INDEX_NONE) { return 0; } RemoveAt(I); return 1; }
	int32 RemoveSwap(const T& V) { int32 N = 0; for (int32 I = Num() - 1; I >= 0; --I) { if (Data[(size_t)I] == V) { RemoveAtSwap(I); ++N; } } return N; }
	template <typename P> int32 RemoveAll(P Pred) { const int32 Before = Num(); Data.erase(std::remove_if(Data.begin(), Data.end(), Pred), Data.end()); return Before - Num(); }
	template <typename P> int32 RemoveAllSwap(P Pred) { return RemoveAll(Pred); }

	int32 Find(const T& V) const { for (int32 I = 0; I < Num(); ++I) { if (Data[(size_t)I] == V) { return I; } } return INDEX_NONE; }
	bool Find(const T& V, int32& Out) const { Out = Find(V); return Out != INDEX_NONE; }
	int32 FindLast(const T& V) const { for (int32 I = Num() - 1; I >= 0; --I) { if (Data[(size_t)I] == V) { return I; } } return INDEX_NONE; }
	bool Contains(const T& V) const { return Find(V) != INDEX_NONE; }
	template <typename K> int32 IndexOfByKey(const K& Key) const { for (int32 I = 0; I < Num(); ++I) { if (Data[(size_t)I] == Key) { return I; } } return INDEX_NONE; }
	template <typename P> int32 IndexOfByPredicate(P Pred) const { for (int32 I = 0; I < Num(); ++I) { if (Pred(Data[(size_t)I])) { return I; } } return INDEX_NONE; }
	template <typename P> T* FindByPredicate(P Pred) { for (auto& V : Data) { if (Pred(V)) { return &V; } } return nullptr; }
	template <typename P> const T* FindByPredicate(P Pred) const { for (const auto& V : Data) { if (Pred(V)) { return &V; } } return nullptr; }
	template <typename K> T* FindByKey(const K& Key) { for (auto& V : Data) { if (V == Key) { return &V; } } return nullptr; }
	template <typename K> const T* FindByKey(const K& Key) const { for (const auto& V : Data) { if (V == Key) { return &V; } } return nullptr; }
	template <typename P> bool ContainsByPredicate(P Pred) const { return FindByPredicate(Pred) != nullptr; }
	template <typename P> TArray FilterByPredicate(P Pred) const { TArray R; for (const auto& V : Data) { if (Pred(V)) { R.Add(V); } } return R; }

	void Sort() { std::sort(Data.begin(), Data.end()); }
	template <typename P> void Sort(P Pred) { std::sort(Data.begin(), Data.end(), Pred); }
	void StableSort() { std::stable_sort(Data.begin(), Data.end()); }
	template <typename P> void StableSort(P Pred) { std::stable_sort(Data.begin(), Data.end(), Pred); }
	void Swap(int32 A, int32 B) { std::swap(Data[(size_t)A], Data[(size_t)B]); }

	bool operator==(const TArray& O) const { return Data == O.Data; }
	bool operator!=(const TArray& O) const { return Data != O.Data; }

	auto begin() { return Data.begin(); }
	auto end() { return Data.end(); }
	auto begin() const { return Data.begin(); }
	auto end() const { return Data.end(); }

private:
	std::vector<T> Data;
};

template <typename T, int32 N>
using TInlineAllocatorArray = TArray<T>;
template <int32 N> struct TInlineAllocator {};
template <int32 N> struct TFixedAllocator {};

template <typename K, typename V>
struct TPair
{
	K Key;
	V Value;
	TPair() = default;
	TPair(const K& InK, const V& InV) : Key(InK), Value(InV) {}
	bool operator==(const TPair& O) const { return Key == O.Key && Value == O.Value; }
};
template <typename K, typename V>
using TTuple = TPair<K, V>;
template <typename K, typename V>
TPair<K, V> MakeTuple(const K& A, const V& B) { return TPair<K, V>(A, B); }

inline uint32 HashCombine(uint32 A, uint32 C)
{
	uint32 B = 0x9e3779b9;
	A += B;
	A -= B; A -= C; A ^= (C >> 13);
	B -= C; B -= A; B ^= (A << 8);
	C -= A; C -= B; C ^= (B >> 13);
	A -= B; A -= C; A ^= (C >> 12);
	B -= C; B -= A; B ^= (A << 16);
	C -= A; C -= B; C ^= (B >> 5);
	A -= B; A -= C; A ^= (C >> 3);
	B -= C; B -= A; B ^= (A << 10);
	C -= A; C -= B; C ^= (B >> 15);
	return C;
}

template <typename T, typename = void>
struct THasTypeHash : std::false_type {};
template <typename T>
struct THasTypeHash<T, std::void_t<decltype(GetTypeHash(std::declval<const T&>()))>> : std::true_type {};

/** Hash para TMap/TSet: GetTypeHash (por ADL) si existe, si no std::hash. */
template <typename T>
struct THostHash
{
	size_t operator()(const T& V) const
	{
		if constexpr (std::is_enum_v<T>) { return std::hash<std::underlying_type_t<T>>()((std::underlying_type_t<T>)V); }
		else if constexpr (THasTypeHash<T>::value) { return (size_t)GetTypeHash(V); }
		else { return std::hash<T>()(V); }
	}
};
template <typename K, typename V>
uint32 GetTypeHash(const TPair<K, V>& P) { return HashCombine((uint32)THostHash<K>()(P.Key), (uint32)THostHash<V>()(P.Value)); }

/** TMap con el orden de iteración de Unreal cuando no hay borrados: orden de inserción. */
template <typename K, typename V>
class TMap
{
public:
	using ElementType = TPair<K, V>;

	TMap() = default;
	/** Como TMap(std::initializer_list<TPairInitializer<...>>) de Unreal: {{Clave, Valor}, ...}. */
	TMap(std::initializer_list<TPair<K, V>> Init) { for (const TPair<K, V>& P : Init) { Add(P.Key, P.Value); } }
	int32 Num() const { return Pairs.Num(); }
	bool IsEmpty() const { return Pairs.IsEmpty(); }
	V& Add(const K& Key, const V& Value = V())
	{
		if (V* Existing = Find(Key)) { *Existing = Value; return *Existing; }
		Pairs.Add(TPair<K, V>(Key, Value));
		Index.emplace(HashOf(Key), Pairs.Num() - 1);
		return Pairs.Last().Value;
	}
	V& Emplace(const K& Key, const V& Value = V()) { return Add(Key, Value); }
	V& FindOrAdd(const K& Key) { if (V* E = Find(Key)) { return *E; } return Add(Key, V()); }
	V& FindOrAdd(const K& Key, const V& Default) { if (V* E = Find(Key)) { return *E; } return Add(Key, Default); }
	V* Find(const K& Key)
	{
		auto Range = Index.equal_range(HashOf(Key));
		for (auto It = Range.first; It != Range.second; ++It) { if (Pairs[It->second].Key == Key) { return &Pairs[It->second].Value; } }
		return nullptr;
	}
	const V* Find(const K& Key) const { return const_cast<TMap*>(this)->Find(Key); }
	V FindRef(const K& Key) const { const V* P = Find(Key); return P ? *P : V(); }
	const V& FindChecked(const K& Key) const { const V* P = Find(Key); check(P); return *P; }
	V& FindChecked(const K& Key) { V* P = Find(Key); check(P); return *P; }
	bool Contains(const K& Key) const { return Find(Key) != nullptr; }
	V& operator[](const K& Key) { return FindChecked(Key); }
	const V& operator[](const K& Key) const { return FindChecked(Key); }
	int32 Remove(const K& Key)
	{
		const int32 I = Pairs.IndexOfByPredicate([&](const TPair<K, V>& P) { return P.Key == Key; });
		if (I == INDEX_NONE) { return 0; }
		Pairs.RemoveAt(I);
		Rebuild();
		return 1;
	}
	void Empty() { Pairs.Empty(); Index.clear(); }
	void Reset() { Empty(); }
	void Reserve(int32) {}
	template <typename A> void GetKeys(A& Out) const { Out.Reset(); for (const auto& P : Pairs) { Out.Add(P.Key); } }
	template <typename A> void GenerateValueArray(A& Out) const { Out.Reset(); for (const auto& P : Pairs) { Out.Add(P.Value); } }
	template <typename A> void GenerateKeyArray(A& Out) const { GetKeys(Out); }
	template <typename P> void KeySort(P Pred) { Pairs.StableSort([&](const TPair<K, V>& A, const TPair<K, V>& B) { return Pred(A.Key, B.Key); }); Rebuild(); }
	template <typename P> void ValueSort(P Pred) { Pairs.StableSort([&](const TPair<K, V>& A, const TPair<K, V>& B) { return Pred(A.Value, B.Value); }); Rebuild(); }
	bool operator==(const TMap& O) const
	{
		if (Num() != O.Num()) { return false; }
		for (const auto& P : Pairs) { const V* OV = O.Find(P.Key); if (!OV || !(*OV == P.Value)) { return false; } }
		return true;
	}

	auto begin() { return Pairs.begin(); }
	auto end() { return Pairs.end(); }
	auto begin() const { return Pairs.begin(); }
	auto end() const { return Pairs.end(); }

private:
	static size_t HashOf(const K& Key) { return THostHash<K>()(Key); }
	void Rebuild() { Index.clear(); for (int32 I = 0; I < Pairs.Num(); ++I) { Index.emplace(HashOf(Pairs[I].Key), I); } }
	TArray<TPair<K, V>> Pairs;
	std::unordered_multimap<size_t, int32> Index;
};

template <typename T>
class TSet
{
public:
	TSet() = default;
	TSet(std::initializer_list<T> Init) { for (const T& V : Init) { Add(V); } }
	void Add(const T& V) { if (!Contains(V)) { Items.Add(V); } }
	bool Contains(const T& V) const { return Items.Contains(V); }
	int32 Remove(const T& V) { return Items.Remove(V); }
	int32 Num() const { return Items.Num(); }
	bool IsEmpty() const { return Items.IsEmpty(); }
	void Empty() { Items.Empty(); }
	void Reset() { Items.Reset(); }
	TArray<T> Array() const { return Items; }
	auto begin() { return Items.begin(); }
	auto end() { return Items.end(); }
	auto begin() const { return Items.begin(); }
	auto end() const { return Items.end(); }
	bool operator==(const TSet& O) const
	{
		if (Num() != O.Num()) { return false; }
		for (const T& V : Items) { if (!O.Contains(V)) { return false; } }
		return true;
	}

private:
	TArray<T> Items;
};

template <typename T>
class TOptional
{
public:
	TOptional() = default;
	TOptional(const T& V) : Value(V), bSet(true) {}
	bool IsSet() const { return bSet; }
	explicit operator bool() const { return bSet; }
	const T& GetValue() const { check(bSet); return Value; }
	T& GetValue() { check(bSet); return Value; }
	T Get(const T& Default) const { return bSet ? Value : Default; }
	void Reset() { bSet = false; }
	T& Emplace(const T& V) { Value = V; bSet = true; return Value; }
	const T* operator->() const { return &GetValue(); }
	T* operator->() { return &GetValue(); }
	const T& operator*() const { return GetValue(); }
	T& operator*() { return GetValue(); }

private:
	T Value{};
	bool bSet = false;
};

#include <memory>
template <typename T>
using TUniquePtr = std::unique_ptr<T>;
template <typename T, typename... A>
TUniquePtr<T> MakeUnique(A&&... Args) { return std::make_unique<T>(std::forward<A>(Args)...); }
template <typename T>
using TSharedPtr = std::shared_ptr<T>;
template <typename T, typename... A>
TSharedPtr<T> MakeShared(A&&... Args) { return std::make_shared<T>(std::forward<A>(Args)...); }

template <typename T>
using TFunction = std::function<T>;
template <typename T>
using TFunctionRef = std::function<T>;
template <typename T>
using TUniqueFunction = std::function<T>;

// ---------------------------------------------------------------------------
// Cadenas
// ---------------------------------------------------------------------------

/** Como en Unreal (Misc/CString.h): distinguir o no mayúsculas al comparar cadenas. */
namespace ESearchCase
{
	enum Type { CaseSensitive, IgnoreCase };
}

class FString
{
public:
	FString() = default;
	FString(const char* S) : Str(S ? S : "") {}
	FString(const std::string& S) : Str(S) {}
	FString(int32 Len, const char* S) : Str(S, (size_t)Len) {}

	static FString Printf(const char* Fmt, ...)
	{
		char Buffer[4096];
		va_list Args;
		va_start(Args, Fmt);
		std::vsnprintf(Buffer, sizeof(Buffer), Fmt, Args);
		va_end(Args);
		return FString(Buffer);
	}
	static FString FromInt(int32 V) { return FString(std::to_string(V)); }
	static FString SanitizeFloat(double V) { char B[64]; std::snprintf(B, sizeof(B), "%g", V); return FString(B); }
	static FString Join(const TArray<FString>& Parts, const char* Sep)
	{
		std::string R;
		for (int32 I = 0; I < Parts.Num(); ++I) { if (I) { R += Sep; } R += Parts[I].Str; }
		return FString(R);
	}

	const char* operator*() const { return Str.c_str(); }
	int32 Len() const { return (int32)Str.size(); }
	bool IsEmpty() const { return Str.empty(); }
	void Empty() { Str.clear(); }
	void Reset() { Str.clear(); }
	char& operator[](int32 I) { return Str[(size_t)I]; }
	char operator[](int32 I) const { return Str[(size_t)I]; }
	FString& operator+=(const FString& O) { Str += O.Str; return *this; }
	FString& operator+=(const char* O) { Str += O; return *this; }
	FString& operator+=(char C) { Str += C; return *this; }
	// Como en UE: el carácter nulo no se añade.
	FString& AppendChar(char C) { if (C != 0) { Str += C; } return *this; }
	FString& Append(const FString& O) { Str += O.Str; return *this; }
	friend FString operator+(const FString& A, const FString& B) { return FString(A.Str + B.Str); }
	friend FString operator+(const FString& A, const char* B) { return FString(A.Str + B); }
	friend FString operator+(const char* A, const FString& B) { return FString(std::string(A) + B.Str); }
	bool operator==(const FString& O) const { return Equals(O, true); }
	bool operator!=(const FString& O) const { return !(*this == O); }
	bool operator<(const FString& O) const { return Lower(Str) < Lower(O.Str); }
	bool Equals(const FString& O, bool bCaseSensitive = true) const { return bCaseSensitive ? Str == O.Str : Lower(Str) == Lower(O.Str); }
	/** Firma de Unreal (FString::Equals con ESearchCase). */
	bool Equals(const FString& O, ESearchCase::Type SearchCase) const { return Equals(O, SearchCase == ESearchCase::CaseSensitive); }
	bool Equals(const char* O, ESearchCase::Type SearchCase) const { return Equals(FString(O), SearchCase); }
	int32 Compare(const FString& O) const { return Str.compare(O.Str); }
	bool StartsWith(const FString& P) const { return Str.rfind(P.Str, 0) == 0; }
	bool EndsWith(const FString& P) const { return Str.size() >= P.Str.size() && Str.compare(Str.size() - P.Str.size(), P.Str.size(), P.Str) == 0; }
	bool Contains(const FString& P) const { return Str.find(P.Str) != std::string::npos; }
	int32 Find(const FString& P) const { const size_t I = Str.find(P.Str); return I == std::string::npos ? INDEX_NONE : (int32)I; }
	FString Left(int32 N) const { return FString(Str.substr(0, (size_t)FMath::Max(0, N))); }
	FString Right(int32 N) const { const size_t L = Str.size(); const size_t K = (size_t)FMath::Clamp(N, 0, (int32)L); return FString(Str.substr(L - K)); }
	FString Mid(int32 Start, int32 Count = MAX_int32) const { return Start >= Len() ? FString() : FString(Str.substr((size_t)Start, (size_t)Count)); }
	FString ToLower() const { return FString(Lower(Str)); }
	FString ToUpper() const { std::string R = Str; for (char& C : R) { C = (char)std::toupper((unsigned char)C); } return FString(R); }
	FString TrimStartAndEnd() const
	{
		const size_t A = Str.find_first_not_of(" \t\r\n");
		if (A == std::string::npos) { return FString(); }
		const size_t B = Str.find_last_not_of(" \t\r\n");
		return FString(Str.substr(A, B - A + 1));
	}
	FString Replace(const char* From, const char* To) const
	{
		std::string R = Str;
		const std::string F(From), T(To);
		if (F.empty()) { return FString(R); }
		for (size_t P = R.find(F); P != std::string::npos; P = R.find(F, P + T.size())) { R.replace(P, F.size(), T); }
		return FString(R);
	}
	int32 ParseIntoArray(TArray<FString>& Out, const char* Delim, bool bCullEmpty = true) const
	{
		Out.Reset();
		const std::string D(Delim);
		size_t Start = 0;
		while (true)
		{
			const size_t P = Str.find(D, Start);
			const std::string Part = Str.substr(Start, P == std::string::npos ? std::string::npos : P - Start);
			if (!bCullEmpty || !Part.empty()) { Out.Add(FString(Part)); }
			if (P == std::string::npos) { break; }
			Start = P + D.size();
		}
		return Out.Num();
	}
	const std::string& Std() const { return Str; }

private:
	static std::string Lower(const std::string& S) { std::string R = S; for (char& C : R) { C = (char)std::tolower((unsigned char)C); } return R; }
	std::string Str;
};

inline uint32 GetTypeHash(const FString& S) { return (uint32)std::hash<std::string>()(S.ToLower().Std()); }

/**
 * TCString/FCString (Misc/CString.h): solo las conversiones numéricas. Como en Unreal,
 * Atod sigue a strtod (redondeo correcto, locale «C») y Atoi64 no detecta desbordamientos.
 */
template <typename T>
struct TCString
{
	static double Atod(const T* S) { return S ? std::strtod(S, nullptr) : 0.0; }
	static float Atof(const T* S) { return (float)Atod(S); }
	static int32 Atoi(const T* S) { return S ? (int32)std::strtol(S, nullptr, 10) : 0; }
	static int64 Atoi64(const T* S) { return S ? (int64)std::strtoll(S, nullptr, 10) : 0; }
	static int32 Strlen(const T* S) { return S ? (int32)std::strlen(S) : 0; }
};
using FCString = TCString<TCHAR>;
using FCStringAnsi = TCString<ANSICHAR>;

enum EName { NAME_None };

/** FName: en Unreal compara sin distinguir mayúsculas; aquí igual. */
class FName
{
public:
	FName() = default;
	FName(EName) {}
	FName(const char* S) : Name(S) {}
	FName(const FString& S) : Name(S) {}
	bool IsNone() const { return Name.IsEmpty() || Name.Equals(FString("None"), false); }
	bool IsValid() const { return true; }
	FString ToString() const { return IsNone() ? FString("None") : Name; }
	bool operator==(const FName& O) const { return (IsNone() && O.IsNone()) || Name.Equals(O.Name, false); }
	bool operator!=(const FName& O) const { return !(*this == O); }
	bool operator==(EName) const { return IsNone(); }
	bool operator!=(EName) const { return !IsNone(); }
	bool LexicalLess(const FName& O) const { return Name < O.Name; }
	bool FastLess(const FName& O) const { return Name < O.Name; }

private:
	FString Name;
};
inline uint32 GetTypeHash(const FName& N) { return GetTypeHash(N.IsNone() ? FString() : N.ToString()); }

class FText
{
public:
	FText() = default;
	static FText FromString(const FString& S) { FText T; T.Str = S; return T; }
	static FText FromName(const FName& N) { return FromString(N.ToString()); }
	static FText GetEmpty() { return FText(); }
	const FString& ToString() const { return Str; }
	bool IsEmpty() const { return Str.IsEmpty(); }

private:
	FString Str;
};
#define LOCTEXT(Key, Text) FText::FromString(FString(Text))
#define NSLOCTEXT(Ns, Key, Text) FText::FromString(FString(Text))

inline FString LexToString(int32 V) { return FString::FromInt(V); }
inline FString LexToString(float V) { return FString::SanitizeFloat(V); }
inline FString LexToString(const FName& V) { return V.ToString(); }

struct FSoftObjectPath
{
	FSoftObjectPath() = default;
	FSoftObjectPath(const FString& P) : Path(P) {}
	FSoftObjectPath(const char* P) : Path(P) {}
	FString ToString() const { return Path; }
	bool IsValid() const { return !Path.IsEmpty(); }
	bool IsNull() const { return Path.IsEmpty(); }
	bool operator==(const FSoftObjectPath& O) const { return Path == O.Path; }
	FString Path;
};

struct FMemory
{
	template <typename T> static void Memzero(T& V) { std::memset((void*)&V, 0, sizeof(T)); }
	static void Memzero(void* P, SIZE_T N) { std::memset(P, 0, N); }
	static void* Memcpy(void* D, const void* S, SIZE_T N) { return std::memcpy(D, S, N); }
	template <typename T> static void Memcpy(T& D, const T& S) { std::memcpy((void*)&D, (const void*)&S, sizeof(T)); }
	static void* Memset(void* D, uint8 V, SIZE_T N) { return std::memset(D, V, N); }
	static int32 Memcmp(const void* A, const void* B, SIZE_T N) { return std::memcmp(A, B, N); }
};

/** En Unreal llega a través de CoreMinimal (HAL/CriticalSection.h). */
class FCriticalSection
{
public:
	void Lock() { M.lock(); }
	void Unlock() { M.unlock(); }

private:
	std::recursive_mutex M;
};

#include "Math/HostMath.h"
