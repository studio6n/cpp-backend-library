# impetus

A lightweight C++20 backend framework for Linux, providing threading,
event-driven network I/O, and logging.

> Status: under active development. API is not yet stable — expect breaking
> changes between commits. `network/` is IPv4 only.

## Requirements

- Linux (no other platform is supported)
- g++ with C++20 support
- GNU Make

Optional, for development:
- `clang-format` (18+) — code formatting
- `clang-tidy` (18+) — static analysis
- `cppcheck` — static analysis

## Building

`impetus` builds as a static library (`libimpetus.a`).

- `make` (no target) — prints usage. Builds nothing.
- `make <variant>` — builds that variant into
  `build/<variant>/lib/libimpetus.a`, and ensures the shared
  `build/include/` exists. What each variant is for:

  | Variant | Purpose | Flags |
  |---|---|---|
  | `dev-memory` | Fast iteration, memory bugs | ASan + UBSan, `-O0`, `-g` |
  | `dev-thread` | Fast iteration, race bugs | TSan + UBSan, `-O0`, `-g` |
  | `alpha-memory` | Pre-release check, memory bugs | ASan + UBSan, `-O2` |
  | `alpha-thread` | Pre-release check, race bugs | TSan + UBSan, `-O2` |
  | `prod` | What actually ships | no sanitizers, `-O2` |

- `make all` — runs all five variant targets above, one after another.
- `make clean` — removes the entire `build/` directory (every variant,
  plus the shared `build/include/`).
- `make install` — not supported. Prints a message saying so.
- `make test` — **not ready yet.** The target exists in the Makefile and is
  meant to build all five variants and run a sandbox program against each,
  but the sandbox source it depends on (`src/sandbox/main.cpp`) hasn't been
  written yet, so `make test` currently fails.
- `make format` — reformats every `.cpp`/`.hpp` under `src/` in place with
  `clang-format`, using the `.clang-format` config at the project root.
- `make lint` — runs `clang-tidy` and `cppcheck` against everything under
  `src/`. Both are fatal: any finding from either tool fails the target.

There is no incremental build support for the compiled library itself —
each variant target always cleans that variant's own `build/<variant>/`
directory first, then rebuilds every `.cpp` file from scratch. Variants
are fully isolated from each other and all coexist under `build/` —
building one never touches or overwrites another's `lib/`. There is no
`make -j` support — builds are serial.

`build/include/` is the one exception: it's shared and identical across
all five variants (same source headers, no variant-specific content), is
not part of any single variant's subtree, and building or cleaning one
variant never touches it. It only rebuilds when a header under `src/` has
actually changed since the last build. Running `make all` builds all five
variants in one invocation, and since they all depend on this same
`build/include/` target, it is built at most once during that run, not
once per variant. `build/<variant>/lib/` + `build/include/` together are
the complete, self-contained pair an external project links against.

## Using impetus in your own project

### Retrieval

Run `make <variant>` for whichever variant you need (see Building above —
same command, nothing new). That produces `build/include/` and
`build/<variant>/lib/libimpetus.a`. Take both and place them wherever your
own project wants them — copy the two paths, nothing else is required.
`include/` is identical no matter which variant produced it; only the
`.a` differs between variants.

### APIs

How to use each part of the library.

#### Logging

```cpp
#include "log/log_consumer.hpp"
#include "log/logger.hpp"

class AppProducer : public impetus::log::ILogProducer {};

using namespace impetus::log;

if (!LogHub::instance().add_producer("app", std::make_unique<AppProducer>())) {
    // producer name already registered
}
if (!LogHub::instance().add_consumer("app", std::make_unique<LogConsumer>("app.log"))) {
    // consumer name already registered
}

LOG("app", LogLevel::INFO, "startup", "Server starting up");
LOG("app", LogLevel::WARN, "net", "Connection retry attempt 3");
```

#### ThreadPool

```cpp
#include "threads/thread_pool.hpp"

using namespace impetus::threads;

ThreadConfiguration config;
config.name = "worker";

if (!ThreadPool::instance().init(config, 4)) {
    // already initialised
}

if (!ThreadPool::instance().submit([] {
        // work runs on one of the pool's 4 threads
    })) {
    // queue failed
}

ThreadPool::instance().stop();
```

#### WorkerThread

```cpp
#include "threads/worker_thread.hpp"

using namespace impetus::threads;

ThreadConfiguration config;
config.name = "reactor";

WorkerThread worker(config);
if (!worker.start([] {
        // runs repeatedly until worker.stop() is called
    })) {
    // failed to start
}

worker.stop();
```

#### EventPollReactor / EventPollRegistry

```cpp
#include "poll/event_poll_registry.hpp"
#include "poll/event_poll_subscriber.hpp"

using namespace impetus::poll;

class MySocket : public EventPollSubscriber {
    public:
    explicit MySocket(int fd) noexcept : EventPollSubscriber(fd) {}

    void on_read() noexcept override {
        // fd is readable
    }

    void on_error() noexcept override {
        // fd hit an error or hangup
    }

    void on_detached() noexcept override {
        // the reactor has removed the fd and closed it
    }
};

if (!EventPollRegistry::instance().init()) {
    // already initialised
}

// The reactor keeps only a weak_ptr, so the subscriber must be shared-owned.
auto socket = std::make_shared<MySocket>(fd);

if (!EventPollRegistry::instance().attach("main", EventPollFlag::IN, socket)) {
    // reactor not found, or attach failed — the fd is still yours to close
}

EventPollRegistry::instance().run(); // call in a loop — processes one round of events per call

// Once attach() succeeded, never close the fd yourself: detach hands it back
// and the reactor closes it on its next round. Detach before destroying.
EventPollRegistry::instance().detach("main", *socket);
```

#### SocketBuffer

```cpp
#include "buffer/socket_buffer.hpp"

using namespace impetus::buffer;

SocketBuffer buffer(SocketBuffer::Memory::MEDIUM, SocketBuffer::Capacity::MEDIUM);

const char* data = "hello";
if (buffer.append(data, 5) == BufferCode::OK) {
    // buffer.data(), buffer.size() now reflect the appended bytes
}
```

## Project structure

| Module | Namespace | Responsibility |
|---|---|---|
| `src/shared/` | `impetus::shared` | Cross-cutting utilities, constants, kernel helpers, time helper |
| `src/dispatch/` | `impetus::dispatch` | Generic `IProducer<T>` / `IConsumer<T>` fan-out pattern |
| `src/buffer/` | `impetus::buffer` | Fixed-size byte buffer for I/O staging |
| `src/log/` | `impetus::log` | `LOG` macro, `LogHub`, `ILogProducer`, `ILogConsumer`, `LogConsumer` |
| `src/threads/` | `impetus::threads` | `IThreadRunnable`, `WorkerThread`, `ThreadPool` |
| `src/poll/` | `impetus::poll` | `EventPollSubscriber`, `EventPollReactor`, `EventPollRegistry` |
| `src/network/` | `impetus::network` | `TcpSocket`, `UdpSocket`, `TcpChannelListener`, `ITcpConnectorListener`, `IUdpListener` |

Full dependency rules between modules are in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Conventions

This project has explicit, mandatory conventions — read before contributing:

- [docs/NAMING-CONVENTION.md](docs/NAMING-CONVENTION.md) — file, class,
  variable, macro, and namespace naming rules.
- [docs/CODING-STYLE.md](docs/CODING-STYLE.md) — header organisation, class
  structure, type safety, memory ownership, modern C++ usage.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — module
  structure and dependency rules.
- `.clang-format` at the project root enforces formatting mechanically —
  run `make format` before committing.
