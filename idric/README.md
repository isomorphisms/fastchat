# Idriç conversation core

This directory contains the executable semantic core for FastChat's request,
conversation, transport and view boundaries.

It deliberately does **not** define a connection pool. A `model_request` is
submitted to a `transport_authority`; the eventual transport implementation is
free to use HTTP/1.1, HTTP/2, HTTP/3, a local broker, or another mechanism without
changing conversation semantics.

## Settled semantics in this slice

- conversation identity, view identity, request identity and transport-attempt
  identity are distinct types;
- a request can outlive every visible view;
- views observe a durable event log through independent cursors;
- non-visual consumers can read the same log directly through an `event_cursor`;
- user commands produce typed transport commands; neither the renderer nor the
  conversation core owns sockets or a connection pool;
- the conversation core admits canonical lifecycle events;
- the store log supplies monotone durable sequence numbers;
- stale-attempt and duplicate/late transport observations are rejected from
  canonical history and retained as diagnostics;
- cancellation is request-scoped;
- if completion races a cancellation acknowledgement, whichever terminal event
  the core admits first wins;
- losing the transport creates an explicit `delivery_uncertain` state;
- retry from uncertainty is explicit and receives a fresh attempt identity;
- late events from the old attempt cannot enter canonical history.

The in-memory event log and simulated transport are semantic fixtures. They are
not claims about the eventual Android storage or networking implementation.

## Acceptance

The workflow pins the compiler to:

`isomorphisms/Idric@ff4d852862a3942592f8ade9afde8d409d9803be`

It bootstraps that exact Idriç compiler, typechecks both source files, then runs
the pure fixture suite. A green host run establishes this source-level semantic
slice only. It does not establish an Android backend, physical-device behavior,
network correctness, or durable-storage behavior.

The companion architecture discussion is
[`notes/process-architecture.md`](../notes/process-architecture.md).
