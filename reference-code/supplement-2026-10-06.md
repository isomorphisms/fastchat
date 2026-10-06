# GitHub reference sweep supplement — 2026-10-06

This supplements, rather than rewrites, `sweep-2026-10-06.md`. The original 302-repository file is a dated receipt of its original search queries. A later audit found several high-signal repositories those queries did not return.

Inclusion is discovery evidence, not endorsement, maintenance status, license approval or a decision to vendor code.

## Why the original sweep was insufficient

The original query set was strong on generic Android ChatGPT/LLM clients but weak on:

- mature Android LLM clients whose repository descriptions do not use the exact original search vocabulary;
- ARMv7-specific Android forks;
- ChatGPT-account/session/history bridges;
- conversation exporters using ChatGPT's private web endpoints;
- rendering-system references relevant to the Material 3 versus tiny-native experiment.

No finite GitHub search can prove global exhaustiveness. The goal is sufficient coverage of implementation families plus reproducible search receipts.

## Additional native/mobile references

- https://github.com/rikkahub/rikkahub — major native Android LLM client; Kotlin, Compose, Material You, Room, branching, multimodal input, tools/MCP and search.
- https://github.com/jacob-ayang/rikkahub-armv7a — third-party ARMv7a RikkaHub build; especially relevant to cheap 32-bit ARM Android qualification.
- https://github.com/MukheshKumarV/aria-android — small single-screen native Android OpenAI client using Kotlin, Compose and Material 3.
- https://github.com/Vali-98/ChatterUI — mobile LLM frontend with remote APIs and on-device llama.cpp through a React Native bridge.
- https://github.com/a-ghorbani/pocketpal-ai — mobile local-LLM client using llama.cpp/React Native with substantial Android lifecycle/performance work.
- https://github.com/shubham0204/SmolChat-Android — Android on-device GGUF/llama.cpp chat reference.
- https://github.com/egorpariy/gpt-mobile — Kotlin/Compose/Material 3 multi-provider chat client.
- https://github.com/ekam-labs/ekm_android — Kotlin/Compose Material You OpenAI-compatible/Gemini client.

## ChatGPT account/session/history references

These are architecturally distinct from ordinary OpenAI-compatible API clients. They are valuable for understanding ingestion or continuation of existing ChatGPT account threads. They depend on private or browser-facing ChatGPT behavior unless explicitly documented otherwise and therefore require isolation behind an adapter.

- https://github.com/planetaryescape/chatgpt-cli — indexes/searches/exports existing ChatGPT history from a logged-in browser session into SQLite; explicitly documents private web API fragility.
- https://github.com/DrA1ex/chatgpt-bridge — browser-extension bridge exposing a logged-in ChatGPT tab over local HTTP/SSE/JSON-RPC.
- https://github.com/l0z4n0-a1/chatgpt-bridge — compact localhost OpenAI-compatible bridge using ChatGPT OAuth/session tokens.
- https://github.com/defcron/mirror — self-hosted ChatGPT mirror plus OpenAI-compatible API over ChatGPT's private web protocol; conversation operations and SSE are useful protocol references.
- https://github.com/guyah/chatgptexporter — browser extension exporting current/all ChatGPT conversations via internal conversation endpoints with resumable bulk export.
- https://github.com/Niek/chatgpt-web — simple direct OpenAI-compatible frontend; not an Android target, but useful as a minimal feature/control baseline.

## Material / renderer references missed by the original chat-client query set

- https://github.com/material-components/material-components-android — Views-based Material Components implementation; entered maintenance mode in 2026 but remains useful as a non-Compose Material 3 comparator.
- https://github.com/androidx/androidx — Compose Material 3 source remains the primary upstream API/implementation reference.
- internal executable renderer references are catalogued in `renderers.md`.

## Supplemental search vocabulary

Future refreshes should include query families equivalent to:

- `android llm client material you compose`
- `android llm frontend llama.cpp`
- `android openai compatible material 3 compose`
- `chatgpt browser session conversation history`
- `chatgpt conversation exporter internal api`
- `chatgpt bridge browser extension local api`
- `chatgpt oauth local openai compatible bridge`
- `armv7 android llm chat client`

Search results change with time; preserve future refreshes as new dated receipts rather than silently rewriting old ones.
