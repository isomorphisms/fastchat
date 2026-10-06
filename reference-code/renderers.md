# FastChat renderer reference map

Snapshot: 2026-10-06

FastChat should keep the conversation/controller core independent of its Android renderer. The current product priority is a Material 3 Android UI. A second, deliberately minimal native renderer should remain buildable as an experiment so APK size, memory, CPU, thermal behavior and interaction latency can be measured on the cheap-phone targets instead of guessed.

## Ownership

- `isomorphisms/fastchat` owns the application renderer choice and the comparison between renderer lanes.
- `isomorphisms/android-NDK` owns generic direct-DEX/JNI/NativeActivity/package boundaries. Its root README explicitly says it does **not** own application rendering choices.
- `Ashtray-Archer/utilities-android-phone-user` currently contains the strongest executable internal renderer precedents.
- `isomorphisms/catfood` owns supported-device build/deployment facts and physical acceptance evidence.

## Lane A — Material 3 product renderer

"Material 3" must be used precisely.

### Compose Material 3

The current future-facing Android Material implementation is `androidx.compose.material3`. The pinned API reference already captured for our Android work is:

- `Ashtray-Archer/utilities-android-phone-user`, branch `material3-api-reference`
- `material3-api/README.md`
- `material3-api/INDEX.md`
- 43 upstream API/ABI signature files
- pinned AndroidX source commit `cdeafe5b4e450d61cdb623d58c148efbb0d508cf`

That snapshot is reference material, not a declaration that FastChat must use every Compose layer.

High-value chat implementations to inspect before freezing FastChat's Material 3 screen:

- https://github.com/HatsyRei/maid-native — small native Android chat client; Compose/Material 3; streaming, Room, branching and incremental Markdown.
- https://github.com/rikkahub/rikkahub — large mature native Android LLM client; Kotlin/Compose/Material You; branching, attachments, tools/MCP, search and Room.
- https://github.com/Minis233/miniichat — compact Compose + Material 3 OpenAI-compatible client.
- https://github.com/FammasMaz/MaterialChat — Material 3 Expressive chat client with streaming, persistence and branching.
- https://github.com/Taewan-P/gpt_mobile — Kotlin/Compose/Material You client with local history and multiple providers.
- https://github.com/MukheshKumarV/aria-android — intentionally small single-screen Kotlin/Compose/Material 3 chat/speech client.
- https://github.com/GetStream/stream-chat-android-ai — reusable Android AI-chat components, especially composer and streaming state.

### Views Material components

https://github.com/material-components/material-components-android remains a useful implementation and size/performance comparator for Material 3 themes/components without Compose, but upstream entered maintenance mode in 2026 as Android/Material moved Compose-first. Do not confuse `android:style/Theme.Material` with Material 3.

If FastChat experiments with a Views implementation, call it an MDC/Views lane and measure it independently rather than describing a platform `Theme.Material` app as Material 3.

## Lane B — native pixel/surface renderer

For an ordinary Android application, "framebuffer renderer" here means application-owned pixels presented through an Android surface. It does **not** mean that FastChat should try to own `/dev/fb0` on Android 14.

Two existing internal implementations are directly reusable as references:

### Raw `ANativeWindow_Buffer` pixels

`Ashtray-Archer/utilities-android-phone-user/accelerometer/app/src/main/c/native_main.c`

This code already contains:

- `ANativeWindow_Buffer`
- explicit `put_pixel` / pixel blending
- software glyph construction/sampling
- NativeActivity lifecycle integration

This is the closest existing code reference for a genuinely tiny software renderer.

### Native state/layout with Android Canvas/Paint text

`Ashtray-Archer/utilities-android-phone-user/math-characters/app/src/main/c/native_main.c`

This code already demonstrates:

- NativeActivity + C-owned application state
- `ANativeWindow` dimensions/content rectangle handling
- JNI attachment from native code
- Android `Canvas` / `Paint` text rendering from native orchestration
- touch/editing/clipboard integration in the same application family

For a text-heavy chat client this is probably the more valuable first native-renderer experiment: keep layout/state and most rendering orchestration small/native while borrowing Android's mature font rasterization instead of immediately writing a shaping engine.

## Shared core boundary

The renderer experiment is only meaningful if both lanes consume the same application model.

```text
conversation/session core
  |- thread graph and sibling/branch identity
  |- streamed response events and cancellation
  |- local persistence / append-replay
  |- attachments and tool events
  |- markdown/block parse model
  |- draft/input state
  |- authentication / ChatGPT or API adapter
  |
  +-- Material 3 renderer
  |
  +-- native surface renderer
```

Do not fork protocol, persistence or conversation semantics merely to make the native UI easier.

## Text is the hard boundary

A pixel renderer can make rectangles and scrolling cheaply. A usable chat client still needs:

- Unicode shaping and fallback
- line breaking and measurement
- selection/copy
- cursor placement
- IME composition
- bidirectional text
- accessibility behavior

Therefore the first native experiment should reuse Android text services where practical. A fully native shaping/rasterization stack is a separate experiment and should only be justified by measured benefit.

## Required comparison

Record the following on the same source/core revision and same physical device:

| Metric | Material 3 | Native surface |
| --- | ---: | ---: |
| APK bytes | measure | measure |
| cold launch to usable composer | measure | measure |
| idle RSS | measure | measure |
| RSS with long thread | measure | measure |
| CPU while streaming | measure | measure |
| CPU while scrolling | measure | measure |
| dropped frames / jank | measure | measure |
| idle thermal/battery behavior | measure | measure |
| IME/selection correctness | qualify | qualify |
| accessibility | qualify | qualify |

Multiple visible chats should also be tested in one process before assuming multiple processes are required.

## Build boundary references

The generic NativeActivity packaging lane belongs in:

- https://github.com/isomorphisms/android-NDK
- `apk/build-nativeactivity-apk.ysh`
- `ARCHITECTURE.md`
- `apk/README.md`

That work is a packaging/runtime boundary, not evidence that FastChat should select a native renderer.
