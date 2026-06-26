#include "GameVersion.h"

namespace gameversion
{
    namespace
    {
        bool initialized = false;
        bool enhanced = false;

        void Initialize()
        {
            if (initialized) return;

            char path[MAX_PATH];
            if (GetModuleFileNameA(NULL, path, MAX_PATH) != 0)
            {
                const char* exeName = PathFindFileNameA(path);
                enhanced = (_stricmp(exeName, "GTA5_Enhanced.exe") == 0);
            }
            initialized = true;
        }
    }

    bool IsEnhanced()
    {
        Initialize();
        return enhanced;
    }

    bool IsLegacy()
    {
        Initialize();
        return !enhanced;
    }

    int GetGameBuild()
    {
        const wchar_t* targetExe = IsEnhanced() ? L"GTA5_Enhanced.exe" : L"GTA5.exe";

        DWORD gta5Pid = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return -1;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, targetExe) == 0)
                {
                    gta5Pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);

        if (gta5Pid == 0)
            return -1;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, gta5Pid);
        if (!hProcess)
            return -1;

        char exePath[MAX_PATH] = { 0 };
        if (!GetModuleFileNameExA(hProcess, NULL, exePath, MAX_PATH)) {
            CloseHandle(hProcess);
            return -1;
        }
        CloseHandle(hProcess);

        DWORD handle = 0;
        DWORD versionSize = GetFileVersionInfoSizeA(exePath, &handle);
        if (versionSize == 0)
            return -1;

        std::vector<char> versionData(versionSize);
        if (!GetFileVersionInfoA(exePath, 0, versionSize, versionData.data()))
            return -1;

        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT len = 0;
        if (!VerQueryValueA(versionData.data(), "\\", reinterpret_cast<LPVOID*>(&fileInfo), &len) || fileInfo == nullptr)
            return -1;

        DWORD build = (fileInfo->dwProductVersionLS >> 16) & 0xffff;
        return static_cast<int>(build);
    }

}