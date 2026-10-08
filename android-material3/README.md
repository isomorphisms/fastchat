# Compose Material 3 comparison baseline

This independent renderer lane implements Sun M3 for **MIRO A1 only**.
It uses actual `androidx.compose.material3` composables. The platform Activity
theme only supplies a launch window; it is not the Material 3 implementation.

Base: `isomorphisms/fastchat@b49f0f7083d2d034ba76b910a43592d6d07bd18b`.
Semantic reference: PR #4, “Implement executable Idriç conversation core”,
`c66021957fb5dc195a995b2c0cd212863ff68915`.

## Implemented boundary

`UserCommand → core → durable journal → Transport → canonical events → durable journal → view projection → Compose`

The Kotlin `core` module has no Android, Compose, network, or coroutine dependency.
It adapts the current Idriç semantic model into the renderer lane's host language;
it does not execute the Idriç compiler/backend on Android. The five PR #4 fixture
behaviors are ported, including interleaved requests and independent view cursors.
The UI never owns request admission, retry identity, event ordering, or persistence.

`ChatController` serializes commands on an application-scoped worker. It survives
Activity recreation and having no visible renderer. Android may still kill the
process; replay then durably records uncertainty for any active attempt. No
foreground service or claim of guaranteed background execution is introduced.

The default projection hides all response chunks until a completion event is
durable. Stop, failure, and uncertain delivery do not reveal incomplete text.
The same core/journal can project stored streaming chunks by selecting
`ProjectionMode.StoredStreaming`; the screen accepts ordinary message updates
without needing another transport or storage design. That mode is not enabled
in this first comparison APK.

The deterministic provider needs no credentials and no network permission:
ordinary prompts complete, `/fail` fails, and `/uncertain` loses delivery state.
Retry is an explicit user action with a fresh attempt ID; `/uncertain` remains a
deliberate loss fixture on retry. `Transport` is the adapter boundary for future
real providers. No live HTTP adapter was available at this base, and none is
duplicated in the UI.

## Journal semantics

Ordinary app-private files, no appendFAT. One exclusive file-lock owner, versioned
length/CRC frames, strict UTF-8, request/attempt/conversation/authority identities,
and monotone sequence numbers. A command's entire event batch occupies one frame.
Its bytes and file metadata are synced before state publication or transport
submission. Creation also syncs the parent directory through Android's `Os` API.

The logical address is `AppendAddress(conversation stream, byte offset)`.
`committed ≤ durable ≤ written ≤ capacity` is represented explicitly. Capacity
is the ordinary-file address bound, not reserved free disk space; write failures
remain possible. On a failed write/sync the owner fails closed, attempts rollback,
and requires reopening. A crash can leave a whole valid but unacknowledged frame;
replay treats active requests as uncertain and never automatically resends them.

Only an incomplete final frame is truncated. Invalid magic/length, CRC mismatch,
identity mismatch, invalid UTF-8, or invalid canonical sequence/phase is an error;
the store does not erase that history to get a successful startup.

## Text and lifecycle

Platform Compose text layout/input supplies shaping, bidirectional text, fonts,
IME, cursor, and selection. `TextFieldValue` preserves editing selection and
composition; updates are not trimmed or asynchronously filtered. The saveable
draft is cleared only after durable send acknowledgement. Manual selection and a
whole-message copy action are available. Headings, paragraphs, and fenced code
are rendered; other Markdown stays readable verbatim. This is not a full CommonMark
implementation, syntax highlighter, or custom text shaper.

Lazy rows have stable request/role keys. Dragging the list stops automatic following;
“Latest messages” resumes it. Saving/recreating the screen retains draft and list
position. Canonical conversation state comes from the journal, not saved UI bundles.

## Build cost and scope exception

This task explicitly requests the conventional Compose comparison, including
recording Gradle/AGP/Kotlin cost. It therefore authorizes this lane's Kotlin → JVM
→ DEX/ART path as an isolated comparison exception to the usual ICK/NDK producer
policy. It is not an ICK build, not an NDK-built native renderer, and not a change
to Cat Food's general toolchain policy. The existing direct NativeActivity
packager cannot package this Compose implementation without replacing its purpose.

Pinned host tools: JDK 17, Gradle 8.9, AGP 8.5.2, Kotlin/Compose compiler 2.0.20,
Compose BOM 2024.09.02, Android SDK 34/build-tools 34.0.0. No NDK, Room, WebView,
HTTP client, Markdown framework, or model runtime is added. Dependency resolution
and checksum verification are recorded with the build. Builds happen off-device.

Use the standard Gradle wrapper from a verified build host, specifying this directory
with `-p` if the working directory differs. The build targets are `:core:test`,
`:app:testDebugUnitTest`, `:app:assembleDebugAndroidTest`, and `:app:verifyM3Apk`.
`verifyM3Apk` checks the finished package/version/SDK/ABI/signer and writes a digest
receipt. The public stable development signer is documented in [signing/](signing/).

## Evidence and comparison

See [qualification.md](qualification.md), [references.tsv](references.tsv),
[comparison.tsv](comparison.tsv), and [fixtures.md](fixtures.md).
Host tests and packaged APK bytes never count as physical MIRO A1 acceptance.
No C67 build or acceptance is part of this job. This branch remains a draft
alternative and must not merge merely because its host CI succeeds.
