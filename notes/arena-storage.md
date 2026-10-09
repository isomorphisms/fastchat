# Shared userspace response arena

This branch layers the reusable userspace arena primitive from
`isomorphisms/sd-card-append-fat` under the existing FastChat disk-streaming
experiment.

Pinned implementation provenance is in
`third-party/appendfat-arena/PROVENANCE.md`.

## Storage shape

Each conversation owns:

- `history.events`: the existing canonical event journal and admission authority.
- `responses.arena`: one long-lived response byte arena shared by all attempts.

The journal remains authoritative. FastChat does not use the arena library's
`.used` sidecar protocol for response visibility because the existing journal
already defines durable/committed response extents.

The arena-backed journal is versioned as `FC02`; it is intentionally not read as the earlier per-attempt `FC01` store format. The record extent field is an absolute byte position in `responses.arena`.
For SUBMIT/RETRY it records the new attempt base. For PREFIX and terminal
records it records `base + durable_extent`. The in-memory response extents
remain relative to the current attempt, so the renderer and transport contracts
do not change.

## Reservation and reuse

The arena grows in 64 KiB quanta through
`appendfat_arena_reserve_fd()`. Growth explicitly zero-fills only the new
reserved tail. Completion does not truncate the arena. The next attempt starts
at the previous high-water mark and consumes any remaining reserved capacity
before another growth operation.

This matters because doing the same zero-fill reservation independently for
every attempt would add write traffic without amortizing it. The shared arena
is the actual FastChat experiment.

## Durability

Response bytes are still fsynced before their PREFIX/terminal journal record is
made durable. A journal record is what makes a response prefix canonical.
Uncommitted tail bytes after process death are not replayed as completed data
and may be overwritten by a later attempt.

The arena file is not chmod-sealed after completion; immutability is enforced
by monotone journaled addresses and by never allocating a later attempt below
the recovered high-water mark.

## Evidence boundary

Host tests require reserved-tail reuse across two attempts and fresh-process
replay. Android logs report reserve bytes/calls plus arena capacity/high-water.

This experiment does not yet establish that explicit zero-fill reservation
reduces physical flash writes or improves latency on either MIRO phone. That is
a physical-device comparison gate.
