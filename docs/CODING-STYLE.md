# Coding Style

## Header Organisation

### Header Guards

Use `#pragma once` in every header file. Do not use `#ifndef` include guards.

```cpp
#pragma once
```

### Include Order

Include order is enforced by clang-format. See [`.clang-format`](../.clang-format) for the configuration.

### Forward Declarations

Prefer forward declarations over includes in header files when only a pointer or reference to a type is needed. Only include the full header when the definition is required (e.g. inheritance, member by value, calling methods).

```cpp
// Prefer this in a header when only a reference is used
class EventPollSubscriber;

// Only include the full header when the definition is needed
#include "event_poll_subscriber.hpp"
```

## Class Structure

### Section Ordering

Declare sections in this order: `public` → `protected` → `private`. Within each access level, functions and data members each get their own repeated access specifier — functions first, then data members. This keeps them visually separated for readability.

```cpp
class MyClass {
    public:
    MyClass();
    ~MyClass();

    void do_something();

    private:
    void helper_();

    private:
    int internal_;
};
```

### Rule of Zero

If all data members are RAII types (`std::unique_ptr`, `std::vector`, etc.), define none of the five special member functions. The compiler-generated ones are correct.

```cpp
class MyClass {
    private:
    std::unique_ptr<char[]> data_;  // compiler handles everything
};
```

### Rule of Five

If a class directly **owns** a raw resource (file descriptor, raw pointer, OS handle), explicitly define or `= delete` all five special member functions. The compiler cannot manage a raw resource correctly on your behalf.

```cpp
class MyClass {
    public:
    MyClass(const MyClass&) = delete;
    MyClass& operator=(const MyClass&) = delete;
    MyClass(MyClass&&) = delete;
    MyClass& operator=(MyClass&&) = delete;
    ~MyClass();  // releases the raw resource

    private:
    int fd_ = -1;  // raw resource — owns it
};
```

A class that merely *references* a resource it does not own falls under Rule of Zero.

### Member Access

Private variables are the default. Do not declare public data members unless there is no alternative.

Every private variable that needs to be read externally gets a `const` getter. Mutation goes through a dedicated method — never by exposing the member directly.

```cpp
private:
int fd_ = -1;

public:
[[nodiscard]]
int fd() const noexcept { return fd_; }

void set_fd(int fd) noexcept { fd_ = fd; }  // only if mutation is genuinely needed
```

### Explicit Constructors

All single-argument constructors must be `explicit` to prevent unintended implicit conversions.

```cpp
explicit EventPollReactor(std::string_view suffix);
```

### Override and Final

Always use `override` when overriding a virtual function. Use `final` to prevent further overriding when intended.

```cpp
void on_read() noexcept override;
void on_error() noexcept override final;
```

---

## Type Safety

### const Correctness

Apply `const` to every method that does not mutate state and every variable or parameter that is not modified after initialisation.

```cpp
[[nodiscard]]
bool has_valid_fd() const noexcept;

const int fd = subscriber->fd();
```

### noexcept

Mark functions `noexcept` when they cannot throw. Required on:
- All destructors
- All move constructors and move assignment operators
- Any function that genuinely cannot throw

```cpp
void on_error() noexcept override;
bool fd_close(int fd) noexcept;
```

### [[nodiscard]]

Apply `[[nodiscard]]` to any function whose return value must not be silently discarded — error codes, result types, resource handles, and boolean status returns.

```cpp
[[nodiscard]]
bool start();

[[nodiscard]]
BufferCode append(const char* src, std::size_t size);
```

### nullptr

Always use `nullptr`. Never use `NULL` or `0` to represent a null pointer.

### Integer Types

Prefer fixed-width integer types from `<cstdint>` over `int`, `long`, `unsigned`, etc. Use plain types only when interfacing with a C API that requires them (e.g. `epoll_wait`, `close`, `fcntl`) or when size genuinely does not matter.

| Prefer | Over |
|--------|------|
| `std::int32_t` | `int` |
| `std::uint32_t` | `unsigned int` |
| `std::uint8_t` | `unsigned char` |
| `std::size_t` | `unsigned long` |
| `std::ptrdiff_t` | `long` |

---

## Memory

- Prefer `std::unique_ptr` for single ownership. Use `std::shared_ptr` only when shared ownership is genuinely required.
- Never use raw `new` or `delete`. Resource allocation must go through RAII types.
- No raw owning pointers. A raw pointer in a class signals a non-owning reference — document it as such.

---

## Modern C++

### using over typedef

```cpp
// Correct
using Callback = std::function<void()>;

// Wrong
typedef std::function<void()> Callback;
```

### Brace Initialisation

Use `{}` for value initialisation to avoid narrowing conversions and the most-vexing-parse problem.

```cpp
epoll_event ev{};
Subscription sub{ interest, listener, subscriber };
```

### auto

Use `auto` only when the type is explicitly obvious at the point of declaration — iterator types, cast results, and lambdas. Do not use it as a general shorthand to avoid writing types.

```cpp
// Acceptable — iterator type is verbose and obvious from context
auto it = subscriptions_.find(fd);

// Avoid — type is not obvious at a glance
auto result = compute();
```

---

## Comments

### Documentation Comments

Use `/** */` block comments for all documentation (classes, methods, members). Even when the content is a single line, always use the expanded multi-line form. Never collapse it onto one line.

```cpp
// Wrong
/** Brief description. */

// Correct
/**
 * Brief description.
 */
```

For multi-line content:

```cpp
/**
 * Brief description.
 *
 * @param fd
 * The file descriptor to operate on.
 *
 * @return
 * True on success, false otherwise.
 */
bool fd_close(int fd) noexcept;
```

### Inline Comments

Use `//` for comments inside function implementations only.

```cpp
bool EventPollReactor::start() {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        // epoll_create1 failed — reactor cannot proceed
        reactor_state_.store(ReactorState::ERROR_EPOLL_FD);
        return false;
    }

    reactor_state_.store(ReactorState::RUNNING);
    return true;
}
```
