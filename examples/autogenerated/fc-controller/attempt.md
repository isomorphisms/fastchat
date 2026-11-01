# FC-CONTROL-01: typed controller and watcher fixture

## Type sketch before implementation

Values: a job identity, parent identity, STAR/SUN/EARTH/MOON allocation,
repository/source/history references, complete job text, acceptance conditions,
and an FP-owned payload digest. A proposal is data; it has no shell authority.
Delivery, dispatch and independent acceptance are distinct observations.
The trusted external FP adapter must validate real captures and hashes before
constructing an observation. This pure model does not authenticate receipts.

Central operations: proposal → controller state; state × observed delivery →
state; state × explicit dispatch authority → state × optional typed FP submit;
state × dispatch acknowledgement → state; state × independent acceptance →
state. Mismatched identities/digests/dispatch receipts leave the state unchanged.
No model reply or GitHub issue creation is a dispatch acknowledgement.

Watcher values: source identity, enabled state, durable cursor and pending event.
Observe one matching fresh event → one proposal identity. Re-observe that event
after restoring the same state → no proposal. Disabled/unrelated/stale events
do not move the cursor. A trusted store must persist that state; passing values
through a fixture does not exercise a filesystem crash or hosted watcher.

Effects and backend: the Idriç model is pure; only its fixture main performs
output. Existing Icky C owns actual disk effects and HTTP/2. The native
conversation request adapter now reads durable history in bounded windows;
it refuses overbudget context before POST. JSON remains at the external API
boundary. No generated C/RefC fallback is introduced.

Planned sources: FastChatController.idric and its semantic tests, staged into
the existing FastChat Idriç acceptance harness. Local compiler identity and
results will be recorded after execution; hosted CI keeps its established
immutable compiler pin. This fixture is not wired into the APK's controls and
does not provide a deployed FP transport, visible sink, model credentials or
watcher service.

First deployment gap: an authenticated FP submit/status adapter with captured
delivery/dispatch receipts. The acceptance test is one actual observed remote
dispatch with matching full payload, followed by independent ai-ci acceptance;
the pure fixture cannot establish that result. No frontend LoRA training or
fixed memory classifier is added.

## Executed local result

The actual Idriç compiler at source
`2734ac5a86610eddfab73b1644d72b4ac85a65ee` typechecked the source and executed
`--exec main FastChatController.idric` through Chez, reporting
`PASS typed controller and watcher semantic fixtures`. Its uninstalled local
layout first reported `Module Prelude not found`; supplying its existing
prelude/base TTC paths and support/Chez paths resolved that environment issue.
No language fallback was used. Hosted acceptance retains the existing
`ff4d852862a3942592f8ade9afde8d409d9803be` compiler pin; the local result is not
evidence for that older compiler until the pinned workflow runs.

No Idriç language capability blocked this pure slice. Required next work is
the effect adapter and independently validated external receipts, followed by
actual crash-safe persistence and remote dispatch acceptance. This is a
deployment/FFI integration boundary, not a claim of a compiler defect.
