#pragma once

#include "../Shared/CommunicationProtocol.h"

#include <array>
#include <chrono>
#include <type_traits>

int FindProcessIdByName(const wchar_t* processName);

// User-mode wrapper for cross-process memory reads and writes over a bounded,
// multi-worker protocol. StartCommunication creates up to ten indexed request
// streams, each backed by a detached transport worker and kernel-side processor.
//
// Threading contract: public operations must be called by the thread that called
// StartCommunication. The threadIndex argument selects an independent protocol
// stream, but each call waits synchronously for that stream to become idle. This
// is multi-worker transport support, not a concurrently callable client API.
//
// Instances must remain alive until StopCommunication completes because worker
// calls are detached.
class MemoryCommunicationClient
{
public:
    MemoryCommunicationClient() = default;

    MemoryCommunicationClient(const MemoryCommunicationClient&) = delete;
    MemoryCommunicationClient& operator=(const MemoryCommunicationClient&) = delete;

    // Starts one transport worker and request stream per requested worker.
    // Valid range: [1, kMaximumWorkerCount].
    void StartCommunication(int workerCount = 1);
    void InitializeTarget(int targetProcessId);

    // Bidirectional raw-buffer operations. threadIndex selects the request
    // stream used for this synchronous operation.
    void Write(uintptr_t targetProcessAddress, uintptr_t clientBufferAddress, size_t size,
        int threadIndex = 0);
    void Read(uintptr_t clientBufferAddress, uintptr_t targetProcessAddress, size_t size,
        int threadIndex = 0);
    void ReadValidated(uintptr_t clientBufferAddress, uintptr_t targetProcessAddress, size_t size,
        int threadIndex = 0);

    template <typename T>
    void Write(uintptr_t targetProcessAddress, const T& value, int threadIndex = 0)
    {
        static_assert(std::is_trivially_copyable_v<T>,
            "Protocol values must be trivially copyable.");
        Write(targetProcessAddress, reinterpret_cast<uintptr_t>(&value), sizeof(T), threadIndex);
    }

    template <typename T>
    T Read(uintptr_t targetProcessAddress, int threadIndex = 0)
    {
        static_assert(std::is_trivially_copyable_v<T>,
            "Protocol values must be trivially copyable.");
        T value{};
        Read(reinterpret_cast<uintptr_t>(&value), targetProcessAddress, sizeof(T), threadIndex);
        return value;
    }

    template <typename T>
    T ReadValidated(uintptr_t targetProcessAddress, int threadIndex = 0)
    {
        static_assert(std::is_trivially_copyable_v<T>,
            "Protocol values must be trivially copyable.");
        T value{};
        ReadValidated(
            reinterpret_cast<uintptr_t>(&value), targetProcessAddress, sizeof(T), threadIndex);
        return value;
    }

    void StopCommunication();
    uintptr_t GetImageBaseAddress();
    uintptr_t GetTargetModuleBaseAddress(const wchar_t* targetModuleName);
    bool IsKernelComponentAttached() const;
    void RequestDelay(int milliseconds, int threadIndex = 0);
    void RefreshHeartbeat();

private:
    void ValidateThread(int threadIndex) const;
    void WaitForIdle(int threadIndex) const;

    static constexpr std::chrono::seconds kRequestTimeout{5};

    std::array<CommunicationRequest, kMaximumWorkerCount> requests_{};
    int workerCount_ = 0;
    unsigned long ownerThreadId_ = 0;
};
