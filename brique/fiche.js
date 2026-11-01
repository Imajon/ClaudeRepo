// Brique : la fiche de jeu (FICHE-JEU.md). Un seul code pour la page (lecture en français, aperçu, envoi) et server.js (vérification).
// Mêmes mots et mêmes bornes que bqFicheParse() du firmware (c3-tft-meatboy.ino).
(function (F) {
const has = (o, k) => typeof k === 'string' && Object.prototype.hasOwnProperty.call(o, k);
const ACTIONS = { rien: 'rien ne se passe', aller_gauche: 'il va à gauche', aller_droite: 'il va à droite', sauter: 'il saute', aller_haut: 'il monte', aller_bas: 'il descend',
  tourner_gauche: 'il tourne à gauche', tourner_droite: 'il tourne à droite', avancer_case: 'il saute sur la case devant lui', tirer: 'il tire' };
const FORMES = { pic: 'un pic', scie: 'une scie', bloc: 'un bloc', ennemi: 'un ennemi', projectile: 'un projectile' };
const MOUVEMENTS = ['fixe', 'aller_retour', 'patrouille', 'boucle', 'tomber', 'suivre'];
const CONTACTS = { perd: 'Le toucher fait perdre.', rebond: 'Le toucher fait rebondir.', rien: 'On peut le toucher sans risque.', ramasse: 'On le ramasse en le touchant.', vie_moins: 'Le toucher enlève une vie.' };
const DECL = ['toujours', 'proche', 'touche'];
const QUAND = ['touche', 'ramasse_tout', 'ecrase_tout', 'temps', 'points'], QUAND_ARG = ['touche', 'temps', 'points'];
const ALORS = ['gagne', 'perd', 'ouvre', 'ferme', 'vie_plus', 'message', 'vitesse'], ALORS_ARG = ['ouvre', 'ferme', 'message', 'vitesse'];
const BUTS = ['arriver', 'ramasser_tout', 'ecraser_tout', 'survivre', 'regles'];
const BORDS = ['mur', 'vide', 'boucle'], MODELES = ['plateforme', 'cote', 'iso'];
const PROPS = { solide: 1, mortel: 1, glisse: 1, rebond: 20, disparait: 10, ramasse: 1, arrivee: 1 };   // propriété d'une brique -> valeur maximale
const TAILLES = { plateforme: [3, 80, 1, 20], cote: [15, 30, 20, 96], iso: [3, 12, 3, 12] };            // lignes min, max ; largeur min, max
const LETTRE = /^[!#-[\]-~]$/;                         // un caractère imprimable, sans espace, guillemet ni \ (le firmware lit le JSON sans échappement)
const definissable = l => LETTRE.test(l) && l !== '.' && l !== 'S';
const ascii = t => String(t ?? '').normalize('NFD').replace(/[\u0300-\u036f]/g, '').replace(/[^ -~]/g, '').replace(/["\\]/g, '').trim();   // police de la console
const num = (v, a, b) => { v = typeof v === 'boolean' ? +v : v; return typeof v === 'number' && isFinite(v) ? Math.min(b, Math.max(a, v)) : undefined; };
const motArg = (v, mots) => {                          // "mot" ou {"mot": valeur} -> [mot, valeur]
  if (typeof v === 'string') return mots.includes(v) ? [v] : null;
  const k = v && typeof v === 'object' && !Array.isArray(v) ? Object.keys(v) : [];
  return k.length === 1 && mots.includes(k[0]) ? [k[0], v[k[0]]] : null;
};
const s = n => n > 1 ? 's' : '';

// ---------- nettoyer : ne garde que ce que le firmware sait lire, dans ses bornes ; refus = ce qui a été enlevé (dit à l'enfant) ----------
F.nettoyer = function (f) {
  const refus = [], o = {};
  const sait = (obj, cles, ou) => { if (obj && typeof obj === 'object') for (const k of Object.keys(obj)) if (!cles.includes(k)) refus.push(`${ou} : je ne connais pas « ${k} »`); };
  if (!f || typeof f !== 'object') f = {};
  sait(f, ['modele', 'rows', 'haut', 'monde', 'heros', 'briques', 'objets', 'regles', 'but'], 'fiche');
  o.modele = MODELES.includes(f.modele) ? f.modele : 'plateforme';
  const T = TAILLES[o.modele];
  o.rows = (Array.isArray(f.rows) ? f.rows : []).filter(r => typeof r === 'string').slice(0, T[1]).map(r => [...r].slice(0, T[3]).map(c => LETTRE.test(c) || c === '.' ? c : '.').join(''));
  if (o.modele === 'iso' && Array.isArray(f.haut)) o.haut = f.haut.slice(0, 12).map(l => String(l).replace(/[^0-4]/g, '0').slice(0, 12));
  const m = f.monde || {}; sait(m, ['gravite', 'bord', 'camera'], 'monde');
  o.monde = { gravite: num(m.gravite, 0, 200) ?? 0 };
  if (BORDS.includes(m.bord)) o.monde.bord = m.bord;
  if (m.camera === 'suit' || m.camera === 'fixe') o.monde.camera = m.camera;
  const h = f.heros || {}; sait(h, ['boutons', 'vitesse', 'elan', 'saut', 'mur', 'vies'], 'héros');
  const b = h.boutons || {}; sait(b, ['G', 'D', 'P3'], 'boutons (seulement G, D et P3)');
  o.heros = { boutons: {} };
  for (const k of ['G', 'D', 'P3']) { o.heros.boutons[k] = has(ACTIONS, b[k]) ? b[k] : 'rien'; if (b[k] !== undefined && !has(ACTIONS, b[k])) refus.push(`bouton ${k} : je ne connais pas « ${b[k]} »`); }
  for (const [k, a, z] of [['vitesse', 0, 20], ['elan', 0, 1], ['vies', 0, 9]]) { const n = num(h[k], a, z); if (n !== undefined) o.heros[k] = k === 'vies' ? Math.round(n) : n; }
  if (h.saut && typeof h.saut === 'object') { sait(h.saut, ['hauteur', 'en_l_air'], 'saut'); o.heros.saut = { hauteur: num(h.saut.hauteur, 0, 20) ?? 0, en_l_air: Math.round(num(h.saut.en_l_air, 0, 2) ?? 0) }; }
  if (h.mur && typeof h.mur === 'object') { sait(h.mur, ['glisse', 'saut_mural'], 'mur'); o.heros.mur = { glisse: h.mur.glisse ? 1 : 0, saut_mural: h.mur.saut_mural ? 1 : 0 }; }
  o.briques = {};
  for (const [l, v] of Object.entries(f.briques && typeof f.briques === 'object' ? f.briques : {})) {
    if (!definissable(l)) { refus.push(`brique « ${l} » : une seule lettre (pas S ni .)`); continue; }
    if (Object.keys(o.briques).length >= 16) { refus.push('16 sortes de briques au plus'); break; }
    const p = {};
    for (const [k, x] of Object.entries(v && typeof v === 'object' ? v : {})) {
      if (!has(PROPS, k)) { refus.push(`brique ${l} : je ne connais pas « ${k} »`); continue; }
      const n = num(typeof x === 'string' ? +x : x, 0, PROPS[k]); if (n) p[k] = n;
    }
    o.briques[l] = p;
  }
  o.objets = {};
  for (let [l, v] of Object.entries(f.objets && typeof f.objets === 'object' ? f.objets : {})) {
    if (!definissable(l) || has(o.briques, l)) { refus.push(`objet « ${l} » : une seule lettre, qui n'est pas déjà une brique`); continue; }
    if (Object.keys(o.objets).length >= 16) { refus.push('16 sortes d\'objets au plus'); break; }
    const x = {}; if (!v || typeof v !== 'object') v = {};
    sait(v, ['forme', 'mouvement', 'points', 'duree', 'pause', 'declencheur', 'distance', 'cache', 'contact', 'dessus', 'vitesse'], `objet ${l}`);
    x.forme = has(FORMES, v.forme) ? v.forme : 'bloc';
    x.mouvement = MOUVEMENTS.includes(v.mouvement) ? v.mouvement : 'fixe';
    if (Array.isArray(v.points)) x.points = v.points.filter(Array.isArray).slice(0, 4).map(([a, c]) => [Math.round(num(+a, -20, 20) || 0), Math.round(num(+c, -40, 40) || 0)]);
    for (const [k, a, z] of [['duree', .1, 30], ['pause', 0, 30], ['distance', 1, 20], ['vitesse', .5, 30]]) { const n = num(v[k], a, z); if (n !== undefined) x[k] = n; }
    if (DECL.includes(v.declencheur)) x.declencheur = v.declencheur;
    if (v.cache) x.cache = 1;
    x.contact = has(CONTACTS, v.contact) ? v.contact : 'perd';
    if (v.dessus === 'ecrase') x.dessus = 'ecrase';
    o.objets[l] = x;
  }
  o.regles = [];
  const rg = Array.isArray(f.regles) ? f.regles : [];
  if (rg.length > 8) refus.push('8 règles au plus');
  for (const r of rg.slice(0, 8)) {
    const q = motArg(r?.quand, QUAND), a = motArg(r?.alors, ALORS);
    if (!q || !a) { refus.push('une règle dit « quand » et « alors », avec des mots que je connais'); continue; }
    let [qm, qv] = q, [am, av] = a;
    if (!QUAND_ARG.includes(qm)) qv = undefined;
    if (!ALORS_ARG.includes(am)) av = undefined;
    if (qm === 'touche' && !LETTRE.test(qv)) { refus.push('règle « touche » : quelle lettre ?'); continue; }
    if (qm === 'temps' || qm === 'points') qv = Math.round(num(+qv, 1, 600) ?? 10);
    if ((am === 'ouvre' || am === 'ferme') && !LETTRE.test(av)) { refus.push(`règle « ${am} » : quelle lettre ?`); continue; }
    if (am === 'message') { av = ascii(av).slice(0, 20); if (!av) { refus.push('règle « message » : quel texte ?'); continue; } }
    if (am === 'vitesse') av = num(+av, .1, 3) ?? 1;
    o.regles.push({ quand: qv === undefined ? qm : { [qm]: qv }, alors: av === undefined ? am : { [am]: av } });
  }
  const bt = motArg(f.but, BUTS) || ['arriver'];
  if (f.but !== undefined && !motArg(f.but, BUTS)) refus.push(`but : je ne connais pas « ${JSON.stringify(f.but)} »`);
  o.but = bt[0] === 'survivre' ? { survivre: Math.round(num(+bt[1], 1, 600) ?? 30) } : bt[0];
  return { fiche: o, refus };
};

// ---------- traduire : lettre de l'enfant -> brique du moteur (mêmes choix que le firmware) ----------
F.traduire = function (f) {
  const t = { '.': '.', S: 'S' }, m = f.modele;
  for (const [l, p] of Object.entries(f.briques)) {
    let o = p.arrivee ? 'G' : p.ramasse ? '*' : p.mortel ? '^' : p.solide ? (p.rebond ? 'R' : p.disparait ? 'C' : p.glisse >= .5 ? 'I' : '#') : '.';
    if (m === 'cote') o = o === 'C' ? 'T' : 'RI'.includes(o) ? '#' : o;
    if (m === 'iso') o = 'CRI'.includes(o) ? '#' : o;
    t[l] = o;
  }
  for (const [l, x] of Object.entries(f.objets)) {
    const pic = x.forme === 'pic', fixe = x.mouvement === 'fixe', proche = x.declencheur === 'proche' || x.declencheur === 'touche';
    if (m === 'plateforme') t[l] = x.forme === 'projectile' ? '.' : x.mouvement === 'patrouille' || x.forme === 'ennemi' ? 'E' : pic && proche && fixe ? 'H' : pic && fixe && !x.cache ? '^'
      : x.forme === 'scie' ? 'W' : pic ? 'H' : x.contact === 'ramasse' ? '*' : '#';   // aperçu : l'objet à sa place de départ
    else t[l] = pic && proche ? 'H' : pic && fixe ? '^' : pic ? (m === 'iso' ? 'P' : '^') : x.mouvement === 'patrouille' && m === 'cote' ? 'E' : x.forme === 'bloc' && fixe ? '#' : m === 'iso' ? '#' : '.';
  }
  return t;
};

// ---------- lire : la fiche en français, ce qui manque, ce qui empêche l'envoi ----------
const phraseObjet = (l, x) => {
  const F0 = FORMES[x.forme], Fm = F0[0].toUpperCase() + F0.slice(1), pts = x.points?.length ? x.points : [[0, 0]], d = x.duree ?? 1;
  const vers = (dx, dy) => [dx && `de ${Math.abs(dx)} case${s(Math.abs(dx))} vers la ${dx > 0 ? 'droite' : 'gauche'}`, dy && `de ${Math.abs(dy)} case${s(Math.abs(dy))} vers le ${dy > 0 ? 'bas' : 'haut'}`].filter(Boolean).join(' et ') || 'sur place';
  if (x.forme === 'projectile') return `⚡ ${l} : le projectile du bouton « tirer » (${x.vitesse ?? 10} cases par seconde). Il casse les ennemis.`;
  let t = `${Fm}`;
  if (x.mouvement === 'fixe') t += x.declencheur && x.declencheur !== 'toujours' ? '' : ' ne bouge pas.';
  else if (x.mouvement === 'aller_retour' || x.mouvement === 'boucle') {
    if (pts.length < 2) t += ' devrait bouger, mais il n\'a qu\'un point : donne-lui un trajet.';
    else if (pts.length === 2 && x.mouvement === 'aller_retour') t += ` va ${vers(pts[1][0] - pts[0][0], pts[1][1] - pts[0][1])} en ${d} s${x.pause ? `, attend ${x.pause} s` : ''} et revient.`;
    else t += ` ${x.mouvement === 'boucle' ? 'fait le tour de' : 'va et vient entre'} ses ${pts.length} points (${d} s par trajet${x.pause ? `, ${x.pause} s d'arrêt` : ''}).`;
  }
  else t += { patrouille: ' marche d\'un bout à l\'autre de sa plateforme.', tomber: ' tombe jusqu\'au sol.', suivre: ` te suit (${x.vitesse ?? 3} cases par seconde).` }[x.mouvement];
  const quand = x.declencheur === 'proche' ? `quand tu arrives à ${x.distance ?? 3} case${s(x.distance ?? 3)}.` : x.declencheur === 'touche' ? 'quand tu arrives dessus.' : '';
  if (quand) t += x.mouvement === 'fixe' ? (x.cache ? ' sort ' : ' s\'active ') + quand : ' Il démarre ' + quand;
  if (x.cache) t += ' Il est caché jusque-là.';
  t += ' ' + CONTACTS[x.contact];
  if (x.dessus === 'ecrase') t += ' On l\'écrase en lui sautant dessus.';
  return `⚙️ ${l} : ${t}`;
};
const phraseBrique = (l, p) => {
  const t = [p.solide && 'on marche dessus', p.glisse && 'ça glisse', p.rebond && `fait rebondir de ${p.rebond} case${s(p.rebond)}`, p.disparait && `disparaît ${p.disparait} s après qu'on a marché dessus`,
    p.mortel && 'la toucher fait perdre', p.ramasse && 'se ramasse', p.arrivee && 'c\'est l\'arrivée'].filter(Boolean);
  return `🧱 ${l} : ${t.join(', ') || 'du décor, on passe à travers'}.`;
};
const phraseRegle = r => {
  const [q, qv] = motArg(r.quand, QUAND), [a, av] = motArg(r.alors, ALORS);
  const Q = { touche: `Quand on touche ${qv}`, ramasse_tout: 'Quand tout est ramassé', ecrase_tout: 'Quand tous les ennemis sont écrasés', temps: `Au bout de ${qv} s`, points: `À ${qv} point${s(qv)}` }[q];
  const A = { gagne: 'on gagne', perd: 'on perd', ouvre: `${av} disparaît (ça s'ouvre)`, ferme: `${av} devient un mur`, vie_plus: 'une vie en plus', message: `le message « ${av} » s'affiche`, vitesse: `le héros va ${av} fois plus vite` }[a];
  return `📜 ${Q}, ${A}.`;
};
F.lire = function (f) {
  const phrases = [], manques = [], bloquants = [], H = f.heros, B = H.boutons, act = Object.values(B), g = f.monde.gravite, m = f.modele;
  const utilise = new Set(f.rows.join('')), t = F.traduire(f);
  phrases.push({ plateforme: '🎮 Un jeu de plateforme (écran de 20 cases de large).', cote: '🎮 Un jeu vu de côté (le monde défile).', iso: '🎮 Un jeu isométrique (case par case).' }[m]);
  for (const k of ['G', 'D', 'P3']) if (B[k] !== 'rien') phrases.push(`🔘 Quand j'appuie sur ${k} : ${ACTIONS[B[k]]}.`);
  if (act.every(a => a === 'rien')) manques.push('Quand j\'appuie sur G, D ou P3 : rien ne se passe. Explique ce que font les boutons !');
  if (m === 'iso') { if (!act.includes('avancer_case')) manques.push('Dans le jeu isométrique, il faut un bouton « avancer_case » pour avancer.'); }
  else {
    if (act.some(a => a.startsWith('aller_'))) H.vitesse > 0 ? phrases.push(`🏃 Il avance de ${H.vitesse} cases par seconde${H.elan >= .6 ? ', et glisse longtemps' : H.elan <= .05 ? ' et s\'arrête net' : ''}.`) : manques.push('Ton héros veut bouger, mais sa vitesse est 0 : dis à quelle vitesse il avance.');
    phrases.push(g ? `⬇️ La gravité tire vers le bas (${g} cases/s²).` : '🪶 Rien ne tombe : c\'est comme une vue de dessus.');
    if (act.includes('sauter')) {
      if (!g) manques.push('Sans gravité, sauter ne sert à rien : ajoute de la gravité, ou des boutons aller_haut / aller_bas.');
      else if (!(H.saut?.hauteur > 0)) manques.push('Le bouton « sauter » ne fera rien : dis de combien de cases il saute.');
      else phrases.push(`🦘 Il saute de ${H.saut.hauteur} case${s(H.saut.hauteur)}${H.saut.en_l_air ? `, et encore ${H.saut.en_l_air} fois en l'air` : ''}.`);
    }
    if (H.mur?.glisse || H.mur?.saut_mural) phrases.push(`🧗 Contre un mur : ${[H.mur.glisse && 'il glisse', H.mur.saut_mural && 'il peut sauter d\'un mur à l\'autre'].filter(Boolean).join(', ')}.`);
    if (f.monde.bord === 'mur') phrases.push('🧱 Le bas du niveau est un sol : on ne tombe pas dans le vide.');
    if (f.monde.bord === 'boucle') phrases.push('🔁 Le monde boucle : on ressort de l\'autre côté.');
    if (f.monde.camera === 'fixe') phrases.push('📷 La caméra ne bouge pas.');
  }
  phrases.push(H.vies ? `❤️ ${H.vies} vie${s(H.vies)}.` : '❤️ On recommence au départ à chaque fois.');
  for (const l of utilise) if (l !== '.' && l !== 'S' && !has(f.briques, l) && !has(f.objets, l)) bloquants.push(`Je ne sais pas ce qu'est la lettre ${l} : décris-la dans les briques ou les objets.`);
  for (const [l, p] of Object.entries(f.briques)) if (utilise.has(l)) phrases.push(phraseBrique(l, p));
  for (const [l, x] of Object.entries(f.objets)) if (utilise.has(l) || x.forme === 'projectile') phrases.push(phraseObjet(l, x));
  for (const r of f.regles) phrases.push(phraseRegle(r));
  // le but
  const cellules = l => [...f.rows.join('')].filter(c => c === l).length, nb = c => [...f.rows.join('')].filter(l => t[l] === c).length;
  const ram = [...f.rows.join('')].filter(l => f.briques[l]?.ramasse || f.objets[l]?.contact === 'ramasse').length;
  const ecr = [...f.rows.join('')].filter(l => f.objets[l] && (f.objets[l].forme === 'ennemi' || f.objets[l].dessus)).length;
  const [but, bv] = motArg(f.but, BUTS);
  phrases.push('🏁 Pour gagner : ' + { arriver: 'toucher l\'arrivée.', ramasser_tout: 'tout ramasser.', ecraser_tout: 'écraser tous les ennemis.', survivre: `tenir ${bv} s sans perdre.`, regles: 'ce que disent les règles.' }[but]);
  if (but === 'arriver' && !nb('G')) manques.push('Il n\'y a pas d\'arrivée : comment on gagne ?');
  if (but === 'ramasser_tout' && !ram) manques.push('Le but est de tout ramasser, mais il n\'y a rien à ramasser.');
  if (but === 'ecraser_tout' && !ecr) manques.push('Le but est d\'écraser tous les ennemis, mais il n\'y en a aucun.');
  if (but === 'regles' && !f.regles.some(r => r.alors === 'gagne')) manques.push('Aucune règle ne fait gagner.');
  for (const r of f.regles) { const [q, qv] = motArg(r.quand, QUAND), [a, av] = motArg(r.alors, ALORS);
    if (q === 'touche' && !cellules(qv)) manques.push(`La règle « quand on touche ${qv} » ne marchera pas : il n'y a pas de ${qv} dans la grille.`);
    if ((a === 'ouvre' || a === 'ferme') && !cellules(av)) manques.push(`La règle « ${a} ${av} » ne fait rien : il n'y a pas de ${av} dans la grille.`); }
  // la grille
  const T = TAILLES[m], w = Math.max(0, ...f.rows.map(r => r.length));
  if (f.rows.length < T[0] || f.rows.length > T[1] || w < T[2] || w > T[3]) bloquants.push(`La grille doit faire ${T[0]} à ${T[1]} lignes de ${T[2]} à ${T[3]} cases (elle fait ${f.rows.length} × ${w}).`);
  if (cellules('S') !== 1) bloquants.push('Il faut un seul départ S dans la grille.');
  if (m !== 'plateforme') {
    if (nb('G') !== 1) bloquants.push(`Dans ce jeu, il faut une seule case d'arrivée (il y en a ${nb('G')}).`);
    const nom = m === 'cote' ? 'le jeu de côté' : 'le jeu isométrique';
    if (f.regles.length || but !== 'arriver') manques.push(`Les règles et les autres buts ne marchent que dans la plateforme : dans ${nom}, on gagne en arrivant.`);
    for (const [l, x] of Object.entries(f.objets)) if (utilise.has(l) && x.mouvement !== 'fixe' && !(m === 'cote' && x.mouvement === 'patrouille') && !(m === 'iso' && x.forme === 'pic'))
      manques.push(`Dans ${nom}, ${l} ne bouge pas (${t[l] === '.' ? 'il est enlevé' : t[l] === '#' ? 'il devient du sol' : 'il devient un pic'}).`);
    if (m === 'cote' && Object.values(f.briques).some(p => p.glisse || p.rebond)) manques.push('Dans le jeu de côté, la glace et les ressorts deviennent du sol normal.');
  }
  return { phrases, manques, bloquants };
};

// ---------- pour l'aperçu de la page : la grille dans l'alphabet du moteur ----------
F.apercu = function (niv) {
  const f = niv.fiche, t = F.traduire(f);
  const rows = niv.rows.map(r => [...r].map(c => t[c] ?? '.').join(''));
  return { ...niv, rows, params: { ...niv.params, jeu: f.modele === 'plateforme' ? 'plateforme' : f.modele, haut: f.haut } };
};
F.ligne = f => { const { rows, ...reste } = f; return reste; };   // ce qui part sur la console avec rows
F.fusion = function fusion(a, b) {                     // changements de l'IA : null efface, les objets se fusionnent, le reste remplace
  if (!b || typeof b !== 'object' || Array.isArray(b)) return b === undefined ? a : b;
  const o = a && typeof a === 'object' && !Array.isArray(a) ? { ...a } : {};
  for (const [k, v] of Object.entries(b)) if (v === null) delete o[k]; else o[k] = fusion(o[k], v);
  return o;
};
F.poser = function (f, l, x, y) {                      // pose une lettre dans la grille (x = colonne depuis la gauche, y = ligne depuis le haut)
  if (typeof l !== 'string' || !LETTRE.test(l) || !Number.isInteger(x) || !Number.isInteger(y) || !f.rows[y] || x < 0 || x >= TAILLES[f.modele][3] || l === 'S') return false;
  const r = f.rows[y].padEnd(x + 1, '.'); f.rows[y] = r.slice(0, x) + l + r.slice(x + 1); return true;
};

// ---------- les points de départ ----------
const STD = { G: 'aller_gauche', D: 'aller_droite', P3: 'sauter' };
F.DEPARTS = {
  vide: { modele: 'plateforme', monde: { gravite: 0 }, heros: { boutons: { G: 'rien', D: 'rien', P3: 'rien' } }, briques: { '#': { solide: 1 } }, objets: {}, regles: [], but: 'arriver',
          rows: [...Array(13).fill('.'.repeat(20)), '..S.................', '#'.repeat(20)] },
  plateforme: { modele: 'plateforme', monde: { gravite: 87.5, bord: 'vide', camera: 'suit' },   // les valeurs du firmware : la plateforme d'aujourd'hui
    heros: { boutons: STD, vitesse: 12.5, elan: 0.2, saut: { hauteur: 4.5, en_l_air: 0 }, mur: { glisse: 1, saut_mural: 1 }, vies: 0 },
    briques: { '#': { solide: 1 }, I: { solide: 1, glisse: 0.9 }, C: { solide: 1, disparait: 0.5 }, R: { solide: 1, rebond: 7 }, '^': { mortel: 1 }, '*': { ramasse: 1 }, G: { arrivee: 1 } },
    objets: { W: { forme: 'scie', mouvement: 'aller_retour', points: [[0, -3], [0, 0]], duree: 1.2, contact: 'perd' },
              E: { forme: 'ennemi', mouvement: 'patrouille', contact: 'perd', dessus: 'ecrase' },
              H: { forme: 'pic', declencheur: 'proche', distance: 2, cache: 1, contact: 'perd' } },
    regles: [], but: 'arriver' },
  cote: { modele: 'cote', monde: { gravite: 87.5 },
    heros: { boutons: STD, vitesse: 7.5, saut: { hauteur: 4.5, en_l_air: 0 }, mur: { glisse: 1, saut_mural: 1 }, vies: 0 },
    briques: { '#': { solide: 1 }, T: { solide: 1, disparait: 1 }, '^': { mortel: 1 }, '*': { ramasse: 1 }, G: { arrivee: 1 } },
    objets: { E: { forme: 'ennemi', mouvement: 'patrouille', contact: 'perd', dessus: 'ecrase' }, H: { forme: 'pic', declencheur: 'proche', distance: 2, cache: 1, contact: 'perd' } },
    regles: [], but: 'arriver' },
  iso: { modele: 'iso', monde: { gravite: 0 }, heros: { boutons: { G: 'tourner_gauche', D: 'tourner_droite', P3: 'avancer_case' }, vies: 0 },
    briques: { '#': { solide: 1 }, '^': { mortel: 1 }, '*': { ramasse: 1 }, G: { arrivee: 1 } },
    objets: { P: { forme: 'pic', mouvement: 'aller_retour', points: [[0, 0], [0, 0]], duree: 1, contact: 'perd' }, H: { forme: 'pic', declencheur: 'proche', distance: 1, cache: 1, contact: 'perd' } },
    regles: [], but: 'arriver' },
};
})(typeof module === 'object' ? module.exports : (window.Fiche = {}));
