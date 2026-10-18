# Plan: document the socket test cases

## Goal
Record the eight runtime cases run against 681d4eb plus the popped stash in
`docs/socket-test-case.md`, so anyone can re-run or extend them.

## Content
- Shared setup: one main reactor over loopback, sockets created with `make_shared`,
  "pump N" meaning N `run()` calls, `make dev-memory` flags (ASan + UBSan, `-Werror`);
  any sanitizer report fails a case.
- Per case: what it checks, steps, pass condition, and the regression it guards against.
- Last run: 8/8 on 681d4eb plus the local `connect()` fix; 7/8 on 681d4eb alone.
- Where the cases live: not in the repository; how to run them through the sandbox.

## Out of scope
- Adding the test program to the repository (`src/sandbox/main.cpp`): not requested.
- Linking the new doc from README.

## Verification
- Every case in the doc matches the program that was run: setup, steps, pass condition.
