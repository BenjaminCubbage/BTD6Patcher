#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <unordered_map>

#ifndef BLOONSTD6_ROOT
#error "BLOONSTD6_ROOT was not defined!"
#endif

#ifndef PAYLOAD_DLL
#error "PAYLOAD_DLL was not defined!"
#endif

#ifndef KERNEL32_DLL
#error "KERNEL32_DLL was not defined!"
#endif

/*
    Normalize the path, usually for comparing two paths for
    equality.
*/
static std::string canonicalizePath(std::string path) {
    return std::filesystem::canonical(path).string();
}

/*
    Get the RVA of the LoadLibraryA function in another
    process's address space.
*/
static uintptr_t getLoadLibraryARVA(uintptr_t kernel32Base) {
    HMODULE hModule = GetModuleHandleA(KERNEL32_DLL);
    if (hModule == nullptr)
        return 0;

    uintptr_t localRVA = reinterpret_cast<uintptr_t>(
        GetProcAddress(hModule, "LoadLibraryA"));
    if (localRVA == 0)
        return 0;

    return kernel32Base + localRVA - reinterpret_cast<uintptr_t>(hModule);
}

int main() {
    const std::string exePath = BLOONSTD6_ROOT "\\BloonsTD6.exe";

    /*
        Start BTD6 as a debugged process

        TODO: Check success
    */
    STARTUPINFOA startupInfo{
        .cb = sizeof(STARTUPINFOA)
    };
    PROCESS_INFORMATION pi{};
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
        &pi);
    DebugSetProcessKillOnExit(true);

    std::cout
        << "BTD6Patcher: Waiting for child process to load kernel32.dll..."
        << std::endl;

    /*
        Keep track of the spawned threads so we can
        suspend them later.
    */
    std::unordered_map<int, HANDLE> spawnedThreads = {
        { pi.dwThreadId, pi.hThread }
    };

    /*
        Save dll base when kernel32 is loaded.
    */
    uintptr_t kernel32Base{};

    /*
        Loop through debug events until game assembly is
        loaded or process dies.
    */
    for (DEBUG_EVENT event;;) {
        if (WaitForDebugEvent(&event, 5000) == 0) {
            std::cout
                << "\x1b[91m"
                   "BTD6Patcher: WaitForDebugEvent failed or timed out. Code: "
                << GetLastError()
                << "\x1b[0m"
                << std::endl;

            return 1;
        }

        if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            /* Loaded DLL, check if it's kernel32.dll */
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
                << "\x1b[90m"
                   "BTD6Patcher: Game loaded library "
                << dllPath
                << "\x1b[0m"
                << std::endl;

            if (dllPath == canonicalizePath(KERNEL32_DLL)) {
                /* Found it! */
                kernel32Base = reinterpret_cast<uintptr_t>(
                    event.u.LoadDll.lpBaseOfDll);
                break;
            }
        }

        if (event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) {
            /* Track spawned thread */
            spawnedThreads.insert_or_assign(event.dwThreadId, event.u.CreateThread.hThread);
        } else if (event.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT) {
            /* Stop tracking spawned thread */
            spawnedThreads.erase(event.dwThreadId);
        }

        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            /* Don't care */
            CloseHandle(event.u.CreateProcessInfo.hFile);
        } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            /* Process died :( */
            std::cout
                << "\x1b[91m"
                   "BTD6Patcher: Process exited before kernel32.dll was loaded! "
                   "Does it exist? Expected at: " KERNEL32_DLL
                << "\x1b[0m"
                << std::endl;
            return 1;
        }

        ContinueDebugEvent(
            event.dwProcessId,
            event.dwThreadId,
            DBG_CONTINUE);
    }

    std::cout
        << "BTD6Patcher: kernel32.dll was loaded! "
        << std::endl
        << "BTD6Patcher: Suspending threads..."
        << std::endl;

    /*
        Suspend all spawned threads.

        Since we stopped at kernel32 loading, it's probably
        just the main thread. But we are being good.
    */
    for (const auto& [_, handle] : spawnedThreads)
        SuspendThread(handle);

    /*
        Allocate payload dll path string
    */
    void* remoteStringMem = VirtualAllocEx(
        pi.hProcess,
        nullptr,
        sizeof(PAYLOAD_DLL),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);

    WriteProcessMemory(
        pi.hProcess,
        remoteStringMem,
        PAYLOAD_DLL,
        sizeof(PAYLOAD_DLL),
        nullptr);

    uintptr_t loadLibraryRVA = getLoadLibraryARVA(kernel32Base);

    const uint8_t stub[] = {
        0x48, 0xB9,                                                      // v
        reinterpret_cast<uintptr_t>(remoteStringMem) & 0xFF,             // v
        reinterpret_cast<uintptr_t>(remoteStringMem) & 0xFF00     >> 8,  // v
        reinterpret_cast<uintptr_t>(remoteStringMem) & 0xFF0000   >> 16, // v
        reinterpret_cast<uintptr_t>(remoteStringMem) & 0xFF000000 >> 24, // mov rcx, remoteStringMem
        0x48, 0xB8,                                                      // v
        loadLibraryRVA & 0xFF,                                           // v
        loadLibraryRVA & 0xFF00     >> 8,                                // v
        loadLibraryRVA & 0xFF0000   >> 16,                               // v
        loadLibraryRVA & 0xFF000000 >> 24,                               // mov rax, loadLibraryRVA
        0xFF, 0xE0                                                       // jmp rax
    };
    
    /*
        Inject stub
    */
    void* stubMem = VirtualAllocEx(
        pi.hProcess,
        nullptr,
        sizeof(stub),
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);

    WriteProcessMemory(
        pi.hProcess,
        stubMem,
        stub,
        sizeof(stub),
        nullptr);

    DebugActiveProcessStop(pi.dwProcessId);
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}