#pragma once
#include "framework.h"


namespace hooks {

	extern void ApplyVMTHooks();
	extern void RemoveVMTHooks();
	extern void ApplyMemoryPatches();
	extern void ApplyHooks();
	extern void DestroyHooks();

}

