#include "SystemModules.h"

#include "UndocumentedStructures.h"

void* FindSystemModuleExport(const wchar_t* targetModuleName, const char* exportName)
{
    if (targetModuleName == nullptr || exportName == nullptr) {
        return nullptr;
    }

    void* moduleBase = FindSystemModuleBase(targetModuleName);
    return moduleBase == nullptr
        ? nullptr
        : RtlFindExportedRoutineByName(moduleBase, exportName);
}

void* FindSystemModuleBase(const wchar_t* targetModuleName)
{
    if (targetModuleName == nullptr || targetModuleName[0] == L'\0') {
        return nullptr;
    }

    void* moduleBase = nullptr;
    __try {
        UNICODE_STRING listName{};
        RtlInitUnicodeString(&listName, L"PsLoadedModuleList");
        const auto moduleList = static_cast<PLIST_ENTRY>(
            MmGetSystemRoutineAddress(&listName));
        if (moduleList == nullptr) {
            return nullptr;
        }

        UNICODE_STRING requestedName{};
        RtlInitUnicodeString(&requestedName, targetModuleName);
        for (PLIST_ENTRY link = moduleList->Flink;
             link != moduleList;
             link = link->Flink) {
            const auto entry = CONTAINING_RECORD(
                link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            if (RtlEqualUnicodeString(&entry->BaseDllName, &requestedName, TRUE)) {
                moduleBase = entry->DllBase;
                break;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        moduleBase = nullptr;
    }

    return moduleBase;
}
