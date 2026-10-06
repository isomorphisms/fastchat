# Process architecture: design before placement

Status: design note, not a frozen implementation.

FastChat should make sense as a system before process count, connection count, or benchmarks are chosen. Measurements validate or reject a design; they do not substitute for one.

## Human model

The basic objects are:

1. **Conversation** — durable logical history.
2. **View** — an ephemeral lens onto a conversation.
3. **Request** — one user/model transaction.
4. **Stream** — ordered events produced by a request.
5. **Store** — durable append/read authority for conversation events.
6. **Transport** — authority that turns requests into response streams.
7. **Renderer** — consumes view state and emits user actions.
8. **Process** — a placement/failure container, not a semantic object.

A conversation is not a process. A view is not a process. A model stream is not a TCP/TLS connection. A physical connection is an implementation detail of Transport.

## Architectural rule

One responsibility should have one explicit typed interface.

That does **not** imply one responsibility per Linux/Android process.

A useful design should permit the same semantic components to be placed:

- together in one process;
- renderer/view in one process and transport/store behind IPC;
- several view processes talking to one broker;
- more aggressively split when isolation, restart behavior, security, or resource measurements justify it.

This keeps process layout replaceable without changing conversation semantics.

## McIlroy / Plan 9 instinct

Prefer small components and explicit streams/capabilities over objects that know the whole application.

The core data flow should look roughly like:

```text
human action
   |
   v
renderer/view
   |
   | UserCommand
   v
conversation core
   |---------------------> store.append(Event)
   |
   | ModelRequest
   v
transport
   |
   | Stream ModelEvent
   v
conversation core
   |---------------------> store.append(Event)
   |
   v
renderer/view
```

Each arrow should remain meaningful if it crosses an address-space boundary later.

## Transport is not "a connection pool" at the interface

The UI should not ask for sockets or own connections.

Expose something closer to:

```idric
record ConversationId where
  ...

record RequestId where
  ...

record AuthorityId where
  ...

record Endpoint where
  ...

record UserCommand where
  conversation : ConversationId
  body         : CommandBody

record ModelRequest where
  conversation : ConversationId
  request      : RequestId
  authority    : AuthorityId
  endpoint     : Endpoint
  payload      : RequestPayload

data ModelEvent
  = ResponseStarted RequestId
  | ResponseChunk   RequestId Bytes
  | ToolEvent       RequestId ToolPayload
  | ResponseEnded   RequestId FinishReason
  | ResponseFailed  RequestId Failure

interface Transport where
  submit : ModelRequest -> Stream ModelEvent
  cancel : RequestId -> Result ()

interface ThreadStore where
  append : ConversationId -> Event -> Result SequenceNumber
  read   : ConversationId -> Cursor -> Stream Event

interface Renderer where
  present : ViewState -> RenderResult
  actions : Stream UserCommand
```

This is an Idriç-shaped architecture sketch, not a claim that current Idriç accepts these exact declarations.

The physical implementation beneath `Transport` may use HTTP/1.1, HTTP/2, HTTP/3, one connection, several connections, or reconnection. None of that belongs in the renderer or conversation API.

## Natural sharing boundary

If sharing exists, the strongest first candidate is the transport authority keyed by things that actually govern connection reuse:

```text
(account/authority, origin/endpoint, protocol policy, network context)
```

Several conversations may use one physical connection. One conversation may use several physical connections over its lifetime. Therefore conversation identity and connection identity should never be equated.

Credentials also belong with the authority/transport boundary, not with every view.

## Store boundary

The store should expose append/read semantics, not database implementation.

That keeps open:

- ordinary files;
- SQLite;
- appendFAT generations;
- an IB/Pensieve-backed retained corpus;
- a brokered store in another process.

The conversation core should not care.

## Process topologies worth keeping possible

### A — co-located

```text
[ views | conversation core | store adapter | transport adapter ]
```

This is the simplest likely first implementation. Typed boundaries still exist in-process.

### B — shared broker

```text
[view/core A] --[view/core B] ----> [transport/store broker]
[view/core C] --/
```

This buys a stronger failure/restart/security boundary while still sharing expensive transport/auth state.

### C — per-view processes

```text
[view/core A] -> [broker]
[view/core B] -> [broker]
[view/core C] -> [broker]
```

This should remain possible but should not be the default merely because the interfaces permit it.

### D — fully independent views

Each view owns its own transport/store resources. This is architecturally simple but probably duplicates the most state and is the least attractive default. Keep it mainly as a comparison/failure-isolation extreme.

## What intuition says before measurement

A shared transport authority is attractive because:

- authentication state is naturally shared;
- HTTP/2 or HTTP/3 can multiplex logical streams;
- reconnect/backoff policy should not be independently reinvented by every view;
- rate-limit state and provider policy are account/origin concerns, not view concerns;
- background generation should survive destruction/recreation of one view when possible.

A shared *everything* object is unattractive because it turns an implementation convenience into semantic coupling.

So the initial architectural preference is:

> explicit small interfaces, one application process at first, one shared transport authority, durable store behind its own interface, renderer/view instances as clients.

The process boundary can then move without redesigning the program.

## Questions that require human design discussion

Before freezing the first implementation, decide explicitly:

1. whether the conversation core or the store owns canonical event ordering;
2. whether active generation survives loss of every visible view;
3. whether one conversation may have several simultaneous model requests;
4. whether cancellation is scoped to request, conversation, or authority;
5. whether tool/attachment streams are ordinary conversation events or separate capabilities;
6. whether the transport broker is allowed to retain conversation state beyond what is needed to route requests;
7. whether credentials are process-local, broker-local, or exposed through a narrower capability;
8. whether appendFAT should be a primary store implementation or merely one durable sink;
9. how IB/Pensieve consumes the same event stream without becoming part of FastChat's critical path.

These are architecture questions. Benchmarks should not answer them accidentally.

## Measurements and calculations

After the interfaces and semantics are coherent, measure placements rather than components in isolation.

Use real conversation-size distributions and real phones, but treat results as evidence about where to place already-defined responsibilities.

The cross-project analytical/measurement mirror is:

https://github.com/walnut-burgundy/computer-science/issues/76

The FastChat application tracker is:

https://github.com/isomorphisms/fastchat/issues/2
