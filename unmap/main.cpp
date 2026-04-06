#include <cstdio>
#include <cstdlib>
#include <Windows.h>
#include "ModuleIntegrity.hpp"

static void PrintUsage(const char* exe)
{
    printf("Uso: %s <PID> <NomeDoModulo>\n", exe);
    printf("\n");
    printf("  PID            PID do processo alvo\n");
    printf("  NomeDoModulo   Modulo pra restaurar (ex: ntdll.dll)\n");
    printf("\n");
    printf("Exemplo:\n");
    printf("  %s 1234 ntdll.dll\n", exe);
}

int main(int argc, char* argv[])
{
    printf("[*] Module Integrity Restore - PoC\n");
    printf("[*] Tecnica: NtUnmapViewOfSection + NtMapViewOfSection\n\n");

    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }

    DWORD pid = static_cast<DWORD>(strtoul(argv[1], nullptr, 10));
    if (pid == 0) {
        printf("[-] PID invalido: %s\n", argv[1]);
        return 1;
    }

  
    int len = MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, nullptr, 0);
    std::wstring moduleName(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, moduleName.data(), len);

    printf("[*] PID alvo  : %lu\n", pid);
    printf("[*] Modulo    : %s\n", argv[2]);
    printf("[*] Restaurando...\n");

    bool ok = ModuleIntegrity::Restore(pid, moduleName.c_str());

    if (ok) {
        printf("[+] Modulo restaurado com sucesso.\n");
    }
    else {
        printf("[-] Falhou. Certifica que ta rodando como Administrador.\n");
    }

    return ok ? 0 : 1;
}