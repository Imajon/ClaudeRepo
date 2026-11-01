// node test-fiche.js : la fiche (fiche.js) garde les départs intacts, refuse ce que la console ne sait pas lire, et dit ce qui manque
const assert = require('assert'), F = require('./fiche.js');
const net = f => F.nettoyer(f);

// les 4 départs passent tels quels (aucun refus) ; le jeu vide dit que les boutons ne font rien
for (const [nom, d] of Object.entries(F.DEPARTS)) {
  const rows = nom === 'vide' ? d.rows : nom === 'iso' ? ['S###', '#.#^', '##*G'] : nom === 'cote' ? [...Array(13).fill('.'.repeat(30)), '.S.......E........H.........G', '#'.repeat(30)] : ['.....G..............', '...#####..W.........', '.S..*...E....R..^...', '####################'];
  const { fiche, refus } = net({ ...d, rows });
  assert.deepStrictEqual(refus, [], nom + ' : ' + refus);
  assert.deepStrictEqual(F.lire(fiche).bloquants, [], nom);
}
const vide = net(F.DEPARTS.vide).fiche, lu = F.lire(vide);
assert(lu.manques.some(m => m.includes('rien ne se passe')), 'jeu vide : boutons');
assert(lu.manques.some(m => m.includes('pas d\'arrivée')), 'jeu vide : arrivée');
// la plateforme copiée se traduit dans les lettres du moteur, à l'identique
const t = F.traduire(net(F.DEPARTS.plateforme).fiche);
for (const c of '.S#ICR^*GWEH') assert.strictEqual(t[c], c, 'traduction ' + c);

// bornes et refus
const r = net({ ...F.DEPARTS.vide, monde: { gravite: 999, vent: 3 }, heros: { boutons: { G: 'voler', X: 'sauter' }, vies: 50 },
  objets: { Z: { forme: 'scie', mouvement: 'aller_retour', points: [[0, 0], [99, 0]], duree: 2, pause: 1 } },
  regles: [{ quand: { touche: 'K' }, alors: { ouvre: 'P' } }, { quand: 'touche', alors: 'gagne' }, { quand: 'ramasse_tout', alors: { message: 'Très bien !' } }] });
assert.strictEqual(r.fiche.monde.gravite, 200);
assert.strictEqual(r.fiche.heros.boutons.G, 'rien');
assert.strictEqual(r.fiche.heros.vies, 9);
assert.deepStrictEqual(r.fiche.objets.Z.points[1], [20, 0]);
assert.strictEqual(r.fiche.regles.length, 2, 'règle sans lettre enlevée');
assert.strictEqual(r.fiche.regles[1].alors.message, 'Tres bien !', 'message sans accents');
for (const mot of ['vent', 'voler', 'X', 'quelle lettre']) assert(r.refus.some(x => x.includes(mot)), 'refus ' + mot);
// lecture en français (exemple 3 de FICHE-JEU.md)
assert(F.lire({ ...r.fiche, rows: ['Z.S', '###'] }).phrases.some(p => p.includes('va de 20 cases vers la droite en 2 s, attend 1 s et revient')));
// lettre inconnue et départ manquant : on ne peut pas envoyer
assert(F.lire({ ...vide, rows: ['..Q..', '#####', '#####'] }).bloquants.length === 2);
// fusion des changements de l'IA (null efface) et pose dans la grille
const f2 = F.fusion(net(F.DEPARTS.plateforme).fiche, { heros: { saut: { hauteur: 9 } }, objets: { W: null } });
assert.strictEqual(f2.heros.saut.hauteur, 9); assert.strictEqual(f2.heros.vitesse, 12.5); assert(!f2.objets.W);
const f3 = { ...vide, rows: [...vide.rows] }; assert(F.poser(f3, 'Z', 5, 2) && f3.rows[2][5] === 'Z' && !F.poser(f3, 'S', 1, 1));
// jeu de côté : ce qui ne bouge pas est signalé, l'arrivée unique est exigée
const cote = net({ ...F.DEPARTS.cote, rows: [...Array(14).fill('.'.repeat(20)), 'S'.padEnd(20, '#')], objets: { M: { forme: 'bloc', mouvement: 'boucle', contact: 'rien' } } }).fiche;
cote.rows[13] = '..M.................';
const lc = F.lire(cote); assert(lc.bloquants.some(b => b.includes('arrivée')) && lc.manques.some(m => m.includes('M ne bouge pas')));
console.log('test-fiche : ok');
