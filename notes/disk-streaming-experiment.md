# FC-S1 — disk-backed live-streaming comparison

Status: **BLOCKED at the Icky C frontend prerequisite; not implemented**.

Independent branch: `sun/disk-streaming-icky-c-lua`.
Exact fetched main base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b`.
Keep this experiment unmerged.

The requested path is:

`provider chunks → disk/store → renderer follows eligible stored prefixes → completed immutable response`.

The shared [store/admission contract](disk-store-contract.md) stays identical
to the disk-first experiment. The streaming change affects visibility. It does
not select HTTP/2, a different language, different conversation semantics,
another renderer family, appendFAT or another device.

## Prefix visibility rule

A body prefix is eligible only when its bytes are durable and referenced by
durably committed admitted chunk events, with complete UTF-8 code points.
Visibility never exceeds committed extent. Written but unflushed or uncommitted
tail bytes remain invisible. Keep received, written, durable, committed and
display-eligible lengths separately observable.

Network boundaries are not text or framing boundaries. Incomplete UTF-8 bytes
can remain stored, but a displayed string cannot end in a partial code point.
JSON/SSE decoding sits at the transport adapter. Markdown delimiters and code
fences need renderer state across prefix reads; they do not justify a second
full-answer buffer.

Use batching rather than an unconditional fsync per tiny fragment. The chosen
batch bound and timing require measurement. A slow renderer follows a store
cursor through a bounded viewport buffer; it does not retain the full answer.
A slow store blocks/throttles the producer explicitly. Measure every buffer's
configured bound and observed high-water size.

Replay must distinguish committed completion, recoverable partial attempt,
stale/abandoned attempt, and uncertain remote outcome. Partial display is labelled
as partial; it never converts an interrupted request into completed history.

## Acceptance still owed

Complete the C implementation before beginning Lua. Execute the shared
[acceptance matrix](../qualification/acceptance-matrix.tsv) and
[comparison corpus](../qualification/comparison-corpus.tsv), including hostile
UTF-8, framing, Markdown, code-fence, tiny/giant-chunk and keepalive cases.
These are pending fixture descriptions, not executed tests or a test runner.

Compare the same deterministic corpus with FC-D1 and RAM control. Record peak
RSS, disk bytes/write count, first-visible-text latency, terminal-to-final-render
latency, CPU while streaming, cold replay and all bounded-buffer high-water
sizes. Compare the resulting completed response bytes and canonical event
streams exactly. All measurements are **NOT_RUN**, not zero.

## First blocker and subsequent gates

The arrow probe fails with the owned ICK C frontend; the ordinary-C control
passes. [Exact receipt](../qualification/icky-assignment/README.md).

After that prerequisite exists, the complete C streaming slice, fault/replay
tests, A1 native packaging and later Lua refactor remain required.
The sibling disk-first branch has no working implementation yet; it is not a
merge prerequisite. This branch does not inherit Idriç code or depend on merging
the semantic reference.

A1 artifact, signer/update identity, physical execution and the FC-D1 comparison
are **BLOCKED/NOT_RUN**. No compiler is to be installed on the phone.
