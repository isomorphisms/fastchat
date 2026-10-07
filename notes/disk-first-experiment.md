# FC-D1 — disk-first comparison

Status: **assignment syntax passes on the pinned compiler fix; application not implemented**.

Independent branch: `sun/disk-first-icky-c-lua`.
Exact fetched main base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b`.
Keep this experiment unmerged.

The requested path is:

`provider chunks → durable local store → completed immutable response → renderer`.

The renderer must receive no response body during generation. Its first response
body read occurs only after the core admits completion and the store durably
commits the completion record and its exact immutable response extent. Failure,
cancellation and uncertain-delivery status may be presented without presenting
a partial response as complete.

The shared [store/admission contract](disk-store-contract.md) is preserved for
comparison with `sun/disk-streaming-icky-c-lua`. The RAM sink is a test/control
within this experiment; it does not create a third branch.

## Acceptance still owed

Complete the C implementation before beginning Lua. Execute the full shared
[acceptance matrix](../qualification/acceptance-matrix.tsv), using the
[comparison corpus](../qualification/comparison-corpus.tsv). These files describe
pending cases; no storage or semantic test runner exists yet.

The corpus is tab-separated: response bytes are hexadecimal, `-` denotes an
empty body, and `chunk_bytes` gives ordered input chunk sizes. Hostile UTF-8
splits deliberately divide both a three-byte and a four-byte code point. The
same corpus bytes and lifecycle cases must be used for both branches.

Long-response measurements still require a deterministic generated body (at
least several MiB), RAM control and disk sink in separate processes, plus an
instrumented bounded renderer. Record steady/peak RSS, response/journal bytes,
write count, CPU, terminal-event-to-visible-completed-answer time, cold restart
and replay time. Values are **NOT_RUN**, not zero.

## First blocker and subsequent gates

The unchanged arrow probe now passes with the owned rebuilt ICK on its pinned
fix branch. [Fixed frontend receipt](../qualification/icky-assignment/fixed-frontend.md).
The original failure diagnostics remain in the historical receipt.

With the pinned arrow path: complete the C vertical slice and deterministic
fault/replay tests; qualify ARM32/Bionic declarations and native platform linking;
produce readable NativeActivity presentation and composer; only then perform
the Icky Lua policy pass and repeat tests.

A1 APK, signing/update identity, artifact digest, memory comparison and physical
execution are **BLOCKED/NOT_RUN**. No compiler is to be installed on the phone.

