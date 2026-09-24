---
kind: error_handling
name: Integer-Return Error Propagation with SSH Disconnect Codes
category: error_handling
scope:
    - '**'
source_files:
    - src/session.h
    - src/session.c
    - src/net.h
    - src/ssh.h
    - src/main.c
    - src/packet.h
---

## Overview

The Coalesce SSH-2 transport implementation uses a minimal, idiomatic C error-handling strategy: functions return `int` status codes where `0` indicates success and any non-zero value (conventionally `-1`) signals failure. There are no custom error types, sentinel values, exception-like mechanisms, or `setjmp`/`longjmp` usage. Errors propagate upward through the call stack until they reach the top-level `main()` / `client()` / `server()` entry points, which log via `fprintf(stderr, ...)` and exit with a non-zero code.

## Return-Code Convention

Every public API in this codebase follows the same pattern:

- **Success**: return `0`.
- **Failure**: return `-1` (or another negative integer in rare cases).

This is visible across all layers:
- Network layer (`net.h`): `net_init()`, `net_set_nonblocking()`, `net_shutdown_write()` return `int`; `net_read_full()` / `net_write_full()` return `ssize_t` where `<= 0` means EOF/error; `net_read_line()` returns `ssize_t`.
- Session layer (`session.h`): `session_ident_check()`, `session_exchange_ident()`, `session_set_keys()`, `session_send()`, `session_send_buf()`, `session_recv()` all return `int`.
- Packet layer (`packet.h`): `pkt_send()`, `pkt_send_buf()`, `pkt_recv()` return `int`.
- Buffer layer (`buffer.h`): `buf_put_raw()` etc. return `int`.

There is no central error enum or struct — callers interpret the numeric result directly.

## Error Information: `net_get_error()`

The only structured error information mechanism is the network layer's `const char *net_get_error(void)` accessor declared in `src/net.h`. It returns a human-readable string describing the last OS/network error. Callers retrieve it immediately after a failing call, e.g.:

```c
if (!NET_IS_VALID(fd)) {
    fprintf(stderr, "[server] net_listen failed: %s\n", net_get_error());
    return -1;
}
```

This pattern is used consistently for socket creation, connection, and accept failures. Other layers do not expose equivalent error strings; they rely on the caller to infer the cause from context.

## Upper-Layer Handling in `main.c`

Top-level functions (`main`, `server`, `client`) are the single point of error presentation to the user:

- Invalid CLI arguments produce a `usage:` message via `fprintf(stderr, ...)` and `return 1`.
- Failed `net_init()` prints the `net_get_error()` message and exits with code `1`.
- Protocol failures (e.g. `session_exchange_ident != 0`, `session_recv == -1`) print a short diagnostic like `[client] identification exchange failed` and clean up resources before returning `-1`.
- Successful paths return `0`.

There is no centralized logging framework, no log levels, and no structured log output — just `printf` for normal progress and `fprintf(stderr, ...)` for errors.

## Protocol-Level Error Signaling

While internal function calls use integer return codes, the SSH protocol itself is also signaled via standardized disconnect messages defined in `src/ssh.h`. The header enumerates RFC 4250 §4.2.2 disconnect reason codes such as `SSH_DISCONNECT_PROTOCOL_ERROR`, `SSH_DISCONNECT_KEY_EXCHANGE_FAILED`, `SSH_DISCONNECT_MAC_ERROR`, `SSH_DISCONNECT_BY_APPLICATION`, etc., and channel open failure codes (`SSH_OPEN_*`). These constants exist for building `SSH_MSG_DISCONNECT` payloads when the peer should be notified of an SSH-layer failure. In the current demo code these constants are defined but not actively sent by the session layer — the transport simply returns `-1` to the caller, which then closes the socket. This leaves room for future enhancement where a MAC failure or protocol violation would first send a `SSH_MSG_DISCONNECT` with the appropriate reason code before closing.

## Resource Cleanup Pattern

Because there is no RAII or cleanup-on-error helper, each caller is responsible for freeing resources on every error path. For example, `session_exchange_ident` allocates temporary buffers internally and frees them before returning `-1`; the caller must still free `ssh_session_t` fields (`i_c`, `i_s`) via `session_free()` and close sockets via `net_close()`. The `main.c` server/client functions demonstrate this pattern: on any error branch they call `session_free(&sess)`, `net_close(...)`, and `net_shutdown()` before returning.

## No Panic/Recover or Exceptions

The codebase contains no `assert()`-based fatal checks, no `abort()`, no `exit()` inside library code (only the CLI may terminate), and no `setjmp`/`longjmp`. All failures are recoverable at the call site via the integer return value. This keeps the library usable in embedded or constrained environments where dynamic allocation failure is expected and must be handled explicitly.

## Constraints Observed

- Every public function that can fail returns `int`; callers must check the return value. Unchecked returns are present in the demo code (e.g. `session_send_buf` result is checked but its return value is not propagated), which is acceptable for a proof-of-concept but risky in production.
- Allocation failures (`malloc` returning `NULL`) are treated uniformly as `-1` errors; there is no separate `ENOMEM`-style code.
- Protocol violations (invalid packet length, bad padding, MAC mismatch) return `-1` without distinguishing the specific violation to the caller — the comment in `session_recv` notes that a MAC failure means "caller should disconnect".
- The SSH disconnect reason codes are defined centrally in `src/ssh.h` and serve as the canonical source of truth for protocol-level error semantics, even though they are not yet wired into the session I/O path.