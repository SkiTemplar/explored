// Mini framework de tests para el host (sin dependencias). Uso:
//   HOST_TEST("Grupo.Caso") { EXPECT_TRUE(...); EXPECT_NEAR(a, b, tol); }
#pragma once

#include "CoreMinimal.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace HostTest
{
	struct FCase
	{
		const char* Name;
		std::function<void()> Body;
	};

	inline std::vector<FCase>& Registry()
	{
		static std::vector<FCase> Cases;
		return Cases;
	}

	inline int& Failures()
	{
		static int N = 0;
		return N;
	}

	struct FRegistrar
	{
		FRegistrar(const char* Name, std::function<void()> Body) { Registry().push_back({Name, std::move(Body)}); }
	};

	inline void Fail(const char* File, int Line, const std::string& Msg)
	{
		++Failures();
		std::fprintf(stderr, "  FALLO %s:%d: %s\n", File, Line, Msg.c_str());
	}
}

#define HOST_CONCAT2(A, B) A##B
#define HOST_CONCAT(A, B) HOST_CONCAT2(A, B)
#define HOST_TEST(Name) \
	static void HOST_CONCAT(HostTestBody_, __LINE__)(); \
	static HostTest::FRegistrar HOST_CONCAT(HostTestReg_, __LINE__)(Name, &HOST_CONCAT(HostTestBody_, __LINE__)); \
	static void HOST_CONCAT(HostTestBody_, __LINE__)()

#define EXPECT_TRUE(Cond) \
	do { if (!(Cond)) { HostTest::Fail(__FILE__, __LINE__, "se esperaba cierto: " #Cond); } } while (0)
#define EXPECT_FALSE(Cond) EXPECT_TRUE(!(Cond))
#define EXPECT_EQ(A, B) \
	do { if (!((A) == (B))) { HostTest::Fail(__FILE__, __LINE__, "se esperaba igualdad: " #A " == " #B); } } while (0)
#define EXPECT_NE(A, B) \
	do { if ((A) == (B)) { HostTest::Fail(__FILE__, __LINE__, "se esperaba distinto: " #A " != " #B); } } while (0)
#define EXPECT_NEAR(A, B, Tol) \
	do { const double HostA = (double)(A), HostB = (double)(B); \
		if (!(std::fabs(HostA - HostB) <= (double)(Tol))) { \
			HostTest::Fail(__FILE__, __LINE__, std::string("se esperaba ") + #A " ≈ " #B " (" + std::to_string(HostA) + " vs " + std::to_string(HostB) + ")"); } } while (0)
#define EXPECT_LT(A, B) \
	do { if (!((A) < (B))) { HostTest::Fail(__FILE__, __LINE__, std::string("se esperaba ") + #A " < " #B " (" + std::to_string((double)(A)) + " vs " + std::to_string((double)(B)) + ")"); } } while (0)
#define EXPECT_LE(A, B) \
	do { if (!((A) <= (B))) { HostTest::Fail(__FILE__, __LINE__, std::string("se esperaba ") + #A " <= " #B " (" + std::to_string((double)(A)) + " vs " + std::to_string((double)(B)) + ")"); } } while (0)
#define EXPECT_GT(A, B) EXPECT_LT(B, A)
#define EXPECT_GE(A, B) EXPECT_LE(B, A)
