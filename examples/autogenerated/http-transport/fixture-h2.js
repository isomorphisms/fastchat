// Foreign Node HTTP/2 fixture: protocol/TLS are Node's maintained implementation.
'use strict';
const http2 = require('node:http2');
const fs = require('node:fs');
const [cert, key, port] = process.argv.slice(2);
const server = http2.createSecureServer({cert: fs.readFileSync(cert), key: fs.readFileSync(key), allowHTTP1: false});
let connection = 0;
server.on('session', session => {
  const identity = ++connection;
  session.fixtureIdentity = identity;
  console.log(`session\t${identity}\t${session.socket.alpnProtocol}`);
  session.on('error', () => {});
});
server.on('stream', (stream, headers) => {
  stream.on('error', () => {});
  const path = headers[':path'];
  console.log(`request\t${stream.session.fixtureIdentity}\t${stream.id}\t${path}`);
  let body = '';
  stream.on('data', bytes => { body += bytes; if (body.length > 65536) stream.close(http2.constants.NGHTTP2_CANCEL); });
  stream.on('end', async () => {
    if (headers[':method'] !== 'POST' || headers['content-type'] !== 'application/json' || body !== '{"prompt":"fixture"}') {
      stream.respond({':status': 400}); stream.end(); return;
    }
    stream.respond({':status': 200, 'content-type': 'text/event-stream'});
    const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
    if (path === '/reset') { await wait(30); stream.close(http2.constants.NGHTTP2_INTERNAL_ERROR); return; }
    if (path === '/close') { stream.write('data: partial\n\n'); await wait(30); stream.session.destroy(); return; }
    if (path === '/incomplete') { stream.end('data: partial\n\n'); return; }
    const count = path === '/large' ? 400 : 3;
    for (let i = 0; i < count && !stream.destroyed && !stream.closed; i++) {
      const record = path === '/large' ? `data: ${'x'.repeat(1000)}\n\n` : `data: chunk-${i}\r\n\r\n`;
      // Split SSE lines and CRLF across application writes, with deterministic data.
      if (!stream.write(record.slice(0, 3))) await new Promise(resolve => stream.once('drain', resolve));
      if (!stream.write(record.slice(3))) await new Promise(resolve => stream.once('drain', resolve));
      if (path !== '/large') await wait(30);
    }
    if (!stream.destroyed && !stream.closed) stream.end('data: [DONE]\n\n');
  });
});
server.listen(Number(port), process.env.FC_FIXTURE_BIND || '127.0.0.1', () => console.log(`ready\t${port}`));
process.on('SIGTERM', () => { server.close(); process.exit(0); });
