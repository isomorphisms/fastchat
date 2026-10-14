'use strict';
// Foreign protocol harness; does not replace the Idriç conversation core.
const {spawn, spawnSync} = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const [binary, protocol, directory, python, coreBinary] = process.argv.slice(2);
fs.mkdirSync(directory, {recursive: true});
const cert = path.join(directory, 'fixture-cert.pem'), key = path.join(directory, 'fixture-key.pem');
const certificate = spawnSync('openssl', ['req', '-x509', '-newkey', 'rsa:2048', '-nodes',
  '-keyout', key, '-out', cert, '-days', '2', '-subj', '/CN=localhost',
  '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1']);
assert.equal(certificate.status, 0, certificate.stderr.toString());
const port = process.env.FC_FIXTURE_PORT || (protocol === '2' || protocol === 'candidate' ? '9442' : '9443');
const script = path.join(__dirname, protocol === '2' || protocol === 'candidate' ? 'fixture-h2.js' : 'fixture-h3.py');
const fixture = spawn(protocol === '2' || protocol === 'candidate' ? process.execPath : python, [script, cert, key, port]);
let serverLog = '';
fixture.stdout.on('data', bytes => { serverLog += bytes; });
fixture.stderr.on('data', bytes => { serverLog += bytes; });
const run = async (name, first, second, mode) => {
  const child = spawn(binary, [`https://localhost:${port}`, cert, protocol, first, second, mode]);
  let output = '', errors = '';
  child.stdout.on('data', bytes => { output += bytes; });
  child.stderr.on('data', bytes => { errors += bytes; });
  const status = await new Promise(resolve => child.on('close', resolve));
  fs.writeFileSync(path.join(directory, `${name}.tsv`), output);
  assert.equal(status, 0, `${name}: ${errors}\n${output}`);
  const events = output.split('\n').filter(line => line.startsWith('event\t')).map(line => {
    const columns = line.split('\t');
    return {request: Number(columns[1]), attempt: Number(columns[2]), kind: Number(columns[3]), text: columns[4]};
  });
  const metricLine = output.split('\n').find(line => line.startsWith('metrics\t'));
  const metrics = Object.fromEntries(metricLine.split('\t').slice(1).map(field => field.split('=')));
  assert.equal(Number(metrics.http), protocol === '2' ? 3 : 30, 'negotiated protocol');
  assert.ok(Number(metrics.peak_queue) <= 65536, 'bounded application buffering');
  if (mode !== 'retry' && first !== '/close') assert.equal(Number(metrics.connections), 1, 'one shared connection');
  const terminal = request => events.filter(event => event.request === request && event.kind >= 3);
  if (mode === 'cancel') { assert.equal(terminal(1)[0].kind, 4); assert.equal(terminal(2)[0].kind, 3); }
  else if (mode === 'retry') {
    assert.deepEqual(terminal(1).map(event => [event.attempt, event.kind]), [[1, 6], [3, 3]]);
    assert.deepEqual(terminal(2).map(event => [event.attempt, event.kind]), [[2, 6], [4, 3]]);
    assert.equal(Number(metrics.connections), 2, 'one reconnection after loss');
  } else if (first === '/reset' || first === '/incomplete') {
    assert.equal(terminal(1)[0].kind, 6); assert.equal(terminal(2)[0].kind, 3);
  } else {
    assert.equal(terminal(1)[0].kind, 3); assert.equal(terminal(2)[0].kind, 3);
    assert.equal(events.filter(event => event.request === 2 && event.kind === 2).length, 3);
  }
  if (mode === 'slow') {
    assert.ok(Number(metrics.pauses) > 0, 'receive pause exercised');
    assert.equal(events.filter(event => event.request === 1 && event.kind === 2).length, 400);
    assert.ok(events.findIndex(event => event.request === 2 && event.kind === 3) <
              events.findIndex(event => event.request === 1 && event.kind === 3), 'slow request does not kill peer');
  }
  console.log(`PASS ${name}`);
};
(async () => {
  try {
    const deadline = Date.now() + 10000;
    while (!serverLog.includes('ready\t')) {
      assert.ok(Date.now() < deadline, `fixture did not start: ${serverLog}`);
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    if (protocol === 'candidate') {
      const child = spawn(binary, [`https://localhost:${port}`, cert, directory]);
      let output = '', errors = '';
      child.stdout.on('data', bytes => { output += bytes; });
      child.stderr.on('data', bytes => { errors += bytes; });
      const status = await new Promise(resolve => child.on('close', resolve));
      fs.writeFileSync(path.join(directory, 'boundary.log'), output + errors);
      assert.equal(status, 0, errors + output);
      assert.equal(serverLog.split('\n').filter(line => line.startsWith('request\t')).length, 11, 'no hidden POST retry');
      process.stdout.write(output);
      return;
    }
    await run('multiplex', '/stream', '/stream', 'normal');
    await run('cancel', '/stream', '/stream', 'cancel');
    await run('reset', '/reset', '/stream', 'normal');
    await run('loss-retry', '/close', '/stream', 'retry');
    await run('incomplete', '/incomplete', '/stream', 'normal');
    await run('slow-consumer', '/large', '/stream', 'slow');
    const requests = serverLog.split('\n').filter(line => line.startsWith('request\t'));
    assert.equal(requests.length, 14, 'no hidden resubmission of POST');
    // First requests must be distinct wire streams on the same connection.
    const first = requests[0].split('\t'), second = requests[1].split('\t');
    assert.equal(first[1], second[1]); assert.notEqual(first[2], second[2]);
    if (coreBinary) {
      const core = spawn(coreBinary, [`https://localhost:${port}`, cert, protocol]);
      let output = '', errors = '';
      core.stdout.on('data', bytes => { output += bytes; });
      core.stderr.on('data', bytes => { errors += bytes; });
      const status = await new Promise(resolve => core.on('close', resolve));
      fs.writeFileSync(path.join(directory, 'core.txt'), output + errors);
      assert.equal(status, 0, 'typed core process');
      assert.ok(output.includes('PASS FastChat core with real HTTP transport'), output + errors);
      assert.ok(!output.includes('FAIL'), output + errors);
      console.log('PASS live typed core, fresh retry and stale-attempt rejection');
    }
    if (protocol === '3' && process.env.FC_TEST_PATH_LOSS === '1') {
      await run('path-loss-retry', '/blackhole', '/stream', 'retry');
      assert.ok(serverLog.includes('blackhole\t'), 'path loss actually injected');
    }
    if (process.env.FC_FIXTURE_DROP_EVERY) assert.ok(serverLog.includes('drop\t'), 'packet loss actually injected');
    fs.writeFileSync(path.join(directory, 'fixture.tsv'), serverLog);
  } finally { fixture.kill('SIGTERM'); fs.writeFileSync(path.join(directory, 'fixture.tsv'), serverLog); }
})().catch(error => { console.error(error); process.exitCode = 1; });
