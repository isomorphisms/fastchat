"""Foreign aioquic fixture. QUIC/TLS/QPACK remain in the pinned library."""
import asyncio
import sys
import os
from aioquic.asyncio import QuicConnectionProtocol, serve
from aioquic.h3.connection import H3_ALPN, H3Connection
from aioquic.h3.events import HeadersReceived, DataReceived
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.events import ProtocolNegotiated, HandshakeCompleted, StreamReset, StopSendingReceived, ConnectionTerminated

connection_count = 0

class Fixture(QuicConnectionProtocol):
    def __init__(self, *args, **kwargs):
        global connection_count
        super().__init__(*args, **kwargs)
        connection_count += 1
        self.identity = connection_count
        self.http = None
        self.requests = {}
        self.tasks = {}
        self.packets = 0
        self.blackholed = False

    def datagram_received(self, data, addr):
        if self.blackholed:
            return
        self.packets += 1
        interval = int(os.environ.get('FC_FIXTURE_DROP_EVERY', '0'))
        if self.http is not None and interval and self.packets % interval == 0:
            print(f'drop\t{self.identity}\t{self.packets}', flush=True)
            return
        super().datagram_received(data, addr)

    def transmit(self):
        if not self.blackholed:
            super().transmit()

    def quic_event_received(self, event):
        if isinstance(event, HandshakeCompleted):
            assert not event.early_data_accepted
            print(f'handshake\t{self.identity}\tearly_data=false', flush=True)
        if isinstance(event, ProtocolNegotiated):
            self.http = H3Connection(self._quic)
            print(f"session\t{self.identity}\t{event.alpn_protocol}", flush=True)
        if isinstance(event, (StreamReset, StopSendingReceived)):
            task = self.tasks.get(event.stream_id)
            if task:
                task.cancel()
            print(f"reset\t{self.identity}\t{event.stream_id}", flush=True)
        if isinstance(event, ConnectionTerminated):
            for task in self.tasks.values():
                task.cancel()
        if self.http is None:
            return
        for observed in self.http.handle_event(event):
            if isinstance(observed, HeadersReceived):
                headers = dict(observed.headers)
                self.requests[observed.stream_id] = [headers, bytearray()]
                print(f"request\t{self.identity}\t{observed.stream_id}\t{headers[b':path'].decode()}", flush=True)
                if observed.stream_ended:
                    self.begin(observed.stream_id)
            elif isinstance(observed, DataReceived):
                request = self.requests[observed.stream_id]
                request[1].extend(observed.data)
                if len(request[1]) > 65536:
                    self._quic.reset_stream(observed.stream_id, 0x10c)
                elif observed.stream_ended:
                    self.begin(observed.stream_id)

    def begin(self, stream):
        self.tasks[stream] = asyncio.create_task(self.respond(stream))

    async def respond(self, stream):
        headers, body = self.requests[stream]
        if headers[b':method'] != b'POST' or headers.get(b'content-type') != b'application/json' or body != b'{"prompt":"fixture"}':
            self.http.send_headers(stream, [(b':status', b'400')], end_stream=True)
            self.transmit()
            return
        self.http.send_headers(stream, [(b':status', b'200'), (b'content-type', b'text/event-stream')])
        self.transmit()
        path = headers[b':path']
        if path == b'/reset':
            await asyncio.sleep(.03)
            self._quic.reset_stream(stream, 0x102)
            self.transmit()
            return
        if path == b'/close':
            self.http.send_data(stream, b'data: partial\n\n', end_stream=False)
            self.transmit()
            await asyncio.sleep(.03)
            self.close(error_code=0x102, reason_phrase='fixture connection interruption')
            return
        if path == b'/blackhole':
            self.http.send_data(stream, b'data: partial\n\n', end_stream=False)
            self.transmit()
            await asyncio.sleep(.03)
            self.blackholed = True
            print(f'blackhole\t{self.identity}', flush=True)
            return
        if path == b'/incomplete':
            self.http.send_data(stream, b'data: partial\n\n', end_stream=True)
            self.transmit()
            return
        for number in range(400 if path == b'/large' else 3):
            record = (b'data: ' + b'x' * 1000 + b'\n\n') if path == b'/large' else f'data: chunk-{number}\r\n\r\n'.encode()
            self.http.send_data(stream, record[:3], end_stream=False)
            self.http.send_data(stream, record[3:], end_stream=False)
            self.transmit()
            if path != b'/large':
                await asyncio.sleep(.03)
        self.http.send_data(stream, b'data: [DONE]\n\n', end_stream=True)
        self.transmit()

async def main():
    cert, key, port = sys.argv[1:]
    configuration = QuicConfiguration(is_client=False, alpn_protocols=H3_ALPN)
    configuration.load_cert_chain(cert, key)
    server = await serve(os.environ.get('FC_FIXTURE_BIND', '127.0.0.1'), int(port), configuration=configuration, create_protocol=Fixture,
                         session_ticket_fetcher=None, session_ticket_handler=None)
    print(f'ready\t{port}', flush=True)
    try:
        await asyncio.Future()
    finally:
        server.close()

asyncio.run(main())
