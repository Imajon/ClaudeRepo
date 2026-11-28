// Jeux « à bulles » (tranche 6-9 ans) : l'enfant touche des bulles, la page assemble un programme Berry à partir de PIÈCES déjà testées (jeu, personnage, couleur, monde, action).
// Pas d'IA pour le code : chaque pièce est du Berry écrit et joué dans verif (node test-bulles.js), donc toute combinaison marche du premier coup.
// Même fichier pour la page (window.JeuxBulles) et le serveur / les tests (require). Ajouter un SYSTÈME de jeu = une entrée dans SYSTEMES + ses bulles dans JEUX.
(function (M) {
  const AR = typeof require === 'function' ? require('./arbres-foret.js') : window.ArbresForet;   // 3 arbres en pixel art (images du jeu), décor du monde Forêt
  const BE = typeof require === 'function' ? require('./bestiaire.js') : window.Bestiaire;   // les formes du bestiaire (bon, mal, personnages en plus)
  const rgb = c => `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
  const mel = (a, b, k) => a.map((v, i) => Math.round(v + (b[i] - v) * k));
  const fonce = c => c.map(v => Math.round(v * 0.6)), clair = c => c.map(v => Math.min(255, Math.round(v * 1.2 + 45)));
  const liste0 = (n, f) => Array.from({ length: n }, (_, i) => f(i));
  const liste = (n, f) => '[' + liste0(n, f).join(', ') + ']';

  // une bulle par forme du bestiaire d'un rôle (bon = à attraper, mal = à éviter) : le choix vaut l'id de la forme
  const EMOJI = { pomme: '🍎', piece: '🪙', etoile: '⭐', perle: '🫧', poisson: '🐟', gemme: '💎', flocon: '❄️', champignon: '🍄', voiture: '🚗', meteorite: '☄️', meduse: '🪼', cactus: '🌵', glacon: '🧊' };
  const formesBulles = role => BE.FORMES.filter(f => f.role === role).map(f => ({ id: f.id, bouton: (EMOJI[f.id] || '🔷') + ' ' + f.nom }));
  const formeObjet = id => BE.lignes(BE.forme(id), { dx: -8, dy: -8 });   // forme 16 x 16 centrée en (x, y), comme les objets des mondes
  const objetCode = (r, k, mo, im) => r[k] === 'img' ? `  image(${im[k]}, x - 8, y - 8, 0, 1)` : BE.forme(r[k]) ? formeObjet(r[k]) : mo[k];

  // ---------- les bulles (étape -> choix) ; id = ce que le générateur comprend, bouton = ce que l'enfant voit ----------
  const ETAPES = [
    // ARBORESCENCE : un genre (grande famille) -> un jeu de ce genre (les bulles « genre: » ne s'affichent que sous le genre choisi). Trois genres au départ ; les autres arrivent avec « Des idées pour toi » (plus: true).
    // STRUCTURE VISÉE (verbe -> jeux ; récap dans l'Établi, onglet Genres) : saute -> plateforme ; attrape -> perso en bas / plateforme ; tire -> invader / vaisseau central qui tourne / vaisseau sur fond qui défile ; casse -> casse-briques / plateforme qui casse ; rebondis -> plateforme qui rebondit / flappy / pong ; apprends -> quiz / plateforme à questions ; fais grandir -> tamagotchi / plateforme qui grandit.
    // Exemples Berry testés de chacun : systemes/jeu-*.be. Pas encore de moteur ici (donc pas de bulle) : tout sauf plateforme, chute, quiz, soigne, courses. Un nouveau moteur = une entrée dans SYSTEMES + sa bulle ici.
    // PARCOURS de l'enfant : le type (genre) -> le quoi (perso, ou les questions du quiz) -> le où (monde) -> « comment » sans fin (tout le reste, par 2, puis « aller plus loin ou tester ? »). `pre` : choix déjà faits par la bulle (« Je tire » = invader + gros bouton tir). casse / rebondis : à ajouter avec leur moteur.
    { id: 'genre', h: 210, icone: '🎮', nom: 'Le genre', q: 'Que fais-tu dans ton jeu ?', bulles: [
      { id: 'plateforme', bouton: '🦘 Je saute' }, { id: 'runner', bouton: '💨 J’avance' }, { id: 'invader', bouton: '🧺 J’attrape' }, { id: 'tire', bouton: '💥 Je tire' },
      { id: 'quiz', bouton: '🧠 J’apprends' }, { id: 'soigne', bouton: '🌱 Je fais grandir' }, { id: 'casse', bouton: '🧱 Je casse' }, { id: 'rebond', bouton: '🏓 Je rebondis' }, { id: 'reagis', bouton: '⚡ Je réagis' }, { id: 'empile', bouton: '🧩 J’empile' }, { id: 'laby', bouton: '👻 Je me faufile' }] },
    { id: 'jeu', libre: true, h: 235, icone: '🕹', nom: 'Le jeu', q: 'Quel jeu ?', bulles: [
      { id: 'plat-but', genre: 'plateforme', bouton: '🏁 Je cours et je saute jusqu’au drapeau' }, { id: 'plat-attrape', genre: 'plateforme', bouton: '⭐ Je saute pour ramasser toutes les étoiles' }, { id: 'plat-evite', genre: 'plateforme', bouton: '👾 Je saute par-dessus les monstres' }, { id: 'plat-course', genre: 'plateforme', bouton: '⏱ Je cours plus vite que mon rival' }, { id: 'toits', genre: 'plateforme', bouton: '🏙 Je saute par-dessus les immeubles et je fais des figures' }, { id: 'lianes', genre: 'plateforme', bouton: '🌿 Je me balance de liane en liane' },
      { id: 'cote', genre: 'runner', bouton: '🏃 Je cours en sautant les obstacles' }, { id: 'derriere', genre: 'runner', bouton: '🚗 Je conduis et je change de voie' }, { id: 'dessus', genre: 'runner', bouton: '🛰 Je pilote ma voiture sur la piste' }, { id: 'iso', genre: 'runner', bouton: '🧊 Je roule en 3D en diagonale' },
      { id: 'attrape', genre: 'invader', bouton: '🧺 J’attrape ce qui tombe' }, { id: 'esquive', genre: 'invader', bouton: '💨 J’évite ce qui tombe' }, { id: 'pluie', genre: 'invader', bouton: '🌧 J’attrape tout, rien ne doit tomber' },
      { id: 'tir-haut', genre: 'tire', bouton: '⬆️ Je tire vers le haut' }, { id: 'tir-perso', genre: 'tire', bouton: '➡️ Je tire vers la droite' }, { id: 'tir-autour', genre: 'tire', bouton: '🎯 Je tire autour de moi' }, { id: 'tir-vagues', genre: 'tire', bouton: '👾 Je défends contre des vagues' },
      { id: 'casse-briques', genre: 'casse', bouton: '🧱 Je casse un mur de briques avec une balle' }, { id: 'plateforme-casse', genre: 'casse', bouton: '📦 Je saute et je casse des caisses' },
      { id: 'pong', genre: 'rebond', bouton: '🏓 Je joue au pong contre la console' }, { id: 'flappy', genre: 'rebond', bouton: '🐦 Je bats des ailes entre les tuyaux' }, { id: 'plateforme-rebond', genre: 'rebond', bouton: '🦘 Je rebondis de plateforme en plateforme' },
      { id: 'rythme', genre: 'reagis', bouton: '🎸 J’appuie en rythme quand les notes tombent' }, { id: 'alignement', genre: 'reagis', bouton: '🏗 J’arrête la plateforme pile sur l’autre' },
      { id: 'tetris', genre: 'empile', bouton: '🧩 J’empile des blocs pour faire des lignes' }, { id: 'labyrinthe', genre: 'laby', bouton: '👻 Je mange tout dans un labyrinthe et j’évite les fantômes' },
      { id: 'quiz', genre: 'quiz', bouton: '❓ Je réponds à des questions' }, { id: 'soigne', genre: 'soigne', bouton: '🐣 Je m’occupe d’un personnage' }] },
    // « toits » seulement : la figure faite en l'air en rappuyant sur le gros bouton (3 points si on retombe après)
    { id: 'figtoit', h: 175, icone: '🤸', nom: 'Ma figure', q: 'Quelle figure fais-tu en l’air ?', bulles: [{ id: 'salto', bouton: '🤸 Un salto' }, { id: 'vrille', bouton: '🌀 Une vrille' }, { id: 'etoile', bouton: '⭐ Une étoile' }] },
    { id: 'figure', h: 20, icone: '🏍', nom: 'Mon véhicule', q: 'Sur quoi roules-tu ?', bulles: [{ id: 'auto', bouton: '🚗 En voiture' }, { id: 'moto', bouton: '🏍 En moto de motocross' }, { id: 'surf', bouton: '🏄 Sur une planche de surf' }] },
    { id: 'obst', h: 55, icone: '🚧', nom: 'Les obstacles', q: 'Des obstacles sur la route ?', bulles: [{ id: 'avec', bouton: '🚧 Avec obstacles' }, { id: 'sans', bouton: '🛣 Sans obstacles' }] },
    { id: 'perso', h: 28, icone: '🐱', nom: 'Mon personnage', q: 'Qui joues-tu ?', ia: true, bulles: [
      { id: 'chat', bouton: '🐱 Je joue un chat' }, { id: 'robot', bouton: '🤖 Je joue un robot' }, { id: 'dragon', bouton: '🐉 Je joue un dragon' }, { id: 'fusee', bouton: '🚀 Je joue une fusée' },
      { id: 'fantome', bouton: '👻 Je joue un fantôme' }, { id: 'magicien', bouton: '🧙 Je joue un magicien' }, { id: 'grenouille', bouton: '🐸 Je joue une grenouille' }, { id: 'hibou', bouton: '🦉 Je joue un hibou' }, { id: 'oiseau', bouton: '🐦 Je joue un oiseau' }, { id: 'blob', bouton: '🟡 Je joue une bestiole' }] },
    { id: 'couleur', h: 330, icone: '🎨', nom: 'Sa couleur', q: 'De quelle couleur ?', bulles: [
      { id: 'rouge', bouton: '🔴 Rouge' }, { id: 'orange', bouton: '🟠 Orange' }, { id: 'jaune', bouton: '🟡 Jaune' }, { id: 'vert', bouton: '🟢 Vert' },
      { id: 'bleu', bouton: '🔵 Bleu' }, { id: 'violet', bouton: '🟣 Violet' }, { id: 'rose', bouton: '🩷 Rose' }, { id: 'blanc', bouton: '⚪ Blanc' }] },
    { id: 'monde', libre: true, h: 150, icone: '🌍', nom: 'Le monde', q: 'Où es-tu ?', bulles: [
      { id: 'foret', bouton: '🌳 Je joue une forêt' }, { id: 'ville', bouton: '🏙 La ville la nuit' }, { id: 'espace', bouton: '🌌 L’espace' },
      { id: 'eau', bouton: '🌊 Sous l’eau' }, { id: 'desert', bouton: '🏜 Je joue un désert' }, { id: 'neige', bouton: '❄️ La neige' }] },
    // quiz seulement : l'enfant dit le sujet des questions et le genre de réponses, l'IA écrit 5 questions (api/quiz) ; la bulle prise porte les questions dans .q
    { id: 'quest', quiz: true, h: 300, icone: '❓', nom: 'Les questions', q: 'Sur quoi sont tes questions, et quelles réponses veux-tu ?', bulles: [] },
    { id: 'bon', h: 100, icone: '🎁', nom: 'Ce que j’attrape', q: 'Que veux-tu attraper ?', ia: true, bulles: [{ id: 'monde', bouton: '🎁 Ceux du monde' }, ...formesBulles('bon')] },
    { id: 'mal', h: 0, icone: '💣', nom: 'Ce que j’évite', q: 'Que veux-tu éviter ?', ia: true, bulles: [{ id: 'monde', bouton: '💣 Ceux du monde' }, ...formesBulles('mal')] },
    // jeux de tir au vaisseau fixe seulement ; opt : proposées dans « Que veux-tu personnaliser ? », jamais posées d'office dans le parcours
    { id: 'compo', opt: true, h: 15, icone: '🌀', nom: 'Leur façon de bouger', q: 'Comment bougent tes ennemis ?', bulles: [
      { id: 'droit', bouton: '➡️ Tout droit' }, { id: 'zigzag', bouton: '〰️ En zigzag' }, { id: 'suit', bouton: '🎯 Ils me poursuivent' }, { id: 'accel', bouton: '⚡ Ils accélèrent en approchant' }] },
    { id: 'typeEn', opt: true, h: 290, icone: '👹', nom: 'Leurs types', q: 'Quels ennemis veux-tu ?', bulles: [
      { id: 'pareils', bouton: '👾 Tous pareils' }, { id: 'costauds', bouton: '💪 Des costauds : 3 tirs pour les détruire' }, { id: 'rapides', bouton: '💨 Des rapides' }, { id: 'melange', bouton: '🎲 Un peu de tout' }] },
    { id: 'action', h: 265, icone: '⚡', nom: 'Le gros bouton', q: 'Que fait le gros bouton ?', bulles: [
      { id: 'saut', bouton: '🦘 Sauter' }, { id: 'tir', bouton: '💥 Tirer' }, { id: 'bouclier', bouton: '🛡 Bouclier' }, { id: 'turbo', bouton: '💨 Turbo' }, { id: 'aimant', bouton: '🧲 Aimant' }] },
    // étapes de « seconde version » (v2) : proposées seulement une fois le premier jeu fabriqué, depuis « Modifier ma présentation »
    // « Message du jeu » : fin, gagne et saute sont regroupées en UNE bulle du menu de personnalisation (groupe: 'msg'), puis s'enchaînent
    { id: 'fin', v2: true, groupe: 'msg', h: 340, icone: '😵', nom: 'Quand je perds', q: 'Que dit le cartel quand tu perds ?', bulles: [
      { id: 'perdu', bouton: '😵 PERDU !' }, { id: 'oups', bouton: '😬 OUPS !' }, { id: 'over', bouton: '💀 GAME OVER' }, { id: 'encore', bouton: '😸 ENCORE !' }, { id: 'rate', bouton: '🔥 RATÉ !' }, { id: 'erreur', bouton: '🤖 ERREUR' }] },
    { id: 'gagne', v2: true, groupe: 'msg', h: 300, icone: '🏆', nom: 'Quand je gagne', q: 'Que dit le cartel quand tu gagnes ?', bulles: [
      { id: 'bravo', bouton: '👏 BRAVO !' }, { id: 'gagne', bouton: '🏆 GAGNÉ !' }, { id: 'youpi', bouton: '🥳 YOUPI !' }, { id: 'champion', bouton: '🥇 CHAMPION' }, { id: 'super', bouton: '😎 SUPER !' }, { id: 'wow', bouton: '🤩 WOUHOU !' }] },
    { id: 'saute', v2: true, groupe: 'msg', h: 320, icone: '🦘', nom: 'Quand je saute', q: 'Que dit ton personnage quand il saute ?', bulles: [
      { id: 'rien', bouton: '🤐 Rien' }, { id: 'hop', bouton: '🦘 HOP !' }, { id: 'boing', bouton: '🌀 BOING !' }, { id: 'youpi', bouton: '🥳 YOUPI !' }, { id: 'wouhou', bouton: '🤩 WOUHOU' }, { id: 'hiha', bouton: '🤠 HI HA !' }] },
    // les bulles « niv » sont fabriquées par nivBulles() à partir du nombre de niveaux actuel : ajouter un niveau, ou rester
    { id: 'niv', v2: true, h: 190, icone: '🪜', nom: 'Les niveaux', q: 'Un niveau de plus ?', bulles: [] },
    // plateforme à plusieurs niveaux : deux étapes indépendantes, donc combinables (la vitesse, elle, ne monte pas dans ces jeux)
    { id: 'plan', v2: true, h: 230, icone: '🗺', nom: 'Les plateformes', q: 'Difficulté : les plateformes des niveaux ?', bulles: [
      { id: 'non', bouton: '🧱 Les mêmes à chaque niveau' }, { id: 'oui', bouton: '🗺 À des endroits différents à chaque niveau' }] },
    { id: 'piege', v2: true, h: 270, icone: '👾', nom: 'Les pièges', q: 'Difficulté : et les pièges ?', bulles: [
      { id: 'non', bouton: '⚖️ Autant de pièges à chaque niveau' }, { id: 'oui', bouton: '👾 De plus en plus de pièges' }] }
  ];
  const DEFAUT = { genre: 'invader', jeu: 'attrape', obst: 'avec', perso: 'chat', couleur: 'orange', monde: 'foret', bon: 'monde', mal: 'monde', action: 'saut', figure: 'auto', figtoit: 'salto', fin: 'perdu', gagne: 'bravo', saute: 'rien', niv: 'un', plan: 'non', piege: 'non', compo: 'droit', typeEn: 'pareils' };
  const COMPOS = ['droit', 'zigzag', 'suit', 'accel'], TYPES_EN = ['pareils', 'costauds', 'rapides', 'melange'];   // leur rang = la valeur de COMPO / TYPE dans le Berry des tirs
  const CARTELS = { perdu: ['PERDU !'], oups: ['OUPS !', [255, 120, 100]], over: ['GAME OVER', [255, 70, 70]], encore: ['ENCORE !', [255, 225, 90]], rate: ['RATE !', [255, 150, 50]], erreur: ['ERREUR', [110, 230, 130]] };   // texte (sans accent : la console n'en dessine pas) et couleur ; 9 lettres au plus, le cartel fait 200 px
  const CARTELS_GAGNE = { bravo: ['BRAVO !'], gagne: ['GAGNE !', [255, 225, 90]], youpi: ['YOUPI !', [120, 230, 140]], champion: ['CHAMPION', [255, 180, 60]], super: ['SUPER !', [100, 200, 255]], wow: ['WOUHOU !', [240, 120, 175]] };   // le cartel de victoire (8 lettres au plus : le cartel fait 200 px)
  const MOTS_SAUTE = { hop: 'HOP !', boing: 'BOING !', youpi: 'YOUPI !', wouhou: 'WOUHOU', hiha: 'HI HA !' };   // le mot qui jaillit au-dessus du personnage quand il saute
  const JEUX = ETAPES.find(e => e.id === 'jeu').bulles, GENRE_DE = Object.fromEntries(JEUX.map(b => [b.id, b.genre])), JEUX_DE = {}; JEUX.forEach(b => (JEUX_DE[b.genre] = JEUX_DE[b.genre] || []).push(b.id));
  const NIVEAUX = { un: 1, trois: 3, cinq: 5 }, NIV_MAX = 6, MONDES_MAX = 2;   // 6 niveaux au plus (PIEGES_PLUS : 2 monstres de plus par niveau)
  // un monde par niveau (plateforme à plusieurs niveaux) : monde2 .. monde6, mêmes bulles que « monde » + « comme le niveau 1 » ; libre : le champ « Ce que tu aimes » choisit le monde le plus proche
  for (let k = 2; k <= NIV_MAX; k++) { ETAPES.push({ id: 'monde' + k, v2: true, libre: true, h: 130 + k * 22, icone: ['2️⃣', '3️⃣', '4️⃣', '5️⃣', '6️⃣'][k - 2], nom: 'Monde du niveau ' + k, q: 'Niveau ' + k + ' : où es-tu ?', bulles: [{ id: 'meme', bouton: '↩️ Comme le niveau 1' }, ...ETAPES.find(e => e.id === 'monde').bulles] }); DEFAUT['monde' + k] = 'meme'; }
  const nbNiv = id => NIVEAUX[id] || +String(id || '').slice(1) || 1;   // 'n4' = 4 niveaux ; les anciens 'un', 'trois', 'cinq' marchent encore
  const MOTS = {'genre/soigne': 'Grandir', 'jeu/plat-but': 'Drapeau', 'jeu/plat-attrape': 'Étoiles', 'jeu/plat-evite': 'Monstres', 'jeu/plat-course': 'Rival', 'jeu/toits': 'Immeubles', 'jeu/lianes': 'Lianes', 'figtoit/salto': 'Salto', 'figtoit/vrille': 'Vrille', 'figtoit/etoile': 'Étoile','jeu/cote': 'Obstacles', 'jeu/derriere': 'Voies', 'jeu/dessus': 'Piste', 'jeu/iso': '3D', 'jeu/attrape': 'Attraper', 'jeu/esquive': 'Éviter', 'jeu/pluie': 'Tout attraper', 'jeu/tir-haut': 'Tir en haut', 'jeu/tir-perso': 'Tir à droite', 'jeu/tir-autour': 'Tir autour', 'jeu/tir-vagues': 'Vagues', 'jeu/casse-briques': 'Briques', 'jeu/pong': 'Pong', 'jeu/flappy': 'Flappy', 'jeu/rythme': 'Rythme', 'jeu/alignement': 'Alignement', 'jeu/plateforme-casse': 'Caisses', 'jeu/plateforme-rebond': 'Rebonds', 'jeu/tetris': 'Tetris', 'jeu/labyrinthe': 'Labyrinthe', 'jeu/quiz': 'Questions', 'jeu/soigne': 'Soigner', 'figure/moto': 'Moto', 'figure/surf': 'Surf', 'perso/chat': 'Chat', 'perso/robot': 'Robot', 'perso/dragon': 'Dragon', 'perso/fusee': 'Fusée', 'perso/fantome': 'Fantôme', 'perso/magicien': 'Magicien', 'perso/grenouille': 'Grenouille', 'perso/hibou': 'Hibou', 'perso/oiseau': 'Oiseau', 'perso/blob': 'Bestiole', 'monde/foret': 'Forêt', 'monde/ville': 'Ville', 'monde/desert': 'Désert', 'compo/droit': 'Tout droit', 'compo/zigzag': 'Zigzag', 'compo/suit': 'Poursuite', 'compo/accel': 'Accélèrent', 'typeEn/pareils': 'Tous pareils', 'typeEn/costauds': 'Costauds', 'typeEn/rapides': 'Rapides', 'typeEn/melange': 'Mélange', 'plan/non': 'Mêmes niveaux', 'plan/oui': 'Niveaux changeants', 'piege/non': 'Pièges égaux', 'piege/oui': 'Pièges en plus'};   // le mot clef d'une réponse longue : le récapitulatif ne garde que lui (la réponse entière se relit en touchant la pastille)
  for (let k = 2; k <= NIV_MAX; k++) { MOTS['monde' + k + '/meme'] = 'Même monde'; for (const w of ['foret', 'ville', 'espace', 'eau', 'desert', 'neige']) MOTS['monde' + k + '/' + w] = { foret: 'Forêt', ville: 'Ville', espace: 'Espace', eau: 'Eau', desert: 'Désert', neige: 'Neige' }[w]; }
  const mot = (axe, b) => { const m = MOTS[axe + '/' + b.id]; return m ? b.bouton.replace(/^(\S+\s).*$/, '$1') + m : b.bouton; };
  const nivBulles = id => { const n = nbNiv(id), bs = [{ id: 'n' + n, bouton: '✋ Rester à ' + n + (n > 1 ? ' niveaux' : ' niveau') }]; if (n < NIV_MAX) bs.unshift({ id: 'n' + (n + 1), bouton: '➕ Ajouter un niveau (ça fera ' + (n + 1) + ')' }); return bs; };
// L'IA propose d'autres idées pour perso, bon et mal : chacune devient une image (icône de la page, numéro dans r.imgs) et son choix vaut 'img'. Sans image trouvée, retour au dessin du monde.
const COURSES = { cote: 1, derriere: 1, dessus: 1, iso: 1 }, NOUVEAUX = { 'plat-but': 1, 'plat-attrape': 1, 'plat-evite': 1, 'plat-course': 1, toits: 1, lianes: 1, ...COURSES };   // les nouveaux jeux : le gros bouton a son rôle fixe (sauter, turbo), les obstacles ne comptent que pour les courses
const FIXES = { 'tir-perso': 1, 'tir-autour': 1, 'tir-vagues': 1, 'casse-briques': 1, pong: 1, flappy: 1, rythme: 1, alignement: 1, 'plateforme-casse': 1, 'plateforme-rebond': 1, tetris: 1, labyrinthe: 1 }, CHOIX = { 'casse-briques': ['bon'], pong: ['bon'], flappy: ['perso'], rythme: ['bon'], tetris: [], alignement: [], 'plateforme-casse': ['perso', 'bon'], 'plateforme-rebond': ['perso'], labyrinthe: ['perso', 'mal'] }, TIRS = { 'tir-haut': 1, 'tir-perso': 1, 'tir-autour': 1 };   // jeux de tir au vaisseau fixe : seuls le genre, le jeu et le monde se choisissent (le monde donne le fond, les particules et les ennemis)
const SOLO = { quiz: 1, soigne: 1 };   // ces deux jeux n'ont ni obstacles, ni objets, ni gros bouton à choisir
// tir vers la droite et tir autour : en plus du genre, du jeu et du monde, leur personnage, leurs tirs (bon) et leurs ennemis (mal) se choisissent
const peutSauter = r => /^plat-|^toits$|^lianes$/.test(r.jeu) || (r.jeu === 'cote' && r.obst !== 'sans') || (['attrape', 'esquive', 'pluie'].includes(r.jeu || DEFAUT.jeu) && (r.action || DEFAUT.action) === 'saut');   // les jeux où le gros bouton fait sauter
const saute = (id, r) => { r = r || {}; if (SOLO[r.genre]) r = { ...r, jeu: r.genre }; if (id === 'compo' || id === 'typeEn') return !TIRS[r.jeu]; if (FIXES[r.jeu] && id !== 'genre' && id !== 'jeu' && id !== 'monde') return !(CHOIX[r.jeu] || ['perso', 'bon', 'mal']).includes(id); if (id === 'quest') return r.jeu !== 'quiz'; if (id === 'plan' || id === 'piege') return !(/^plat-/.test(r.jeu) && nbNiv(r.niv) > 1); if (/^monde[2-6]$/.test(id)) return !(/^plat-/.test(r.jeu) && nbNiv(r.niv) >= +id.slice(5)); if (id === 'jeu') return !!SOLO[r.genre]; if (SOLO[r.jeu] && ['obst', 'bon', 'mal', 'action', 'niv'].includes(id)) return true; if (id === 'gagne') return !NOUVEAUX[r.jeu]; if (id === 'saute') return !peutSauter(r); if (id === 'figtoit') return r.jeu !== 'toits'; if (id === 'niv' && r.jeu === 'lianes') return true; if (id === 'figure') return !COURSES[r.jeu]; return id === 'couleur' || (id === 'bon' && r.jeu !== 'plat-attrape' && r.jeu !== 'lianes') || id === 'mal' || (id === 'obst' && !COURSES[r.jeu]) || (id === 'action' && (!!NOUVEAUX[r.jeu] || r.jeu === 'tir-haut')); };   // une image garde ses couleurs : pas d'étape « couleur »   // une étape sans choix : le jeu marche quand même

  const FIG = {"auto":["rect(x - 3, y + 16, 30, 9, C2) rect(x - 3, y + 16, 30, 3, C3) disque(x + 3, y + 25, 4, NO) disque(x + 21, y + 25, 4, NO)","rect(x - 2, y + 16, 28, 10, C2) rect(x - 2, y + 16, 28, 3, C3) rect(x, y + 25, 6, 4, NO) rect(x + 18, y + 25, 6, 4, NO)"],"moto":["disque(x + 2, y + 24, 5, NO) disque(x + 22, y + 24, 5, NO) rect(x + 2, y + 17, 20, 4, C2) rect(x + 18, y + 12, 3, 6, NO)","rect(x + 9, y + 18, 6, 10, NO) rect(x + 4, y + 17, 16, 3, C2)"],"surf":["rect(x - 3, y + 21, 30, 3, C2) tri(x + 27, y + 21, x + 27, y + 24, x + 32, y + 22, C2) rect(x - 4, y + 25, 32, 2, A3)","rect(x + 5, y + 20, 14, 5, C2) rect(x + 3, y + 25, 18, 2, A3)"]};   // le véhicule sous le personnage dans les courses : [vue de côté, vue de derrière / dessus]
  const COULEURS = { rouge: [225, 60, 55], orange: [240, 140, 40], jaune: [245, 205, 50], vert: [80, 190, 90], bleu: [70, 130, 230], violet: [150, 90, 210], rose: [240, 120, 175], blanc: [240, 240, 245] };
  // jeu : part de mauvais objets (%), un bon objet qui touche le sol coûte une vie ?, un mauvais objet évité rapporte un point ?
  const MODES = { attrape: { pb: 40, manque: false, esquive: false, nom: '' }, esquive: { pb: 100, manque: false, esquive: true, nom: 'Esquive ' }, 'tir-haut': { pb: 100, manque: false, esquive: false, nom: 'Tir ' }, pluie: { pb: 20, manque: true, esquive: false, nom: 'Pluie de tresors ', lent: 0.4 } };
  const NOMS = { chat: 'Chat', robot: 'Robot', dragon: 'Dragon', fusee: 'Fusee', fantome: 'Fantome', magicien: 'Magicien', grenouille: 'Grenouille', hibou: 'Hibou', oiseau: 'Oiseau', blob: 'Bestiole' };

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

  // personnages venus du bestiaire (forme 16 x 16 agrandie 1,5 fois dans la boîte de 24 x 24, aux couleurs choisies)
  for (const id of ['oiseau', 'blob']) PERSOS[id] = BE.lignes(BE.forme(id), { z: 1.5, ex: { a: 'C1', b: 'C3', c: 'C2', w: 'BL', k: 'NO' } });

  // ---------- décors en COUCHES : des éléments graphiques séparés (ELEMENTS), assemblés par `couches` d'après la fiche d'un monde ----------
  // Un élément = du Berry qui dessine à (x, y) avec la taille h, couleur c (et c2) ; sol à y = 208. Ajouter un élément = une entrée ici, puis l'employer dans les couches d'un monde.
  const ELEMENTS = {
    soleil: c => `disque(x, y, h, ${c})`,
    nuage: c => `disque(x, y, h, ${c}) disque(x + h, y + h / 2, h * 3 / 4, ${c}) disque(x - h, y + h / 2, h * 2 / 3, ${c})`,
    colline: c => `disque(x, 222, h, ${c})`,
    sapin: (c, c2) => `rect(x - 3, 208 - h, 6, h, ${c2}) tri(x - 16, 216 - h, x + 16, 216 - h, x, 180 - h, ${c})`,
    arbre: (c, c2, l) => `image(${l.n0} + i % 3, x - 12, 174 + h % 5, 0, 1)`,   // image n0, n0 + 1, n0 + 2 = les 3 arbres (ARBRES) ; le sol les recouvre un peu
    etoile: c => `if (i + int(temps() * 2)) % 4 != 0 rect(x, y, 2, 2, ${c}) end`,
    planete: (c, c2) => `disque(x, y, h, ${c}) disque(x - h / 3, y - h / 3, h / 3, ${c2}) disque(x + h / 3, y + h / 3, h / 5, ${c2}) ligne(x - h * 4 / 3, y + h / 2, x + h * 4 / 3, y - h / 2, A1)`
  };
  // Une couche : { el, c, c2, at: [[x, y, h]...] (positions fixes) } ou { el, nb, pas, vit, k, y, dy, h, dz } (nb éléments répartis sur la largeur, vit = dérive en px/s, y + DY % dy, h + DZ * dz). Dans l'ordre : du fond vers l'avant.
  const couches = (liste) => '  var t = 0\n' + liste.map(l => {
    const el = ELEMENTS[l.el](l.c || 'A1', l.c2 || 'A3', l);
    if (l.at) return l.at.map(([x, y, h]) => `  x = ${x} y = ${y} h = ${h}\n  ${el}`).join('\n');
    const ix = l.pas > 1 ? `i * ${l.pas}` : 'i', ex = l.vit ? `(DX[${ix}]${l.k > 1 ? ` * ${l.k}` : ''} + t) % 380 - 30` : `DX[${ix}]`;
    const ey = l.dy ? `${l.y || 0} + DY[i] % ${l.dy}` : l.y || 0, eh = l.dz ? `${l.h || 0} + DZ[i]${l.dz > 1 ? ` * ${l.dz}` : ''}` : l.h || 0;
    return (l.vit ? `  t = int(temps() * ${l.vit})\n` : '') + `  i = 0
  while i < ${l.nb}
    x = ${ex} y = ${ey} h = ${eh}
    ${el}
    i += 1
  end`;
  }).join('\n');

  // ---------- les mondes : ciel (haut, bas), sol, 4 couleurs d'ambiance A1..A4, décor, bon objet, mauvais objet (centrés en (x, y), rayon 8) ----------
  const MONDES = {
    foret: { nom: 'dans la foret', ciel: [[130, 200, 240], [200, 235, 200]], sol: [[70, 140, 60], [105, 185, 80]], a: [[255, 225, 90], [255, 255, 255], [110, 75, 45], [40, 120, 70]], n: 8,
      decor: r => (r.arbres ? '  # arbres-foret\n' : '') + couches([
        { el: 'soleil', at: [[270, 40, 16]] },
        { el: 'nuage', c: 'A2', nb: 3, vit: 4, k: 2, y: 28, dy: 40, h: 9 },
        { el: 'colline', c: 'SOL2', nb: 4, pas: 2, h: 44, dz: 2 },
        r.arbres ? { el: 'arbre', n0: r.arbres, nb: 8, dz: 1 } : { el: 'sapin', c: 'A4', c2: 'A3', nb: 8, h: 22, dz: 1 }]),   // r.arbres = numéro de la 1re image d'arbre (0 : sapins dessinés)
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
    espace: { nom: 'dans l espace', ciel: [[3, 3, 18], [45, 22, 85]], sol: [[95, 90, 115], [135, 130, 155]], a: [[255, 255, 255], [150, 90, 200], [110, 60, 160], [255, 240, 160]], n: 16,
      decor: couches([
        { el: 'etoile', nb: 16, dy: 200 },
        { el: 'planete', c: 'A2', c2: 'A3', at: [[70, 70, 26]] },
        { el: 'planete', c: 'A4', c2: 'A2', at: [[262, 44, 11]] }]),
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

  // ---------- l'AVANT-PLAN de chaque monde : dessiné APRÈS le personnage, qui passe donc derrière (3 éléments par écran, positions DX décalées de celles du décor ; sol à y = 208) ----------
  // Même contexte que `decor` (variables i j x y h, tableaux DX DY DZ, couleurs A1..A4). Un nouveau monde = une entrée ici (sinon pas d'avant-plan).
  const POS_AV = '(DX[i] * 2 + 60) % 320';
  const AVANT = {
    foret: `  i = 1
  while i < 8
    x = ${POS_AV} h = 11 + DZ[i] % 5
    disque(x - h, 214, h * 3 / 4, rgb(30, 105, 55)) disque(x + h, 214, h * 2 / 3, rgb(30, 105, 55)) disque(x, 210, h, rgb(40, 125, 62)) disque(x - 3, 206, h / 3, rgb(95, 170, 70))
    i += 3
  end`,
    ville: `  i = 1
  while i < 8
    x = ${POS_AV}
    rect(x - 22, 196, 44, 14, rgb(18, 18, 36)) rect(x - 13, 187, 26, 10, rgb(18, 18, 36)) rect(x - 10, 189, 8, 6, rgb(60, 70, 120)) rect(x + 2, 189, 8, 6, rgb(60, 70, 120)) rect(x + 18, 199, 4, 3, A3)
    disque(x - 12, 210, 4, NO) disque(x + 12, 210, 4, NO)
    i += 3
  end`,
    espace: `  i = 1
  while i < 8
    x = ${POS_AV} h = 10 + DZ[i] % 5
    tri(x - 5, 210, x + 5, 210, x, 188 - h, A2)
    disque(x - 6, 214, h, rgb(70, 62, 92)) disque(x + h / 2, 212, h * 2 / 3, rgb(88, 80, 112))
    i += 3
  end`,
    eau: `  var s = int(temps() * 3)
  i = 1
  while i < 8
    x = ${POS_AV}
    ligne(x, 216, x + (s + i) % 3 * 3 - 3, 172 - DZ[i], rgb(25, 120, 70)) ligne(x + 1, 216, x + 1 + (s + i) % 3 * 3 - 3, 172 - DZ[i], rgb(25, 120, 70)) ligne(x + 2, 216, x + 2 + (s + i) % 3 * 3 - 3, 172 - DZ[i], rgb(25, 120, 70))
    ligne(x + 8, 216, x + 8 + (s + i + 1) % 3 * 3 - 3, 184 - DZ[i], rgb(35, 150, 85)) ligne(x + 9, 216, x + 9 + (s + i + 1) % 3 * 3 - 3, 184 - DZ[i], rgb(35, 150, 85))
    i += 3
  end`,
    desert: `  i = 1
  while i < 8
    x = ${POS_AV}
    if i == 4
      disque(x, 216, 14, rgb(170, 110, 60)) disque(x + 12, 218, 10, rgb(150, 95, 52)) disque(x - 4, 210, 5, rgb(205, 150, 90))
    else
      rect(x - 4, 172, 8, 40, rgb(40, 110, 60)) rect(x - 14, 184, 6, 14, rgb(40, 110, 60)) rect(x - 14, 194, 14, 6, rgb(40, 110, 60)) rect(x + 8, 180, 6, 14, rgb(40, 110, 60)) rect(x + 2, 190, 8, 6, rgb(40, 110, 60))
    end
    i += 3
  end`,
    neige: `  i = 1
  while i < 8
    x = ${POS_AV}
    if i == 4
      disque(x, 224, 26, A1) disque(x - 10, 220, 14, rgb(205, 222, 245)) disque(x + 14, 222, 12, A1)
    else
      rect(x - 3, 212, 6, 8, A4) tri(x - 22, 214, x + 22, 214, x, 150, rgb(25, 80, 60)) tri(x - 15, 184, x + 15, 184, x, 140, rgb(25, 80, 60)) tri(x - 7, 156, x + 7, 156, x, 140, A1)
    end
    i += 3
  end`
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
    const tirH = r.jeu === 'tir-haut', cp = COMPOS.indexOf(r.compo), ty = TYPES_EN.indexOf(r.typeEn);   // tir vers le haut : les ennemis (les mauvais objets) bougent et se distinguent comme dans les autres tirs
    const perte = (cond) => `if ${cond} VIES -= 1 HT = 0.8 son(140, 160) effet(IX[i], IY[i]) if VIES <= 0 FIN = true son(100, 400) end end`;
    return `# ${nom}${tirH ? '\nimport math' : ''}
var C1 = 0 var C2 = 0 var C3 = 0 var BL = 0 var NO = 0 var SOL = 0 var SOL2 = 0 var A1 = 0 var A2 = 0 var A3 = 0 var A4 = 0 var BAND = []
var DX = ${dx} var DY = ${dy} var DZ = ${dz}
var PX = 148.0 var PY = 184.0 var PTS = 0 var VIES = 3 var FIN = false var HT = 0.0 var SP = 0.6
var IX = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var IY = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var IK = [0, 0, 0, 0, 0, 0] var IV = [false, false, false, false, false, false]
var FX = [0.0, 0.0, 0.0, 0.0] var FY = [0.0, 0.0, 0.0, 0.0] var FT = [0.0, 0.0, 0.0, 0.0]
${tirH ? `var IB = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var IP = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var IE = [0, 0, 0, 0, 0, 0] var IH = [1, 1, 1, 1, 1, 1] var COMPO = ${cp} var TYPE = ${ty}
` : ''}${ac.vars}

def effet(x, y)
  var i = 0
  while i < 4
    if FT[i] <= 0 FX[i] = x FY[i] = y FT[i] = 0.4 return end
    i += 1
  end
end

def decor()
  var i = 0 var j = 0 var x = 0 var y = 0 var h = 0
${typeof mo.decor === 'function' ? mo.decor(r) : mo.decor}
end

def avant()
  var i = 0 var j = 0 var x = 0 var y = 0 var h = 0
${AVANT[r.monde]}
end

def objet(x, y, k)
  if k == 0
${objetCode(r, 'bon', mo, im)}
  else
${objetCode(r, 'mal', mo, im)}
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
${tirH ? `        IB[i] = IX[i] IP[i] = real(hasard(0, 6)) IE[i] = 0 IH[i] = 1
        if TYPE == 1 && hasard(0, 2) == 0 IE[i] = 1 IH[i] = 3 end
        if TYPE == 2 && hasard(0, 2) == 0 IE[i] = 2 end
        if TYPE == 3
          IE[i] = hasard(0, 2)
          if IE[i] == 1 IH[i] = 3 end
        end
` : ''}        i = 6
      end
      i += 1
    end
  end
  i = 0
  while i < 6
    if IV[i]
${tirH ? `      var ve = vc
      if COMPO == 3 ve = ve * (1.0 + IY[i] / 150.0) end
      if IE[i] == 2 ve = ve * 1.8 end
      IY[i] += ve * dt IP[i] += 3 * dt
      if COMPO == 1
        IX[i] = IB[i] + math.sin(IP[i]) * 30
      elif COMPO == 2
        if PX + 12 > IX[i] + 2 IX[i] += 40 * dt elif PX + 12 < IX[i] - 2 IX[i] -= 40 * dt end
      end
      if IX[i] < 12 IX[i] = 12.0 elif IX[i] > 308 IX[i] = 308.0 end` : '      IY[i] += vc * dt'}
${tirH ? ac.objet.replace('IV[i] = false SV[k] = false PTS += 1 score(PTS) son(520, 50) effet(IX[i], IY[i])', 'SV[k] = false IH[i] -= 1\n            if IH[i] <= 0 IV[i] = false PTS += 1 score(PTS) son(520, 50) effet(IX[i], IY[i]) else son(300, 30) end') : ac.objet}
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
    if IV[i]
      objet(int(IX[i]), int(IY[i]), IK[i])
${tirH ? `      if IE[i] == 1 texte(int(IX[i]) - 3, int(IY[i]) - 25, str(IH[i]), NO, 2) texte(int(IX[i]) - 4, int(IY[i]) - 26, str(IH[i]), BL, 2) end
      if IE[i] == 2 rect(int(IX[i]) - 4, int(IY[i]) - 24, 2, 9, BL) rect(int(IX[i]) + 3, int(IY[i]) - 20, 2, 7, BL) end
` : ''}    end
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
  avant()
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
    const parNiv = !!r.mondes && new Set(r.mondes).size > 1;   // un monde par niveau : couleurs, décor, avant-plan et objets se choisissent d'après NV
    const quand = f => { const ws = [...new Set(r.mondes)]; return ws.map((w, j) => (j === ws.length - 1 ? '  else' : `  ${j ? 'elif' : 'if'} ${r.mondes.map((x, k) => x === w ? 'NV == ' + (k + 1) : '').filter(Boolean).join(' || ')}`) + '\n' + f(MONDES[w], w)).join('\n') + '\n  end'; };
    const dxyz = m => [liste(m.n, i => Math.round(14 + i * 292 / Math.max(1, m.n - 1) + (i * 37) % 17 - 8)), liste(m.n, i => (i * 53 + 17) % 180), liste(m.n, i => (i * 7) % 14)];
    const palette = m => `  SOL = ${rgb(m.sol[0])} SOL2 = ${rgb(m.sol[1])}
  A1 = ${rgb(m.a[0])} A2 = ${rgb(m.a[1])} A3 = ${rgb(m.a[2])} A4 = ${rgb(m.a[3])}
  BAND = ${liste(10, i => rgb(mel(m.ciel[0], m.ciel[1], i / 9)))}`;
    const decorDe = m => typeof m.decor === 'function' ? m.decor(r) : m.decor, objetDe = k => r[k] === 'img' || BE.forme(r[k]) || !parNiv ? objetCode(r, k, mo, im) : quand(m => m[k]);
    const dx = liste(mo.n, i => Math.round(14 + i * 292 / Math.max(1, mo.n - 1) + (i * 37) % 17 - 8)), dy = liste(mo.n, i => (i * 53 + 17) % 180), dz = liste(mo.n, i => (i * 7) % 14);
    return {
      parNiv,
      tete: `var C1 = 0 var C2 = 0 var C3 = 0 var BL = 0 var NO = 0 var SOL = 0 var SOL2 = 0 var RO = 0 var A1 = 0 var A2 = 0 var A3 = 0 var A4 = 0 var BAND = []
var DX = ${dx} var DY = ${dy} var DZ = ${dz}
var PTS = 0 var VIES = 3 var FIN = 0 var HT = 0.0 var T = 0.0 var V = ${v === undefined ? -1 : v}`,
      defs: `def couleurs()
  C1 = rgb(${col.join(', ')}) C2 = rgb(${fonce(col).join(', ')}) C3 = rgb(${clair(col).join(', ')}) BL = rgb(255, 255, 255) NO = rgb(30, 30, 45) RO = rgb(70, 70, 80)
${parNiv ? quand(m => palette(m) + '\n  DX = ' + dxyz(m)[0] + ' DY = ' + dxyz(m)[1] + ' DZ = ' + dxyz(m)[2]) : palette(mo)}
end

def decor()
  var i = 0 var j = 0 var x = 0 var y = 0 var h = 0
${parNiv ? quand(decorDe) : decorDe(mo)}
end

def avant()
  var i = 0 var j = 0 var x = 0 var y = 0 var h = 0
${parNiv ? quand((m, w) => AVANT[w]) : AVANT[r.monde]}
end

def objet(x, y, k)
  if k == 0
${objetDe('bon')}
  else
${objetDe('mal')}
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
  // toits : un immeuble (rect gris, fenêtres, toit) et la figure choisie, dessinée autour du personnage (p de 0 à 1 pendant 0,4 s)
  const FIGURES = {
    salto: '  while i < 4\n    a = (p + i * 0.25) * 6.2832\n    disque(x + int(math.cos(a) * 20.0), y + int(math.sin(a) * 20.0), 3, A4)\n    i += 1\n  end',
    vrille: '  while i < 6\n    a = (p * 2.0 + i / 6.0) * 6.2832\n    disque(x + int(math.cos(a) * 18.0), y + int(math.sin(a) * 6.0), 3, A4)\n    i += 1\n  end',
    etoile: '  r = 10 + int(14.0 * math.sin(p * 3.1416))\n  while i < 8\n    a = i * 0.7854\n    ligne(x + int(math.cos(a) * 6.0), y + int(math.sin(a) * 6.0), x + int(math.cos(a) * r), y + int(math.sin(a) * r), A4)\n    i += 1\n  end' };
  const toitsDefs = f => `
def immeuble(x, y)
  var j = y + 8
  rect(x - 12, y, 24, 208 - y, RO) rect(x - 12, y, 24, 3, A3)
  while j < 200
    rect(x - 8, j, 5, 5, A4) rect(x + 3, j, 5, 5, A4)
    j += 10
  end
end

def figure(x, y, p)
  var i = 0 var a = 0.0 var r = 0
${FIGURES[f] || FIGURES.salto}
end
`;
  function plateforme(r) {
    const cote = r.jeu === 'cote', toits = r.jeu === 'toits', b = base(r, cote ? 0 : -1), o = r.obst === 'avec';
    const c = toits ? { nom: 'Toits ', long: 1400, pl: false, hz: [300, 480, 660, 840, 1020, 1200], vh: 0, fin: 'drapeau', rival: 0, auto: true, saut: true, toits: true }   // immeubles de 20, 28 puis 36 px : le saut (50 px) les passe, avec 6 px de marge pour le plus haut
      : cote ? { nom: 'Course cote ', long: 1400, pl: false, hz: o ? [300, 480, 660, 840, 1020, 1200] : [], vh: 0, fin: 'drapeau', rival: 180, auto: true, saut: o }
      : { ...PLATEFORMES[r.jeu], long: 1000, pl: true, auto: false, saut: true };
    // plusieurs niveaux : les données de chaque niveau sont à plat (niveau k = tranche k) et charge() les recopie au début du niveau
    // niveau 1 placé à la main, jusqu'à 4 niveaux (r.edit : 7 plateformes [x, haut, largeur], 1 à 8 étoiles [x, y], 0 à 8 monstres x) : les autres niveaux ne changent pas
    const nb = (v, a, z) => Math.max(a, Math.min(z, Math.round(+v) || a)), e = r.edit;
    const ed = c.pl && nbNiv(r.niv) <= 4 && e && e.pl?.length === 7 ? { pl: e.pl.map(p => [nb(p[0], 0, 940), nb(p[1], 72, 192), nb(p[2], 30, 140)]), st: (e.st || []).slice(0, 8).map(q => [nb(q[0], 0, 990), nb(q[1], 20, 200)]), hz: (e.hz || []).slice(0, 8).map(h => Array.isArray(h) ? [nb(h[0], 70, 900), nb(h[1], 60, 200)] : [nb(h, 70, 900), 200]) } : null;   // monstre : [x, y] (un nombre seul = au sol)
    const nn = c.pl ? nbNiv(r.niv) : 1, multi = nn > 1, plans = liste0(nn, k => k === 0 && ed ? ed.pl : r.plan === 'oui' ? PLANS[(k + 1) % PLANS.length] : PLATS);
    const pool = [...c.hz, ...PIEGES_PLUS], hzK = liste0(nn, k => k === 0 && ed ? ed.hz.map(h => h[0]) : pool.slice(0, c.hz.length + (r.piege === 'oui' ? 2 * k : 0)).map(x => r.plan === 'oui' ? x + DEC[k % 6] : x)), NH = Math.max(...hzK.map(h => h.length));
    const hzPad = k => Array.from({ length: NH }, (_, i) => i < hzK[k].length ? hzK[k][i] : -999);
    if (multi) c.hz = hzPad(0); else if (ed) c.hz = ed.hz.map(h => h[0]);
    const hy = c.hz.map((_, i) => c.toits ? 208 - [20, 28, 36][i % 3] : ed?.hz[i]?.[1] ?? 200);   // hauteur de chaque monstre : 200 = au sol ; toits : le haut de l'immeuble
    const pl = c.pl ? plans[0] : [], st = c.pl ? (ed?.st.length ? ed.st : [...pl.map(p => [p[0] + p[2] / 2, p[1] - 16]), [70, 192]]) : [], FL = c.long - 60, MAXX = c.long - 30, drap = c.fin === 'drapeau';
    const reels = a => liste(a.length, i => a[i].toFixed(1));
    return `${nomJeu(c.nom, r)}${c.toits ? 'import math\n' : ''}${b.tete}
${c.toits ? 'var TP = 0.0 var TOK = 0\n' : ''}var PX = 20.0 var PY = 184.0 var PVY = 0.0 var sol = true var CAMX = 0 var RX = 20.0
var PLX = ${liste(pl.length, i => pl[i][0])} var PT = ${liste(pl.length, i => pl[i][1])} var PW = ${liste(pl.length, i => pl[i][2])}
var SX = ${liste(st.length, i => st[i][0])} var SY = ${liste(st.length, i => st[i][1])} var SV = ${liste(st.length, () => 'true')}
var HX = ${reels(c.hz)} var HB = ${reels(c.hz)} var HD = ${liste(c.hz.length, () => '1.0')} var HY = ${reels(hy)}
${multi ? `var LP = ${liste(nn * 7, i => plans[(i / 7) | 0][i % 7][0])} var LT = ${liste(nn * 7, i => plans[(i / 7) | 0][i % 7][1])} var LW = ${liste(nn * 7, i => plans[(i / 7) | 0][i % 7][2])}
var LH = ${liste(nn * NH, i => hzPad((i / NH) | 0)[i % NH].toFixed(1))}
${ed ? `var E1X = ${liste(st.length, i => st[i][0])} var E1Y = ${liste(st.length, i => st[i][1])} var E1H = ${reels(hy)}\n` : ''}` : ''}${b.defs}${c.toits ? toitsDefs(r.figtoit) : ''}
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
  while i < ${NH} HB[i] = LH[k * ${NH} + i] HY[i] = ${ed ? '(k == 0 ? E1H[i] : 200.0)' : '200.0'} i += 1 end
end

` : ''}def nouvelle()
  var i = 0
${multi ? '  charge()\n' + (b.parNiv ? '  couleurs()\n' : '') : ''}  PX = 20.0 PY = 184.0 PVY = 0.0 sol = true RX = 20.0 PTS = 0 VIES = 3 FIN = 0 HT = 0.0 T = 0.0
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
${c.toits ? `  if appui(P3)
    if sol PVY = -300.0 sol = false son(660, 40) TOK = 0 elif TP == 0.0 && TOK == 0 TP = 0.01 son(880, 50) end
  end
  if TP > 0.0
    TP += dt * 2.5
    if TP >= 1.0 TP = 0.0 TOK = 1 son(1320, 80) end
  end` : c.saut ? '  if appui(P3) && sol PVY = -300.0 sol = false son(660, 40) end' : ''}
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
${c.toits ? '  if sol && TOK == 1 PTS += 3 score(PTS) son(1500, 80) TOK = 0 end\n  if sol TP = 0.0 end\n' : ''}  i = 0
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
` : ''}    if HT <= 0 && ${c.toits ? 'HX[i] + 12 > PX + 3 && HX[i] - 12 < PX + 21 && PY + 24 > HY[i]' : 'HX[i] + 8 > PX + 3 && HX[i] - 8 < PX + 21 && PY + 24 > HY[i] - 8 && PY + 4 < HY[i] + 8'}
      VIES -= 1 HT = 1.0 son(140, 160) PVY = -220.0 sol = false PX -= 30${c.toits ? ' TP = 0.0 TOK = 0' : ''}
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
    if HX[i] > CAMX - 20 && HX[i] < CAMX + 340 ${c.toits ? 'immeuble(int(HX[i]), int(HY[i]))' : 'objet(int(HX[i]), int(HY[i]), 1)'} end
    i += 1
  end
${c.rival ? `  if RX > CAMX - 30 && RX < CAMX + 340
    s = C1 C1 = rgb(95, 95, 105)
    perso(int(RX), 202)
    C1 = s
  end` : ''}
  rect(int(PX) + 3, 207, 18, 3, SOL2)
  if HT <= 0 || int(t * 12) % 2 == 0 perso(int(PX), int(PY)) parle(int(PX), int(PY), CAMX) end
${c.toits ? '  if TP > 0.0 figure(int(PX) + 12, int(PY) + 12, TP) end\n' : ''}  s = CAMX % 320
  camera(s, 0) avant() camera(s - 320, 0) avant()
  camera(0, 0)
  ${c.fin === 'etoiles' ? `texte(9, 9, str(PTS) + "/${st.length}", NO, 3) texte(8, 8, str(PTS) + "/${st.length}", BL, 3)` : 'texte(9, 9, str(PTS), NO, 3) texte(8, 8, str(PTS), BL, 3)'}
${drap ? `  barre(int(PX * 100 / ${c.long}), ${c.rival ? `int(RX * 100 / ${c.long})` : -1})` : ''}
${c.hz.length ? '  coeurs()' : ''}
  fin()
end
`;
  }

  // lianes : on se balance de liane en liane. Accroché : G monte sur la liane, D descend, le gros bouton lâche ; en l'air, on rattrape toute liane qu'on croise. Le vide (pointes) coûte une vie : on repart de la dernière liane.
  // Le balancement est imposé (angle = 0,85 rad x sin), donc la portée d'un lâcher ne dépend que de la longueur de corde (mesurée : 84 px à 40 de corde, 116 à 80, 141 à 130) : 18 lianes espacées de 90 à 112 px (de plus en plus loin : il faut penser à allonger la corde), toutes atteignables.
  const LIANES = [], NB_LIANES = 18; for (let i = 0, x = 70; i < NB_LIANES; x += 90 + Math.round(i * 22 / 16) + (i % 2) * 4, i++) LIANES.push(x);   // x des lianes
  const LI_FIN = LIANES[NB_LIANES - 1] + 100, LI_LONG = LI_FIN + 190;   // x d'arrivée (la plateforme) ; largeur du monde
  function lianes(r) {
    const b = base(r, -1), n = LIANES.length, ns = n - 1;
    return `${nomJeu('Lianes ', r)}import math
${b.tete}
var AX = ${liste(n, i => LIANES[i])} var SX = ${liste(ns, k => (LIANES[k] + LIANES[k + 1]) / 2)} var SY = ${liste(ns, k => 60 + (k * 37) % 50)} var SV = ${liste(ns, () => 'true')}
var GX = ${LIANES[0]}.0 var GY = 80.0 var GVX = 0.0 var GVY = 0.0 var AT = 0 var LAST = 0 var LR = 80.0 var PH = 0.0 var CD = 0.0 var PX = ${LIANES[0] - 12}.0 var PY = 78.0 var sol = true var CAMX = 0
${b.defs}
def nouvelle()
  var i = 0
  GX = ${LIANES[0]}.0 GY = 80.0 GVX = 0.0 GVY = 0.0 AT = 0 LAST = 0 LR = 80.0 PH = 0.0 CD = 0.0 PX = ${LIANES[0] - 12}.0 PY = 78.0 sol = true
  PTS = 0 VIES = 3 FIN = 0 HT = 0.0 T = 0.0
  while i < ${ns} SV[i] = true i += 1 end
  score(0)
end

def debut()
  couleurs()
  nouvelle()
end

def maj(dt)
  var i = 0 var th = 0.0 var w = 0.0 var v = 0.0 var om = 0.0 var dx = 0.0 var s = 0.0
  if FIN > 0
    if appui(P3) nouvelle() end
    return
  end
  T += dt
  if HT > 0 HT -= dt end
  if CD > 0 CD -= dt end
  if AT >= 0
    if bouton(G) LR -= 60.0 * dt end
    if bouton(D) LR += 60.0 * dt end
    if LR < 40.0 LR = 40.0 elif LR > 130.0 LR = 130.0 end
    om = 3.3 * math.sqrt(80.0 / LR)
    PH += om * dt
    th = 0.85 * math.sin(PH)
    GX = AX[AT] + LR * math.sin(th)
    GY = LR * math.cos(th)
    if appui(P3)
      v = LR * 0.85 * om * math.cos(PH)
      GVX = v * math.cos(th) GVY = -v * math.sin(th)
      AT = -1 CD = 0.3 sol = false son(660, 40)
    end
  else
    GVY += 900.0 * dt
    GX += GVX * dt GY += GVY * dt
    if CD <= 0 && GY > 24 && GY < 140
      i = 0
      while i < ${n}
        if GX > AX[i] - 12 && GX < AX[i] + 12
          LR = GY
          if LR < 40.0 LR = 40.0 elif LR > 130.0 LR = 130.0 end
          dx = GX - AX[i]
          th = math.atan2(dx, LR)
          w = (GVX * math.cos(th) - GVY * math.sin(th)) / LR
          s = th / 0.85
          if s > 1.0 s = 1.0 elif s < -1.0 s = -1.0 end
          PH = math.asin(s)
          if w < 0 PH = 3.1416 - PH end
          AT = i LAST = i sol = true son(880, 40)
          i = ${n}
        end
        i += 1
      end
    end
  end
  PX = GX - 12.0 PY = GY - 2.0
  i = 0
  while i < ${ns}
    if SV[i] && GX > SX[i] - 16 && GX < SX[i] + 16 && GY + 10 > SY[i] - 20 && GY + 10 < SY[i] + 20
      SV[i] = false PTS += 1 score(PTS) son(988, 50)
    end
    i += 1
  end
  if FIN == 0 && GX >= ${LI_FIN}
    ${GAGNE}
  elif FIN == 0 && AT < 0 && GY >= 188.0
    VIES -= 1 HT = 1.0 son(140, 160)
    if VIES <= 0
      FIN = 2 son(100, 400)
    else
      AT = LAST LR = 80.0 PH = 0.0 GVX = 0.0 GVY = 0.0 GX = AX[LAST] * 1.0 GY = 80.0 PX = GX - 12.0 PY = 78.0 sol = true
    end
  end
end

def dessine()
  var i = 0 var t = temps() var s = 0 var x = 0
  CAMX = int(GX - 140)
  if CAMX < 0 CAMX = 0 elif CAMX > ${LI_LONG - 320} CAMX = ${LI_LONG - 320} end
  fond(BAND[0])
  while i < 10 rect(0, i * 24, 320, 24, BAND[i]) i += 1 end
  s = CAMX * 3 / 10 % 320
  camera(s, 0) decor() camera(s - 320, 0) decor()
  camera(CAMX, 0)
  rect(${LI_FIN - 10}, 170, ${LI_LONG - LI_FIN + 10}, 70, SOL) rect(${LI_FIN - 10}, 170, ${LI_LONG - LI_FIN + 10}, 4, SOL2)
  rect(${LI_FIN + 80}, 130, 3, 40, NO) tri(${LI_FIN + 83}, 130, ${LI_FIN + 83}, 142, ${LI_FIN + 100}, 136, A4)
  rect(CAMX, 214, 320, 26, NO)
  i = CAMX / 20
  while i < CAMX / 20 + 17
    if i * 20 < ${LI_FIN - 10} tri(i * 20, 240, i * 20 + 20, 240, i * 20 + 10, 204 + i % 2 * 6, rgb(210, 60, 50)) end
    i += 1
  end
  i = 0
  while i < ${n}
    x = AX[i]
    if i != AT && x > CAMX - 20 && x < CAMX + 340
      rect(x - 2, 0, 5, 140, NO) rect(x - 1, 0, 3, 140, rgb(110, 170, 70))
      tri(x + 2, 50, x + 11, 44, x + 8, 55, rgb(70, 150, 60)) tri(x - 2, 90, x - 11, 84, x - 8, 95, rgb(70, 150, 60)) disque(x, 141, 4, rgb(70, 150, 60))
    end
    i += 1
  end
  i = 0
  while i < ${ns}
    if SV[i] objet(int(SX[i]), SY[i], 0) end
    i += 1
  end
  if AT >= 0 ligne(AX[AT], 0, int(GX), int(GY), NO) ligne(AX[AT] + 1, 0, int(GX) + 1, int(GY), rgb(110, 170, 70)) end
  if HT <= 0 || int(t * 12) % 2 == 0 perso(int(PX), int(PY)) parle(int(PX), int(PY), CAMX) end
  s = CAMX % 320
  camera(s, 0) avant() camera(s - 320, 0) avant()
  camera(0, 0)
  texte(9, 9, str(PTS) + "/${ns}", NO, 3) texte(8, 8, str(PTS) + "/${ns}", BL, 3)
  barre(int(GX * 100 / ${LI_LONG}), -1)
  coeurs()
  if T < 8.0 texte(17, 41, "G monte  D descend  gros bouton : lacher", NO, 1) texte(16, 40, "G monte  D descend  gros bouton : lacher", BL, 1) end
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
    const b = base(r), q = r.quest?.length ? r.quest : QUESTIONS[r.monde], n = q.length;   // r.quest : questions écrites par l'IA selon le sujet de l'enfant (même format que QUESTIONS)
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

  // jeux de tir a look fixe (vaisseau, ennemis) : les memes Berry que systemes/jeu-shmup.be et jeu-vaisseau-centre.be, testes dans verif ; personnage, monde et gros bouton ne se choisissent pas
  const TIR_PERSO = `# Tir et deplace
import math
var y = 110.0 var pts = 0 var vies = 3 var fin = false var inv = 0.0 var prochain = 1.0 var rythme = 1.0
var BX = [] var BY = []                             # 5 tirs : BX < 0 = libre
var EX = [] var EY = [] var EP = [] var EA = []     # 6 ennemis : EP = phase de l'ondulation
var EB = [] var EK = [] var EH = []                 # EB = hauteur de depart, EK = type (0 normal, 1 costaud, 2 rapide), EH = points de vie
var SX = [] var SY = []                             # 24 etoiles

def reset()
  y = 110.0 pts = 0 vies = 3 fin = false inv = 0.0 prochain = 1.0 rythme = 1.0
  for i: 0 .. 4 BX[i] = -10.0 end
  for i: 0 .. 5 EA[i] = false end
end
def debut()
  for i: 0 .. 4 BX.push(-10.0) BY.push(0.0) end
  for i: 0 .. 5 EX.push(0.0) EY.push(0.0) EP.push(0.0) EA.push(false) EB.push(0.0) EK.push(0) EH.push(1) end
  for i: 0 .. 23 SX.push(hasard(0, 319)) SY.push(hasard(0, 239)) end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) y -= 130 * dt end
  if bouton(D) y += 130 * dt end
  if y < 8 y = 8.0 elif y > 232 y = 232.0 end
  if inv > 0 inv -= dt end
  for i: 0 .. 23 SX[i] -= (i % 2 == 0 ? 30 : 80) * dt if SX[i] < 0 SX[i] += 320 end end   # 2 couches : lente et rapide
  if appui(P3)
    for i: 0 .. 4 if BX[i] < 0 BX[i] = 50.0 BY[i] = y son(1200, 20) break end end
  end
  for i: 0 .. 4 if BX[i] >= 0 BX[i] += 320 * dt if BX[i] > 320 BX[i] = -10.0 end end end
  rythme += 0.02 * dt prochain -= dt * rythme
  if prochain <= 0
    prochain = 1.0
    for i: 0 .. 5
      if !EA[i]
        EA[i] = true EX[i] = 330.0 EY[i] = real(hasard(20, 220)) EB[i] = EY[i] EP[i] = real(hasard(0, 6))
        var k = 0
        if TYPE == 1 && hasard(0, 2) == 0 k = 1 end
        if TYPE == 2 && hasard(0, 2) == 0 k = 2 end
        if TYPE == 3 k = hasard(0, 2) end
        EK[i] = k EH[i] = 1
        if k == 1 EH[i] = 3 end
        break
      end
    end
  end
  for i: 0 .. 5
    if EA[i]
      var v = 60 + pts * 2
      if COMPO == 3 v = v * (1.0 + (330 - EX[i]) / 220) end
      if EK[i] == 2 v = v * 1.8 end
      EX[i] -= v * dt EP[i] += 3 * dt
      if COMPO == 1
        EY[i] = EB[i] + math.sin(EP[i]) * 30
      elif COMPO == 2
        if y > EY[i] + 2 EY[i] += 45 * dt elif y < EY[i] - 2 EY[i] -= 45 * dt end
      end
      if EY[i] < 8 EY[i] = 8.0 elif EY[i] > 232 EY[i] = 232.0 end
      var dy = EY[i] - y var dx = EX[i] - 34
      if EX[i] < -20 EA[i] = false
      elif inv <= 0 && dx * dx + dy * dy < 300 EA[i] = false vies -= 1 inv = 1.5 son(150, 200) end
      for j: 0 .. 4
        var ex = BX[j] - EX[i] var ey = BY[j] - EY[i]
        if BX[j] >= 0 && EA[i] && ex * ex + ey * ey < 200
          BX[j] = -10.0 EH[i] -= 1
          if EH[i] <= 0 EA[i] = false pts += 1 score(pts) son(440, 50) else son(300, 30) end
        end
      end
    end
  end
  if vies <= 0 fin = true son(200, 300) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for i: 0 .. 23 rect(int(SX[i]), SY[i], 2, 2, i % 2 == 0 ? PT1 : PT2) end
  for i: 0 .. 5
    if EA[i]
      objet(int(EX[i]), int(EY[i]), 1)
      if EK[i] == 1 texte(int(EX[i]) - 4, int(EY[i]) - 24, str(EH[i]), TXT, 2) end
      if EK[i] == 2 rect(int(EX[i]) + 10, int(EY[i]) - 4, 14, 2, TXT) rect(int(EX[i]) + 14, int(EY[i]) + 3, 10, 2, TXT) end
    end
  end
  for i: 0 .. 4 if BX[i] >= 0 objet(int(BX[i]), int(BY[i]), 0) end end
  if inv <= 0 || int(temps() * 10) % 2 == 0
    perso(22, int(y) - 12)
  end
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), TXT, 2)
  if fin texte(80, 100, "PERDU !", TIRC, 4) texte(90, 150, "P3 : rejouer", TXT, 2) end
end
`;
  const TIR_AUTOUR = `# Tir autour
import math
var ang = 0.0 var pts = 0 var vies = 3 var fin = false var inv = 0.0 var prochain = 1.0 var rythme = 1.0
var BX = [] var BY = [] var BVX = [] var BVY = [] var BV = []   # 6 tirs : BV = duree de vie restante (<= 0 : libre)
var AX = [] var AY = [] var AVX = [] var AVY = [] var AA = []   # 6 asteroides, AA = actif
var AP = [] var AK = [] var AH = []                             # AP = phase du zigzag, AK = type (0 normal, 1 costaud, 2 rapide), AH = points de vie

def reset()
  ang = 0.0 pts = 0 vies = 3 fin = false inv = 0.0 prochain = 1.0 rythme = 1.0
  for i: 0 .. 5 BV[i] = 0.0 AA[i] = false end
end
def debut()
  for i: 0 .. 5
    BX.push(0.0) BY.push(0.0) BVX.push(0.0) BVY.push(0.0) BV.push(0.0)
    AX.push(0.0) AY.push(0.0) AVX.push(0.0) AVY.push(0.0) AA.push(false) AP.push(0.0) AK.push(0) AH.push(1)
  end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) ang -= 3.5 * dt end
  if bouton(D) ang += 3.5 * dt end
  if inv > 0 inv -= dt end
  if appui(P3)
    for i: 0 .. 5
      if BV[i] <= 0
        BX[i] = 160.0 + math.cos(ang) * 20 BY[i] = 120.0 + math.sin(ang) * 20 BV[i] = 1.2
        BVX[i] = math.cos(ang) * 260 BVY[i] = math.sin(ang) * 260 son(1200, 20) break
      end
    end
  end
  rythme += 0.02 * dt prochain -= dt * rythme
  if prochain <= 0
    prochain = 1.0
    for i: 0 .. 5
      if !AA[i]
        var a = real(hasard(0, 628)) / 100.0         # arrive d'un point du bord, vise le centre
        AX[i] = 160.0 + math.cos(a) * 200 AY[i] = 120.0 + math.sin(a) * 200
        AVX[i] = -math.cos(a) * 40 AVY[i] = -math.sin(a) * 40 AA[i] = true AP[i] = 0.0
        var k = 0
        if TYPE == 1 && hasard(0, 2) == 0 k = 1 end
        if TYPE == 2 && hasard(0, 2) == 0 k = 2 end
        if TYPE == 3 k = hasard(0, 2) end
        AK[i] = k AH[i] = 1
        if k == 1 AH[i] = 3 end
        break
      end
    end
  end
  for i: 0 .. 5
    if BV[i] > 0 BV[i] -= dt BX[i] += BVX[i] * dt BY[i] += BVY[i] * dt end
    if AA[i]
      var dx = AX[i] - 160 var dy = AY[i] - 120
      var d = math.sqrt(dx * dx + dy * dy) + 0.1
      var s = 1.0
      if AK[i] == 2 s = 1.8 end
      if COMPO == 3 s = s * (1.0 + (200 - d) / 120) end
      AP[i] += 4 * dt
      var vx = AVX[i] var vy = AVY[i]
      if COMPO == 1 vx = vx - AVY[i] * math.sin(AP[i]) * 2 vy = vy + AVX[i] * math.sin(AP[i]) * 2 end
      if COMPO == 2 vx = (-dx * 40 - dy * 30) / d vy = (-dy * 40 + dx * 30) / d end
      AX[i] += vx * s * dt AY[i] += vy * s * dt
      dx = AX[i] - 160 dy = AY[i] - 120
      if dx * dx + dy * dy < 200 && inv <= 0        # touche le vaisseau (distance au carre)
        AA[i] = false vies -= 1 inv = 1.5 son(150, 200)
      end
      for j: 0 .. 5
        var ex = BX[j] - AX[i] var ey = BY[j] - AY[i]
        if BV[j] > 0 && AA[i] && ex * ex + ey * ey < 144
          BV[j] = 0.0 AH[i] -= 1
          if AH[i] <= 0 AA[i] = false pts += 1 score(pts) son(440, 50) else son(300, 30) end
        end
      end
    end
  end
  if vies <= 0 fin = true son(200, 300) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for i: 0 .. 5
    if AA[i]
      objet(int(AX[i]), int(AY[i]), 1)
      if AK[i] == 1 texte(int(AX[i]) - 4, int(AY[i]) - 24, str(AH[i]), TXT, 2) end
      if AK[i] == 2 rect(int(AX[i] - AVX[i] * 0.3), int(AY[i] - AVY[i] * 0.3), 3, 3, TXT) rect(int(AX[i] - AVX[i] * 0.55), int(AY[i] - AVY[i] * 0.55), 2, 2, TXT) end
    end
    if BV[i] > 0 objet(int(BX[i]), int(BY[i]), 0) end
  end
  if inv <= 0 || int(temps() * 10) % 2 == 0 perso(148, 108) end
  tri(160 + int(math.cos(ang) * 27), 120 + int(math.sin(ang) * 27), 160 + int(math.cos(ang + 0.3) * 19), 120 + int(math.sin(ang + 0.3) * 19), 160 + int(math.cos(ang - 0.3) * 19), 120 + int(math.sin(ang - 0.3) * 19), TIRC)   # le viseur : la direction du tir
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), TXT, 2)
  if fin texte(80, 100, "PERDU !", TIRC, 4) texte(90, 150, "P3 : rejouer", TXT, 2) end
end
`;

  // invader : une formation d'ennemis qui va et vient et descend a chaque bord, accelere quand il en reste peu, riposte avec des bombes ; meme Berry que systemes/formation.be, habille par le monde
  const TIR_VAGUES = `# Vagues
var CO = 6 var LI = 3 var VIV = [] var fx = 20.0 var fy = 30.0 var dir = 1 var reste = 0 var nv = 0
var x = 160.0 var tx = 0.0 var ty = -1.0 var pts = 0 var vies = 3 var fin = false var inv = 0.0 var tir_e = 1.0
var BX = [] var BY = []                             # 3 bombes ennemies : BY < 0 = libre ; mon tir : un seul a la fois (ty < 0 = pas de tir)

def vague()
  VIV = []
  for i: 0 .. CO * LI - 1 VIV.push(true) end
  reste = CO * LI fx = 20.0 fy = 30.0 dir = 1
  for k: 0 .. 2 BY[k] = -1.0 end
end
def reset()
  x = 160.0 ty = -1.0 pts = 0 vies = 3 fin = false inv = 0.0 nv = 0 tir_e = 1.0
  vague()
end
def debut()
  for k: 0 .. 2 BX.push(0.0) BY.push(-1.0) end
  vague()
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) x -= 150 * dt end
  if bouton(D) x += 150 * dt end
  if x < 14 x = 14.0 elif x > 306 x = 306.0 end
  if inv > 0 inv -= dt end
  if appui(P3) && ty < 0 tx = x ty = 196.0 son(1200, 20) end
  if ty >= 0 ty -= 320 * dt end
  var morts = CO * LI - reste
  fx += dir * (22 + morts * 4 + nv * 8) * dt        # moins il en reste, plus le bloc va vite
  if fx < 4 || fx + (CO - 1) * 40 + 16 > 316 dir = -dir fx += dir * 4 fy += 10 end   # un bord : tout le bloc descend
  for i: 0 .. CO * LI - 1
    if VIV[i] && ty >= 0
      var cx = fx + (i % CO) * 40 + 8 var cy = fy + (i / CO) * 28 + 8
      if tx > cx - 10 && tx < cx + 10 && ty > cy - 10 && ty < cy + 10
        VIV[i] = false ty = -1.0 reste -= 1 pts += 1 score(pts) son(440, 50)
      end
    end
  end
  if reste == 0 nv += 1 vague() return end          # vague vue : la suivante va plus vite
  tir_e -= dt
  if tir_e <= 0                                     # riposte de plus en plus frequente
    tir_e = 0.35 + reste * 0.05 - nv * 0.05
    if tir_e < 0.3 tir_e = 0.3 end
    var j = hasard(0, CO * LI - 1)
    for q: 0 .. CO * LI - 1
      var m = (j + q) % (CO * LI)
      if VIV[m]
        for k: 0 .. 2
          if BY[k] < 0 BX[k] = fx + (m % CO) * 40 + 8 BY[k] = fy + (m / CO) * 28 + 16 break end
        end
        break
      end
    end
  end
  for k: 0 .. 2
    if BY[k] >= 0
      BY[k] += (110 + nv * 15) * dt
      if BY[k] > 240
        BY[k] = -1.0
      elif inv <= 0 && BY[k] > 196 && BY[k] < 222 && BX[k] > x - 12 && BX[k] < x + 12
        BY[k] = -1.0 vies -= 1 inv = 1.5 son(150, 200)
      end
    end
  end
  if fy + (LI - 1) * 28 + 16 > 190 vies = 0 end     # ils sont arrives en bas
  if vies <= 0 fin = true son(200, 300) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for i: 0 .. CO * LI - 1
    if VIV[i] objet(int(fx) + (i % CO) * 40 + 8, int(fy) + (i / CO) * 28 + 8, 1) end
  end
  for k: 0 .. 2 if BY[k] >= 0 rect(int(BX[k]) - 1, int(BY[k]), 3, 9, TIRC) end end
  if ty >= 0 objet(int(tx), int(ty), 0) end
  if inv <= 0 || int(temps() * 10) % 2 == 0 perso(int(x) - 12, 200) end
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), TXT, 2)
  if fin texte(80, 100, "PERDU !", TIRC, 4) texte(90, 150, "P3 : rejouer", TXT, 2) end
end
`;

  // casse-briques : raquette (G et D), P3 lance la balle, 4 rangees de briques aux couleurs du monde, 3 vies ; meme Berry que systemes/jeu-casse-briques.be, la balle (bon) et le monde se choisissent
  const TIR_CASSE = `# Casse
var bx = 160.0 var by = 212.0 var vx = 0.0 var vy = 0.0 var rx = 130.0 var colle = true
var pts = 0 var vies = 3 var fin = false var gagne = false
var BR = [] var ROW = []                            # 4 rangees de 8 briques : true = encore la ; ROW = couleur de chaque rangee

def reset()
  bx = 160.0 by = 212.0 vx = 0.0 vy = 0.0 rx = 130.0 colle = true pts = 0 vies = 3 fin = false gagne = false
  for i: 0 .. 31 BR[i] = true end
end
def debut()
  for i: 0 .. 31 BR.push(true) end
  ROW.push(A1) ROW.push(A2) ROW.push(A3) ROW.push(A4)
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) rx -= 220 * dt end
  if bouton(D) rx += 220 * dt end
  if rx < 0 rx = 0.0 elif rx > 260 rx = 260.0 end
  if colle
    bx = rx + 30 by = 212.0
    if appui(P3) colle = false vx = 80.0 vy = -170.0 end
    return
  end
  bx += vx * dt by += vy * dt
  if bx < 8 bx = 8.0 vx = -vx son(600, 10) elif bx > 312 bx = 312.0 vx = -vx son(600, 10) end
  if by < 8 by = 8.0 vy = -vy end
  if vy > 0 && by > 206 && by < 222 && bx > rx - 6 && bx < rx + 66
    by = 206.0 vy = -vy vx = (bx - rx - 30) * 5 son(900, 20)   # l'angle depend de l'endroit touche
  end
  if by > 245
    vies -= 1 colle = true son(150, 200)
    if vies <= 0 fin = true end
  end
  var py = by - 8
  if vy > 0 py = by + 8 end
  var c = int(bx / 40) var r = int((py - 30) / 16)
  if py >= 30 && r < 4 && c >= 0 && c < 8 && BR[r * 8 + c]
    BR[r * 8 + c] = false vy = -vy * 1.02 pts += 10 score(pts) son(1200, 20)   # chaque brique accelere un peu la balle
    if vy < -320 vy = -320.0 elif vy > 320 vy = 320.0 end
    var reste = 0
    for i: 0 .. 31 if BR[i] reste += 1 end end
    if reste == 0 fin = true gagne = true son(988, 300) end
  end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for i: 0 .. 31
    if BR[i]
      rect((i % 8) * 40 + 1, 30 + (i / 8) * 16, 38, 14, NO)
      rect((i % 8) * 40 + 2, 31 + (i / 8) * 16, 36, 12, ROW[i / 8])
    end
  end
  rect(int(rx), 220, 60, 6, TXT)
  objet(int(bx), int(by), 0)
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), TXT, 2)
  if fin
    if gagne texte(80, 120, "BRAVO !", TIRC, 4) else texte(80, 120, "PERDU !", TIRC, 4) end
    texte(90, 170, "P3 : rejouer", TXT, 2)
  end
end
`;

  // pong : ma raquette a gauche (G monte, D descend), la console joue a droite, premier a 5 gagne ; meme Berry que systemes/jeu-pong.be, la balle (bon) et le monde se choisissent
  const TIR_PONG = `# Pong
var ry = 100.0 var oy = 100.0 var bx = 160.0 var by = 120.0 var vx = 150.0 var vy = 90.0
var moi = 0 var lui = 0 var fin = false var gagne = false

def serve(sens)
  bx = 160.0 by = 120.0 vx = 150.0 * sens vy = real(hasard(-100, 100))
end
def reset()
  ry = 100.0 oy = 100.0 moi = 0 lui = 0 fin = false gagne = false
  serve(1)
end
def debut()
  reset()
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) ry -= 180 * dt end
  if bouton(D) ry += 180 * dt end
  if ry < 0 ry = 0.0 elif ry > 190 ry = 190.0 end
  if by > oy + 25 oy += 110 * dt elif by < oy + 25 oy -= 110 * dt end   # la console suit la balle, un peu moins vite
  if oy < 0 oy = 0.0 elif oy > 190 oy = 190.0 end
  bx += vx * dt by += vy * dt
  if by < 8 by = 8.0 vy = -vy elif by > 232 by = 232.0 vy = -vy end
  if vx < 0 && bx < 26 && bx > 12 && by > ry - 8 && by < ry + 58
    bx = 26.0 vx = -vx * 1.05 vy = (by - ry - 25) * 5 son(900, 20)   # l'angle depend de l'endroit touche
  end
  if vx > 0 && bx > 294 && bx < 308 && by > oy - 8 && by < oy + 58
    bx = 294.0 vx = -vx * 1.05 vy = (by - oy - 25) * 5 son(900, 20)
  end
  if vx > 330 vx = 330.0 elif vx < -330 vx = -330.0 end
  if bx < -10
    lui += 1 son(150, 200) serve(1)
  elif bx > 330
    moi += 1 score(moi) son(988, 100) serve(-1)
  end
  if moi >= 5 fin = true gagne = true elif lui >= 5 fin = true end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for i: 0 .. 11 rect(158, i * 20 + 4, 4, 10, PT2) end   # filet
  rect(10, int(ry), 8, 50, TXT) rect(302, int(oy), 8, 50, TIRC)
  objet(int(bx), int(by), 0)
  texte(120, 8, str(moi), TXT, 3) texte(184, 8, str(lui), TIRC, 3)
  if fin
    if gagne texte(80, 100, "BRAVO !", TIRC, 4) else texte(80, 100, "PERDU !", TIRC, 4) end
    texte(90, 150, "P3 : rejouer", TXT, 2)
  end
end
`;

  // flappy : P3 bat des ailes, le personnage (perso) tombe tout seul entre des tuyaux aux couleurs du monde qui accelerent ; meme Berry que systemes/jeu-flappy.be
  const TIR_FLAPPY = `# Flappy
var y = 110.0 var vy = 0.0 var pts = 0 var fin = false
var PX = [] var PG = [] var PS = []                 # 3 tuyaux : x, hauteur du haut du trou, deja compte

def reset()
  y = 110.0 vy = 0.0 pts = 0 fin = false
  for i: 0 .. 2 PX[i] = 320.0 + i * 120 PG[i] = hasard(40, 120) PS[i] = false end
end
def debut()
  for i: 0 .. 2 PX.push(0.0) PG.push(0) PS.push(false) end
  reset()
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if appui(P3) vy = -210.0 son(700, 20) end
  vy += 700 * dt y += vy * dt
  if y < 12 y = 12.0 vy = 0.0 end
  if y > 214 fin = true son(150, 300) end
  var v = 90.0 + pts * 2
  if v > 160 v = 160.0 end
  for i: 0 .. 2
    PX[i] -= v * dt
    if PX[i] < -40 PX[i] += 360 PG[i] = hasard(40, 120) PS[i] = false end   # le tuyau sorti revient a droite
    if !PS[i] && PX[i] + 30 < 68 PS[i] = true pts += 1 score(pts) son(988, 40) end
    if PX[i] < 92 && PX[i] + 30 > 68 && (y - 10 < PG[i] || y + 10 > PG[i] + 96) fin = true son(150, 300) end   # dans le tuyau, hors du trou
  end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  rect(0, 226, 320, 14, A3)
  for i: 0 .. 2
    var x = int(PX[i])
    rect(x, 0, 30, PG[i], A1) rect(x - 3, PG[i] - 8, 36, 8, A2)
    rect(x, PG[i] + 96, 30, 130 - PG[i], A1) rect(x - 3, PG[i] + 96, 36, 8, A2)
  end
  perso(68, int(y) - 12)
  texte(150, 8, str(pts), TXT, 4)
  if fin texte(80, 100, "PERDU !", TIRC, 4) texte(90, 150, "P3 : rejouer", TXT, 2) end
end
`;

  // rythme (guitar hero) : des notes tombent dans 3 couloirs, un bouton chacun (G, gros bouton, D), appuyer quand la note passe sur la ligne, combo, 40 secondes ; meme Berry que systemes/jeu-rythme.be, la note (bon) et le monde se choisissent
  const TIR_RYTHME = `# Rythme
var pts = 0 var combo = 0 var rates = 0 var reste = 40.0 var prochain = 0.5 var msg = "" var tmsg = 0.0 var fin = false
var NY = [] var NL = [] var NA = []                 # 8 notes : hauteur, couloir (0 G, 1 gros bouton, 2 D), active
var TOUCHE = [] var ROW = []                        # ROW = couleur de chaque couloir

def reset()
  pts = 0 combo = 0 rates = 0 reste = 40.0 prochain = 0.5 msg = "" tmsg = 0.0 fin = false
  for i: 0 .. 7 NA[i] = false end
end
def debut()
  TOUCHE = [G, P3, D]
  ROW = [A1, A2, A3]
  for i: 0 .. 7 NY.push(0.0) NL.push(0) NA.push(false) end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  reste -= dt tmsg -= dt
  if reste <= 0 fin = true score(pts) return end
  prochain -= dt
  if prochain <= 0
    prochain = 0.45
    for i: 0 .. 7 if !NA[i] NA[i] = true NY[i] = -10.0 NL[i] = hasard(0, 2) break end end
  end
  for i: 0 .. 7
    if NA[i]
      NY[i] += 150 * dt
      if NY[i] > 236 NA[i] = false combo = 0 rates += 1 msg = "Rate" tmsg = 0.5 end   # la note est passee
    end
  end
  for l: 0 .. 2
    if appui(TOUCHE[l])
      var meilleur = -1 var ecart = 24.0              # la note la plus proche de la ligne (y = 200), a 24 pixels au plus
      for i: 0 .. 7
        if NA[i] && NL[i] == l
          var d = NY[i] - 200 if d < 0 d = -d end
          if d < ecart ecart = d meilleur = i end
        end
      end
      if meilleur >= 0
        NA[meilleur] = false combo += 1 pts += 10 + combo * 2 score(pts) msg = ecart < 8 ? "Parfait !" : "Bien" tmsg = 0.5 son(500 + l * 250, 40)
      else combo = 0 msg = "Trop tot" tmsg = 0.5 son(150, 60) end
    end
  end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for l: 0 .. 3 rect(40 + l * 80 - 1, 0, 2, 240, PT2) end
  for l: 0 .. 2
    rect(40 + l * 80 + 4, 192, 62, 16, ROW[l])
    texte(68 + l * 80, 222, l == 0 ? "G" : (l == 1 ? "P3" : "D"), TXT, 2)
  end
  rect(40, 199, 240, 2, TXT)
  for i: 0 .. 7
    if NA[i] objet(75 + NL[i] * 80, int(NY[i]), 0) end
  end
  texte(4, 4, str(pts), TXT, 2) texte(4, 24, "x" + str(combo), TIRC, 2)
  texte(280, 4, str(int(reste)), TXT, 2)
  if tmsg > 0 texte(100, 100, msg, TIRC, 2) end
  if fin texte(60, 80, "FINI !", TIRC, 4) texte(40, 130, str(pts) + " points", TXT, 3) texte(70, 170, "P3 : rejouer", TXT, 2) end
end
`;

  // tetris : 7 pieces tombent dans un puits (G / D deplacent, P3 tourne, tenu = descend vite), lignes pleines effacees ; meme Berry que systemes/jeu-tetris.be, les couleurs des pieces viennent du monde
  const TIR_TETRIS = `# Tetris
var W = 10 var H = 20 var S = 11 var BX = 100 var BY = 10
var BASE = [[0, 1, 1, 1, 2, 1, 3, 1], [0, 0, 1, 0, 0, 1, 1, 1], [1, 0, 0, 1, 1, 1, 2, 1], [1, 0, 2, 0, 0, 1, 1, 1], [0, 0, 1, 0, 1, 1, 2, 1], [0, 0, 0, 1, 1, 1, 2, 1], [2, 0, 0, 1, 1, 1, 2, 1]]
var NB = [4, 2, 3, 3, 3, 3, 3]                      # taille de la boite de chaque piece (I, O, T, S, Z, J, L)
var PTSL = [0, 100, 300, 500, 800]
var SH = []                                          # SH[piece * 4 + rotation] = 4 cases (x, y) dans la boite, precalculees
var GRI = []                                         # la grille : 0 vide, sinon 1 + numero de piece
var pc = 0 var pr = 0 var qx = 3 var qy = 0 var nx = 0 var tg = 0.0
var pts = 0 var lignes = 0 var niv = 0 var fin = false var tm = 0.0 var ldir = 0 var rep = 0.0 var ph = 0.0
var PAL = [] var BORD = 0 var NOIR = 0

def libre(p, r, x, y)
  var c = SH[p * 4 + r] var k = 0
  while k < 4
    var cx = x + c[2 * k] var cy = y + c[2 * k + 1]
    if cx < 0 || cx >= W || cy >= H return false end
    if cy >= 0 && GRI[cy * W + cx] != 0 return false end
    k += 1
  end
  return true
end

def nouveau()
  pc = nx nx = hasard(0, 6) pr = 0 qx = 3 qy = 0 tg = 0.0
  if !libre(pc, pr, qx, qy) fin = true tm = 0.0 son(150, 300) end
end

def fige()                                           # pose la piece, efface les lignes pleines, compte les points
  var c = SH[pc * 4 + pr] var k = 0
  while k < 4
    var yy = qy + c[2 * k + 1]
    if yy >= 0 GRI[yy * W + qx + c[2 * k]] = pc + 1 end
    k += 1
  end
  var n = 0 var y = H - 1
  while y >= 0
    var plein = true var x = 0
    while x < W
      if GRI[y * W + x] == 0 plein = false break end
      x += 1
    end
    if plein
      n += 1
      var r = y
      while r > 0
        var x2 = 0
        while x2 < W GRI[r * W + x2] = GRI[(r - 1) * W + x2] x2 += 1 end
        r -= 1
      end
      var x3 = 0
      while x3 < W GRI[x3] = 0 x3 += 1 end
    else y -= 1 end
  end
  if n > 0
    pts += PTSL[n] * (niv + 1) lignes += n niv = lignes / 10 score(pts) son(n == 4 ? 1320 : 880, 120)
  else son(220, 30) end
end

def reset()
  var i = 0
  while i < W * H GRI[i] = 0 i += 1 end
  pts = 0 lignes = 0 niv = 0 fin = false ldir = 0 rep = 0.0 ph = 0.0
  nx = hasard(0, 6)
  nouveau()
  score(0)
end

def debut()
  var p = 0
  while p < 7
    var c = BASE[p] var r = 0
    while r < 4
      SH.push(c)
      var d = [0, 0, 0, 0, 0, 0, 0, 0] var k = 0
      while k < 4 d[2 * k] = NB[p] - 1 - c[2 * k + 1] d[2 * k + 1] = c[2 * k] k += 1 end   # un quart de tour : (x, y) -> (n - 1 - y, x)
      c = d r += 1
    end
    p += 1
  end
  var i = 0
  while i < W * H GRI.push(0) i += 1 end
  PAL = PALETTE
  BORD = PT2 NOIR = rgb(8, 10, 20)
  reset()
end

def bouger(d)
  if libre(pc, pr, qx + d, qy) qx += d son(330, 10) end
end

def tourner()                                        # un quart de tour ; si ca gene le mur ou le tas, on essaie de decaler un peu
  var r = (pr + 1) % 4
  var dx = [0, -1, 1, -2, 2] var k = 0
  while k < 5
    if libre(pc, r, qx + dx[k], qy) pr = r qx += dx[k] son(520, 15) return end
    k += 1
  end
end

def maj(dt)
  if fin
    tm += dt
    if tm > 0.7 && appui(P3) reset() end
    return
  end
  var dir = 0
  if bouton(G) && !bouton(D) dir = -1 elif bouton(D) && !bouton(G) dir = 1 end
  if dir != 0
    if dir != ldir bouger(dir) rep = 0.22
    else
      rep -= dt
      if rep <= 0 bouger(dir) rep = 0.07 end
    end
  end
  ldir = dir
  if appui(P3) tourner() end
  if bouton(P3) ph += dt else ph = 0.0 end
  var iv = 0.8 - niv * 0.07
  if iv < 0.08 iv = 0.08 end
  if ph > 0.2 && iv > 0.04 iv = 0.04 end
  tg += dt
  if tg >= iv
    tg = 0.0
    if libre(pc, pr, qx, qy + 1) qy += 1 else fige() nouveau() end
  end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  rect(BX - 3, BY - 3, W * S + 5, H * S + 5, BORD) rect(BX, BY, W * S - 1, H * S - 1, NOIR)
  var y = 0
  while y < H
    var x = 0
    while x < W
      var v = GRI[y * W + x]
      if v > 0 rect(BX + x * S, BY + y * S, S - 1, S - 1, PAL[v - 1]) end
      x += 1
    end
    y += 1
  end
  var c = SH[pc * 4 + pr] var k = 0
  while k < 4
    rect(BX + (qx + c[2 * k]) * S, BY + (qy + c[2 * k + 1]) * S, S - 1, S - 1, PAL[pc])
    k += 1
  end
  texte(8, 14, "SCORE", PT2, 1) texte(8, 26, str(pts), TXT, 2)
  texte(8, 56, "LIGNES", PT2, 1) texte(8, 68, str(lignes), TXT, 2)
  texte(8, 98, "NIVEAU", PT2, 1) texte(8, 110, str(niv + 1), TXT, 2)
  texte(4, 170, "G / D bouger", PT2, 1) texte(4, 184, "P3 tourner", PT2, 1) texte(4, 198, "P3 tenu : vite", PT2, 1)
  texte(226, 14, "SUIVANT", PT2, 1)
  var n = SH[nx * 4] k = 0
  while k < 4
    rect(236 + n[2 * k] * S, 34 + n[2 * k + 1] * S, S - 1, S - 1, PAL[nx])
    k += 1
  end
  if fin
    rect(BX, 80, W * S - 1, 70, NOIR)
    texte(BX + 20, 90, "PERDU", rgb(240, 80, 80), 3)
    if tm > 0.7 texte(BX + 9, 125, "P3 : rejouer", TXT, 1) end
  end
end
`;

  // labyrinthe facon Pac-Man : le heros avance tout seul, G tourne a gauche, D a droite, P3 demi-tour ; il mange toutes les pastilles, 2 fantomes (mal) le poursuivent, 3 vies ; meme Berry que systemes/jeu-labyrinthe.be, le heros (perso), les fantomes (mal) et le monde se choisissent
  const TIR_LABY = `# Labyrinthe
var pts = 0 var vies = 3 var fin = false var gagne = false var reste = 0 var want = -1
var MAP = ["#############", "#.....#.....#", "#.###.#.###.#", "#...........#", "#.##.###.##.#", "#...........#", "#.###.#.###.#", "#.....#.....#", "#############"]
var WL = [] var DT = []                              # 13 x 9 cases de 24 pixels : mur (1), pastille (1)
var EX = [] var EY = [] var ED = [] var EP = [] var EM = []   # 0 = le heros, 1 et 2 = les fantomes : case, direction (0 droite, 1 bas, 2 gauche, 3 haut), avancee vers la case suivante, en route
var DX = [1, 0, -1, 0] var DY = [0, 1, 0, -1]

def libre(x, y)
  return x >= 0 && x < 13 && y >= 0 && y < 9 && WL[y * 13 + x] == 0
end
def place()                                          # chacun retourne a son depart
  EX[0] = 6 EY[0] = 5 ED[0] = 0 EX[1] = 1 EY[1] = 1 ED[1] = 0 EX[2] = 11 EY[2] = 1 ED[2] = 2
  for i: 0 .. 2 EP[i] = 0.0 EM[i] = false end
  want = -1
end
def reset()
  pts = 0 vies = 3 fin = false gagne = false reste = 0
  for y: 0 .. 8
    for x: 0 .. 12
      var k = y * 13 + x
      WL[k] = MAP[y][x] == "#" ? 1 : 0
      DT[k] = MAP[y][x] == "." ? 1 : 0
      reste += DT[k]
    end
  end
  DT[5 * 13 + 6] = 0 reste -= 1
  place()
end
def debut()
  for i: 0 .. 116 WL.push(0) DT.push(0) end
  for i: 0 .. 2 EX.push(0) EY.push(0) ED.push(0) EP.push(0.0) EM.push(false) end
  reset()
end

def px(i) return (EX[i] + DX[ED[i]] * EP[i]) * 24 + 16 end
def py(i) return (EY[i] + DY[ED[i]] * EP[i]) * 24 + 34 end

def tourne0()                                        # le heros prend la direction demandee si elle est libre, sinon il continue tout droit (ou attend)
  if want >= 0 && libre(EX[0] + DX[want], EY[0] + DY[want]) ED[0] = want end
  want = -1
  EM[0] = libre(EX[0] + DX[ED[0]], EY[0] + DY[ED[0]])
end
def tourne(i)                                        # un fantome : pas de demi-tour sauf cul-de-sac ; vers le heros (le 1er, avec un peu de hasard) ou au hasard (le 2e)
  var best = -1 var bd = 9999
  for d: 0 .. 3
    if d != (ED[i] + 2) % 4 && libre(EX[i] + DX[d], EY[i] + DY[d])
      var a = EX[i] + DX[d] - EX[0] var b = EY[i] + DY[d] - EY[0]
      if a < 0 a = -a end
      if b < 0 b = -b end
      var s = a + b
      if i == 2 || hasard(0, 99) < 25 s = hasard(0, 50) end
      if s < bd bd = s best = d end
    end
  end
  if best < 0 best = (ED[i] + 2) % 4 end
  ED[i] = best EM[i] = libre(EX[i] + DX[best], EY[i] + DY[best])
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if appui(G) want = (ED[0] + 3) % 4 end
  if appui(D) want = (ED[0] + 1) % 4 end
  if appui(P3)                                       # demi-tour, meme en route
    if EM[0] EX[0] += DX[ED[0]] EY[0] += DY[ED[0]] EP[0] = 1.0 - EP[0] end
    ED[0] = (ED[0] + 2) % 4
    if !EM[0] tourne0() end
  end
  for i: 0 .. 2
    if EM[i]
      EP[i] += (i == 0 ? 5.5 : 4.2) * dt
      if EP[i] >= 1.0
        EX[i] += DX[ED[i]] EY[i] += DY[ED[i]] EP[i] = 0.0 EM[i] = false
        if i == 0
          var k = EY[0] * 13 + EX[0]
          if DT[k] == 1 DT[k] = 0 reste -= 1 pts += 10 score(pts) son(660, 15) end
          tourne0()
        else tourne(i) end
      end
    elif i == 0 tourne0()
    else tourne(i) end
  end
  for i: 1 .. 2
    var a = px(0) - px(i) var b = py(0) - py(i)
    if a < 0 a = -a end
    if b < 0 b = -b end
    if a < 14 && b < 14
      vies -= 1 son(150, 300)
      if vies <= 0 fin = true else place() end
      return
    end
  end
  if reste <= 0 fin = true gagne = true pts += 100 score(pts) son(988, 200) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  rect(4, 22, 312, 216, NO)
  for y: 0 .. 8
    for x: 0 .. 12
      var k = y * 13 + x
      if WL[k] == 1
        rect(4 + x * 24, 22 + y * 24, 24, 24, A2) rect(6 + x * 24, 24 + y * 24, 20, 20, A1)
      elif DT[k] == 1 rect(14 + x * 24, 32 + y * 24, 4, 4, TIRC) end
    end
  end
  perso(int(px(0)) - 12, int(py(0)) - 12)
  for i: 1 .. 2 objet(int(px(i)), int(py(i)), 1) end
  texte(6, 4, str(pts), TXT, 2) texte(240, 4, "vies " + str(vies), TXT, 2)
  if fin
    rect(40, 80, 240, 90, NO)
    if gagne texte(90, 90, "BRAVO !", TIRC, 4) else texte(80, 90, "PERDU !", TIRC, 4) end
    texte(100, 140, "P3 : rejouer", TXT, 2)
  end
end
`;

  // le monde choisi habille un tir fixe : dégradé du ciel, particules, ennemis = le mauvais objet du monde (objet(x, y), rayon 8) ; texte et tirs contrastent avec un ciel clair
  // perso = true (tir autour) : le personnage, les tirs (bon) et les ennemis (mal) se choisissent aussi (dessin du monde ou image) ; sinon look fixe
  // alignement : une plateforme glisse, P3 l'arrete pile sur la precedente (ce qui depasse est coupe) ; la tour (couleurs du monde) monte et ca accelere ; meme Berry que systemes/jeu-alignement.be
  const TIR_ALIGN = `# Alignement
var etage = 1 var bx = 0.0 var bw = 100.0 var sens = 1 var vit = 90.0 var fin = 0 var msg = "" var tmsg = 0.0
var PX = [] var PW = []                             # les etages deja poses : x et largeur

def reset()
  etage = 1 bx = 0.0 bw = 100.0 sens = 1 vit = 90.0 fin = 0 msg = "" tmsg = 0.0
  PX[0] = 110.0 PW[0] = 100.0                       # la base
end
def debut()
  for i: 0 .. 12 PX.push(0.0) PW.push(0.0) end
  reset()
end

def maj(dt)
  if fin > 0
    if appui(P3) reset() end
    return
  end
  tmsg -= dt
  bx += sens * vit * dt
  if bx < 0 bx = 0.0 sens = 1 elif bx + bw > 320 bx = 320.0 - bw sens = -1 end
  if appui(P3)
    var px = PX[etage - 1] var pw = PW[etage - 1]
    var g = bx > px ? bx : px var d = bx + bw < px + pw ? bx + bw : px + pw   # le bout commun
    if d - g <= 2 fin = 1 son(150, 400) return end                           # plus rien en commun : perdu
    if bx - px < 5 && px - bx < 5 g = px d = px + pw msg = "Parfait !" son(988, 80) else msg = "Bien" son(600, 40) end   # presque pile : on garde tout
    tmsg = 0.8 PX[etage] = g PW[etage] = d - g bw = d - g
    etage += 1 vit += 12 score(etage)
    if etage > 12 fin = 2 son(1200, 400) end
  end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  var cl = [A1, A2, A3, A4]
  for i: 0 .. etage - 1 rect(int(PX[i]), 224 - i * 16, int(PW[i]), 14, cl[i % 4]) end
  if fin == 0 rect(int(bx), 224 - etage * 16, int(bw), 14, TIRC) end
  texte(4, 4, "Etage " + str(etage - 1) + " / 12", TXT, 2)
  if tmsg > 0 texte(100, 30, msg, TIRC, 2) end
  if fin == 1 texte(70, 70, "TROP LOIN !", TIRC, 3) end
  if fin == 2 texte(70, 70, "TOUR FINIE !", TIRC, 3) end
  if fin > 0 texte(70, 110, "P3 : rejouer", TXT, 2) end
end
`;

  // plateforme qui casse : G / D courent, P3 saute, la tete casse une caisse par en dessous, l'objet (bon) apparait et se prend ; 5 caisses a decouvrir ; meme Berry que systemes/jeu-plateforme-casse.be
  const TIR_PCASSE = `# Plateforme casse
var px = 20.0 var py = 200.0 var vy = 0.0 var sol = true var pts = 0 var fin = false
var CX = [90, 150, 210, 40, 250] var CY = [150, 120, 150, 110, 110]   # 5 caisses : coin haut gauche
var CE = [0, 0, 0, 0, 0]                            # 0 caisse, 1 cassee (objet visible), 2 objet pris

def reset()
  px = 20.0 py = 200.0 vy = 0.0 sol = true pts = 0 fin = false
  for i: 0 .. 4 CE[i] = 0 end
end
def debut()
  reset()
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) px -= 110 * dt end
  if bouton(D) px += 110 * dt end
  if px < 0 px = 0.0 elif px > 296 px = 296.0 end
  if appui(P3) && sol vy = -330.0 sol = false son(660, 40) end
  vy += 900 * dt
  var ny = py + vy * dt
  sol = false
  if ny >= 200 ny = 200.0 vy = 0.0 sol = true end   # le sol
  for i: 0 .. 4
    if CE[i] == 0 && px + 24 > CX[i] && px < CX[i] + 24
      if vy < 0 && py - 24 >= CY[i] + 16 && ny - 24 < CY[i] + 16   # la tete touche le dessous : elle casse
        CE[i] = 1 vy = 60.0 ny = real(CY[i] + 40) son(300, 60)
      elif vy >= 0 && py <= CY[i] && ny >= CY[i] ny = real(CY[i]) vy = 0.0 sol = true end   # on peut marcher dessus
    elif CE[i] == 1 && px + 24 > CX[i] + 4 && px < CX[i] + 20 && ny > CY[i] && ny - 24 < CY[i] + 16
      CE[i] = 2 pts += 1 score(pts) son(988, 80)   # on prend l'objet decouvert
    end
  end
  py = ny
  var tout = true
  for i: 0 .. 4 if CE[i] != 2 tout = false end end
  if tout fin = true son(1200, 300) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  rect(0, 200, 320, 40, A2) rect(0, 200, 320, 4, A1)
  for i: 0 .. 4
    if CE[i] == 0 rect(CX[i], CY[i], 24, 16, A3) cadre(CX[i], CY[i], 24, 16, NO) texte(CX[i] + 8, CY[i] + 4, "?", TXT, 1)
    elif CE[i] == 1 objet(CX[i] + 12, CY[i] + 8 - int(temps() * 4) % 2, 0) end
  end
  perso(int(px), int(py) - 24)
  texte(6, 6, str(pts) + " / 5", TXT, 2)
  if fin texte(60, 90, "TOUT TROUVE !", TIRC, 3) texte(90, 130, "P3 : rejouer", TXT, 2) end
end
`;

  // plateforme qui rebondit : le personnage rebondit tout seul sur des plateformes, G / D le deplacent (l'ecran se rebouble), trampolines plus hauts, l'ecran monte avec lui, tomber = perdu ; meme Berry que systemes/jeu-plateforme-rebond.be
  const TIR_PREBOND = `# Plateforme qui rebondit
var px = 150.0 var py = 200.0 var vy = -420.0 var haut = 0 var fin = false
var PX = [] var PY = [] var PT = []                 # 8 plateformes espacees de 30 : PT = trampoline

def reset()
  px = 150.0 py = 200.0 vy = -420.0 haut = 0 fin = false
  for i: 0 .. 7 PX[i] = real(hasard(0, 260)) PY[i] = 230.0 - i * 30 PT[i] = false end
  PX[0] = 130.0
end
def debut()
  for i: 0 .. 7 PX.push(0.0) PY.push(0.0) PT.push(false) end
  reset()
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) px -= 140 * dt end
  if bouton(D) px += 140 * dt end
  if px < -24 px = 320.0 elif px > 320 px = -24.0 end   # sort a droite, revient a gauche
  vy += 900 * dt
  var ny = py + vy * dt
  if vy > 0
    for i: 0 .. 7
      if px + 24 > PX[i] && px < PX[i] + 60 && py <= PY[i] && ny >= PY[i]
        ny = PY[i] vy = PT[i] ? -620.0 : -420.0 son(PT[i] ? 880 : 600, 30)   # le trampoline renvoie plus haut
      end
    end
  end
  py = ny
  if py < 100                                       # l'ecran monte avec le personnage
    var d = 100 - py
    py = 100.0 haut += int(d)
    for i: 0 .. 7
      PY[i] += d
      if PY[i] > 240 PY[i] -= 240 PX[i] = real(hasard(0, 260)) PT[i] = hasard(0, 5) == 0 end
    end
    score(haut)
  end
  if py > 250 fin = true son(150, 300) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  for i: 0 .. 7
    var c = PT[i] ? TIRC : A2
    rect(int(PX[i]), int(PY[i]), 60, 8, c) rect(int(PX[i]), int(PY[i]) + 6, 60, 2, A1)
  end
  perso(int(px), int(py) - 24)
  texte(6, 6, str(haut), TXT, 2)
  if fin texte(80, 100, "PERDU !", TIRC, 4) texte(90, 150, "P3 : rejouer", TXT, 2) end
end
`;

  const tirMonde = (code, perso, nom = 'Tir ') => r => {
    const mo = MONDES[r.monde], im = r.imgs || {}, col = COULEURS[r.couleur], pale = mo.ciel[0].concat(mo.ciel[1]).reduce((t, v) => t + v, 0) / 6 > 130;
    const c = pale ? { pt1: [255, 255, 255], pt2: fonce(mo.ciel[0]), tir: [225, 80, 20], txt: [30, 30, 45] } : { pt1: [90, 90, 120], pt2: [220, 220, 250], tir: [255, 220, 60], txt: [255, 255, 255] };
    return code.replace(/^# .*\n/, '# ' + (nom + mo.nom).slice(0, 40) + '\n').replace('\nvar ', '\nvar NO = 0 var A1 = 0 var A2 = 0 var A3 = 0 var A4 = 0 var BAND = [] var PT1 = 0 var PT2 = 0 var TIRC = 0 var TXT = 0 var COMPO = 0 var TYPE = 0' + (perso ? ' var C1 = 0 var C2 = 0 var C3 = 0 var BL = 0' : '') + '\nvar ')
      .replace('\ndef debut()\n', (perso ? `\ndef objet(x, y, k)\n  if k == 0\n${objetCode(r, 'bon', mo, im)}\n  else\n${objetCode(r, 'mal', mo, im)}\n  end\nend\ndef perso(x, y)\n${r.perso === 'img' ? `  image(${im.perso}, x, y, 0, 1)` : PERSOS[r.perso]}\nend` : `\ndef objet(x, y)\n${mo.mal}\nend`) + `\ndef debut()\n  COMPO = ${COMPOS.indexOf(r.compo)} TYPE = ${TYPES_EN.indexOf(r.typeEn)}\n${perso ? `  C1 = rgb(${col.join(', ')}) C2 = rgb(${fonce(col).join(', ')}) C3 = rgb(${clair(col).join(', ')}) BL = rgb(255, 255, 255)\n` : ''}  NO = rgb(30, 30, 45) A1 = ${rgb(mo.a[0])} A2 = ${rgb(mo.a[1])} A3 = ${rgb(mo.a[2])} A4 = ${rgb(mo.a[3])}\n  BAND = ${liste(10, i => rgb(mel(mo.ciel[0], mo.ciel[1], i / 9)))}\n  PT1 = ${rgb(c.pt1)} PT2 = ${rgb(c.pt2)} TIRC = ${rgb(c.tir)} TXT = ${rgb(c.txt)}\n`);
  };

  const SYSTEMES = { quiz, soigne, attrape: chute, esquive: chute, pluie: chute, 'tir-haut': chute, 'tir-perso': tirMonde(TIR_PERSO, true), 'tir-autour': tirMonde(TIR_AUTOUR, true), 'tir-vagues': tirMonde(TIR_VAGUES, true, 'Vagues '), 'casse-briques': tirMonde(TIR_CASSE, true, 'Casse '), pong: tirMonde(TIR_PONG, true, 'Pong '), flappy: tirMonde(TIR_FLAPPY, true, 'Flappy '), rythme: tirMonde(TIR_RYTHME, true, 'Rythme '), alignement: tirMonde(TIR_ALIGN, false, 'Alignement '), 'plateforme-casse': tirMonde(TIR_PCASSE, true, 'Caisses '), 'plateforme-rebond': tirMonde(TIR_PREBOND, true, 'Rebond '), tetris: r => tirMonde(TIR_TETRIS, false, 'Tetris ')(r).replace('PALETTE', '[' + [...MONDES[r.monde].a, clair(MONDES[r.monde].a[0]), clair(MONDES[r.monde].a[2]), [255, 255, 255]].map(rgb).join(', ') + ']'), labyrinthe: tirMonde(TIR_LABY, true, 'Labyrinthe '), 'plat-but': plateforme, 'plat-attrape': plateforme, 'plat-evite': plateforme, 'plat-course': plateforme, cote: plateforme, toits: plateforme, lianes, derriere: course, dessus: course, iso: course };   // jeu -> son système

  function recetteComplete(r) {   // une étape sans choix (ou un choix inconnu) prend sa valeur par défaut : le jeu marche toujours
    const o = {};
    r = r || {}; const imgs = r.imgs || {}; o.imgs = imgs; if (r.arbres > 0) o.arbres = r.arbres; if (r.edit) o.edit = r.edit; if (Array.isArray(r.quest) && r.quest.length) o.quest = r.quest;   // quest : les questions du quiz écrites par l'IA   // edit : niveau 1 de la plateforme placé à la main (voir plateforme)
    for (const e of ETAPES.filter(e => !e.quiz)) o[e.id] = (e.id === 'niv' ? nbNiv(r.niv) <= NIV_MAX && r.niv : e.bulles.some(b => b.id === r[e.id])) || (e.ia && r[e.id] === 'img' && imgs[e.id] > 0) ? r[e.id] : DEFAUT[e.id];
    if (!JEUX_DE[r.genre]) o.genre = GENRE_DE[o.jeu];                                    // pas de genre : celui du jeu (anciennes recettes)
    else if (GENRE_DE[o.jeu] !== o.genre) o.jeu = SOLO[o.genre] ? o.genre : JEUX_DE[o.genre][0];   // un jeu d'un autre genre : le premier du genre choisi
    const nn = nbNiv(o.niv); if (/^plat-/.test(o.jeu) && nn > 1) { const m = liste0(nn, k => k > 0 && o['monde' + (k + 1)] !== 'meme' ? o['monde' + (k + 1)] : o.monde); const vus = []; m.forEach((w, k) => { if (!vus.includes(w) && vus.length >= MONDES_MAX) m[k] = o.monde; else if (!vus.includes(w)) vus.push(w); }); if (m.some(x => x !== o.monde)) o.mondes = m; }   // un monde par niveau (seulement s'il y en a un de différent) ; 2 mondes différents au plus (décor + avant-plan de chacun : ~3 Ko de mémoire de console chacun ; à 3 mondes on dépasse le plafond de 72000 du test) : un autre monde reprend celui du niveau 1
    if (o.jeu === 'lianes') o.niv = 'un';   // un seul niveau : pas de niveaux ni de mondes en plus
    if (o.jeu === 'tir-haut') o.action = 'tir';   // « tirer vers le haut » : le gros bouton tire toujours
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
  // « Message du jeu » : le cartel de victoire et le mot du saut (le saut note l'heure dans MSJ, le dessin affiche le mot 0,7 s en le faisant monter)
  const messages = (code, c) => {
    const g = CARTELS_GAGNE[c.gagne]; if (g && c.gagne !== 'bravo') code = code.replace(/texte\(92, 90, "BRAVO !", BL, 3\)/g, `texte(${160 - 9 * g[0].length}, 90, "${g[0]}", ${rgb(g[1])}, 3)`);
    const m = MOTS_SAUTE[c.saute], ancre = /parle\(int\(PX\), int\(PY\), (?:0|CAMX)\) end/;
    if (!m || !/sol = false son\(660, 40\)/.test(code) || !/var sol = true/.test(code) || !ancre.test(code)) return code;
    return code.replace('var sol = true', 'var sol = true var MSJ = -9.0').replace(/sol = false son\(660, 40\)/g, 'sol = false son(660, 40) MSJ = temps()')
      .replace(ancre, a => `${a}
  if temps() - MSJ < 0.7 texte(int(PX) + 13 - ${6 * m.length}, int(PY) - 14 - int((temps() - MSJ) * 30), "${m}", NO, 2) texte(int(PX) + 12 - ${6 * m.length}, int(PY) - 15 - int((temps() - MSJ) * 30), "${m}", BL, 2) end`);
  };
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
  const PAROLES = ['Change mon decor !', "Et si j'etais bleu ?", 'Invente ton jeu !', 'Bzzz ! Attrape-moi !'];
  const parole = (p, i) => { const s = typeof p?.[i] === 'string' ? p[i].replace(/[^A-Za-z0-9 !?.,'-]/g, '').trim().slice(0, 26) : ''; return s.length > 2 ? s : PAROLES[i]; };   // phrases de l'IA (r.paroles), sinon celles de base ; que des caracteres sans risque dans le code Berry
  // simple = 4 niveaux ou plus : la memoire de la console est presque pleine, donc 3 phrases et pas de petit cube volant
  const PAROLE = (p, simple) => `def parle(x, y, cx)
  var t = temps() var k = int(t / 4) var u = t - k * 4 var s = ${JSON.stringify([0, 1, 2, 3].slice(0, simple ? 3 : 4).map(i => parole(p, i)))}[k % ${simple ? 3 : 4}] var w = size(s) * 6 + 8 var b = x + 12 - w / 2${simple ? '' : ' var c = BL var d = NO'}
  if u > 2.5 return end
${simple ? '' : `  if k % 4 == 3
    x = int(cx + 30 + u * 60) y = 70 + int(t * 6) % 2 * 3 b = x - 20 c = NO d = BL
    rect(x, y, 14, 14, rgb(255, int(t * 200) % 255, 255 - int(t * 90) % 255)) rect(x + 3, y + 4, 3, 3, BL) rect(x + 9, y + 4, 3, 3, BL)
  end
`}  if b < cx + 2 b = cx + 2 elif b + w > cx + 318 b = cx + 318 - w end
  rect(b, y - 24, w, 14, ${simple ? 'BL' : 'c'}) tri(x + 8, y - 10, x + 16, y - 10, x + 12, y - 5, ${simple ? 'BL' : 'c'}) texte(b + 4, y - 21, s, ${simple ? 'NO' : 'd'}, 1)
end
`;
  function assembler(r) {
    const c = recetteComplete(r), f = SYSTEMES[c.jeu], nn = SOLO[c.jeu] || FIXES[c.jeu] ? 1 : nbNiv(c.niv);
    let code = FIXES[c.jeu] ? f(c) : messages(cartel(f(c), c), c); if (nn > 1) code = niveaux(code, nn, f === chute, f === plateforme && /^plat-/.test(c.jeu));
    if (nn > 6) code = code.replace(/ parle\(.*?, (0|CAMX)\)/g, '');   // (6 niveaux au plus : jamais atteint)
    if (/ parle\(/.test(code)) code = code.replace(/\ndef perso\(/, '\n' + PAROLE(r && r.paroles, nn > 3) + '\ndef perso(');   // pas de bulle dans les questions ni le personnage a soigner : leur ecran est deja plein
    return MULTI.has(f) ? multi(code, MULTI.get(f), !!COURSE[c.jeu]) : code;
  }
  const AIDE = { saut: 'il saute', tir: 'il tire', bouclier: 'il se protege avec un bouclier', turbo: 'il va plus vite', aimant: 'il attire les bons objets' };
const AIDE_JEU = { 'plat-but': 'G et D pour courir, le gros bouton : il saute. Arrive au drapeau !', 'plat-attrape': 'G et D pour courir, le gros bouton : il saute. Attrape toutes les etoiles !', 'plat-evite': 'G et D pour courir, le gros bouton : il saute. Evite les monstres et arrive au drapeau !', 'plat-course': 'G et D pour courir, le gros bouton : il saute. Arrive au drapeau avant le rival !',
  cote: 'Il court tout seul : D va plus vite, G ralentit, le gros bouton : turbo. Arrive avant le rival !', 'cote-obst': 'Il court tout seul : D va plus vite, G ralentit, le gros bouton : il saute. Saute les obstacles et arrive avant le rival !', derriere: 'G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !', dessus: 'G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !', iso: 'G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !' };
const AIDE_OBST = { derriere: ' Evite les mauvais objets.', dessus: ' Evite les mauvais objets.', iso: ' Evite les mauvais objets.' };
AIDE_JEU['tir-perso'] = 'G monte, D descend, le gros bouton : tir. Detruis les ennemis qui arrivent, 3 vies !'; AIDE_JEU['plateforme-casse'] = 'G et D courent, le gros bouton saute. Saute sous une caisse pour la casser et prends ce qu elle cachait. Trouve les 5 !'; AIDE_JEU['plateforme-rebond'] = 'Tu rebondis tout seul : G et D te deplacent, tu ressors par le cote. Les plateformes orange sont des trampolines. Monte le plus haut possible, ne tombe pas !'; AIDE_JEU.alignement = 'Une plateforme glisse de gauche a droite : appuie sur le gros bouton pour l arreter pile sur la precedente. Ce qui depasse est coupe, la tour monte et ca accelere. 12 etages pour gagner !'; AIDE_JEU.tetris = 'Les blocs tombent : G et D les deplacent, le gros bouton les tourne (tenu : ils descendent vite). Remplis des lignes entieres, elles disparaissent !'; AIDE_JEU.labyrinthe = 'Tu avances tout seul : G tourne a gauche, D tourne a droite, le gros bouton fait demi-tour. Mange toutes les pastilles et evite les fantomes, 3 vies !'; AIDE_JEU.rythme = 'Les notes tombent dans 3 couloirs : G, le gros bouton et D. Appuie quand une note passe sur la ligne, enchaine pour faire un combo, tu as 40 secondes !'; AIDE_JEU.pong = 'G monte ta raquette, D la descend. La console joue en face : renvoie la balle, le premier a 5 points gagne !'; AIDE_JEU.flappy = 'Le gros bouton bat des ailes, sinon tu tombes. Passe dans le trou de chaque tuyau, ca va de plus en plus vite !'; AIDE_JEU['casse-briques'] = 'G et D bougent la raquette, le gros bouton lance la balle. Casse toutes les briques, elle va de plus en plus vite, 3 vies !'; AIDE_JEU['tir-vagues'] = 'G et D pour bouger, le gros bouton : tir (un seul a la fois). Detruis toute la vague avant qu elle arrive en bas, evite les bombes, 3 vies !'; AIDE_JEU['tir-autour'] = 'G et D tournent, le gros bouton : tir dans la direction du vaisseau. Detruis tout ce qui arrive de partout, 3 vies !';
AIDE_JEU.toits = 'Il court tout seul : D va plus vite, G ralentit, le gros bouton : il saute. Rappuie en l air : il fait sa figure, 3 points si tu retombes apres. Saute les immeubles jusqu au drapeau !'; AIDE_JEU.lianes = 'Tu te balances tout seul : G monte sur la liane, D descend, le gros bouton : tu la laches. Rattrape la liane suivante en l air, ramasse les etoiles et arrive sur la plateforme. Attention aux pointes, 3 vies !'; AIDE_JEU.quiz ='Lis la question, puis choisis ta reponse : G (la 1re), le gros bouton (la 2e) ou D (la 3e). 3 vies !'; AIDE_JEU.soigne = 'G : il mange, le gros bouton : il joue, D : il dort. Garde ses 3 jauges pleines pendant 45 secondes !';
M.aide = r => { const c = recetteComplete(r); if (SOLO[c.jeu] || FIXES[c.jeu]) return AIDE_JEU[c.jeu]; if (NOUVEAUX[c.jeu]) return AIDE_JEU[c.jeu === 'cote' && c.obst === 'avec' ? 'cote-obst' : c.jeu] + (c.obst === 'avec' ? AIDE_OBST[c.jeu] || '' : ''); const a = c.action; return 'G et D pour bouger, le gros bouton : ' + AIDE[a] + '.'; };
M.EN = {"Placer à la main":"Place things by hand","🧊 Je roule en 3D en diagonale":"🧊 I ride in diagonal 3D","Mon véhicule":"My vehicle","Sur quoi roules-tu ?":"What are you riding?","🚗 En voiture":"🚗 In a car","🏍 En moto de motocross":"🏍 On a motocross bike","🏄 Sur une planche de surf":"🏄 On a surfboard","Modifier ma présentation":"Change my game’s look","Que veux-tu changer ?":"What do you want to change?","Quand je perds":"When I lose","Message du jeu":"Game message","Quand je gagne":"When I win","Que dit le cartel quand tu gagnes ?":"What does the sign say when you win?","👏 BRAVO !":"👏 WELL DONE!","🏆 GAGNÉ !":"🏆 YOU WON!","🥳 YOUPI !":"🥳 YAY!","🥇 CHAMPION":"🥇 CHAMPION","😎 SUPER !":"😎 SUPER!","🤩 WOUHOU !":"🤩 WOOHOO!","Quand je saute":"When I jump","Que dit ton personnage quand il saute ?":"What does your character say when it jumps?","🤐 Rien":"🤐 Nothing","🦘 HOP !":"🦘 HOP!","🌀 BOING !":"🌀 BOING!","🤩 WOUHOU":"🤩 WOOHOO","🤠 HI HA !":"🤠 YEE HAW!","Que dit le cartel quand tu perds ?":"What does the sign say when you lose?","😵 PERDU !":"😵 YOU LOST!","😬 OUPS !":"😬 OOPS!","💀 GAME OVER":"💀 GAME OVER","😸 ENCORE !":"😸 AGAIN!","🔥 RATÉ !":"🔥 MISSED!","🤖 ERREUR":"🤖 ERROR","Les niveaux":"The levels","Combien de niveaux ?":"How many levels?","1️⃣ Un seul niveau":"1️⃣ A single level","🪜 3 niveaux":"🪜 3 levels","🏔 5 niveaux":"🏔 5 levels","❓ Je réponds à des questions":"❓ I answer questions","🐣 Je m’occupe d’un personnage":"🐣 I take care of a character","Lis la question, puis choisis ta reponse : G (la 1re), le gros bouton (la 2e) ou D (la 3e). 3 vies !":"Read the question, then pick your answer: L (1st), big button (2nd) or R (3rd). 3 lives!","G : il mange, le gros bouton : il joue, D : il dort. Garde ses 3 jauges pleines pendant 45 secondes !":"L: it eats, big button: it plays, R: it sleeps. Keep its 3 gauges full for 45 seconds!","🏁 Plateforme : arriver au drapeau":"🏁 Platformer: reach the flag","⭐ Plateforme : tout attraper":"⭐ Platformer: catch everything","👾 Plateforme : éviter les monstres":"👾 Platformer: dodge the monsters","⏱ Plateforme : course":"⏱ Platformer: race","🏃 Course de côté":"🏃 Side-view race","🚗 Course vue de derrière":"🚗 Race seen from behind","🛰 Course vue de dessus":"🛰 Race seen from above","Les obstacles":"The obstacles","Des obstacles sur la route ?":"Any obstacles on the road?","🚧 Avec obstacles":"🚧 With obstacles","🛣 Sans obstacles":"🛣 No obstacles","G et D pour courir, le gros bouton : il saute. Arrive au drapeau !":"L and R to run, big button: it jumps. Reach the flag!","G et D pour courir, le gros bouton : il saute. Attrape toutes les etoiles !":"L and R to run, big button: it jumps. Catch all the stars!","G et D pour courir, le gros bouton : il saute. Evite les monstres et arrive au drapeau !":"L and R to run, big button: it jumps. Dodge the monsters and reach the flag!","G et D pour courir, le gros bouton : il saute. Arrive au drapeau avant le rival !":"L and R to run, big button: it jumps. Reach the flag before the rival!","Il court tout seul : D va plus vite, G ralentit, le gros bouton : il saute. Saute les obstacles et arrive avant le rival !":"It runs by itself: R goes faster, L slows down, big button: it jumps. Jump over the obstacles and get there before the rival!","Il court tout seul : D va plus vite, G ralentit, le gros bouton : turbo. Arrive avant le rival !":"It runs by itself: R goes faster, L slows down, big button: turbo. Get there before the rival!","G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival !":"L and R to change lane, big button: turbo. Pick up the good things and get there before the rival!","G et D pour changer de voie, le gros bouton : turbo. Ramasse les bons objets et arrive avant le rival ! Evite les mauvais objets.":"L and R to change lane, big button: turbo. Pick up the good things and get there before the rival! Avoid the bad things.","Le jeu":"The game","Quel jeu veux-tu ?":"Which game?","🍎 Attraper":"🍎 Catch","💨 Esquiver":"💨 Dodge","🌧 Ne rien rater":"🌧 Miss nothing","Mon personnage":"My character","Qui es-tu ?":"Who are you?","🐱 Un chat":"🐱 A cat","🤖 Un robot":"🤖 A robot","🐉 Un dragon":"🐉 A dragon","🚀 Une fusée":"🚀 A rocket","👻 Un fantôme":"👻 A ghost","🧙 Un magicien":"🧙 A wizard","🐸 Une grenouille":"🐸 A frog","🦉 Un hibou":"🦉 An owl","Sa couleur":"Its colour","De quelle couleur ?":"Which colour?","🔴 Rouge":"🔴 Red","🟠 Orange":"🟠 Orange","🟡 Jaune":"🟡 Yellow","🟢 Vert":"🟢 Green","🔵 Bleu":"🔵 Blue","🟣 Violet":"🟣 Purple","🩷 Rose":"🩷 Pink","⚪ Blanc":"⚪ White","Le monde":"The world","Où es-tu ?":"Where are you?","🌳 Une forêt":"🌳 A forest","🏙 La ville la nuit":"🏙 The city at night","🌌 L’espace":"🌌 Space","🌊 Sous l’eau":"🌊 Underwater","🏜 Un désert":"🏜 A desert","❄️ La neige":"❄️ Snow","Ce que j’attrape":"What I catch","Que veux-tu attraper ?":"What do you want to catch?","🎁 Ceux du monde":"🎁 The ones from this world","Ce que j’évite":"What I avoid","Que veux-tu éviter ?":"What do you want to avoid?","💣 Ceux du monde":"💣 The ones from this world","Le gros bouton":"The big button","Que fait le gros bouton ?":"What does the big button do?","🦘 Sauter":"🦘 Jump","💥 Tirer":"💥 Shoot","🛡 Bouclier":"🛡 Shield","💨 Turbo":"💨 Turbo","🧲 Aimant":"🧲 Magnet","G et D pour bouger, le gros bouton : il saute.":"L and R to move, big button: it jumps.","G et D pour bouger, le gros bouton : il tire.":"L and R to move, big button: it shoots.","G et D pour bouger, le gros bouton : il se protege avec un bouclier.":"L and R to move, big button: it protects itself with a shield.","G et D pour bouger, le gros bouton : il va plus vite.":"L and R to move, big button: it goes faster.","G et D pour bouger, le gros bouton : il attire les bons objets.":"L and R to move, big button: it attracts the good things."};   // la page anglaise (lang="en") traduit les bulles avec ce dictionnaire
Object.assign(M.EN, { "Un niveau de plus ?": "One more level?", "🦘 Je saute": "🦘 I jump", "💨 J’avance": "💨 I move forward", "🧺 J’attrape": "🧺 I catch", "💥 Je tire": "💥 I shoot", "🧠 J’apprends": "🧠 I learn", "🌱 Je fais grandir": "🌱 I make it grow", "Tu veux aller plus loin ou tester ton jeu ?": "Go further, or test your game?", "Aller plus loin": "Go further", "Tester mon jeu": "Test my game", "Changer un choix": "Change a choice", "Ton jeu est déjà prêt : change quelque chose avant de le renvoyer.": "Your game is ready: change something before sending it again." });
Object.assign(M.EN, { "🏙 Je saute par-dessus les immeubles et je fais des figures": "🏙 I jump over the buildings and do tricks", "Ma figure": "My trick", "Quelle figure fais-tu en l’air ?": "Which trick do you do in the air?", "🤸 Un salto": "🤸 A flip", "🌀 Une vrille": "🌀 A spin", "⭐ Une étoile": "⭐ A star jump", "Il court tout seul : D va plus vite, G ralentit, le gros bouton : il saute. Rappuie en l air : il fait sa figure, 3 points si tu retombes apres. Saute les immeubles jusqu au drapeau !": "It runs by itself: R goes faster, L slows down, big button: it jumps. Press again in the air: it does its trick, 3 points if you land after it. Jump over the buildings to the flag!" });
Object.assign(M.EN, { "🌿 Je me balance de liane en liane": "🌿 I swing from vine to vine", "Tu te balances tout seul : G monte sur la liane, D descend, le gros bouton : tu la laches. Rattrape la liane suivante en l air, ramasse les etoiles et arrive sur la plateforme. Attention aux pointes, 3 vies !": "You swing by yourself: L climbs up the vine, R goes down, big button: you let go. Catch the next vine in the air, collect the stars and reach the platform. Watch out for the spikes, 3 lives!" });
for (let n = 1; n <= NIV_MAX; n++) { M.EN['➕ Ajouter un niveau (ça fera ' + (n + 1) + ')'] = '➕ Add a level (that makes ' + (n + 1) + ')'; M.EN['✋ Rester à ' + n + (n > 1 ? ' niveaux' : ' niveau')] = '✋ Stay at ' + n + (n > 1 ? ' levels' : ' level'); }
Object.assign(M.EN, { "Les plateformes": "The platforms", "Difficulté : les plateformes des niveaux ?": "Difficulty: the platforms in each level?", "🧱 Les mêmes à chaque niveau": "🧱 The same in every level", "🗺 À des endroits différents à chaque niveau": "🗺 In different places in every level", "Les pièges": "The traps", "Difficulté : et les pièges ?": "Difficulty: and the traps?", "⚖️ Autant de pièges à chaque niveau": "⚖️ The same number of traps", "👾 De plus en plus de pièges": "👾 More and more traps" });
Object.assign(M.EN, { "⬆️ Je tire vers le haut": "⬆️ I shoot upwards", "➡️ Je tire vers la droite": "➡️ I shoot to the right", "🎯 Je tire autour de moi": "🎯 I shoot all around me", "👾 Je défends contre des vagues": "👾 I defend against waves", "🧱 Je casse": "🧱 I break things", "🏓 Je rebondis": "🏓 I bounce", "⚡ Je réagis": "⚡ I react", "🎸 J’appuie en rythme quand les notes tombent": "🎸 I press in rhythm as the notes fall", "🧩 J’empile": "🧩 I stack", "👻 Je me faufile": "👻 I sneak around", "🧩 J’empile des blocs pour faire des lignes": "🧩 I stack blocks to make lines", "🏗 J’arrête la plateforme pile sur l’autre": "🏗 I stop the platform right on top of the other", "📦 Je saute et je casse des caisses": "📦 I jump and break crates", "🦘 Je rebondis de plateforme en plateforme": "🦘 I bounce from platform to platform", "Ce que cachent les caisses": "What the crates hide", "Que cachent les caisses ?": "What do the crates hide?", "👻 Je mange tout dans un labyrinthe et j’évite les fantômes": "👻 I eat everything in a maze and dodge the ghosts", "Les blocs tombent : G et D les deplacent, le gros bouton les tourne (tenu : ils descendent vite). Remplis des lignes entieres, elles disparaissent !": "Blocks fall: L and R move them, big button turns them (hold: they drop fast). Fill whole lines, they disappear!", "Tu avances tout seul : G tourne a gauche, D tourne a droite, le gros bouton fait demi-tour. Mange toutes les pastilles et evite les fantomes, 3 vies !": "You move by yourself: L turns left, R turns right, big button turns around. Eat all the dots and dodge the ghosts, 3 lives!", "🏓 Je joue au pong contre la console": "🏓 I play pong against the console", "🐦 Je bats des ailes entre les tuyaux": "🐦 I flap between the pipes", "🧱 Je casse un mur de briques avec une balle": "🧱 I break a brick wall with a ball" });
Object.assign(M.EN, { "Le genre": "The genre", "Quel jeu ?": "Which game?", "🏃 Je saute de plateforme en plateforme": "🏃 I jump from platform to platform", "💨 Je cours tout seul et j’évite les obstacles": "💨 I run on my own and dodge obstacles", "🛸 Je reste en bas, tout vient vers moi": "🛸 I stay at the bottom, everything comes at me", "🧺 J’attrape ce qui tombe": "🧺 I catch what falls" });
// le niveau 1 de la plateforme tel que le jeu le fait (point de départ de « Placer à la main ») ; null si ce jeu n'est pas une plateforme
M.platDefaut = r => {
  const c = recetteComplete(r), P = PLATEFORMES[c.jeu]; if (!P) return null;
  const pl = (c.plan === 'oui' ? PLANS[1] : PLATS).map(p => p.slice()), hz = [...P.hz, ...PIEGES_PLUS].slice(0, P.hz.length).map(x => c.plan === 'oui' ? x + DEC[0] : x);
  return { pl, st: [...pl.map(p => [p[0] + p[2] / 2, p[1] - 16]), [70, 192]], hz: hz.map(x => [x, 200]), long: 1000, drapeau: 940, bouge: !!P.vh };
};
Object.assign(M.EN, {"🐦 Je joue un oiseau": "🐦 I play a bird", "🟡 Je joue une bestiole": "🟡 I play a critter", "🍎 Pomme": "🍎 Apple", "🪙 Pièce": "🪙 Coin", "⭐ Étoile": "⭐ Star", "🫧 Perle": "🫧 Pearl", "🐟 Poisson": "🐟 Fish", "💎 Gemme": "💎 Gem", "❄️ Flocon": "❄️ Snowflake", "🍄 Champignon": "🍄 Mushroom", "🚗 Voiture": "🚗 Car", "☄️ Météorite": "☄️ Meteorite", "🪼 Méduse": "🪼 Jellyfish", "🌵 Cactus": "🌵 Cactus", "🧊 Glaçon pointu": "🧊 Sharp icicle"});   // formes du bestiaire (bon, mal) et personnages en plus
for (let k = 2; k <= NIV_MAX; k++) Object.assign(M.EN, { ['Monde du niveau ' + k]: 'World of level ' + k, ['Niveau ' + k + ' : où es-tu ?']: 'Level ' + k + ': where are you?' }); M.EN['↩️ Comme le niveau 1'] = '↩️ Like level 1';
M.mondeBulles = (id, r) => {   // les bulles de « monde du niveau k » : si le jeu a déjà 2 mondes différents (niveau 1 + un autre niveau), seuls ces 2 restent offerts
  const nn = nbNiv(r.niv), pris = new Set([r.monde || DEFAUT.monde]); for (let k = 2; k <= nn; k++) { const w = r['monde' + k]; if (k !== +id.slice(5) && w && w !== 'meme') pris.add(w); }
  const tous = ETAPES.find(e => e.id === id).bulles; return pris.size >= MONDES_MAX ? tous.filter(b => b.id === 'meme' || pris.has(b.id)) : tous;
};
const LIB_TIR = { bon: { nom: 'Mes tirs', q: 'Avec quoi tires-tu ?' }, mal: { nom: 'Mes ennemis', q: 'Que veux-tu détruire ?' } };   // « tir autour » : « ce que j’attrape / évite » devient « mes tirs / mes ennemis »
Object.assign(M.EN, { "Mes tirs": "My shots", "Avec quoi tires-tu ?": "What do you shoot with?", "Mes ennemis": "My enemies", "Que veux-tu détruire ?": "What do you want to destroy?" });
Object.assign(M.EN, { "Leur façon de bouger": "How they move", "Comment bougent tes ennemis ?": "How do your enemies move?", "➡️ Tout droit": "➡️ Straight ahead", "〰️ En zigzag": "〰️ Zigzag", "🎯 Ils me poursuivent": "🎯 They chase me", "⚡ Ils accélèrent en approchant": "⚡ They speed up as they come", "Leurs types": "Their types", "Quels ennemis veux-tu ?": "Which enemies do you want?", "👾 Tous pareils": "👾 All the same", "💪 Des costauds : 3 tirs pour les détruire": "💪 Tough ones: 3 shots to destroy them", "💨 Des rapides": "💨 Fast ones", "🎲 Un peu de tout": "🎲 A bit of everything", "Aller plus loin": "Go further" });   // tirs : comment bougent les ennemis, leurs types
const LIB_CAISSE = { bon: { nom: 'Ce que cachent les caisses', q: 'Que cachent les caisses ?' } }, LIB_CASSE = { bon: { nom: 'Ma balle', q: 'Quelle balle veux-tu ?' } }, LIB_NOTE = { bon: { nom: 'Mes notes', q: 'Quelles notes tombent ?' } };
M.lib = (a, r, k) => (r && CHOIX[r.jeu]?.includes('bon') && (r.jeu === 'rythme' ? LIB_NOTE : r.jeu === 'plateforme-casse' ? LIB_CAISSE : LIB_CASSE)[a.id]?.[k]) || (r && FIXES[r.jeu] && !CHOIX[r.jeu] && LIB_TIR[a.id]?.[k]) || a[k];   // k = 'q' ou 'nom' : le texte de l'étape pour ce jeu
M.ARBRES = AR.ARBRES; M.ETAPES = ETAPES; M.GROUPES = { msg: { icone: '💬', nom: 'Message du jeu' } }; M.genreDe = jeu => ETAPES[0].bulles.find(b => b.id === GENRE_DE[jeu]); M.saute = saute; M.nivBulles = nivBulles; M.mot = mot; M.nbNiv = nbNiv; M.DEFAUT = DEFAUT; M.recetteComplete = recetteComplete; M.assembler = assembler;
})(typeof module === 'object' ? module.exports : (window.JeuxBulles = {}));
