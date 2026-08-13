#include <ntddk.h>
#include <ntifs.h>

#include "RequestProcessor.h"
#include "SystemModules.h"
#include "InlineInterception.h"

namespace
{
constexpr int kProtocolHandshake = 0x4D44434D; // "MDCM"
constexpr wchar_t kInterceptedModule[] = L"dxgkrnl.sys";
constexpr char kInterceptedExport[] = "NtFlipObjectCreate";

void CommunicationEntry(int key, CommunicationRequest* sharedRequest)
{
    if (key != kProtocolHandshake || sharedRequest == nullptr ||
        sharedRequest->workerCount <= 0 ||
        sharedRequest->workerCount > static_cast<int>(kMaximumWorkerCount) ||
        sharedRequest->workerIndex < 0 ||
        sharedRequest->workerIndex >= sharedRequest->workerCount) {
        return;
    }

    if (sharedRequest->workerIndex + 1 == sharedRequest->workerCount) {
        void* targetFunction =
            FindSystemModuleExport(kInterceptedModule, kInterceptedExport);
        InlineInterception::Restore(targetFunction);
    }

    RequestProcessor requestProcessor(sharedRequest);
    requestProcessor.Run();
}
} // namespace

extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath)
{
    UNREFERENCED_PARAMETER(driverObject);
    UNREFERENCED_PARAMETER(registryPath);

    // This archival transport modifies an undocumented kernel entry point. A
    // supported redesign should expose a secured device and use IOCTL requests.
    void* targetFunction =
        FindSystemModuleExport(kInterceptedModule, kInterceptedExport);
    if (targetFunction == nullptr) {
        return STATUS_PROCEDURE_NOT_FOUND;
    }

    return InlineInterception::Install(targetFunction, reinterpret_cast<void*>(&CommunicationEntry));
}
