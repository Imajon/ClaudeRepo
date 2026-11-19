// node vive.js jeu.be [...] : memoire VIVANTE d'un jeu Berry (apres chargement et debut(), ramasse-miettes force), au verificateur (64 bits : x 0,6 environ pour la console)
const fs = require('fs'), { execFileSync } = require('child_process');
const V = require('path').join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');
const vive = src => { const v = JSON.parse(execFileSync(V, [], { input: src + '\nimport gc\nmaj = def (dt) gc.collect() raise "mesure", str(gc.allocated()) end\n' })); const m = /mesure: (\d+)/.exec(v.erreur || ''); return m ? +m[1] : 'ERR ' + (v.erreur || '').slice(0, 80); };
module.exports = { vive };
if (require.main === module) for (const f of process.argv.slice(2)) { const m = vive(fs.readFileSync(f, 'utf8')); console.log(f.padEnd(40), m, typeof m === 'number' ? `~${Math.round(m * 0.6 / 1000)} Ko console` : ''); }
