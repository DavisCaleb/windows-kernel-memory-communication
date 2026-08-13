# Architecture

## Scope

MemoryDriverAndComm is an archival Windows x64 prototype. This document describes the source layout, request flow, trust boundaries, known risks, and a supported redesign direction. It is not an operational deployment guide.

## Components

```text
User mode                                      Kernel mode
┌──────────────────────────────┐               ┌─────────────────────────────┐
│ MemoryCommunicationClient    │    request    │ RequestProcessor            │
│ - validates local arguments  ├──────────────►│ - dispatches commands       │
│ - owns request structures    │  shared ABI   │ - owns process references   │
│ - waits for completion       │               │ - performs query/copy work  │
└───────────────┬──────────────┘               └──────────────┬──────────────┘
                └──── Shared/CommunicationProtocol.h ─────────┘
```

### Shared protocol

`Shared/CommunicationProtocol.h` defines command values, worker limits, and the x64 `CommunicationRequest` layout. Both projects include this header so their ABI cannot silently diverge.

The protocol remains experimental. Its `volatile` fields document externally observed values but do not supply production-grade atomicity or memory ordering.

### User-mode client library

`ClientLibrary/MemoryCommunicationClient.cpp`:

- locates processes by exact executable name;
- creates a bounded set of request slots;
- invokes the historical prototype entry point;
- validates local arguments and worker indices;
- polls request completion with a finite timeout;
- provides typed helpers for trivially copyable values.

Worker calls are detached because the callback remains active while processing requests. The client object must remain alive until communication has stopped. A production design should avoid this lifetime constraint.

### Kernel driver project

- `DriverEntry.cpp` establishes the historical transport and validates initial request metadata.
- `RequestProcessor.cpp` dispatches commands and monitors the client heartbeat.
- `ProcessMemory.cpp` owns referenced `PEPROCESS` objects and implements copy/query helpers.
- `SystemModules.cpp` resolves internal module exports.
- `InlineInterception.cpp` preserves the original unsupported interception mechanism for archival study.
- `UndocumentedStructures.h` isolates unstable internal structure declarations.

## Request Flow

1. The client initializes one or more zeroed `CommunicationRequest` slots.
2. The historical entry call transfers a slot pointer to the kernel callback.
3. The callback checks the handshake and worker metadata, then starts a request processor.
4. The client populates request fields and changes `command` from `CommunicationCommand::Idle`.
5. The kernel processes the request and restores the idle state.
6. The client observes completion or reports a timeout.
7. A heartbeat timeout or stop command ends request processing.

## Trust Boundaries

### User-controlled request memory

The client controls the request command, process identifiers, addresses, lengths, timing, and object lifetime. Basic checks help with accidental misuse but do not provide safe kernel capture/probe semantics.

### Cross-process operations

A request identifies client and target processes with distinct lifetimes and security contexts. The code balances object references but has no production authorization policy.

### Undocumented kernel state

Transport setup and module discovery depend on internal exports and loader layouts. These are not stable Windows contracts.

## Principal Risks

| Risk | Consequence |
|---|---|
| Unsupported inline interception | Integrity failures, crashes, or version incompatibility |
| User-owned shared request | Races, stale pointers, and time-of-check/time-of-use defects |
| Polling protocol | CPU overhead and weak synchronization semantics |
| Missing authorization policy | Unintended privileged requests |
| Internal loader traversal | Invalid layout assumptions and unsafe pointer traversal |
| Detached client workers | Difficult cancellation and object-lifetime hazards |
| Unverified WDK build/runtime | Compiler, linker, and runtime defects may remain |

## Supported Redesign Direction

A professional successor should use documented Windows driver patterns:

1. Use KMDF—or complete WDM dispatch and unload handling—for explicit lifecycle management.
2. Create a secured device with a versioned IOCTL contract instead of modifying another module.
3. Select a documented buffering model and validate request versions, exact lengths, ranges, and access rights.
4. Restrict device access and enforce a narrow authorization policy for every operation.
5. Use framework queues, cancellation, events, or request completion rather than polling shared fields.
6. Return operation status explicitly and add structured diagnostics for checked builds.
7. Compile and analyze in Windows CI before considering isolated, authorized VM testing.

The portfolio value of this repository is as a documented case study: it demonstrates low-level API familiarity while identifying why supported interfaces, strict trust-boundary validation, and verifiable lifecycle management are essential.
