# Disk-authoritative comparison

Historical comparison description. The converged candidate is authoritative
in [native-candidate.md](native-candidate.md): its shared arena is not truncated
or permission-sealed, and its framing/provider implementation is shared between
the deterministic and HTTP/2 backends. Earlier per-attempt and retry record
descriptions below are reference evidence, not the candidate format contract.

These experiments share a conversation core, file store, fixture protocol and
Canvas/Paint renderer. The working C checkpoint is followed by the actual
[Icky Lua composition pass](icky-lua-pass.md). FC-D1 reads a response only after its completion record;
FC-S1 also reads admitted stored prefixes. Main was fetched at
`b49f0f7083d2d034ba76b910a43592d6d07bd18b`. Neither experiment depends on merging
the other or [isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4)
at `c66021957fb5dc195a995b2c0cd212863ff68915`.

## Ordering and visibility

Submission commits user text and allocates distinct request/attempt identities.
Provider start admits a generating attempt. Decoded response bytes go directly
to its file in writes of at most 4096 bytes. No disk path collects a whole answer
before writing. `AppendAddress {stream, offset}` names the attempt and actual
written byte position, independently of an operating-system descriptor.

Allocated capacity, received logical bytes, written bytes, durable bytes and
committed bytes are separate counters. Always:

`eligible ≤ committed ≤ durable ≤ written ≤ capacity`.

Received counts logical response bytes offered to the sink, including an
unconsumed offer; it does not count JSON/SSE envelope bytes. Completion rejects
an outstanding unconsumed offer. On replay, received/written/durable extents are
reconstructed at the committed lower bound; pre-death uncommitted receive/write
progress is unknown, and a nonterminal attempt becomes uncertain. Reservations grow
in 65536-byte increments. Every 16384 written bytes, a data barrier precedes a
checksummed prefix record and journal barrier. Native presentation also requests
a barrier after 200 ms with pending bytes. Both branches use the same batching;
neither syncs every network fragment. Metadata events first commit pending data.

Committed prefixes authorize byte reads. A reader returns only a complete valid
UTF-8 prefix within a 4096-byte window: its returned text extent can lag the
eligible byte extent. Displayed text and canonical completion are different.
D1 returns no response bytes until `COMPLETED`. S1 may display a partial attempt,
including a recovered prefix explicitly labelled uncertain, cancelled or failed.
Completion truncates reservation slack, seals permissions, syncs the file, then
commits a separate terminal record. Further response writes/terminals are rejected.

Canonical metadata is admitted by the core; durable sequence is supplied by the
store. Journal records carry conversation, request, attempt, sequence, event,
referenced extent, rolling response checksum and record checksum. These CRCs
detect corruption; they are not an adversarial integrity mechanism.

## Interruption and races

The first admitted terminal wins. Pending cancellation accepts chunks and start,
even when cancellation preceded start, as in the active Idriç core. A provider
cancellation can also terminate generation without a local cancel request.
Uncertainty accepts only explicit retry; retry keeps request identity and obtains
a new attempt. Events with old identities cannot write or extend the journal.

Replay validates framing, order, phase, identities and referenced file checksums
with bounded reads. Torn trailing records are removed; a complete corrupt record
fails closed. A nonterminal attempt receives a durable uncertain-delivery record.
Its reserved/uncommitted tail cannot become a completed response. Old attempts
stay separate. Completed records replay immutably; an old provider is never
reattached automatically. A valid terminal record found on replay may have survived
before the prior process acknowledged it; replay recovers durable records, not
whether the previous UI saw them.

## Bounds and transport

Ordinary files and an exclusive journal lock are the first storage backend.
The append address, extent order and write/finish admission boundary permit a
future appendFAT store. No appendFAT code is included. Posix file operations are
isolated in named store functions rather than exposing them to a transport.

The synchronous file writer blocks the producer on a slow disk. An explicit
would-block result carries the consumed byte count. The fixture adapter retains
one decoded frame and resumes precisely at the unconsumed slice. It has two
8192-byte buffers: wire line and decoded data. A frame exceeding that bound is
rejected. A giant *input offer* can contain arbitrarily many bounded frames.
The renderer has a 4096-byte window and no queued copies of answer text. Page
navigation keeps at most 64 byte offsets; an arbitrarily slow renderer does not
hold up durable ingestion or create an answer-sized private copy.

The deterministic fake adapter accepts one `data:` field per SSE frame,
`[START]`, JSON `{"text":"…"}`, `[DONE]`, `[FAILED]`, `[LOST]`, empty frames and
comment keepalives. It decodes JSON escapes/surrogate pairs across any input
split. Unknown or multiline data frames fail explicitly. It is a fixture protocol,
not a complete live-provider adapter or an HTTP/2 implementation. Markdown and
code fences are displayed as plain text and preserved byte for byte.

## Native presentation and evidence

The native adapter uses the internal Canvas/Paint precedent through public JNI,
plus a platform EditText for Unicode composition/IME. It contains no application
DEX. `ANativeWindow_toSurface` requires the declared API-26 floor. It presents
the latest turn, with bounded paging; older turns remain durable but this minimal
UI has no history navigator. Native callbacks, disk operations and fixture ticks
currently share the main looper: physical responsiveness under slow storage is
an acceptance question, not a measured success.

Cat Food owns current MIRO A1 device facts:
[authority](https://github.com/isomorphisms/catfood/blob/62ac940588f5ee4c5be468d48e38d74514da3757/AGENTS.md).
Packaging uses [android-NDK's NativeActivity boundary](https://github.com/isomorphisms/android-NDK/blob/7c61ee43e75f7c2dab9288edb0e10055898b36e6/apk/build-nativeactivity-apk.sh).
Builds run on the producer, never on the phone. Host tests and file-read latency
are not physical MIRO A1 execution or visible-answer latency. Exact current
receipts and blockers are recorded under `qualification/fc-comparison/`.
