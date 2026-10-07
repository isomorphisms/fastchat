# Historical live-state receipt — 2026-10-07

Fetched `isomorphisms/fastchat` main at start and again before publication:
`b49f0f7083d2d034ba76b910a43592d6d07bd18b`. Both transport branches start
independently from that exact base, with no merge/cherry-pick from the semantic
reference or from one another.

Before publication the live branches were `main`, `idric-conversation-core`,
`process-architecture-design`, `reference-audit-material3-framebuffer`,
`sun/android-material3`, `sun/disk-first-icky-c-lua` and
`sun/disk-streaming-icky-c-lua`. Neither requested transport branch already existed.

Open PRs were:

- [isomorphisms/fastchat PR #4, “Implement executable Idriç conversation core”](https://github.com/isomorphisms/fastchat/pull/4)
- [isomorphisms/fastchat PR #5, “FC-D1: record blocked disk-first Icky C/Lua preflight”](https://github.com/isomorphisms/fastchat/pull/5)
- [isomorphisms/fastchat PR #6, “FC-S1: record blocked streaming Icky C/Lua preflight”](https://github.com/isomorphisms/fastchat/pull/6)
- [isomorphisms/fastchat PR #7, “Add Compose Material 3 disk-first comparison baseline”](https://github.com/isomorphisms/fastchat/pull/7)

This is a dated observation, not a maintained live work queue. Those alternatives
are not dependencies. No unrelated branch is edited and no merge is requested.

The semantic head is `c66021957fb5dc195a995b2c0cd212863ff68915`. Immutable copied
blobs match exactly:

| Reference source | Git blob |
| --- | --- |
| `idric/FastChatCore.idric` | `f3b0d2a52c8cc5370fedff96652f155af2f9a929` |
| `idric/FastChatCoreTests.idric` | `eb9979329db34bf574f0f100f594696b3fc0454e` |

A1 profile/build-path inputs: Cat Food
`62ac940588f5ee4c5be468d48e38d74514da3757`, Android NDK repository
`7c61ee43e75f7c2dab9288edb0e10055898b36e6`, NDK r29
`29.0.14206865`, evaluated ICK snapshot
`7afb1820cd59c0c51d19a7e37902c14f4466f442`, and Idriç
`ff4d852862a3942592f8ade9afde8d409d9803be`.

Scope is MIRO A1. RP2040/ATtiny are architectural pressure only, with no build
or capability claims. Physical A1 installation, runtime, performance and native
Idriç FFI remain BLOCKED/NOT_RUN. Host protocol qualification, ARMv7 artifact
build and provider-account qualification are separate facts.
