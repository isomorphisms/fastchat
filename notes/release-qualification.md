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

## Concrete shared-production boundary

The application supplies `scripts/build-paired.grease` for the existing FP
registered Android stage. Its `.sh` file is only the fixed Bash entry interface
required by that stage; Grease remains the producer implementation. The recipe
uses the same native, HTTP/2, packager, signer and repeat-production procedures
as the hosted matrix, then leaves independent ai-ci admission to FP.

The build host must explicitly supply GREASE_BINARY, FC_NDK_ROOT,
FC_ICKY_LUA_SOURCE, FC_HTTP2_SOURCES, FC_CMAKE, FC_ICK_ARM_DRIVER,
FC_ICK_ARM_SUPPORT, FC_ICK_ARM64_DRIVER and FC_ICK_ARM64_SUPPORT, plus the
existing SDK, Cat Food and packager inputs. It performs no phone-side builds.

At Cat Food main `eafd7fa9e61138a664b205bf8ba82ec6837bbbb9`, the actual
`plan-android-build` command rejects this application with
`missing or duplicate approved launcher identity`. The required registration
already exists in isomorphisms/catfood PR #136, “FC-M2/M4: register FastChat
planning identity and classify captured devices”:
https://github.com/isomorphisms/catfood/pull/136 . Reuse that work.

The independent Grease repair isomorphisms/fastchat PR #16, “Repair remaining
FastChat workflows with pinned Grease producer”
(https://github.com/isomorphisms/fastchat/pull/16), still fails its source
producer at the `builtin-trap` spec group: four reference-shell failures against
the existing allowance of two. Its actual job log was inspected; this is not
an accepted or waived producer. The current test candidate retains the existing
digest-verified downloaded Grease bytes. Integrating an unqualified repair
would not satisfy production acceptance.

## October 31, 2026: tablet handoff

TAB_P10 replaces A1/C67 as the active physical acceptance target; both phones
are unavailable after water damage. Their existing receipts are historical,
and the paired ARM32/ARM64 *compiler* lanes remain maintained.

A separate Cat Food tablet planner request now must emit
`tablet / TAB_P10 / arm64-v8a / required / not_run`. The historical
phone/C67 plan remains for the present independent AICI paired-ABI interface.
This preserves both ABIs but does not equate C67 physical results with TAB_P10.
The read-only `select-tablet-apk.sh` checks a supplied Cat Food observation,
plan and matching native source-binding record and reports ELIGIBLE_ONLY. Its
fixtures are synthetic, and it neither signs nor installs an APK.

The successor source head needs fresh hosted core, HTTP/2, Idriç, signed native
and repeat-packaging checks; previous successful runs qualify only the former
source head. TAB_P10 inventory, actual APK install/launch, signed update
retention, IME/rotation, live provider, trust, performance, and independent
policy/producer admission remain NOT_RUN or BLOCKED. No release promotion.
