# Module Integrity Restore – PoC

PoC de restauração de integridade de módulos usando `NtUnmapViewOfSection` + `NtMapViewOfSection`.

## O que faz?

Basicamente o fluxo é:

1. Faz backup do módulo que tá carregado na memória do processo (com os dados de runtime e tudo)
2. Dá unmap no módulo
3. Remapeia uma cópia limpa direto do arquivo original no disco (`SEC_IMAGE`)
4. Reescreve as sections não-executáveis (`.data`, `.rdata`, etc.) usando o backup pra não quebrar o estado do processo

No final, as sections executáveis (`.text`) ficam com o código limpo do disco, e as sections de dados mantêm os valores que o processo tava usando.

## Fluxo

```
1. ReadProcessMemory    → salva backup do módulo inteiro
2. CreateFile           → abre a DLL original do disco
3. NtCreateSection      → cria section com SEC_IMAGE
4. SuspendThreads       → congela as threads do processo
5. NtUnmapViewOfSection → remove o módulo atual
6. NtMapViewOfSection   → mapeia a cópia limpa do disco
7. WriteProcessMemory   → restaura as sections de dados
8. ResumeThreads        → descongela o processo
```

## Como usar

```bash
# precisa rodar como admin
restore_example.exe <PID> <NomeDoModulo>

# exemplo: restaurar ntdll.dll no processo 1234
restore_example.exe 1234 ntdll.dll
```

## Usando no código

```cpp
#include "ModuleIntegrity.hpp"

bool ok = ModuleIntegrity::Restore(targetPid, L"ntdll.dll");
```

## Requisitos

- Windows 10/11 x64
- Privilégio de administrador (precisa de `PROCESS_ALL_ACCESS`)
- MSVC com C++17

## Aviso

Isso aqui é só uma proof of concept pra estudo. Use por sua conta e risco.

## Licença

MIT
