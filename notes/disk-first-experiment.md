# FC-D1 — disk-first comparison

Independent branch sun/disk-first-icky-c-lua; exact fetched main base
b49f0f7083d2d034ba76b910a43592d6d07bd18b. Preserve this experiment unmerged.

Provider chunks go directly to the authoritative file store. The renderer receives no response body until a completed immutable response is committed.
Failure, cancellation and uncertainty remain explicit. The RAM sink is only a
test/control. Request and attempt identities stay separate.

Both passes are implemented and verified on the producer. The complete Icky C
checkpoint is preserved with its APK and receipts, followed by the actual Icky
Lua composition refactor. See [disk architecture](disk-path.md),
[C checkpoint](c-pass.md), [Lua pass and recipe](icky-lua-pass.md), and
[exact comparison receipts](../qualification/fc-comparison/).

The sibling uses the same core, store, fake SSE/JSON transport, batching and
Canvas/Paint/EditText presentation. Only stored-prefix visibility and package/
receipt identity differ. The response corpus is shared in tests/conversation.c;
scripts/compare-branches.grease compares independently built sibling executables,
and scripts/compare-passes.grease verifies preservation of the C checkpoint.

Signed APKs use the existing android-NDK NativeActivity packager and stable
public development signer. Physical MIRO A1 install, presentation/IME, lifecycle,
RSS/CPU and visible-text measurements remain **BLOCKED/NOT_RUN**.

