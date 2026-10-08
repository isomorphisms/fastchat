# Public development identity

Package: `org.isomorphisms.fastchat.material3`, initially versionCode 1.

`fastchat-material3-dev.jks` is a deliberately public development key, created
once for this comparison package. It is reused for both debug and release builds;
no runner-generated debug signer is used. Never use it for production/store signing.

- alias: `fastchat-material3-dev`
- store/key password: `fastchat-development` (public, not a secret)
- certificate SHA-256: `aa9151e3922fa4795987c3655bc7aa4712620f1d94c24a2075acd7b29d8ac86e`

Future updates keep this package, signer, and a nondecreasing versionCode.
Physical replacement-install acceptance is still required; never uninstall to
work around a signature or downgrade failure.
