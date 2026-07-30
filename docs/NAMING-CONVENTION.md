# Naming Conventions

## Files

| Type | Extension | Rule | Example |
|------|-----------|------|---------|
| Header | `.hpp` | `snake_case` of the primary class name | `worker_thread.hpp` |
| Source | `.cpp` | Matches the header name | `worker_thread.cpp` |
| Interface header | `.hpp` | `i_` prefix + `snake_case` of class name | `i_thread_runnable.hpp` |

Files are named after the primary class or entity they declare. No module prefix in the filename — the enclosing folder provides that context.

## Namespaces

`snake_case`. Must match the enclosing folder name.

```cpp
namespace impetus {
    namespace poll { }
    namespace network { }
    namespace logging { }
    namespace threads { }
    namespace buffer { }
    namespace shared { }
}
```

Namespace declarations use **nested braces** (not `namespace impetus::poll`).

## Classes & Structs

PascalCase.

| Kind | Rule | Example |
|------|------|---------|
| Concrete class | PascalCase | `WorkerThread`, `TcpSocket` |
| Interface (pure abstract) | `I` prefix + PascalCase | `IThreadRunnable`, `ILogProducer` |
| Struct (data bundle / POD) | PascalCase | `BufferConfig`, `Subscription` |

## Functions & Methods

`snake_case`.

```cpp
void start_reactor();
bool has_valid_fd() const noexcept;
```

## Variables

| Scope | Rule | Example |
|-------|------|---------|
| Local | `snake_case` | `byte_count` |
| Local static | `s_` prefix | `s_instance` |
| Member (public) | `snake_case` | `fd` |
| Member (protected / private) | `snake_case` + `_` suffix | `fd_`, `epoll_fd_` |
| Member static (public) | `s_` prefix | `s_default_capacity` |
| Member static (protected / private) | `s_` prefix + `_` suffix | `s_instance_` |
| `const` constant | `UPPER_SNAKE_CASE` | `MAX_EVENTS` |
| `constexpr` (non-string) | `K_` prefix + `UPPER_SNAKE_CASE` | `K_INVALID_FD`, `K_MAX_SIZE` |
| `constexpr` (string) | `UPPER_SNAKE_CASE` | `REACTOR_NAME_PREFIX` |
| `static constexpr` (non-string) | Same as non-string `constexpr` | `K_BUFFER_NPOS` |
| `static constexpr` (string) | `UPPER_SNAKE_CASE` | `DEFAULT_LOG_DIR` |

## Enums

Prefer `enum class` over plain `enum`.

| Kind | Rule | Example |
|------|------|---------|
| Enum type | PascalCase | `ReactorState`, `BufferCode` |
| Enum values | `UPPER_SNAKE_CASE` | `NOT_STARTED`, `ERR_FULL` |

## Templates

Single uppercase letter or `T`-prefixed PascalCase for type parameters.

```cpp
template <typename T>
template <typename TValue>
```

## Macros

`UPPER_SNAKE_CASE`. Logging macros in `log.hpp` use a `LOG_` prefix. All other library macros use an `IMPETUS_` prefix to avoid collisions.

```cpp
// Logging (public API)
LOG_INFO(...)
LOG_WARN(...)
LOG_ERROR(...)
LOG_DEBUG(...)

// Library internals
IMPETUS_MAX_EVENTS
IMPETUS_TIMEOUT_MS
```
