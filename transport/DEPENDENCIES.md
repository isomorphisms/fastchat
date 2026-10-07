# Exact dependency and provenance record

Date: 2026-10-07. Sources are external pinned builds, not floating system TLS or
protocol libraries. Copyright/license notices are retained in `licenses/` and
included with native artifacts. No HPACK, QPACK, QUIC, congestion-control or TLS
implementation was authored here.

| Component | Version and exact source | License | Incorporation |
| --- | --- | --- | --- |
| curl | 8.20.0; `a05f34973e6c4bb629d018f7cb51487be1c904d8` | curl | Static HTTP client/connection-filter engine; exact local patch below. |
| OpenSSL | 3.5.5; `67b5686b4419b4cb8caa502711c41815f5279751` | Apache-2.0 | Static TLS/crypto, no apps/modules/legacy/tests. |
| nghttp2 | 1.67.1; `49908f992027821912b96a13898b665a35aa3a0a` | MIT | Static H2/HPACK, library-only build. |
| ngtcp2 (H3 only) | 1.18.0; `fefb3f16d56e1ee306b7067cab1630e14878e6fa` | MIT | QUIC and OpenSSL crypto adapter. |
| nghttp3 (H3 only) | 1.11.0; `f3eb315feda478cdb4919720a7961c0321e1bd89` | MIT | HTTP3/QPACK. |
| sfparse (nghttp3) | `7eaf5b651f67123edf2605391023ed2fd7e2ef16` | MIT | nghttp3's exact submodule. |
| NDK runtime/startup | r29 / `29.0.14206865`; LLVM toolchain source `5e96669f06077099aa41290cdb4c5e6fa0f59349` | Apache-2.0 WITH LLVM-exception; retained sysroot BSD/Apache notices | Native compiler builtins/startup as selected by the NDK linker; runtime archive hash is retained. |

Upstreams: [curl](https://github.com/curl/curl),
[OpenSSL](https://github.com/openssl/openssl),
[nghttp2](https://github.com/nghttp2/nghttp2),
[ngtcp2](https://github.com/ngtcp2/ngtcp2),
[nghttp3](https://github.com/ngtcp2/nghttp3).

`_/transport/curl-small-windows.patch` is the complete incorporated source diff:
H2 connection window 256 KiB and stream window 64 KiB; H3 stream window 64 KiB,
initial 32 KiB, connection window 256 KiB, 16 peer streams and 64 KiB send-buffer
ceiling; internal `Curl_retry_request` replay is stopped with an explicit error.
The source preparation and build recipes compare the entire diff with this file.
These changes affect policy/limits, not protocol implementation. Its SHA-256
and all native glue input digests accompany the build evidence.

## Selection and inspected alternatives

Before adding machinery, this work inspected curl's maintained H2/H3 paths,
nghttp2 headers/library configuration, ngtcp2's OpenSSL QUIC interface and
nghttp3's library configuration. Relevant curl seams are `lib/cfilters.h`,
`lib/cf-socket.h`, `lib/vtls/openssl.c`, `lib/http2.c`,
`lib/vquic/curl_ngtcp2.c`, `lib/vquic/curl_nghttp3.c`, `lib/transfer.c` and
the upstream HTTP3 build documentation. They preserve TLS/network layering
without implementing new protocol state machines. OpenSSL's maintained QUIC TLS
callbacks avoid a separate patched TLS fork.

The current ICU source was inspected as the user's socket/HTTP client reference;
its maintained transport is HTTP/1, so it does not provide an existing qualified
H2/H3 implementation. Its historical curl transport does not establish current
ICU H2/H3 support. No ICU source is incorporated.

OkHttp's application/SSE ownership was reviewed through the native Maid reference
below. It is a useful architecture reference but would introduce the JVM/Android
application build path rather than this native ICK/NDK lane. Cronet was considered
as an Android maintained H2/H3 stack; this experiment avoids incorporating its
larger Chromium/JNI distribution and does not make an unmeasured size claim about
it. Quiche was evaluated from curl's current upstream HTTP3 build guidance; the
selected maintained ngtcp2/nghttp3 path keeps the foreign dependencies in C and
uses a proven ARMv7 NDK build here. No quiche or MsQuic ARMv7 rejection is claimed.

Client references inspected, with no copied implementation:

- HatsyRei/maid-native `830003f95e74cd0e2aa4a40f1f6e5817434ec6c6`:
  `data/remote/OpenAiClient.kt`, `JsonStreamBody.kt` and cancellation ownership.
  Provider JSON/SSE lives above HTTP; request body copies can dominate memory.
- sigoden/aichat `82976d349ad97ac9aae0655ad631dace5e2a6385`:
  `src/client/stream.rs` and its SSE/JSON/UTF-8 boundaries. Its unbounded sender
  is not copied into this transport.
- FastChat's README, process architecture, reference README and supplements;
  unchanged same-repository core/tests from the exact reference head recorded
  in `transport/README.md`. No project license is invented for that owned code.

## Toolchains and build consequences

Android NDK r29 (`Pkg.Revision=29.0.14206865`) compiles and links handwritten C
glue and the pinned native libraries. A1 uses
`armv7a-linux-androideabi24-clang`, Android API 24, `armeabi-v7a`, softfp/Bionic.
The runtime profile is Android 14/API 34, 32-bit, 4 KiB pages. The DSO's only
dynamic requirements are Android `libc.so` and `libdl.so`; foreign TLS/protocol
libraries are static and their symbols hidden. No Java, Gradle, d8, RefC or
generated-C lowering is used.

ICK was inspected at `7afb1820cd59c0c51d19a7e37902c14f4466f442`: the evaluated
installed snapshot offers AArch64 target tools, not a qualified ARM32/Bionic
compiler/linker for this complete static library build. That exact available
target/runtime gap justifies NDK here; it is not a general claim that ICK cannot
compile ARM32. Idriç is pinned at
`ff4d852862a3942592f8ade9afde8d409d9803be`; native-module probes and host Chez
FFI qualification are retained, with no ARM/Thumb execution claim.

Host tools: CMake 4.4.4, Node v24.19.0 (H2 fixture), host `openssl` 3.0.13
(ephemeral certificate generation), Python 3.12 (H3 fixture only), GNU make/Perl,
and Grease. The evaluated Grease runtime binary has SHA-256
`7e31cd05b7a9d8fb2a4a9e003a7f3fcb0159138506d17f0fb28da8cbe22aa85c` and is the
inherited implementation invoked through its legacy applet entry point in the
execution receipt. Its source-to-binary correspondence was not re-established
here. Consumer recipes are named/invoked as Grease; no new shell-language fallback
is introduced. Build output records the actual compiler version and ELF headers.

All linked libraries are built with size-oriented CMake settings and PIC.
Compression, proxy, cookies, HSTS, Alt-Svc, libpsl, IDN and threaded DNS are disabled.
H3 adds ngtcp2/nghttp3 to the same native configuration, retaining H2 capability
for a direct incremental comparison; H3 requests themselves are `3ONLY` and
never fall back. 0-RTT is not enabled. The original project has no APK/native
transport baseline, so DSO file bytes are measured; APK growth is NOT_RUN.

## Fixture-only Python dependencies

H3 fixture dependencies are not embedded in the A1 library. Exact versions,
wheel hashes and license expressions are recorded in
`fixture-requirements.txt` and `fixture-licenses.tsv`. Installation on the build
host uses pip's `--require-hashes`; the recorded hashes are for the evaluated
CPython 3.12 Linux x86-64 wheels. Other host platforms need separately pinned
compatible wheels. All fixture behavior delegates QUIC/TLS/QPACK to aioquic
1.3.0 and pylsqpack 0.3.24. Their notices are retained separately.
