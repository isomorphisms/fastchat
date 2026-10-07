# H2 evidence — 2026-10-07

Base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b`, fetched from current `main`.
Independent branch: `sun/transport-http2`. No dependency on merging the semantic
reference. All runtime protocol evidence below is **host Linux**, not physical A1.

## Deterministic qualification

`host/suite.txt` is the recipe-driven acceptance result. `host/*.tsv` retain the
probe's real events/metrics and server's ALPN/session/wire-stream log. Test payloads
are identical across H2/H3: JSON POST `{"prompt":"fixture"}`, three delayed SSE
text records, or 400 records of 1000 `x` bytes, followed by `[DONE]`. The server
checks the exact method, content-type and body. Its data splits across line/record
boundaries; consumers cannot assume callback boundaries correspond to events.

| Required behavior | Result and evidence |
| --- | --- |
| TLS ALPN H2 | PASS; H2-only negotiation, certificate verification, negotiated libcurl version 3 (`HTTP_VERSION_2_0`), `fixture.tsv` sessions report `h2`. |
| Headers/body and streamed response | PASS; exact POST validated by server, three separate data events then provider completion. |
| Two requests/one connection | PASS; distinct server wire streams share one session; probe reports one new connection. |
| Independent cancellation/reset | PASS; request 1 cancelled after first data; request 2 completes. Forced peer reset yields uncertainty for request 1 while peer succeeds. |
| In-flight connection loss | PASS; `/close` destroys the H2 session after a partial record; both in-flight attempts become uncertain. |
| Explicit new retry identity | PASS; retry is requests 1/2 with attempts 3/4, one new connection, deterministic completion; no implicit POST replay. |
| Incomplete response | PASS; HTTP EOF without `[DONE]` becomes uncertain, not completed. |
| Bounded slow-consumer receive | PASS; 500 ms initial stall, then delayed consumption; peak raw ring 65536 bytes, 12 resume observations, all 400 records, other request completes first. |
| Typed core with live transport | PASS; `core.txt` records two submitted/completed streams, then a real connection-loss request, admission of uncertainty, explicit retry through a new connection, and rejection of an injected late old-attempt chunk. |
| Engine-independent framing | PASS; byte-split CRLF/LF, multiline Unicode, completion marker, malformed UTF-8/NUL and size bound, `framing-test.txt`. |

The harness checks 14 protocol-fixture submissions before four additional typed-core
requests. It asserts observed events, connection counts, exact terminal outcomes,
bounded ring size and no hidden retransmission of an application POST. Protocol
packet retransmission is the maintained engine's responsibility, not a new
FastChat attempt. No production credentials are required.

The unchanged reference suite prints `FAIL fastchat idric core` with exit 0.
`reference-retry-diagnostic.txt` records expected log size 10, actual 9,
one rejected stale observation, a fresh attempt, and successful completion.
This reference-test issue is not relabelled PASS or repaired in the copied source.

## A1 native build and delivery boundary

**Build PASS**: NDK r29, API 24, ELF32 ARM/Bionic. Compiler, ELF, configure output,
input digests and stripped/unstripped sizes are retained under `a1/`. Foreign
libraries are static; the DSO needs only Android libc/libdl. The runtime target
is MIRO A1, 32-bit `armeabi-v7a`, softfp, API 34 and 4096-byte pages. No work for
another phone is represented by this build.

| A1 measurement/stage | Result |
| --- | --- |
| Stripped added native DSO | 4,388,520 bytes; includes the complete static HTTP/TLS dependency configuration. |
| Stripped standalone acceptance probe | 4,390,684 bytes; independently linked test executable, not an APK component. |
| Added APK bytes | BLOCKED/NOT_RUN; this base has no FastChat APK/native Activity package or signing identity. |
| Installation and launch | BLOCKED/NOT_RUN; no physical MIRO A1 endpoint is available. |
| Idle connection RSS / two-active-stream RSS | BLOCKED/NOT_RUN on A1. |
| Establishment / first byte / first event / CPU | BLOCKED/NOT_RUN on A1. |
| Cancellation and reconnect behavior | BLOCKED/NOT_RUN on A1; host behavior qualified separately. |
| A1 sockets/connections | BLOCKED/NOT_RUN. |
| Native Idriç ARM/Thumb adapter/core execution | BLOCKED/NOT_RUN; actual typed-core run uses host Chez. |

The native runtime archive and digest are published alongside this branch's
evidence. They contain the DSO, executable, licenses and exact build receipt;
they contain no source compiler, device build instructions or production tokens.
An artifact build/ELF inspection does not establish installation or device
execution. Cat Food's `62ac940588f5ee4c5be468d48e38d74514da3757` A1 IB observation
is an ordinary-file baseline, not HTTP transport evidence. The A1 ABI profile
comes from android-NDK `7c61ee43e75f7c2dab9288edb0e10055898b36e6`.

## Host measurements, not A1 measurements

One local multiplex run observed: establishment 3.628 ms, first response byte
8.193 ms, first admitted event 7.616 ms, RSS 7224 KiB after both response-start
observations and 7484 KiB after both provider completions, 6134 µs total process
CPU over 98.806 ms wall time. Cancellation observation was 5.206 ms in its
separate case. Connection-loss/retry completed in 128.224 ms with two total
connections. Values are raw diagnostics, not performance thresholds.

The curl first-byte timer can refer to a different concurrent request from the
first observed event. RSS snapshots include the probe and libraries; idle is
after request completion with the session cache still owned. CPU includes TLS,
probe output and bookkeeping, not isolated model-stream work. `ru_maxrss` is a
separate pre-final-metric sample. Host socket counts are not separately audited;
one connection is corroborated by the server session and curl connection metric.
No A1 latency, RAM or superiority claim follows from these single host runs.

Production provider compatibility, JSON/tool-event adaptation and account
evidence remain NOT_RUN. See the transport README for synchronous DNS, request
deadlines, identity lowering and other current experiment limits.
