# Plan: remove IEventPollListener

## Why

`IEventPollListener` is redundant:

- The reactor never uses it for I/O; `dispatch_events()` only calls the subscriber.
- `on_attached()` / `on_detached()` on the listener only forward to the subscriber's own hooks.
- `on_detaching()` has no subscriber equivalent, but its only implementation (`BaseSocket`) is empty.
- The only implementer is `BaseSocket`, which is its own listener (`attach(..., this, this)` in all four
  call sites) purely to pass the reactor's null-listener check.
- That null check silently drops an attach after `attach()` has already returned `true`.

## Changes

1. Delete `src/poll/i_event_poll_listener.hpp`.
2. `src/poll/event_poll_reactor.hpp`
   - include `event_poll_subscriber.hpp` instead of `i_event_poll_listener.hpp`
   - `attach(EventPollFlag interest, EventPollSubscriber* subscriber)`; drop the listener doc
   - `Subscription` keeps `flag` and `subscriber` only
   - `flush_detach_requests()` doc no longer mentions `on_detaching()`
3. `src/poll/event_poll_reactor.cpp`
   - `attach()` drops the listener parameter
   - `flush_attach_requests()`: skip only on null subscriber; call `subscriber->on_attached()`
   - `flush_detach_requests()`: remove the `on_detaching()` call
   - `flush_detached_callbacks()`: call `subscriber->on_detached()`
4. `src/poll/event_poll_registry.hpp` / `.cpp`: drop the listener forward declaration, include, and
   parameter; forward to `reactor->attach(interest, subscriber)`.
5. `src/network/base_socket.hpp`: inherit `poll::EventPollSubscriber` only; remove the listener
   include, the `on_detaching()` override, and the doc paragraph about being its own listener.
6. `src/network/tcp_socket.cpp` (3 call sites), `src/network/udp_socket.cpp` (1 call site):
   `attach(reactor_name_, flag, this)`.
7. `README.md`: poll example without `MyListener`; project-structure row lists `EventPollSubscriber`.
8. `docs/ARCHITECTURE.md`: poll row drops `IEventPollListener`.

## Out of scope (later)

- `on_detached()` is not delivered while the reactor is stopping (`run()` returns before
  `flush_detached_callbacks()`). Unchanged by this plan.

## Verification

- Baseline before changes: all 14 `.cpp` and 31 `.hpp` compile with the Makefile's `WARNFLAGS` (`-Werror`).
- After changes: same compile, zero mentions of `IEventPollListener` / `on_detaching` left in the repo,
  `clang-format` run on every modified file.
- `clang-tidy` / `cppcheck` are not installed, so `make lint` cannot be run.
