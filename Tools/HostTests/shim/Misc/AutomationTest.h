// Shim de Misc/AutomationTest.h: permite ejecutar en el host los Automation Specs de
// Source/Explored/Tests que solo usan modelos puros (BEGIN_DEFINE_SPEC, Describe, It,
// BeforeEach/AfterEach, TestTrue/TestEqual/...). Cada It se registra como un caso de
// HostTest con el nombre "<Spec> <Describe...> <It>".
#pragma once

#include "CoreMinimal.h"
#include "HostTest.h"

#include <deque>
#include <memory>

#define WITH_DEV_AUTOMATION_TESTS 1
#define WITH_EDITOR 0

struct EAutomationTestFlags
{
	enum Type : uint32
	{
		EditorContext = 1 << 0,
		ClientContext = 1 << 1,
		ServerContext = 1 << 2,
		CommandletContext = 1 << 3,
		ApplicationContextMask = 0xF,
		SmokeFilter = 1 << 24,
		EngineFilter = 1 << 25,
		ProductFilter = 1 << 26,
		PerfFilter = 1 << 27,
		StressFilter = 1 << 28,
		NegativeFilter = 1 << 29,
		ProductFilterMask = 0xFF000000,
	};
};
inline constexpr EAutomationTestFlags::Type operator|(EAutomationTestFlags::Type A, EAutomationTestFlags::Type B)
{
	return (EAutomationTestFlags::Type)((uint32)A | (uint32)B);
}

namespace HostTestDetail
{
	template <typename T>
	std::string ToText(const T& V)
	{
		if constexpr (std::is_enum_v<T>) { return std::to_string((long long)V); }
		else if constexpr (std::is_arithmetic_v<T>) { return std::to_string(V); }
		else if constexpr (std::is_same_v<T, FString>) { return V.Std(); }
		else if constexpr (std::is_same_v<T, FName>) { return V.ToString().Std(); }
		else if constexpr (std::is_same_v<T, FVector> || std::is_same_v<T, FVector3f>)
		{
			return "(" + std::to_string(V.X) + ", " + std::to_string(V.Y) + ", " + std::to_string(V.Z) + ")";
		}
		else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector2f>)
		{
			return "(" + std::to_string(V.X) + ", " + std::to_string(V.Y) + ")";
		}
		else { return "<valor>"; }
	}
}

class FAutomationSpecBase
{
public:
	explicit FAutomationSpecBase(const char* InName) : SpecName(InName) {}
	virtual ~FAutomationSpecBase() = default;
	virtual void Define() = 0;

	/** Recorre Define() y registra cada It como caso de HostTest. */
	static void Register(std::function<std::shared_ptr<FAutomationSpecBase>()> Factory)
	{
		std::shared_ptr<FAutomationSpecBase> Probe = Factory();
		Probe->Define();
		for (size_t I = 0; I < Probe->Leaves.size(); ++I)
		{
			static std::deque<std::string> Names;
			Names.push_back(Probe->Leaves[I].FullName);
			HostTest::Registry().push_back({Names.back().c_str(), [Factory, I]()
			{
				// Instancia nueva por caso: los miembros del spec no arrastran estado entre Its.
				std::shared_ptr<FAutomationSpecBase> Spec = Factory();
				Spec->Define();
				const FLeaf& Leaf = Spec->Leaves[I];
				for (const auto& Before : Leaf.Befores) { Before(); }
				Leaf.Body();
				for (auto It = Leaf.Afters.rbegin(); It != Leaf.Afters.rend(); ++It) { (*It)(); }
			}});
		}
	}

	// --- API de definición --------------------------------------------------------

	void Describe(const FString& Description, std::function<void()> Body)
	{
		Scopes.push_back({Description.Std(), {}, {}});
		Body();
		Scopes.pop_back();
	}
	void xDescribe(const FString&, std::function<void()>) {}
	void It(const FString& Description, std::function<void()> Body)
	{
		FLeaf Leaf;
		Leaf.FullName = SpecName;
		for (const FScope& S : Scopes)
		{
			Leaf.FullName += " " + S.Name;
			Leaf.Befores.insert(Leaf.Befores.end(), S.Befores.begin(), S.Befores.end());
			Leaf.Afters.insert(Leaf.Afters.end(), S.Afters.begin(), S.Afters.end());
		}
		Leaf.Befores.insert(Leaf.Befores.begin(), RootBefores.begin(), RootBefores.end());
		Leaf.Afters.insert(Leaf.Afters.begin(), RootAfters.begin(), RootAfters.end());
		Leaf.FullName += " " + Description.Std();
		Leaf.Body = std::move(Body);
		Leaves.push_back(std::move(Leaf));
	}
	void xIt(const FString&, std::function<void()>) {}
	void BeforeEach(std::function<void()> Body)
	{
		if (Scopes.empty()) { RootBefores.push_back(std::move(Body)); }
		else { Scopes.back().Befores.push_back(std::move(Body)); }
	}
	void AfterEach(std::function<void()> Body)
	{
		if (Scopes.empty()) { RootAfters.push_back(std::move(Body)); }
		else { Scopes.back().Afters.push_back(std::move(Body)); }
	}

	// --- Aserciones -------------------------------------------------------------------

	void AddError(const FString& Msg, int32 = 0) { HostTest::Fail(SpecName.c_str(), 0, Msg.Std()); }
	void AddWarning(const FString&, int32 = 0) {}
	void AddInfo(const FString&, int32 = 0) {}

	bool TestTrue(const FString& What, bool bValue) { if (!bValue) { AddError(What + FString(": se esperaba cierto")); } return bValue; }
	bool TestFalse(const FString& What, bool bValue) { return TestTrue(What, !bValue); }
	template <typename T> bool TestNotNull(const FString& What, const T* P) { return TestTrue(What, P != nullptr); }
	template <typename T> bool TestNull(const FString& What, const T* P) { return TestTrue(What, P == nullptr); }
	template <typename T> bool TestValid(const FString& What, const T& P) { return TestTrue(What, (bool)P); }

	bool TestEqual(const FString& What, float A, float B, float Tol = UE_KINDA_SMALL_NUMBER) { return Near(What, A, B, Tol); }
	bool TestEqual(const FString& What, double A, double B, double Tol = UE_KINDA_SMALL_NUMBER) { return Near(What, A, B, Tol); }
	bool TestEqual(const FString& What, const FVector& A, const FVector& B, float Tol = UE_KINDA_SMALL_NUMBER)
	{
		return Report(What, A.Equals(B, Tol), A, B);
	}
	bool TestEqual(const FString& What, const FVector2D& A, const FVector2D& B, float Tol = UE_KINDA_SMALL_NUMBER)
	{
		return Report(What, A.Equals(B, Tol), A, B);
	}
	bool TestEqual(const FString& What, const FString& A, const FString& B) { return Report(What, A == B, A, B); }
	bool TestEqual(const FString& What, const char* A, const char* B) { return TestEqual(What, FString(A), FString(B)); }
	bool TestEqual(const FString& What, const FString& A, const char* B) { return TestEqual(What, A, FString(B)); }
	template <typename T, typename U>
	bool TestEqual(const FString& What, const T& A, const U& B)
	{
		if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) { return Report(What, A == (T)B, A, (T)B); }
		else { return Report(What, A == B, A, B); }
	}
	template <typename T, typename U>
	bool TestNotEqual(const FString& What, const T& A, const U& B) { return TestTrue(What, !(A == B)); }
	bool TestNearlyEqual(const FString& What, double A, double B, double Tol = UE_KINDA_SMALL_NUMBER) { return Near(What, A, B, Tol); }

protected:
	struct FScope
	{
		std::string Name;
		std::vector<std::function<void()>> Befores;
		std::vector<std::function<void()>> Afters;
	};
	struct FLeaf
	{
		std::string FullName;
		std::vector<std::function<void()>> Befores;
		std::vector<std::function<void()>> Afters;
		std::function<void()> Body;
	};

	bool Near(const FString& What, double A, double B, double Tol)
	{
		return Report(What, std::fabs(A - B) <= Tol, A, B);
	}
	template <typename T, typename U>
	bool Report(const FString& What, bool bOk, const T& A, const U& B)
	{
		if (!bOk)
		{
			AddError(What + FString(": ") + FString(HostTestDetail::ToText(A)) + FString(" vs ") + FString(HostTestDetail::ToText(B)));
		}
		return bOk;
	}

	std::string SpecName;
	std::vector<FScope> Scopes;
	std::vector<std::function<void()>> RootBefores;
	std::vector<std::function<void()>> RootAfters;
	std::vector<FLeaf> Leaves;
};

#define BEGIN_DEFINE_SPEC(TClass, PrettyName, TFlags) \
	class TClass : public FAutomationSpecBase \
	{ \
	public: \
		TClass() : FAutomationSpecBase(PrettyName) {} \
		virtual void Define() override; \
	private:

#define END_DEFINE_SPEC(TClass) \
	}; \
	namespace { struct TClass##HostRegistrar { TClass##HostRegistrar() { \
		FAutomationSpecBase::Register([]() { return std::shared_ptr<FAutomationSpecBase>(new TClass()); }); } }; \
		static TClass##HostRegistrar TClass##HostRegistrarInstance; }

#define DEFINE_SPEC(TClass, PrettyName, TFlags) \
	BEGIN_DEFINE_SPEC(TClass, PrettyName, TFlags) \
	END_DEFINE_SPEC(TClass)
