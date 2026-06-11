/**
 * RPLiDAR A2 — Diagnostic v2 (test DTR + RTS séparément)
 * Usage: node lidar-a2-diag-v2.js COM3 256000
 */
const { SerialPort } = require('serialport');

const PORT = process.argv[2];
const BAUD = parseInt(process.argv[3]) || 115200;
const DELAY = ms => new Promise(r => setTimeout(r, ms));

function hex(buf) {
  return [...buf].map(b => b.toString(16).padStart(2,'0').toUpperCase()).join(' ');
}

const CMD_STOP     = Buffer.from([0xA5, 0x25]);
const CMD_SCAN     = Buffer.from([0xA5, 0x20]);
const CMD_GET_INFO = Buffer.from([0xA5, 0x50]);

function makeMotorPwm(pwm) {
  const lo = pwm & 0xFF, hi = (pwm >> 8) & 0xFF;
  let cs = 0; cs ^= 0xA5; cs ^= 0xF0; cs ^= 0x02; cs ^= lo; cs ^= hi;
  return Buffer.from([0xA5, 0xF0, 0x02, lo, hi, cs]);
}

function setSignals(port, opts) {
  return new Promise(res => port.set(opts, err => {
    if (err) console.log(`    ✗ set(${JSON.stringify(opts)}) erreur: ${err.message}`);
    else     console.log(`    ✓ set(${JSON.stringify(opts)}) OK`);
    res();
  }));
}

async function testMotorSignal(port, label, opts) {
  console.log(`\n─── Test moteur via ${label} ───────────────────────`);

  // Couper d'abord tout
  await setSignals(port, { dtr: false, rts: false });
  await DELAY(500);

  // Activer le signal testé
  console.log(`    Activation ${label}...`);
  await setSignals(port, opts);
  await DELAY(300);

  // Envoyer SET_MOTOR_PWM 600
  const pkt = makeMotorPwm(600);
  console.log(`    → SET_MOTOR_PWM(600): ${hex(pkt)}`);
  await new Promise(res => port.write(pkt, res));
  await DELAY(300);

  // Lancer le scan et compter les bytes
  console.log(`    → CMD SCAN...`);
  let rx = 0;
  port.once('data', d => { rx += d.length; });
  await new Promise(res => port.write(CMD_SCAN, res));
  await DELAY(2000);

  // Compter les bytes de scan (on re-écoute)
  const listener = d => { rx += d.length; };
  port.on('data', listener);
  await DELAY(2000);
  port.off('data', listener);

  console.log(`    Bytes reçus : ${rx}`);
  if (rx > 50)  console.log(`    ✓✓ MOTEUR ACTIF via ${label} ! Scan reçu.`);
  else if (rx > 7) console.log(`    ~ Descripteur seulement — moteur trop lent ou mauvais signal`);
  else          console.log(`    ✗ Pas de données scan`);

  // Stopper
  await new Promise(res => port.write(CMD_STOP, res));
  await DELAY(200);
  await setSignals(port, { dtr: false, rts: false });
  await DELAY(500);
}

async function main() {
  if (!PORT) {
    const ports = await SerialPort.list();
    console.log('\nPorts disponibles:');
    ports.forEach(p => console.log(` ${p.path}  ${p.manufacturer || ''}`));
    console.log('\nUsage: node lidar-a2-diag-v2.js <PORT> [BAUDRATE]');
    return;
  }

  console.log(`\n══════════════════════════════════════════════`);
  console.log(` RPLiDAR A2 — Diagnostic v2 (DTR vs RTS)`);
  console.log(` Port: ${PORT}  Baud: ${BAUD}`);
  console.log(`══════════════════════════════════════════════`);

  const port = new SerialPort({ path: PORT, baudRate: BAUD, autoOpen: false });
  port.on('error', err => console.error('Erreur port:', err.message));

  await new Promise((res, rej) => port.open(err => err ? rej(err) : res()));
  console.log('✓ Port ouvert\n');

  // Vérifier que le firmware répond bien
  let rxInfo = 0;
  const infoListener = d => { rxInfo += d.length; };
  port.on('data', infoListener);
  await new Promise(res => port.write(CMD_GET_INFO, res));
  await DELAY(500);
  port.off('data', infoListener);
  console.log(`GET_INFO → ${rxInfo} bytes reçus ${rxInfo >= 27 ? '✓ firmware OK' : '✗ pas de réponse'}`);

  // ── Test 1 : DTR seul ────────────────────────────────────────────────────
  await testMotorSignal(port, 'DTR seul', { dtr: true, rts: false });

  // ── Test 2 : RTS seul ────────────────────────────────────────────────────
  await testMotorSignal(port, 'RTS seul', { dtr: false, rts: true });

  // ── Test 3 : DTR + RTS ───────────────────────────────────────────────────
  await testMotorSignal(port, 'DTR + RTS', { dtr: true, rts: true });

  // ── Fermeture propre ─────────────────────────────────────────────────────
  await new Promise(res => port.write(CMD_STOP, res));
  await DELAY(100);
  await new Promise(res => port.close(res));

  console.log('\n══════════════════════════════════════════════');
  console.log(' Diagnostic v2 terminé');
  console.log(' → Le signal qui a donné >50 bytes est le bon');
  console.log('══════════════════════════════════════════════\n');
}

main().catch(err => { console.error('Erreur fatale:', err.message); process.exit(1); });
