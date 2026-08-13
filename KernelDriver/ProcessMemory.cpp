#include "ProcessMemory.h"

#include "UndocumentedStructures.h"

namespace
{
void ReplaceProcessReference(PEPROCESS& destination, PEPROCESS replacement)
{
    if (destination != nullptr) {
        ObDereferenceObject(destination);
    }
    destination = replacement;
}
} // namespace

ProcessMemory::~ProcessMemory()
{
    ReplaceProcessReference(clientProcess_, nullptr);
    ReplaceProcessReference(targetProcess_, nullptr);
}

void* ProcessMemory::FindTargetModuleBase(const wchar_t* targetModuleName) const
{
    if (targetProcess_ == nullptr || targetModuleName == nullptr || targetModuleName[0] == L'\0') {
        return nullptr;
    }

    UNICODE_STRING requestedName{};
    RtlInitUnicodeString(&requestedName, targetModuleName);

    PPEB peb = PsGetProcessPeb(targetProcess_);
    if (peb == nullptr) {
        return nullptr;
    }

    void* moduleBase = nullptr;
    KAPC_STATE state{};
    KeStackAttachProcess(targetProcess_, &state);

    __try {
        if (peb->Ldr != nullptr) {
            const PLIST_ENTRY head = &peb->Ldr->InMemoryOrderModuleList;
            for (PLIST_ENTRY link = head->Flink; link != head; link = link->Flink) {
                const auto entry = CONTAINING_RECORD(
                    link, LDR_DATA_TABLE_ENTRY, InMemoryOrderModuleList);
                if (RtlCompareUnicodeString(&entry->BaseDllName, &requestedName, TRUE) == 0) {
                    moduleBase = entry->DllBase;
                    break;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        moduleBase = nullptr;
    }

    KeUnstackDetachProcess(&state);
    return moduleBase;
}

void* ProcessMemory::GetTargetImageBase() const
{
    if (targetProcess_ == nullptr) {
        return nullptr;
    }

    PPEB peb = PsGetProcessPeb(targetProcess_);
    if (peb == nullptr) {
        return nullptr;
    }

    void* imageBase = nullptr;
    KAPC_STATE state{};
    KeStackAttachProcess(targetProcess_, &state);
    __try {
        imageBase = peb->ImageBaseAddress;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        imageBase = nullptr;
    }
    KeUnstackDetachProcess(&state);
    return imageBase;
}

NTSTATUS ProcessMemory::SetClientProcess(int processId)
{
    if (processId <= 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PEPROCESS process = nullptr;
    const NTSTATUS status = PsLookupProcessByProcessId(
        reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(processId)), &process);
    if (NT_SUCCESS(status)) {
        ReplaceProcessReference(clientProcess_, process);
    }
    return status;
}

NTSTATUS ProcessMemory::SetTargetProcess(int processId)
{
    if (processId <= 0) {
        return STATUS_INVALID_PARAMETER;
    }

    PEPROCESS process = nullptr;
    const NTSTATUS status = PsLookupProcessByProcessId(
        reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(processId)), &process);
    if (NT_SUCCESS(status)) {
        ReplaceProcessReference(targetProcess_, process);
    }
    return status;
}

NTSTATUS ProcessMemory::ReadProcessMemory(
    void* sourceAddress, void* clientBuffer, size_t size) const
{
    if (targetProcess_ == nullptr || clientProcess_ == nullptr ||
        sourceAddress == nullptr || clientBuffer == nullptr || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T bytesCopied = 0;
    __try {
        return MmCopyVirtualMemory(targetProcess_, sourceAddress, clientProcess_, clientBuffer,
            size, KernelMode, &bytesCopied);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}

NTSTATUS ProcessMemory::WriteProcessMemory(
    void* targetProcessAddress, const void* clientBuffer, size_t size) const
{
    if (targetProcess_ == nullptr || clientProcess_ == nullptr ||
        targetProcessAddress == nullptr || clientBuffer == nullptr || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SIZE_T bytesCopied = 0;
    __try {
        return MmCopyVirtualMemory(clientProcess_, const_cast<void*>(clientBuffer),
            targetProcess_, targetProcessAddress, size, KernelMode, &bytesCopied);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}

NTSTATUS ProcessMemory::ValidatedReadProcessMemory(
    void* sourceAddress, void* clientBuffer, size_t size) const
{
    if (targetProcess_ == nullptr || sourceAddress == nullptr ||
        clientBuffer == nullptr || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_ACCESS_VIOLATION;
    KAPC_STATE state{};
    KeStackAttachProcess(targetProcess_, &state);
    __try {
        if (MmIsAddressValid(sourceAddress)) {
            status = ReadProcessMemory(sourceAddress, clientBuffer, size);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }
    KeUnstackDetachProcess(&state);
    return status;
}

NTSTATUS ProcessMemory::WriteKernelMemory(
    void* targetProcessAddress, const void* sourceBuffer, size_t size)
{
    if (targetProcessAddress == nullptr || sourceBuffer == nullptr || size == 0 ||
        size > static_cast<size_t>(MAXULONG)) {
        return STATUS_INVALID_PARAMETER;
    }

    PMDL mdl = IoAllocateMdl(
        targetProcessAddress, static_cast<ULONG>(size), FALSE, FALSE, nullptr);
    if (mdl == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    bool pagesLocked = false;
    void* mappedAddress = nullptr;
    NTSTATUS status = STATUS_SUCCESS;

    __try {
        MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
        pagesLocked = true;
        mappedAddress = MmMapLockedPagesSpecifyCache(
            mdl, KernelMode, MmNonCached, nullptr, FALSE, NormalPagePriority);
        if (mappedAddress == nullptr) {
            status = STATUS_INSUFFICIENT_RESOURCES;
        } else {
            status = MmProtectMdlSystemAddress(mdl, PAGE_READWRITE);
            if (NT_SUCCESS(status)) {
                RtlCopyMemory(mappedAddress, sourceBuffer, size);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    if (mappedAddress != nullptr) {
        MmUnmapLockedPages(mappedAddress, mdl);
    }
    if (pagesLocked) {
        MmUnlockPages(mdl);
    }
    IoFreeMdl(mdl);
    return status;
}
