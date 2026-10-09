# Division source qualification — 2026-10-09

Maintained arithmetic uses literal `÷` with existing integer/floating C semantics.
ICK source is `c61e448251744a2f40ad743ebef1a027bdcd2f9d`; the shared native and
Android producer is `isomorphisms/ai-ci@015cc7901ae0b3ad262b476f24e129b53c56db95`.
Comments, include paths, strings and the independently pinned Lua runtime remain
lexically unchanged.

Local qualification passed the complete existing host core suite and an actual
ARMv7/API 26 Android native library build with exact NDK r29. Source compilation
retains all existing optimization, warning, PIC and ABI flags. ICK's own builtin
headers precede the exact Bionic sysroot under `-nostdinc`; the existing GNU ARM
assembler, NDK linker, Bionic CRT/stubs and ARM builtins retain their declared roles.
The result exports NativeActivity, is ELF32 ARM, and rejects the hard-float calling
convention. Physical-device execution is NOT_RUN.


Hosted workflows bind their subsequent artifacts and source-revision receipts to
the exact PR head. Local source/runtime acceptance does not claim those hosted
jobs, APK installation or physical execution have already passed.
