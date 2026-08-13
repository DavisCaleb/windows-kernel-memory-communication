#pragma once

#include <ntifs.h>

// Historical inline interception retained for archival study. This mechanism is
// unsupported by Windows and must not be used in production.
namespace InlineInterception
{
NTSTATUS Install(void* targetFunction, void* replacementFunction);
NTSTATUS Restore(void* targetFunction);
} // namespace InlineInterception
