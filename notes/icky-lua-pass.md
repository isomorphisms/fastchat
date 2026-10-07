# C checkpoint → C + Icky Lua

The working functorial C checkpoints are preserved at:
- FC-D1: 7c79736673964be38bdaa85a74d9d5c590b93083.
- FC-S1: bfe347651ab7d7dd84650d9c58faa3387e8dbb70.

Both checkpoints execute the full host suite, build the complete native C slice
and produce signed APKs. Their exact artifacts and raw receipts remain in
artifacts/fastchat-c-pass.apk and qualification/fc-comparison/c-pass/.
Physical A1 execution was BLOCKED/NOT_RUN at that gate.

Only afterward, policy/composition moved into policy/composition.lua: stored-prefix
visibility, composer submit/retry/cancel selection, time-based durability batching
and fake fixture sequencing. Named closures use actual Icky Lua ←, ƒ, ≟, ≥ and ≤.
The siblings differ only in follows_stored_prefixes (and package/receipt identity).

The durable conversation/store, lifecycle admission, JSON/SSE framing, UTF-8 reader,
JNI presentation, bounded fixture expansion and file mechanisms stay in Icky C.
The Lua heap never receives streamed response bytes or renderer windows. It
receives bounded prompt/phase/timing metadata and holds constant fixture frames.
Icky C still uses real assignment
arrows and ordinary C pointer/dereference syntax. No glyph-to-stock-source layer
or Clang C compiler is involved.

isomorphisms/lua is pinned at 87306483cec50f8c750a22dda1d0742246fad756. Its actual
foreign onelua.c runtime is compiled through ICK with MAKE_LIB and LUA_USE_POSIX.
The POSIX configuration uses the runtime's maintained mkstemp path. Both Android
API macros are explicitly 26 so current Bionic guarded declarations and the ICK
availability checker agree. NDK headers remain unchanged.

The C bridge embeds the exact Lua UTF-8 bytes with assembler .incbin and loads
text through the real parser. One VM uses a tracked allocator capped at 131072
bytes and a 10000-instruction call limit. No standard libraries are opened.
Observed healthy policy heap peak is 12637 bytes. Invalid syntax, an infinite
policy, an oversized allocation and invalid results fail closed; no fallback
renderer policy is selected. Only explicit reinitialization recovers that VM.

The adapter owns two fixed 8192-byte framing buffers. A giant single wire offer
of 1053469 bytes decodes and stores 1048576 logical bytes; the measured maximum
active wire-plus-decoded occupancy is 8203 bytes. An oversized individual frame
is rejected. Disk writes and renderer windows remain capped at 4096 bytes.
There is no renderer queue; slow storage blocks or returns explicit backpressure.

The shared corpus verifies both sibling semantics and C-to-Lua preservation:
15 response/split stores and an 8 MiB response have byte-identical completed
journals/responses across both branches and both passes. Native fixtures additionally
run through the actual Lua sequencing/transport at hostile one-byte splits.

Current producer recipes:

    grease scripts/test-core.grease ICK_DRIVER ICK_LIBEXEC ICKY_LUA_SOURCE OUTPUT [host options...]
    OUTPUT
    grease scripts/build-native.grease ICK_ARM_DRIVER ICK_LIBEXEC NDK ICKY_LUA_SOURCE OUTPUT_DIRECTORY
    grease scripts/package-native.grease ANDROID_NDK_REPOSITORY NATIVE_LIBRARY OUTPUT_APK
    grease scripts/compare-branches.grease D1_EXECUTABLE S1_EXECUTABLE NEW_OUTPUT_DIRECTORY
    grease scripts/compare-passes.grease C_COMPARISON_DIRECTORY LUA_COMPARISON_DIRECTORY OUTPUT_RECEIPT

Native production builds require clean tracked source and bind the source revision.
Use the explicit stable signer profile in android/signing/README.md; versionCode 2
updates the C checkpoint's versionCode 1 while preserving package data. The
packager includes the Icky Lua MIT notice as an APK asset.

Use :long for the 8 MiB paced response, :lost for explicit uncertain delivery,
and :fail for explicit failure. Send retries an uncertain request with a fresh
attempt; the loss fixture's retry completes. Ordinary text uses the short fixture.

The maintained workflows check the shared ai-ci build-toolchain contract and
compile exact FastChat heads. Android production reuses the qualified ARM compiler
from ICK source 515c0f29fe6e2e96e10495fbaf25da93532e7722, run 37581942577, successful
ARM job 112663856334, artifact 11466225121. ZIP SHA-256:
d3d60223712a8f5119451e15344f9a22ace0824eca19f2e78792b0dc18ab509c.
Its artifact name contains workflow merge SHA 8257ccfbe6da364e1d4949b2e2b81e07a63e57cb;
the workflow explicitly checks out the PR head above. The temporary artifact
expires 2027-01-05; restore the same source build if it is no longer available.

Grease's implementation runtime provenance and alias boundary are in c-pass.md.
The Android workflow also verifies the runtime executable SHA-256. That temporary
runtime artifact must be restored from the pinned Grease source after expiry;
this is producer bootstrap work, never an on-phone source-build fallback.

Host RSS uses this executable's /proc/self/status VmHWM; ru_maxrss can retain a
launcher's pre-exec high-water mark. Steady RSS is /proc/self/statm. All three
controls initialize the same bounded policy VM. Replay has both in-process and
fresh-process receipts; filesystem cache state is unspecified, so these are not
cache-cold storage measurements. Times describe completed file reads, not physical
visible Android text. Native logs separately record Canvas posting, source identity,
RSS, writes, policy/framing bounds and restart time for later exact-device testing.

Cat Food main 62ac940588f5ee4c5be468d48e38d74514da3757 and the active A1 profile at
[7427a776a1fa689a6764478b572e270496768711](https://github.com/isomorphisms/catfood/blob/7427a776a1fa689a6764478b572e270496768711/android/devices/miro-a1.md)
were inspected. Cat Food owns device facts; this branch targets MIRO A1 only.
The API-26 application floor follows its Surface bridge, independently of the
profile's separate native-program floor. Physical handset identity and artifact
location must be observed before installation; no guessed download path is given.

Exact remaining acceptance: physical install/launch, JNI/Canvas layout,
EditText/IME and touch behavior, lifecycle and cold process replay, cancellation/
uncertainty, and long-response RSS/CPU/visible-answer timings on MIRO A1.
**BLOCKED/NOT_RUN**; no device execution was available. The current renderer is
plain text, uses bounded paging for the latest turn, and has no older-turn navigator.
The provider is deterministic; live-provider adaptation and credentials are not
storage-correctness prerequisites. No experiment or compiler draft is merged.
