# Architecture

## Scope

MemoryDriverAndComm is an archival Windows x64 prototype centered on cross-process memory reading and writing over a multi-worker user/kernel protocol. This document describes the source layout, request streams, trust boundaries, known risks, and a supported redesign direction. It is not an operational deployment guide.

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
- creates up to ten indexed request streams;
- starts one detached transport worker for each configured stream;
- exposes raw-buffer and typed cross-process read/write helpers;
- selects a stream through each operation's `threadIndex` argument;
- validates local arguments, active worker indices, and calling-thread ownership;
- polls the selected stream for completion with a finite timeout.

Worker calls are detached because the callback remains active while processing requests. The client object must remain alive until communication has stopped. A production design should avoid this lifetime constraint.

### Worker and stream model

Each active stream owns a separate `CommunicationRequest`, worker index, command field, address fields, and kernel-side `RequestProcessor`. This isolates protocol state and demonstrates a multithreaded transport capable of maintaining multiple request channels.

The archival user-mode wrapper is deliberately more restrictive than the protocol layout: `ValidateThread` requires operations to originate from the thread that called `StartCommunication`, and each operation blocks until its selected request returns to `Idle`. Consequently, the source demonstrates **multiple kernel-side worker streams**, but it does not claim a thread-safe API for simultaneous calls from arbitrary client threads.

### Kernel driver project

- `DriverEntry.cpp` establishes the historical transport and validates initial request metadata.
- `RequestProcessor.cpp` dispatches commands and monitors the client heartbeat.
- `ProcessMemory.cpp` owns referenced `PEPROCESS` objects and implements copy/query helpers.
- `SystemModules.cpp` resolves internal module exports.
- `InlineInterception.cpp` preserves the original unsupported interception mechanism for archival study.
- `UndocumentedStructures.h` isolates unstable internal structure declarations.

## Request Flow

1. The client initializes one or more zeroed `CommunicationRequest` stream slots.
2. One transport worker per slot invokes the historical entry call and transfers that slot pointer to the kernel callback.
3. The callback checks the handshake, total worker count, and unique worker index, then starts a request processor for that stream.
4. The client chooses a stream with `threadIndex`, populates its request fields, and changes `command` from `CommunicationCommand::Idle`.
5. That stream's kernel processor performs the requested read, validated read, write, delay, heartbeat, or query operation and restores the idle state.
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
| Owner-thread client restriction | Worker streams cannot be submitted concurrently from arbitrary client threads |
| Per-stream synchronous polling | Parallel transport structure is not fully exposed as an asynchronous API |
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
