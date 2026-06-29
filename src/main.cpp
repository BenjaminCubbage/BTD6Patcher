#include <filesystem>
#include <iostream>
#include <iterator>

#ifndef BLOONSTD6_ROOT
#error "BLOONSTD6_ROOT was not defined!"
#endif

static std::string canonicalizePath(std::string path) {
    return std::filesystem::canonical(path).string();
}

int main() {
    /* 
        Relevant canonicalized paths 
    */
    const std::string exePath          = canonicalizePath(BLOONSTD6_ROOT "/BloonsTD6.exe");
    const std::string gameAssemblyPath = canonicalizePath(BLOONSTD6_ROOT "/GameAssembly.dll");

    /*
        Start BTD6 as a debugged process

        TODO: Check success
    */
    STARTUPINFOA startupInfo{
        .cb = sizeof(STARTUPINFOA)
    };
    PROCESS_INFORMATION processInfo{};
    CreateProcessA(
        exePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        false,
        DEBUG_ONLY_THIS_PROCESS,
        nullptr,
        BLOONSTD6_ROOT,
        &startupInfo,
        &processInfo);
    DebugSetProcessKillOnExit(false);

    std::cout
        << "BTD6Patcher: Waiting for child process to load the game assembly library..."
        << std::endl;

    /*
        Loop through debug events until game assembly is
        loaded or process dies.
    */
    for (DEBUG_EVENT event;;) {
        WaitForDebugEvent(&event, INFINITE);

        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            /* Loaded DLL, check if it's game assembly */
            const HANDLE hFile = event.u.LoadDll.hFile;

            char dllRawPath[MAX_PATH]{};
            GetFinalPathNameByHandleA(
                hFile,
                dllRawPath,
                std::size(dllRawPath),
                FILE_NAME_NORMALIZED);
            const std::string dllPath = canonicalizePath(dllRawPath);

            CloseHandle(event.u.LoadDll.hFile);

            std::cout 
                << "\x1b[90mBTD6Patcher: Game loaded library "
                << dllPath
                << "\x1b[0m"
                << std::endl;

            if (dllPath == gameAssemblyPath)
                break;
        } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            /* Process died :( */
            std::cout
                << "\x1b[91m"
                   "BTD6Patcher: Process exited before game assembly was loaded! "
                   "Game may already be running, or we are waiting on the wrong "
                   "game assembly path. Path was: "
                << gameAssemblyPath
                << "\x1b[0m"
                << std::endl;
            return 1;
        } else if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
            CloseHandle(event.u.CreateProcessInfo.hFile);

        ContinueDebugEvent(
            event.dwProcessId,
            event.dwThreadId,
            DBG_CONTINUE);
    }

    std::cout
        << "BTD6Patcher: Game assembly library loaded!"
        << std::endl;

    DebugActiveProcessStop(processInfo.dwProcessId);

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
}