// Test moteur rapide — lancer avec: electron test-motor-quick.js
// (ou depuis npm start après avoir changé main dans package.json temporairement)
const { app } = require('electron');
const { SerialPort } = require('serialport');

const PORT = process.argv[2] || 'COM17';
const BAUD = parseInt(process.argv[3] || '256000');

app.whenReady().then(async () => {
  console.log(`\n=== TEST MOTEUR ${PORT} @ ${BAUD} ===\n`);

  const port = new SerialPort({ path: PORT, baudRate: BAUD, autoOpen: false });

  port.open(err => {
    if (err) { console.error('OPEN ERR:', err.message); app.quit(); return; }
    console.log('✓ Port ouvert');

    // STOP d'abord
    port.write(Buffer.from([0xA5, 0x25]));
    console.log('→ STOP envoyé');

    setTimeout(() => {
      port.set({ dtr: false, rts: true }, err2 => {
        if (err2) { console.error('SET RTS ERR:', err2.message); app.quit(); return; }
        console.log('✓ RTS=true');

        // SET_MOTOR_PWM(600)
        const pwm = 600;
        const lo = pwm & 0xFF, hi = (pwm >> 8) & 0xFF;
        let cs = 0; [0xA5, 0xF0, 0x02, lo, hi].forEach(b => cs ^= b);
        port.write(Buffer.from([0xA5, 0xF0, 0x02, lo, hi, cs]));
        console.log(`→ SET_MOTOR_PWM(${pwm}) envoyé`);

        // Compter les bytes reçus pendant 3s
        let count = 0;
        port.on('data', d => { count += d.length; });

        setTimeout(() => {
          console.log(`\nBytes reçus en 3s : ${count}`);
          if (count > 1000) console.log('✓ MOTEUR OK — données scan reçues');
          else              console.log('✗ MOTEUR KO — pas de données (moteur immobile ?)');
          port.set({ rts: false }, () => port.close(() => app.quit()));
        }, 3000);
      });
    }, 500);
  });
});
