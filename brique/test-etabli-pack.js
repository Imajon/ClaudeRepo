// node test-etabli-pack.js : aller-retour chiffre, mauvaise cle refusee, fichier abime refuse
const assert = require('assert'), fs = require('fs'), os = require('os'), path = require('path'), { pack, unpack } = require('./etabli-pack');
const d = fs.mkdtempSync(path.join(os.tmpdir(), 'etabli-')); fs.mkdirSync(path.join(d, 'img'));
fs.writeFileSync(path.join(d, 'index.html'), '<p>é</p>'); fs.writeFileSync(path.join(d, 'img', 'a.bin'), Buffer.from([0, 255, 7]));
const buf = pack(d, 'cle-1'), m = unpack(buf, 'cle-1');
assert.deepStrictEqual([...m.keys()].sort(), ['img/a.bin', 'index.html']);
assert.strictEqual(m.get('index.html').toString(), '<p>é</p>'); assert.deepStrictEqual([...m.get('img/a.bin')], [0, 255, 7]);
assert.throws(() => unpack(buf, 'cle-2'));
const abime = Buffer.from(buf); abime[abime.length - 1] ^= 1; assert.throws(() => unpack(abime, 'cle-1'));
assert(!buf.includes('<p>')); console.log('ok');
