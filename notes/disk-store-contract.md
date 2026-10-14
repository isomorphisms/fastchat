# Shared experiment contract

Status: design preserved while the C language prerequisite is blocked.
This is not a storage implementation or runtime evidence.

The two comparison branches vary response visibility. They must use the same
transport-independent conversation semantics, ordinary-file store boundary,
native text renderer family, deterministic corpus and two-pass language order.
No HTTP/2 work, appendFAT implementation, extra RAM branch or extra device is
part of either experiment.

## Store

`AppendAddress { stream; offset }` identifies a position in an attempt's byte
stream. Conversation identity, request identity and attempt identity remain
distinct. A stream address is not a request or event identity.

Capacity is reserved backing capacity. Written extent tracks successful writes.
Durable extent tracks bytes whose storage durability barrier succeeded.
Committed extent tracks bytes referenced by durable, admitted canonical events:

`committed ≤ durable ≤ written ≤ capacity`.

Keep a response data stream and a length-delimited canonical event journal.
The core decides whether an event is admissible; the store supplies durable,
monotone order. Flush referenced response data before durably publishing its
canonical journal record. Advance committed extent only after that publication
succeeds. Batched commits may cover several chunks. Completion refers to the
exact final extent and seals the response against further writes.

An interrupted/torn journal record is not a complete event. Uncommitted tail
bytes do not imply a completed response. Replay must validate record boundaries
and references before admitting history, distinguish an incomplete attempt from
a completed immutable response, and preserve uncertainty about remote delivery.
Actual record encoding, torn-write detection and filesystem barriers require
implementation and fault tests; this note does not prove them.

Use bounded producer buffers and explicit write/finish results that propagate
short writes, interrupted calls, storage exhaustion and transport backpressure.
Do not build the complete answer in RAM before disk writes. The RAM sink remains
a comparison control. Keep the adapter seam compatible with future appendFAT
without implementing that backend here.

## Conversation admission

The semantic reference is [isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4)
at `c66021957fb5dc195a995b2c0cd212863ff68915`.

- Submit records user text plus an independent request/attempt identity.
- Response-start precedes admitted chunks and completion.
- Cancellation requests enter a pending state; they do not immediately mean
  the provider acknowledged cancellation. Chunks can still be admitted there.
- The first admitted completion/cancellation/failure terminal outcome wins.
  Duplicate or later terminal observations remain diagnostics.
- Transport loss records uncertain delivery. Explicit retry keeps the request
  identity, creates a fresh attempt, and rejects observations from the old one.
- Store failures do not invent successful durable canonical events.

Transport chunks are bytes. Decoding/framing must not treat them as complete
UTF-8, SSE, JSON, Markdown or token units. Exact canonical response bytes and
event order must agree across disk-first, streaming and RAM control runs.

## Renderer and Android boundary

The inspected internal precedents are
`Ashtray-Archer/utilities-android-phone-user@ec022aea6fe6836ea78f479b498e85b3452d88b7`:
`accelerometer/app/src/main/c/native_main.c` (`ANativeWindow_Buffer`) and
`math-characters/app/src/main/c/native_main.c` (Android Canvas/Paint).
Borrow platform text services for a readable native surface and composer;
writing a new shaping engine is not part of this experiment.

Cat Food owns A1 facts. Keep device facts there, not duplicated in FastChat.
Framework NativeActivity packaging belongs to the pinned android-NDK boundary.
Use a stable explicit signer, package identity and monotone version code when
the application reaches packaging. Compile off-device. Record stripped native
sizes, APK bytes, source revision, SHA-256 and separate physical acceptance.

## Two passes

First complete and execute the whole functorial Icky C vertical slice.
Only then refactor policy/composition into the actual Icky Lua fork and rerun
the same tests. Do not begin the Lua implementation while the C pass is blocked.
