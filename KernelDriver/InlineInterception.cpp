#include "InlineInterception.h"

#include "ProcessMemory.h"

namespace
{
constexpr size_t kJumpInstructionSize = 12;
UCHAR gOriginalBytes[kJumpInstructionSize]{};
volatile LONG gHookInstalled = FALSE;
} // namespace

NTSTATUS InlineInterception::Install(void* targetFunction, void* replacementFunction)
{
    if (targetFunction == nullptr || replacementFunction == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (InterlockedCompareExchange(&gHookInstalled, TRUE, FALSE) != FALSE) {
        return STATUS_ALREADY_REGISTERED;
    }

    UCHAR jumpInstruction[kJumpInstructionSize]{
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0
    };
    const uintptr_t replacementAddress =
        reinterpret_cast<uintptr_t>(replacementFunction);

    __try {
        RtlCopyMemory(jumpInstruction + 2, &replacementAddress, sizeof(replacementAddress));
        RtlCopyMemory(gOriginalBytes, targetFunction, sizeof(gOriginalBytes));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&gHookInstalled, FALSE);
        return GetExceptionCode();
    }

    const NTSTATUS status = ProcessMemory::WriteKernelMemory(
        targetFunction, jumpInstruction, sizeof(jumpInstruction));
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&gHookInstalled, FALSE);
    }
    return status;
}

NTSTATUS InlineInterception::Restore(void* targetFunction)
{
    if (targetFunction == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }
    if (InterlockedCompareExchange(&gHookInstalled, FALSE, TRUE) != TRUE) {
        return STATUS_NOT_FOUND;
    }

    const NTSTATUS status =
        ProcessMemory::WriteKernelMemory(targetFunction, gOriginalBytes, sizeof(gOriginalBytes));
    if (!NT_SUCCESS(status)) {
        InterlockedExchange(&gHookInstalled, TRUE);
    }
    return status;
}
