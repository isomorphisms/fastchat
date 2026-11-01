# Provider identity belongs to the durable attempt

The application binds a public provider identity before any new network
attempt: backend (fixture, Chat Completions or Responses), HTTPS origin,
endpoint path and model. The existing ATTEMPT_SUBMIT journal record carries
a bounded version-1 payload; its normal checksum and durability barrier cover
the identity together with the request/attempt numbers. Retries make another
explicit user decision and bind a fresh identity before network effects.

The native composer and Retry control supply that identity. The transport
compares it with the actual origin, schema, endpoint and model before POST.
Changing those arguments after submission refuses locally instead of silently
changing the source of an answer. The reader labels a selected historical turn
with its recorded model or test responder rather than the current settings.

Authorization headers and CA paths are not copied into the identity. Origins
exclude user information, paths, query and fragment; endpoint paths exclude
query and fragment. Fields reject control characters and malformed UTF-8.
Bounds are 511 origin bytes, 255 endpoint bytes and 128 model bytes. Profiles
exceeding those bounds refuse locally. Credentials belong in the existing
private authorization capability, never in routing fields.

Old FC02 records with empty identity payloads remain readable and explicitly
PROVIDER_UNRECORDED. No migration, rewrite, fabricated historical identity or
automatic resend occurs. Older readers reject the extended attempt payload;
this change does not claim downgrade compatibility. History still presents
the most recent attempt of a request; earlier attempts remain in the journal.

This is application storage/transport work under the existing FastChat
assignment and registered fastchat-mechanical-open source envelope. It does
not implement authenticated FP dispatch, shared paired APK production, live
provider access or physical phone acceptance. Those gates remain separate.
