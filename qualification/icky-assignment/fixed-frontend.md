# Assignment-arrow prerequisite: fixed frontend

The historical probe in this directory failed with the old owned ICK frontend.
It now compiles unchanged with the compiler fix in
[dilapidated-shed/ick PR #77, “Accept ← assignment in the Icky C frontend”](https://github.com/dilapidated-shed/ick/pull/77).

Compiler source revision: `2a27ad6ab4e4601c9a0e4a5fa7915712db707af0`.
Receipt-only descendant: `e81e8f98ebf94fb209b001b4ae0e85a1b3fb75c7`.
Exact [compiler qualification receipt](https://github.com/dilapidated-shed/ick/blob/e81e8f98ebf94fb209b001b4ae0e85a1b3fb75c7/qualification/c-assignment-arrow/receipt.tsv).

The C frontend maps the existing single UTF-8 U+2190 token to the ordinary
assignment/initializer token after preprocessing. No consumer-side translator
is used. The exact probe SHA256 remains
`afecda98c162cb9ce6ff3e6bf7f8b8b960dcc207d76f3710dfb89f8bc7f15957`.
Old frontend: exit 1. Rebuilt frontend: exit 0, warnings as errors.
Fixed `cc1` SHA256:
`e4ef7cc5192c103809eeb99de79f92101f65b58bfe27d2169954d9b81d658fe5`.

The compiler regressions cover initialization, member/pointer assignment,
single evaluation, chaining, macros, literals and UTF-8 byte preservation.
They pass under Linux/AArch64 QEMU; invalid lvalues, read-only variables and an
unsupported neighboring glyph remain rejected. All source-manifest hashes
and fresh materialization pass.

This removes the demonstrated syntax failure on the explicitly pinned fix
revision. ICK main has not been merged or relabelled. Both FastChat experiments
remain unimplemented and unmerged. The full C pass, storage/fault/replay tests,
Lua second pass, RAM comparison, ARM32/Bionic/native packaging and physical A1
acceptance remain required. Physical MIRO A1: **BLOCKED/NOT_RUN**.

The later [application-boundary retry](../android-application-boundary/README.md)
reruns the passing arrow probe, executes an Icky C host-file witness, and
records the next failures against real NDK r29 declarations. The current
application blocker is Bionic/Android frontend support, rather than `←`.
