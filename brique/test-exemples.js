// node test-exemples.js : chaque exemple tourne 10 s dans verif (meme Berry que la console) sans erreur, et le choix par mots-cles tombe juste
const assert = require('assert'), { execFileSync } = require('child_process'), path = require('path'), fs = require('fs');
const { EXEMPLES, choisir } = require('./exemples.js');
const VERIF = path.join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');

if (fs.existsSync(VERIF)) for (const e of [...EXEMPLES, ...require('./exemples.js').systemes.EXEMPLES]) {
  const v = JSON.parse(execFileSync(VERIF, [], { input: e.code }));
  assert(v.ok, `${e.f} : ${v.erreur}`);
  assert(v.ops <= 1200 && v.memoire <= 70000, `${e.f} : trop lourd (${v.ops} dessins, ${v.memoire} octets)`);
} else console.log('verif non compile : exemples non joues');

const premier = (...t) => (choisir(...t)[0] || {}).f;
assert.strictEqual(premier('un jeu de bagarre entre deux ninjas'), '9-combat.be');
assert.strictEqual(premier('une course de voitures qui dérape'), '1-circuit.be');
assert.strictEqual(premier('du ski dans la neige avec des figures'), '8-glisse.be');
assert.strictEqual(premier('un serpent à deux consoles'), '7-serpent.be');
assert.strictEqual(premier('ajoute un deuxième robot', [], '# Combat de robots'), '9-combat.be');   // la suite d'un jeu : son nom suffit
assert.deepStrictEqual(choisir('bonjour'), []);
const sys = t => require('./exemples.js').systemes.choisir(t).map(e => e.f);
assert(['piste.be', 'derapage.be'].every(f => sys('un drift dans des épingles au chrono').includes(f)), 'drift + epingles : piste et derapage');
assert(sys('une course en spirale').includes('piste.be'), 'spirale : piste');
assert(choisir('un jeu de saut avec des plateformes et un labyrinthe et du ski').length === 2, '2 exemples au plus');
console.log(`ok : ${EXEMPLES.length} exemples, ${require('./exemples.js').systemes.EXEMPLES.length} systemes`);
