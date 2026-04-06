#pragma once

#include <Windows.h>
#include <string>

/// @brief PoC: Restauração de integridade de módulo via NtUnmapViewOfSection / NtMapViewOfSection.
/// Remapeia o módulo a partir do arquivo original no disco e restaura
/// os dados das sections não-executáveis (.data, .rdata, etc.) a partir de um backup.
/// As sections executáveis (.text) são substituídas pela versão limpa do binário.
class ModuleIntegrity {
public:
    /// @brief Restaura um módulo pro estado original do disco dentro de um processo remoto.
    /// @param pid        PID do processo alvo.
    /// @param moduleName Nome do módulo (ex: L"ntdll.dll").
    /// @return true se deu certo.
    static bool Restore(DWORD pid, LPCWSTR moduleName);

private:
    /// @brief Suspende ou resume todas as threads de um processo.
    static void SetProcessState(DWORD pid, bool suspend);
};