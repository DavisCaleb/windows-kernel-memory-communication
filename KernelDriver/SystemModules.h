#pragma once

#include <ntifs.h>

void* FindSystemModuleBase(const wchar_t* targetModuleName);
void* FindSystemModuleExport(const wchar_t* targetModuleName, const char* exportName);
