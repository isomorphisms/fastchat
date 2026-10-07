# First C pass: source prepared; execution in progress

This is implementation source, **not a working vertical-slice receipt**.
The two-pass gate remains closed: no Lua policy implementation has begun.

Exact fetched FastChat main base:
b49f0f7083d2d034ba76b910a43592d6d07bd18b.

## Implemented source boundary

The named composition is submit_text → admit_event(START) →
store_response_bytes → commit_stored_prefix → admit_event(COMPLETE) →
read_response_window. Assignments use the current ICK C ← token; pointer
declarators/dereferences keep their C meaning.

One conversation owns separate request and attempt counters. Each new request
and explicit uncertain retry creates a separate response file. A length-delimited
binary journal supplies sequence order. Its fixed, little-endian header contains
request, attempt, event, referenced extent, rolling response CRC and record CRC.
Prompt bytes are bounded to 1024. History and attempt files remain on disk; replay
holds only the current conversation metadata.

Response writes use slices of at most 4096 bytes and reserve backing space through
posix_fallocate in 65536-byte increments. Written bytes advance only on actual
write results. At a 16384-byte batch boundary, data fsync precedes the prefix
journal write and journal fsync. Committed advances afterward. Completion first
commits the final prefix, truncates reservation slack, seals the file permissions,
syncs it, then commits a distinct completion event.

The store is synchronous: slow disk blocks the producer; explicit would-block
returns FC_BACKPRESSURE and the accepted byte count. The caller retains/reoffers
only the unconsumed slice. Permanent errors poison the writer until replay.
There is no queue and no full-response allocation in the disk implementation.
The RAM sink exists only in the test program.

Cancellation pending admits response bytes, start, completion and cancellation
acknowledgment according to the active Idriç core's phase table, including pending
cancellation requested before response-start. Provider cancellation is admitted
during generation as well as after a local cancellation request. The first admitted terminal wins.
Uncertain delivery requires explicit retry; old-attempt bytes and terminals fail
admission without new journal records. This follows the semantics inspected in
[isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4)
at c66021957fb5dc195a995b2c0cd212863ff68915, without depending on its merge.

Restart validates record framing, sequence, lifecycle and referenced response
checksums. A torn trailing record is truncated to the last admitted boundary.
A complete damaged record is a corruption error. A nonterminal recovered attempt
gets a durable uncertain-delivery event; neither its reserved capacity nor its
uncommitted file tail becomes completed history. Files for abandoned attempts
remain separate. The next unused attempt ID can safely reclaim its orphan file
under the exclusive journal lock.

A renderer reads a bounded 4096-byte UTF-8 window directly from disk. Incomplete
code points are withheld. Invalid sequences produce an explicit text error.
The disk-first policy returns no body until completion; the streaming sibling
uses the same reader against committed prefixes. Markdown is currently plain
text; there is no Markdown parser or private full-answer renderer buffer.

## Recipe and authored tests

Use the real Grease entrypoint and a source-built ICK driver/support directory:

    grease scripts/test-core.grease ICK_DRIVER ICK_LIBEXEC OUTPUT [target compile/link options...]

This only builds; execute OUTPUT with the matching runtime. The recipe requires
an explicit driver, accepts explicit target libraries, compiles the actual arrow
source, and never substitutes Clang. Recipe execution is NOT_RUN. The exact-head GitHub producer workflow builds
ICK and executes the C source; pending runs are not passing evidence.

tests/conversation.c contains assertions for empty bodies; every cut of a valid
UTF-8 body; completed replay; duplicate terminals; cancellation races; explicit
failure; short/partial writes; exhaustion; failed barriers; interrupted restart;
uncertainty/retry/stale attempts; actual child-process death; torn/corrupt journal
records; backpressure; a giant offered chunk; bounded reads; and byte-for-byte RAM
control comparison. Authored tests are not passing test evidence.

## Producer recovery and qualification — 2026-10-07

After the compiler repair, an actual syntax check of unchanged NDK r29 file and
NativeActivity headers passed with the locally rebuilt ICK/GCC AArch64 frontend,
API 24, and BIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD. The latter is Bionic's documented
opt-out for the optional C signedness overload; annotations were retained.
This is an AArch64/API-24 declaration probe, not the A1/API-21 application gate.

The repair checkout is based on ICK assignment fix
14f582c920af18ec20eb5fad2583926e0560b3f5. Its extra parser/type-check changes
are locally prepared, not yet published/qualified as a compiler revision.
Existing [dilapidated-shed/ick PR #74, “Qualify A1 application C without ICK-to-Clang fallback”](https://github.com/dilapidated-shed/ick/pull/74)
also has active declaration work; reconcile that existing work rather than
opening a competing integration request. Its current source is not substituted
for the tested local repair.

The earlier fresh ARM32 compiler build stopped at a bootstrap dependency:
gengtype-lex.cc was missing after flex could not run. Before repair/retry,
the local executor stopped returning even a pwd command, including from /tmp
with a non-login shell. Outstanding file writes/build checks could not be
confirmed. GitHub remained available and preserved these source files.

Producer execution has returned with replacement checkouts under a different
workspace; the unpublished compiler sources are being reconstructed. The
original materialized compiler/build directories are absent. The first host
GitHub run at FastChat 6710f393318c0d4d18c30c995dd969f1b998b9b1 compiled the
actual arrow C source, then failed at static linking because its recipe omitted
GCC's unwind library. It executed no application tests or measurements.
[Run 37576109211](https://github.com/isomorphisms/fastchat/actions/runs/37576109211).
The revised recipe explicitly links libgcc_eh and preserves the producer compiler.

Required next work, still authorized: preserve and
qualify the compiler fix (including API 21 ARM32), build/run/repair this C core,
complete the native Canvas/Paint presentation and composer through the pinned
android-NDK NativeActivity packager, then perform actual Icky Lua policy
refactoring using isomorphisms/lua at 87306483cec50f8c750a22dda1d0742246fad756.
No stock-Lua fallback or consumer glyph translator is permitted.

Full corpus/framing runner, streaming hostile-split tests, Android
APK/signing/publication and all physical MIRO A1 acceptance
are **BLOCKED/NOT_RUN**. A separate-process 8 MiB C control comparison is authored
in the producer workflow; its measurements are pending exact-run evidence.
These are bounded host file-reader measurements, not Android visible-text latency.
No build tools were installed on the phone.
The experiment branches remain independent and must not be merged.
