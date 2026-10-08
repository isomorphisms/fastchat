# Material 3 qualification boundary

## Target and source

- M3 base: `b49f0f7083d2d034ba76b910a43592d6d07bd18b` (current main when fetched).
- Idriç semantics: `c66021957fb5dc195a995b2c0cd212863ff68915` from
  [isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4).
- Device-fact authority: `isomorphisms/catfood@62ac940588f5ee4c5be468d48e38d74514da3757`.
  `phone` is the armv7 Android runtime target. The particular physical A1
  instance, fingerprint, keyboard, refresh rate, and installed package are
  unresolved here. This job neither selects nor qualifies C67.
- Native FC-D1/FC-S1 branches were absent from fetched remote refs at the start;
  a later fetch found their blocked design/corpus branches at
  `66d4c1d202a3f2b3764e35123b51e1e2a7cbaca1` and
  `600cf446fc3cc6a1311e7493e422df0de11252af`. The six-row byte corpus is now
  imported and tested here. No runnable native renderer/artifact is available.
  PR #4 is the shared semantic baseline, not a merge dependency.

The disk-first completion/visibility/admission boundary matches the native
contract. The current M3 store journals canonical text directly and its replay
state retains chunks in managed memory; it does not yet implement the proposed
native response byte-stream/reserved-capacity adapter or bounded viewport body
reads. Capacity here is an address bound, not preallocated backing capacity.
Record these store/core differences alongside renderer/runtime measurements;
the initial table is not proof that any size/RSS difference comes from Compose
alone. A shared store adapter or a controlled fixed-state renderer comparison is
needed to isolate renderer cost after the native implementation exists.

## Evidence to collect

Local core/storage/controller suites passed before publication; the workflow
records exact-head results and exports its artifact receipt. The instrumented
test APK compiles but has not executed. See [build-cost.tsv](build-cost.tsv) for
host tool/dependency costs. Runtime numbers remain unmeasured.

Host source/core tests, controller tests, APK packaging and instrumented-test
compilation are separate stages. Instrumented tests need execution receipts;
compilation alone is insufficient. The CI artifact receipt binds the exact head,
APK SHA-256, package/version, signer and native ABI entries.

Do the following on the **same identified physical MIRO A1** for every renderer
lane. Keep source revision, corpus revision, APK digest, device alias/fingerprint,
IME/version, and Android build in the receipt.

1. Install the exact APK; later exercise replacement install with the same signer
   and nondecreasing version code. Preserve existing app data. Never uninstall to
   avoid a signature conflict.
2. Cold-launch repeatedly to a focused/usable composer. Report sample count,
   individual timings, and median rather than one best run.
3. Measure idle RSS and RSS after the 500-request long-thread fixture. Use actual
   A1 process memory; distinguish RSS from Android PSS if collecting both.
4. Send ordinary Unicode prompts, `/fail`, and `/uncertain`; stop an active request.
   Verify no incomplete response text appears in disk-first mode, no automatic
   resend follows uncertainty, and explicit retry uses a new attempt.
5. Type combining accents, CJK input, Arabic/RTL, emoji with modifiers/ZWJ and the
   symbols in `Fixtures.unicode`. Exercise live IME composition, middle-of-text
   edits, cursor motion, select/copy/paste and long multiline drafts. Verify copied
   bytes/text against the source corpus. Programmatic text tests do not prove a
   real keyboard's composing behavior.
6. Rotate/recreate while typing and generating. Preserve draft/cursor, scroll
   position and one request lifetime. Scroll upward during generation; updates
   must not pull the reader away. Exercise code horizontal scroll and selection.
7. End the application process after completed responses, then relaunch and time
   replay. Repeat during an active request: it must become uncertain, retain the
   user prompt, hide partial response text, and not restart transport by itself.
8. Collect actual CPU and frame/jank evidence for response updates and a fixed
   scrolling interval. Bind it to the device refresh rate and collection method.

No physical timings, memory, input/IME, frame, install, or launch result is inferred
from desktop/emulator output. `comparison.tsv` keeps those rows `NOT_RUN` until
matching receipts exist.

## Exact remaining boundaries

- Physical MIRO A1 install/launch, replacement install, IME/cursor/selection,
  rotation/process-death behavior and runtime performance measurements.
- Instrumented screen tests have to execute on Android; host compilation is only
  a prerequisite. A host controller test demonstrates view-independent lifetime,
  and a real JVM child-process exit demonstrates host replay, not Android behavior.
- The native Icky C/Lua alternatives must supply exact comparable artifacts and
  run this same corpus before any renderer winner is claimed.
- Live transports remain adapters behind `Transport`; this deterministic baseline
  has no live provider, credentials, HTTP client, or INTERNET permission.
- First Cat Food delivery registration remains dependent on a published,
  qualified artifact. M3 does not weaken Cat Food's general producer policy or
  claim Cat Food installation. No independent repository policy change is hidden
  in this branch.

See [references.tsv](references.tsv) for exact inspected upstream commits and
license decisions. No application source was copied from these references.
The Gradle wrapper is generated unchanged by Gradle 8.9, Apache-2.0, as the narrow
upstream build-system entrypoint. First-party logic is Kotlin; no shell helper or
Python program is introduced.
