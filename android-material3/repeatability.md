# Repeatable M3 build stages

Flexible Pipes owns reusable orchestration; FastChat supplies this application
recipe. The exact-head workflow `.github/workflows/android-material3.yml` is the
reproducible candidate runner for this branch, with separate host provisioning,
semantic/controller tests, instrumented-test compilation, signed package validation,
and artifact export. Runtime/physical acceptance remains a separate stage.

The inspected Flexible Pipes `scripts/android-producer-stage` composes the
native ICK/NDK producer gate. Its default no-DEX/native packaging operation is
incompatible with this explicitly requested Kotlin/Compose comparison. Do not
switch its no-DEX flag off and call the result a qualified native producer, or
feed the M3 artifact into the Crystal operation. This branch does not alter that
global operation or policy.

A future registered `fastchat-material3-a1-comparison` operation should bind:

- this source/head and `android-material3` recipe;
- the explicit M3 comparison exception, independently from native producer rules;
- JDK/Gradle/AGP/Kotlin/Compose/SDK pins and Gradle verification metadata;
- stable package/version/signer and actual APK/native ABI/strip validation;
- semantic fixture outcomes, controller boundary outcomes and test compilation;
- the finished APK digest and evidence-stage receipt;
- separate installation, launch, text/IME, and physical measurement obligations.

No such registered operation is claimed qualified or dispatched here. Host build
results, candidate artifact export, and future Cat Food delivery remain distinct.
No user-facing shell script is needed for the current APK; the standard Gradle
wrapper is unchanged upstream build-system plumbing. Kitchen remains the owner
for any later pasteable physical-acceptance procedure.
