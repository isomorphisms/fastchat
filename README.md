# FastChat

A small native Android interface for talking to AI agents and reading their
answers on inexpensive phones. Replies stream into a durable disk store;
reading uses a bounded window instead of keeping whole answers in memory.

**Test candidate. Production release is BLOCKED pending physical A1/C67,
live-provider and independent producer acceptance.**

- Platform keyboard, Unicode composer, selection and clipboard.
- Streaming replies, Stop and an explicit Retry for uncertain delivery.
- Durable conversation history and provider/model identity for each attempt.
- In-app HTTPS provider setup for Chat Completions or Responses requests.
- Private, atomic settings replacement and draft checkpoints across restart.
- NativeActivity with no application DEX, Java, Kotlin or Gradle.

Start with [installation and provider setup](INSTALL.md). The work continues in
[isomorphisms/fastchat PR #17, “Native FastChat: composer, provider setup, durable drafts and paired APKs”](https://github.com/isomorphisms/fastchat/pull/17).
Use only the signed artifact and source revision identified by its receipt.

## Hardware evidence

MIRO A1 is primary; MIRO C67 is its ARM64 companion. Hosted compilation and
packaging do not establish keyboard behavior, readability or handset speed.

| Measurement | Evidence |
| --- | --- |
| Previous ARM32 APK | 2,257,692 bytes at source `7add1607b43a3ecdfb8175de950955f5b7b22b11` |
| Current A1/C67 keyboard and reading | Physical acceptance NOT_RUN |
| Current handset RAM, latency and responsiveness | NOT_MEASURED |
| Current live-provider conversation and restart | NOT_RUN |

The [release checklist](notes/release-qualification.md) records the remaining
conditions. No phone performance claim is inferred from host benchmarks.

## Inspect the implementation

[Architecture](notes/process-architecture.md),
[durable streaming](notes/disk-path.md),
[attempt identity](notes/attempt-provider.md), and
[provider setup](notes/provider-setup.md).
The renderer supports a conservative Markdown subset; see the installation
notes for limits on prompts, context, event size and selection.

Compiler and runtime work:
[ICK](https://github.com/dilapidated-shed/ick),
[Idriç](https://github.com/isomorphisms/Idric),
[Icky Lua](https://github.com/isomorphisms/lua),
[Grease](https://github.com/dilapidated-shed/grease), and
[native Android packaging](https://github.com/isomorphisms/android-NDK).
The experimental disk-first, Compose and HTTP/3 comparisons remain available
on their existing branches.

[Project background and hardware motivation](notes/project-background.md).

**Full luxury communism now.**
