# HTTP transport experiment

The conversation owns `model_request`, `request_id`, `attempt_id`, and
`model_event` from FastChatCore. The transport action takes a model request and
produces ordered observations; cancellation names one request and attempt.
The transport must not create retries or choose canonical event order.

Domain signatures: submit : model_request → IO stream; cancel : request_id →
attempt_id → IO result; observe : stream → IO model_event. Distinct request and
attempt types remain above the foreign ABI's 64-bit integers.

Effects: HTTPS POST, bounded incremental receive, cancellation, event delivery.
Invariants: at most two requests, one authority/origin, two attempts never share
an identity, no silent replay, no stale event admission, no incomplete-success.
Required services: asynchronous TLS/HTTP2 callbacks, multiplexing, reset and
flow-control primitives, ARMv7 Bionic runtime, native event polling FFI.

`NativeTransportAttempt.idric` is a compilable-syntax capability probe. It tries
the documented module/import surface before selecting a foreign protocol
implementation. The pinned compiler has no `Network.HTTP2` module. The exact
command/result lives in the evidence directory. The native implementation is
handwritten library glue; it is not RefC or generated-C lowering. The existing
core is copied unchanged from FastChat PR #4 at its recorded exact head solely
for independent semantic qualification; no branch dependency is introduced.

Node's maintained HTTP2 API supplies the foreign fixture server. An H3 fixture
uses aioquic's Python API at the external library boundary. Neither fixture
implements HPACK, QPACK, QUIC, or TLS. Their success is protocol-harness evidence,
not Idriç implementation or mobile backend acceptance.

First language work: a typed polling FFI binding with bounded byte ownership on
the ARM/Thumb backend. Acceptance must submit a real request, poll observations,
cancel one of two streams and preserve each request/attempt identity on A1.
