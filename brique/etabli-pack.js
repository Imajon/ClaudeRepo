// L'Etabli en ligne : GitHub Pages sert tout le depot en clair, donc brique/etabli/ n'y va jamais ; on y met brique/etabli.enc
// (tout le dossier, compresse puis chiffre en AES-256-GCM). Le serveur le dechiffre avec ETABLI_KEY (variable d'environnement de Render).
// Regenerer apres chaque modification de etabli/ :  node etabli-pack.js   (cle : ETABLI_KEY, sinon la ligne « etabli : ... » de 2026/code.txt)
const fs = require('fs'), path = require('path'), zlib = require('zlib'), crypto = require('crypto');
const cle = k => crypto.createHash('sha256').update(String(k)).digest();

function pack(dossier, k) {                              // [4 octets : taille de l'index][index JSON [[chemin, taille]...]][fichiers] -> gzip -> iv | tag | chiffre
  const fichiers = [];
  (function parcourir(d) { for (const e of fs.readdirSync(d, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name)))
    e.isDirectory() ? parcourir(path.join(d, e.name)) : fichiers.push([path.relative(dossier, path.join(d, e.name)).split(path.sep).join('/'), fs.readFileSync(path.join(d, e.name))]); })(dossier);
  const index = Buffer.from(JSON.stringify(fichiers.map(([p, b]) => [p, b.length]))), tete = Buffer.alloc(4); tete.writeUInt32BE(index.length);
  const iv = crypto.randomBytes(12), c = crypto.createCipheriv('aes-256-gcm', cle(k), iv);
  const clair = zlib.gzipSync(Buffer.concat([tete, index, ...fichiers.map(([, b]) => b)]), { level: 9 });
  const chiffre = Buffer.concat([c.update(clair), c.final()]);
  return Buffer.concat([iv, c.getAuthTag(), chiffre]);
}

function unpack(buf, k) {                                // -> Map chemin -> Buffer ; leve une erreur si la cle est fausse ou le fichier abime
  const d = crypto.createDecipheriv('aes-256-gcm', cle(k), buf.subarray(0, 12)); d.setAuthTag(buf.subarray(12, 28));
  const clair = zlib.gunzipSync(Buffer.concat([d.update(buf.subarray(28)), d.final()]));
  const n = clair.readUInt32BE(0), index = JSON.parse(clair.subarray(4, 4 + n)), m = new Map();
  let o = 4 + n; for (const [p, t] of index) { m.set(p, clair.subarray(o, o + t)); o += t; }
  return m;
}

module.exports = { pack, unpack };

if (require.main === module) {
  let k = process.env.ETABLI_KEY;
  if (!k) { try { k = (fs.readFileSync(path.join(__dirname, '..', '..', 'code.txt'), 'utf8').match(/^\s*etabli\s*:\s*(\S+)/mi) || [])[1]; } catch {} }
  if (!k) { console.error('Pas de cle : ETABLI_KEY ou la ligne « etabli : ... » de 2026/code.txt'); process.exit(1); }
  const sortie = path.join(__dirname, 'etabli.enc'), buf = pack(path.join(__dirname, 'etabli'), k);
  fs.writeFileSync(sortie, buf); console.log(`etabli.enc : ${unpack(buf, k).size} fichiers, ${(buf.length / 1e6).toFixed(1)} Mo`);
}
