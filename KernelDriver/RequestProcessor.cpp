#include "RequestProcessor.h"

namespace
{
constexpr ULONGLONG kClientTimeoutMilliseconds = 1000;
volatile LONG gStopRequested = FALSE;
} // namespace

RequestProcessor::RequestProcessor(CommunicationRequest* sharedRequest)
    : sharedRequest_(sharedRequest), lastHeartbeatTime_(0)
{
    if (sharedRequest_ != nullptr) {
        if (sharedRequest_->workerIndex == 0) {
            InterlockedExchange(&gStopRequested, FALSE);
            UpdateHeartbeat();
        }
        sharedRequest_->isKernelComponentAttached = true;
    }
}

void RequestProcessor::Run()
{
    if (sharedRequest_ == nullptr) {
        return;
    }

    sharedRequest_->command = CommunicationCommand::Idle;
    sharedRequest_->isKernelComponentAttached = true;
    unsigned int timeoutCheckCounter = 0;

    while (sharedRequest_->command != CommunicationCommand::Stop &&
        InterlockedCompareExchange(&gStopRequested, FALSE, FALSE) == FALSE) {
        switch (sharedRequest_->command) {
        case CommunicationCommand::Idle:
            break;
        case CommunicationCommand::InitializeProcesses:
            InitializeProcesses();
            break;
        case CommunicationCommand::SetTargetProcess:
            SetTarget();
            break;
        case CommunicationCommand::SetClientProcess:
            SetClient();
            break;
        case CommunicationCommand::WriteProcessMemory:
            Write();
            break;
        case CommunicationCommand::ReadProcessMemory:
            Read();
            break;
        case CommunicationCommand::ReadValidated:
            ValidatedRead();
            break;
        case CommunicationCommand::RequestDelay:
            AllowSleep();
            break;
        case CommunicationCommand::GetImageBase:
            GetImageBase();
            break;
        case CommunicationCommand::GetModuleBase:
            GetModuleBase();
            break;
        case CommunicationCommand::RefreshHeartbeat:
            UpdateHeartbeat();
            break;
        default:
            sharedRequest_->command = CommunicationCommand::Idle;
            break;
        }

        if (++timeoutCheckCounter >= 10000) {
            timeoutCheckCounter = 0;
            if (sharedRequest_->workerIndex == 0 && HasClientTimedOut()) {
                InterlockedExchange(&gStopRequested, TRUE);
                sharedRequest_->isKernelComponentAttached = false;
            }
        }
    }

    InterlockedExchange(&gStopRequested, TRUE);
    sharedRequest_->isKernelComponentAttached = false;
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::InitializeProcesses()
{
    NTSTATUS clientStatus = processMemory_.SetClientProcess(sharedRequest_->clientProcessId);
    NTSTATUS targetStatus = processMemory_.SetTargetProcess(sharedRequest_->targetProcessId);
#ifdef DEBUG
    if (!NT_SUCCESS(clientStatus) || !NT_SUCCESS(targetStatus)) {
        DbgPrint("MemoryDriver: process initialization failed (client=%08X, target=%08X)\n",
            clientStatus, targetStatus);
    }
#else
    UNREFERENCED_PARAMETER(clientStatus);
    UNREFERENCED_PARAMETER(targetStatus);
#endif
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::Read()
{
    NTSTATUS status = processMemory_.ReadProcessMemory(
        reinterpret_cast<void*>(sharedRequest_->targetProcessAddress),
        reinterpret_cast<void*>(sharedRequest_->clientBufferAddress), sharedRequest_->size);
#ifdef DEBUG
    if (!NT_SUCCESS(status)) {
        DbgPrint("MemoryDriver: read failed (%08X)\n", status);
    }
#else
    UNREFERENCED_PARAMETER(status);
#endif
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::Write()
{
    NTSTATUS status = processMemory_.WriteProcessMemory(
        reinterpret_cast<void*>(sharedRequest_->targetProcessAddress),
        reinterpret_cast<void*>(sharedRequest_->clientBufferAddress), sharedRequest_->size);
#ifdef DEBUG
    if (!NT_SUCCESS(status)) {
        DbgPrint("MemoryDriver: write failed (%08X)\n", status);
    }
#else
    UNREFERENCED_PARAMETER(status);
#endif
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::ValidatedRead()
{
    NTSTATUS status = processMemory_.ValidatedReadProcessMemory(
        reinterpret_cast<void*>(sharedRequest_->targetProcessAddress),
        reinterpret_cast<void*>(sharedRequest_->clientBufferAddress), sharedRequest_->size);
#ifdef DEBUG
    if (!NT_SUCCESS(status)) {
        DbgPrint("MemoryDriver: validated read failed (%08X)\n", status);
    }
#else
    UNREFERENCED_PARAMETER(status);
#endif
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::SetClient()
{
    processMemory_.SetClientProcess(sharedRequest_->clientProcessId);
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::SetTarget()
{
    processMemory_.SetTargetProcess(sharedRequest_->targetProcessId);
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::GetImageBase()
{
    sharedRequest_->targetProcessAddress =
        reinterpret_cast<uintptr_t>(processMemory_.GetTargetImageBase());
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::GetModuleBase()
{
    sharedRequest_->targetProcessAddress =
        reinterpret_cast<uintptr_t>(processMemory_.FindTargetModuleBase(sharedRequest_->targetModuleName));
    sharedRequest_->command = CommunicationCommand::Idle;
}

void RequestProcessor::AllowSleep()
{
    LARGE_INTEGER interval{};
    interval.QuadPart = static_cast<LONGLONG>(sharedRequest_->requestedDelayMilliseconds) * -10000LL;
    sharedRequest_->command = CommunicationCommand::Idle;
    KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

void RequestProcessor::UpdateHeartbeat()
{
    LARGE_INTEGER tickCount{};
    KeQueryTickCount(&tickCount);
    lastHeartbeatTime_ = static_cast<ULONGLONG>(tickCount.QuadPart);
    sharedRequest_->command = CommunicationCommand::Idle;
}

bool RequestProcessor::HasClientTimedOut() const
{
    LARGE_INTEGER tickCount{};
    KeQueryTickCount(&tickCount);

    LARGE_INTEGER tickIncrement{};
    tickIncrement.QuadPart = KeQueryTimeIncrement();
    const ULONGLONG elapsedMilliseconds =
        (static_cast<ULONGLONG>(tickCount.QuadPart) - lastHeartbeatTime_) *
        static_cast<ULONGLONG>(tickIncrement.QuadPart) / 10000ULL;
    return elapsedMilliseconds > kClientTimeoutMilliseconds;
}
