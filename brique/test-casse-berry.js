// node test-casse-berry.js : le casse-briques en fiche (fiche.js, modele "casse") joue par moteur-casse.be dans verif : ca tourne, ca tient en memoire, ca fait ce que dit la fiche
// Boutons scriptes + pilote automatique (la raquette suit la balle) : voir essai-verif.js
const assert = require('assert'), { execFileSync } = require('child_process'), fs = require('fs');
const Fiche = require('./fiche.js'), { assembler } = require('./fiche-berry.js'), { VERIF, essai: essaiScript, pres } = require('./essai-verif.js');
if (!fs.existsSync(VERIF)) { console.log('verif non compile : casse-briques non joue'); process.exit(0); }

const clone = o => JSON.parse(JSON.stringify(o));
const base = (changes = {}, rows) => Fiche.fusion({ ...clone(Fiche.DEPARTS_CASSE), ...(rows ? { rows } : {}) }, changes);
const script = f => { const r = assembler(f); assert(r.script, 'assemblage : ' + r.refus.join(' ; ')); return r.script; };
const essai = (nom, f, o) => essaiScript(nom, script(f), o);
// pilote : P3 tape toutes les secondes (lance une balle collee), la raquette suit la balle quand elle descend vers elle, sinon le bonus qui tombe
const PILOTE = `var hb = size(BX) > 0
  var tx = (hb && BY[0] > 140 && BVY[0] > 0) || size(IX) == 0 ? (hb ? BX[0] : 160.0) : IX[0] var c = rx + pw / 2
  if k == 2 return t > 0.4 && int(t * 4) % 4 == 0 end
  if k == 1 return tx > c + 4 end
  return tx < c - 4`;
const PILOTE1 = PILOTE.replace('if k == 2 return t > 0.4 && int(t * 4) % 4 == 0 end', 'if k == 2 return t > 0.4 && t < 0.5 end');   // un seul lancer : P3 de plus rejouerait la partie apres "perdu"

// 1. le depart tourne, tient en memoire, dessine peu
const v = JSON.parse(execFileSync(VERIF, [], { input: script(base()) }));
assert(v.ok, v.erreur); assert(v.ops <= 1200, v.ops + ' dessins');
const ko = Math.round(v.memoire * 0.6 / 1000); assert(ko <= 45, `~${ko} Ko`);
console.log(`  depart : ok, ${v.ops} dessins, ~${ko} Ko, ${v.instructions} instructions par image`);
// grille pleine : 14 x 20, 16 sortes de briques
const L = 'ABCDEFGHIJKLMNOP'; const grosses = [...Array(14)].map((_, r) => [...Array(20)].map((_, c) => L[(r + c) % 16]).join(''));
const v2 = JSON.parse(execFileSync(VERIF, [], { input: script(base({ briques: Object.fromEntries([...L].map((l, i) => [l, { coups: 1 + i % 3, bonus: i % 5 === 0 ? 'vie' : undefined }])) }, grosses)) }));
assert(v2.ok, v2.erreur); assert(v2.ops <= 1200, 'grille pleine : ' + v2.ops + ' dessins');
console.log(`  grille pleine 14 x 20 : ok, ${v2.ops} dessins, ~${Math.round(v2.memoire * 0.6 / 1000)} Ko`);

// 2. la raquette : 192 px/s (12 cases/s), s'arrete aux bords, largeur de la fiche
let m = essai('raquette', base(), { scen: 'return k == 1 && t > 0.5 && t < 1.0', mesure: 'rx', quand: [0.4, 1.1] });
assert(Math.abs(m[1] - m[0] - 96) <= 8, `raquette : ${m} (96 px en 0,5 s)`);
m = essai('bord', base({ heros: { largeur: 5 } }), { scen: 'return k == 1', mesure: 'rx', quand: [3] }); assert.strictEqual(m[0], 240, `bord droit : raquette de 5 cases = x 240, vu ${m}`);
m = essai('bord', base(), { scen: 'return k == 0', mesure: 'rx', quand: [3] }); assert.strictEqual(m[0], 0);
console.log('  raquette : 192 px/s, bords, largeur : ok');

// 3. la balle : 160 px/s (10 cases/s) au depart, reste dans l'ecran ; 3 balles ; accelere avec les briques
m = essai('vitesse balle', base(), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'math.sqrt(BVX[0] * BVX[0] + BVY[0] * BVY[0])', quand: [0.8] }); assert(Math.abs(m[0] - 160) <= 3, `vitesse ${m}`);
m = essai('vitesse 14', base({ balle: { vitesse: 14 } }), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'math.sqrt(BVX[0] * BVX[0] + BVY[0] * BVY[0])', quand: [0.8] }); assert(Math.abs(m[0] - 224) <= 3, `vitesse 14 : ${m}`);
const dans = essai('dans l\'ecran', base(), { scen: PILOTE, mesure: 'int(size(BX) > 0 ? BX[0] : 160.0) * 1000 + int(size(BY) > 0 ? BY[0] : 100.0)', quand: [...Array(80)].map((_, i) => 0.5 + i * 0.1) });
for (const x of dans) { const bx = Math.floor(x / 1000), by = x % 1000; assert(bx >= 3 && bx <= 316 && by >= 3 && by <= 245, `balle hors ecran : ${bx} ${by}`); }
m = essai('3 balles', base({ balle: { nombre: 3 } }), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'size(BX)', quand: [0.3, 1.0] }); assert.deepStrictEqual(m, [3, 3]);
console.log('  balle : vitesse, dans l\'ecran, 3 balles : ok');

// 4. briques : 1 coup casse tout de suite ; 2 coups = deux touches ; incassable ne casse jamais ; brique cassee = un point
const rowsB = ['..B.'], hit1 = { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'pts * 100 + NBR', quand: [0.3, 3] };
m = essai('1 coup', base({ briques: { B: {} } }, rowsB), hit1); assert.deepStrictEqual(m, [1, 100], `1 coup : ${m} (1 brique, 0 point puis 1 point, plus de brique)`.replace('(1 brique', '('));   // pts 0 NBR 1 -> 1 point 0 brique : "1" puis "100"
m = essai('2 coups', base({ briques: { B: { coups: 2 } }, but: 'regles' }, rowsB), { scen: PILOTE, mesure: 'pts * 100 + NBR', quand: [0.3, 1.6, 9] });
assert.strictEqual(m[0], 1); assert.strictEqual(m[1], 1, `2 coups : toujours la apres une touche (${m})`); assert.strictEqual(m[2], 100, `2 coups : cassee a la 2e touche (${m})`);
m = essai('incassable', base({ briques: { X: { indestructible: 1 } } }, ['XXXX']), { scen: PILOTE, mesure: 'GR[0] * 1000 + pts * 10 + (gagne ? 1 : 0)', quand: [6] }); assert.strictEqual(m[0], 99000, `incassable : ${m}`);
console.log('  briques : 1 coup, 2 coups, incassable : ok');

// 5. vies : sans pilote la balle se perd ; perdu a la derniere vie ; P3 rejoue
m = essai('vies', base(), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'lives', quand: [1, 9] }); assert(m[0] === 3 && m[1] <= 2, `vies : ${m}`);
m = essai('perdu', base({ heros: { vies: 1 } }), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'perdu ? 1 : 0', quand: [1, 9] }); assert.deepStrictEqual(m, [0, 1]);
console.log('  vies : ok');

// 6. gagner : tout casser ; regles (points, temps) ; but points
m = essai('gagne', base({ briques: { B: {} } }, rowsB), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'gagne ? 1 : 0', quand: [0.3, 4] }); assert.deepStrictEqual(m, [0, 1]);
m = essai('but points', base({ briques: { B: {} }, but: { points: 1 } }, ['..B.', '..B.']), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'gagne ? 1 : 0', quand: [0.3, 4] }); assert.deepStrictEqual(m, [0, 1]);
m = essai('regle points', base({ briques: { B: {} }, but: 'regles', regles: [{ quand: { points: 1 }, alors: { message: 'Hop' } }] }, ['..B.', '..B.']), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'MSG == "Hop" ? 1 : 0', quand: [0.3, 3] }); assert.deepStrictEqual(m, [0, 1]);
m = essai('regle temps', base({ but: 'regles', regles: [{ quand: { temps: 3 }, alors: 'perd' }] }), { scen: PILOTE1, mesure: 'perdu ? 1 : 0', quand: [2.5, 3.5] }); assert.deepStrictEqual(m, [0, 1]);
m = essai('acceleration', base({ briques: { B: {} }, but: 'regles', balle: { acceleration: 1 } }, ['..B.', '..B.']), { scen: 'return k == 2 && t > 0.4 && t < 0.5', mesure: 'int(math.sqrt(BVX[0] * BVX[0] + BVY[0] * BVY[0]))', quand: [0.8, 1.7] });
assert(m[0] === 160 && m[1] >= 164, `acceleration : ${m}`);
console.log('  gagner, regles, acceleration : ok');

// 7. bonus : la brique V (bonus vie) lache une vie en plus que la raquette attrape
m = essai('bonus vie', base({ briques: { V: { bonus: 'vie' } }, but: 'regles' }, ['..V.']), { scen: PILOTE, mesure: 'lives', quand: [0.3, 9] }); assert.deepStrictEqual(m, [3, 4], `bonus vie : ${m}`);
m = essai('bonus large', base({ briques: { V: { bonus: 'large' } }, but: 'regles' }, ['..V.']), { scen: PILOTE, mesure: 'int(pw)', quand: [0.3, 1.5, 3, 4.5, 6, 7.5, 9] }); assert(m[0] === 48 && m.includes(64), `bonus large : ${m}`);
m = essai('bonus multi', base({ briques: { V: { bonus: 'multi' } }, but: 'regles' }, ['..V.']), { scen: PILOTE, mesure: 'size(BX)', quand: [0.3, 1.5, 3, 4.5, 6, 7.5, 9] }); assert(m[0] === 1 && m.includes(2), `bonus multi : ${m}`);
console.log('  bonus : ok');
console.log('ok : casse-briques Berry');
