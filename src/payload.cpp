#include <iostream>

__declspec(dllexport)
bool DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
    if (fdwReason != DLL_PROCESS_ATTACH)
        return true;

    return true;
}