// node test-bulles.js : chaque combinaison de bulles donne un programme que verif joue 10 s sans erreur (meme Berry que la console)
// memoire : pic de Berry mesure par verif, plafond 72000 (mesure sur console le 17/11 : 46 a 53 Ko de tas libre en jeu avec les jeux les plus lourds, 33 Ko au plus bas avant nettoyage)
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
  assert(v.ops <= 600 && v.memoire <= 72000, `${JSON.stringify(r)} : trop lourd (${v.ops} dessins, ${v.memoire} octets)`);
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
  for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${obst} ${monde} : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 72000, `${jeu} ${obst} ${monde} : trop lourd (${v.ops}, ${v.memoire})`); pire = { ops: Math.max(pire.ops, v.ops), memoire: Math.max(pire.memoire, v.memoire) }; n++; }
}
// seconde version : chaque cartel x chaque nombre de niveaux x chaque jeu, avec l'ecran de fin (FIN = 1) et, pour les niveaux, un coup de P3 simule par la boucle normale
const cartels = J.ETAPES.find(e => e.id === 'fin').bulles.map(b => b.id), niv = ['un', 'trois', 'cinq', 'n2', 'n4', 'n6'];
for (const [i, jeu] of jeux.entries()) for (const [k, fin] of cartels.entries()) for (const nv of niv) {
  const code = J.assembler({ jeu, fin, niv: nv, monde: mondes[(i + k) % mondes.length] });
  for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${fin} ${nv} : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 72000, `${jeu} ${fin} ${nv} : trop lourd (${v.ops}, ${v.memoire})`); n++; }
}
// plateforme a niveaux : plateformes differentes x plus de pieges (combinables), vitesse inchangee
for (const jeu of ['plat-but', 'plat-attrape', 'plat-evite', 'plat-course']) for (const nv of ['trois', 'cinq', 'n6']) for (const plan of ['non', 'oui']) for (const piege of ['non', 'oui']) {
  const code = J.assembler({ jeu, niv: nv, plan, piege });
  assert(code.includes('def charge()') && code.includes('majJ(dt)'), `${jeu} ${nv} : niveaux de plateforme`);
  for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${nv} ${plan} ${piege} : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 72000, `${jeu} ${nv} ${plan} ${piege} : trop lourd (${v.ops}, ${v.memoire})`); n++; }
}
assert(J.saute('plan', { jeu: 'plat-but', niv: 'trois' }) === false && J.saute('plan', { jeu: 'plat-but', niv: 'un' }) && J.saute('piege', { jeu: 'attrape', niv: 'cinq' }), 'plan/piege : seulement plateforme a plusieurs niveaux');
assert(J.assembler({ jeu: 'plat-but', niv: 'trois', plan: 'oui' }) !== J.assembler({ jeu: 'plat-but', niv: 'trois', plan: 'non' }), 'plan change le code');
assert(J.assembler({ jeu: 'plat-but', niv: 'trois', piege: 'oui' }) !== J.assembler({ jeu: 'plat-but', niv: 'trois' }), 'piege change le code');
assert(J.assembler({ jeu: 'attrape', fin: 'over' }).includes('"GAME OVER"') && !J.assembler({ jeu: 'attrape', fin: 'over' }).includes('"PERDU !"'), 'le cartel change de mot');
assert(J.assembler({ jeu: 'plat-but', niv: 'trois' }).includes('majJ(') && !J.assembler({ jeu: 'quiz', niv: 'trois' }).includes('majJ('), 'niveaux : pas pour les jeux solo');
console.log(`ok :${n} programmes joues, au plus ${pire.ops} dessins, ${pire.memoire} octets`);
// les vues de course x les vehicules (auto, moto, surf), avec l'ecran de fin
{ let m = 0; for (const jeu of ['cote', 'derriere', 'dessus', 'iso']) for (const figure of ['auto', 'moto', 'surf']) for (const obst of ['avec', 'sans']) {
  const code = J.assembler({ jeu, figure, obst, monde: mondes[m++ % mondes.length], perso: persos[m % persos.length] });
  assert(code.includes('def vehicule') && code.includes('var V = ' + (jeu === 'cote' ? 0 : 1)), `${jeu} : vehicule`);
  for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok && v.ops <= 600, `${jeu} ${figure} ${obst} : ${v.erreur || v.ops}`); }
} assert(!J.assembler({ jeu: 'attrape', figure: 'moto' }).includes('def vehicule(x, y)\n  if V == 0') || J.assembler({ jeu: 'attrape' }).includes('var V = -1'), 'pas de vehicule hors course'); }
console.log('ok : vues x vehicules');
// plateforme placee a la main (r.edit) : memes donnees que le jeu = meme programme ; moins d'etoiles, plus de monstres, plusieurs niveaux
for (const jeu of ['plat-but', 'plat-attrape', 'plat-evite', 'plat-course']) {
  const d = J.platDefaut({ jeu }); assert(d && d.pl.length === 7, `${jeu} : platDefaut`);
  assert(J.assembler({ jeu, edit: { pl: d.pl, st: d.st, hz: d.hz } }) === J.assembler({ jeu }), `${jeu} : edit = defaut, meme programme`);
  const edits = [{ pl: d.pl, st: d.st.slice(0, 3), hz: [] }, { pl: d.pl.map(p => [p[0] + 10, p[1] - 4, p[2] + 10]), st: [[100, 192]], hz: [200, 250, 300, 350, 400, 450, 500, 550] }, { pl: d.pl, st: d.st, hz: [9999, -5] }, { pl: d.pl, st: d.st, hz: [[200, 100], [300, 60], [400, 200], [500, 999]] }];
  for (const nv of ['un', 'trois', 'n4']) for (const edit of edits) for (const plan of ['non', 'oui']) {
    const code = J.assembler({ jeu, niv: nv, plan, piege: 'oui', edit });
    for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${nv} ${plan} edit : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 72000, `${jeu} ${nv} edit : trop lourd (${v.ops}, ${v.memoire})`); }
  }
}
assert(J.assembler({ jeu: "plat-but", edit: { pl: J.platDefaut({ jeu: "plat-but" }).pl, st: [[100, 192]], hz: [[300, 100]] } }).includes("var HY = [100.0]"), "monstre en l air : sa hauteur est dans le jeu");
assert(J.assembler({ jeu: 'plat-but', niv: 'n6', edit: { pl: J.platDefaut({ jeu: 'plat-but' }).pl, st: [[100, 192]], hz: [] } }) === J.assembler({ jeu: 'plat-but', niv: 'n6' }), '5 niveaux et plus : placement ignore');
assert(J.platDefaut({ jeu: 'attrape' }) === null, 'platDefaut : seulement les plateformes');
assert(J.assembler({ jeu: 'attrape', edit: { pl: [], st: [], hz: [] } }) === J.assembler({ jeu: 'attrape' }), 'edit sans effet hors plateforme');
console.log('ok : plateforme placee a la main');
