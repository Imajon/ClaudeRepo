// Encodeur OSC minimal (UDP) — pas de dépendance externe, basé sur dgram
const dgram = require('dgram');

function padTo4(len) {
  const rem = len % 4;
  return rem === 0 ? 4 : 4 - rem; // toujours au moins 1 octet de padding (chaîne C)
}

function encodeString(str) {
  const strBuf = Buffer.from(str, 'ascii');
  const pad = padTo4(strBuf.length);
  return Buffer.concat([strBuf, Buffer.alloc(pad)]);
}

function encodeFloat32(num) {
  const buf = Buffer.alloc(4);
  buf.writeFloatBE(num, 0);
  return buf;
}

function encodeInt32(num) {
  const buf = Buffer.alloc(4);
  buf.writeInt32BE(num | 0, 0);
  return buf;
}

// args: [{ type: 'i'|'f'|'s', value }]
function buildMessage(address, args) {
  const typeTags = ',' + args.map(a => a.type).join('');
  const parts = [encodeString(address), encodeString(typeTags)];
  for (const a of args) {
    if (a.type === 'f') parts.push(encodeFloat32(a.value));
    else if (a.type === 'i') parts.push(encodeInt32(a.value));
    else if (a.type === 's') parts.push(encodeString(String(a.value)));
  }
  return Buffer.concat(parts);
}

class OSCSender {
  constructor() {
    this.socket = dgram.createSocket('udp4');
    this.socket.on('error', () => {}); // éviter un crash si l'envoi échoue (port fermé, etc.)
  }

  send(host, port, address, args) {
    const msg = buildMessage(address, args);
    this.socket.send(msg, port, host, () => {});
  }

  close() {
    try { this.socket.close(); } catch (_) {}
  }
}

module.exports = { OSCSender, buildMessage };
