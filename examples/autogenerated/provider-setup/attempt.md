# Provider setup and private persistence

## Values and ownership

The editor supplies an HTTPS origin, endpoint path, model, request format and
optional authorization header. The existing durable attempt identity owns only
the four public routing values. A private settings capability owns credentials;
neither the journal nor presentation diagnostics receive that capability.

`public_identity : provider_settings -> attempt_identity` deliberately cannot
return the authorization header. Saving a settings replacement is an effect:
validate all fields, write one mode-0600 temporary file, synchronize it, rename
it, and synchronize its parent. A failed durability barrier disables submission.
The same private-file boundary stores the bounded, unsubmitted UTF-16 draft.

## Idriç slice and native boundary

ProviderSetup.idric describes the public/private ownership boundary in the
project's current Idriç syntax. It does not claim Android widget or file effects.
The established NativeActivity adapter continues to own Android JNI and the
existing ICK C storage/transport boundary performs file effects. No new compiler,
generated-C lowering, app DEX or application runtime is introduced.

Required verification: execute the existing Idriç fixture harness with the new
module staged; execute actual settings corruption/replacement/private-mode
cases through ICK; compile the real Android adapter through ICK. Physical IME,
system-trust export and live-provider behavior require separate phone receipts.

## Language work

Local result: the actual Idriç 94dfd99bd3e376507fedc8611053b7173b2519f0
Linux x86-64 bundle successfully typechecked `ProviderSetup.idric` with
`bin/idric --check ProviderSetup.idric`. This is typechecking only. The hosted
workflow retains its existing compiler pin and checks the module separately.

The missing product-level Idriç capability is the existing native Android JNI
and durable private-file effect interface. A future ownership transfer must
exercise the actual panel, credential persistence and transport, rather than
counting this pure type sketch as their replacement.
