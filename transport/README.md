# Candidate transport boundary

The application uses `src/transport_boundary.h`. Its opaque engine is private:
conversation and renderer code receive no socket, TLS, connection or HTTP/2
stream identity. Request and attempt identities come from the canonical core.

`native/curl_transport.c` is imported from FastChat PR #8 at
`ae920025c1ef8b8262f2003c59217cb69955c4d4`, then integrated with the shared
provider decoder and store. The former copied core and raw-JSON-as-text Idriç
adapter have been retired; `idric/FastChatCore.idric` is the sole semantic model.
The previous experiment remains at its original branch, without extending H3.

Both backends use `native/sse.c` for bounded, byte-cut-safe SSE framing and
`src/provider.c` for bounded JSON decoding. HTTP/2 retains two channels with
16 KiB receive queues, 8 KiB records/request bodies, and the pinned curl window
and no-retry patch. The application drains one event per tick. Pending storage
backpressure stops new observations and preserves the unconsumed decoded slice.
These are explicit application/window limits, not a whole-process RSS claim.

Completion requires `[DONE]` or a valid completed provider event. Malformed
data becomes explicit failure; EOF or connection loss without completion is
uncertain delivery. Only an explicit user retry creates a new attempt. Runtime
authorization remains private, is never journaled, and is erased on close.

All maintained C, including foreign HTTP/2 C dependencies, is compiled with
the pinned ICK. OpenSSL uses `no-asm`; its physical latency is NOT_RUN. NDK r29
supplies Bionic headers, archive tools and Android platform linking. See
[the complete candidate](../notes/native-candidate.md) for exact identities,
configuration, qualification and the external paired-production blocker.
