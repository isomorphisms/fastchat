# FC-D1 — disk-first comparison

Independent branch sun/disk-first-icky-c-lua. Exact fetched main base:
b49f0f7083d2d034ba76b910a43592d6d07bd18b. Preserve it unmerged.

Provider chunks are written directly to the local store. The completed-only
policy reads no response body during generation. After completion is admitted
and stored, it reads the sealed response through bounded windows. Failure,
cancellation and uncertainty are explicit status states.

Store, fake protocol and NativeActivity Canvas/Paint presentation are shared
with FC-S1. Only visibility policy and package identity differ. The RAM sink
is a test/control here, not another branch.

Implementation and host receipts are in [the C pass note](c-pass.md).
The [shared corpus](../qualification/comparison-corpus.tsv) has an executable
runner: core-tests corpus OUTPUT_DIRECTORY. Compare its response and canonical
journal bytes between independently compiled siblings.

Store/admission/framing host tests pass. Native C producer qualification remains
pending; Lua has not begun. [The A1 producer recipe](android-producer.md) uses
the existing android-NDK packaging boundary. Signed APK requires explicit signer
inputs. Physical A1 acceptance and Android memory/presentation measurements are
BLOCKED/NOT_RUN.
