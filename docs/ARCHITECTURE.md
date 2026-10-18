# Architecture Design

## Section 1 — Module Structure & Dependencies

### Modules

Seven modules live under `src/`, each mapping to one `impetus::` sub-namespace:

| Folder | Namespace | Responsibility |
|--------|-----------|----------------|
| `src/shared/` | `impetus::shared` | Cross-cutting utilities, constants, kernel helpers, time helper |
| `src/dispatch/` | `impetus::dispatch` | Generic `IProducer<T>` / `IConsumer<T>` fan-out pattern |
| `src/buffer/` | `impetus::buffer` | Fixed-size byte buffer for I/O staging |
| `src/log/` | `impetus::log` | `LOG` macro, `LogHub`, `ILogProducer`, `ILogConsumer`, `LogConsumer` |
| `src/threads/` | `impetus::threads` | `IThreadRunnable`, `WorkerThread`, `ThreadPool` |
| `src/poll/` | `impetus::poll` | `EventPollRegistry` (Meyers singleton — centralised owner of all reactors), `EventPollReactor`, `EventPollSubscriber` |
| `src/network/` | `impetus::network` | `BaseSocket`, `TcpSocket`, `UdpSocket`, `TcpChannelListener` (stateful — owns one connection), `ITcpConnectorListener`, `IUdpListener` |

### Dependency Rules

Arrows mean "depends on." No module may include headers from a module above it in this chain.

```
shared    ←─── dispatch, buffer, log, threads, poll, network
dispatch  ←─── log
buffer    ←─── network
log       ←─── buffer, threads, poll, network  (via logger.hpp only)
threads   ←─── poll, network
poll      ←─── network
network   ←─── user code
```

`dispatch/` has no dependencies on any other `impetus::` module — it is templates only,
reusable outside of logging. `log/` depends on both `shared/` (timestamp formatting)
and `dispatch/` (`ILogProducer`/`ILogConsumer` are thin wrappers over `IProducer<LogEntry>`/
`IConsumer<LogEntry>`).

Lower-level modules are **never** aware of higher-level ones. A violation of this rule is a hard architectural error.

### Sandbox

`src/sandbox/` holds `main.cpp`, an executable for manual testing and diagnostics. It is
**not** a module: it has no `impetus::` namespace and is excluded from `libimpetus.a`.

It sits where user code sits in the chain above — it may depend on any module, and no module
may depend on it. It is built only on request, with `make <variant> BINARY=1`, linked against
that variant's library so it always carries the same sanitizer flags as the code under test.
