# H3 versus H2 — same native configuration and fixture semantics

This is a reproducible protocol experiment plus build-size comparison. It does
not answer whether H3 improves the physical MIRO A1: that portion is
**BLOCKED/NOT_RUN**. Both branches are independently rooted at
`b49f0f7083d2d034ba76b910a43592d6d07bd18b` and use the same typed semantics,
native glue, framing, compiler settings and pinned TLS/curl source.

H3 adds ngtcp2/nghttp3/crypto adapter while retaining H2 capability in its build.
Its requests remain strictly H3, with no fallback or 0-RTT. The comparison
therefore measures adding H3 to this particular native configuration.

| Build measurement, ARMv7/API 24 | H2 | H3 |
| --- | ---: | ---: |
| Stripped DSO bytes | 4,388,520 | 4,823,964 |
| Stripped standalone probe bytes | 4,390,684 | 4,826,128 |
| DSO addition | — | 435,444 bytes (9.92%) |
| Added APK bytes | BLOCKED/NOT_RUN | BLOCKED/NOT_RUN |
| Native Android dynamic requirements | libc/libdl | libc/libdl |

These are whole configured artifact bytes, not compressed APK bytes or runtime
RAM. The standalone test executable duplicates linked code; it is not an
additional production APK component. Digests/configuration are retained.

The following are **single host Linux loopback observations**, from
`evidence/http2/host/` and `evidence/http3/host/`. Identical request bodies, record
counts, chunk splits and 30 ms delays are used. The servers are different mature
implementations (Node H2 versus aioquic H3), and the runs are not a statistical
benchmark. Results cannot isolate protocol cost from fixture/runtime scheduling.

| Host observation | H2 | H3 |
| --- | ---: | ---: |
| Establishment timer, multiplex case | 3.628 ms | 7.503 ms |
| First byte timer, multiplex case | 8.193 ms | 7.831 ms |
| First observed ModelEvent, multiplex case | 7.616 ms | 8.724 ms |
| RSS after both response-start observations | 7224 KiB | 7936 KiB |
| RSS after completion, session cache owned | 7484 KiB | 8200 KiB |
| Total process CPU / case wall time | 6134 µs / 98.806 ms | 5923 µs / 101.472 ms |
| Cancellation observation, separate case | 5.206 ms | 0.440 ms |
| Forced-close + explicit retry total | 128.224 ms | 135.219 ms |
| Connections before/after loss retry | 1 / 2 total | 1 / 2 total |
| Slow-consumer peak raw ring | 65536 bytes | 65523 bytes |
| Slow-consumer resume observations | 12 | 11 |
| Slow-consumer CPU / wall time | 32201 µs / 2756.352 ms | 33156 µs / 2798.026 ms |
| Concurrent peer finishes despite cancellation/stall | PASS | PASS |

Curl's first-byte/handshake metric can refer to a different concurrent request
from the earliest observed event. CPU includes probe output/bookkeeping and TLS;
RSS includes its entire process. `ru_maxrss` is retained separately and sampled
before final metric output. Cancellation is local observation latency, not remote
generation-stop or billing latency. Connection counts are server/curl observations;
total OS socket/DNS descriptor count is not independently measured.

Both protocols conservatively report unknown acceptance after interruption,
then admit only fresh-attempt events. H2 tests a destroyed TLS/TCP session; H3
tests forced QUIC close, deterministic incoming packet drops and a complete
blackhole. H3's blackhole case reaches the configured 30 s deadline and completes
fresh attempts at 30101.101 ms. Packet-loss runs still finish deterministic
requests and keep the receive ring bounded. These different impairment models
are functional evidence, not a controlled H2/H3 loss-performance comparison.

Before choosing either for A1, run both native artifacts on the exact physical
phone against the same reachable fixture host, with retained ABI/build fingerprint
and digests. Measure repeated idle/two-active RSS, establishment, first data/event,
CPU and cancellation under the same network conditions, including a common
interruption model. Retain raw samples and distributions. Provider H3 support is
a separate check and remains unknown here. No superiority claim or automatic
merge follows from one internet or host request.
