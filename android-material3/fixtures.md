# Shared comparison corpus

These fixtures port PR #4's semantic cases and extend them with real ordinary-file
storage failures. A later fetch found FC-D1 at `66d4c1d202a3f2b3764e35123b51e1e2a7cbaca1`
and FC-S1 at `600cf446fc3cc6a1311e7493e422df0de11252af`. Their six-row
`qualification/comparison-corpus.tsv` is captured byte-for-byte as
`core/src/test/resources/comparison-corpus.tsv`. Its upstream `NOT_RUN` labels
remain historical native-lane statuses; `SharedCorpusTest` independently executes
all six cases in this lane. Native implementations are still blocked, not tested.

`Utf8Chunks` is an adapter helper with at most three pending bytes. It preserves
code points split across transport chunks, reports invalid/truncated encoding,
and introduces no HTTP/SSE implementation. Canonical `Observation` remains the
decoded-text semantic boundary from PR #4. Final response UTF-8 bytes must match
the shared corpus exactly across durable completion and reopening.

| Case | Input/observation | Required outcome |
| --- | --- | --- |
| Interleaving | Submit first/second; start both; chunk both; complete second/first | distinct IDs, both complete, 10 canonical events |
| Views | two cursors at 0; reread one after advancing | 5 / 0 / 5 visible events; wrong conversation reads none |
| Completion wins | submit, start, cancel intent, complete, late cancel | completed, 5 canonical events |
| Cancellation wins | submit, start, cancel intent, cancel ack, late complete/chunk | cancelled; late events absent |
| Uncertainty | start, partial chunk, connection loss | uncertain; no completed response; no automatic resend |
| Retry | explicit retry after uncertainty, old-attempt chunk/complete | fresh attempt; stale observations absent; response contains only new attempt |
| Admission | wrong authority/request, chunk before start | ignored, no canonical append |
| Restart | abrupt child-process exit after durable partial response | active attempt becomes uncertain exactly once; explicit retry uses next attempt |
| Partial frame | every byte prefix of a second transaction | only first complete command replays |
| Write/sync failure | injected partial write and sync error | no published submission; no transport effect; owner fails closed |
| Corruption/ownership | corrupt CRC; second writer | error preserving bytes; exclusive owner |
| Text | `Fixtures.unicode`, `Fixtures.response` | exact durable UTF-8 round trip; fenced code and heading blocks |
| Long history | `Fixtures.longThread`, 500 requests | 1000 stable message rows, 2500 canonical events |

`core` tests execute these host fixtures; `app` unit tests also exercise the real
controller/Transport boundary without any view. Instrumented screen tests cover
save/restore, Unicode send, failed-send draft retention, incremental phase changes,
copy/code controls, and reading-position behavior. Compiling those tests does not
prove execution or physical IME/cursor correctness.

For renderer cost comparison, use the same Unicode/response bytes, 32 UTF-16-unit
chunks without splitting surrogate pairs, 80 ms event intervals, and the same
500-request corpus. Both lanes must publish only durably completed text initially.
Any later FC-S1 comparison selects stored-streaming projection on both sides.
