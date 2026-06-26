/*
 * This file is part of the CitizenFX project - http://citizen.re/
 * https://github.com/citizenfx/fivem/blob/master/code/client/shared/HookFunction.cpp
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "HookFunction.h"

#include <exception>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

static HookFunctionBase* g_hookFunctions;

void HookFunctionBase::Register()
{
	m_next = g_hookFunctions;
	g_hookFunctions = this;
}

void HookFunctionBase::RunAll()
{
	for (auto func = g_hookFunctions; func; func = func->m_next)
	{
		try
		{
			func->Run();
		}
		catch (const std::exception& ex)
		{
			MessageBoxA(nullptr, ex.what(), "ASI: hook failed", MB_OK | MB_ICONERROR);
		}
		catch (...)
		{
			MessageBoxA(nullptr, "Unknown exception in HookFunctionBase::RunAll", "ASI: hook failed", MB_OK | MB_ICONERROR);
		}
	}
}

static RuntimeHookFunction* g_runtimeHookFunctions;

void RuntimeHookFunction::Register()
{
	m_next = g_runtimeHookFunctions;
	g_runtimeHookFunctions = this;
}

void RuntimeHookFunction::Run(const char* key)
{
	for (auto func = g_runtimeHookFunctions; func; func = func->m_next)
	{
		if (func->m_key == key)
		{
			try
			{
				func->m_function();
			}
			catch (const std::exception& ex)
			{
				MessageBoxA(nullptr, ex.what(), "ASI: runtime hook failed", MB_OK | MB_ICONERROR);
			}
			catch (...)
			{
				MessageBoxA(nullptr, "Unknown exception in RuntimeHookFunction::Run", "ASI: runtime hook failed", MB_OK | MB_ICONERROR);
			}
		}
	}
}
