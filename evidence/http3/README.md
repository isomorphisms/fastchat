# H3 evidence — 2026-10-07

Base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b`, independently fetched `main`.
Branch: `sun/transport-http3`. H2 was inspected and its common adapter, core,
framing and payload semantics reused without a Git ancestry dependency.
The wire engine is curl + ngtcp2 + nghttp3 + OpenSSL 3.5.5. FastChat core semantics
are identical. QUIC connection/stream IDs, QPACK and recovery stay private.

| Behavior | Result |
| --- | --- |
| Establishment/ALPN/TLS | PASS, live host QUIC/TLS 1.3 with `h3`; certificate/hostname verified. libcurl negotiated enum 30. |
| Request and stream data | PASS, exact deterministic POST and separately framed delayed data. |
| Two requests/one session | PASS, distinct QUIC wire streams, one connection. |
| Independent cancellation and reset | PASS, cancelled request terminates while peer completes; forced stream reset affects only that attempt. |
| Connection interruption | PASS, forced close after partial response produces uncertain delivery for in-flight attempts. |
| Explicit retry | PASS, attempts 3/4 are new identities and establish one fresh connection. No implicit POST replay. |
| Missing provider completion | PASS, stream EOF becomes uncertain. |
| Buffering/backpressure | PASS, 500 ms consumer stall plus delayed drain, 400 full records, other stream completes first, peak ring 65523 bytes, 11 resume observations. |
| Typed core and stale events | PASS, host Idriç/FFI/core consumes two real H3 requests, then admits a real connection-loss request as uncertain, retries on a new connection and rejects an injected late old-attempt event. |
| Deterministic packet loss | PASS, every seventh inbound datagram after ALPN dropped; fixture log confirms actual drops; same acceptance assertions still pass. |
| Abrupt path loss | PASS, `/blackhole` stops all traffic on the old connection; 30 s request timeout produces uncertainty; explicit new attempts reconnect and complete. Total case 30101.101 ms. |
| 0-RTT | Disabled in client; fixture supplies no session-ticket store and asserts `early_data_accepted=false` on observed handshakes. No early state-changing submission is used. |

Raw ordinary results are in `host/`; packet-loss and path-loss results in `loss/`.
The fixture uses aioquic 1.3.0 rather than handwritten QUIC/TLS/QPACK.
`FC_FIXTURE_DROP_EVERY=7` and `FC_TEST_PATH_LOSS=1` reproduce the extra checks.
The deterministic packet-loss model is inbound-only and not a realistic mobile
radio model. The blackhole tests request-deadline recovery, not path migration.
No connection migration or NAT rebinding qualification is claimed.

## Native A1 build

NDK r29/API 24 builds both native ARMv7 outputs: stripped DSO 4,823,964 bytes and
standalone probe 4,826,128 bytes. `a1/` records compiler, ELF, configuration,
input digests and artifact digests. A1's runtime is 32-bit Bionic/softfp/API 34,
4 KiB pages. Only Android libc/libdl are dynamically required. The native archive
contains runtime outputs, notices and receipt, with no compiler prerequisites
on the device.

**Physical A1: BLOCKED/NOT_RUN.** Installation, launch, idle/two-stream RSS,
handshake/first-event latency, streaming CPU, sockets and reconnection on the
phone remain unmeasured. No available attached/remote physical A1 endpoint was
provided. The artifact is not an APK; application/Activity/DEX integration and
signing remain absent on this base. ARM/Thumb Idriç FFI/core execution is also
unqualified; the live semantic test is host Chez. No other phone is a target.

Production endpoint H3 support, provider credentials and JSON/tool-event
compatibility are NOT_RUN. This controlled protocol fixture does not establish
provider-account compatibility. Source/dependency licensing is recorded in
`transport/DEPENDENCIES.md`; no full stack was implemented for a minimal target.

See [direct H2 comparison](H2-COMPARISON.md) and
[delegated-channel design](../../notes/transport-delegated-channel.md).

The unchanged semantic reference's retry test still reports FAIL (expected log
size 10, actual 9). Its exact original blobs and diagnostic are retained in the
H2 reference evidence. The new live transport/core and invariant checks pass;
the reference issue does not become a falsely green prerequisite.
