# FC-U1 native convergence candidate

This isolated candidate converges the disk-streaming/shared-arena application,
the current executable Idriç core, and bounded HTTP/2. It is a hosted,
public-test-signed candidate for later physical acceptance, not a production
account, release, shared-producer authorization or device acceptance claim.

## Exact inputs and decision

| Input | Exact fetched source |
| --- | --- |
| FastChat main | b49f0f7083d2d034ba76b910a43592d6d07bd18b |
| PR #4 core | c50ca1a1e3aedd873d9e17e31c7ea0ce54782e28 |
| PR #5 disk-first reference | 7e2129cf4021af68fe69726e9e913751393e6a94 |
| PR #6 disk-streaming | d09978739c54858adf8a619ed4ba02cf312863ce |
| PR #10 shared arena/base | 4b9eb2cc7e3a0fd6e6dd074e5a8efe9f1d2b6253 |
| PR #8 HTTP/2 | ae920025c1ef8b8262f2003c59217cb69955c4d4 |
| PR #7 Compose comparison only | c5fc80ea3350186b4af4960de1fb236acdc6b854 |
| PR #9 H3 comparison only | 94068b5aefa5ba61ccb9dce6f0577e0a5f9dbd0b |

Before selecting the arena, pristine PR #5 and PR #10 were compiled through
the same ICK host compiler and actual Icky Lua. Both returned the exact 8 MiB
body CRC `31e202a1`, used 4096-byte reads/writes, and passed fresh-process replay.
Reference arena peak RSS was 1312 KiB versus disk-first 1076 KiB; fresh replay
was 79.643 versus 88.589 ms. These single host samples are comparisons, not
physical predictions. The original arena's 8 MiB reservation traffic was
omitted from its reported write counters, and its allocator allocated a 256 KiB
temporary buffer. A bounded zero-fill replacement was measured and rejected:
it raised application writes from 2563 to 4612. The candidate instead uses
`posix_fallocate` like disk-first, with a metered 4096-byte fallback only when
kernel allocation is unsupported. Allocation calls/bytes are reported separately
from application writes. A fresh 8 MiB sample used 1072 KiB peak RSS, 2564 writes
(one extra canonical attempt record), 4096-byte maximum reads/writes, 0.206 ms
first visibility and 87.193 ms replay. Replay, monotone allocation and
reserved-tail reuse remain the reason to retain it. No reduced flash-write
claim is made. Physical flash writes and physical RSS/latency are NOT_RUN.

## One canonical lifecycle

`idric/FastChatCore.idric` is the semantic model. Its actual per-entry admission
functions emit the checked native table in `src/admission.generated.h`; the
Idriç acceptance harness compares the whole generated line with native source.
Icky C supplies the file effects and one-active-composer scheduling policy.
That policy narrows scheduling, not request/attempt semantics. Native PREFIX
records checkpoint accumulated text; byte-level transport fragmentation is
not a user event. In-memory Idriç history is a semantic fixture, not storage.

Submission journals `user_submitted` before fresh `transport_attempt_submitted`.
Uncertain delivery requires `retry_requested` with the prior attempt before
another `transport_attempt_submitted` with a fresh identity. Tests inspect
the exact event kinds and identities. A targeted removed-retry mutation must
fail `missing canonical retry_requested before transport_attempt_submitted`.
Both terminal orders, start/chunks after cancellation, stale attempts and
duplicate terminals are tested. No transport or renderer creates a retry.

The FC02 journal references absolute arena extents; full FC01 records fail
closed. A complete corrupt/foreign/stale record fails closed; a torn trailing
record is truncated to the prior valid boundary. Response fsync precedes prefix
journal write/fsync. Fault tests fork at data write, response fsync, journal
write and journal fsync; these are process-death tests, not simulated physical
power-loss evidence. Failed barriers and reservation exhaustion poison further
writes. Recovery turns nonterminal attempts into durable uncertainty. Replay
validates old response bytes, extents, UTF-8, sequence and attempt identities.
The earlier experimental FC02 lifecycle layout is incompatible and rejected;
no automatic destructive migration or history erasure is implemented.

## Application and provider seam

The composer/send, streaming, Stop, explicit Retry, restart/replay, older/newer
turns and bounded page reader share the same store. Rendering holds at most one
4096-byte response window, a bounded prompt/history selection and 64 page
offsets; it never caches an answer. Historical metadata is scanned on selection,
not on each frame. UI/IME/touch/jank remain physical NOT_RUN.

Absent configuration selects the deterministic acceptance backend, including
`:long`, `:lost` and `:fail`. It uses the same SSE parser and provider JSON
decoder as HTTP/2. To select HTTP/2, the app-private `transport.profile.tsv`
contains exactly four unique tab-separated rows: `origin`, `ca`, `model`,
`path`. HTTPS and an absolute CA path are required. Optional app-private
`authorization.header` supplies a runtime-owned authorization header; it is
never acquired, embedded, logged or journaled here. Invalid configuration fails
closed instead of silently falling back. Tests are entirely credential-free.

The request adaptation emits a bounded streaming Chat Completions request
(`model`, `stream`, one user message). Configure a compatible endpoint path;
this is not an assertion that arbitrary Responses request bodies are supported.
The response decoder supports Chat Completions content/tool deltas and the
Responses text/completed/failure/tool event families. Tool/event JSON is retained
as a durable event up to 1024 bytes and is never executed; larger or unknown
events fail explicitly. Records are bounded to 8192 bytes, depth 32 and 256
JSON tokens; malformed escapes, duplicate keys, truncated JSON, invalid UTF-8,
and unknown event types fail closed. EOF without a terminal is uncertain.
Local TLS fixtures cover all event paths, cancellation, slow storage, fresh
replay and absence of hidden POST retries. Real account/provider acceptance
is NOT_RUN. Provider contract references:
https://developers.openai.com/api/reference/resources/responses/streaming-events
and https://developers.openai.com/api/docs/guides/streaming-responses.

## Producer identity

The existing A1 producer and generic android-NDK packager remain the route.
The generic packager's pinned repair (android-NDK PR #17) normalizes payload
timestamps using the source commit epoch, sorts assets, normalizes copied
file modes, omits ZIP UID/GID extra fields and uses APK v2+ signing
for this API 26 candidate. The hosted gate repeats native compilation and
packaging and compares exact library/APK bytes; dependency build dates use the
same source epoch. This is qualification through the maintained recipes,
not a second producer or a whole-path physical reproducibility claim.
Package `org.isomorphisms.fastchat.diskstreaming.arena`, versionCode 4,
versionName `0.4-candidate`, API 26 minimum / 34 target, armeabi-v7a, NativeActivity,
no application DEX. The public development signer certificate remains
`aa9151e3922fa4795987c3655bc7aa4712620f1d94c24a2075acd7b29d8ac86e`.
It is not a production/store signer.

| Tool/material | Exact identity |
| --- | --- |
| ICK | 515c0f29fe6e2e96e10495fbaf25da93532e7722 |
| GCC material | 6294f1d9e7536e5ffcde09d1528c918d63abfef5; GCC 17.0.0 20260813 |
| ICK ARM driver / cc1 SHA256 | c78697d096ffe8ba10fbcc3bae9b35d0c934bee11a3626813656ac84e126f9ad / 289dee590a515e6426a6ab74d7dc8c143e428836a3a79036a75444f0622fba73 |
| ICK host driver / cc1 SHA256 | 86cdbaf8d405d7d05883f20ddcc0ac21a55a48bae20cc9f11aca78f1710867d3 / 5aff3188c58cae7cfffb20bdade56ec3e9ab89667ff6feafcfeed066350f9400 |
| Icky Lua | isomorphisms/lua 87306483cec50f8c750a22dda1d0742246fad756; compiled onelua.c through ICK |
| Idriç | ff4d852862a3942592f8ade9afde8d409d9803be; its maintained bootstrap, Chez 10.4.1 |
| Grease implementation | ba869518c7d850de6c47d8c6234654575e264e6c; run 37478624499 / artifact 11419719229; runtime SHA256 7e31cd05b7a9d8fb2a4a9e003a7f3fcb0159138506d17f0fb28da8cbe22aa85c |
| NDK | r29 / 29.0.14206865; ld.lld/LLVM 21 platform link, Bionic CRT and ARM compiler helper archive |
| GNU ARM assembler | Ubuntu binutils-arm-linux-gnueabi 2.42; explicit compiler/as binding |
| Packager | isomorphisms/android-NDK 32daa084390733d5b90cdec3157646832c75a3ad; maintained apk/build-nativeactivity-apk.sh, source-epoch reproducibility opt-in |
| Build-tools/platform | Android build-tools 34.0.0 / android-34; aapt2, zipalign, apksigner |
| Foreign generator | CMake 4.4.0; pinned curl/OpenSSL/nghttp2 in transport/DEPENDENCIES.md |

Every maintained C object, including HTTP/2 glue and foreign C, uses ICK.
OpenSSL `no-asm` avoids a new foreign assembler path; physical handshake latency
is NOT_RUN. Android feature probes link ICK objects with the NDK platform
driver because bare ICK's GNU target lacks Android CRT/runtime linking. The
final application link invokes ld.lld directly. The exact gap is recorded in
`ci/build-toolchain.tsv`, not used to replace first-party compilation.

## Authoritative paired-production dependency

Freshly fetched shared retained heads are Cat Food
`e2d65a5d76ff81bcc65f5cabf8ed2ad0e3a57482`, Kitchen
`dd7b40f2d4ddc47c33d019f4a4caa68b059bdeaf`, Flexible Pipes
`86c2d16f634eb73e0e338f2531827d1ea040b037`, and ai-ci
`5c80279dfcee565f4e999d2b1f97feb966b8efd3`.
Cat Food's `application-targets.tsv` remains authoritative for A1-primary →
C67-companion. No hardware facts are duplicated or new C67 producer created.

Flexible Pipes' registered `android-conversation-paired-build` still exits
unconditionally before an Android attempt in `scripts/run-registered-operation.pi`
with `MISSING_PREDECESSOR:S2 approved immutable plan; S4 qualified shared decision;
S5 exact Kitchen procedure; S3 independent acceptance`. This unavailable
authoritative predecessor closure is the single external paired-production
blocker; its first missing object is the S2 approved immutable application
plan. Fixture/test receipts are not producer authorization. No service is
activated, shared policy amended, or fabricated plan/decision supplied here.
A1 hosted qualification is independent; C67 paired production is BLOCKED.

Installation, launch, IME/touch/jank/RSS/CPU/latency and physical A1/C67 receipts,
and live account/provider acceptance are all NOT_RUN.
