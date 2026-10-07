# Source-bound delivery evidence

This receipt contains executed producer evidence for both comparison branches.
Disk-first source: `abecbc205cce5f76540b294ae505de42dcc75676`.
Streaming source: `89aba43a753ce492e44a3fc671a23e936f99ff2e`.
The local APK in this branch is [fastchat-icky-lua.apk](../../../artifacts/fastchat-icky-lua.apk);
[apk.receipt.tsv](apk.receipt.tsv) binds its source, digest, ABI and stable signer.
Both APKs are 94,751 bytes, versionCode 2, NativeActivity/API 26, with no application DEX.
The artifact-recording commit changes evidence only; it does not change compiled source.

Both actual ICK-built executables pass the deterministic suite. The shared
15-store corpus and 8 MiB response have identical completed bytes and canonical
journals across siblings and across the preserved C checkpoint and Lua pass.
Raw logs, executable digests, comparison receipts and fresh-process replay times
are retained here. The public source trees contain the exact build inputs.

| 8 MiB host control | Peak RSS KiB | Steady RSS KiB | First text ms | Terminal drain ms | Generation CPU ms |
|---|---:|---:|---:|---:|---:|
| Disk first | 1076 | 1032 | 89.844 | 90.710 | 89.627 |
| Streaming disk | 1076 | 1032 | 0.201 | 0.162 | 180.820 |
| RAM control | 9304 | 9256 | 3.437 | 90.249 | 3.351 |

These are file-reader timings on the disposable x86_64 producer, not Android
visible-frame timings. Each disk branch wrote 8,388,608 response bytes and
32,969 journal bytes in 2,563 writes. Maximum write and view were 4096 bytes.
The healthy policy heap peaked at 12,637 bytes under its 131,072-byte cap.
The giant wire-offer test measured 8203 active framing bytes inside two fixed
8192-byte buffers. There is no renderer queue or full-answer private copy.

Fresh-process replay took 86.321 ms (disk first) and 82.718 ms (streaming);
filesystem cache state was unspecified. Physical cold replay is not established.
See [gates.tsv](gates.tsv) for exact toolchain pins and acceptance boundaries.
Build recipes, policy responsibilities and storage invariants are described in
[the Lua pass](../../../notes/icky-lua-pass.md) and
[the disk path](../../../notes/disk-path.md).

Physical MIRO A1 installation, launch, IME/Canvas readability, touch/lifecycle,
long-response RSS/CPU and visible-answer/replay measurements are
**BLOCKED/NOT_RUN**. No phone compiler or build tools were installed.
Both branches remain independent draft comparisons; neither is merged.
