# FC-CONTROL-01: durable model context and typed controller fixture

This is a partial implementation of
[SUN FC-CONTROL-01](https://github.com/isomorphisms/flexible-pipes/issues/223),
stacked on [isomorphisms/fastchat PR #17, “FC-UI-01: render the message editor in its own input surface”](https://github.com/isomorphisms/fastchat/pull/17),
parent `de3f747f8b55e8160c5acc9d89e0962f9d748764`.

## Implemented application behavior

The real transport boundary now assembles ordered user/assistant text from
canonical durable history, instead of sending only the latest prompt. Both
Chat Completions `messages` and Responses `input` use explicit message arrays.
The reader takes at most one 4096-byte stored response window at a time; the
existing transport's 8192-byte JSON body bound is retained. History is neither
silently truncated nor summarized. Overbudget/invalid context refuses before
POST, clears the unsuccessful body and returns a local refusal. The native UI
records this known refusal as failure rather than inventing remote uncertainty.

Previously cancelled, failed or uncertain turns carry an application-generated
history-status message before any committed partial assistant text. Completed
text remains distinguished from partial text. Existing tool events remain
durable data; this text-only context adapter neither executes tools nor claims
complete provider-specific reasoning/tool-call continuation.

The normal screen identifies the configured model or explicitly identifies
the test responder. Provider configuration remains app-private and external;
no model, API key, ChatGPT session or vendor account is supplied here. The
continuation now binds public routing/model identity to each new durable
attempt and checks it before POST; see [attempt-provider.md](attempt-provider.md).
Old turns remain explicitly unrecorded. In-app endpoint setup and model
selection remain unfinished.

## Controller and watcher boundary

The generated Idriç attempt contains typed allocations, proposals with full
text/context/digest, separate delivery/dispatch/acceptance states and a bounded
single-pending-event watcher. Wrong identity, altered digest, absent authority,
empty/wrong acknowledgement, premature acceptance and repeated dispatch are
rejected by the semantic fixture. Restoring the pending watcher state suppresses
duplicate proposals; disabled, unrelated and stale events do not produce work.

This model is executable source, not an authenticated effect adapter. It is
staged into the existing Idriç acceptance harness and remains unwired to the
APK controls. A caller must obtain real captures and independently validate
hashes before supplying receipts. Passing a constructed receipt to the pure
fixture proves no remote dispatch. Likewise passing saved values back into a
fixture proves no atomic disk persistence or externally hosted watcher.

## Validation and remaining boundaries

The local full ICK/Icky Lua core suite passes, including added durable two-turn
replay/context, exact JSON and UTF-8, exact-capacity/overbudget refusal and
cancelled partial-history tests. The local compiler driver SHA-256 is
`a5739c9bedb9e7999ae30ed9e29e82dabb17e11beb7ee7ab1a4b713681b015c9`;
cc1 is `6ee7b286e364305a5f9456d38dd1aad063efb2034e3c5fd864312a9c3890f218`.
Grease SHA-256 is
`7e31cd05b7a9d8fb2a4a9e003a7f3fcb0159138506d17f0fb28da8cbe22aa85c`;
Icky Lua is `87306483cec50f8c750a22dda1d0742246fad756`.
Host linking used the existing GNU 13 CRT/helper library directory; no stock
C frontend compiled the maintained source. The hosted workflows keep their
established exact ICK/Idriç producer pins.

The local Idriç controller fixture passes through Chez. Both changed Node TLS
fixture files pass Node syntax checking. The extended actual TLS harness now
checks two POSTs separated by store close/reopen for each request schema,
including exact previous Unicode answer bytes at the server. The existing
eleven-path scenario and two explicit retries are retained; the exact POST
count becomes 17. Local TLS execution is NOT_RUN because the matching pinned
HTTP/2 static dependency prefix is absent; hosted CI must execute it.

Android compilation/package, physical A1/C67 UI, live provider, genuine FP job
dispatch and independent production acceptance remain unqualified. No devices
were mutated; no model requests or watcher effects were sent to external
services. No merge or release is authorized by this candidate.

## Decision and evidence

Keep model transport, canonical history, pure controller decisions and trusted
FP effects as distinct interfaces. Consume the existing FP handoff identity
and captured visible/dispatch receipts. Inspection of FP main
`181c6011956a39454fd8b018380c2ea54f0ab8e7` and `docs/job-delivery.md` finds no
deployed visible-sink/model-job adapter. The separate Android registered
candidate still refuses absent S2/S4/S5/S3 predecessor closure. A phone-side
second scheduler or raw shell dispatcher would not close this boundary.

Recommended allocation
- Sun: further work only if the deployed FP API contradicts these boundaries.
- Earth: connect the typed model to an authenticated existing FP submit/status
  adapter and native controls; bind durable request/attempt provider identity.
- Moon: after that exact Earth head passes, propagate verified packaging and
  physical-device acceptance receipts through the existing owners.

Earth follow-up job
EARTH FC-CONTROL-02 — Real FP adapter and controller controls
Refresh this candidate and the current FP deployment. Preserve the complete
job text, parent/repository/history/source/acceptance fields and FP digest.
Use the established FP capture/authorization/dispatch/ai-ci interfaces to
connect the typed operations to the native app. Persist pending watcher state
before any proposal effect and rehydrate it on restart. Bind model/provider
identity to each attempt without erasing or silently migrating old history.
Do not reinterpret a pure receipt, issue creation or chat output as dispatch.
The first required external material is a qualified authenticated FP
submit/status endpoint and trusted receipt validator. If it is absent, report
that exact material as BLOCKED; do not build a competing scheduler. Validate
actual delivery bytes, duplicate/restart behavior, one genuine acknowledged
dispatch and independent acceptance; keep device/live-provider evidence separate.
No merge, release, credentials acquisition or broad watcher activation.

Moon follow-up job(s)
NONE until the Earth implementation has an exact qualified head and actual
external dispatch/acceptance evidence.
