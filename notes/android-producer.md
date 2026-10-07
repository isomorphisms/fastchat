# Native producer checkpoint

The maintained slice is android/native_activity.c, built by
scripts/build-native.grease and scripts/package-native.grease. It uses native
Canvas/Paint and a platform EditText/IME composer. The superseded native_chat.c
touch-key prototype and its separate producer recipe were removed when the live
branch work was reconciled; the fresh-process replay check was preserved.

The complete functorial Icky C slice has executed host tests and passed ARM32
ICK compilation, NDK platform linking, signing and NativeActivity packaging.
Exact compiler/source/artifact qualification is recorded by notes/c-pass.md and
qualification/fc-comparison. Earlier unexecuted producer observations remain
historical, not current gate failures.

Cat Food owns the MIRO A1 profile. The application uses API 26 because its
public native-window-to-Surface bridge requires it. No device hardware inventory
is copied here. All compilation occurs on the producer.

Physical install, launch, IME, layout, cancellation and visible-frame/RSS/replay
measurements remain BLOCKED/NOT_RUN. Host reads and ELF/APK producer checks are
not physical acceptance. Neither comparison branch is merged.

