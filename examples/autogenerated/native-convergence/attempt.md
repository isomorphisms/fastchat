# Native convergence

Inputs are user commands and provider observations, scoped to a conversation,
request and fresh attempt. Output is an admitted durable event, rejected stale
observation, bounded stored prefix, or explicit backpressure/error.

The executable Idriç model is `idric/FastChatCore.idric`, reconciled from
`c50ca1a1e3aedd873d9e17e31c7ea0ce54782e28`. Its actual per-entry admission
function supplies the native transition masks. The checked Icky C representation
adds file/durability effects, a single active request policy, and bounded readers;
it does not replace the Idriç model with an independently designed lifecycle.

Required primitives are Bionic/POSIX pread/pwrite/fsync, private TLS/H2 curl
interfaces, NativeActivity/Canvas/IME, bounded JSON/SSE parsing and the actual
Icky Lua runtime. PR #8's recorded Idriç ARM/Thumb FFI execution is NOT_RUN;
the existing candidate's explicit Icky C implementation remains the application
lane. No RefC/generated-C or new interpreter fallback is used.

Invariants: first admitted terminal wins; user retry precedes fresh transport
submission; response barrier precedes journal barrier; replay never infers
completion from EOF; storage/render buffers remain bounded; provider tool data
is retained as an event and never executed. The native candidate serializes
composer submissions, whereas the pure model can contain multiple entries.
That scheduling restriction does not change a request's event semantics.

First Idriç capability boundary: native ARM/Thumb FFI/core integration lacks an
execution receipt. Acceptance would run these same entry transitions through
the target backend with file effects, rather than only host semantic fixtures.
