// node test-fnaf-berry.js : la nuit des peluches en fiche (fiche.js, modele "fnaf") jouee par moteur-fnaf.be dans verif : la fiche se lit, ca tourne, ca tient en memoire, ca fait ce que dit la fiche
// verif joue 10 s au plus : les tests qui ont besoin de plus (fin de nuit, panne) avancent la variable du jeu dans le scenario (voir essai-verif.js)
const assert = require('assert'), { execFileSync } = require('child_process'), fs = require('fs');
const Fiche = require('./fiche.js'), { assembler } = require('./fiche-berry.js'), { VERIF, essai: essaiScript } = require('./essai-verif.js');

const clone = o => JSON.parse(JSON.stringify(o));
// --- la fiche (JS) : lecture, refus, manques
const net = f => Fiche.nettoyer(f), lit = f => Fiche.lire(net(f).fiche);
let r = net(Fiche.DEPARTS_FNAF); assert.deepStrictEqual(r.refus, [], r.refus.join(' ; ')); assert.strictEqual(lit(Fiche.DEPARTS_FNAF).bloquants.length + lit(Fiche.DEPARTS_FNAF).manques.length, 0);
r = net({ ...clone(Fiche.DEPARTS_FNAF), truc: 1, peluches: { A: { forme: 'dragon' } }, salles: { Z: {} } }); assert(r.refus.some(x => x.includes('truc')) && r.refus.some(x => x.includes('dragon')) && r.refus.some(x => x.includes('Z')), r.refus.join(' ; '));
r = net({ ...clone(Fiche.DEPARTS_FNAF), rows: ['ABCDEFGH', 'ABCDEFGH'].map((x, i) => [...x].map((c, j) => String.fromCharCode(65 + i * 8 + j)).join('')) }); assert(r.refus.some(x => x.includes('12 salles')) && r.fiche.rows.join('').replace(/\./g, '').length === 12);
assert(lit({ ...clone(Fiche.DEPARTS_FNAF), salles: {} }).manques.some(x => x.includes('aucune salle ne donne dessus')), 'peluche sans porte');
assert(lit({ ...clone(Fiche.DEPARTS_FNAF), rows: ['A.B'], salles: { B: { porte: 'droite' } }, peluches: { Z: { depart: 'A', porte: 'droite' } } }).manques.some(x => x.includes('pas reliées')), 'salles non reliees');
assert(lit({ ...clone(Fiche.DEPARTS_FNAF), heros: { boutons: { G: 'rien', D: 'rien', P3: 'camera' } } }).manques.some(x => x.includes('ferme une porte')));
assert(lit({ rows: [] }).bloquants.length, 'carte vide');
console.log('  fiche : lecture, refus, manques : ok');

if (!fs.existsSync(VERIF)) { console.log('verif non compile : nuit des peluches non jouee'); process.exit(0); }
// --- le moteur
const script = f => { const r = assembler(f); assert(r.script, 'assemblage : ' + r.refus.join(' ; ')); return r.script; };
const essai = (nom, f, o) => essaiScript(nom, script(f), o);
const fiche = (over = {}) => ({ ...clone(Fiche.DEPARTS_FNAF), ...over });
const chasseur = (carte, porte, extra = {}) => ({ rows: carte, salles: { [carte[0].slice(-1)]: { porte } }, peluches: { Zed: { depart: 'A', porte, agressivite: 20, delai: [1, 1], attente: 1, ...extra } } });

// 1. le depart tourne, tient en memoire, dessine peu ; caméra ouverte aussi
let v = JSON.parse(execFileSync(VERIF, [], { input: script(fiche()) })); assert(v.ok, v.erreur); assert(v.ops <= 1200, v.ops + ' dessins');
let ko = Math.round(v.memoire * 0.6 / 1000); assert(ko <= 45, `~${ko} Ko`); console.log(`  depart : ok, ${v.ops} dessins, ~${ko} Ko, ${v.instructions} instructions par image`);
v = JSON.parse(execFileSync(VERIF, [], { input: script(fiche({ rows: ['ABCDEF', 'GHIJKL'], salles: { A: { porte: 'gauche' }, L: { porte: 'droite' } }, peluches: Object.fromEntries(['a', 'b', 'c', 'd', 'e'].map((n, i) => [n, { depart: 'GHIJK'[i] }])) })) })); assert(v.ok, v.erreur);
console.log(`  grande carte, 5 peluches : ok, ${v.ops} dessins, ~${Math.round(v.memoire * 0.6 / 1000)} Ko`);

// 2. les portes et la pile : une porte fermee coute de la pile ; G bascule la porte gauche
const pulse = (k, d) => `(k == ${k} && t > ${d + 0.005} && t < ${d + 0.1})`;   // 0,005 s de decalage : un debut de fenetre pile sur une image donne 2 appuis de suite (appui compare a t - 0,0167)
let m = essai('porte', fiche({ peluches: {} }), { scen: `return ${pulse(2, 0.1)} || ${pulse(0, 0.5)}`, mesure: '(DC[0] ? 1 : 0) * 1000 + int(pile * 10)', quand: [0.3, 5] });
assert.strictEqual(Math.floor(m[1] / 1000), 1, 'porte gauche fermee'); const avec = m[1] % 1000;
m = essai('sans porte', fiche({ peluches: {} }), { scen: `return ${pulse(2, 0.1)}`, mesure: 'int(pile * 10)', quand: [5] });
assert(avec < m[0] - 10, `pile : ${avec / 10} avec la porte contre ${m[0] / 10} sans`);
console.log('  portes, pile : ok');

// 3. la peluche avance de salle en salle vers sa porte (une case par deplacement)
m = essai('chemin', fiche(chasseur(['ABCD'], 'droite')), { scen: `return ${pulse(2, 0.1)}`, mesure: 'PRM[0]', quand: [2.5, 3.5, 4.5, 5.5] }); assert.deepStrictEqual(m, [0, 1, 2, 3], `chemin ${m}`);
// 4. porte ouverte : elle attrape ; porte fermee : elle repart
m = essai('attrape', fiche(chasseur(['AB'], 'droite')), { scen: `return ${pulse(2, 0.1)}`, mesure: 'etat', quand: [2, 7.5] }); assert(m[0] === 1 && m[1] >= 2, `attrape ${m}`);
m = essai('porte fermee', fiche(chasseur(['AB'], 'droite')), { scen: `return ${pulse(2, 0.1)} || ${pulse(1, 0.5)}`, mesure: 'PRM[0] * 10 + PST[0] + etat * 100', quand: [3.5, 7.5] }); assert.deepStrictEqual(m, [111, 100], `porte fermee ${m}`);
m = essai('mauvaise porte', fiche(chasseur(['AB'], 'gauche')), { scen: `return ${pulse(2, 0.1)} || ${pulse(1, 0.5)}`, mesure: 'etat', quand: [7.5] }); assert(m[0] >= 2, 'la porte de droite fermee ne protege pas la gauche');
console.log('  peluche : chemin, attrape, repart : ok');

// 5. caméra : P3 ouvre, passe de salle en salle, puis ferme ; camera_suivante ; lampe
const trois = { rows: ['ABC'], salles: { C: { porte: 'droite' } }, peluches: {} };
m = essai('camera', fiche(trois), { scen: `return (k == 2 && ((t > 0.105 && t < 0.2) || (t % 0.5 > 0.005 && t % 0.5 < 0.1 && t > 0.95 && t < 2.65)))`, mesure: '(cam ? 1 : 0) * 10 + ci', quand: [1.2, 1.7, 2.2, 2.7] }); assert.deepStrictEqual(m, [10, 11, 12, 0], `camera ${m}`);
m = essai('camera suivante', fiche({ ...trois, heros: { boutons: { G: 'camera', D: 'camera_suivante', P3: 'rien' } } }), { scen: `return ${pulse(0, 0.1)} || ${pulse(0, 1)} || ${pulse(1, 1.5)} || ${pulse(0, 2.0)}`, mesure: '(cam ? 1 : 0) * 10 + ci', quand: [0.3, 1.2, 1.7, 2.2] }); assert.deepStrictEqual(m, [0, 10, 11, 1], `camera suivante ${m}`);
m = essai('lampe', fiche({ ...trois, heros: { boutons: { G: 'lampe', D: 'rien', P3: 'rien' } } }), { scen: `return ${pulse(1, 0.1)} || (k == 0 && t > 0.505 && t < 2)`, mesure: 'lamp ? 1 : 0', quand: [0.3, 1, 3] }); assert.deepStrictEqual(m, [0, 1, 0], `lampe ${m}`);
// timide : on le regarde, il ne bouge pas ; sinon il avance
const guette = timide => fiche({ ...chasseur(['ABC'], 'droite', { timide }) });
m = essai('timide', guette(1), { scen: `return (k == 2 && ((t > 0.105 && t < 0.2) || (t > 0.505 && t < 0.6)))`, mesure: 'PRM[0]', quand: [3, 9] }); assert.deepStrictEqual(m, [0, 0], `timide ${m}`);
m = essai('pas timide', guette(0), { scen: `return (k == 2 && ((t > 0.105 && t < 0.2) || (t > 0.505 && t < 0.6)))`, mesure: 'PRM[0]', quand: [9] }); assert(m[0] > 0, `pas timide ${m}`);
console.log('  camera, lampe, timide : ok');

// 6. la nuit : 6 h = fin de la nuit (duree), nuit suivante, derniere nuit = bravo ; panne de courant ; regles
const aube = { peluches: {}, nuit: { duree: 30, nuits: 2, rage: 2 } }, tot = 'if DUREE > 5 DUREE = 5.0 end ';
m = essai('aube', fiche(aube), { scen: `${tot}return ${pulse(2, 0.1)}`, mesure: 'etat * 10 + nuit', quand: [4, 6, 9.5], duree: 9.8 }); assert.deepStrictEqual(m, [11, 41, 2], `aube ${m}`);   // en jeu (nuit 1) -> 6 AM -> retour au titre de la nuit 2
m = essai('derniere nuit', fiche({ ...aube, nuit: { duree: 30, nuits: 1, rage: 2 } }), { scen: `${tot}return ${pulse(2, 0.1)}`, mesure: 'etat', quand: [4, 6, 9.5], duree: 9.8 }); assert.deepStrictEqual(m, [1, 4, 5], `derniere nuit ${m}`);
m = essai('panne', fiche({ peluches: {} }), { scen: `if t > 1 && pile > 0 pile = 0.0 panne = 14.0 end return ${pulse(2, 0.1)}`, mesure: 'etat * 10 + (DC[0] ? 1 : 0)', quand: [0.8, 1.5, 4], duree: 4.1 }); assert(m[0] === 10 && m[1] === 10 && m[2] >= 20, `panne ${m}`);
m = essai('regle temps', fiche({ peluches: {}, regles: [{ quand: { temps: 2 }, alors: { message: 'Hop' } }] }), { scen: `return ${pulse(2, 0.1)}`, mesure: 'MSG == "Hop" ? 1 : 0', quand: [1.5, 3] }); assert.deepStrictEqual(m, [0, 1]);
m = essai('regle pile', fiche({ peluches: {}, regles: [{ quand: { temps: 2 }, alors: { energie: -50 } }] }), { scen: `return ${pulse(2, 0.1)}`, mesure: 'int(pile)', quand: [1.5, 3] }); assert(m[0] >= 98 && m[1] <= 52, `regle pile ${m}`);
m = essai('regle rage', fiche({ peluches: {}, regles: [{ quand: { temps: 1 }, alors: { rage: 5 } }] }), { scen: `return ${pulse(2, 0.1)}`, mesure: 'RAGEX', quand: [0.5, 2] }); assert.deepStrictEqual(m, [0, 5]);
m = essai('regle gagne', fiche({ peluches: {}, but: 'regles', regles: [{ quand: { temps: 2 }, alors: 'gagne' }] }), { scen: `return ${pulse(2, 0.1)}`, mesure: 'etat', quand: [1.5, 3] }); assert.deepStrictEqual(m, [1, 4]);
m = essai('regle perd', fiche({ peluches: {}, regles: [{ quand: { pile: 50 }, alors: 'perd' }] }), { scen: `if t > 1 && pile > 49 pile = 49.0 end return ${pulse(2, 0.1)}`, mesure: 'etat', quand: [0.8, 2], duree: 2.1 }); assert(m[0] === 1 && m[1] === 2, `regle perd ${m}`);
console.log('  nuit : fin, derniere nuit, panne, regles : ok');
console.log('ok : nuit des peluches Berry');
