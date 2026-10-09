# Actual host consumer runtime admission

The host producer requires the shared Flexible Pipes qualifier at
`5caaba31256bfb424638f8a3d4d554c56fa9a970` (Flexible Pipes PR #52). Its exact-head
hosted run `37808205749` passed the modified interpreter, embedded raw glyph
fixture and eight stock, stale, PATH, source and producer dependency controls.
The runtime is isomorphisms/lua `87306483cec50f8c750a22dda1d0742246fad756`, Lua
5.5.1, not a version-only admission. See that pinned owner's
`docs/modified-lua-runtime.md` for the full source/compiler closure.

The host recipe is now:

    grease scripts/test-core.grease ICK_DRIVER ICK_LIBEXEC ICKY_LUA_SOURCE OUTPUT FP_CHECKOUT GREASE STOCK_LUA_SOURCE RUNTIME_ICK_DRIVER RUNTIME_ICK_LIBEXEC [host options...]

`FP_CHECKOUT` must be clean at the exact qualifier commit. `STOCK_LUA_SOURCE`
must be clean at `0b29f408433e92953cc72b1d3e06c7ac8139e439`, the actual stock
5.5.1 control. The script executes the whole glyph fixture using both the exact
interpreter and an embedded static runtime. It links that checked `icky-lua.o`
into FastChat and verifies its digest before and after the link. The final
executable's `.rodata.fastchat_policy` bytes must compare exactly with
`policy/composition.lua`; dynamic Lua dependencies are rejected. It executes
the unchanged FastChat suite. A second executable with the actual stock object
and the same consumer/policy must fail at policy admission, not an unrelated
build or launch error. `OUTPUT.runtime/consumer.tsv` and the shared qualifier's
receipt preserve actual paths, digests and results.

The host CI restores xgcc/cc1/collect2 from the qualified ICK source
`515c0f29fe6e2e96e10495fbaf25da93532e7722`, FastChat run `37589849631`, artifact
`11468256648`. The qualifier checks the published executable digests before
compilation. GCC 13's installed host headers/CRT/search paths and GNU as/ld
provide the declared host link support. `-fno-use-linker-plugin` disables the
omitted LTO plugin; no LTO is requested. This restoration is an artifact-backed
producer lane. An expired or absent artifact is a blocker requiring restoration
of the same owned source build, not permission to select another compiler.

Grease uses its exact verified artifact and the unchanged owner launcher at
packaging commit `ba869518c7d850de6c47d8c6234654575e264e6c`. The three-line
launcher is inherited runtime packaging plumbing. Qualification orchestration
is Grease. The ASan engine runs with `detect_leaks=0` on the host runner.

This is host admission only. The separate Android ICK/NDK producer lane and
physical MIRO A1 acceptance retain their own receipts. The host consumer receipt
explicitly records Android build and physical device as `NOT_RUN`.
