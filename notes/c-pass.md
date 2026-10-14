# Completed functorial Icky C checkpoint

The first pass works on the producer: both C branches execute deterministic
conversation, store, transport, fault and replay tests, and produce byte-identical
completed journals/responses. The whole native presentation/composer slice also
compiles with ICK and links with the NDK platform boundary. Signed NativeActivity
packaging has passed. Physical UI and A1 execution are BLOCKED/NOT_RUN.

This checkpoint precedes all Lua refactoring. The C checkpoint commit is preserved
in each branch's history; later receipts identify it exactly. Source uses actual
ICK ← assignment and ordinary C pointer syntax, without a glyph translator.

Main base: b49f0f7083d2d034ba76b910a43592d6d07bd18b.
Qualified ICK: 515c0f29fe6e2e96e10495fbaf25da93532e7722.
Immutable GCC source: 6294f1d9e7536e5ffcde09d1528c918d63abfef5.

See disk-path.md for ordering, bounds and races, and
../qualification/fc-comparison/c-pass/ for raw executed comparison results.
Earlier NOT_RUN compiler notes under qualification/icky-assignment and
android-application-boundary are historical. They are superseded for the focused
C application compile gate by this receipt, not deleted or promoted into A1
acceptance. The former bootstrap/flex and annotation blockers were resolved.

Producer recipes (the consumer command is Grease):

    grease scripts/test-core.grease ICK_DRIVER ICK_LIBEXEC OUTPUT [host link options...]
    OUTPUT
    grease scripts/build-native.grease ICK_ARM_DRIVER ICK_LIBEXEC NDK OUTPUT_DIRECTORY
    grease scripts/package-native.grease ANDROID_NDK_REPOSITORY NATIVE_LIBRARY OUTPUT_APK
    grease scripts/compare-branches.grease D1_EXECUTABLE S1_EXECUTABLE NEW_OUTPUT_DIRECTORY

Both actual branch executables are required for comparison. A renderer-policy
toggle on one executable is not sibling-branch evidence. APK packaging requires
explicit SDK, package/version, API and stable signer environment, as documented
by the pinned android-NDK packager and android/signing/README.md. The application
API floor is 26. Cat Food remains the authority for MIRO A1 facts.

Current Grease implementation qualification used the actual pinned fork runtime
from ba869518c7d850de6c47d8c6234654575e264e6c, artifact 11419719229 from run
37478624499. That run's alias diagnostics were incomplete; it is not a whole
Grease-suite pass. Its inherited reference executable was invoked only inside
this implementation boundary because the consumer alias is not implemented.
ASAN_OPTIONS=detect_leaks=0 was required by the ptraced executor; this is not a
source-language fallback.

Host 8 MiB fixture: process-image VmHWM/steady RSS, actual file I/O and bounded
4096-byte reads. File-read times are not Android visible-frame times. All disk
branches wrote 8,388,608 data bytes, 32,969 journal bytes, in 2,563 writes; maximum
write/view were 4096 bytes. Fixed transport buffers total 16 KiB. No renderer queue.
See raw TSVs rather than inferring phone measurements from this host.

Remaining physical gate: install/launch, Canvas/Paint readability, EditText/IME,
touch paging/cancellation, lifecycle, cold replay, RSS, CPU and visible-frame
timing on MIRO A1. Exact source/artifact must accompany that future receipt.
No phone compiler or build tools were installed. No experiment is merged.

