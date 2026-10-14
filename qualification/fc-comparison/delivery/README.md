# Source-bound delivery evidence

This receipt contains executed producer evidence for both independent comparison branches.
Disk-first compiled source: `6dff84f007213b275ed0ffb6bde151769b5bcac0`.
Streaming compiled source: `eeee961cd60cf9281295eb318f2c3fb362385e74`.
The APK in this branch is [fastchat-icky-lua.apk](../../../artifacts/fastchat-icky-lua.apk);
[apk.receipt.tsv](apk.receipt.tsv) binds its exact source, digest, ABI and stable signer.
Both APKs are 94,751 bytes, versionCode 2, NativeActivity/API 26, with no application DEX.
The artifact-recording commit changes evidence only; it does not change compiled source.

Both actual ICK-built executables pass the deterministic suite, including completion
refusing an offered but unwritten slice. Replay reconstructs received/written/durable
progress at the committed lower bound; pre-death uncommitted progress is unknown.
The shared 15-store corpus and 8 MiB response have identical completed bytes and
canonical journals across siblings and across the preserved C checkpoint and Lua pass.
Raw logs, executable digests, comparison receipts and fresh-process replay times
are retained here. The public source trees contain the exact build inputs.

| 8 MiB host control | Peak RSS KiB | Steady RSS KiB | First text ms | Terminal first window ms | Terminal drain ms | Generation CPU ms |
|---|---:|---:|---:|---:|---:|---:|
| Disk first | 1080 | 1036 | 84.315 | 0.026 | 87.538 | 84.144 |
| Streaming disk | 1076 | 1032 | 0.228 | 0.020 | 0.137 | 198.386 |
| RAM control | 9304 | 9256 | 3.702 | 0.000 | 86.225 | 3.636 |

These are file-reader timings on the disposable x86_64 producer, not Android
visible-frame timings. RSS excludes the kernel page cache. Each disk branch wrote
8,388,608 response bytes and 32,969 journal bytes in 2,563 writes; these are
application write counters, not physical block-device traffic. Maximum write and
view were 4096 bytes. The healthy policy heap peaked at 12,637 bytes under its
131,072-byte cap. The giant wire-offer test measured 8203 active framing bytes
inside two fixed 8192-byte buffers. There is no renderer queue or full-answer
private copy. RAM is only a test/control sink.

Fresh-process replay took 94.747 ms (disk first) and 88.332 ms (streaming);
filesystem cache state was unspecified. Physical cold replay is not established.
Both exact source heads passed host-core and Android APK producer CI.
[producer-ci.tsv](producer-ci.tsv) records runs and GitHub-reported artifact ZIP
digests; those ZIP digests identify separate CI bundles, not this branch's APK.
[gates.tsv](gates.tsv) records exact toolchain pins and acceptance boundaries.
Build recipes, policy responsibilities and storage invariants are described in
[the Lua pass](../../../notes/icky-lua-pass.md) and
[the disk path](../../../notes/disk-path.md).

Physical MIRO A1 installation, launch, IME/Canvas readability, touch/lifecycle,
long-response RSS/CPU and visible-answer/replay measurements are
**BLOCKED/NOT_RUN**. No physical execution path was available, and no phone
compiler or build tools were installed. The deterministic provider, plain-text
latest-turn paging and shared main-looper disk operations remain explicit scope
limits. Both branches remain independent draft comparisons; neither is merged.
