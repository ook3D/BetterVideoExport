/*
 * This file is part of the CitizenFX project - http://citizen.re/
 * https://github.com/citizenfx/fivem/blob/master/code/client/shared/Hooking.cpp
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "Hooking.h"

namespace hook
{
	static LPVOID FindPrevFreeRegion(LPVOID pAddress, LPVOID pMinAddr, DWORD dwAllocationGranularity)
	{
		ULONG_PTR tryAddr = (ULONG_PTR)pAddress;

		// Round down to the next allocation granularity.
		tryAddr -= tryAddr % dwAllocationGranularity;

		// Start from the previous allocation granularity multiply.
		tryAddr -= dwAllocationGranularity;

		while (tryAddr >= (ULONG_PTR)pMinAddr)
		{
			MEMORY_BASIC_INFORMATION mbi;
			if (VirtualQuery((LPVOID)tryAddr, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) ==
				0)
				break;

			if (mbi.State == MEM_FREE)
				return (LPVOID)tryAddr;

			if ((ULONG_PTR)mbi.AllocationBase < dwAllocationGranularity)
				break;

			tryAddr = (ULONG_PTR)mbi.AllocationBase - dwAllocationGranularity;
		}

		return NULL;
	}

	// Size of each memory block. (= page size of VirtualAlloc)
	const uint64_t MEMORY_BLOCK_SIZE = 0x1000;

	// Max range for seeking a memory block. (= 1024MB)
	const uint64_t MAX_MEMORY_RANGE = 0x40000000;

	void* AllocateFunctionStub(void* ptr, int type)
	{
#if defined(GTA_FIVE) || defined(IS_RDR3)
		typedef void*(*AllocateType)(void*, int);
		static AllocateType func;

		if (func == nullptr)
		{
			HMODULE coreRuntime = GetModuleHandleW(L"CoreRT.dll");
			func = (AllocateType)GetProcAddress(coreRuntime, "AllocateFunctionStubImpl");
		}

		return func(ptr, type);
#else
		// Standalone: allocate a stub within ±2GB that does mov <reg>, imm64; jmp <reg>
		uint8_t* stub = (uint8_t*)AllocateStubMemory(64);
		if (!stub)
			return ptr;

		size_t off = 0;
		uint8_t reg = type & 7;

		// REX.W prefix (+ REX.B for r8-r15)
		stub[off++] = (type >= 8) ? 0x49 : 0x48;
		stub[off++] = 0xB8 + reg;   // mov <reg>, imm64
		*(uint64_t*)(stub + off) = (uint64_t)ptr;
		off += 8;

		// jmp <reg>: FF E0+reg (with 0x41 REX prefix for r8-r15)
		if (type >= 8)
			stub[off++] = 0x41;
		stub[off++] = 0xFF;
		stub[off++] = 0xE0 + reg;

		return stub;
#endif
	}

	void* AllocateStubMemory(size_t size)
	{
		void* origin = GetModuleHandle(NULL);

		ULONG_PTR minAddr;
		ULONG_PTR maxAddr;

		SYSTEM_INFO si;
		GetSystemInfo(&si);
		minAddr = (ULONG_PTR)si.lpMinimumApplicationAddress;
		maxAddr = (ULONG_PTR)si.lpMaximumApplicationAddress;

		if ((ULONG_PTR)origin > MAX_MEMORY_RANGE &&
			minAddr < (ULONG_PTR)origin - MAX_MEMORY_RANGE)
			minAddr = (ULONG_PTR)origin - MAX_MEMORY_RANGE;

		if (maxAddr > (ULONG_PTR)origin + MAX_MEMORY_RANGE)
			maxAddr = (ULONG_PTR)origin + MAX_MEMORY_RANGE;

		LPVOID pAlloc = origin;

		void* stub = nullptr;
		while ((ULONG_PTR)pAlloc >= minAddr)
		{
			pAlloc = FindPrevFreeRegion(pAlloc, (LPVOID)minAddr, si.dwAllocationGranularity);
			if (pAlloc == NULL)
				break;

			stub = VirtualAlloc(pAlloc, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (stub != NULL)
				break;
		}

		return stub;
	}
}