# Socket test cases

Runtime cases for `network/` and its use of `poll/`. Each one exercises a path that has broken
before, or that a header documents as safe. They are run by hand: the Makefile has no test target.

## Shared setup

- One process. `EventPollRegistry::instance().init()` is called once, and every socket uses the
  `"main"` reactor over loopback (`127.0.0.1`). Each case listens on its own port.
- Every `TcpSocket` and `UdpSocket` is created with `std::make_shared`. The reactor holds
  subscribers by `weak_ptr`, so a socket owned any other way cannot attach.
- **Pump N** means N calls to `EventPollRegistry::instance().run()`. Each call applies queued
  attach, modify and detach requests, then waits up to 100 ms in `epoll_wait`.
- Built the way `make dev-memory` builds: `WARNFLAGS` with `-Werror`, `-O0 -g`, ASan and UBSan.
  A case fails if its pass condition is false **or** a sanitizer reports anything.
- Each case closes what it opened and pumps twice before the next case starts.

## Cases

### 1. Close and reconnect in the same cycle

- **Checks:** a socket opened between a `close()` and the next `run()` still gets attached.
- **Steps:**
  1. Client A connects. Pump 20.
  2. Close A. Without pumping, create client B with a new `::socket()` and connect it.
  3. Pump 20.
- **Passes when:** A and B both receive `on_connected()`.
- **Guards against:** fd-number reuse. If A's fd were closed before the reactor removed A's entry,
  the kernel could give the same number to B. The reactor would then apply B's attach while A's
  entry still held that number, skip it, and remove A's entry, so B would never be watched. The
  reactor now closes a detached fd only after removing its entry.

### 2. Greeting written right after `adopt()`

- **Checks:** data written to a connection before the reactor has applied its attach is still sent.
- **Steps:**
  1. The server's `on_accepted()` calls `adopt()`, then immediately `write("HELLO")`.
  2. A client connects. Pump 20.
- **Passes when:** the client receives `HELLO`.
- **Guards against:** reordering the flush in `EventPollReactor::run()`. `adopt()` queues an
  attach with read interest, and `write()` queues a modify that adds write interest. Attaches are
  applied before modifies. If that order were reversed, the modify would find no entry and be
  dropped, and the data would stay in the write buffer forever.

### 3. Destroy closed channels after each `run()`

- **Checks:** a closed channel can be destroyed between `run()` calls.
- **Steps:**
  1. A client connects and calls `close()` from its own `on_connected()`.
  2. For 20 rounds: `run()`, then destroy every server-side channel whose `is_open()` is false.
- **Passes when:** the client connected, every server-side channel was destroyed, and no sanitizer
  reported anything.
- **Guards against:** the reactor calling into a socket that its owner has already destroyed. It
  used to call `on_detaching()` on a socket one `run()` after the socket closed, so destroying it
  in between was a use-after-free. The reactor now holds subscribers by `weak_ptr` and calls
  nothing once a subscriber is gone.

### 4. Destroy a channel inside its own `on_closed()`

- **Checks:** the guarantee in `tcp_channel_listener.hpp` that a closed channel may be destroyed
  from inside `on_closed()`.
- **Steps:**
  1. Each server-side channel removes itself from its owner, destroying itself, inside
     `on_closed()`.
  2. A client connects and closes as soon as it is connected. Pump 20.
- **Passes when:** the client connected, the server-side channel was destroyed, and no sanitizer
  reported anything.
- **Guards against:** the socket being freed while its own callback is still running. Dispatch
  holds a `shared_ptr` to the socket for the length of each callback.

### 5. Destroy an open channel without `close()`

- **Checks:** destroying a connected channel without calling `close()` still releases its fd
  correctly.
- **Steps:**
  1. Client A, owned by a `unique_ptr`, connects. Pump 20.
  2. Destroy A without calling `close()`.
  3. Client B connects. Pump 20.
  4. Close B. Pump 2.
- **Passes when:** A and B both connected, and no sanitizer reported anything.
- **Guards against:** a destructor closing the fd without telling the reactor. That left A's entry
  in place, so B's attach on the reused number was skipped, and B's later detach reached A's freed
  object. Now `~TcpChannelListener()` closes the connection and logs an error, and `~BaseSocket()`
  hands the fd to the reactor.

### 6. TCP echo, and the server's close reaches the client

- **Checks:** the TCP data path in both directions, and close propagation.
- **Steps:**
  1. The client sends `PING` from `on_connected()`, and the server echoes it. Pump 20.
  2. The server closes its side of the connection. Pump 5.
- **Passes when:** the client connected, received `PING`, and received `on_closed()`.

### 7. UDP echo

- **Checks:** UDP `bind()`, `connect()`, send and receive.
- **Steps:**
  1. A server socket is bound to `INADDR_ANY` and replies to each datagram's sender.
  2. A client socket is `connect()`ed to the server and writes `PONG`. Pump 20.
- **Passes when:** the server received `PONG`, and the client received the echoed `PONG`.

### 8. `on_connected()` after an immediate connect

- **Checks:** `on_connected()` fires when `::connect()` succeeds immediately.
- **Steps:**
  1. Create a `TcpSocket` with `std::make_shared`, and clear `O_NONBLOCK` on its fd so that
     `::connect()` returns 0 straight away.
  2. Call `connect()`, then restore the fd's flags. Pump 20.
- **Passes when:** `connect()` returns true, and the channel receives `on_connected()`.
- **Guards against:** promoting the socket to CONNECTED inside `connect()`. `on_connected()` is
  only called from `finish_connect()`, which only runs while the socket is CONNECTING, so the
  callback never fired. On Linux a non-blocking TCP connect almost always returns `EINPROGRESS`,
  so only a forced immediate success reaches this path.

## Last run

| Code | Result |
|---|---|
| 681d4eb plus the uncommitted fix to `TcpSocket::connect()`, 2026-10-18 | 8 of 8 pass |
| 681d4eb alone | 7 of 8 pass; case 8 fails |

## Running the cases

The test program is not in the repository. To run the cases, put them in `src/sandbox/main.cpp`
and build with `make dev-memory BINARY=1` (see README).
