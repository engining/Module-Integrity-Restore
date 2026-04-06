#include "ModuleIntegrity.hpp"

#include <TlHelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <vector>
#include <cstdio>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;

typedef NTSTATUS(NTAPI* fnNtCreateSection)(
    PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
    PLARGE_INTEGER, ULONG, ULONG, HANDLE);

typedef NTSTATUS(NTAPI* fnNtMapViewOfSection)(
    HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T,
    PLARGE_INTEGER, PSIZE_T, SECTION_INHERIT, ULONG, ULONG);

typedef NTSTATUS(NTAPI* fnNtUnmapViewOfSection)(
    HANDLE, PVOID);

void ModuleIntegrity::SetProcessState(DWORD pid, bool suspend)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    if (Thread32First(hSnap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid)
                continue;

            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
            if (!hThread)
                continue;

            suspend ? SuspendThread(hThread) : ResumeThread(hThread);
            CloseHandle(hThread);
        } while (Thread32Next(hSnap, &te));
    }

    CloseHandle(hSnap);
}


// Restore – dá unmap no módulo, remapeia do disco original
bool ModuleIntegrity::Restore(DWORD pid, LPCWSTR moduleName)
{
  
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return false;

    auto NtCreateSection = (fnNtCreateSection)GetProcAddress(hNtdll, "NtCreateSection");
    auto NtMapViewOfSection = (fnNtMapViewOfSection)GetProcAddress(hNtdll, "NtMapViewOfSection");
    auto NtUnmapViewOfSection = (fnNtUnmapViewOfSection)GetProcAddress(hNtdll, "NtUnmapViewOfSection");

    if (!NtCreateSection || !NtMapViewOfSection || !NtUnmapViewOfSection)
        return false;


    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap == INVALID_HANDLE_VALUE) {
        CloseHandle(hProc);
        return false;
    }

    PVOID        baseAddr = nullptr;
    DWORD64      modSize = 0;
    std::wstring fullPath;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);

    if (Module32FirstW(hSnap, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) {
                baseAddr = me.modBaseAddr;
                modSize = me.modBaseSize;
                fullPath = me.szExePath;
                break;
            }
        } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);

    if (!baseAddr) {
        CloseHandle(hProc);
        return false;
    }

    //  backup da imagem do módulo na memória 
    std::vector<BYTE> backup(modSize);
    if (!ReadProcessMemory(hProc, baseAddr, backup.data(), modSize, nullptr)) {
        CloseHandle(hProc);
        return false;
    }

    // cria section a partir do arquivo original no disco 
    HANDLE hFile = CreateFileW(
        fullPath.c_str(), GENERIC_READ | GENERIC_EXECUTE,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hProc);
        return false;
    }

    HANDLE hSection = nullptr;
    NTSTATUS status = NtCreateSection(
        &hSection, SECTION_ALL_ACCESS, nullptr, nullptr,
        PAGE_READONLY, SEC_IMAGE, hFile);
    CloseHandle(hFile);

    if (status != 0 || !hSection) {
        CloseHandle(hProc);
        return false;
    }

    // suspende, unmap, remap 
    SetProcessState(pid, true);

    status = NtUnmapViewOfSection(hProc, baseAddr);
    if (status != 0) {
        SetProcessState(pid, false);
        CloseHandle(hSection);
        CloseHandle(hProc);
        return false;
    }

    PVOID  viewBase = baseAddr;
    SIZE_T viewSize = 0;
    LARGE_INTEGER offset{};

    status = NtMapViewOfSection(
        hSection, hProc, &viewBase, 0, 0,
        &offset, &viewSize, ViewUnmap, 0, PAGE_EXECUTE_READ);

    if (status != 0) {
        // remap falhou 
        SetProcessState(pid, false);
        CloseHandle(hSection);
        CloseHandle(hProc);
        return false;
    }


    auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(backup.data());
    auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(backup.data() + dos->e_lfanew);
    auto* sections = IMAGE_FIRST_SECTION(nt);

    // escreve os headers do PE
    WriteProcessMemory(hProc, baseAddr, backup.data(),
        nt->OptionalHeader.SizeOfHeaders, nullptr);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        // pula sections executáveis 
        if (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)
            continue;

        auto* target = reinterpret_cast<BYTE*>(baseAddr) + sections[i].VirtualAddress;
        auto* source = backup.data() + sections[i].VirtualAddress;
        DWORD size = sections[i].Misc.VirtualSize;

        DWORD oldProtect = 0;
        VirtualProtectEx(hProc, target, size, PAGE_READWRITE, &oldProtect);
        WriteProcessMemory(hProc, target, source, size, nullptr);
        VirtualProtectEx(hProc, target, size, oldProtect, &oldProtect);
    }

    // limpeza 
    EmptyWorkingSet(hProc);
    SetProcessState(pid, false);

    CloseHandle(hSection);
    CloseHandle(hProc);
    return true;
}