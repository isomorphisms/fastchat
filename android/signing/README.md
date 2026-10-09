# Stable public comparison signer

The FC-U1 native candidate retains the arena application's existing package ID
`org.isomorphisms.fastchat.diskstreaming.arena`, with versionCode 4 and
versionName `0.4-candidate`. It uses the same persistent public certificate
below. Its pinned generic packager uses source-epoch APK v2+ signing for the
API 26 candidate. Hosted packaging is separate from physical replacement
installation, which remains NOT_RUN.

The earlier two experimental package IDs are independent:
`org.isomorphisms.fastchat.diskfirst` and
`org.isomorphisms.fastchat.diskstreaming`. Each starts at versionCode 1 and keeps
its ID, this certificate and a nondecreasing versionCode on later updates.

`fastchat-comparison-dev.jks` is the existing deliberately public FastChat
development key, copied byte for byte from `android-material3/signing/`
at FastChat `c5fc80ea3350186b4af4960de1fb236acdc6b854`. It was not regenerated.
Reusing the same public certificate for these distinct comparison packages is
explicit; this does not change the material3 package's identity.

- Store type: JKS.
- Alias: `fastchat-material3-dev`.
- Public store/key password: `fastchat-development`.
- Certificate SHA-256: `aa9151e3922fa4795987c3655bc7aa4712620f1d94c24a2075acd7b29d8ac86e`.

This public test identity is not a production/store signing identity. The existing
android-NDK packager verifies the certificate before and after signing. Physical
replacement installation must retain data and must not uninstall to bypass a
signature or downgrade error. Replacement/install/launch/IME/replay acceptance
on MIRO A1 remains separately `BLOCKED/NOT_RUN` until an exact-artifact receipt
exists.
