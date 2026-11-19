// node test-fiche-berry.js : des fiches jouees par le moteur Berry dans verif (meme Berry que la console) : ca tourne, ca tient en memoire, ca fait ce que dit la fiche
// verif joue 10 s : P3 tenu (0-3 s), + G (3-5 s), + D (5-7 s), P3 qui tape (7-8 s), G (8-9 s), D (9-10 s) ; "suivi" = les variables demandees, une ligne par seconde
const assert = require('assert'), { execFileSync } = require('child_process'), path = require('path'), fs = require('fs');
const Fiche = require('./fiche.js'), { assembler } = require('./fiche-berry.js');
const VERIF = path.join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');
if (!fs.existsSync(VERIF)) { console.log('verif non compile : fiches non jouees'); process.exit(0); }

const clone = o => JSON.parse(JSON.stringify(o));
const grille = [...Array(11).fill('.'.repeat(20)), '.S..................', '####################', '####################'];   // 13 lignes : sol en bas, depart a gauche
const base = (changes = {}, rows = grille) => Fiche.fusion({ ...clone(Fiche.DEPARTS.plateforme), rows: clone(rows) }, changes);
const VARS = ['px', 'py', 'vy', 'pts', 'gagne', 'perdu', 'morts', 'chrono', 'lives'];
function joue(nom, fiche) {
  const { script, refus } = assembler(fiche);
  assert(script, `${nom} : ${refus.join(' ; ')}`);
  const v = JSON.parse(execFileSync(VERIF, VARS, { input: script }));
  assert(v.ok, `${nom} : ${v.erreur}`);
  assert(v.ops <= 1200, `${nom} : ${v.ops} dessins`);
  const ko = Math.round(v.memoire * 0.6 / 1000);   // meme estimation que server.js : verif tourne en 64 bits
  assert(ko <= 45, `${nom} : ~${ko} Ko de memoire (moteur + fiche ; plafond du serveur 66)`);
  const suivi = v.suivi.trim().split('\n').map(l => Object.fromEntries([...l.matchAll(/ (\w+)=(\S+)/g)].map(m => [m[1], m[2] === 'true' ? true : m[2] === 'false' ? false : +m[2]])));
  console.log(`  ${nom} : ok, ${v.ops} dessins, ~${ko} Ko, ${script.length} octets`);
  return suivi;
}
const bouge = s => s.some(l => l.px !== s[0].px), saute = s => s.some(l => l.py !== s[0].py);

// 1. le jeu vide : le carre ne bouge pas, les boutons ne font rien
let s = joue('jeu vide', clone(Fiche.DEPARTS.vide));
assert(!bouge(s) && s.every(l => l.px === s[0].px), 'le jeu vide ne doit pas bouger');

// 2. la plateforme d'aujourd'hui : P3 saute, G et D deplacent
s = joue('plateforme', base());
assert(bouge(s), 'D / G doivent deplacer le heros');
assert(s.some(l => l.py < s[0].py - 8) || s.some(l => l.vy !== 0), 'P3 doit faire sauter le heros');

// 3. les boutons de la fiche comptent : G et D seulement (pas de saut) -> le heros reste au sol
const sansSaut = base({ heros: { boutons: { P3: 'rien' } } }); s = joue('sans saut', sansSaut);
assert(s.every(l => Math.abs(l.py - s[0].py) < 1), 'sans bouton de saut, le heros reste au sol');

// 4. une scie qui va-et-vient (jamais en panne) + un ennemi qui suit + une plateforme mobile
const objets = { Z: { forme: 'scie', mouvement: 'aller_retour', points: [[0, 0], [6, 0]], duree: 2, pause: 1, declencheur: 'toujours', contact: 'perd' },
  F: { forme: 'ennemi', mouvement: 'suivre', vitesse: 3, contact: 'perd', dessus: 'ecrase' },
  M: { forme: 'bloc', mouvement: 'aller_retour', points: [[0, 0], [5, 0]], duree: 2, contact: 'rien' } };
const rows = clone(grille); rows[8] = '..........Z.........'; rows[10] = '..................F.'; rows[9] = '.M..................';
s = joue('scie, ennemi, plateforme mobile', base({ objets }, rows));

// 5. une cle ouvre une porte, 60 s font perdre, un message
const regles = [{ quand: { touche: 'K' }, alors: { ouvre: 'P' } }, { quand: { temps: 6 }, alors: { message: 'Trop lent' } }, { quand: { temps: 8 }, alors: 'perd' }];
const r2 = clone(grille); r2[11] = '.S.....K..........P.'; r2[10] = '..................P.';
s = joue('regles', base({ briques: { K: {}, P: { solide: 1 } }, regles, but: 'regles' }, r2));
assert(s.some(l => l.morts >= 1), 'la regle « temps 8 -> perd » doit faire perdre en 10 s');

// 6. ramasser tout : etoiles sur la trajectoire, le but se declenche
const r3 = clone(grille); r3[11] = '.S*.................';
s = joue('ramasser tout', base({ but: 'ramasser_tout', heros: { boutons: { G: 'aller_gauche', D: 'aller_droite', P3: 'aller_droite' } } }, r3));
assert(s.some(l => l.gagne === true), 'ramasser la seule etoile doit faire gagner');

// 7. vue de dessus (gravite 0), vies, bord qui boucle, camera fixe
s = joue('dessus', base({ monde: { gravite: 0, bord: 'boucle', camera: 'fixe' }, heros: { boutons: { G: 'aller_gauche', D: 'aller_droite', P3: 'aller_haut' }, vies: 3 } }));
assert(bouge(s), 'vue de dessus : les boutons deplacent');

// ---------- mesures precises (essai-verif.js) : boutons scriptes, valeurs lues dans le jeu a des instants donnes ----------
const { essai: essaiScript, pres } = require('./essai-verif.js');
const essai = (nom, fiche, o) => essaiScript(nom, assembler(fiche).script, o);
const sauteA = (t0, t1) => `return k == 2 && t > ${t0} && t < ${t1}`;

// A. saut : 4,5 cases = 72 px depuis le sol (py 178) ; 9 cases = 144 px ; lacher tot = petit saut ; en_l_air 1 = un 2e saut en l'air monte plus haut
const sommet = (changes, scen) => Math.min(...essai('saut', base(changes), { scen, mesure: 'py', quand: [...Array(60)].map((_, i) => 0.5 + i * 0.05) }));
let h = 178 - sommet({}, sauteA(0.5, 2)); assert(Math.abs(h - 72) <= 6, `saut 4,5 cases : ${h} px`);   // un peu moins que 72 : pas de temps de 1/60 s (le moteur en C fait pareil)
const h2 = 178 - sommet({ heros: { saut: { hauteur: 9 } } }, sauteA(0.5, 2)); assert(Math.abs(h2 / h - 2) <= 0.1, `saut 9 cases : ${h2} px, doit etre le double de ${h}`);
h = h2;
h = 178 - sommet({}, sauteA(0.5, 0.55)); assert(h > 5 && h < 40, `petit saut : ${h} px`);
h = 178 - sommet({ heros: { saut: { hauteur: 4.5, en_l_air: 1 } } }, `return k == 2 && ((t > 0.5 && t < 2) || (t > 0.8 && t < 2))`.replace('((t > 0.5 && t < 2) || (t > 0.8 && t < 2))', '(t > 0.5 && t < 0.9) || (t > 0.95 && t < 2)'));
assert(h > 90, `double saut : ${h} px (plus que 72)`); console.log(`  saut : 4,5 cases, 9 cases, petit saut, double saut : ok`);

// B. scie de l'enfant : "va de 6 cases vers la droite en 2 s, attend 1 s et revient" (depart a t = 0 : 48 px a 1 s, 96 de 2 a 3 s, retour, 0 a 5 s)
const scie = { Z: { forme: 'scie', mouvement: 'aller_retour', points: [[0, 0], [6, 0]], duree: 2, pause: 1, declencheur: 'toujours', contact: 'perd' } };
const rz = clone(grille); rz[3] = '..Z.................';
pres('scie', essai('scie', base({ objets: scie }, rz), { mesure: 'int(OBJS[0]["x"] - OBJS[0]["x0"])', quand: [1, 2, 2.5, 2.95, 4, 5, 5.5] }), [48, 96, 96, 96, 48, 0, 0], 3);

// C. plateforme mobile : le heros pose dessus est emporte (px suit la plateforme)
const bloc = { M: { forme: 'bloc', mouvement: 'aller_retour', points: [[0, 0], [8, 0]], duree: 4, contact: 'rien' } };
const rm = clone(grille); rm[10] = '..S.................'; rm[11] = '..M.................'; rm[12] = '#'.repeat(20); rm[11] = '..M.................';
const mm = essai('plateforme', base({ objets: bloc }, rm), { mesure: 'int(px) * 1000 + int(OBJS[0]["x"])', quand: [1, 2, 3, 4] });   // px et x de la plateforme, ensemble
const dp = mm.map(x => Math.floor(x / 1000) - (x % 1000)); assert(Math.max(...dp) - Math.min(...dp) <= 6, `la plateforme n'emporte pas le heros : ecarts ${dp}`);
assert(mm[3] % 1000 - mm[0] % 1000 > 20, 'la plateforme doit bouger'); console.log(`  plateforme mobile : ecart heros - plateforme ${dp} (emporte)`);

// D. ennemi qui suit : le heros immobile est rejoint puis perd (morts >= 1) ; l'ennemi avance de 3 cases/s = 48 px/s
const suit = { F: { forme: 'ennemi', mouvement: 'suivre', vitesse: 3, contact: 'perd' } };
const rf = clone(grille); rf[11] = '.S.............F....';
const sf = essai('suivre', base({ objets: suit }, rf), { mesure: 'int(OBJS[0]["x0"] - OBJS[0]["x"]) * 100 + morts', quand: [1, 2, 3, 4, 5, 6] });
assert(sf[0] > 4000 && sf[0] < 5200 && sf[5] % 100 >= 1, `ennemi qui suit : ${sf}`); console.log(`  ennemi qui suit : ${sf.join(' ')} (avance ~48 px/s puis touche)`);

// E. cle et porte : sans regle la porte (2 cases de haut) arrete le heros ; avec « touche K -> ouvre P » il passe
const rk = clone(grille); rk[11] = '.S..K...P.........'.padEnd(20, '.'); rk[10] = '........P...........';
const marche = `return k == 1`;
const sansCle = essai('porte', base({ briques: { K: {}, P: { solide: 1 } } }, rk), { scen: marche, mesure: 'int(px)', quand: [4, 8] });
assert(sansCle[1] > 100 && sansCle[1] < 125, `la porte doit arreter le heros : px ${sansCle}`);
const avecCle = essai('cle', base({ briques: { K: {}, P: { solide: 1 } }, regles: [{ quand: { touche: 'K' }, alors: { ouvre: 'P' } }] }, rk), { scen: marche, mesure: 'int(px)', quand: [4, 8] });
assert(avecCle[1] > 280, `la cle doit ouvrir la porte : px ${avecCle}`); console.log(`  cle et porte : sans ${sansCle}, avec ${avecCle}`);

// F. regle de temps : « au bout de 3 s on perd », une fois par vie (3 s, 6 s...)
const sr = essai('temps', base({ regles: [{ quand: { temps: 3 }, alors: 'perd' }], but: 'regles' }), { mesure: 'morts', quand: [2.5, 3.5, 6.5, 9.5] });
assert(sr[0] === 0 && sr[1] === 1 && sr[2] === 2 && sr[3] === 3, `regle de temps : morts ${sr}`); console.log(`  regle de temps : morts ${sr}`);

// G. but « survivre 4 s » : gagne a 4 s
const sg = essai('survivre', base({ but: { survivre: 4 } }), { mesure: 'gagne ? 1 : 0', quand: [3.5, 4.5] }); assert(sg[0] === 0 && sg[1] === 1, `survivre : ${sg}`);

// 8. modeles pas encore faits en Berry : refus clair
assert(assembler({ ...clone(Fiche.DEPARTS.cote), rows: clone(grille) }).script === undefined, 'cote : pas de script (reste au firmware)');
console.log('ok : fiche-berry');
