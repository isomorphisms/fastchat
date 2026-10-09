# Retried Icky C application boundary

Status: **BLOCKED**, after assignment-arrow repair. This is a prerequisite
qualification, not a FastChat implementation or completed C pass.

The unchanged FastChat arrow probe passes with the owned rebuilt compiler from
[dilapidated-shed/ick PR #77, “Accept ← assignment in the Icky C frontend”](https://github.com/dilapidated-shed/ick/pull/77).
Compiler source: `2a27ad6ab4e4601c9a0e4a5fa7915712db707af0`.
Its current descendant `326366fffbbcaa8b23fa0abe4f58d1c2b3c07420` has no
differences in `ick/` or `.gitmodules`. ICK main remains
`e3c2a40b4edafc4d9caca55d1f7c094e6aab9589`; the fix was explicitly selected,
not assumed merged.

## What ran

| Probe | Result | Evidence scope |
| --- | --- | --- |
| Existing unchanged `←` assignment fixture | PASS, exit 0 | owned ICK frontend syntax |
| `host-files-probe.c` | PASS, exit 0 | Linux/AArch64 QEMU; open, bounded short-write loop, fsync, close, reopen, exact UTF-8 bytes |
| `nullability-probe.c` | FAIL, exit 1 | frontend does not parse `_Nonnull` pointer annotation |
| `availability-probe.c` | FAIL, exit 1 | `introduced`/`strict` not accepted; availability attribute ignored |
| `application-probe.c` with unmodified NDK r29 headers | FAIL, exit 1 | file + NativeActivity/window declarations cannot compile |

The executable host witness uses named functions, descriptive names and `←`
assignment. It writes through an ordinary Linux file and reads only after the
durability barrier/close. It proves those prerequisites can be composed with
the fixed compiler. It does **not** implement canonical event admission,
request/attempt identity, crash recovery, a sink interface, a renderer, or
any of the requested application tests. No host benchmark was collected.

The real Android probe first fails in
`bits/get_device_api_level_inlines.h` on `_Nonnull`. The full diagnostic also
rejects `_Nullable` and a trailing attribute on the inline definition of
`ANativeWindow_clearFrameRate` in `android/native_window.h:375`.
The isolated availability probe independently preserves that known frontend
gap. Header paths are explicit; the failures are declaration/parser support,
not a missing include search path. No annotations/attributes were removed and
no consumer code was compiled by Clang.

## Target/evidence boundary

The available fixed compiler targets `aarch64-linux-gnu`. The real-header
probe deliberately uses matching AArch64 NDK headers and performs only syntax
checking. This diagnoses shared frontend requirements; it is **not** an A1
ARM32 compiler, object, link, runtime, performance or acceptance receipt.
The isolated annotation probes are independent of the target ABI.
MIRO A1 remains the sole application target; no AArch64 device lane was added.

NDK: r29, `29.0.14206865`. API policy supplied to the declaration probe: 24.
Cat Food device/build authority inspected:
`isomorphisms/catfood@62ac940588f5ee4c5be468d48e38d74514da3757`.
NativeActivity/package boundary inspected:
`isomorphisms/android-NDK@7c61ee43e75f7c2dab9288edb0e10055898b36e6`.
No hardware facts are duplicated here.
FastChat main/base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b`.
Semantic reference:
[isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4),
`c66021957fb5dc195a995b2c0cd212863ff68915`; its source/tests and empty
discussion were inspected. The sibling/reference need not be merged.

## Next required boundary

ICK must support the Bionic nullability declarations and Android API
availability semantics, including the inline-definition spelling. A compiler
repair must then pass these preserved fixtures and the actual unmodified
headers. Afterward the ARM32/Bionic compile, link, whole C vertical slice,
requested deterministic storage/semantic tests and APK still need execution.
Keep the two-pass order: the Lua refactor starts after that C pass works.

Application implementation/tests, C→Lua refactor, RAM/disk/streaming comparison,
A1 native artifact/APK and physical A1 execution: **BLOCKED/NOT_RUN**.
The independent comparison drafts remain unmerged. This receipt is shared
unchanged between them because their compiler prerequisite is identical.

Full commands and diagnostics are retained alongside the receipt. The host
executable is a transient diagnostic; its digest is recorded, and it is not
packaged or published as an Android artifact.
