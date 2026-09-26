#pragma once
// FPlatformTime del shim: reloj monótono en segundos, como FPlatformTime::Seconds de UE.
#include <chrono>

struct FPlatformTime
{
	static double Seconds()
	{
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}
};
