# A delegated lower channel does not change FastChat semantics

The local responsibilities that must survive on any future target are creating
and tracking FastChat request/attempt identities, admitting or rejecting events
for the current attempt, representing uncertain delivery, asking for cancellation
of that particular attempt, and rendering the admitted conversation. A broker
must echo those identities independently of every TCP or QUIC connection and
wire stream. A cancelled or superseded request may still have remote work in
flight; late events must still fail admission locally.

There are several possible divisions, none implemented here:

| Local work | Delegated work | Boundary that changes |
| --- | --- | --- |
| Core, provider framing, HTTP semantics | Network access and optionally secure channel | A new HTTP driver/channel adapter; core unchanged. Curl is not assumed to accept a serial channel unchanged. |
| Core and provider framing | HTTPS, pooling, HPACK/QPACK, multiplexing, TLS, TCP/QUIC | Broker returns bounded HTTP-body chunks plus status, termination and uncertainty. Existing byte-fed framing can remain local. |
| Core and renderer | Provider account/JSON/SSE and the complete HTTP/network stack | Broker returns typed model events with request/attempt identity, plus explicit admission/cancellation/loss acknowledgments. |

The second split needs a bounded byte-message protocol with flow credit so a
slow local consumer cannot force an unbounded broker queue. The third needs the
same bound at the event boundary. All splits need explicit meanings for
submitted, acknowledged, cancelled and disconnected; loss of the broker channel
cannot turn unknown remote acceptance into successful completion. Retries remain
new FastChat attempts. None promises exactly-once remote generation.

For H2, the secure channel is a TLS byte stream above TCP. For H3, QUIC integrates
TLS 1.3 handshake/keying with packet protection, streams, recovery and congestion
control over UDP; it is not a TLS byte stream over TCP. That structural difference
belongs wholly below the Transport boundary. QUIC/TLS 1.3/HTTP3 are reasonable
broker candidates to investigate when local resources are constrained.

This job makes no claim about whether an RP2040, ATtiny or another minimal
machine can run any complete stack. No such machine was built or measured.
The actionable result is that FastChat semantics, including uncertain delivery
and stale-attempt rejection, do not require those stacks to run locally. Serial,
USB or BLE broker implementations require their own later qualification.
