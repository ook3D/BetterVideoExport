/*
 * This file is part of the CitizenFX project - http://citizen.re/
 * https://github.com/citizenfx/fivem/blob/master/code/client/shared/Hooking.Stubs.h
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once
#include <MinHook.h>
#include "Hooking.h"

namespace hook
{
extern void trampoline_raw(void* address, const void* target, void** origTrampoline);

template<typename TFunc, typename TAddr>
TFunc* trampoline(TAddr address, TFunc* target)
{
	TFunc* orig = nullptr;
	trampoline_raw(address, target, (void**)&orig);

	return orig;
}
}
