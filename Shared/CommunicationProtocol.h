#pragma once

#include <stddef.h>
#include <stdint.h>

// The protocol is intentionally kept in one header so the user-mode client and
// kernel component cannot silently drift apart. It models multiple indexed
// request streams for cross-process read/write and query operations. This remains
// a prototype ABI: volatile fields do not provide production-grade inter-process
// synchronization or make the client API safe for arbitrary concurrent callers.
enum class CommunicationCommand : int
{
    Idle = 0,
    InitializeProcesses = 1,
    SetTargetProcess,
    SetClientProcess,
    WriteProcessMemory,
    ReadProcessMemory,
    GetImageBase,
    GetModuleBase,
    RequestDelay,
    RefreshHeartbeat,
    Stop,
    ReadValidated
};

// Maximum number of independently indexed transport/request streams.
inline constexpr size_t kMaximumWorkerCount = 10;
inline constexpr size_t kModuleNameCapacity = 30;

struct CommunicationRequest
{
    volatile CommunicationCommand command;
    // Process identifiers and addresses are supplied by the user-mode client.
    int targetProcessId;
    int clientProcessId;
    uintptr_t clientBufferAddress;
    uintptr_t targetProcessAddress;
    size_t size;
    int requestedDelayMilliseconds;
    int workerCount;
    int workerIndex;
    wchar_t targetModuleName[kModuleNameCapacity];
    volatile bool isKernelComponentAttached;
};

static_assert(sizeof(uintptr_t) == 8, "This prototype supports x64 builds only.");
static_assert(sizeof(CommunicationCommand) == sizeof(int),
    "CommunicationCommand must retain its 32-bit representation.");
static_assert(sizeof(CommunicationRequest) == 120,
    "CommunicationRequest layout changed; both protocol peers must use the same x64 ABI.");
