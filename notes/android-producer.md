# A1 native producer boundary

Cat Food owns the device profile. The current A1 conversation target is
`isomorphisms/catfood@7427a776a1fa689a6764478b572e270496768711`,
`android/devices/miro-a1.md` (active PR source; not a new physical receipt).
FastChat uses that target without copying its hardware inventory.

This experiment uses NativeActivity, native state/layout, and Android
Canvas/Paint text, following the math-characters precedent named in
`reference-code/renderers.md`. Android shapes and rasterizes the text.
UTF-8 stored windows become bounded UTF-16 windows for JNI `NewString`;
the code does not send four-byte Unicode through modified UTF-8 `NewStringUTF`.
Window content bounds apply to drawing and hit testing. The small composer
has touch keys and Unicode clipboard paste. It is a minimal experiment;
IME composition, selection and accessibility need further product work.

The Canvas surface bridge `ANativeWindow_toSurface` is introduced at API 26.
Consequently this artifact explicitly uses API 26 and `minSdk=26`. It does
not claim API 21 compatibility. This is an application symbol requirement,
not a decision to equate the build floor with the A1's current physical API.

`scripts/build-a1-native.grease` compiles first-party C and the NDK's existing
NativeActivity glue through the owned ICK compiler. Its input compiler is
source-built at `dilapidated-shed/ick@c2b84a381d23bf5d0b5153e24be51a52e78a022e`.
The ARM job 112659050603 passed in run 37580514958; compiler artifact 11464479465
has ZIP SHA-256 `5c2b3a8a42c797e370f254e6e8c13913a855eefc9e5048f84922218555a539eb`.
Clang receives only `-x assembler` input and platform link inputs; it never
compiles FastChat C. NDK r29 is pinned to `29.0.14206865`. The payload is
stripped before publication. The receipt preserves source/compiler/payload
digests, sizes, ELF identity and imports.

Grease's inherited executable entrypoint is `_bin/cxx-asan/ysh`, built from
its authoritative source gitlink `5651cf97a1b5042f24f14112a7ade9a1518eb0bc`.
The workflow selects artifact `11419782646` from receipt run `37476471876`;
the executable SHA-256 is
`7e31cd05b7a9d8fb2a4a9e003a7f3fcb0159138506d17f0fb28da8cbe22aa85c`.
This is the Grease implementation boundary, not a stock-shell substitution.
That temporary artifact expires 2026-10-13; a later producer must restore
this same source/runtime or update and qualify an explicit replacement.

Signed packaging runs `scripts/package-a1.grease` through the generic
`isomorphisms/android-NDK@7c61ee43e75f7c2dab9288edb0e10055898b36e6`
packager. Supply its explicit package/version/API/keystore/alias/password/
certificate profile (see that repository's `apk/README.md`). The profile must
agree with this branch's manifest. The packager verifies the finished APK's
signer, launcher, payload and absence of DEX. No fallback signer is generated.

Current execution status: native build pending; signed APK BLOCKED by absent
explicit signer inputs; physical A1 execution BLOCKED/NOT_RUN. A producer ELF
or host test does not establish A1 runtime acceptance. A physical receipt
must bind the exact source, stripped payload and signed APK digest to the
observed A1 instance, launch, composer, completion, cancellation, loss/retry,
and restart/replay results. The runtime log's `completed_post_ms` measures
Canvas posting, not proof of physical first-visible presentation.
