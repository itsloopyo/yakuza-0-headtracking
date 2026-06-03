#include "mod.h"

#include <windows.h>

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        yakuza0::StartModAsync();
    }
    return TRUE;
}
