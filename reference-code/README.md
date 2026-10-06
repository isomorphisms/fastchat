# Reference code

Snapshot: 2026-10-06

FastChat's design is **not frozen**. Two or three examples are not enough evidence for the feature set, and UI source alone cannot establish the Android process/service boundary.

This directory keeps upstream implementations visible while FastChat is being designed for the MIRO A1. Reference projects are not build dependencies by default.

## Rules

1. Treat reference projects as evidence, not as the architecture.
2. Do not conclude that a process, service, permission, lifecycle hook, or feature is unnecessary until the physical Shizuku/runtime inventory and the relevant references have been checked.
3. Keep references link-only until there is a concrete reason to compile or modify them.
4. Before vendoring or copying code, inspect the license and record upstream provenance and an exact commit.
5. If a reference becomes an executable compatibility target, prefer a pinned submodule or an explicit fixture over a floating copy.
6. Large platform trees such as Chromium, AndroidX, AOSP, Shizuku, and Termux should remain links unless a very small source slice is deliberately captured.
7. The dated raw sweep in [sweep-2026-10-06.md](sweep-2026-10-06.md) intentionally retains weak and redundant candidates so later work can revisit them.
8. The original sweep is not globally exhaustive. Keep later discoveries in dated supplements rather than silently changing its receipt counts; see [supplement-2026-10-06.md](supplement-2026-10-06.md).
9. Renderer implementation evidence is tracked separately in [renderers.md](renderers.md), including the Material 3 product lane and the native-surface comparison lane.

## Read first: renderer choices

- [Renderer reference map](renderers.md) — FastChat-specific Material 3 versus native-surface architecture, internal executable references, text-rendering boundary, and required measurements.
- [Supplemental GitHub sweep](supplement-2026-10-06.md) — high-signal clients/session bridges missed by the original 302-repository sweep.

## Read first: native/mobile chat clients

- [HatsyRei/maid-native](https://github.com/HatsyRei/maid-native) — unusually valuable low-end reference: native Kotlin/Compose, OpenAI-compatible streaming, branching conversation trees, Room persistence, incremental Markdown, and a reported ~1.7 MB APK.
- [Mobile-Artificial-Intelligence/maid](https://github.com/Mobile-Artificial-Intelligence/maid) — original Maid implementation.
- [HatsyRei/maid](https://github.com/HatsyRei/maid) — React Native parity reference used by maid-native.
- [woheller69/gptAssist](https://github.com/woheller69/gptAssist) — deliberately small Android WebView wrapper for the actual ChatGPT site; useful counterexample to a full native reimplementation.
- [skydoves/chatgpt-android](https://github.com/skydoves/chatgpt-android) — established Compose sample; chat state, coroutines, WorkManager/background work, Stream integration.
- [rahulmasal/AetherisAI](https://github.com/rahulmasal/AetherisAI) — native Material 3, direct multi-provider routing, proper SSE framing, encrypted keys, local persistence.
- [FammasMaz/MaterialChat](https://github.com/FammasMaz/MaterialChat) — branching, search/export, voice, images, Android assistant role/overlay and floating bubble.
- [roseforljh/EveryTalk](https://github.com/roseforljh/EveryTalk) — native Android, multimodal input, voice, web search, direct/provider modes, explicit long-stream buffering and scroll control.
- [cogwheel0/conduit](https://github.com/cogwheel0/conduit) — mobile client for Open WebUI, direct APIs, and Hermes agents; background streaming and mobile lifecycle problems are central to the project.
- [Chevey339/kelivo](https://github.com/Chevey339/kelivo) — broad mobile/desktop LLM client; assistants, attachments, voice/TTS, MCP, web search.
- [garfiec/Librechat-Mobile](https://github.com/garfiec/Librechat-Mobile) — native Kotlin Multiplatform LibreChat client; branching/siblings, artifacts, agents, MCP, attachments, tool progress, large conversation-management surface.
- [GetStream/stream-chat-android-ai](https://github.com/GetStream/stream-chat-android-ai) — reusable Compose AI-chat components for streaming text, Markdown, composer state, attachments, stop/send state, and speech input.
- [AndraxDev/speak-gpt](https://github.com/AndraxDev/speak-gpt) — Android voice-assistant design and multimodal/provider surface.
- [mindylab/lmsmob_chat](https://github.com/mindylab/lmsmob_chat) — native Android controller for a remote local-model server, including MCP/server tools and scheduled watch jobs.
- [adebnar/hermes-android](https://github.com/adebnar/hermes-android) — native phone control surface for remote agents: sessions, models, scheduled jobs, usage, messaging and live agent activity.
- [rikkahub/rikkahub](https://github.com/rikkahub/rikkahub) — major native Android LLM client: Kotlin/Compose/Material You, Room, branching, multimodal input, tools/MCP and search.
- [jacob-ayang/rikkahub-armv7a](https://github.com/jacob-ayang/rikkahub-armv7a) — third-party ARMv7a build of RikkaHub; useful specifically as 32-bit Android evidence, not as an upstream authority.
- [MukheshKumarV/aria-android](https://github.com/MukheshKumarV/aria-android) — deliberately small single-screen Kotlin/Compose/Material 3 OpenAI client with speech input/output.
- [Vali-98/ChatterUI](https://github.com/Vali-98/ChatterUI) — mobile LLM frontend spanning remote APIs and on-device llama.cpp; useful for mobile lifecycle and local/native bridge behavior.
- [a-ghorbani/pocketpal-ai](https://github.com/a-ghorbani/pocketpal-ai) — substantial mobile llama.cpp client with Android performance/hardware work.
- [shubham0204/SmolChat-Android](https://github.com/shubham0204/SmolChat-Android) — compact Android GGUF/llama.cpp client.
- [dzianisv/opencode-mobile](https://github.com/dzianisv/opencode-mobile) — Android controller for remote coding-agent sessions.
- [allocsys/openhands-android-client](https://github.com/allocsys/openhands-android-client) — native Android controller for OpenHands using REST + WebSocket.

Other Android/mobile implementations worth keeping visible include [wieslawsoltes/ChatGPT](https://github.com/wieslawsoltes/ChatGPT), [Taewan-P/gpt_mobile](https://github.com/Taewan-P/gpt_mobile), [mardillu/OpenAI-Client-Android](https://github.com/mardillu/OpenAI-Client-Android), [danil0vah/ChatGPTAndroidClient](https://github.com/danil0vah/ChatGPTAndroidClient), [NNCVA/ChatPPP](https://github.com/NNCVA/ChatPPP), [tapir/chattoneko](https://github.com/tapir/chattoneko), [CodeNeow/MyLlama](https://github.com/CodeNeow/MyLlama), [xing133/OriginChat](https://github.com/xing133/OriginChat), [Shashank02051997/AnywhereGPT-Android](https://github.com/Shashank02051997/AnywhereGPT-Android), [simplifylabs/WearAI](https://github.com/simplifylabs/WearAI), [hiylo/starburst](https://github.com/hiylo/starburst), [ykai55/TinyChat](https://github.com/ykai55/TinyChat), [ethanchzhong/eChat](https://github.com/ethanchzhong/eChat), [PacifAIst/API2CHAT](https://github.com/PacifAIst/API2CHAT), [oriveo/oriveo](https://github.com/oriveo/oriveo), and [Minis233/miniichat](https://github.com/Minis233/miniichat).

## ChatGPT account/session/history adapters

These references are separate from ordinary OpenAI-compatible API clients. They matter if FastChat or IB needs to discover, import, search, or continue existing ChatGPT-account threads. Private-web-protocol dependencies must stay isolated behind an adapter because they can change without notice.

- [planetaryescape/chatgpt-cli](https://github.com/planetaryescape/chatgpt-cli) — indexes/searches/exports existing ChatGPT history from a logged-in browser session into SQLite.
- [DrA1ex/chatgpt-bridge](https://github.com/DrA1ex/chatgpt-bridge) — browser-extension bridge exposing a logged-in ChatGPT tab over local HTTP/SSE/JSON-RPC.
- [l0z4n0-a1/chatgpt-bridge](https://github.com/l0z4n0-a1/chatgpt-bridge) — compact localhost OpenAI-compatible bridge using ChatGPT OAuth/session tokens.
- [defcron/mirror](https://github.com/defcron/mirror) — ChatGPT mirror plus OpenAI-compatible API over the private web protocol, including conversation operations and streaming.
- [guyah/chatgptexporter](https://github.com/guyah/chatgptexporter) — browser extension for current/bulk ChatGPT conversation export with incremental/resumable bookkeeping.

## Web feature baselines

These are too large to imitate wholesale, but they expose mature feature sets and interaction semantics.

- [open-webui/open-webui](https://github.com/open-webui/open-webui)
- [LibreChat-AI/LibreChat](https://github.com/LibreChat-AI/LibreChat)
- [lobehub/lobehub](https://github.com/lobehub/lobehub)
- [ChatGPTNextWeb/NextChat](https://github.com/ChatGPTNextWeb/NextChat)
- [mckaywrigley/chatbot-ui](https://github.com/mckaywrigley/chatbot-ui)
- [huggingface/chat-ui](https://github.com/huggingface/chat-ui)
- [vercel/chatbot](https://github.com/vercel/chatbot)
- [assistant-ui/assistant-ui](https://github.com/assistant-ui/assistant-ui)
- [chatboxai/chatbox](https://github.com/chatboxai/chatbox)
- [lencx/ChatGPT](https://github.com/lencx/ChatGPT)
- [ChatGPTBox-dev/chatGPTBox](https://github.com/ChatGPTBox-dev/chatGPTBox)
- [iOfficeAI/AionUi](https://github.com/iOfficeAI/AionUi)

## Minimal / terminal controllers

These help separate the essential controller protocol from browser-sized UI assumptions.

- [sigoden/aichat](https://github.com/sigoden/aichat)
- [simonw/llm](https://github.com/simonw/llm)
- [TheR1D/shell_gpt](https://github.com/TheR1D/shell_gpt)
- [aandrew-me/tgpt](https://github.com/aandrew-me/tgpt)
- [Aider-AI/aider](https://github.com/Aider-AI/aider)
- [gptme/gptme](https://github.com/gptme/gptme)
- [charmbracelet/mods](https://github.com/charmbracelet/mods) — archived in 2026, still useful as a small pipeline-oriented reference.
- [charmbracelet/crush](https://github.com/charmbracelet/crush) — current successor direction for the non-interactive/agent side.

## WebView and hybrid references

- [woheller69/gptAssist](https://github.com/woheller69/gptAssist)
- [Duanzhoutao/chatgpt-webview-android](https://github.com/Duanzhoutao/chatgpt-webview-android)
- [phatal/WrapGPT](https://github.com/phatal/WrapGPT)
- [MrHuaweiFan/WebGPT](https://github.com/MrHuaweiFan/WebGPT)
- [Electric714/ChatGPT-WebView](https://github.com/Electric714/ChatGPT-WebView) — iOS/WKWebView, useful for lifecycle/cache comparisons.
- [VadimBoev/Android-JNI-WebView](https://github.com/VadimBoev/Android-JNI-WebView) — JNI/native-host WebView reference.

## Agent backends and remote-control surfaces

The phone can be a controller rather than the machine doing the expensive work.

- [anomalyco/opencode](https://github.com/anomalyco/opencode)
- [OpenHands/OpenHands](https://github.com/OpenHands/OpenHands)
- [aaif-goose/goose](https://github.com/aaif-goose/goose)
- [mindroom-ai/mindroom-chat](https://github.com/mindroom-ai/mindroom-chat)
- [pydantick/clawnest](https://github.com/pydantick/clawnest)
- [qingchencloud/clawapp](https://github.com/qingchencloud/clawapp)

## Android runtime / process references

These matter directly to the unresolved question: **what processes and services does a useful phone-side controller actually need?**

- [RikkaApps/Shizuku](https://github.com/RikkaApps/Shizuku) — Binder/system-API bridge and privileged process model.
- [RikkaApps/Shizuku-API](https://github.com/RikkaApps/Shizuku-API) — client API/examples.
- [termux/termux-app](https://github.com/termux/termux-app) — terminal/service/process model on Android.
- [termux/termux-api](https://github.com/termux/termux-api) — Android API bridge from Termux.
- [androidx/androidx](https://github.com/androidx/androidx) — WebKit, Activity, lifecycle, WorkManager and other Android support implementation.
- [chromium/chromium](https://github.com/chromium/chromium) — renderer/network/GPU process behavior behind Chromium/WebView; link only, never vendor wholesale.
- [aosp-mirror/platform_frameworks_base](https://github.com/aosp-mirror/platform_frameworks_base) — Android framework process/service behavior; mirror is archived but remains useful source history.

## Internal references

FastChat also has to fit the existing local architecture rather than becoming an isolated clone:

- [isomorphisms/ib](https://github.com/isomorphisms/ib) — threads/skeins and alternative front ends.
- [isomorphisms/catfood](https://github.com/isomorphisms/catfood) — supported-device build/deployment facts.
- [dilapidated-shed/ick](https://github.com/dilapidated-shed/ick) — compiler/toolchain work.
- [fuego-ironworks/idric-arm-thumb](https://github.com/fuego-ironworks/idric-arm-thumb) — ARM/Thumb compiler-backend experiments and physical-phone work.

## Feature/process axes to extract before freezing FastChat

Reference review should produce explicit decisions for at least:

- thread/conversation identity, branches, sibling responses, edit/regenerate/fork, search, archive and export;
- streaming protocol, cancellation, backpressure, incremental Markdown/code rendering, very long answers and scroll behavior;
- background/foreground transitions and whether active generation survives switching apps;
- text, voice/STT, TTS, images, files, clipboard, share intents and camera/document pickers;
- tool calls, approvals, progress/events, agent activity, scheduled work and remote-agent control;
- authentication and transport: ChatGPT web session, OpenAI-compatible API, LibreChat/Open WebUI-style server, local/LAN endpoint, or custom agent gateway;
- local persistence, offline behavior, retries and recovery after process death;
- Android Activity/Service/WebView renderer/network/media/process boundaries;
- notifications, foreground-service requirements and system-assistant integration;
- Shizuku/Termux bridges where they actually provide value;
- APK size, RSS, process count, cold start, CPU, disk I/O and network traffic on the MIRO A1.

The physical Shizuku inventory is still an input to this list, not a task that can be replaced by reading repositories.
