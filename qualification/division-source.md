# Division source qualification — 2026-10-09

The maintained transport receive threshold and standalone timing calculation use
literal ÷ through ICK `c61e448251744a2f40ad743ebef1a027bdcd2f9d`.
`_/transport/OwnedC.mk` is called by the existing Grease build/link/qualification
recipes. It compiles each maintained translation unit to assembly, then uses the
original NDK assembler/linker. External library sources and their complete curl
policy patch retain their exact pins. No stock-C source fallback exists.

Both changed C translation units compile and assemble locally for native x86_64
and ARMv7/API24 against the exact curl source headers. The existing byte-split
SSE/Unicode/framing/bounds suite passes through the same host producer.
The published workflow performs full external-library builds, final links and
the existing native protocol fixture on the exact head. Full protocol, typed
Idriç core and physical-phone acceptance are distinct: only the hosted native
fixture is added here; the original typed-core Grease qualification remains
available and is not replaced by a source compilation claim.
