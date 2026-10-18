/**
 * @brief
 * Sandbox entry point for manual testing and diagnostics.
 *
 * Built only on request — `make <variant> BINARY=1` — and linked against that
 * variant's libimpetus.a, so it always carries the same sanitizer flags as the
 * library under test. Never part of the library itself.
 *
 * TSan builds (dev-thread, alpha-thread) must be run with address
 * randomization disabled, or GCC 13's ThreadSanitizer fails to start:
 *
 *     setarch $(uname -m) -R ./build/dev-thread/bin/sandbox
 */
int main() {
    return 0;
}
