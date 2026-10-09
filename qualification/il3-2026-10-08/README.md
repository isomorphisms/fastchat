# Executed IL-3 host preservation

The independent disk-first and streaming experiments were rebuilt at D1
`7e2129cf4021af68fe69726e9e913751393e6a94` and S1
`d09978739c54858adf8a619ed4ba02cf312863ce`. All application code, Lua policy
and original tests were unchanged. This directory is an evidence-only child.

The actual Icky Lua fork `87306483cec50f8c750a22dda1d0742246fad756`
was compiled through the exact hosted ICK artifact. The restored full frontend
was verified against the immutable ZIP member before a debug-only stripped
copy was used for these Lua rebuilds. `host-identity.tsv` records both identities;
this is a debug-only derivative, not an alternate compiler. Host GCC 13 headers
and link support provide the declared host system boundary. The hosted package
omits `liblto_plugin.so`, so the explicit `-fno-use-linker-plugin` package flag
was used. No C or Lua source symbols were normalized.

The preserved original `test-core.grease`, `compare-branches.grease` and
`compare-passes.grease` ran through the exact recorded producer Grease runtime
entrypoint. Both current executables passed storage/admission/cancellation,
framing/UTF-8, malformed policy, VM bounds and restart/replay tests. The same
logical 15 stores and 8 MiB response compare byte for byte between siblings and
against their preserved C-before-Lua checkpoints. Raw source ranges bounded by
`fastchat_policy_begin/end` were extracted from each actual executable and
`cmp` matches the original policy files (D1 SHA-256
`02a35c0ab0139fabe9af792214bc345f2b9006539da0269b5c1a8e961dbc8c1f`, S1
`2e46a121818a03ec3b94994245c90a8ac4fcaace0e48f3032a54dad34f371bd5`).
Dynamic dependencies are only libc and libm; the actual fork is compiled in.

The wider IL-0 qualification separately passes the interpreter and literal
embedded parser fixture plus all eight rejection controls: same-version stock,
stale source, earlier PATH stock, wrong linked object, older symbolic fork,
ordinary-source substitution, missing producer dependency and stock compiler.
Consumer receipt evidence here is not substituted for that producer gate.

The default main ref was freshly observed at
`ff84af404be34c4a8369de549bf8b32429445c19`; PR metadata reports the independent
comparison base `b49f0f7083d2d034ba76b910a43592d6d07bd18b`. This result makes
no merge decision and does not combine comparison branches or FC-U1.

All execution is Ubuntu 24.04.3 x86_64 host evidence. Fresh-process replay does
not establish a cold filesystem cache. No APK was built in this execution and
no physical MIRO A1 or TAB_P10 acceptance was run. Existing artifact/device
boundaries remain separate. See the IL-3 owner brief in
[ai-ci #235](https://github.com/isomorphisms/ai-ci/issues/235).
