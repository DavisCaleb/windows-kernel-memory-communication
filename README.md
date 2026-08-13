# MemoryDriverAndComm

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D4)
![Memory operations](https://img.shields.io/badge/memory-read%20%2F%20write-6A5ACD)
![Worker streams](https://img.shields.io/badge/worker%20streams-up%20to%2010-2E8B57)
![Status](https://img.shields.io/badge/status-archival%20prototype-orange)

An archival C++ prototype designed for **cross-process memory reading and writing** through a Windows x64 kernel component and user-mode client. Its protocol supports a **multi-worker architecture with multiple independent request streams**, alongside module discovery, heartbeat handling, process-object ownership, and a shared request ABI.

> **Status:** Educational source archive—not production software. The transport relies on undocumented Windows internals. No binaries, installer, or releases are provided.

## Project Summary

> **Historical Note:** This prototype was originally developed in 2021. The repository was later cleaned up and documented for archival portfolio presentation purposes.

The repository is organized into three focused components:

- **`KernelDriver/`** — WDM/WDK kernel project that services read, validated-read, write, and process/module query requests while managing referenced process objects.
- **`ClientLibrary/`** — C++17 static library exposing raw-buffer and typed `Read<T>`/`Write<T>` helpers over indexed communication streams.
- **`Shared/CommunicationProtocol.h`** — single ABI definition shared by both projects, including bounded worker and stream metadata.

### Core design goals

- **Bidirectional memory transfer:** copy bytes from a selected target process into the client process, or from the client into the selected target.
- **Typed client operations:** read and write trivially copyable C++ values without duplicating buffer setup code.
- **Multiple communication streams:** allocate up to ten request slots, each with its own worker index, kernel-side request processor, command state, and request data.
- **Multithreaded transport:** start one detached transport worker for each active request stream so the kernel side can maintain multiple independently processed channels.

> **Concurrency scope:** The prototype implements multiple worker streams, but the archival client wrapper intentionally requires public calls to originate from the thread that called `StartCommunication`. Operations also wait synchronously for their selected stream to return to idle. It therefore demonstrates a multi-worker protocol architecture—not a production-ready, concurrently callable client API.

The project is presented as an early systems-programming experiment and design case study, not as a secure or deployable driver.

## Technical Highlights

- Implemented cross-process reads and writes with `MmCopyVirtualMemory`, including a separate validated-read command.
- Added raw-buffer operations plus constrained `Read<T>`, `ReadValidated<T>`, and `Write<T>` helpers for trivially copyable values.
- Modeled up to **10 indexed communication streams** with dedicated request structures and kernel-side processors.
- Started one transport worker per configured stream and validated worker counts and indices at the protocol boundary.
- Separated kernel, user-mode, and shared-protocol responsibilities into clear projects.
- Used WDK primitives including `PEPROCESS`, `KAPC_STATE`, MDLs, `NTSTATUS`, and structured exception handling.
- Centralized the x64 request ABI to prevent duplicate protocol definitions from drifting.
- Added request timeouts, input checks, process-reference cleanup, heartbeat monitoring, and explicit failure handling.
- Documented trust boundaries and a migration path toward a supported IOCTL/KMDF architecture.

## Repository Layout

```text
.
├── ClientLibrary/                         # User-mode static library
│   ├── MemoryCommunicationClient.cpp
│   ├── MemoryCommunicationClient.h
│   └── MemoryCommunicationClient.vcxproj
├── KernelDriver/                          # WDK driver project
│   ├── DriverEntry.cpp
│   ├── RequestProcessor.cpp/.h
│   ├── ProcessMemory.cpp/.h
│   ├── SystemModules.cpp/.h
│   ├── InlineInterception.cpp/.h
│   ├── UndocumentedStructures.h
│   └── MemoryCommunicationDriver.vcxproj
├── Shared/
│   └── CommunicationProtocol.h
├── docs/ARCHITECTURE.md
└── MemoryDriverAndComm.slnx
```

## Requirements

- Windows x64 development environment
- Visual Studio 2022 with Desktop C++ tools
- Windows SDK and Windows Driver Kit (WDK)

Both projects target C++17. The client library uses the Visual Studio `v143` toolset; the kernel project uses `WindowsKernelModeDriver10.0`.

## Build Status

The solution/project XML and user-mode source were structurally checked on Linux. The user-mode translation unit also passes a MinGW x64 syntax check, with one expected warning for the dynamically resolved historical entry-point cast.

A Windows WDK build and runtime test were not available and are **not claimed**. Open `MemoryDriverAndComm.slnx` in a matching Visual Studio/WDK environment to perform the authoritative build.

No loading, signing, deployment, bypass, or runtime-use instructions are included. Treat this repository as source for code review and architecture discussion.

## Important Limitations

- Memory operations are privileged and must only target processes the operator owns or is explicitly authorized to inspect or modify.
- Multiple kernel-side worker streams exist, but the current client object is owner-thread constrained and waits synchronously for each selected stream.
- The transport modifies an undocumented kernel entry point and is incompatible with production-driver expectations.
- Shared polling fields use `volatile`; this is not a formally synchronized request mechanism.
- Request data crosses a user/kernel trust boundary without production-grade authentication, authorization, or probing.
- Internal loader structures and exports may differ across Windows releases.
- The prototype lacks a secured device object, normal dispatch routines, installation package, signing pipeline, and verified Windows tests.
- Kernel defects can cause crashes, corruption, or security vulnerabilities.

See [Architecture](docs/ARCHITECTURE.md) for a candid design review.

## Safer Redesign Roadmap

A supported successor should:

1. Replace interception and shared polling with a secured device and versioned IOCTL contract.
2. Enforce restrictive device access and an explicit authorization model.
3. Use a documented buffering model with exact schema, length, range, and access validation.
4. Replace detached polling workers with framework-managed requests, cancellation, and synchronization.
5. Prefer KMDF lifecycle management and documented Windows interfaces.
6. Add Windows CI for compilation and static analysis before any runtime test plan.

## Responsible Use

Use only for defensive education, code review, or research on systems you own and are explicitly authorized to test. Do not use it to access another process or system without permission, bypass security controls, interfere with software, or violate applicable law or terms of service.

Do not deploy this prototype to production or a daily-use computer. Review [SECURITY.md](SECURITY.md) before working with the source.

## Contributing

Documentation, portability notes, build corrections, and defensive code-quality improvements are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE). The license does not grant permission to misuse the software or override third-party rights and policies.
