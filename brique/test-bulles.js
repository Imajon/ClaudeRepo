// node test-bulles.js : chaque combinaison de bulles donne un programme que verif joue 10 s sans erreur (meme Berry que la console)
const assert = require('assert'), { execFileSync } = require('child_process'), path = require('path'), fs = require('fs');
const J = require('./jeux-bulles.js');
const VERIF = path.join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');
const choix = ['jeu', 'perso', 'couleur', 'monde', 'action'].map(id => J.ETAPES.find(e => e.id === id).bulles.map(b => b.id));
const { imgs, ...sans } = J.recetteComplete({}); assert.deepStrictEqual(sans, J.DEFAUT, 'sans choix : les valeurs par defaut');
assert.strictEqual(J.recetteComplete({ perso: 'img' }).perso, 'chat', 'image absente : defaut');
assert.strictEqual(J.recetteComplete({ perso: 'img', imgs: { perso: 2 } }).perso, 'img');
assert.strictEqual(J.recetteComplete({ monde: 'nimporte' }).monde, J.DEFAUT.monde, 'choix inconnu : defaut');
if (!fs.existsSync(VERIF)) { console.log('verif non compile : jeux non joues'); process.exit(0); }
let n = 0, pire = { ops: 0, memoire: 0 };
const [jeux, persos, couleurs, mondes, actions] = choix;
// toutes les combinaisons perso x monde x action (jeu et couleur tournent) : 8 x 6 x 5 = 240 programmes
for (const [i, perso] of persos.entries()) for (const [j, monde] of mondes.entries()) for (const [k, action] of actions.entries()) {
  const r = { jeu: jeux[(i + j + k) % jeux.length], perso, couleur: couleurs[(i * 3 + j + k) % couleurs.length], monde, action }, code = J.assembler(r);
  const v = JSON.parse(execFileSync(VERIF, [], { input: code }));
  assert(v.ok, `${JSON.stringify(r)} : ${v.erreur}`);
  assert(v.ops <= 600 && v.memoire <= 70000, `${JSON.stringify(r)} : trop lourd (${v.ops} dessins, ${v.memoire} octets)`);
  assert(/^# [A-Za-z0-9 ]+\n/.test(code), 'premiere ligne = nom sans accents');
  pire = { ops: Math.max(pire.ops, v.ops), memoire: Math.max(pire.memoire, v.memoire) }; n++;
}
// avec des images (icones de l'IA) a la place du personnage, des bons et des mauvais objets
for (const jeu of jeux) for (const action of actions) { const v = JSON.parse(execFileSync(VERIF, [], { input: J.assembler({ jeu, action, perso: 'img', bon: 'img', mal: 'img', imgs: { perso: 1, bon: 2, mal: 3 } }) })); assert(v.ok, `images ${jeu} ${action} : ${v.erreur}`); n++; }
// les 3 jeux avec chaque couleur
for (const jeu of jeux) for (const couleur of couleurs) { const v = JSON.parse(execFileSync(VERIF, [], { input: J.assembler({ jeu, couleur }) })); assert(v.ok, `${jeu} ${couleur} : ${v.erreur}`); n++; }
// les nouveaux jeux : chaque monde x avec / sans obstacles, puis l'ecran de fin (FIN forcee a 1 : panneau + rejouer)
const mondes2 = choix[3];
for (const jeu of jeux.filter(j => !['attrape', 'esquive', 'pluie'].includes(j))) for (const obst of ['avec', 'sans']) for (const [i, monde] of mondes2.entries()) {
  const code = J.assembler({ jeu, obst, monde, perso: persos[i], couleur: couleurs[i] });
  for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${obst} ${monde} : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 70000, `${jeu} ${obst} ${monde} : trop lourd (${v.ops}, ${v.memoire})`); pire = { ops: Math.max(pire.ops, v.ops), memoire: Math.max(pire.memoire, v.memoire) }; n++; }
}
console.log(`ok : ${n} programmes joues, au plus ${pire.ops} dessins, ${pire.memoire} octets`);
