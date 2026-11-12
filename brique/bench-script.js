// Banc d'essai du mode script : quelle methode Mistral cree / modifie le mieux, le plus vite, pour le moins de tokens ?
// MISTRAL_API_KEY=... node bench-script.js [methodes=mistral,large,medium,devstral] [repetitions=1] [cas=tous, ex. m1,m3] [serie=script]
//   mistral  = la methode du site (liste de points, plan, mistral-large reecrit tout, codestral corrige 2 fois, relecture)
//   large    = mistral-large-2512 en remplacements (contexte-mistral.md, 1 correction) ; medium (3.5) et devstral = pareil ; sonnet = reference (ANTHROPIC_API_KEY)
// Lance server.js en local (port au hasard, donnees bench/<serie>-data.json : plusieurs series peuvent tourner en meme temps), envoie les demandes comme la page (/api/script),
// puis un juge (Large 4) lit le code : demande faite ? note /10. Resultats : bench/<serie>-resultats.json + bench/<serie>/<methode>/<cas>-<rep>.be
const fs = require('fs'), path = require('path'), { spawn } = require('child_process');
const SERIE = process.argv[5] || 'script', B = path.join(__dirname, 'bench'), DATA = path.join(B, SERIE + '-data.json'), PORT = 18000 + Math.floor(Math.random() * 1000);
const lire = f => fs.readFileSync(path.join(__dirname, f), 'utf8');
const DRIFT = lire('exemples/1-drift.be');               // gros jeu (341 l.) : en ligne, Mistral a echoue 12 fois sur 12 en le modifiant (nov. 2026)
const CAS = [                                            // c = creation, m = modif de Drift, o = modif d'autres jeux, x = multi (deux consoles)
  ['c1', '', 'un oiseau qui vole entre des tuyaux, P3 pour battre des ailes'],
  ['c2', '', 'un casse-briques avec 3 vies'],
  ['c3', '', 'des zombies arrivent des deux cotes et je dois les repousser'],
  ['c4', '', 'un jeu de plateformes avec des ennemis qui patrouillent et une cle pour ouvrir la porte'],
  ['c5', '', 'une grenouille qui charge son saut pour sauter sur des nenuphars'],
  ['m1', DRIFT, 'ajoute des flaques d\'huile sur la piste qui font deraper la voiture'],
  ['m2', DRIFT, 'la course se fait en 3 tours, affiche le numero du tour en haut'],
  ['m3', DRIFT, 'ajoute un mode 2 joueurs a deux consoles par la radio, on voit la voiture de l\'autre'],
  ['m4', DRIFT, 'la voiture laisse des flammes derriere elle quand elle va tres vite'],
  ['m5', DRIFT, 'ajoute une voiture ordinateur qui fait la course contre moi'],
  ['o1', lire('exemples/4-plateforme.be'), 'ajoute des pics qui font perdre une vie, avec 3 coeurs en haut de l\'ecran'],
  ['o2', lire('exemples/3-meteorites.be'), 'quand je tire sur une grosse meteorite elle se casse en deux petites'],
  ['o3', lire('exemples/9-combat.be'), 'ajoute une attaque speciale quand je tiens P3 longtemps, avec une jauge qui se remplit'],
  ['o4', lire('../c3-berry/jeux/front-des-portes.be'), 'ajoute un boss a la fin qui a beaucoup de vie et tire sur mon armee'],
  ['o5', lire('exemples/8-glisse.be'), 'ajoute des pieces a ramasser en l\'air et un score'],
  ['x1', '', 'une course de voitures a deux consoles'],
  ['x2', '', 'un pong a deux consoles, chacun sa raquette'],
  ['x3', '', 'un tir a la corde a deux consoles en appuyant le plus vite'],
  ['x4', lire('exemples/9-combat.be'), 'on joue a deux consoles : chaque joueur dirige son robot, plus de robot ordinateur'],
  ['x5', lire('exemples/2-plateformes.be'), 'course a deux consoles : on voit le coureur de l\'autre, le premier a 1000 m gagne'],
];
const JUGE = `Tu juges le travail d'une IA qui ecrit des jeux (langage Berry) pour une console d'enfant : 3 boutons G, D, P3, ecran 320x240, la console appelle debut(), maj(dt), dessine().
On te donne la demande de l'enfant, le programme de depart (s'il y en a un) et le programme rendu. Lis le CODE (pas les commentaires) : la demande est-elle vraiment faite, visible et jouable a l'ecran, sans casser le reste ?
Reponds UNIQUEMENT en JSON : {"fait": true ou false, "note": 0 a 10 (10 = exactement ce que l'enfant voulait, amusant, rien de casse), "pourquoi": "une phrase"}`;

async function juger(demande, avant, apres) {
  try {
    const r = await fetch('https://api.mistral.ai/v1/chat/completions', { method: 'POST', signal: AbortSignal.timeout(90000),
      headers: { Authorization: 'Bearer ' + process.env.MISTRAL_API_KEY, 'Content-Type': 'application/json' },
      body: JSON.stringify({ model: 'mistral-large-latest', reasoning_effort: 'low', temperature: 0.1, response_format: { type: 'json_object' },   // juge = Large 4 (reflechit), autre modele que ceux testes
        messages: [{ role: 'system', content: JUGE }, { role: 'user', content: JSON.stringify({ demande, programme_de_depart: avant || '(aucun : nouveau jeu)', programme_rendu: apres }) }] }) });
    if (!r.ok) return { pourquoi: 'juge ' + r.status + ' ' + (await r.text()).slice(0, 150) };
    const c = (await r.json()).choices[0].message.content;
    return JSON.parse(typeof c === 'string' ? c : c.map(p => p.type === 'text' ? p.text : '').join(''));
  } catch (e) { return { pourquoi: 'juge : ' + e.message }; }
}
async function demander(methode, script, texte, id) {   // flux : les en-tetes arrivent tout de suite (sinon fetch coupe a 300 s)
  const t0 = Date.now();
  const r = await fetch(`http://localhost:${PORT}/api/script`, { method: 'POST', headers: { 'Content-Type': 'application/json', 'X-Forwarded-For': id },
    body: JSON.stringify({ texte, script, avant: [], images: [], methode, flux: true }) });
  const t = await r.text(), i = t.lastIndexOf('\0');
  return { ...JSON.parse(i < 0 ? t : t.slice(i + 1)), secondes: Math.round((Date.now() - t0) / 1000) };
}
const tokens = id => { try { return JSON.parse(fs.readFileSync(DATA, 'utf8')).conso?.['ip:' + id]?.j || 0; } catch { return 0; } };   // le serveur compte les tokens par "ip" : une par essai
const moy = a => a.length ? Math.round(a.reduce((x, y) => x + y, 0) / a.length * 10) / 10 : '-';
const med = a => a.length ? a.sort((x, y) => x - y)[a.length >> 1] : '-';

(async () => {
  if (!process.env.MISTRAL_API_KEY) { console.error('MISTRAL_API_KEY manquante'); process.exit(1); }
  const methodes = (process.argv[2] || 'mistral,large,medium,devstral').split(','), reps = +process.argv[3] || 1, filtre = process.argv[4]?.split(',');
  const cas = CAS.filter(c => !filtre || filtre.includes(c[0]));
  fs.mkdirSync(B, { recursive: true }); fs.writeFileSync(DATA, '{}');
  const srv = spawn(process.execPath, ['server.js'], { cwd: __dirname, env: { ...process.env, PORT, DATA, DB_HOST: '' }, stdio: ['ignore', 'pipe', 'inherit'] });
  await new Promise(ok => srv.stdout.on('data', d => /Brique sur/.test(d) && ok()));
  const res = [];
  for (let rep = 1; rep <= reps; rep++) for (const [c, base, texte] of cas) for (const methode of methodes) {   // methodes entrelacees : meme heure, memes conditions
    const id = `${methode}-${c}-${rep}`;
    let j; try { j = await demander(methode, base, texte, id); } catch (e) { j = { erreur: e.message }; }
    const tourne = !!j.script && !j.verif && !j.erreur && !j.refus, avis = j.script ? await juger(texte, base, j.script) : {};
    const l = { methode, cas: c, rep, tourne, fait: avis.fait === true, note: typeof avis.note === 'number' ? avis.note : null, secondes: j.secondes, tokens: tokens(id),
      corrections: j.corrections ?? null, lignes: j.script ? j.script.split('\n').length : 0, probleme: j.erreur || j.refus || j.verif || '', juge: avis.pourquoi || '' };
    res.push(l); console.log(JSON.stringify(l));
    if (j.script) { fs.mkdirSync(path.join(B, SERIE, methode), { recursive: true }); fs.writeFileSync(path.join(B, SERIE, methode, `${c}-${rep}.be`), j.script); }
    fs.writeFileSync(path.join(B, SERIE + '-resultats.json'), JSON.stringify(res, null, 1));
  }
  srv.kill();
  const tab = {};
  for (const methode of methodes) for (const [type, p] of [['creation', 'c'], ['modif drift', 'm'], ['modif autres', 'o'], ['multi', 'x']]) {
    const L = res.filter(r => r.methode === methode && r.cas.startsWith(p)); if (!L.length) continue;
    tab[methode + ' ' + type] = { essais: L.length, tourne: Math.round(100 * L.filter(r => r.tourne).length / L.length) + ' %', 'tourne+fait': Math.round(100 * L.filter(r => r.tourne && r.fait).length / L.length) + ' %',
      note: moy(L.filter(r => r.note !== null).map(r => r.note)), secondes_med: med(L.map(r => r.secondes || 0)), tokens_moy: moy(L.map(r => r.tokens)), corrections: moy(L.map(r => r.corrections || 0)) };
  }
  console.table(tab);
  fs.writeFileSync(path.join(B, SERIE + '-resultats.json'), JSON.stringify({ resume: tab, essais: res }, null, 1));
})();
