# FASTCHAT-RELEASE-01

Status: **BLOCKED**. The user authorized execution of the release brief and
requested reuse of the existing composer/provider work.

The existing native convergence on main already integrates the conversation
model, disk-backed streaming, response arena and HTTP/2. The reading changes
and the composer/provider-identity continuation remain stacked on that main.
Their implementation is reused rather than recreated. Experimental Compose,
HTTP/3 and comparison branches stay independent.

The original continuation at `7add1607b43a3ecdfb8175de950955f5b7b22b11`
passed its four hosted gates. Those are historical for later source changes:
core https://github.com/isomorphisms/fastchat/actions/runs/39706180532 ;
HTTP/2 https://github.com/isomorphisms/fastchat/actions/runs/39706180558 ;
Idriç https://github.com/isomorphisms/fastchat/actions/runs/39706181009 ;
ARM32 APK https://github.com/isomorphisms/fastchat/actions/runs/39706181456 .

## Required evidence

| Boundary | Required result |
| --- | --- |
| Source integration | Reconcile against actual main; preserve comparisons and all useful semantics. |
| Host behavior | Current-source ICK core, fault/replay, settings/draft and hostile mutation checks pass. |
| Protocol behavior | Current-source credential-free TLS/provider scenarios pass, with bounded reads and exact POST counts. |
| Idriç | Existing core/controller fixture execution and provider-setup typecheck pass at the declared compiler pin. |
| APKs | Exact-source ARM32 and ARM64 native builds, repeat packaging and stable signatures. |
| Shared production | FastChat registry entry, paired recipe, Cat Food plan, Kitchen execution and independent ai-ci acceptance. |
| Live provider | An actual configured API conversation, multi-turn context and durable restart. |
| MIRO A1 | Exact artifact installed; keyboard, Unicode editing/paste, paging/copy, Stop/Retry, rotation, restart, update retention, RAM and responsiveness observed. |
| MIRO C67 | The same checks with the actual ARM64 artifact; never substitute ARM32 compatibility. |
| Public release | Accepted APKs, instructions, app-captured screenshots or demonstration and accurate topics. |

No connected physical device or provider credentials are assumed. A hosted
artifact cannot satisfy those final boundaries. Keep the pull request in draft
and release status BLOCKED until receipts identify each device and exact APK.
The shared producer registry currently contains Wegert only; FastChat's
registration must use its own reviewed paired recipe. The existing pinned
Grease source-producer repair has its own qualification gate and cannot be
called accepted merely because an equivalent downloaded runtime succeeds.
