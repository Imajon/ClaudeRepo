// Banc d'essai : l'IA ecrit-elle de meilleurs jeux avec des JEUX entiers (exemples/) ou des SYSTEMES (systemes/) en exemple ?
// MISTRAL_API_KEY=... node bench-exemples.js [repetitions=2]      (node bench-exemples.js --sec : montre seulement les exemples choisis, sans IA)
// Meme chemin que le site (consigne, modele, verif, 2 corrections codestral), puis un juge (mistral-large) compare les 2 jeux a l'aveugle.
// Resultats : bench/resultats.json + bench/<jeux|systemes>/<n>-<r>.be (a essayer sur la console).
const fs = require('fs'), path = require('path'), { execFile } = require('child_process');
const Ex = require('./exemples.js');
const CONSIGNE = fs.readFileSync(path.join(__dirname, 'consigne-script.txt'), 'utf8');
const VERIF = path.join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');
const CAS = { jeux: Ex, systemes: Ex.systemes };
const DEMANDES = [   // les 6 premieres ressemblent a un jeu d'exemple, les autres non
  'une course de voitures sur un circuit avec des virages',
  'un jeu ou on saute de plateforme en plateforme et on ramasse des pieces',
  'un serpent qui mange des pommes',
  'un vaisseau qui tire sur des meteorites',
  'un combat de ninjas contre l\'ordinateur',
  'du ski avec des sauts',
  'un jeu de tennis contre la console',
  'un oiseau qui vole entre des tuyaux, P3 pour battre des ailes',
  'un tank dans un labyrinthe qui tire sur des cibles',
  'un pac-man avec des fantomes qui me poursuivent',
  'un casse-briques avec 3 vies',
  'des zombies arrivent des deux cotes et je dois les repousser',
  'une course de voitures a deux consoles',
  'un jeu de peche : on lance la ligne et on attrape des poissons',
  'un jeu de plateformes avec des ennemis qui patrouillent et une cle pour ouvrir la porte',
  'un jeu de rythme : appuyer sur P3 quand la note arrive sur la ligne',
  'des envahisseurs qui descendent et me tirent dessus',
  'un casse-briques avec des briques solides et des bonus',
  "un jeu de reflexe : il faut appuyer des que l'ecran devient vert",
  "une jauge qui bouge et il faut l'arreter dans le vert pour tirer un penalty",
  'un quiz de calcul avec 3 reponses',
  'un jeu de memoire avec des couleurs a repeter',
  'un tir a la corde a deux consoles en appuyant le plus vite',
  'une grenouille qui charge son saut pour sauter sur des nenuphars',
];

if (process.argv[2] === '--sec') {
  for (const d of DEMANDES) console.log(d, '\n   jeux     :', Ex.choisir(d).map(e => e.f).join(', ') || '(aucun)', '\n   systemes :', Ex.systemes.choisir(d).map(e => e.f).join(', ') || '(aucun)');
  process.exit();
}
if (!process.env.MISTRAL_API_KEY) { console.error('MISTRAL_API_KEY manquante'); process.exit(1); }
const R = +process.argv[2] || 2;

async function mistral(consigne, contenu, modele, json) {
  for (let essai = 0; ; essai++) {
    const r = await fetch('https://api.mistral.ai/v1/chat/completions', { method: 'POST', signal: AbortSignal.timeout(120000),
      headers: { Authorization: 'Bearer ' + process.env.MISTRAL_API_KEY, 'Content-Type': 'application/json' },
      body: JSON.stringify({ model: modele, temperature: 0.2, safe_prompt: true, ...(json ? { response_format: { type: 'json_object' } } : {}),
        messages: [{ role: 'system', content: consigne }, { role: 'user', content: JSON.stringify(contenu) }] }) });
    if (r.status === 429 && essai < 6) { await new Promise(ok => setTimeout(ok, 5000 * (essai + 1))); continue; }   // limite de debit : on attend
    if (!r.ok) throw new Error('Mistral ' + r.status + ' ' + (await r.text()).slice(0, 200));
    return (await r.json()).choices[0].message.content;
  }
}
function programme(t) {                                  // comme iaScript dans server.js : bloc ```, imports oublies, "step"
  let s = (/```[^\n]*\n([\s\S]*?)```/.exec(t) || [])[1] || '';
  for (const m of ['math', 'string'])
    if (new RegExp(`\\b${m}\\.`).test(s) && !new RegExp(`^\\s*import\\s+${m}\\b`, 'm').test(s)) s = s.startsWith('#') ? s.replace(/^(.*\n?)/, `$1import ${m}\n`) : `import ${m}\n` + s;
  return s.replace(/(\bfor\s+\w+\s*:\s*)(.+?)\s*\.\.\s*(.+?)\s+step\s+(\S+)/g, '$1range($2, $3, $4)');
}
const verifier = s => new Promise(ok => execFile(VERIF, [], { timeout: 5000 }, (e, o) => { try { ok(JSON.parse(o)); } catch { ok({ ok: false, erreur: 'verif muet' }); } }).stdin.end(s));
const probleme = v => !v.ok ? (v.erreur || 'erreur').replace(/\s+/g, ' ') : v.ops > 1200 ? 'trop de dessins' : v.memoire > 70000 ? 'trop de memoire' : '';
const entree = (demande, script, erreur) => ({ programme_actuel: script || '(aucun : c\'est le premier jeu)', la_console_a_dit: erreur || 'rien de special', test_automatique: '(pas de programme)', demandes_precedentes: [], demande });

async function generer(cas, demande) {
  const choix = CAS[cas].choisir(demande);
  const t0 = Date.now();
  let s = programme(await mistral(CONSIGNE + CAS[cas].texte(choix), entree(demande), 'mistral-large-latest'));
  let v = await verifier(s), pb = probleme(v), premier = pb, n = 0;
  while (pb && n < 2) { n++; s = programme(await mistral(CONSIGNE, entree('Corrige seulement l\'erreur signalee, sans rien changer d\'autre au jeu.', s, pb), 'codestral-latest')) || s; pb = probleme(v = await verifier(s)); }
  return { cas, exemples: choix.map(e => e.f), premier_ok: !premier, premier_erreur: premier.slice(0, 120), corrections: n, ok: !pb, erreur: pb.slice(0, 120),
    lignes: s.split('\n').length, ops: v.ops, memoire: v.memoire, instructions: v.instructions, duree: Date.now() - t0, script: s };
}

const JUGE = `Tu juges des jeux ecrits en Berry pour une console a 3 boutons (G, D, P3), ecran 320x240, pour des enfants de 9 a 14 ans.
On te donne la demande de l'enfant et deux programmes A et B (avec ce que le verificateur a vu : ok ou erreur). Lis le code attentivement et imagine la partie.
Criteres, dans l'ordre : 1) il tourne (un programme en erreur perd), 2) il fait ce que l'enfant a demande, avec ses mots, 3) il est jouable et amusant avec ces 3 boutons (on comprend quoi faire, on peut gagner ou perdre), 4) rien d'inutile.
Reponds en JSON : {"A": note sur 10, "B": note sur 10, "meilleur": "A" | "B" | "egal", "raison": "une phrase"}`;
async function juger(demande, a, b) {
  const pres = x => ({ verificateur: x.ok ? 'tourne sans erreur' : 'ERREUR : ' + x.erreur, programme: x.script });
  return JSON.parse(await mistral(JUGE, { demande, A: pres(a), B: pres(b) }, 'mistral-large-latest', true));
}

async function tache(i, r) {
  const d = DEMANDES[i];
  const [j, s] = await Promise.all([generer('jeux', d), generer('systemes', d)]);
  const inverse = Math.random() < 0.5, [A, B] = inverse ? [s, j] : [j, s];   // a l'aveugle : ordre tire au sort
  let jug; try { jug = await juger(d, A, B); } catch (e) { jug = { erreur: e.message }; }
  const note = { jeux: inverse ? jug.B : jug.A, systemes: inverse ? jug.A : jug.B };
  const gagnant = jug.meilleur === 'egal' ? 'egal' : jug.meilleur ? ((jug.meilleur === 'A') !== inverse ? 'jeux' : 'systemes') : '?';
  for (const x of [j, s]) { fs.mkdirSync(path.join(__dirname, 'bench', x.cas), { recursive: true }); fs.writeFileSync(path.join(__dirname, 'bench', x.cas, `${i + 1}-${r}.be`), x.script); }
  console.log(`${i + 1}-${r} ${gagnant.padEnd(8)} jeux ${note.jeux}/10 ${j.ok ? 'ok' : 'ERR'}  systemes ${note.systemes}/10 ${s.ok ? 'ok' : 'ERR'}  | ${d}`);
  return { demande: d, n: i + 1, r, gagnant, note, raison: jug.raison, jeux: { ...j, script: undefined }, systemes: { ...s, script: undefined } };
}

(async () => {
  const travail = []; for (let r = 1; r <= R; r++) for (let i = 0; i < DEMANDES.length; i++) travail.push([i, r]);
  const res = [];
  await Promise.all(Array.from({ length: 3 }, async () => { while (travail.length) { const [i, r] = travail.shift(); try { res.push(await tache(i, r)); } catch (e) { console.error(i + 1, r, e.message); } } }));   // 3 a la fois
  const m = (cas, f) => res.reduce((t, x) => t + f(x[cas], x), 0) / res.length;
  const bilan = {};
  for (const cas of ['jeux', 'systemes']) bilan[cas] = {
    victoires: res.filter(x => x.gagnant === cas).length,
    note_moyenne: +m(cas, (_, x) => +x.note[cas] || 0).toFixed(2),
    tourne_du_premier_coup: Math.round(100 * m(cas, y => y.premier_ok)) + ' %',
    tourne_a_la_fin: Math.round(100 * m(cas, y => y.ok)) + ' %',
    corrections: +m(cas, y => y.corrections).toFixed(2), lignes: Math.round(m(cas, y => y.lignes)), secondes: Math.round(m(cas, y => y.duree) / 1000) };
  bilan.egal = res.filter(x => x.gagnant === 'egal').length;
  const proche = res.filter(x => x.n <= 6), loin = res.filter(x => x.n > 6), v = (l, c) => l.filter(x => x.gagnant === c).length;
  bilan.demandes_proches_des_jeux = `jeux ${v(proche, 'jeux')} / systemes ${v(proche, 'systemes')} / egal ${v(proche, 'egal')}`;
  bilan.demandes_nouvelles = `jeux ${v(loin, 'jeux')} / systemes ${v(loin, 'systemes')} / egal ${v(loin, 'egal')}`;
  fs.writeFileSync(path.join(__dirname, 'bench', 'resultats.json'), JSON.stringify({ date: new Date().toISOString(), bilan, res: res.sort((a, b) => a.n - b.n || a.r - b.r) }, null, 1));
  console.log('\n', bilan);
})();
