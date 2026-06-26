#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include <iostream>

#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shlwapi.lib")


namespace gameversion
{
    bool IsEnhanced();
    bool IsLegacy();
    int GetGameBuild();
}