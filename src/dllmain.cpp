#include "Hooking.h"
#include "EditorEncoder.h"
#include "EditorExport.h"
#include "Logging.h"
#include "GameVersion.h"


DWORD WINAPI Main()
{
	int version = gameversion::GetGameBuild();
	logging::Initialize("BetterVideoExport", "BetterVideoExport.log");
	logging::LogStartupBanner(version);

	HookFunctionBase::RunAll();
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		Main();
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{ }

	return TRUE;
}