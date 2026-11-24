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
// un monde par niveau (plateforme a plusieurs niveaux) : neige au niveau 1, foret au niveau 2... plafond 74000 (au lieu de 72000) : le 2e monde ajoute son decor et son avant-plan (~3 Ko), le pire cas mesure est 73,4 Ko (plat-evite, 6 niveaux, neige puis eau) : a essayer sur la console
{ const W = ['neige', 'foret', 'ville', 'espace', 'eau', 'desert'];
  assert(J.saute('monde2', { jeu: 'plat-but', niv: 'n3' }) === false && J.saute('monde3', { jeu: 'plat-but', niv: 'n3' }) === false && J.saute('monde4', { jeu: 'plat-but', niv: 'n3' }) && J.saute('monde2', { jeu: 'plat-but', niv: 'un' }) && J.saute('monde2', { jeu: 'attrape', niv: 'n3' }), 'monde du niveau k : seulement la plateforme et les niveaux qui existent');
  assert.deepStrictEqual(J.recetteComplete({ jeu: 'plat-but', niv: 'n3', monde: 'neige', monde2: 'foret' }).mondes, ['neige', 'foret', 'neige'], 'un niveau sans choix reprend le monde du niveau 1');
  assert(!J.recetteComplete({ jeu: 'plat-but', niv: 'n3', monde: 'neige', monde2: 'meme' }).mondes && !J.recetteComplete({ jeu: 'attrape', niv: 'n3', monde: 'neige', monde2: 'foret' }).mondes, 'pas de mondes par niveau : rien ne change');
  assert.deepStrictEqual(J.recetteComplete({ jeu: 'plat-but', niv: 'n6', monde: 'neige', monde2: 'foret', monde3: 'ville', monde4: 'espace', monde5: 'eau', monde6: 'desert' }).mondes, ['neige', 'foret', 'neige', 'neige', 'neige', 'neige'], '2 mondes differents au plus');
  assert(J.assembler({ jeu: 'plat-but', niv: 'n3', monde: 'neige' }) === J.assembler({ jeu: 'plat-but', niv: 'n3', monde: 'neige', monde2: 'meme', monde3: 'meme' }) && J.assembler({ jeu: 'plat-but', niv: 'n3', monde: 'neige', monde2: 'foret' }).includes('NV == 1 || NV == 3'), 'le monde du niveau 2 est dans le code');
  for (const jeu of ['plat-but', 'plat-attrape', 'plat-evite', 'plat-course']) for (const nv of [2, 3, 4, 6]) for (let d = 0; d < 6; d++) {
    const r = { jeu, niv: 'n' + nv, monde: W[d] }; for (let k = 2; k <= nv; k++) r['monde' + k] = W[(d + k - 1) % 6];
    const code = J.assembler(r); assert(code.includes('def couleurs()') && /NV == 1/.test(code), `${jeu} ${nv} : un monde par niveau`);
    for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${nv} ${W[d]} : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 74000, `${jeu} ${nv} ${W[d]} par niveau : trop lourd (${v.ops}, ${v.memoire})`); n++; }
  }
  assert.deepStrictEqual(J.mondeBulles('monde3', { niv: 'n3', monde: 'neige', monde2: 'foret' }).map(b => b.id), ['meme', 'foret', 'neige'], 'deja 2 mondes : pas de 3e');
  assert(J.mondeBulles('monde3', { niv: 'n3', monde: 'neige' }).length === 7 && J.mondeBulles('monde2', { niv: 'n3', monde: 'neige', monde2: 'ville' }).length === 7, 'un 2e monde reste libre (le choix du niveau lui-meme ne compte pas)');
  console.log('ok : un monde par niveau'); }
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
// les tirs fixes : chaque monde habille le fond et les ennemis ; l'etape « monde » n'est plus sautee
for (const jeu of ['tir-perso', 'tir-autour']) {
  // tir vers la droite et tir autour : le personnage, les tirs (bon), les ennemis (mal), leur facon de bouger et leurs types se choisissent
  assert(J.saute('monde', { genre: 'tire', jeu }) === false && ['perso', 'bon', 'mal', 'compo', 'typeEn'].every(id => J.saute(id, { genre: 'tire', jeu }) === false) && J.saute('action', { genre: 'tire', jeu }), `${jeu} : etapes choisies`);
  for (const monde of choix[3]) {
    const code = J.assembler({ jeu, monde }); assert(code.includes('def objet(x, y, k)') && code.includes('BAND = ['), `${jeu} ${monde} : monde applique`);
    for (const c of [code, code.replace(/fin = false/g, 'fin = true')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok && v.ops <= 600 && v.memoire <= 72000, `${jeu} ${monde} : ${v.erreur || v.ops + ' dessins ' + v.memoire + ' octets'}`); }
  }
  assert(J.assembler({ jeu, monde: 'neige' }) !== J.assembler({ jeu, monde: 'espace' }), `${jeu} : le monde change le code`);
}
console.log('ok : tirs x mondes');
// tir autour : chaque personnage x chaque tir / ennemi du monde ou du bestiaire, et les images de l'enfant (perso, tirs, ennemis)
const formesBon = J.ETAPES.find(e => e.id === 'bon').bulles.map(b => b.id), formesMal = J.ETAPES.find(e => e.id === 'mal').bulles.map(b => b.id);
const jouer = (r, nom, jeu = 'tir-autour') => { const code = J.assembler({ jeu, ...r }); for (const c of [code, code.replace(/fin = false/g, 'fin = true')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok && v.ops <= 600 && v.memoire <= 72000, `${jeu} ${nom} : ${v.erreur || v.ops + ' dessins ' + v.memoire + ' octets'}`); } };
for (const jeu of ['tir-autour', 'tir-perso']) {
  for (const [i, perso] of persos.entries()) jouer({ perso, monde: mondes[i % mondes.length], bon: formesBon[i % formesBon.length], mal: formesMal[(i * 3) % formesMal.length] }, perso, jeu);
  for (const monde of mondes) jouer({ monde, perso: 'img', bon: 'img', mal: 'img', imgs: { perso: 1, bon: 2, mal: 3 } }, 'images ' + monde, jeu);
  assert(J.assembler({ jeu, perso: 'chat' }) !== J.assembler({ jeu, perso: 'robot' }), `${jeu} : le personnage change le code`);
}
assert(J.assembler({ jeu: 'tir-autour', perso: 'chat' }) !== J.assembler({ jeu: 'tir-autour', perso: 'robot' }), 'tir-autour : le personnage change le code');
assert(J.assembler({ jeu: 'tir-autour', mal: 'img', imgs: { mal: 4 } }).includes('image(4,'), 'tir-autour : l image de l ennemi est dans le jeu');
assert(J.lib(J.ETAPES.find(e => e.id === 'mal'), { jeu: 'tir-autour' }, 'q') !== J.ETAPES.find(e => e.id === 'mal').q && J.lib(J.ETAPES.find(e => e.id === 'mal'), { jeu: 'attrape' }, 'q') === J.ETAPES.find(e => e.id === 'mal').q, 'tir-autour : questions propres a ce jeu');
console.log('ok : tir autour x personnage, tirs, ennemis');
// vagues (invader) : perso, tirs, ennemis et monde se choisissent ; pas de facon de bouger ni de types (la formation est fixe) ; chaque monde x perso joue, et un pilote automatique marque des points
{ const { essai } = require('./essai-verif.js'), jeu = 'tir-vagues';
  assert(J.genreDe(jeu).id === 'tire' && J.saute('monde', { genre: 'tire', jeu }) === false && ['perso', 'bon', 'mal'].every(id => J.saute(id, { genre: 'tire', jeu }) === false) && ['compo', 'typeEn', 'action'].every(id => J.saute(id, { genre: 'tire', jeu })), 'vagues : etapes choisies');
  for (const [i, perso] of persos.entries()) jouer({ perso, monde: mondes[i % mondes.length], bon: formesBon[i % formesBon.length], mal: formesMal[(i * 3) % formesMal.length] }, perso, jeu);
  for (const monde of mondes) jouer({ monde }, monde, jeu);
  jouer({ perso: 'img', bon: 'img', mal: 'img', imgs: { perso: 1, bon: 2, mal: 3 } }, 'images', jeu);
  const r = essai('vagues', J.assembler({ jeu }), { scen: 'if k == 2 return int(t * 6) % 2 == 0 end return int(t / 1.5) % 2 == 0', mesure: 'pts * 100 + vies', quand: [8], duree: 8.1 })[0];
  assert(r >= 100 && r % 100 <= 3, 'vagues : le pilote marque (' + r + ')');
  assert(J.assembler({ jeu, perso: 'chat' }) !== J.assembler({ jeu, perso: 'robot' }) && J.assembler({ jeu }).startsWith('# Vagues'), 'vagues : le personnage change le code');
  console.log('ok : vagues x mondes, personnages, images, pilote'); }
// casse-briques : seuls la balle (bon) et le monde se choisissent ; chaque monde x balle joue, et un pilote qui suit la balle casse des briques sans perdre de vie
{ const { essai } = require('./essai-verif.js'), jeu = 'casse-briques', r0 = { genre: 'casse', jeu };
  assert(J.genreDe(jeu).id === 'casse' && J.saute('monde', r0) === false && J.saute('bon', r0) === false && ['perso', 'mal', 'compo', 'typeEn', 'action', 'couleur', 'niv'].every(id => J.saute(id, r0)), 'casse : etapes choisies (monde, balle)');
  assert(J.lib(J.ETAPES.find(e => e.id === 'bon'), r0, 'q') === 'Quelle balle veux-tu ?' && J.lib(J.ETAPES.find(e => e.id === 'bon'), { jeu: 'tir-autour' }, 'q') === 'Avec quoi tires-tu ?', 'casse : la question de la balle');
  for (const [i, monde] of mondes.entries()) jouer({ monde, bon: formesBon[i % formesBon.length] }, monde, jeu);
  jouer({ bon: 'img', imgs: { bon: 2 } }, 'balle image', jeu);
  const r = essai('casse', J.assembler({ jeu }), { scen: 'if k == 2 return int(t * 6) % 2 == 0 end if k == 0 return bx < rx + 26 end return bx > rx + 34', mesure: 'pts * 100 + vies', quand: [8], duree: 8.1 })[0];
  assert(r >= 3000 && r % 100 === 3, 'casse : le pilote casse des briques sans perdre de vie (' + r + ')');
  assert(J.assembler({ jeu, monde: 'neige' }) !== J.assembler({ jeu, monde: 'espace' }) && J.assembler({ jeu }).startsWith('# Casse'), 'casse : le monde change le code');
  console.log('ok : casse-briques x mondes, balles, pilote'); }
// pong (la balle et le monde se choisissent) et flappy (le personnage et le monde) : chaque monde x choix joue ; pong : un pilote qui suit la balle ne perd aucun point, un joueur immobile en perd ; flappy : un pilote qui vise le trou passe des tuyaux, un joueur immobile tombe
{ const { essai } = require('./essai-verif.js'), nonChoisies = ['mal', 'compo', 'typeEn', 'action', 'couleur', 'niv'];
  const pong = { genre: 'rebond', jeu: 'pong' }, flappy = { genre: 'rebond', jeu: 'flappy' };
  assert(J.genreDe('pong').id === 'rebond' && J.genreDe('flappy').id === 'rebond', 'rebond : le genre des deux jeux');
  assert(J.saute('monde', pong) === false && J.saute('bon', pong) === false && J.saute('perso', pong) && nonChoisies.every(id => J.saute(id, pong)), 'pong : etapes choisies (monde, balle)');
  assert(J.saute('monde', flappy) === false && J.saute('perso', flappy) === false && J.saute('bon', flappy) && nonChoisies.every(id => J.saute(id, flappy)), 'flappy : etapes choisies (monde, personnage)');
  assert(J.lib(J.ETAPES.find(e => e.id === 'bon'), pong, 'q') === 'Quelle balle veux-tu ?', 'pong : la question de la balle');
  for (const [i, monde] of mondes.entries()) { jouer({ monde, bon: formesBon[i % formesBon.length] }, monde, 'pong'); jouer({ monde, perso: persos[i % persos.length] }, monde, 'flappy'); }
  jouer({ bon: 'img', imgs: { bon: 2 } }, 'balle image', 'pong'); jouer({ perso: 'img', imgs: { perso: 3 } }, 'perso image', 'flappy');
  const mes = (jeu, scen, mesure) => essai(jeu, J.assembler({ jeu }), { scen, mesure, quand: [9], duree: 9.1 })[0];
  assert(mes('pong', 'return k == 0 && by < ry + 25 || k == 1 && by > ry + 25', 'lui') === 0 && mes('pong', 'return false', 'lui') >= 1, 'pong : le pilote qui suit la balle ne perd pas, le joueur immobile perd');
  const vise = 'var tg = 140.0 var best = 999.0 for i: 0 .. 2 if PX[i] + 30 > 60 && PX[i] < best best = PX[i] tg = PG[i] + 48.0 end end return k == 2 && y > tg + 8 && int(t * 30) % 2 == 0';
  assert(mes('flappy', vise, 'pts * 100 + (fin ? 1 : 0)') >= 300 && mes('flappy', 'return false', '(fin ? 1 : 0)') === 1 && mes('flappy', vise, '(fin ? 1 : 0)') === 0, 'flappy : le pilote passe des tuyaux, le joueur immobile tombe');
  assert(J.assembler({ jeu: 'flappy', perso: 'chat' }) !== J.assembler({ jeu: 'flappy', perso: 'robot' }) && J.assembler({ jeu: 'pong', monde: 'neige' }) !== J.assembler({ jeu: 'pong', monde: 'espace' }), 'pong / flappy : le choix change le code');
  console.log('ok : pong et flappy x mondes, choix, pilotes'); }
// rythme (guitar hero) : la note (bon) et le monde se choisissent ; un pilote qui appuie quand la note passe sur la ligne (une fois sur deux, appui = front montant) marque et rate peu, un joueur immobile rate toutes les notes
{ const { essai } = require('./essai-verif.js'), r0 = { genre: 'reagis', jeu: 'rythme' };
  assert(J.genreDe('rythme').id === 'reagis' && J.saute('monde', r0) === false && J.saute('bon', r0) === false && ['perso', 'mal', 'compo', 'typeEn', 'action', 'couleur', 'niv'].every(id => J.saute(id, r0)), 'rythme : etapes choisies (monde, note)');
  assert(J.lib(J.ETAPES.find(e => e.id === 'bon'), r0, 'q') === 'Quelles notes tombent ?', 'rythme : la question des notes');
  for (const [i, monde] of mondes.entries()) jouer({ monde, bon: formesBon[i % formesBon.length] }, monde, 'rythme');
  jouer({ bon: 'img', imgs: { bon: 2 } }, 'note image', 'rythme');
  const code = J.assembler({ jeu: 'rythme' }), mes = (scen, mesure) => essai('rythme', code, { scen, mesure, quand: [9], duree: 9.1 })[0];
  const pilote = 'var l = 2 if k == 0 l = 0 elif k == 2 l = 1 end var ok = false for i: 0 .. 7 if NA[i] && NL[i] == l && NY[i] > 197 && NY[i] < 205 ok = true end end return ok && int(t * 30) % 2 == 0';
  assert(mes(pilote, 'pts') >= 200 && mes(pilote, 'rates') <= 4 && mes('return false', 'pts') === 0 && mes('return false', 'rates') >= 10, 'rythme : le pilote marque, le joueur immobile rate tout');
  assert(J.assembler({ jeu: 'rythme', monde: 'neige' }) !== J.assembler({ jeu: 'rythme', monde: 'espace' }) && code.startsWith('# Rythme'), 'rythme : le monde change le code');
  console.log('ok : rythme x mondes, notes, pilote'); }

// ennemis des 3 tirs (haut, droite, autour) : leur facon de bouger x leurs types, joues avec un pilote automatique (tire sans arret, bouge en va-et-vient) ; propres aux tirs, proposes dans « Que veux-tu personnaliser ? » (opt) sans etre poses d'office
{ const { essai } = require('./essai-verif.js'), TIRS = ['tir-haut', 'tir-perso', 'tir-autour'], pilote = { 'tir-autour': 'if k == 2 return int(t * 6) % 2 == 0 end return k == 1', def: 'if k == 2 return int(t * 6) % 2 == 0 end if k == 0 return int(t / 1.5) % 2 == 0 end return int(t / 1.5) % 2 == 1' };
  const compos = J.ETAPES.find(e => e.id === 'compo').bulles.map(b => b.id), types = J.ETAPES.find(e => e.id === 'typeEn').bulles.map(b => b.id), pts = {};
  assert(J.ETAPES.find(e => e.id === 'compo').opt && J.ETAPES.find(e => e.id === 'typeEn').opt, 'compo / typeEn : etapes opt');
  for (const jeu of TIRS) {
    assert(!J.saute('compo', { jeu }) && !J.saute('typeEn', { jeu }), `${jeu} : compo et typeEn proposes`);
    for (const compo of compos) for (const typeEn of types) {
      const code = J.assembler({ jeu, compo, typeEn, monde: mondes[(compos.indexOf(compo) + types.indexOf(typeEn)) % mondes.length], perso: persos[(compos.indexOf(compo) * 3 + types.indexOf(typeEn)) % persos.length] });
      for (const c of [code, code.replace(/fin = false/g, 'fin = true').replace(/FIN = false/g, 'FIN = true')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok && v.ops <= 600 && v.memoire <= 72000, `${jeu} ${compo} ${typeEn} : ${v.erreur || v.ops + ' dessins ' + v.memoire + ' octets'}`); }
      const [p, v] = jeu === 'tir-haut' ? ['PTS', 'VIES'] : ['pts', 'vies'], r = essai(`${jeu} ${compo} ${typeEn}`, code, { scen: pilote[jeu] || pilote.def, mesure: `${p} * 100 + ${v}`, quand: [8], duree: 8.1 })[0];
      assert(r >= 0 && r % 100 <= 3, `${jeu} ${compo} ${typeEn} : mesure ${r}`); if (compo === 'droit') pts[jeu + typeEn] = Math.floor(r / 100);
    }
    assert(pts[jeu + 'pareils'] > 0 && pts[jeu + 'costauds'] <= pts[jeu + 'pareils'], `${jeu} : le pilote marque, et les costauds demandent plus de tirs (${pts[jeu + 'pareils']} / ${pts[jeu + 'costauds']})`);
  }
  assert(J.saute('compo', { jeu: 'attrape' }) && J.saute('typeEn', { jeu: 'plat-but' }) && J.saute('compo', { genre: 'quiz', jeu: 'quiz' }), 'compo / typeEn : seulement les jeux de tir');
  assert(J.assembler({ jeu: 'tir-haut', compo: 'zigzag' }) !== J.assembler({ jeu: 'tir-haut' }) && J.assembler({ jeu: 'tir-autour', typeEn: 'costauds' }) !== J.assembler({ jeu: 'tir-autour' }) && J.assembler({ jeu: 'attrape', compo: 'zigzag', typeEn: 'rapides' }) === J.assembler({ jeu: 'attrape' }), 'compo / typeEn changent le code des tirs, pas des autres jeux');
  console.log('ok : ennemis des tirs (facon de bouger x types)'); }
// toits : immeubles a sauter + figure en l'air (rappuyer sur le gros bouton) ; chaque figure x chaque monde joue, la figure change le code, l'etape ne sort que pour ce jeu
{ const figs = J.ETAPES.find(e => e.id === 'figtoit').bulles.map(b => b.id);
  assert(!J.saute('figtoit', { jeu: 'toits' }) && J.saute('figtoit', { jeu: 'plat-but' }) && J.saute('figtoit', { jeu: 'cote' }) && J.saute('obst', { jeu: 'toits' }) && J.saute('action', { jeu: 'toits' }), 'figtoit : seulement les toits');
  assert.strictEqual(J.recetteComplete({ jeu: 'toits', figtoit: 'nimporte' }).figtoit, 'salto', 'figure inconnue : salto');
  assert(J.assembler({ jeu: 'toits' }).includes('def immeuble') && !J.assembler({ jeu: 'plat-but' }).includes('def figure'), 'toits : immeubles et figure, pas ailleurs');
  for (const f of figs) for (const [i, monde] of mondes2.entries()) for (const niv of ['un', 'n3']) {
    const code = J.assembler({ jeu: 'toits', figtoit: f, monde, niv, perso: persos[i] });
    assert(code.includes(`def figure(`) && code.includes('math.'), `${f} : la figure est dans le code`);
    for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok && v.ops <= 600 && v.memoire <= 72000, `toits ${f} ${monde} ${niv} : ${v.erreur || v.ops + ' ' + v.memoire}`); }
  }
  assert(new Set(figs.map(f => J.assembler({ jeu: 'toits', figtoit: f }))).size === figs.length, 'chaque figure change le code');
  console.log('ok : toits x figures'); }

// lianes : on se balance de liane en liane (G monte, D descend, gros bouton lache) ; chaque monde x perso joue (aussi sur l'ecran de fin) ; un pilote automatique (Berry, boutons scriptes) gagne, un autre perd
{ const mondes3 = choix[3];
  assert(J.saute('niv', { jeu: 'lianes' }) && J.saute('figtoit', { jeu: 'lianes' }) && J.saute('action', { jeu: 'lianes' }) && J.saute('obst', { jeu: 'lianes' }) && !J.saute('bon', { jeu: 'lianes' }) && !J.saute('gagne', { jeu: 'lianes' }) && !J.saute('saute', { jeu: 'lianes' }), 'lianes : ni niveaux, ni figure, ni gros bouton a choisir ; les etoiles (bon), le cartel et le mot du saut oui');
  assert.strictEqual(J.recetteComplete({ jeu: 'lianes', niv: 'n4' }).niv, 'un', 'lianes : un seul niveau');
  assert.strictEqual(J.genreDe('lianes').id, 'plateforme', 'lianes : dans le genre plateforme');
  for (const [i, monde] of mondes3.entries()) {
    const code = J.assembler({ jeu: 'lianes', monde, perso: persos[i], couleur: couleurs[i] });
    assert(code.includes('math.atan2') && !code.includes('def immeuble'), 'lianes : le balancier, pas les immeubles');
    for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1'), code.replace(/FIN = 0/g, 'FIN = 2')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok && v.ops <= 600 && v.memoire <= 72000, `lianes ${monde} : ${v.erreur || v.ops + ' ' + v.memoire}`); }
  }
  // le pilote lache quand la liane monte vers l'avant (sin(PH) > seuil) et garde la corde vers 80 px ; verif ne joue que ~10 s : on lit l'etat a 9,5 s
  const pilote = seuil => { const fin = `
var _m = maj var _t0 = -1.0
appui = def (k) return k == 2 && AT >= 0 && 0.85 * math.sin(PH) > ${seuil} && math.cos(PH) > 0 end
bouton = def (k) return AT >= 0 && ((k == 1 && LR < 72.0) || (k == 0 && LR > 92.0)) end
maj = def (dt)
  if _t0 < 0 _t0 = temps() end
  _m(dt)
  if temps() - _t0 >= 9.5 raise "mesure", str(FIN) + " " + str(PTS) + " " + str(VIES) + " " + str(int(GX)) end
end
`; const m = /mesure: (\S+) (\S+) (\S+) (\S+)/.exec(JSON.parse(execFileSync(VERIF, [], { input: J.assembler({ jeu: 'lianes' }) + fin })).erreur || ''); assert(m, 'lianes : pas de mesure ' + seuil); return { fin: +m[1], pts: +m[2], vies: +m[3], x: +m[4] }; };
  const g = pilote(0.4), p = pilote(0.05);
  assert(g.fin === 1 && g.vies === 3 && g.pts >= 10 && g.x >= 1920, 'lianes : le pilote qui lache au bon moment arrive sur la plateforme sans perdre de vie : ' + JSON.stringify(g));
  assert(p.fin === 2 && p.vies === 0, 'lianes : lacher trop tot = tomber dans les pointes, 3 vies perdues : ' + JSON.stringify(p));
  console.log('ok : lianes (pilote gagnant ' + JSON.stringify(g) + ', perdant ' + JSON.stringify(p) + ')'); }

// « Message du jeu » : cartel de victoire et mot du saut, joues avec l'ecran de fin (FIN = 1) ; les jeux sans victoire / sans saut sautent ces etapes
{ let k = 0;
  for (const jeu of ['plat-but', 'plat-attrape', 'plat-evite', 'plat-course', 'toits', 'lianes', 'cote', 'attrape']) for (const gagne of J.ETAPES.find(e => e.id === 'gagne').bulles.map(b => b.id)) for (const saute of J.ETAPES.find(e => e.id === 'saute').bulles.map(b => b.id)) for (const niv of ['un', 'n3']) {
    const code = J.assembler({ jeu, gagne, saute, niv });
    for (const c of [code, code.replace(/FIN = 0/g, 'FIN = 1')]) { const v = JSON.parse(execFileSync(VERIF, [], { input: c })); assert(v.ok, `${jeu} ${gagne} ${saute} ${niv} : ${v.erreur}`); assert(v.ops <= 600 && v.memoire <= 74000, `${jeu} ${gagne} ${saute} ${niv} : trop lourd (${v.ops}, ${v.memoire})`); k++; }
  }
  assert(J.assembler({ jeu: 'plat-but', gagne: 'youpi' }).includes('"YOUPI !"') && !J.assembler({ jeu: 'plat-but', gagne: 'youpi' }).includes('"BRAVO !"'), 'le cartel de victoire change de mot');
  assert(J.assembler({ jeu: 'plat-but', saute: 'boing' }).includes('"BOING !"') && !J.assembler({ jeu: 'plat-but' }).includes('MSJ'), 'le mot du saut : seulement si choisi');
  assert(J.assembler({ jeu: 'attrape', saute: 'hop' }).includes('"HOP !"'), 'le mot du saut : chute avec gros bouton saut');
  assert(!J.assembler({ jeu: 'attrape', action: 'tir', saute: 'hop' }).includes('"HOP !"'), 'pas de mot du saut sans saut');
  assert(J.saute('gagne', { jeu: 'attrape' }) && J.saute('saute', { jeu: 'tir-haut' }) && J.saute('saute', { jeu: 'quiz', genre: 'quiz' }) && !J.saute('gagne', { jeu: 'toits' }) && !J.saute('saute', { jeu: 'cote', obst: 'avec' }) && J.saute('saute', { jeu: 'cote', obst: 'sans' }), 'gagne / saute : seulement ou ils servent');
  console.log('ok : message du jeu (' + k + ' jeux joues)'); }
