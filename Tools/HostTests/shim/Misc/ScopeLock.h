// Shim: FCriticalSection / FScopeLock sobre std::mutex.
#pragma once
#include "CoreMinimal.h"

class FScopeLock
{
public:
	explicit FScopeLock(FCriticalSection* InCs) : Cs(InCs) { Cs->Lock(); }
	~FScopeLock() { Cs->Unlock(); }
	FScopeLock(const FScopeLock&) = delete;
	FScopeLock& operator=(const FScopeLock&) = delete;

private:
	FCriticalSection* Cs;
};
