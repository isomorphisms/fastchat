# Read-only ChatGPT history: native consumer boundary

FastChat presently talks to agents through a bounded streaming
Chat Completions/Responses transport; it is **not** an authenticated ChatGPT
web-history client. Sending a prompt and retrieving account history are
different provider contracts.

This candidate creates only the read-only history **selection boundary**.
It consumes the native C endpoint source from
[dilapidated-shed/ChatGPTExporter](https://github.com/dilapidated-shed/ChatGPTExporter/pull/10)
at immutable revision
54ee4fceaa3e5cd1776ea7bfe70280f2e8343e5b, independently compiles the
planner and runs synthetic FastChat consumer assertions in CI. No vendored copy
can quietly diverge. The two repositories may later share the same source
through a formally pinned native build dependency.

FastChat **permits** paginated main/archived/project/shared history, conversation
graphs, bounded batch lookups and conversation file descriptors. It
**rejects** authentication probing, account memories/settings and arbitrary
operations from the thread-reading UI. It cannot mutate remote conversations.

## Still required before a useful history-reading APK

- A consented and bounded native session strategy, with any browser-owned
  credentials kept outside the application history selector.
- A fixed-origin verified HTTPS GET/POST transport with response-size limits,
  uncertainty and retry semantics, and private diagnostics.
- Mapping of actual provider graphs into the durable FastChat thread/skein
  model, including branches, tools and assets, without discarding unknown data.
- ICK-qualified native ARM32 and ARM64 builds (A1 required, C67 companion),
  followed by physical login/render/replay acceptance.
- Registered Flexible Pipes dispatch, independent ai-ci acceptance, and
  observed handoff receipts. [Envelope #147](https://github.com/isomorphisms/flexible-pipes/issues/147)
  is not an execution receipt.

This slice has no browserless live account access, shipping APK, or runtime
request execution. Do not merge or release on host CI alone.
