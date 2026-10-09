# FC-S1 — disk streaming comparison

Independent branch sun/disk-streaming-icky-c-lua. Exact fetched main base:
b49f0f7083d2d034ba76b910a43592d6d07bd18b. Preserve it unmerged.

Provider chunks are written to the same authoritative store as FC-D1.
The renderer follows committed stored byte prefixes, withholding incomplete
UTF-8 scalars. It owns one bounded window, and no private full-answer copy.
A completed response is sealed and immutable. Disk timing/backpressure,
protocol and event admission are identical to the sibling.

Only the visibility policy and APK package identity differ. Framing boundaries
are independent of text and token boundaries; the fake SSE/JSON fixture works
without credentials or any dependency on HTTP/2.

[The C pass note](c-pass.md) contains implementation and qualified host receipts.
The independently compiled siblings pass the same tests and TSV corpus;
their canonical journals and completed response files compare byte-for-byte.
Native producer qualification is pending, and Lua has not begun.
[The A1 producer recipe](android-producer.md) uses the existing android-NDK
packaging boundary. Signed APK needs explicit signer inputs. Physical A1
acceptance, device RSS and actual visible-text measurements are BLOCKED/NOT_RUN.
