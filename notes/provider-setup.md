# Private provider setup

The existing composer, durable attempts and provider identity are retained.
The new setup panel edits six bounded fields using platform EditTexts in a
separate application panel. Native Save/Back hit targets remain outside it.
This extends the existing NativeActivity/JNI path without application DEX.

`src/settings.c` owns validation and private-file effects. One mode-0600
`provider.settings.tsv` contains routing, request format, CA selection and the
optional authorization header. A replacement writes a private temporary file,
synchronizes it, renames it and synchronizes the containing directory. Only
the committed name is read. Malformed, overlong, nonprivate, linked or duplicate
settings fail closed. A failed save barrier disables further submission until
a successful explicit save or restart resolves the settings.

Credentials stay in this private capability and the existing transport engine.
The existing ATTEMPT_SUBMIT record contains public provider/model identity only.
The setup panel masks the authorization field and uses FLAG_SECURE. Neither
setup values nor headers are logged or included in saved-instance state.

`system` builds a bounded private PEM bundle from Android's system certificate
directory, preferring the Android 14 Conscrypt APEX directory. This avoids
assuming that Android's certificate filenames use OpenSSL's hash convention.
The existing TLS engine still verifies peer certificates and hostnames. No
verification bypass, downloaded CA bundle or network fallback is added.
Actual phone trust-store access remains a physical acceptance requirement.

New private settings take precedence over the existing `transport.profile.tsv`
and `authorization.header` interface. Those legacy files are still read when
the new settings file is absent. They are not rewritten or migrated silently.
With neither configuration, the UI explicitly identifies the test responder.

Drafts use the same atomic private-file boundary, storing at most 4096 UTF-16
units outside the conversation journal. The main loop checks once a second,
compares bytes with its bounded checkpoint and writes only changes. Stop and
Setup also checkpoint. Submission clears the draft after the durable user
event is accepted. Named deterministic acceptance stores never read or write
ordinary settings or drafts.

The host suite exercises real private-file replacement, incomplete writes left
under the temporary name, malformed and duplicate settings, credential-header
injection rejection, draft persistence and private modes.
The Idriç module records the public/private type boundary; its typecheck is
separate from native Android and physical acceptance.
