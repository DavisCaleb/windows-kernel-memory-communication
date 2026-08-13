#pragma once

#include <ntifs.h>

class ProcessMemory
{
public:
    ProcessMemory() = default;
    ~ProcessMemory();

    ProcessMemory(const ProcessMemory&) = delete;
    ProcessMemory& operator=(const ProcessMemory&) = delete;

    void* FindTargetModuleBase(const wchar_t* targetModuleName) const;
    void* GetTargetImageBase() const;

    NTSTATUS SetClientProcess(int processId);
    NTSTATUS SetTargetProcess(int processId);

    NTSTATUS ReadProcessMemory(void* sourceAddress, void* clientBuffer, size_t size) const;
    NTSTATUS WriteProcessMemory(void* targetProcessAddress, const void* clientBuffer, size_t size) const;
    NTSTATUS ValidatedReadProcessMemory(void* sourceAddress, void* clientBuffer, size_t size) const;

    static NTSTATUS WriteKernelMemory(void* targetProcessAddress, const void* sourceBuffer, size_t size);

private:
    PEPROCESS clientProcess_ = nullptr;
    PEPROCESS targetProcess_ = nullptr;
};
