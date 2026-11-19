// node test-fiche-casse.js : la fiche du casse-briques (fiche.js) : bornes, refus, lecture en francais, et les fiches de plateforme restent intactes
const assert = require('assert'), F = require('./fiche.js');
const net = f => F.nettoyer(f);

const d = net(F.DEPARTS_CASSE);
assert.deepStrictEqual(d.refus, [], 'le depart passe tel quel : ' + d.refus);
assert.deepStrictEqual(F.lire(d.fiche).bloquants, []);
assert.deepStrictEqual(F.lire(d.fiche).manques, []);
assert(F.lire(d.fiche).phrases.some(p => p.includes('2 coups') || p.includes('2 coups pour')), 'D : 2 coups');

// bornes et refus
const r = net({ modele: 'casse', rows: ['BB.B', 'XXXX'], heros: { vitesse: 99, largeur: 0, vies: 50, boutons: { G: 'sauter', P3: 'lancer' } }, balle: { vitesse: 1, nombre: 9, vent: 1 },
  briques: { X: { indestructible: 1, coups: 9, bonus: 'feu' }, Y: { bonus: 'vie', coups: 3 } }, regles: [{ quand: 'casse_tout', alors: { message: 'Très bien !' } }, { quand: 'touche', alors: 'gagne' }], but: { points: 5 } });
assert.strictEqual(r.fiche.heros.vitesse, 30); assert.strictEqual(r.fiche.heros.largeur, 1); assert.strictEqual(r.fiche.heros.vies, 9);
assert.strictEqual(r.fiche.balle.vitesse, 2); assert.strictEqual(r.fiche.balle.nombre, 3);
assert.strictEqual(r.fiche.heros.boutons.G, 'rien'); assert.strictEqual(r.fiche.heros.boutons.P3, 'lancer');
assert.strictEqual(r.fiche.briques.X.coups, 5); assert.strictEqual(r.fiche.briques.X.bonus, undefined); assert.deepStrictEqual(r.fiche.briques.Y, { bonus: 'vie', coups: 3 });
assert.strictEqual(r.fiche.regles.length, 1); assert.strictEqual(r.fiche.regles[0].alors.message, 'Tres bien !');
assert.deepStrictEqual(r.fiche.but, { points: 5 });
for (const mot of ['vent', 'sauter', 'feu', 'quand']) assert(r.refus.some(x => x.includes(mot)), 'refus ' + mot);

// ce qui manque / bloque
let lu = F.lire(net({ ...F.DEPARTS_CASSE, heros: { boutons: { G: 'rien', D: 'rien', P3: 'rien' } } }).fiche);
assert(lu.manques.some(m => m.includes('raquette ne peut pas bouger')), 'raquette immobile');
assert(lu.phrases.some(p => p.includes('toute seule')), 'balle sans bouton lancer');
lu = F.lire(net({ ...F.DEPARTS_CASSE, rows: ['XXXX'], briques: { X: { indestructible: 1 } } }).fiche);
assert(lu.manques.some(m => m.includes('incassables')), 'tout incassable');
lu = F.lire(net({ ...F.DEPARTS_CASSE, rows: ['....', '....'] }).fiche);
assert(lu.manques.some(m => m.includes('aucune brique')), 'grille vide');
lu = F.lire(net({ ...F.DEPARTS_CASSE, rows: ['BBB'] }).fiche);
assert(lu.bloquants.some(m => m.includes('lignes de 4 à 20')), 'trop etroit');
lu = F.lire(net({ ...F.DEPARTS_CASSE, but: { points: 99 } }).fiche);
assert(lu.manques.some(m => m.includes('99 briques')), 'but impossible');

// poser une lettre dans la grille (comme le fait l'IA), fusion de changements
const f = structuredClone(d.fiche);
assert(F.poser(f, 'Z', 3, 3)); assert.strictEqual(f.rows[3][3], 'Z');
const g = F.fusion(f, { balle: { vitesse: 14 }, briques: { Z: { bonus: 'vie' } } });
assert.strictEqual(net(g).fiche.balle.vitesse, 14); assert.strictEqual(net(g).fiche.briques.Z.bonus, 'vie');

// la plateforme n'a pas bouge
assert.strictEqual(net(F.DEPARTS.plateforme).fiche.modele, 'plateforme');
console.log('ok : fiche casse');
