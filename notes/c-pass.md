# First C pass

Exact fetched main base: b49f0f7083d2d034ba76b910a43592d6d07bd18b.
Both comparison branches remain independent and unmerged. Lua has not begun:
the native C slice still needs producer qualification.

The named composition is submit_text → admit_event(START) →
store_response_bytes → commit_stored_prefix → admit_event(COMPLETE) →
render_stored_window. Descriptive boundaries use accepted ICK C ← spellings,
including initialization. Pointer syntax retains its C meaning.

One conversation has separate monotone request and attempt identities. Each
attempt has an ordinary response file. next_append_address returns the attempt
stream and written offset. A length-delimited binary journal supplies durable
ordering, recording sequence, identity, event, referenced extent and CRCs.
Replay validates the framing, lifecycle, sequence and referenced stored bytes.

Capacity, written, durable and committed remain distinct. Received is the
offered logical byte extent; eligible is the committed byte limit. Text visibility
additionally withholds incomplete UTF-8 scalars. Writes are bounded to 4096 bytes.
Capacity reservation grows in 65536-byte steps. At 16384 written bytes since
the last commit, response fsync precedes the prefix record and journal fsync.
The native fixture uses a shared 100 ms barrier in both branches. No fsync
is tied to every network fragment. Completion commits the remaining prefix,
truncates slack, seals file permissions, syncs it and admits a distinct completion.
The completed response cannot be appended through the API.

The synchronous store blocks input on slow disk. Backpressure returns accepted
bytes; permanent failure poisons the writer until replay. The fixture decoder
retains three fixed 4096-byte buffers. Its narrow SSE grammar is one JSON data
line per event, with comments/empty keepalives ignored. Unicode escapes and
surrogate pairs are decoded to UTF-8. Oversized frames are explicitly rejected.
There is no disk-path allocation proportional to the response. The RAM sink
exists only in the control tests. Future appendFAT can replace the small
file/write/barrier/read boundaries without changing stream/offset or admission;
appendFAT is not implemented.

The phase table follows
[isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4)
at c66021957fb5dc195a995b2c0cd212863ff68915, without depending on its merge.
Cancellation pending permits response events, including cancellation requested
before start. Provider cancellation is admitted while generating. The first
terminal wins. Loss becomes uncertainty; explicit retry keeps the request and
changes the attempt. Old-attempt events are rejected.

Replay truncates a torn journal tail; a full corrupt record is an error.
Nonterminal recovery durably records uncertainty. A committed partial prefix,
an uncommitted tail, a stale attempt and a completed immutable response remain
distinct. Older attempt files remain separate; replay retains only current
metadata and bounded buffers. The renderer reads one 4096-byte stored window,
completed-only in FC-D1 and admitted prefixes in FC-S1. Markdown is exact plain
text in the minimal native presentation.

## Qualified host receipt

Source 70a1ac0f977dec15356b8926cd19fad50e2979e5 passed in
[run 37577668540](https://github.com/isomorphisms/fastchat/actions/runs/37577668540).
Owned ICK source: 14f582c920af18ec20eb5fad2583926e0560b3f5. Its source-built
GCC frontend compiled actual arrow C. System GCC bootstrapped ICK only;
GNU libraries provided the explicit static host link.

The required empty/chunk/UTF-8/exact-byte/restart/duplicate/stale/cancellation/
uncertainty/RAM-equivalence cases passed, including short writes, exhaustion,
failed barriers, child-process death, torn/corrupt journal and backpressure.
Separate-process 8 MiB host controls measured:

| Control | Peak RSS KiB | Data bytes | Journal bytes | Writes | Max write/view |
| --- | ---: | ---: | ---: | ---: | ---: |
| Disk-first | 816 | 8388608 | 32969 | 2563 | 4096/4096 |
| Streaming | 944 | 8388608 | 32969 | 2563 | 4096/4096 |
| RAM | 9036 | 0 | 0 | 0 | 0/4096 |

Both disk controls produced identical response and canonical journal bytes,
rolling CRC 31e202a1. File-reader first-text times were 296.074, 0.664 and
2.187 ms; terminal-to-first-window times 0.309, 0.388 and 0.000 ms respectively.
These are host file-reader measurements, not Android visible-text acceptance.
The old replay number was a warm same-process reopen. The expanded runner now
labels that and separately measures fresh-process replay, filesystem cache
unspecified, plus a steady resident sample.

Transferred xgcc SHA-256:
ae9a7c6f9875e23e25f89f3e15fdebe5668e36dd6f17f5c390e57f60af943f75.
Transferred cc1 SHA-256:
8781512d09f7063e93b0fbfc9f4e359e383bebd21fcb3124f5c38901678679f5.
Current framing/corpus additions also pass locally with this compiler and the
pinned Grease implementation. The old run does not qualify newer source;
a fresh exact-head producer run binds the expanded slice.

## Remaining C gate

scripts/test-core.grease has executed successfully through real Grease.
The expanded runner tests every SSE/JSON/Unicode/Markdown/fence split,
keepalives, decoder backpressure, shared TSV corpus, steady RSS and fresh-process
replay. See notes/android-producer.md for the native presentation/build recipe.

The Android metadata repair is preserved in
[dilapidated-shed/ick PR #74, “Retain Android annotations and qualify A1 Icky C applications”](https://github.com/dilapidated-shed/ick/pull/74).
Its first ARM run accepted unmodified Bionic headers and enforced API availability,
then failed a multiline ARM-attribute receipt check. The repaired ARM job passed
at c2b84a381d23bf5d0b5153e24be51a52e78a022e in
[run 37580514958](https://github.com/dilapidated-shed/ick/actions/runs/37580514958),
job 112659050603. Compiler artifact 11464479465 preserves its exact producer output.

Native producer execution is pending. Signed APK is BLOCKED by absent explicit
signer inputs. Physical A1 execution, screen latency and device RSS are
BLOCKED/NOT_RUN. No build tools were installed on the phone. Complete the C
native slice before the actual Icky Lua pass using
isomorphisms/lua@87306483cec50f8c750a22dda1d0742246fad756.
