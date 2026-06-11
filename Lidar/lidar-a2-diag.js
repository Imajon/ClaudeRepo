/**
 * RPLiDAR A2 — Script de diagnostic brut
 * Usage: node lidar-a2-diag.js [PORT] [BAUDRATE]
 * Ex:    node lidar-a2-diag.js /dev/ttyUSB0 115200
 *        node lidar-a2-diag.js COM3 256000
 *
 * Ce script teste chaque étape séparément et affiche tout.
 */

const { SerialPort } = require('serialport');

const PORT  = process.argv[2];
const BAUD  = parseInt(process.argv[3]) || 115200;
const DELAY = ms => new Promise(r => setTimeout(r, ms));

// ── Commandes ─────────────────────────────────────────────────────────────────
const CMD_STOP        = Buffer.from([0xA5, 0x25]);
const CMD_RESET       = Buffer.from([0xA5, 0x40]);
const CMD_GET_INFO    = Buffer.from([0xA5, 0x50]);
const CMD_GET_HEALTH  = Buffer.from([0xA5, 0x52]);
const CMD_SCAN        = Buffer.from([0xA5, 0x20]);

function makeMotorPwm(pwm) {
  // SET_MOTOR_PWM : 0xA5 0xF0 <size=2> <pwm_lo> <pwm_hi> <checksum>
  const lo = pwm & 0xFF;
  const hi = (pwm >> 8) & 0xFF;
  let cs = 0;
  cs ^= 0xA5; cs ^= 0xF0; cs ^= 0x02; cs ^= lo; cs ^= hi;
  return Buffer.from([0xA5, 0xF0, 0x02, lo, hi, cs]);
}

function hex(buf) {
  return [...buf].map(b => b.toString(16).padStart(2,'0').toUpperCase()).join(' ');
}

async function main() {
  if (!PORT) {
    console.log('\n=== Ports série disponibles ===');
    const ports = await SerialPort.list();
    ports.forEach(p => console.log(` ${p.path}  ${p.manufacturer || ''}`));
    console.log('\nUsage: node lidar-a2-diag.js <PORT> [BAUDRATE]');
    return;
  }

  console.log(`\n══════════════════════════════════════════════`);
  console.log(` RPLiDAR A2 — Diagnostic`);
  console.log(` Port   : ${PORT}`);
  console.log(` Baud   : ${BAUD}`);
  console.log(`══════════════════════════════════════════════\n`);

  const port = new SerialPort({ path: PORT, baudRate: BAUD, autoOpen: false });

  // Log toutes les données reçues
  let rxCount = 0;
  port.on('data', buf => {
    rxCount += buf.length;
    console.log(`  ← RX [${buf.length}B total=${rxCount}B]: ${hex(buf)}`);
  });
  port.on('error', err => console.error('  ✗ Erreur port:', err.message));

  // ── ÉTAPE 1 : Ouvrir le port ───────────────────────────────────────────────
  console.log('[1] Ouverture du port...');
  await new Promise((res, rej) => port.open(err => err ? rej(err) : res()));
  console.log('    ✓ Port ouvert\n');

  // ── ÉTAPE 2 : État initial des signaux ────────────────────────────────────
  console.log('[2] Lecture état initial (CTS/DSR/RI/CD)...');
  await new Promise(res => port.get((err, status) => {
    if (err) console.log('    ✗ get() erreur:', err.message);
    else     console.log('    Status:', JSON.stringify(status));
    res();
  }));
  console.log();

  // ── ÉTAPE 3 : Test RTS ────────────────────────────────────────────────────
  console.log('[3] Test RTS = false (moteur coupé)...');
  await new Promise(res => port.set({ rts: false }, err => {
    if (err) console.log('    ✗ set(rts:false) erreur:', err.message);
    else     console.log('    ✓ RTS = false OK');
    res();
  }));
  await DELAY(200);

  console.log('    Test RTS = true (démarre moteur)...');
  await new Promise(res => port.set({ rts: true }, err => {
    if (err) console.log('    ✗ set(rts:true) erreur:', err.message);
    else     console.log('    ✓ RTS = true OK  ← le moteur DOIT démarrer ici');
    res();
  }));
  await DELAY(500);
  console.log();

  // ── ÉTAPE 4 : CMD STOP (reset état firmware) ──────────────────────────────
  console.log('[4] Envoi CMD STOP (0xA5 0x25)...');
  console.log(`    → TX: ${hex(CMD_STOP)}`);
  await new Promise(res => port.write(CMD_STOP, res));
  await DELAY(300);
  console.log();

  // ── ÉTAPE 5 : GET_INFO ────────────────────────────────────────────────────
  console.log('[5] Envoi GET_INFO (0xA5 0x50)...');
  console.log(`    → TX: ${hex(CMD_GET_INFO)}`);
  rxCount = 0;
  await new Promise(res => port.write(CMD_GET_INFO, res));
  await DELAY(500);
  if (rxCount === 0) console.log('    ✗ Aucune réponse ! Mauvais baud rate ou câble ?');
  else               console.log(`    ✓ ${rxCount} byte(s) reçus`);
  console.log();

  // ── ÉTAPE 6 : GET_HEALTH ─────────────────────────────────────────────────
  console.log('[6] Envoi GET_HEALTH (0xA5 0x52)...');
  console.log(`    → TX: ${hex(CMD_GET_HEALTH)}`);
  rxCount = 0;
  await new Promise(res => port.write(CMD_GET_HEALTH, res));
  await DELAY(500);
  if (rxCount === 0) console.log('    ✗ Aucune réponse !');
  else               console.log(`    ✓ ${rxCount} byte(s) reçus`);
  console.log();

  // ── ÉTAPE 7 : SET_MOTOR_PWM à différentes valeurs ────────────────────────
  for (const pwm of [0, 400, 600, 800, 1023]) {
    const pkt = makeMotorPwm(pwm);
    console.log(`[7] SET_MOTOR_PWM pwm=${pwm}...`);
    console.log(`    → TX: ${hex(pkt)}`);
    rxCount = 0;
    await new Promise(res => port.write(pkt, res));
    await DELAY(300);
    if (rxCount > 0) console.log(`    ← Réponse reçue (${rxCount}B) — inattendu pour SET_MOTOR_PWM`);
    else             console.log(`    (pas de réponse attendue pour cette commande)`);
    console.log();
  }

  // ── ÉTAPE 8 : Tentative de scan 3 secondes ────────────────────────────────
  console.log('[8] Envoi CMD SCAN (0xA5 0x20) — écoute 3 secondes...');
  console.log(`    → TX: ${hex(CMD_SCAN)}`);
  rxCount = 0;
  await new Promise(res => port.write(CMD_SCAN, res));
  await DELAY(3000);
  console.log(`    Bytes reçus pendant 3s : ${rxCount}`);
  if (rxCount > 50)  console.log('    ✓ Données reçues ! Le scan fonctionne.');
  else if (rxCount > 0) console.log('    ~ Quelques bytes mais pas assez — moteur trop lent ?');
  else               console.log('    ✗ Aucune donnée de scan.');
  console.log();

  // ── ÉTAPE 9 : CMD STOP final ──────────────────────────────────────────────
  console.log('[9] Envoi CMD STOP final + RTS = false...');
  await new Promise(res => port.write(CMD_STOP, res));
  await DELAY(200);
  await new Promise(res => port.set({ rts: false }, res));
  console.log('    ✓ Nettoyage effectué\n');

  // ── Fermeture ─────────────────────────────────────────────────────────────
  await new Promise(res => port.close(res));
  console.log('══════════════════════════════════════════════');
  console.log(' Diagnostic terminé');
  console.log('══════════════════════════════════════════════\n');
}

main().catch(err => {
  console.error('\n✗ Erreur fatale:', err.message);
  process.exit(1);
});
