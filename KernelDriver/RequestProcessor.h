#pragma once

#include "../Shared/CommunicationProtocol.h"
#include "ProcessMemory.h"

#include <ntddk.h>

class RequestProcessor
{
public:
    explicit RequestProcessor(CommunicationRequest* sharedRequest);

    RequestProcessor(const RequestProcessor&) = delete;
    RequestProcessor& operator=(const RequestProcessor&) = delete;

    void Run();

private:
    void InitializeProcesses();
    void Read();
    void Write();
    void ValidatedRead();
    void SetClient();
    void SetTarget();
    void GetImageBase();
    void GetModuleBase();
    void AllowSleep();
    void UpdateHeartbeat();
    bool HasClientTimedOut() const;

    CommunicationRequest* sharedRequest_ = nullptr;
    ProcessMemory processMemory_;
    ULONGLONG lastHeartbeatTime_ = 0;
};
