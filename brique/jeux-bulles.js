// Jeux « à bulles » (tranche 6-9 ans) : l'enfant touche des bulles, la page assemble un programme Berry à partir de PIÈCES déjà testées (jeu, personnage, couleur, monde, action).
// Pas d'IA pour le code : chaque pièce est du Berry écrit et joué dans verif (node test-bulles.js), donc toute combinaison marche du premier coup.
// Même fichier pour la page (window.JeuxBulles) et le serveur / les tests (require). Ajouter un SYSTÈME de jeu = une entrée dans SYSTEMES + ses bulles dans JEUX.
(function (M) {
  const rgb = c => `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
  const mel = (a, b, k) => a.map((v, i) => Math.round(v + (b[i] - v) * k));
  const fonce = c => c.map(v => Math.round(v * 0.6)), clair = c => c.map(v => Math.min(255, Math.round(v * 1.2 + 45)));
  const liste = (n, f) => '[' + Array.from({ length: n }, (_, i) => f(i)).join(', ') + ']';

  // ---------- les bulles (étape -> choix) ; id = ce que le générateur comprend, bouton = ce que l'enfant voit ----------
  const ETAPES = [
    { id: 'jeu', h: 210, icone: '🎮', nom: 'Le jeu', q: 'Quel jeu veux-tu ?', bulles: [
      { id: 'attrape', bouton: '🍎 Attraper' }, { id: 'esquive', bouton: '💨 Esquiver' }, { id: 'pluie', bouton: '🌧 Ne rien rater' }] },
    { id: 'perso', h: 28, icone: '🐱', nom: 'Mon personnage', q: 'Qui es-tu ?', ia: true, bulles: [
      { id: 'chat', bouton: '🐱 Un chat' }, { id: 'robot', bouton: '🤖 Un robot' }, { id: 'dragon', bouton: '🐉 Un dragon' }, { id: 'fusee', bouton: '🚀 Une fusée' },
      { id: 'fantome', bouton: '👻 Un fantôme' }, { id: 'magicien', bouton: '🧙 Un magicien' }, { id: 'grenouille', bouton: '🐸 Une grenouille' }, { id: 'hibou', bouton: '🦉 Un hibou' }] },
    { id: 'couleur', h: 330, icone: '🎨', nom: 'Sa couleur', q: 'De quelle couleur ?', bulles: [
      { id: 'rouge', bouton: '🔴 Rouge' }, { id: 'orange', bouton: '🟠 Orange' }, { id: 'jaune', bouton: '🟡 Jaune' }, { id: 'vert', bouton: '🟢 Vert' },
      { id: 'bleu', bouton: '🔵 Bleu' }, { id: 'violet', bouton: '🟣 Violet' }, { id: 'rose', bouton: '🩷 Rose' }, { id: 'blanc', bouton: '⚪ Blanc' }] },
    { id: 'monde', h: 150, icone: '🌍', nom: 'Le monde', q: 'Où es-tu ?', bulles: [
      { id: 'foret', bouton: '🌳 Une forêt' }, { id: 'ville', bouton: '🏙 La ville la nuit' }, { id: 'espace', bouton: '🌌 L’espace' },
      { id: 'eau', bouton: '🌊 Sous l’eau' }, { id: 'desert', bouton: '🏜 Un désert' }, { id: 'neige', bouton: '❄️ La neige' }] },
    { id: 'bon', h: 100, icone: '🎁', nom: 'Ce que j’attrape', q: 'Que veux-tu attraper ?', ia: true, bulles: [{ id: 'monde', bouton: '🎁 Ceux du monde' }] },
    { id: 'mal', h: 0, icone: '💣', nom: 'Ce que j’évite', q: 'Que veux-tu éviter ?', ia: true, bulles: [{ id: 'monde', bouton: '💣 Ceux du monde' }] },
    { id: 'action', h: 265, icone: '⚡', nom: 'Le gros bouton', q: 'Que fait le gros bouton ?', bulles: [
      { id: 'saut', bouton: '🦘 Sauter' }, { id: 'tir', bouton: '💥 Tirer' }, { id: 'bouclier', bouton: '🛡 Bouclier' }, { id: 'turbo', bouton: '💨 Turbo' }, { id: 'aimant', bouton: '🧲 Aimant' }] }
  ];
  const DEFAUT = { jeu: 'attrape', perso: 'chat', couleur: 'orange', monde: 'foret', bon: 'monde', mal: 'monde', action: 'saut' };
// L'IA propose d'autres idées pour perso, bon et mal : chacune devient une image (icône de la page, numéro dans r.imgs) et son choix vaut 'img'. Sans image trouvée, retour au dessin du monde.
const saute = (id, r) => id === 'couleur' && (r || {}).perso === 'img';   // une image garde ses couleurs : pas d'étape « couleur »   // une étape sans choix : le jeu marche quand même

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

def perso(x, y)
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
  if HT <= 0 || int(t * 12) % 2 == 0 perso(int(PX), int(PY)) end
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

  const SYSTEMES = { attrape: chute, esquive: chute, pluie: chute };   // jeu -> son système ; les prochains jeux (plateforme, course...) ajoutent ici leur propre générateur

  function recetteComplete(r) {   // une étape sans choix (ou un choix inconnu) prend sa valeur par défaut : le jeu marche toujours
    const o = {};
    r = r || {}; const imgs = r.imgs || {}; o.imgs = imgs;
    for (const e of ETAPES) o[e.id] = e.bulles.some(b => b.id === r[e.id]) || (e.ia && r[e.id] === 'img' && imgs[e.id] > 0) ? r[e.id] : DEFAUT[e.id];
    return o;
  }
  function assembler(r) {
    const c = recetteComplete(r);
    return SYSTEMES[c.jeu](c);
  }
  const AIDE = { saut: 'il saute', tir: 'il tire', bouclier: 'il se protege avec un bouclier', turbo: 'il va plus vite', aimant: 'il attire les bons objets' };
M.aide = r => { const a = recetteComplete(r).action; return 'G et D pour bouger, le gros bouton : ' + AIDE[a] + '.'; };
M.EN = {"Le jeu":"The game","Quel jeu veux-tu ?":"Which game?","🍎 Attraper":"🍎 Catch","💨 Esquiver":"💨 Dodge","🌧 Ne rien rater":"🌧 Miss nothing","Mon personnage":"My character","Qui es-tu ?":"Who are you?","🐱 Un chat":"🐱 A cat","🤖 Un robot":"🤖 A robot","🐉 Un dragon":"🐉 A dragon","🚀 Une fusée":"🚀 A rocket","👻 Un fantôme":"👻 A ghost","🧙 Un magicien":"🧙 A wizard","🐸 Une grenouille":"🐸 A frog","🦉 Un hibou":"🦉 An owl","Sa couleur":"Its colour","De quelle couleur ?":"Which colour?","🔴 Rouge":"🔴 Red","🟠 Orange":"🟠 Orange","🟡 Jaune":"🟡 Yellow","🟢 Vert":"🟢 Green","🔵 Bleu":"🔵 Blue","🟣 Violet":"🟣 Purple","🩷 Rose":"🩷 Pink","⚪ Blanc":"⚪ White","Le monde":"The world","Où es-tu ?":"Where are you?","🌳 Une forêt":"🌳 A forest","🏙 La ville la nuit":"🏙 The city at night","🌌 L’espace":"🌌 Space","🌊 Sous l’eau":"🌊 Underwater","🏜 Un désert":"🏜 A desert","❄️ La neige":"❄️ Snow","Ce que j’attrape":"What I catch","Que veux-tu attraper ?":"What do you want to catch?","🎁 Ceux du monde":"🎁 The ones from this world","Ce que j’évite":"What I avoid","Que veux-tu éviter ?":"What do you want to avoid?","💣 Ceux du monde":"💣 The ones from this world","Le gros bouton":"The big button","Que fait le gros bouton ?":"What does the big button do?","🦘 Sauter":"🦘 Jump","💥 Tirer":"💥 Shoot","🛡 Bouclier":"🛡 Shield","💨 Turbo":"💨 Turbo","🧲 Aimant":"🧲 Magnet","G et D pour bouger, le gros bouton : il saute.":"L and R to move, big button: it jumps.","G et D pour bouger, le gros bouton : il tire.":"L and R to move, big button: it shoots.","G et D pour bouger, le gros bouton : il se protege avec un bouclier.":"L and R to move, big button: it protects itself with a shield.","G et D pour bouger, le gros bouton : il va plus vite.":"L and R to move, big button: it goes faster.","G et D pour bouger, le gros bouton : il attire les bons objets.":"L and R to move, big button: it attracts the good things."};   // la page anglaise (lang="en") traduit les bulles avec ce dictionnaire
M.ETAPES = ETAPES; M.saute = saute; M.DEFAUT = DEFAUT; M.recetteComplete = recetteComplete; M.assembler = assembler;
})(typeof module === 'object' ? module.exports : (window.JeuxBulles = {}));
