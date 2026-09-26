// Punto de entrada de los tests del host. Filtro opcional por subcadena: ./explored_host_tests Survival
#include "HostTest.h"

#include <cstring>
#include <exception>

int main(int Argc, char** Argv)
{
	const char* Filter = Argc > 1 ? Argv[1] : nullptr;
	int Run = 0;
	int FailedCases = 0;
	for (const HostTest::FCase& Case : HostTest::Registry())
	{
		if (Filter && !std::strstr(Case.Name, Filter)) { continue; }
		++Run;
		const int Before = HostTest::Failures();
		try
		{
			Case.Body();
		}
		catch (const std::exception& E)
		{
			HostTest::Fail(Case.Name, 0, std::string("excepción: ") + E.what());
		}
		const bool bOk = HostTest::Failures() == Before;
		FailedCases += bOk ? 0 : 1;
		std::printf("[%s] %s\n", bOk ? " OK " : "FALLO", Case.Name);
	}
	std::printf("\n%d casos, %d con fallos\n", Run, FailedCases);
	return FailedCases == 0 && Run > 0 ? 0 : 1;
}
