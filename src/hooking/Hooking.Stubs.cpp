/*
 * This file is part of the CitizenFX project - http://citizen.re/
 * https://github.com/citizenfx/fivem/blob/master/code/client/shared/Hooking.Stubs.cpp
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "Hooking.Stubs.h"

namespace hook
{
void trampoline_raw(void* address, const void* target, void** origTrampoline)
{
	static auto mhInitializer = ([] {
		return MH_Initialize();
	})();

	auto location = reinterpret_cast<void*>(hook::get_adjusted(address));
	MH_CreateHook(location, const_cast<void*>(target), origTrampoline);
	auto status = MH_EnableHook(location);
}
}
