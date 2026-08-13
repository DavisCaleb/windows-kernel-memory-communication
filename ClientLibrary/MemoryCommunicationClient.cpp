#include "MemoryCommunicationClient.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
constexpr int kProtocolHandshake = 0x4D44434D; // "MDCM"

using DriverEntryPoint = void(__stdcall*)(int, CommunicationRequest*);
} // namespace

int FindProcessIdByName(const wchar_t* processName)
{
    if (processName == nullptr || processName[0] == L'\0') {
        return 0;
    }

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    int processId = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                processId = static_cast<int>(entry.th32ProcessID);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processId;
}

void MemoryCommunicationClient::StartCommunication(int workerCount)
{
    if (workerCount <= 0 ||
        workerCount > static_cast<int>(kMaximumWorkerCount)) {
        throw std::invalid_argument("workerCount is outside the protocol limit");
    }
    if (workerCount_ != 0) {
        throw std::logic_error("communication has already been started");
    }

    ownerThreadId_ = GetCurrentThreadId();

    if (LoadLibraryW(L"user32.dll") == nullptr) {
        throw std::runtime_error("failed to load user32.dll");
    }

    const HMODULE win32u = GetModuleHandleW(L"win32u.dll");
    const FARPROC entryAddress =
        win32u == nullptr ? nullptr : GetProcAddress(win32u, "NtFlipObjectCreate");
    if (entryAddress == nullptr) {
        throw std::runtime_error("required prototype entry point is unavailable");
    }

    const auto entryPoint = reinterpret_cast<DriverEntryPoint>(entryAddress);
    workerCount_ = workerCount;

    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount_));

    for (int index = 0; index < workerCount_; ++index) {
        CommunicationRequest& request = requests_[static_cast<size_t>(index)];
        request.command = CommunicationCommand::Idle;
        request.workerCount = workerCount_;
        request.workerIndex = index;
        request.isKernelComponentAttached = false;
        workers.emplace_back(entryPoint, kProtocolHandshake, &request);
    }

    // This historical protocol has no connection-completion primitive.
    Sleep(100);
    for (std::thread& worker : workers) {
        worker.detach();
    }
}

void MemoryCommunicationClient::InitializeTarget(int targetProcessId)
{
    if (targetProcessId <= 0) {
        throw std::invalid_argument("targetProcessId must be positive");
    }

    for (int index = 0; index < workerCount_; ++index) {
        ValidateThread(index);
        CommunicationRequest& request = requests_[static_cast<size_t>(index)];
        request.targetProcessId = targetProcessId;
        request.clientProcessId = static_cast<int>(GetCurrentProcessId());
        request.command = CommunicationCommand::InitializeProcesses;
        WaitForIdle(index);
    }
}

void MemoryCommunicationClient::Write(uintptr_t targetProcessAddress, uintptr_t clientBufferAddress, size_t size,
    int threadIndex)
{
    ValidateThread(threadIndex);
    if (targetProcessAddress == 0 || clientBufferAddress == 0 || size == 0) {
        throw std::invalid_argument("write arguments must be nonzero");
    }

    CommunicationRequest& request = requests_[static_cast<size_t>(threadIndex)];
    request.clientBufferAddress = clientBufferAddress;
    request.targetProcessAddress = targetProcessAddress;
    request.size = size;
    request.command = CommunicationCommand::WriteProcessMemory;
    WaitForIdle(threadIndex);
}

void MemoryCommunicationClient::Read(uintptr_t clientBufferAddress, uintptr_t targetProcessAddress, size_t size,
    int threadIndex)
{
    ValidateThread(threadIndex);
    if (targetProcessAddress == 0 || clientBufferAddress == 0 || size == 0) {
        throw std::invalid_argument("read arguments must be nonzero");
    }

    CommunicationRequest& request = requests_[static_cast<size_t>(threadIndex)];
    request.clientBufferAddress = clientBufferAddress;
    request.targetProcessAddress = targetProcessAddress;
    request.size = size;
    request.command = CommunicationCommand::ReadProcessMemory;
    WaitForIdle(threadIndex);
}

void MemoryCommunicationClient::ReadValidated(uintptr_t clientBufferAddress, uintptr_t targetProcessAddress, size_t size,
    int threadIndex)
{
    ValidateThread(threadIndex);
    if (targetProcessAddress == 0 || clientBufferAddress == 0 || size == 0) {
        throw std::invalid_argument("read arguments must be nonzero");
    }

    CommunicationRequest& request = requests_[static_cast<size_t>(threadIndex)];
    request.clientBufferAddress = clientBufferAddress;
    request.targetProcessAddress = targetProcessAddress;
    request.size = size;
    request.command = CommunicationCommand::ReadValidated;
    WaitForIdle(threadIndex);
}

void MemoryCommunicationClient::StopCommunication()
{
    if (workerCount_ == 0 || !requests_[0].isKernelComponentAttached) {
        return;
    }

    for (int index = 0; index < workerCount_; ++index) {
        CommunicationRequest& request = requests_[static_cast<size_t>(index)];
        request.isKernelComponentAttached = false;
        request.command = CommunicationCommand::Stop;
    }
    WaitForIdle(0);
}

uintptr_t MemoryCommunicationClient::GetImageBaseAddress()
{
    ValidateThread(0);
    requests_[0].command = CommunicationCommand::GetImageBase;
    WaitForIdle(0);
    return requests_[0].targetProcessAddress;
}

uintptr_t MemoryCommunicationClient::GetTargetModuleBaseAddress(const wchar_t* targetModuleName)
{
    ValidateThread(0);
    if (targetModuleName == nullptr || targetModuleName[0] == L'\0') {
        throw std::invalid_argument("targetModuleName must not be empty");
    }

    const errno_t copyResult =
        wcsncpy_s(requests_[0].targetModuleName, targetModuleName, _TRUNCATE);
    if (copyResult != 0) {
        throw std::invalid_argument("targetModuleName exceeds the protocol capacity");
    }
    requests_[0].command = CommunicationCommand::GetModuleBase;
    WaitForIdle(0);
    return requests_[0].targetProcessAddress;
}

bool MemoryCommunicationClient::IsKernelComponentAttached() const
{
    return workerCount_ > 0 && requests_[0].isKernelComponentAttached;
}

void MemoryCommunicationClient::RequestDelay(int milliseconds, int threadIndex)
{
    ValidateThread(threadIndex);
    if (milliseconds < 0) {
        throw std::invalid_argument("milliseconds must not be negative");
    }

    CommunicationRequest& request = requests_[static_cast<size_t>(threadIndex)];
    request.requestedDelayMilliseconds = milliseconds;
    request.command = CommunicationCommand::RequestDelay;
    WaitForIdle(threadIndex);
}

void MemoryCommunicationClient::RefreshHeartbeat()
{
    ValidateThread(0);
    requests_[0].command = CommunicationCommand::RefreshHeartbeat;
    WaitForIdle(0);
}

void MemoryCommunicationClient::ValidateThread(int threadIndex) const
{
    if (workerCount_ == 0) {
        throw std::logic_error("communication has not been started");
    }
    if (GetCurrentThreadId() != ownerThreadId_) {
        throw std::logic_error("operation called from a non-owner thread");
    }
    if (threadIndex < 0 || threadIndex >= workerCount_) {
        throw std::out_of_range("threadIndex is outside the active range");
    }
}

void MemoryCommunicationClient::WaitForIdle(int threadIndex) const
{
    const auto deadline = std::chrono::steady_clock::now() + kRequestTimeout;
    while (requests_[static_cast<size_t>(threadIndex)].command != CommunicationCommand::Idle) {
        if (std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("the kernel request timed out");
        }
        std::this_thread::yield();
    }
}
