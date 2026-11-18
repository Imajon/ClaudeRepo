// Jeux « à bulles » (tranche 6-9 ans) : l'enfant touche des bulles, la page assemble un programme Berry à partir de PIÈCES déjà testées (jeu, personnage, couleur, monde, action).
// Pas d'IA pour le code : chaque pièce est du Berry écrit et joué dans verif (node test-bulles.js), donc toute combinaison marche du premier coup.
// Même fichier pour la page (window.JeuxBulles) et le serveur / les tests (require). Ajouter un SYSTÈME de jeu = une entrée dans SYSTEMES + ses bulles dans JEUX.
(function (M) {
  const rgb = c => `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
  const mel = (a, b, k) => a.map((v, i) => Math.round(v + (b[i] - v) * k));
  const fonce = c => c.map(v => Math.round(v * 0.6)), clair = c => c.map(v => Math.min(255, Math.round(v * 1.2 + 45)));
  const liste0 = (n, f) => Array.from({ length: n }, (_, i) => f(i));
  const liste = (n, f) => '[' + liste0(n, f).join(', ') + ']';

  // ---------- les bulles (étape -> choix) ; id = ce que le générateur comprend, bouton = ce que l'enfant voit ----------
  const ETAPES = [
    // ARBORESCENCE : un genre (grande famille) -> un jeu de ce genre (les bulles « genre: » ne s'affichent que sous le genre choisi). Trois genres au départ ; les autres arrivent avec « Des idées pour toi » (plus: true).
    // À construire (pas encore de moteur, donc pas de bulle) : casse-briques, tetris, labyrinthe. Un nouveau moteur = une entrée dans SYSTEMES + sa bulle ici.
    { id: 'genre', libre: true, h: 210, icone: '🎮', nom: 'Le genre', q: 'Que fais-tu dans ton jeu ?', bulles: [
      { id: 'plateforme', bouton: '🏃 Je saute de plateforme en plateforme' }, { id: 'runner', bouton: '💨 Je cours tout seul et j’évite les obstacles' }, { id: 'invader', bouton: '🛸 Je reste en bas, tout vient vers moi' },
      { id: 'quiz', bouton: '❓ Je réponds à des questions', plus: true }, { id: 'soigne', bouton: '🐣 Je m’occupe d’un personnage', plus: true }] },
    { id: 'jeu', libre: true, h: 235, icone: '🕹', nom: 'Le jeu', q: 'Quel jeu ?', bulles: [
      { id: 'plat-but', genre: 'plateforme', bouton: '🏁 Je cours et je saute jusqu’au drapeau' }, { id: 'plat-attrape', genre: 'plateforme', bouton: '⭐ Je saute pour ramasser toutes les étoiles' }, { id: 'plat-evite', genre: 'plateforme', bouton: '👾 Je saute par-dessus les monstres' }, { id: 'plat-course', genre: 'plateforme', bouton: '⏱ Je cours plus vite que mon rival' },
      { id: 'cote', genre: 'runner', bouton: '🏃 Je cours en sautant les obstacles' }, { id: 'derriere', genre: 'runner', bouton: '🚗 Je conduis et je change de voie' }, { id: 'dessus', genre: 'runner', bouton: '🛰 Je pilote ma voiture sur la piste' }, { id: 'iso', genre: 'runner', bouton: '🧊 Je roule en 3D en diagonale' },
      { id: 'attrape', genre: 'invader', bouton: '🧺 J’attrape ce qui tombe' }, { id: 'esquive', genre: 'invader', bouton: '💨 J’évite ce qui tombe' }, { id: 'pluie', genre: 'invader', bouton: '🌧 J’attrape tout, rien ne doit tomber' },
      { id: 'quiz', genre: 'quiz', bouton: '❓ Je réponds à des questions' }, { id: 'soigne', genre: 'soigne', bouton: '🐣 Je m’occupe d’un personnage' }] },
    { id: 'figure', h: 20, icone: '🏍', nom: 'Mon véhicule', q: 'Sur quoi roules-tu ?', bulles: [{ id: 'auto', bouton: '🚗 En voiture' }, { id: 'moto', bouton: '🏍 En moto de motocross' }, { id: 'surf', bouton: '🏄 Sur une planche de surf' }] },
    { id: 'obst', h: 55, icone: '🚧', nom: 'Les obstacles', q: 'Des obstacles sur la route ?', bulles: [{ id: 'avec', bouton: '🚧 Avec obstacles' }, { id: 'sans', bouton: '🛣 Sans obstacles' }] },
    { id: 'perso', h: 28, icone: '🐱', nom: 'Mon personnage', q: 'Qui joues-tu ?', ia: true, bulles: [
      { id: 'chat', bouton: '🐱 Je joue un chat' }, { id: 'robot', bouton: '🤖 Je joue un robot' }, { id: 'dragon', bouton: '🐉 Je joue un dragon' }, { id: 'fusee', bouton: '🚀 Je joue une fusée' },
      { id: 'fantome', bouton: '👻 Je joue un fantôme' }, { id: 'magicien', bouton: '🧙 Je joue un magicien' }, { id: 'grenouille', bouton: '🐸 Je joue une grenouille' }, { id: 'hibou', bouton: '🦉 Je joue un hibou' }] },
    { id: 'couleur', h: 330, icone: '🎨', nom: 'Sa couleur', q: 'De quelle couleur ?', bulles: [
      { id: 'rouge', bouton: '🔴 Rouge' }, { id: 'orange', bouton: '🟠 Orange' }, { id: 'jaune', bouton: '🟡 Jaune' }, { id: 'vert', bouton: '🟢 Vert' },
      { id: 'bleu', bouton: '🔵 Bleu' }, { id: 'violet', bouton: '🟣 Violet' }, { id: 'rose', bouton: '🩷 Rose' }, { id: 'blanc', bouton: '⚪ Blanc' }] },
    { id: 'monde', h: 150, icone: '🌍', nom: 'Le monde', q: 'Où es-tu ?', bulles: [
      { id: 'foret', bouton: '🌳 Je joue une forêt' }, { id: 'ville', bouton: '🏙 La ville la nuit' }, { id: 'espace', bouton: '🌌 L’espace' },
      { id: 'eau', bouton: '🌊 Sous l’eau' }, { id: 'desert', bouton: '🏜 Je joue un désert' }, { id: 'neige', bouton: '❄️ La neige' }] },
    { id: 'bon', h: 100, icone: '🎁', nom: 'Ce que j’attrape', q: 'Que veux-tu attraper ?', ia: true, bulles: [] },
    { id: 'mal', h: 0, icone: '💣', nom: 'Ce que j’évite', q: 'Que veux-tu éviter ?', ia: true, bulles: [] },
    { id: 'action', h: 265, icone: '⚡', nom: 'Le gros bouton', q: 'Que fait le gros bouton ?', bulles: [
      { id: 'saut', bouton: '🦘 Sauter' }, { id: 'tir', bouton: '💥 Tirer' }, { id: 'bouclier', bouton: '🛡 Bouclier' }, { id: 'turbo', bouton: '💨 Turbo' }, { id: 'aimant', bouton: '🧲 Aimant' }] },
    // étapes de « seconde version » (v2) : proposées seulement une fois le premier jeu fabriqué, depuis « Modifier ma présentation »
    { id: 'fin', v2: true, h: 340, icone: '😵', nom: 'Quand je perds', q: 'Que dit le cartel quand tu perds ?', bulles: [
      { id: 'perdu', bouton: '😵 PERDU !' }, { id: 'oups', bouton: '😬 OUPS !' }, { id: 'over', bouton: '💀 GAME OVER' }, { id: 'encore', bouton: '😸 ENCORE !' }, { id: 'rate', bouton: '🔥 RATÉ !' }, { id: 'erreur', bouton: '🤖 ERREUR' }] },
    // les bulles « niv » sont fabriquées par nivBulles() à partir du nombre de niveaux actuel : ajouter un niveau, ou rester
    { id: 'niv', v2: true, h: 190, icone: '🪜', nom: 'Les niveaux', q: 'Un niveau de plus ?', bulles: [] },
    // plateforme à plusieurs niveaux : deux étapes indépendantes, donc combinables (la vitesse, elle, ne monte pas dans ces jeux)
    { id: 'plan', v2: true, h: 230, icone: '🗺', nom: 'Les plateformes', q: 'Difficulté : les plateformes des niveaux ?', bulles: [
      { id: 'non', bouton: '🧱 Les mêmes à chaque niveau' }, { id: 'oui', bouton: '🗺 À des endroits différents à chaque niveau' }] },
    { id: 'piege', v2: true, h: 270, icone: '👾', nom: 'Les pièges', q: 'Difficulté : et les pièges ?', bulles: [
      { id: 'non', bouton: '⚖️ Autant de pièges à chaque niveau' }, { id: 'oui', bouton: '👾 De plus en plus de pièges' }] }
  ];
  const DEFAUT = { genre: 'invader', jeu: 'attrape', obst: 'avec', perso: 'chat', couleur: 'orange', monde: 'foret', bon: 'monde', mal: 'monde', action: 'saut', figure: 'auto', fin: 'perdu', niv: 'un', plan: 'non', piege: 'non' };
  const CARTELS = { perdu: ['PERDU !'], oups: ['OUPS !', [255, 120, 100]], over: ['GAME OVER', [255, 70, 70]], encore: ['ENCORE !', [255, 225, 90]], rate: ['RATE !', [255, 150, 50]], erreur: ['ERREUR', [110, 230, 130]] };   // texte (sans accent : la console n'en dessine pas) et couleur ; 9 lettres au plus, le cartel fait 200 px
  const JEUX = ETAPES.find(e => e.id === 'jeu').bulles, GENRE_DE = Object.fromEntries(JEUX.map(b => [b.id, b.genre])), JEUX_DE = {}; JEUX.forEach(b => (JEUX_DE[b.genre] = JEUX_DE[b.genre] || []).push(b.id));
  const NIVEAUX = { un: 1, trois: 3, cinq: 5 }, NIV_MAX = 6;   // 6 niveaux au plus (PIEGES_PLUS : 2 monstres de plus par niveau)
  const nbNiv = id => NIVEAUX[id] || +String(id || '').slice(1) || 1;   // 'n4' = 4 niveaux ; les anciens 'un', 'trois', 'cinq' marchent encore
  const nivBulles = id => { const n = nbNiv(id), bs = [{ id: 'n' + n, bouton: '✋ Rester à ' + n + (n > 1 ? ' niveaux' : ' niveau') }]; if (n < NIV_MAX) bs.unshift({ id: 'n' + (n + 1), bouton: '➕ Ajouter un niveau (ça fera ' + (n + 1) + ')' }); return bs; };
// L'IA propose d'autres idées pour perso, bon et mal : chacune devient une image (icône de la page, numéro dans r.imgs) et son choix vaut 'img'. Sans image trouvée, retour au dessin du monde.
const COURSES = { cote: 1, derriere: 1, dessus: 1, iso: 1 }, NOUVEAUX = { 'plat-but': 1, 'plat-attrape': 1, 'plat-evite': 1, 'plat-course': 1, ...COURSES };   // les nouveaux jeux : le gros bouton a son rôle fixe (sauter, turbo), les obstacles ne comptent que pour les courses
const SOLO = { quiz: 1, soigne: 1 };   // ces deux jeux n'ont ni obstacles, ni objets, ni gros bouton à choisir
const saute = (id, r) => { r = r || {}; if (SOLO[r.genre]) r = { ...r, jeu: r.genre }; if (id === 'plan' || id === 'piege') return !(/^plat-/.test(r.jeu) && nbNiv(r.niv) > 1); if (id === 'jeu') return !!SOLO[r.genre]; if (SOLO[r.jeu] && ['obst', 'bon', 'mal', 'action', 'niv'].includes(id)) return true; if (id === 'figure') return !COURSES[r.jeu]; return id === 'couleur' || (id === 'bon' && r.jeu !== 'plat-attrape') || id === 'mal' || (id === 'obst' && !COURSES[r.jeu]) || (id === 'action' && !!NOUVEAUX[r.jeu]); };   // une image garde ses couleurs : pas d'étape « couleur »   // une étape sans choix : le jeu marche quand même

  const FIG = {"auto":["rect(x - 3, y + 16, 30, 9, C2) rect(x - 3, y + 16, 30, 3, C3) disque(x + 3, y + 25, 4, NO) disque(x + 21, y + 25, 4, NO)","rect(x - 2, y + 16, 28, 10, C2) rect(x - 2, y + 16, 28, 3, C3) rect(x, y + 25, 6, 4, NO) rect(x + 18, y + 25, 6, 4, NO)"],"moto":["disque(x + 2, y + 24, 5, NO) disque(x + 22, y + 24, 5, NO) rect(x + 2, y + 17, 20, 4, C2) rect(x + 18, y + 12, 3, 6, NO)","rect(x + 9, y + 18, 6, 10, NO) rect(x + 4, y + 17, 16, 3, C2)"],"surf":["rect(x - 3, y + 21, 30, 3, C2) tri(x + 27, y + 21, x + 27, y + 24, x + 32, y + 22, C2) rect(x - 4, y + 25, 32, 2, A3)","rect(x + 5, y + 20, 14, 5, C2) rect(x + 3, y + 25, 18, 2, A3)"]};   // le véhicule sous le personnage dans les courses : [vue de côté, vue de derrière / dessus]
  const COULEURS = { rouge: [225, 60, 55], orange: [240, 140, 40], jaune: [245, 205, 50], vert: [80, 190, 90], bleu: [70, 130, 230], violet: [150, 90, 210], rose: [240, 120, 175], blanc: [240, 240, 245] };
  // jeu : part de mauvais objets (%), un bon objet qui touche le sol coûte une vie ?, un mauvais objet évité rapporte un point ?
  const MODES = { attrape: { pb: 40, manque: false, esquive: false, nom: '' }, esquive: { pb: 100, manque: false, esquive: true, nom: 'Esquive ' }, pluie: { pb: 20, manque: true, esquive: false, nom: 'Pluie de tresors ', lent: 0.4 } };
  const NOMS = { chat: 'Chat', robot: 'Robot', dragon: 'Dragon', fusee: 'Fusee', fantome: 'Fantome', magicien: 'Magicien', grenouille: 'Grenouille', hibou: 'Hibou' };

  // ---------- les personnages : dessinés dans une boîte de 24 x 24 pixels, coin haut gauche en (x, y) ; C1 corps, C2 ombre, C3 reflet, BL blanc, NO sombre ----------
  const PERSOS = {
    chat: `  rect(x + 3, y + 12, 18, 12, C1)
  disque(x + 12, y + 11, 9, C1)
  tri(x + 3, y + 8, x + 4, y - 1, x + 10, y + 3, C1)
  tri(x + 21, y + 8, x + 20, y - 1, x + 14, y + 3, C1)
  tri(x + 5, y + 6, x + 5, y + 1, x + 8, y + 3, C3)
  tri(x + 19, y + 6, x + 19, y + 1, x + 16, y + 3, C3)
  rect(x + 4, y + 21, 6, 3, C2) rect(x + 14, y + 21, 6, 3, C2)
  ligne(x + 21, y + 21, x + 26, y + 15, C1) ligne(x + 22, y + 21, x + 27, y + 15, C1)
  disque(x + 8, y + 11, 2, BL) disque(x + 16, y + 11, 2, BL) rect(x + 8, y + 11, 1, 2, NO) rect(x + 16, y + 11, 1, 2, NO)
  tri(x + 11, y + 14, x + 13, y + 14, x + 12, y + 16, C2)
  ligne(x + 1, y + 14, x + 6, y + 15, BL) ligne(x + 23, y + 14, x + 18, y + 15, BL)`,
    robot: `  rect(x + 4, y + 9, 16, 13, C1)
  rect(x + 5, y + 1, 14, 9, C1)
  rect(x + 5, y + 1, 14, 2, C3)
  rect(x + 1, y + 11, 3, 8, C2) rect(x + 20, y + 11, 3, 8, C2)
  rect(x + 6, y + 21, 5, 3, C2) rect(x + 13, y + 21, 5, 3, C2)
  rect(x + 8, y + 4, 3, 3, BL) rect(x + 14, y + 4, 3, 3, BL) rect(x + 9, y + 5, 1, 2, NO) rect(x + 15, y + 5, 1, 2, NO)
  ligne(x + 12, y + 1, x + 12, y - 4, C2) disque(x + 12, y - 5, 2, C3)
  rect(x + 8, y + 13, 8, 5, C2) disque(x + 12, y + 15, 1, BL)`,
    dragon: `  tri(x + 1, y + 20, x - 4, y + 15, x + 5, y + 14, C1)
  disque(x + 11, y + 15, 8, C1)
  disque(x + 10, y + 18, 4, C3)
  tri(x + 3, y + 11, x + 6, y + 3, x + 9, y + 10, C2) tri(x + 8, y + 9, x + 12, y + 1, x + 15, y + 9, C2)
  disque(x + 16, y + 8, 6, C1) rect(x + 19, y + 7, 5, 5, C1)
  disque(x + 17, y + 6, 2, BL) rect(x + 18, y + 6, 1, 2, NO) rect(x + 22, y + 8, 1, 2, NO)
  rect(x + 5, y + 21, 5, 3, C2) rect(x + 13, y + 21, 5, 3, C2)`,
    fusee: `  rect(x + 7, y + 6, 10, 15, C1)
  tri(x + 7, y + 6, x + 17, y + 6, x + 12, y - 4, C2)
  rect(x + 9, y + 7, 2, 12, C3)
  disque(x + 13, y + 11, 3, BL) disque(x + 13, y + 11, 1, NO)
  tri(x + 7, y + 14, x + 1, y + 23, x + 7, y + 21, C2) tri(x + 17, y + 14, x + 23, y + 23, x + 17, y + 21, C2)
  tri(x + 9, y + 21, x + 15, y + 21, x + 12, y + 24 + int(temps() * 12) % 3 * 2, A4)`,
    fantome: `  disque(x + 12, y + 10, 9, C1)
  rect(x + 3, y + 10, 18, 11, C1)
  disque(x + 6, y + 21, 3, C1) disque(x + 12, y + 21 + int(temps() * 6) % 2, 3, C1) disque(x + 18, y + 21, 3, C1)
  disque(x + 8, y + 9, 3, BL) disque(x + 16, y + 9, 3, BL) disque(x + 9, y + 10, 1, NO) disque(x + 17, y + 10, 1, NO)
  disque(x + 12, y + 16, 2, NO)
  disque(x + 5, y + 13, 2, C3) disque(x + 19, y + 13, 2, C3)`,
    magicien: `  tri(x + 12, y + 9, x + 2, y + 24, x + 22, y + 24, C1)
  tri(x + 12, y + 9, x + 8, y + 24, x + 16, y + 24, C3)
  disque(x + 12, y + 11, 4, A4)
  tri(x + 8, y + 12, x + 16, y + 12, x + 12, y + 20, BL)
  tri(x + 6, y + 7, x + 18, y + 7, x + 12, y - 6, C2) rect(x + 4, y + 6, 16, 2, C2)
  disque(x + 12, y, 1, A3)
  ligne(x + 20, y + 17, x + 25, y + 7, A4) disque(x + 25, y + 6, 2, A3)
  disque(x + 10, y + 10, 1, NO) disque(x + 14, y + 10, 1, NO)`,
    grenouille: `  disque(x + 12, y + 16, 9, C1)
  disque(x + 12, y + 19, 5, C3)
  rect(x + 2, y + 20, 7, 4, C2) rect(x + 15, y + 20, 7, 4, C2)
  disque(x + 7, y + 7, 4, C1) disque(x + 17, y + 7, 4, C1)
  disque(x + 7, y + 7, 3, BL) disque(x + 17, y + 7, 3, BL) disque(x + 8, y + 8, 1, NO) disque(x + 18, y + 8, 1, NO)
  ligne(x + 6, y + 15, x + 18, y + 15, C2) ligne(x + 6, y + 16, x + 18, y + 16, C2)`,
    hibou: `  disque(x + 12, y + 14, 10, C1)
  disque(x + 12, y + 18, 6, C3)
  tri(x + 3, y + 8, x + 5, y - 1, x + 10, y + 4, C2) tri(x + 21, y + 8, x + 19, y - 1, x + 14, y + 4, C2)
  disque(x + 7, y + 10, 5, BL) disque(x + 17, y + 10, 5, BL) disque(x + 8, y + 11, 2, NO) disque(x + 16, y + 11, 2, NO)
  tri(x + 10, y + 14, x + 14, y + 14, x + 12, y + 19, A3)
  rect(x + 7, y + 22, 3, 2, A3) rect(x + 14, y + 22, 3, 2, A3)`
  };

  // ---------- les mondes : ciel (haut, bas), sol, 4 couleurs d'ambiance A1..A4, décor, bon objet, mauvais objet (centrés en (x, y), rayon 8) ----------
  const MONDES = {
    foret: { nom: 'dans la foret', ciel: [[130, 200, 240], [200, 235, 200]], sol: [[70, 140, 60], [105, 185, 80]], a: [[255, 225, 90], [255, 255, 255], [110, 75, 45], [40, 120, 70]], n: 8,
      decor: `  var t = int(temps() * 4)
  disque(270, 40, 16, A1)
  while i < 3
    x = (DX[i] * 2 + t) % 380 - 30 y = 28 + DY[i] % 40
    disque(x, y, 9, A2) disque(x + 10, y + 4, 7, A2) disque(x - 9, y + 5, 6, A2)
    i += 1
  end
  i = 0
  while i < 8
    x = DX[i] h = 22 + DZ[i]
    rect(x - 3, 208 - h, 6, h, A3)
    tri(x - 16, 216 - h, x + 16, 216 - h, x, 180 - h, A4)
    i += 1
  end`,
      bon: `  disque(x, y + 1, 7, rgb(225, 55, 50)) disque(x - 3, y - 2, 2, rgb(255, 170, 160))
  rect(x - 1, y - 8, 2, 4, rgb(110, 75, 45)) tri(x + 1, y - 7, x + 7, y - 9, x + 4, y - 4, rgb(70, 170, 70))`,
      mal: `  disque(x, y, 8, rgb(120, 120, 130)) disque(x - 3, y - 3, 3, rgb(175, 175, 185)) disque(x + 3, y + 3, 2, rgb(85, 85, 95))
  rect(x - 4, y, 2, 2, NO) rect(x + 2, y, 2, 2, NO)` },
    ville: { nom: 'en ville la nuit', ciel: [[15, 15, 50], [75, 40, 105]], sol: [[50, 50, 62], [85, 85, 100]], a: [[250, 240, 190], [35, 35, 75], [255, 215, 100], [22, 22, 48]], n: 8,
      decor: `  var t = int(temps() / 3) var c = 0
  disque(44, 38, 14, A1) disque(50, 34, 12, BAND[1])
  while i < 8
    x = DX[i] h = 40 + DZ[i] * 3
    rect(x - 17, 208 - h, 34, h, A2)
    j = 0
    while j < 3
      y = 218 - h + j * 14
      c = A4 if (i * 5 + j * 3 + t) % 4 != 0 c = A3 end
      rect(x - 11, y, 6, 8, c)
      c = A4 if (i * 3 + j * 5 + t) % 3 != 0 c = A3 end
      rect(x + 5, y, 6, 8, c)
      j += 1
    end
    i += 1
  end`,
      bon: `  disque(x, y, 8, rgb(230, 175, 40)) disque(x, y, 6, rgb(255, 220, 90)) rect(x - 1, y - 4, 2, 8, rgb(230, 175, 40)) rect(x - 3, y - 4, 6, 1, rgb(230, 175, 40))`,
      mal: `  disque(x, y + 1, 8, rgb(40, 40, 50)) disque(x - 3, y - 2, 2, rgb(110, 110, 125))
  ligne(x + 4, y - 5, x + 7, y - 9, rgb(160, 120, 70))
  disque(x + 7, y - 10, 2 + int(temps() * 8) % 2, rgb(255, 140, 40))` },
    espace: { nom: 'dans l espace', ciel: [[3, 3, 18], [45, 22, 85]], sol: [[95, 90, 115], [135, 130, 155]], a: [[255, 255, 255], [150, 90, 200], [110, 60, 160], [255, 240, 160]], n: 24,
      decor: `  var t = int(temps() * 2)
  while i < 24
    if (i + t) % 4 != 0 rect(DX[i], DY[i] % 200, 2, 2, A1) end
    i += 1
  end
  disque(70, 70, 26, A2) disque(62, 62, 8, A3) disque(80, 80, 5, A3) ligne(36, 82, 106, 56, A1)`,
      bon: `  tri(x, y - 9, x - 8, y + 4, x + 8, y + 4, rgb(255, 225, 70)) tri(x, y + 9, x - 8, y - 4, x + 8, y - 4, rgb(255, 225, 70))`,
      mal: `  disque(x, y, 8, rgb(130, 120, 125)) disque(x - 3, y - 2, 3, rgb(90, 82, 88)) disque(x + 3, y + 3, 2, rgb(90, 82, 88)) disque(x + 2, y - 4, 1, rgb(175, 168, 172))` },
    eau: { nom: 'sous l eau', ciel: [[85, 205, 235], [12, 60, 135]], sol: [[225, 205, 140], [248, 232, 172]], a: [[205, 242, 255], [40, 150, 90], [250, 150, 50], [255, 255, 255]], n: 10,
      decor: `  var t = int(temps() * 14) var s = int(temps() * 3)
  while i < 10
    cercle(DX[i] + s % 3, 200 - (t + DY[i] * 2) % 200, 2 + DZ[i] % 3, A1)
    i += 1
  end
  i = 0
  while i < 6
    x = DX[i]
    ligne(x, 208, x + (s + i) % 3 * 3 - 3, 186 - DZ[i], A2) ligne(x + 1, 208, x + 1 + (s + i) % 3 * 3 - 3, 186 - DZ[i], A2) ligne(x + 2, 208, x + 2 + (s + i) % 3 * 3 - 3, 186 - DZ[i], A2)
    i += 1
  end
  x = t * 2 % 360 - 20
  disque(x, 76, 6, A3) tri(x - 5, 76, x - 13, 70, x - 13, 82, A3) disque(x + 3, 74, 1, NO)`,
      bon: `  disque(x, y, 7, rgb(250, 250, 255)) disque(x - 2, y - 2, 3, rgb(255, 210, 230)) cercle(x, y, 7, rgb(190, 200, 225))`,
      mal: `  disque(x, y, 5, rgb(70, 30, 90))
  ligne(x - 9, y, x + 9, y, rgb(70, 30, 90)) ligne(x, y - 9, x, y + 9, rgb(70, 30, 90)) ligne(x - 7, y - 7, x + 7, y + 7, rgb(70, 30, 90)) ligne(x - 7, y + 7, x + 7, y - 7, rgb(70, 30, 90))
  disque(x - 1, y - 1, 1, rgb(190, 130, 220))` },
    desert: { nom: 'dans le desert', ciel: [[255, 190, 95], [255, 238, 175]], sol: [[235, 190, 110], [250, 215, 142]], a: [[255, 240, 150], [225, 165, 85], [70, 150, 80], [255, 255, 215]], n: 8,
      decor: `  disque(260, 50, 22, A1) disque(260, 50, 15, A4)
  while i < 5
    disque(DX[i] * 4 % 320, 222, 40 + DZ[i], A2)
    i += 1
  end
  i = 5
  while i < 8
    x = DX[i]
    rect(x - 3, 182, 6, 26, A3) rect(x - 11, 190, 4, 10, A3) rect(x - 11, 198, 10, 4, A3) rect(x + 7, 186, 4, 10, A3) rect(x + 3, 194, 6, 4, A3)
    i += 1
  end`,
      bon: `  tri(x, y - 9, x - 6, y + 1, x + 6, y + 1, rgb(70, 150, 235)) disque(x, y + 2, 6, rgb(70, 150, 235)) disque(x - 2, y + 1, 2, rgb(190, 225, 255))`,
      mal: `  tri(x - 6, y - 3, x, y - 14, x + 6, y - 3, rgb(230, 90, 30)) disque(x, y, 8, rgb(240, 110, 30)) disque(x, y, 5, rgb(255, 215, 70))` },
    neige: { nom: 'dans la neige', ciel: [[170, 200, 235], [238, 246, 255]], sol: [[245, 250, 255], [200, 220, 242]], a: [[255, 255, 255], [40, 110, 80], [255, 255, 255], [100, 70, 50]], n: 18,
      decor: `  var t = int(temps() * 30) var s = int(temps() * 3)
  while i < 6
    x = DX[i]
    rect(x - 2, 198, 4, 10, A4)
    tri(x - 15, 204, x + 15, 204, x, 168, A2) tri(x - 11, 186, x + 11, 186, x, 154, A2) tri(x - 5, 164, x + 5, 164, x, 154, A3)
    i += 1
  end
  i = 0
  while i < 18
    rect(DX[i] + (s + i) % 5, (t + DY[i]) % 208, 2, 2, A1)
    i += 1
  end`,
      bon: `  rect(x - 7, y - 6, 14, 14, rgb(220, 60, 60)) rect(x - 1, y - 6, 2, 14, rgb(250, 210, 80)) rect(x - 7, y, 14, 2, rgb(250, 210, 80)) disque(x, y - 8, 2, rgb(250, 210, 80))`,
      mal: `  tri(x - 7, y - 9, x + 7, y - 9, x, y + 9, rgb(120, 205, 240)) tri(x - 4, y - 8, x + 1, y - 8, x - 1, y, rgb(215, 245, 255))` }
  };

  // ---------- les actions du gros bouton P3 : variables, remise à zéro, vitesse, mise à jour du joueur, effet sur chaque objet qui tombe, dessin ----------
  const ACTIONS = {
    saut: { vars: 'var sol = true var PVY = 0.0', raz: 'PVY = 0.0 sol = true',
      maj: `  if appui(P3) && sol PVY = -300.0 sol = false son(660, 40) end
  PVY += 900 * dt
  PY += PVY * dt
  if PY >= 184 PY = 184.0 PVY = 0.0 sol = true end`, objet: '', dessin: '' },
    tir: { vars: 'var CD = 0.0 var SX = [0.0, 0.0, 0.0] var SY = [0.0, 0.0, 0.0] var SV = [false, false, false]', raz: 'CD = 0.0\n  SV[0] = false SV[1] = false SV[2] = false',
      maj: `  CD -= dt
  if appui(P3) && CD <= 0
    i = 0
    while i < 3
      if !SV[i] SV[i] = true SX[i] = PX + 12 SY[i] = PY CD = 0.25 son(880, 30) i = 3 end
      i += 1
    end
  end
  i = 0
  while i < 3
    if SV[i]
      SY[i] -= 330 * dt
      if SY[i] < -10 SV[i] = false end
    end
    i += 1
  end`,
      objet: `      k = 0
      while k < 3
        if SV[k] && IV[i] && IK[i] == 1
          dx = IX[i] - SX[k] dy = IY[i] - SY[k]
          if dx * dx + dy * dy < 220
            IV[i] = false SV[k] = false PTS += 1 score(PTS) son(520, 50) effet(IX[i], IY[i])
          end
        end
        k += 1
      end`,
      dessin: `  i = 0
  while i < 3
    if SV[i] rect(int(SX[i]) - 2, int(SY[i]), 4, 10, BL) rect(int(SX[i]) - 1, int(SY[i]) + 2, 2, 6, C1) end
    i += 1
  end` },
    bouclier: { vars: 'var EN = 1.0 var BOU = false', raz: 'EN = 1.0 BOU = false',
      maj: `  if bouton(P3) && EN > 0
    BOU = true
    EN -= 0.5 * dt
    if EN < 0 EN = 0.0 end
  else
    BOU = false
    if !bouton(P3)
      EN += 0.25 * dt
      if EN > 1 EN = 1.0 end
    end
  end`,
      objet: `      if BOU && IK[i] == 1
        dx = IX[i] - cx dy = IY[i] - cy
        if dx * dx + dy * dy < 700 IV[i] = false son(300, 40) effet(IX[i], IY[i]) end
      end`,
      dessin: `  if BOU cercle(int(PX) + 12, int(PY) + 14, 19, BL) cercle(int(PX) + 12, int(PY) + 14, 18, A4) end
  rect(PX, 232, int(EN * 30), 3, BL)` },
    turbo: { vars: '', raz: '', vit: '  if bouton(P3) vit = 300.0 end',
      maj: '', objet: '',
      dessin: `  if bouton(P3) && (bouton(G) || bouton(D))
    rect(int(PX) + 2, int(PY) + 6, 3, 2, BL) rect(int(PX) + 19, int(PY) + 12, 3, 2, BL) rect(int(PX) + 3, int(PY) + 19, 3, 2, BL)
  end` },
    aimant: { vars: '', raz: '', maj: '',
      objet: `      if bouton(P3)
        dx = cx - IX[i] dy = cy - IY[i]
        if IK[i] == 0
          if dx * dx + dy * dy < 14400
            IX[i] += dx * 1.5 * dt
            IY[i] += dy * 0.8 * dt
          end
        elif dx * dx + dy * dy < 6400
          if dx > 0 IX[i] -= 90 * dt else IX[i] += 90 * dt end
        end
      end`,
      dessin: `  if bouton(P3)
    cercle(int(PX) + 12, int(PY) + 14, 14 + int(temps() * 8) % 3 * 5, A4)
  end` }
  };

  // ---------- le système « chute » : des objets tombent du haut, on les attrape (bons) ou on les évite (mauvais) ----------
  function chute(r) {
    const m = MODES[r.jeu], mo = MONDES[r.monde], ac = ACTIONS[r.action], col = COULEURS[r.couleur];
    const im = r.imgs || {};
    const nom = (m.nom + (NOMS[r.perso] || 'Heros') + ' ' + mo.nom).slice(0, 40);
    const dx = liste(mo.n, i => Math.round(14 + i * 292 / Math.max(1, mo.n - 1) + (i * 37) % 17 - 8)), dy = liste(mo.n, i => (i * 53 + 17) % 180), dz = liste(mo.n, i => (i * 7) % 14);
    const bandes = liste(10, i => rgb(mel(mo.ciel[0], mo.ciel[1], i / 9)));
    const perte = (cond) => `if ${cond} VIES -= 1 HT = 0.8 son(140, 160) effet(IX[i], IY[i]) if VIES <= 0 FIN = true son(100, 400) end end`;
    return `# ${nom}
var C1 = 0 var C2 = 0 var C3 = 0 var BL = 0 var NO = 0 var SOL = 0 var SOL2 = 0 var A1 = 0 var A2 = 0 var A3 = 0 var A4 = 0 var BAND = []
var DX = ${dx} var DY = ${dy} var DZ = ${dz}
var PX = 148.0 var PY = 184.0 var PTS = 0 var VIES = 3 var FIN = false var HT = 0.0 var SP = 0.6
var IX = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var IY = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var IK = [0, 0, 0, 0, 0, 0] var IV = [false, false, false, false, false, false]
var FX = [0.0, 0.0, 0.0, 0.0] var FY = [0.0, 0.0, 0.0, 0.0] var FT = [0.0, 0.0, 0.0, 0.0]
${ac.vars}

def effet(x, y)
  var i = 0
  while i < 4
    if FT[i] <= 0 FX[i] = x FY[i] = y FT[i] = 0.4 return end
    i += 1
  end
end

def decor()
  var i = 0 var j = 0 var x = 0 var y = 0 var h = 0
${mo.decor}
end

def objet(x, y, k)
  if k == 0
${r.bon === 'img' ? `  image(${im.bon}, x - 8, y - 8, 0, 1)` : mo.bon}
  else
${r.mal === 'img' ? `  image(${im.mal}, x - 8, y - 8, 0, 1)` : mo.mal}
  end
end

def perso(x, y0)
  var y = y0 + int(temps() * 5) % 2
${r.perso === 'img' ? `  image(${im.perso}, x, y, 0, 1)` : PERSOS[r.perso]}
end

def nouvelle()
  var i = 0
  PX = 148.0 PY = 184.0 PTS = 0 VIES = 3 FIN = false HT = 0.0 SP = 0.6
  while i < 6 IV[i] = false i += 1 end
  i = 0
  while i < 4 FT[i] = 0.0 i += 1 end
  ${ac.raz}
  score(0)
end

def debut()
  C1 = rgb(${col.join(', ')}) C2 = rgb(${fonce(col).join(', ')}) C3 = rgb(${clair(col).join(', ')}) BL = rgb(255, 255, 255) NO = rgb(30, 30, 45)
  SOL = ${rgb(mo.sol[0])} SOL2 = ${rgb(mo.sol[1])}
  A1 = ${rgb(mo.a[0])} A2 = ${rgb(mo.a[1])} A3 = ${rgb(mo.a[2])} A4 = ${rgb(mo.a[3])}
  BAND = ${bandes}
  nouvelle()
end

def maj(dt)
  var i = 0 var k = 0 var dx = 0.0 var dy = 0.0 var cx = 0.0 var cy = 0.0 var vit = 150.0 var vc = 70.0 var cad = 1.1 var touche = false
  i = 0
  while i < 4
    if FT[i] > 0 FT[i] -= dt end
    i += 1
  end
  if FIN
    if appui(P3) nouvelle() end
    return
  end
  if HT > 0 HT -= dt end
${ac.vit || ''}
  if bouton(G) PX -= vit * dt end
  if bouton(D) PX += vit * dt end
  if PX < 0 PX = 0.0 elif PX > 296 PX = 296.0 end
${ac.maj}
  cx = PX + 12 cy = PY + 14
  vc = 70.0 + PTS * 2.0
  if vc > 150.0 vc = 150.0 end
  SP -= dt
  if SP <= 0
    cad = ${(1.1 + (m.lent || 0)).toFixed(1)} - PTS * 0.015${m.esquive ? '\n    cad = cad * 0.7' : ''}
    if cad < 0.4 cad = 0.4 end
    SP = cad
    i = 0
    while i < 6
      if !IV[i]
        IV[i] = true IX[i] = real(hasard(12, 308)) IY[i] = -10.0 IK[i] = 0
        if hasard(1, 100) <= ${m.pb} IK[i] = 1 end
        i = 6
      end
      i += 1
    end
  end
  i = 0
  while i < 6
    if IV[i]
      IY[i] += vc * dt
${ac.objet}
      if IV[i]
        touche = IX[i] + 8 > PX + 3 && IX[i] - 8 < PX + 21 && IY[i] + 8 > PY + 4 && IY[i] - 8 < PY + 24
        if touche
          IV[i] = false
          if IK[i] == 0
            PTS += 1 score(PTS) son(988, 50) effet(IX[i], IY[i])
          else
            ${perte('HT <= 0')}
          end
        elif IY[i] > 208
          IV[i] = false
          if IK[i] == 1
            ${m.esquive ? 'PTS += 1 score(PTS)' : 'HT = HT'}
          else
            ${m.manque ? perte('true') : 'HT = HT'}
          end
        end
      end
    end
    i += 1
  end
end

def dessine()
  var i = 0 var t = temps()
  fond(BAND[0])
  if HT > 0 camera(hasard(-2, 2), hasard(-2, 2)) end
  while i < 10 rect(0, i * 24, 320, 24, BAND[i]) i += 1 end
  decor()
  rect(0, 208, 320, 32, SOL) rect(0, 208, 320, 4, SOL2)
  i = 0
  while i < 8 rect(i * 41 + 7, 218 + i % 3 * 7, 8, 2, SOL2) i += 1 end
  i = 0
  while i < 6
    if IV[i] objet(int(IX[i]), int(IY[i]), IK[i]) end
    i += 1
  end
  rect(int(PX) + 3, 207, 18, 3, SOL2)
  if HT <= 0 || int(t * 12) % 2 == 0 perso(int(PX), int(PY)) parle(int(PX), int(PY), 0) end
${ac.dessin}
  i = 0
  while i < 4
    if FT[i] > 0 cercle(int(FX[i]), int(FY[i]), int((0.4 - FT[i]) * 60) + 2, A4) end
    i += 1
  end
  camera(0, 0)
  texte(9, 9, str(PTS), NO, 3) texte(8, 8, str(PTS), BL, 3)
  i = 0
  while i < 3
    var c = rgb(80, 80, 90)
    if i < VIES c = rgb(235, 60, 80) end
    disque(296 - i * 20, 14, 5, c) disque(306 - i * 20, 14, 5, c) tri(289 - i * 20, 16, 313 - i * 20, 16, 301 - i * 20, 28, c)
    i += 1
  end
  if FIN
    rect(60, 80, 200, 84, NO) cadre(60, 80, 200, 84, BL)
    texte(92, 90, "PERDU !", BL, 3)
    texte(100, 122, "Score " + str(PTS), BL, 2)
    texte(88, 144, "P3 : rejouer", A4, 1)
  end
end
`;
  }

  // ---------- les systèmes « plateforme » (4 mécaniques) et « course » (de côté, de derrière, de dessus), avec ou sans obstacles ----------
  const PLATEFORMES = {
    'plat-but': { nom: 'Plateforme ', hz: [330, 560, 760], vh: 0, fin: 'drapeau', rival: 0 },
    'plat-attrape': { nom: 'Pieces ', hz: [330, 560, 760], vh: 0, fin: 'etoiles', rival: 0 },
    'plat-evite': { nom: 'Monstres ', hz: [200, 320, 440, 560, 680, 800], vh: 45, fin: 'drapeau', rival: 0 },
    'plat-course': { nom: 'Course ', hz: [330, 560, 760], vh: 0, fin: 'drapeau', rival: 85 }
  };
  const PLATS = [[150, 168, 70], [270, 128, 70], [390, 168, 70], [500, 128, 80], [620, 168, 70], [730, 128, 70], [830, 168, 60]];   // x, haut, largeur : chacune se saute depuis la précédente
  // les plateformes de chaque niveau (7 par niveau, toutes atteignables : montée <= 40 px, trou <= 60 px) ; le niveau k prend la disposition (k + 1) % 8
  const PLANS = [PLATS,
    [[140, 168, 70], [250, 148, 60], [360, 128, 70], [470, 108, 60], [580, 128, 70], [700, 148, 60], [810, 168, 60]],
    [[150, 168, 70], [260, 148, 70], [370, 128, 70], [480, 148, 70], [590, 128, 70], [700, 108, 70], [810, 128, 70]],
    [[150, 168, 60], [265, 128, 60], [380, 108, 60], [490, 128, 60], [600, 148, 60], [715, 128, 60], [830, 168, 60]],
    [[150, 168, 80], [270, 148, 50], [370, 168, 50], [480, 148, 50], [580, 128, 60], [690, 148, 60], [800, 168, 60]],
    [[140, 168, 80], [260, 148, 60], [375, 168, 60], [490, 148, 70], [610, 128, 60], [720, 148, 60], [830, 168, 60]],
    [[150, 168, 60], [255, 128, 60], [360, 168, 60], [465, 128, 60], [570, 168, 60], [675, 128, 60], [780, 168, 60]],
    [[140, 168, 70], [250, 168, 50], [350, 148, 50], [450, 128, 50], [550, 108, 50], [650, 128, 60], [770, 168, 60]]];   // banque de structures : en ajouter une = une ligne (7 plateformes x, haut, largeur, gap <= 60, montee <= 40)
  const DEC = [35, -35, 60, -60, 20, -20];   // décalage des monstres par niveau quand les plateformes changent de place
  const PIEGES_PLUS = [445, 655, 870, 260, 380, 500, 620, 740, 860, 150];   // monstres ajoutés (2 de plus par niveau) à ceux du jeu
  const GAGNE ='FIN = 1 PTS += 10 if T < 40 PTS += int(40 - T) end score(PTS) son(1318, 300)';

  function base(r, v) {   // ce que les nouveaux systèmes partagent : couleurs, décor du monde, personnage, objets, cœurs, fin de partie
    const mo = MONDES[r.monde], col = COULEURS[r.couleur], im = r.imgs || {}; r = { ...r, figure: FIG[r.figure] ? r.figure : 'auto' };
    const dx = liste(mo.n, i => Math.round(14 + i * 292 / Math.max(1, mo.n - 1) + (i * 37) % 17 - 8)), dy = liste(mo.n, i => (i * 53 + 17) % 180), dz = liste(mo.n, i => (i * 7) % 14);
    return {
      tete: `var C1 = 0 var C2 = 0 var C3 = 0 var BL = 0 var NO = 0 var SOL = 0 var SOL2 = 0 var RO = 0 var A1 = 0 var A2 = 0 var A3 = 0 var A4 = 0 var BAND = []
var DX = ${dx} var DY = ${dy} var DZ = ${dz}
var PTS = 0 var VIES = 3 var FIN = 0 var HT = 0.0 var T = 0.0 var V = ${v === undefined ? -1 : v}`,
      defs: `def couleurs()
  C1 = rgb(${col.join(', ')}) C2 = rgb(${fonce(col).join(', ')}) C3 = rgb(${clair(col).join(', ')}) BL = rgb(255, 255, 255) NO = rgb(30, 30, 45) RO = rgb(70, 70, 80)
  SOL = ${rgb(mo.sol[0])} SOL2 = ${rgb(mo.sol[1])}
  A1 = ${rgb(mo.a[0])} A2 = ${rgb(mo.a[1])} A3 = ${rgb(mo.a[2])} A4 = ${rgb(mo.a[3])}
  BAND = ${liste(10, i => rgb(mel(mo.ciel[0], mo.ciel[1], i / 9)))}
end

def decor()
  var i = 0 var j = 0 var x = 0 var y = 0 var h = 0
${mo.decor}
end

def objet(x, y, k)
  if k == 0
${r.bon === 'img' ? `  image(${im.bon}, x - 8, y - 8, 0, 1)` : mo.bon}
  else
${r.mal === 'img' ? `  image(${im.mal}, x - 8, y - 8, 0, 1)` : mo.mal}
  end
end

def vehicule(x, y)
  if V == 0
    ${FIG[r.figure][0]}
  else
    ${FIG[r.figure][1]}
  end
end

def perso(x, y0)
  var y = y0 + int(temps() * 5) % 2
  if V >= 0 vehicule(x, y0 - 5) y = y0 - 8 end
${r.perso === 'img' ? `  image(${im.perso}, x, y, 0, 1)` : PERSOS[r.perso]}
end

def coeurs()
  var i = 0 var c = 0
  while i < 3
    c = rgb(80, 80, 90)
    if i < VIES c = rgb(235, 60, 80) end
    disque(296 - i * 20, 14, 5, c) disque(306 - i * 20, 14, 5, c) tri(289 - i * 20, 16, 313 - i * 20, 16, 301 - i * 20, 28, c)
    i += 1
  end
end

def barre(a, b)
  rect(110, 8, 100, 4, NO)
  if b >= 0 disque(110 + b, 10, 3, BL) end
  disque(110 + a, 10, 4, C1)
end

def fin()
  if FIN > 0
    rect(60, 80, 200, 84, NO) cadre(60, 80, 200, 84, BL)
    if FIN == 1 texte(92, 90, "BRAVO !", BL, 3) else texte(92, 90, "PERDU !", BL, 3) end
    texte(100, 122, "Score " + str(PTS), BL, 2)
    texte(88, 144, "P3 : rejouer", A4, 1)
  end
end
`
    };
  }
  const nomJeu = (pre, r) => '# ' + (pre + (NOMS[r.perso] || 'Heros') + ' ' + MONDES[r.monde].nom).slice(0, 40) + '\n';

  // plateforme : on court et on saute sur un monde qui défile ; de côté : la piste est plate, on avance tout seul (G ralentit, D accélère)
  function plateforme(r) {
    const cote = r.jeu === 'cote', b = base(r, cote ? 0 : -1), o = r.obst === 'avec';
    const c = cote ? { nom: 'Course cote ', long: 1400, pl: false, hz: o ? [300, 480, 660, 840, 1020, 1200] : [], vh: 0, fin: 'drapeau', rival: 180, auto: true, saut: o }
      : { ...PLATEFORMES[r.jeu], long: 1000, pl: true, auto: false, saut: true };
    // plusieurs niveaux : les données de chaque niveau sont à plat (niveau k = tranche k) et charge() les recopie au début du niveau
    // niveau 1 placé à la main, jusqu'à 4 niveaux (r.edit : 7 plateformes [x, haut, largeur], 1 à 8 étoiles [x, y], 0 à 8 monstres x) : les autres niveaux ne changent pas
    const nb = (v, a, z) => Math.max(a, Math.min(z, Math.round(+v) || a)), e = r.edit;
    const ed = c.pl && nbNiv(r.niv) <= 4 && e && e.pl?.length === 7 ? { pl: e.pl.map(p => [nb(p[0], 0, 940), nb(p[1], 72, 192), nb(p[2], 30, 140)]), st: (e.st || []).slice(0, 8).map(q => [nb(q[0], 0, 990), nb(q[1], 20, 200)]), hz: (e.hz || []).slice(0, 8).map(x => nb(x, 70, 900)) } : null;
    const nn = c.pl ? nbNiv(r.niv) : 1, multi = nn > 1, plans = liste0(nn, k => k === 0 && ed ? ed.pl : r.plan === 'oui' ? PLANS[(k + 1) % PLANS.length] : PLATS);
    const pool = [...c.hz, ...PIEGES_PLUS], hzK = liste0(nn, k => k === 0 && ed ? ed.hz : pool.slice(0, c.hz.length + (r.piege === 'oui' ? 2 * k : 0)).map(x => r.plan === 'oui' ? x + DEC[k % 6] : x)), NH = Math.max(...hzK.map(h => h.length));
    const hzPad = k => Array.from({ length: NH }, (_, i) => i < hzK[k].length ? hzK[k][i] : -999);
    if (multi) c.hz = hzPad(0); else if (ed) c.hz = ed.hz;
    const pl = c.pl ? plans[0] : [], st = c.pl ? (ed?.st.length ? ed.st : [...pl.map(p => [p[0] + p[2] / 2, p[1] - 16]), [70, 192]]) : [], FL = c.long - 60, MAXX = c.long - 30, drap = c.fin === 'drapeau';
    const reels = a => liste(a.length, i => a[i].toFixed(1));
    return `${nomJeu(c.nom, r)}${b.tete}
var PX = 20.0 var PY = 184.0 var PVY = 0.0 var sol = true var CAMX = 0 var RX = 20.0
var PLX = ${liste(pl.length, i => pl[i][0])} var PT = ${liste(pl.length, i => pl[i][1])} var PW = ${liste(pl.length, i => pl[i][2])}
var SX = ${liste(st.length, i => st[i][0])} var SY = ${liste(st.length, i => st[i][1])} var SV = ${liste(st.length, () => 'true')}
var HX = ${reels(c.hz)} var HB = ${reels(c.hz)} var HD = ${liste(c.hz.length, () => '1.0')}
${multi ? `var LP = ${liste(nn * 7, i => plans[(i / 7) | 0][i % 7][0])} var LT = ${liste(nn * 7, i => plans[(i / 7) | 0][i % 7][1])} var LW = ${liste(nn * 7, i => plans[(i / 7) | 0][i % 7][2])}
var LH = ${liste(nn * NH, i => hzPad((i / NH) | 0)[i % NH].toFixed(1))}
${ed ? `var E1X = ${liste(st.length, i => st[i][0])} var E1Y = ${liste(st.length, i => st[i][1])}\n` : ''}` : ''}${b.defs}
${multi ? `def charge()
  var i = 0 var k = NV - 1
${ed ? `  while i < 7
    PLX[i] = LP[k * 7 + i] PT[i] = LT[k * 7 + i] PW[i] = LW[k * 7 + i]
    i += 1
  end
  i = 0
  while i < ${Math.min(7, st.length)}
    SX[i] = PLX[i] + PW[i] / 2 SY[i] = PT[i] - 16
    i += 1
  end
  if k == 0
    i = 0
    while i < ${st.length} SX[i] = E1X[i] SY[i] = E1Y[i] i += 1 end
  end
${st.length > 7 ? '  if k > 0 SX[7] = 70 SY[7] = 192 end\n' : ''}` : `  while i < 7
    PLX[i] = LP[k * 7 + i] PT[i] = LT[k * 7 + i] PW[i] = LW[k * 7 + i]
    SX[i] = PLX[i] + PW[i] / 2 SY[i] = PT[i] - 16
    i += 1
  end
`}  i = 0
  while i < ${NH} HB[i] = LH[k * ${NH} + i] i += 1 end
end

` : ''}def nouvelle()
  var i = 0
${multi ? '  charge()\n' : ''}  PX = 20.0 PY = 184.0 PVY = 0.0 sol = true RX = 20.0 PTS = 0 VIES = 3 FIN = 0 HT = 0.0 T = 0.0
  while i < ${st.length} SV[i] = true i += 1 end
  i = 0
  while i < ${c.hz.length} HX[i] = HB[i] HD[i] = 1.0 i += 1 end
  score(0)
end

def debut()
  couleurs()
  nouvelle()
end

def maj(dt)
  var i = 0 var of = 0.0 var f = 0.0 var vit = 150.0
  if FIN > 0
    if appui(P3) nouvelle() end
    return
  end
  T += dt
  if HT > 0 HT -= dt end
${c.auto ? `  vit = 140.0
  if bouton(D) vit = 190.0 end
  if bouton(G) vit = 90.0 end
${c.saut ? '' : '  if bouton(P3) vit += 60.0 end\n'}  PX += vit * dt` : `  if bouton(G) PX -= vit * dt end
  if bouton(D) PX += vit * dt end`}
  if PX < 0 PX = 0.0 elif PX > ${MAXX} PX = ${MAXX}.0 end
${c.saut ? '  if appui(P3) && sol PVY = -300.0 sol = false son(660, 40) end' : ''}
  of = PY + 24
  PVY += 900 * dt
  PY += PVY * dt
  f = PY + 24
  sol = false
  if f >= 208
    PY = 184.0 PVY = 0.0 sol = true
  elif PVY >= 0
    i = 0
    while i < ${pl.length}
      if of <= PT[i] + 1 && f >= PT[i] && PX + 20 > PLX[i] && PX + 4 < PLX[i] + PW[i]
        PY = PT[i] - 24.0 PVY = 0.0 sol = true i = ${pl.length}
      end
      i += 1
    end
  end
  i = 0
  while i < ${st.length}
    if SV[i] && SX[i] + 8 > PX + 3 && SX[i] - 8 < PX + 21 && SY[i] + 8 > PY + 4 && SY[i] - 8 < PY + 24
      SV[i] = false PTS += 1 score(PTS) son(988, 50)
    end
    i += 1
  end
  i = 0
  while i < ${c.hz.length}
${c.vh ? `    HX[i] += HD[i] * ${c.vh} * dt
    if HX[i] > HB[i] + 25 HD[i] = -1.0 elif HX[i] < HB[i] - 25 HD[i] = 1.0 end
` : ''}    if HT <= 0 && HX[i] + 8 > PX + 3 && HX[i] - 8 < PX + 21 && PY + 24 > 192 && PY + 4 < 208
      VIES -= 1 HT = 1.0 son(140, 160) PVY = -220.0 sol = false PX -= 30
      if PX < 0 PX = 0.0 end
      if VIES <= 0 FIN = 2 son(100, 400) end
    end
    i += 1
  end
${drap ? `  if FIN == 0 && PX + 12 >= ${FL}
    ${GAGNE}
  end` : `  if FIN == 0 && PTS >= ${st.length}
    ${GAGNE}
  end`}
${c.rival ? `  RX += ${c.rival} * dt
  if FIN == 0 && RX >= ${FL} FIN = 2 son(100, 400) end` : ''}
end

def dessine()
  var i = 0 var t = temps() var s = 0
  CAMX = int(PX - 140)
  if CAMX < 0 CAMX = 0 elif CAMX > ${c.long - 320} CAMX = ${c.long - 320} end
  fond(BAND[0])
  while i < 10 rect(0, i * 24, 320, 24, BAND[i]) i += 1 end
  s = CAMX * 3 / 10 % 320
  camera(s, 0) decor() camera(s - 320, 0) decor()
  camera(CAMX, 0)
  rect(0, 208, ${c.long}, 32, SOL) rect(0, 208, ${c.long}, 4, SOL2)
  i = 0
  while i < ${Math.ceil(c.long / 41)} rect(i * 41 + 7, 218 + i % 3 * 7, 8, 2, SOL2) i += 1 end
  i = 0
  while i < ${pl.length}
    rect(PLX[i], PT[i], PW[i], 8, SOL) rect(PLX[i], PT[i], PW[i], 3, SOL2)
    i += 1
  end
${drap ? `  rect(${FL}, 168, 3, 40, NO) tri(${FL + 3}, 168, ${FL + 3}, 180, ${FL + 20}, 174, A4)` : ''}
  i = 0
  while i < ${st.length}
    if SV[i] objet(SX[i], SY[i], 0) end
    i += 1
  end
  i = 0
  while i < ${c.hz.length}
    if HX[i] > CAMX - 20 && HX[i] < CAMX + 340 objet(int(HX[i]), 200, 1) end
    i += 1
  end
${c.rival ? `  if RX > CAMX - 30 && RX < CAMX + 340
    s = C1 C1 = rgb(95, 95, 105)
    perso(int(RX), 202)
    C1 = s
  end` : ''}
  rect(int(PX) + 3, 207, 18, 3, SOL2)
  if HT <= 0 || int(t * 12) % 2 == 0 perso(int(PX), int(PY)) parle(int(PX), int(PY), CAMX) end
  camera(0, 0)
  ${c.fin === 'etoiles' ? `texte(9, 9, str(PTS) + "/${st.length}", NO, 3) texte(8, 8, str(PTS) + "/${st.length}", BL, 3)` : 'texte(9, 9, str(PTS), NO, 3) texte(8, 8, str(PTS), BL, 3)'}
${drap ? `  barre(int(PX * 100 / ${c.long}), ${c.rival ? `int(RX * 100 / ${c.long})` : -1})` : ''}
${c.hz.length ? '  coeurs()' : ''}
  fin()
end
`;
  }

  // course de derrière (route en perspective) ou de dessus (route vue d'en haut) : on avance tout seul, G et D pour changer de voie, P3 = turbo ; les bons objets donnent un coup de vitesse, les mauvais (avec obstacles) font ralentir
  function course(r) {
    const b = base(r, 1), o = r.obst === 'avec', iso = r.jeu === 'iso', haut = r.jeu === 'dessus', rival = 118;
    const P = (s, l) => [Math.round(290 - 240 * s + 56 * l), Math.round(40 + 170 * s + 36 * l)], pt = (s, l) => P(s, l).join(', '), isoRoute = `  fond(SOL)
  tri(${pt(-0.1, -1.1)}, ${pt(-0.1, 1.1)}, ${pt(1.2, 1.1)}, RO) tri(${pt(-0.1, -1.1)}, ${pt(1.2, 1.1)}, ${pt(1.2, -1.1)}, RO)
  ligne(${pt(-0.1, -1.1)}, ${pt(1.2, -1.1)}, A4) ligne(${pt(-0.1, 1.1)}, ${pt(1.2, 1.1)}, A4)
  i = 0
  while i < 8
    s = real((DY[i] * 2 + int(P * 0.6)) % 240) / 240.0 * 1.3 - 0.05
    q = real(i % 2 * 2 - 1) * (1.5 + real(DZ[i] % 4) * 0.15)
    x = 290 - int(240.0 * s) + int(56.0 * q)
    y = 40 + int(170.0 * s) + int(36.0 * q)
    disque(x, y, 7, A2) disque(x - 2, y - 2, 3, A3)
    i += 1
  end
  q = P / 100.0
  i = 0
  while i < 6
    s = (real(i) + q - real(int(q))) / 6.0 * 1.3 - 0.05
    x = 290 - int(240.0 * s)
    y = 40 + int(170.0 * s)
    ligne(x, y, x - 14, y + 10, BL)
    i += 1
  end`;
    const route = iso ? isoRoute : haut
      ? `  fond(SOL)
  rect(60, 0, 200, 240, RO)
  ph = P * 0.6
  i = 0
  while i < 8
    x = i % 2 * 272 + DZ[i] % 8 * 5
    y = (DY[i] * 2 + int(ph)) % 240
    disque(x, y, 8, A2) disque(x - 2, y - 2, 4, A3)
    i += 1
  end
  i = 0
  while i < 6
    y = (i * 40 + int(ph)) % 240
    c = A4
    if i % 2 == 0 c = BL end
    rect(158, y, 4, 20, BL) rect(56, y, 4, 20, c) rect(260, y, 4, 20, c)
    i += 1
  end`
      : `  fond(BAND[0])
  while i < 10 rect(0, i * 24, 320, 24, BAND[i]) i += 1 end
  decor()
  rect(0, 96, 320, 144, SOL)
  tri(140, 96, 180, 96, -19, 240, RO) tri(180, 96, 339, 240, -19, 240, RO)
  ph = P / 100.0
  ph = ph - int(ph)
  i = 0
  while i < 6
    q = (i + ph) / 6.0
    s = q * q
    y = 96 + int(s * 144)
    rh = 20 + int(s * 159)
    w = 2 + int(s * 8)
    c = A4
    if i % 2 == 0 c = BL end
    rect(160 - w / 2, y, w, 2 + int(s * 12), BL)
    rect(160 - rh - 4 - int(s * 6), y, 4 + int(s * 10), 2 + int(s * 12), c)
    rect(160 + rh, y, 4 + int(s * 10), 2 + int(s * 12), c)
    i += 1
  end`;
    const pos = iso ? { ox: 'x = 290 - int(240.0 * s) + int(OL[i] * 56.0)', oy: 'y = 40 + int(170.0 * s) + int(OL[i] * 36.0)', px: 'x = 62 + int(PLX * 56.0)\n  y = 170 + int(PLX * 36.0)', py: 'y' }
      : haut ? { ox: 'x = 160 + int(OL[i] * 85)', oy: 'y = -10 + int((1.0 - OZ[i]) * 244)', px: 'x = 160 + int(PLX * 85) - 12', py: 188 }
      : { ox: 'x = 160 + int(OL[i] * (20 + s * 90))', oy: 'y = 96 + int(s * 104)', px: 'x = 160 + int(PLX * 97) - 12', py: 176 };
    return `${nomJeu(iso ? 'Course iso ' : haut ? 'Course dessus ' : 'Course derriere ', r)}${b.tete}
var P = 0.0 var RP = 0.0 var PLX = 0.0 var EN = 1.0 var BO = 0.0 var SP = 0.5
var OZ = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var OL = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var OK = [0, 0, 0, 0, 0, 0] var OV = [false, false, false, false, false, false]
${b.defs}
def nouvelle()
  var i = 0
  P = 0.0 RP = 0.0 PLX = 0.0 EN = 1.0 BO = 0.0 SP = 0.5 PTS = 0 VIES = 3 FIN = 0 HT = 0.0 T = 0.0
  while i < 6 OV[i] = false i += 1 end
  score(0)
end

def debut()
  couleurs()
  nouvelle()
end

def maj(dt)
  var i = 0 var v = 100.0
  if FIN > 0
    if appui(P3) nouvelle() end
    return
  end
  T += dt
  if HT > 0 HT -= dt end
  if BO > 0 BO -= dt end
  if bouton(G) PLX -= 1.6 * dt end
  if bouton(D) PLX += 1.6 * dt end
  if PLX < -1 PLX = -1.0 elif PLX > 1 PLX = 1.0 end
  if bouton(P3) && EN > 0
    v += 60.0
    EN -= 0.5 * dt
    if EN < 0 EN = 0.0 end
  elif !bouton(P3)
    EN += 0.2 * dt
    if EN > 1 EN = 1.0 end
  end
  if BO > 0 v += 50.0 end
  if HT > 0 v = v * 0.4 end
  P += v * dt
  RP += ${rival} * dt
  if P >= 2400
    ${GAGNE}
  elif RP >= 2400
    FIN = 2 son(100, 400)
  end
  SP -= dt
  if SP <= 0
    SP = 0.5 + hasard(0, 6) * 0.1
    i = 0
    while i < 6
      if !OV[i]
        OV[i] = true OZ[i] = 1.0 OL[i] = real(hasard(-90, 90)) / 100.0 OK[i] = 0
        if hasard(1, 100) <= ${o ? 45 : 0} OK[i] = 1 end
        i = 6
      end
      i += 1
    end
  end
  i = 0
  while i < 6
    if OV[i]
      OZ[i] -= v / 450.0 * dt
      if OZ[i] < 0.2 && OZ[i] > -0.04 && PLX - OL[i] < 0.22 && OL[i] - PLX < 0.22
        OV[i] = false
        if OK[i] == 0
          BO = 1.5 PTS += 1 score(PTS) son(988, 50)
        elif HT <= 0
          VIES -= 1 HT = 1.0 son(140, 160)
          if VIES <= 0 FIN = 2 son(100, 400) end
        end
      end
      if OZ[i] <= -0.04 OV[i] = false end
    end
    i += 1
  end
end

def dessine()
  var i = 0 var q = 0.0 var s = 0.0 var y = 0 var x = 0 var rh = 0 var w = 0 var ph = 0.0 var c = 0
${route}
  i = 0
  while i < 6
    if OV[i]
      s = 1.0 - OZ[i]
      ${pos.ox}
      ${pos.oy}
      objet(x, y, OK[i])
    end
    i += 1
  end
  ${pos.px}
  if bouton(P3) && EN > 0 rect(x + 8, ${pos.py} + 24, 8, 4 + int(temps() * 20) % 4 * 2, A4) end
  if HT <= 0 || int(temps() * 12) % 2 == 0 perso(x, ${pos.py}) parle(x, ${pos.py}, 0) end
  texte(9, 9, str(PTS), NO, 3) texte(8, 8, str(PTS), BL, 3)
  barre(int(P / 24.0), int(RP / 24.0))
  rect(8, 232, int(EN * 60), 4, BL)
  if BO > 0 texte(124, 20, "TURBO !", A4, 1) end
${o ? '  coeurs()' : ''}
  fin()
end
`;
  }

  // ---------- deux autres structures : « questions » (G / gros bouton / D = réponse 1, 2, 3) et « soigner un personnage » (G manger, gros bouton jouer, D dormir) ----------
  // question : deux lignes (23 lettres au plus, police sans accents), trois réponses (18 au plus), le numéro de la bonne
  const QUESTIONS = {
    foret: [['Quel animal fait', 'HOU HOU ?', 'La vache', 'Le hibou', 'Le poisson', 1], ['Que donne un', 'pommier ?', 'Des pommes', 'Des fraises', 'Des oeufs', 0], ['Qui vit dans un', 'terrier ?', 'Le requin', 'L\'aigle', 'Le lapin', 2], ['Que fait un arbre', 'en automne ?', 'Il vole', 'Il perd ses feuilles', 'Il chante', 1], ['Combien de pattes', 'a un ecureuil ?', 'Quatre', 'Dix', 'Zero', 0]],
    ville: [['Feu vert :', 'on fait quoi ?', 'On passe', 'On s\'arrete', 'On danse', 0], ['Que voit-on dans', 'le ciel la nuit ?', 'Un tracteur', 'Des etoiles', 'Une vague', 1], ['Qui conduit un', 'bus ?', 'Le chauffeur', 'Le boulanger', 'Le dragon', 0], ['Combien de roues', 'a un velo ?', 'Deux', 'Sept', 'Cent', 0], ['Ou attend-on', 'le train ?', 'Au zoo', 'A la piscine', 'A la gare', 2]],
    espace: [['Quelle planete', 'a des anneaux ?', 'Saturne', 'La Terre', 'Mars', 0], ['Que fait la Lune', 'la nuit ?', 'Elle sonne', 'Elle brille', 'Elle mange', 1], ['Qui voyage en', 'fusee ?', 'Un cosmonaute', 'Un cuisinier', 'Un facteur', 0], ['Quelle etoile nous', 'eclaire le jour ?', 'La Lune', 'Sirius', 'Le Soleil', 2], ['Dans l\'espace,', 'il y a de l\'air ?', 'Oui, partout', 'Non, pas du tout', 'Un peu', 1]],
    eau: [['Comment respire', 'un poisson ?', 'Avec le nez', 'Avec les doigts', 'Avec des branchies', 2], ['Combien de bras', 'a une pieuvre ?', 'Huit', 'Deux', 'Trois', 0], ['Quel est le plus', 'gros animal ?', 'La sardine', 'La baleine', 'Le crabe', 1], ['Comment avance', 'un crabe ?', 'Il vole', 'De cote', 'Il roule', 1], ['L\'eau de la mer', 'est comment ?', 'Sucree', 'Salee', 'Pimentee', 1]],
    desert: [['Combien de bosses', 'a un dromadaire ?', 'Une', 'Trois', 'Dix', 0], ['Qu\'y a-t-il', 'beaucoup ici ?', 'Du sable', 'De la neige', 'Des poissons', 0], ['Quel temps fait-il', 'dans le desert ?', 'Il gele', 'Il neige', 'Il fait chaud', 2], ['Quel animal', 'a un dard ?', 'Le scorpion', 'Le lapin', 'Le dauphin', 0], ['Que fait un cactus', 'avec l\'eau ?', 'Il la boit', 'Il la garde', 'Il la jette', 1]],
    neige: [['De quelle couleur', 'est la neige ?', 'Rouge', 'Blanche', 'Verte', 1], ['Que fait la neige', 'au soleil ?', 'Elle fond', 'Elle chante', 'Elle bout', 0], ['Quel animal vit', 'au pole nord ?', 'Le lion', 'L\'ours blanc', 'Le singe', 1], ['Que met-on pour', 'avoir chaud ?', 'Un maillot', 'Des tongs', 'Une echarpe', 2], ['Sur quoi glisse', 'un skieur ?', 'Sur des skis', 'Sur des roues', 'Sur du sable', 0]]
  };
  const bs = a => '[' + a.map(s => JSON.stringify(s)).join(', ') + ']';   // une liste de textes Berry

  function quiz(r) {   // 5 questions du monde choisi, 3 vies : G, gros bouton ou D donne la 1re, 2e ou 3e réponse
    const b = base(r), q = QUESTIONS[r.monde], n = q.length;
    return `${nomJeu('Quiz ', r)}${b.tete}
var QT = ${bs(q.flatMap(x => [x[0], x[1]]))}
var RP = ${bs(q.flatMap(x => [x[2], x[3], x[4]]))}
var BON = [${q.map(x => x[5]).join(', ')}]
var LB = ["G  ", "P3 ", "D  "]
var QI = 0 var ETAT = 0 var AT = 0.0 var CH = -1
${b.defs}
def nouvelle()
  QI = 0 ETAT = 0 AT = 0.0 CH = -1 PTS = 0 VIES = 3 FIN = 0 T = 0.0
  score(0)
end

def debut()
  couleurs()
  nouvelle()
end

def maj(dt)
  var k = -1
  if FIN > 0
    if appui(P3) nouvelle() end
    return
  end
  T += dt
  if ETAT > 0
    AT -= dt
    if AT <= 0
      ETAT = 0 CH = -1 QI += 1
      if VIES <= 0
        FIN = 2 son(100, 400)
      elif QI >= ${n}
        FIN = 1 PTS += 10 score(PTS) son(1318, 300)
      end
    end
    return
  end
  if appui(G) k = 0 end
  if appui(P3) k = 1 end
  if appui(D) k = 2 end
  if k >= 0
    CH = k AT = 1.2
    if k == BON[QI]
      ETAT = 1 PTS += 1 score(PTS) son(988, 120)
    else
      ETAT = 2 VIES -= 1 son(140, 200)
    end
  end
end

def dessine()
  var i = 0 var c = 0 var y = 184 var t = temps()
  fond(BAND[0])
  while i < 10 rect(0, i * 24, 320, 24, BAND[i]) i += 1 end
  decor()
  rect(0, 208, 320, 32, SOL) rect(0, 208, 320, 4, SOL2)
  if ETAT == 1 && int(t * 10) % 2 == 0 y = 172 end
  rect(20, 207, 24, 3, SOL2)
  perso(20, y)
  if QI < ${n}
    rect(12, 36, 296, 50, NO) cadre(12, 36, 296, 50, BL)
    texte(20, 41, QT[QI * 2], BL, 2) texte(20, 63, QT[QI * 2 + 1], BL, 2)
    i = 0
    while i < 3
      c = NO
      if ETAT > 0
        if i == BON[QI] c = rgb(40, 140, 70) elif i == CH c = rgb(190, 50, 60) end
      end
      rect(12, 94 + i * 26, 296, 22, c) cadre(12, 94 + i * 26, 296, 22, BL)
      texte(20, 98 + i * 26, LB[i] + RP[QI * 3 + i], BL, 2)
      i += 1
    end
  end
  texte(9, 9, str(PTS), NO, 3) texte(8, 8, str(PTS), BL, 3)
  coeurs()
  fin()
end
`;
  }

  function soigne(r) {   // un petit personnage à garder en forme 45 s : faim (G), joie (gros bouton), sommeil (D) ; une jauge à zéro = perdu
    const b = base(r);
    return `${nomJeu('Soin ', r)}${b.tete}
var N = [80.0, 80.0, 80.0] var AN = 0.0 var AK = 0
var LAB = ["Faim", "Joie", "Dodo"]
${b.defs}
def nouvelle()
  N[0] = 80.0 N[1] = 80.0 N[2] = 80.0 AN = 0.0 AK = 0 PTS = 0 FIN = 0 T = 0.0
  score(0)
end

def debut()
  couleurs()
  nouvelle()
end

def soin(k)
  N[k] += 35.0 AN = 0.5 AK = k PTS += 1 score(PTS) son(660 + k * 220, 60)
end

def maj(dt)
  var i = 0
  if FIN > 0
    if appui(P3) nouvelle() end
    return
  end
  T += dt
  if AN > 0 AN -= dt end
  N[0] -= 5.0 * dt N[1] -= 4.0 * dt N[2] -= 3.0 * dt
  if appui(G) soin(0) end
  if appui(P3) soin(1) end
  if appui(D) soin(2) end
  while i < 3
    if N[i] > 100 N[i] = 100.0 end
    if N[i] <= 0 && FIN == 0 FIN = 2 son(100, 400) end
    i += 1
  end
  if FIN == 0 && T >= 45
    FIN = 1 PTS += 10 score(PTS) son(1318, 300)
  end
end

def dessine()
  var i = 0 var c = 0 var y = 184 var t = temps() var bas = 0
  fond(BAND[0])
  while i < 10 rect(0, i * 24, 320, 24, BAND[i]) i += 1 end
  decor()
  rect(0, 208, 320, 32, SOL) rect(0, 208, 320, 4, SOL2)
  y = 184 + int(t * 3) % 2 * 2
  if AN > 0 y = 184 - int(AN * 40) end
  rect(144, 207, 32, 3, SOL2)
  perso(148, y)
  if AN > 0
    if AK == 0 objet(160, y - 14, 0) elif AK == 1 disque(160, y - 14, 7, A1) disque(158, y - 16, 2, BL) else texte(148, y - 22, "Zzz", BL, 2) end
  end
  i = 0
  while i < 3
    c = rgb(90, 200, 100)
    if N[i] < 30 c = rgb(235, 60, 80) bas = 1 end
    rect(54, 10 + i * 14, 104, 10, NO) rect(56, 12 + i * 14, int(N[i]), 6, c)
    texte(8, 11 + i * 14, LAB[i], BL, 1)
    i += 1
  end
  if bas == 1 && int(t * 4) % 2 == 0 texte(156, y - 26, "!", rgb(235, 60, 80), 3) end
  texte(251, 11, str(45 - int(T)), NO, 3) texte(250, 10, str(45 - int(T)), BL, 3)
  texte(10, 218, "G manger", BL, 2) texte(112, 218, "P3 jouer", BL, 2) texte(214, 218, "D dormir", BL, 2)
  fin()
end
`;
  }

  const SYSTEMES = { quiz, soigne, attrape: chute, esquive: chute, pluie: chute, 'plat-but': plateforme, 'plat-attrape': plateforme, 'plat-evite': plateforme, 'plat-course': plateforme, cote: plateforme, derriere: course, dessus: course, iso: course };   // jeu -> son système

  function recetteComplete(r) {   // une étape sans choix (ou un choix inconnu) prend sa valeur par défaut : le jeu marche toujours
    const o = {};
    r = r || {}; const imgs = r.imgs || {}; o.imgs = imgs; if (r.edit) o.edit = r.edit;   // edit : niveau 1 de la plateforme placé à la main (voir plateforme)
    for (const e of ETAPES) o[e.id] = (e.id === 'niv' ? nbNiv(r.niv) <= NIV_MAX && r.niv : e.bulles.some(b => b.id === r[e.id])) || (e.ia && r[e.id] === 'img' && imgs[e.id] > 0) ? r[e.id] : DEFAUT[e.id];
    if (!JEUX_DE[r.genre]) o.genre = GENRE_DE[o.jeu];                                    // pas de genre : celui du jeu (anciennes recettes)
    else if (GENRE_DE[o.jeu] !== o.genre) o.jeu = SOLO[o.genre] ? o.genre : JEUX_DE[o.genre][0];   // un jeu d'un autre genre : le premier du genre choisi
    return o;
  }
  // Multijoueur sans menu : le jeu envoie sa position (« identifiant,x,y ») et dessine un fantôme quand une autre console envoie le MÊME identifiant (= empreinte du programme : même recette = même jeu).
  // Lissage : le fantôme rattrape la dernière position reçue (14 %/image-seconde), il saute directement si l'écart dépasse 80 px (réapparition, début de partie). Course : pas de fantôme, la barre du rival suit l'autre joueur. Questions et personnage : jeux solo, inchangés. ponytail: pas de rôle ni d'attente de départ, chacun joue sa partie ; à ajouter si on veut une vraie course synchronisée.
  const COURSE = { cote: 1, 'plat-course': 1, derriere: 1, dessus: 1, iso: 1 };   // les vraies courses : départ synchronisé (attente de l'autre, puis 3-2-1)
  const MULTI = new Map([[chute, { x: 'int(PX)', y: 'int(PY)', d: 'perso(int(JDX), int(JDY)) disque(int(JDX) + 12, int(JDY) - 6, 3, BL)' }], [plateforme, { x: 'int(PX)', y: 'int(PY)', d: 'camera(CAMX, 0) perso(int(JDX), int(JDY)) disque(int(JDX) + 12, int(JDY) - 6, 3, BL) camera(0, 0)' }], [course, { x: 'int(P)', y: '0', r: 'RP = JDX' }]]);
  function multi(code, m, race, rival) {
    let h = 0x811c9dc5; for (const ch of code) h = Math.imul(h ^ ch.charCodeAt(0), 0x01000193) >>> 0;
    const AB = '23456789ABCDEFGHJKLMNPQRSTUVWXYZ'; let id = ''; for (let i = 0; i < 6; i++) { id += AB[h & 31]; h = (h >>> 5) | (h << 27) >>> 0; }
    const rv = race && m.d ? 'RX = JDX' : m.r;   // course de plateforme : le rival simulé laisse la place à l'autre joueur (barre et arrivée comptent pour lui)
    let c = code.replace(/^def (maj|dessine)\(/gm, (_, f) => 'def ' + f + '0(');
    if (race && m.d) c = c.replace('if RX > CAMX - 30 &&', 'if temps() - JT > 1.0 && RX > CAMX - 30 &&');
    const decl = `import string
var JID = "${id}" var JT = -9.0 var JOX = 0 var JOY = 0 var JOW = 0 var JDX = 0.0 var JDY = 0.0 var JLS = 0.0 var JA = 0.0 var JW = ${race ? 1 : 0} var JC = -1.0
`;
    return c.replace('\n', '\n' + decl) + `def maj(dt)
  var t = temps() var p = nil
  if t - JLS > 0.04 JLS = t envoie(JID + "," + str(${m.x}) + "," + str(${m.y}) + "," + str(JW)) end
  for e: recois()
    if e[0] != moi()
      p = string.split(e[1], ",")
      if size(p) == 4 && p[0] == JID
        if t - JT > 1.0 JDX = int(p[1]) * 1.0 JDY = int(p[2]) * 1.0 JA = 2.0 son(880, 90) end
        JT = t JOX = int(p[1]) JOY = int(p[2]) JOW = int(p[3])
      end
    end
  end
  if JA > 0 JA -= dt end
  if t - JT < 1.0
    var ex = JOX - JDX var ey = JOY - JDY var k = dt * 14.0
    if k > 1.0 k = 1.0 end
    if ex > 80 || ex < -80 || ey > 80 || ey < -80 JDX = JOX * 1.0 JDY = JOY * 1.0 else JDX += ex * k JDY += ey * k end
  end
  if JW == 1
    if JC < 0
      if t - JT < 1.0 && JOW == 1 JC = 3.0 son(660, 80) elif appui(P3) JW = 0 end
    else
      JC -= dt
      if JC <= 0 JW = 0 son(1047, 220) end
    end
    return
  end
  ${race ? 'var f0 = FIN maj0(dt)\n  if f0 > 0 && FIN == 0 JW = 1 JC = -1.0 end' : 'maj0(dt)'}
  ${rv ? 'if t - JT < 1.0 ' + rv + ' end' : ''}
end
def dessine()
  dessine0()
  ${m.d ? 'if temps() - JT < 1.0 ' + m.d + ' end' : ''}
  if JW == 1
    if JC < 0
      texte(29, 101, "Attends l'autre joueur", NO, 2) texte(28, 100, "Attends l'autre joueur", BL, 2)
      texte(101, 131, "P3 : jouer seul", NO, 1) texte(100, 130, "P3 : jouer seul", BL, 1)
    else texte(141, 91, str(int(JC) + 1), NO, 6) texte(140, 90, str(int(JC) + 1), BL, 6) end
  elif JA > 0 texte(47, 31, "Joueur 2 connecte !", NO, 2) texte(46, 30, "Joueur 2 connecte !", BL, 2) end
end
`;
  }
  // le cartel de fin : le mot et sa couleur (le texte « PERDU ! » est dans chaque système, on le remplace centré)
  const cartel = (code, c) => { const [t, col] = CARTELS[c.fin]; return c.fin === 'perdu' ? code : code.replace(/texte\(92, 90, "PERDU !", BL, 3\)/g, `texte(${160 - 9 * t.length}, 90, "${t}", ${rgb(col)}, 3)`); };
  // les niveaux : un compteur NV, et tout le jeu va plus vite (dt x 1,3 puis 1,6...). Chute : un niveau tous les 10 points. Autres : gagner ouvre le niveau suivant (P3), perdre revient au niveau 1.
  function niveaux(code, nn, chuteJeu, plat) {   // plat : jeu de plateforme, qui ne va pas plus vite (les niveaux changent de disposition) et recharge son niveau 1 après une fin
    const c = code.replace(/^def (maj|dessine)\(/gm, (_, f) => 'def ' + f + 'J(').replace('\n', '\nvar NV = 1 var NB = 0.0\n');
    return c + `
def maj(dt)
${chuteJeu ? `  var n = 1 + PTS / 10
  if n > ${nn} n = ${nn} end
  if n > NV NB = 1.5 son(1047, 150) end
  NV = n
  if NB > 0 NB -= dt end
  majJ(dt * (1.0 + 0.3 * (NV - 1)))` : `  var f = FIN
  if NB > 0 NB -= dt end
  if FIN == 1 && NV < ${nn}
    if appui(P3) NV += 1 NB = 1.5 nouvelle() end
    return
  end
  majJ(${plat ? 'dt' : 'dt * (1.0 + 0.3 * (NV - 1))'})
  if f > 0 && FIN == 0 NV = 1${plat ? ' nouvelle()' : ''} end`}
end
def dessine()
  dessineJ()
  texte(9, 35, "Niv " + str(NV) + "/${nn}", NO, 1) texte(8, 34, "Niv " + str(NV) + "/${nn}", BL, 1)
${chuteJeu ? '' : `  if FIN == 1 && NV < ${nn}
    rect(60, 80, 200, 84, NO) cadre(60, 80, 200, 84, BL)
    texte(70, 90, "NIVEAU " + str(NV) + " !", BL, 3)
    texte(100, 122, "Score " + str(PTS), BL, 2)
    texte(72, 144, "P3 : niveau suivant", A4, 1)
  end
`}  if NB > 0 texte(89, 100, "NIVEAU " + str(NV), NO, 3) texte(88, 99, "NIVEAU " + str(NV), BL, 3) end
end
`;
  }
  // le personnage invite l'enfant a modifier son jeu : une bulle 2,5 s toutes les 4 s (cx : decalage de camera, pour rester dans l'ecran)
  const PAROLE = `def parle(x, y, cx)
  var t = temps() var k = int(t / 4) var u = t - k * 4 var s = ["Change mon decor !", "Et si j'etais bleu ?", "Invente ton jeu !", "Ton jeu est nul !"][k % 4] var w = size(s) * 6 + 8 var c = BL var d = NO var b = x + 12 - w / 2
  if u > 2.5 return end
  if k % 4 == 3
    x = int(cx + 30 + u * 60) y = 70 + int(t * 6) % 2 * 3 b = x - 20 c = NO d = BL
    rect(x, y, 14, 14, rgb(255, int(t * 200) % 255, 255 - int(t * 90) % 255)) rect(x + 3, y + 4, 3, 3, BL) rect(x + 9, y + 4, 3, 3, BL)
  end
  if b < cx + 2 b = cx + 2 elif b + w > cx + 318 b = cx + 318 - w end
  rect(b, y - 24, w, 14, c) tri(x + 8, y - 10, x + 16, y - 10, x + 12, y - 5, c) texte(b + 4, y - 21, s, d, 1)
end
`;
  function assembler(r) {
    const c = recetteComplete(r), f = SYSTEMES[c.jeu], nn = SOLO[c.jeu] ? 1 : nbNiv(c.niv);
    let code = cartel(f(c), c); if (nn > 1) code = niveaux(code, nn, f === chute, f === plateforme && /^plat-/.test(c.jeu));
    if (nn > 4) code = code.replace(/ parle\(.*?, (0|CAMX)\)/g, '');   // 5 et 6 niveaux : la memoire de la console est deja pleine, pas de bulle
    if (/ parle\(/.test(code)) code = code.replace(/\ndef perso\(/, '\n' + PAROLE + '\ndef perso(');   // pas de bulle dans les questions ni le personnage a soigner : leur ecran est deja plein
    return MULTI.has(f) ? multi(code, MULTI.get(f), !!COURSE[c.jeu]) : code;
  }
  const AIDE = { saut: 'il saute', tir: 'il tire', bouclier: 'il se protege avec un bouclier', turbo: 'il va plus vite', aimant: 'il attire les bons objets' };
const AIDE_JEU = { 'plat-but': 'G et D pour courir, le gros bouton : il saute. Arrive au drapeau !', 'plat-attrape': 'G et D pour courir, le gros bouton : il saute. Attrape toutes les etoiles !', 'plat-evite': 'G et D pour courir, le gros bouton : il saute. Evite les monstres et arrive au drapeau !', 'plat-course': 'G et D pour courir, le gros bouton : il saute. Arrive au drapeau avant le rival !',
  cote: 'Il court tout seul : D va plus vite, G ralentit, le gros bouton : turbo. Arrive avant le rival !', 'cote-obst': 'Il court tout seul : D va plus vite, G ralentit, le gros bouton : il saute. Saute les obstacles et arrive avant le rival !', derriere: 'G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !', dessus: 'G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !', iso: 'G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !' };
const AIDE_OBST = { derriere: ' Evite les mauvais objets.', dessus: ' Evite les mauvais objets.', iso: ' Evite les mauvais objets.' };
AIDE_JEU.quiz = 'Lis la question, puis choisis ta reponse : G (la 1re), le gros bouton (la 2e) ou D (la 3e). 3 vies !'; AIDE_JEU.soigne = 'G : il mange, le gros bouton : il joue, D : il dort. Garde ses 3 jauges pleines pendant 45 secondes !';
M.aide = r => { const c = recetteComplete(r); if (SOLO[c.jeu]) return AIDE_JEU[c.jeu]; if (NOUVEAUX[c.jeu]) return AIDE_JEU[c.jeu === 'cote' && c.obst === 'avec' ? 'cote-obst' : c.jeu] + (c.obst === 'avec' ? AIDE_OBST[c.jeu] || '' : ''); const a = c.action; return 'G et D pour bouger, le gros bouton : ' + AIDE[a] + '.'; };
M.EN = {"Placer à la main":"Place things by hand","🧊 Je roule en 3D en diagonale":"🧊 I ride in diagonal 3D","Mon véhicule":"My vehicle","Sur quoi roules-tu ?":"What are you riding?","🚗 En voiture":"🚗 In a car","🏍 En moto de motocross":"🏍 On a motocross bike","🏄 Sur une planche de surf":"🏄 On a surfboard","Modifier ma présentation":"Change my game’s look","Que veux-tu changer ?":"What do you want to change?","Quand je perds":"When I lose","Que dit le cartel quand tu perds ?":"What does the sign say when you lose?","😵 PERDU !":"😵 YOU LOST!","😬 OUPS !":"😬 OOPS!","💀 GAME OVER":"💀 GAME OVER","😸 ENCORE !":"😸 AGAIN!","🔥 RATÉ !":"🔥 MISSED!","🤖 ERREUR":"🤖 ERROR","Les niveaux":"The levels","Combien de niveaux ?":"How many levels?","1️⃣ Un seul niveau":"1️⃣ A single level","🪜 3 niveaux":"🪜 3 levels","🏔 5 niveaux":"🏔 5 levels","❓ Je réponds à des questions":"❓ I answer questions","🐣 Je m’occupe d’un personnage":"🐣 I take care of a character","Lis la question, puis choisis ta reponse : G (la 1re), le gros bouton (la 2e) ou D (la 3e). 3 vies !":"Read the question, then pick your answer: L (1st), big button (2nd) or R (3rd). 3 lives!","G : il mange, le gros bouton : il joue, D : il dort. Garde ses 3 jauges pleines pendant 45 secondes !":"L: it eats, big button: it plays, R: it sleeps. Keep its 3 gauges full for 45 seconds!","🏁 Plateforme : arriver au drapeau":"🏁 Platformer: reach the flag","⭐ Plateforme : tout attraper":"⭐ Platformer: catch everything","👾 Plateforme : éviter les monstres":"👾 Platformer: dodge the monsters","⏱ Plateforme : course":"⏱ Platformer: race","🏃 Course de côté":"🏃 Side-view race","🚗 Course vue de derrière":"🚗 Race seen from behind","🛰 Course vue de dessus":"🛰 Race seen from above","Les obstacles":"The obstacles","Des obstacles sur la route ?":"Any obstacles on the road?","🚧 Avec obstacles":"🚧 With obstacles","🛣 Sans obstacles":"🛣 No obstacles","G et D pour courir, le gros bouton : il saute. Arrive au drapeau !":"L and R to run, big button: it jumps. Reach the flag!","G et D pour courir, le gros bouton : il saute. Attrape toutes les etoiles !":"L and R to run, big button: it jumps. Catch all the stars!","G et D pour courir, le gros bouton : il saute. Evite les monstres et arrive au drapeau !":"L and R to run, big button: it jumps. Dodge the monsters and reach the flag!","G et D pour courir, le gros bouton : il saute. Arrive au drapeau avant le rival !":"L and R to run, big button: it jumps. Reach the flag before the rival!","Il court tout seul : D va plus vite, G ralentit, le gros bouton : il saute. Saute les obstacles et arrive avant le rival !":"It runs by itself: R goes faster, L slows down, big button: it jumps. Jump over the obstacles and get there before the rival!","Il court tout seul : D va plus vite, G ralentit, le gros bouton : turbo. Arrive avant le rival !":"It runs by itself: R goes faster, L slows down, big button: turbo. Get there before the rival!","G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !":"L and R to change lane, big button: turbo. Pick up the good things and get there before the rival!","G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival ! Evite les mauvais objets.":"L and R to change lane, big button: turbo. Pick up the good things and get there before the rival! Avoid the bad things.","Le jeu":"The game","Quel jeu veux-tu ?":"Which game?","🍎 Attraper":"🍎 Catch","💨 Esquiver":"💨 Dodge","🌧 Ne rien rater":"🌧 Miss nothing","Mon personnage":"My character","Qui es-tu ?":"Who are you?","🐱 Un chat":"🐱 A cat","🤖 Un robot":"🤖 A robot","🐉 Un dragon":"🐉 A dragon","🚀 Une fusée":"🚀 A rocket","👻 Un fantôme":"👻 A ghost","🧙 Un magicien":"🧙 A wizard","🐸 Une grenouille":"🐸 A frog","🦉 Un hibou":"🦉 An owl","Sa couleur":"Its colour","De quelle couleur ?":"Which colour?","🔴 Rouge":"🔴 Red","🟠 Orange":"🟠 Orange","🟡 Jaune":"🟡 Yellow","🟢 Vert":"🟢 Green","🔵 Bleu":"🔵 Blue","🟣 Violet":"🟣 Purple","🩷 Rose":"🩷 Pink","⚪ Blanc":"⚪ White","Le monde":"The world","Où es-tu ?":"Where are you?","🌳 Une forêt":"🌳 A forest","🏙 La ville la nuit":"🏙 The city at night","🌌 L’espace":"🌌 Space","🌊 Sous l’eau":"🌊 Underwater","🏜 Un désert":"🏜 A desert","❄️ La neige":"❄️ Snow","Ce que j’attrape":"What I catch","Que veux-tu attraper ?":"What do you want to catch?","🎁 Ceux du monde":"🎁 The ones from this world","Ce que j’évite":"What I avoid","Que veux-tu éviter ?":"What do you want to avoid?","💣 Ceux du monde":"💣 The ones from this world","Le gros bouton":"The big button","Que fait le gros bouton ?":"What does the big button do?","🦘 Sauter":"🦘 Jump","💥 Tirer":"💥 Shoot","🛡 Bouclier":"🛡 Shield","💨 Turbo":"💨 Turbo","🧲 Aimant":"🧲 Magnet","G et D pour bouger, le gros bouton : il saute.":"L and R to move, big button: it jumps.","G et D pour bouger, le gros bouton : il tire.":"L and R to move, big button: it shoots.","G et D pour bouger, le gros bouton : il se protege avec un bouclier.":"L and R to move, big button: it protects itself with a shield.","G et D pour bouger, le gros bouton : il va plus vite.":"L and R to move, big button: it goes faster.","G et D pour bouger, le gros bouton : il attire les bons objets.":"L and R to move, big button: it attracts the good things."};   // la page anglaise (lang="en") traduit les bulles avec ce dictionnaire
Object.assign(M.EN, { "Un niveau de plus ?": "One more level?" });
for (let n = 1; n <= NIV_MAX; n++) { M.EN['➕ Ajouter un niveau (ça fera ' + (n + 1) + ')'] = '➕ Add a level (that makes ' + (n + 1) + ')'; M.EN['✋ Rester à ' + n + (n > 1 ? ' niveaux' : ' niveau')] = '✋ Stay at ' + n + (n > 1 ? ' levels' : ' level'); }
Object.assign(M.EN, { "Les plateformes": "The platforms", "Difficulté : les plateformes des niveaux ?": "Difficulty: the platforms in each level?", "🧱 Les mêmes à chaque niveau": "🧱 The same in every level", "🗺 À des endroits différents à chaque niveau": "🗺 In different places in every level", "Les pièges": "The traps", "Difficulté : et les pièges ?": "Difficulty: and the traps?", "⚖️ Autant de pièges à chaque niveau": "⚖️ The same number of traps", "👾 De plus en plus de pièges": "👾 More and more traps" });
Object.assign(M.EN, { "Le genre": "The genre", "Quel jeu ?": "Which game?", "🏃 Je saute de plateforme en plateforme": "🏃 I jump from platform to platform", "💨 Je cours tout seul et j’évite les obstacles": "💨 I run on my own and dodge obstacles", "🛸 Je reste en bas, tout vient vers moi": "🛸 I stay at the bottom, everything comes at me", "🧺 J’attrape ce qui tombe": "🧺 I catch what falls" });
// le niveau 1 de la plateforme tel que le jeu le fait (point de départ de « Placer à la main ») ; null si ce jeu n'est pas une plateforme
M.platDefaut = r => {
  const c = recetteComplete(r), P = PLATEFORMES[c.jeu]; if (!P) return null;
  const pl = (c.plan === 'oui' ? PLANS[1] : PLATS).map(p => p.slice()), hz = [...P.hz, ...PIEGES_PLUS].slice(0, P.hz.length).map(x => c.plan === 'oui' ? x + DEC[0] : x);
  return { pl, st: [...pl.map(p => [p[0] + p[2] / 2, p[1] - 16]), [70, 192]], hz, long: 1000, drapeau: 940, bouge: !!P.vh };
};
M.ETAPES = ETAPES; M.genreDe = jeu => ETAPES[0].bulles.find(b => b.id === GENRE_DE[jeu]); M.saute = saute; M.nivBulles = nivBulles; M.nbNiv = nbNiv; M.DEFAUT = DEFAUT; M.recetteComplete = recetteComplete; M.assembler = assembler;
})(typeof module === 'object' ? module.exports : (window.JeuxBulles = {}));
