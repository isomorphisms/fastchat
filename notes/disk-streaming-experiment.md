# FC-S1 — disk-backed streaming comparison

Independent branch sun/disk-streaming-icky-c-lua, based directly on fetched main
b49f0f7083d2d034ba76b910a43592d6d07bd18b. Do not merge this experiment.

The first C source uses the same admission, journal and file store as FC-D1.
The branch's renderer policy is the sole implementation difference:
renderer_follows_prefixes returns 1 here and 0 in the disk-first sibling.
The renderer reads eligible committed bytes from disk through a bounded UTF-8
window. Written, durable, committed and eligible extents remain explicit.
The store writes synchronously in bounded slices; it returns accepted-byte
counts and explicit backpressure. A slow renderer leaves unread data on disk.

This is a C-pass implementation in progress, not a completed live Android
application. See [the shared C-pass note](c-pass.md) for authored tests,
first producer-run evidence and exact remaining gates. The source-built ICK
workflow must execute against this branch's exact head before any passing
C evidence is claimed. The hosted controls compare the same 8 MiB body and
byte-identical canonical journal/response streams, but do not measure Android
visible-text latency.

Hostile SSE/JSON framing, the complete TSV corpus runner, native Canvas/Paint
presentation and composer, ARM32/API-21 artifact, actual Icky Lua second pass,
APK/signer and physical A1 measurements still remain.
Physical acceptance: BLOCKED/NOT_RUN. No compiler or build tools go on the phone.
