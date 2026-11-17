// Bestiaire de Brique : les FORMES personnalisables des jeux, rangées par thème, et la décomposition de chaque jeu en éléments.
// Une forme = quelques primitives dans une grille 16 x 16, comme les appels de dessin de la console (rect, disque, tri, ligne) : elle se voit en SVG dans l'Établi
// et se traduit mot pour mot en Berry (M.berry). Deux couleurs personnalisables : a (principale) et b (accent) ; c = a foncée ; w = blanc ; k = presque noir ; ou un #hex fixe.
// Même fichier pour l'Établi (copié dans la page) et pour node (test-bestiaire.js).
(function (M) {
  const r = (x, y, w, h, c) => ['r', x, y, w, h, c], d = (x, y, ra, c) => ['d', x, y, ra, c], t = (x1, y1, x2, y2, x3, y3, c) => ['t', x1, y1, x2, y2, x3, y3, c], l = (x1, y1, x2, y2, c) => ['l', x1, y1, x2, y2, c];
  const S = (id, nom, theme, role, a, b, p) => ({ id, nom, theme, role, a, b, p });

  M.THEMES = [{ id: 'heros', nom: 'Héros' }, { id: 'foret', nom: 'Forêt' }, { id: 'ville', nom: 'Ville' }, { id: 'espace', nom: 'Espace' }, { id: 'eau', nom: 'Sous l’eau' }, { id: 'desert', nom: 'Désert' }, { id: 'neige', nom: 'Neige' }, { id: 'pieces', nom: 'Pièces de jeu' }];
  M.ROLES = { heros: 'Héros', plateforme: 'Plateforme, sol', bon: 'À attraper', mal: 'À éviter', ennemi: 'Ennemi', decor: 'Décor', tir: 'Tir', piece: 'Pièce de jeu' };

  M.FORMES = [
    // ---- héros (valables dans tous les mondes) ----
    S('chat', 'Chat', 'heros', 'heros', '#f0a040', '#ffd0a0', [t(2, 7, 3, 1, 7, 4, 'a'), t(14, 7, 13, 1, 9, 4, 'a'), d(8, 9, 6, 'a'), d(8, 11, 3, 'b'), r(5, 7, 2, 2, 'w'), r(9, 7, 2, 2, 'w'), r(6, 8, 1, 1, 'k'), r(10, 8, 1, 1, 'k'), t(7, 10, 9, 10, 8, 11, '#e07080'), l(3, 11, 0, 10, 'k'), l(13, 11, 16, 10, 'k')]),
    S('robot', 'Robot', 'heros', 'heros', '#9aa8c0', '#ff6060', [r(7, 0, 2, 3, 'b'), d(8, 0, 1, 'b'), r(2, 3, 12, 9, 'a'), r(3, 4, 10, 5, 'c'), r(4, 5, 3, 3, 'w'), r(9, 5, 3, 3, 'w'), r(5, 6, 1, 1, 'k'), r(10, 6, 1, 1, 'k'), r(5, 10, 6, 1, 'k'), r(3, 12, 4, 3, 'c'), r(9, 12, 4, 3, 'c')]),
    S('dragon', 'Dragon', 'heros', 'heros', '#50b060', '#e8a040', [t(1, 12, 4, 9, 4, 14, 'c'), d(7, 10, 5, 'a'), d(11, 6, 3, 'a'), t(9, 4, 10, 0, 12, 4, 'b'), t(2, 5, 6, 2, 6, 9, 'b'), r(12, 5, 1, 1, 'k'), t(13, 7, 16, 7, 14, 9, 'a'), t(14, 8, 16, 10, 13, 10, '#ff7030'), d(7, 12, 3, 'b')]),
    S('fusee', 'Fusée', 'heros', 'heros', '#e8ecf4', '#e03030', [t(8, 0, 5, 5, 11, 5, 'b'), r(5, 5, 6, 8, 'a'), d(8, 8, 2, 'w'), d(8, 8, 1, '#60b0ff'), t(5, 9, 1, 15, 5, 13, 'b'), t(11, 9, 15, 15, 11, 13, 'b'), t(6, 13, 10, 13, 8, 16, '#ffa030')]),
    S('fantome', 'Fantôme', 'heros', 'heros', '#e8eaff', '#ff90b0', [d(8, 7, 5, 'a'), r(3, 7, 10, 7, 'a'), t(3, 14, 5, 14, 4, 16, 'a'), t(6, 14, 8, 14, 7, 16, 'a'), t(9, 14, 11, 14, 10, 16, 'a'), r(5, 6, 2, 3, 'k'), r(9, 6, 2, 3, 'k'), d(8, 11, 1, 'b')]),
    S('magicien', 'Magicien', 'heros', 'heros', '#7050c0', '#ffd040', [t(8, 0, 4, 7, 12, 7, 'a'), r(3, 7, 10, 2, 'a'), d(8, 5, 1, 'b'), d(8, 11, 3, '#f0c8a0'), r(7, 10, 1, 1, 'k'), r(9, 10, 1, 1, 'k'), t(5, 12, 11, 12, 8, 16, 'w'), r(4, 13, 8, 3, 'a')]),
    S('grenouille', 'Grenouille', 'heros', 'heros', '#60c050', '#d8f0a0', [d(8, 10, 5, 'a'), d(4, 6, 3, 'a'), d(12, 6, 3, 'a'), d(4, 5, 2, 'w'), d(12, 5, 2, 'w'), d(4, 5, 1, 'k'), d(12, 5, 1, 'k'), d(8, 12, 3, 'b'), r(5, 10, 6, 1, 'c')]),
    S('hibou', 'Hibou', 'heros', 'heros', '#a07850', '#e8d0a0', [t(2, 1, 6, 4, 3, 6, 'c'), t(14, 1, 10, 4, 13, 6, 'c'), d(8, 9, 6, 'a'), d(8, 12, 3, 'b'), d(5, 7, 2, 'w'), d(11, 7, 2, 'w'), d(5, 7, 1, 'k'), d(11, 7, 1, 'k'), t(7, 9, 9, 9, 8, 11, '#f0a020')]),
    S('oiseau', 'Oiseau', 'heros', 'heros', '#f4d03f', '#e8a020', [d(7, 8, 5, 'a'), d(6, 9, 3, 'b'), t(12, 7, 16, 8, 12, 10, '#f07020'), d(5, 7, 1, 'k'), r(3, 3, 3, 2, 'a')]),
    S('blob', 'Bestiole', 'heros', 'heros', '#ffcc66', '#ff9090', [d(8, 9, 6, 'a'), d(6, 8, 1, 'k'), d(10, 8, 1, 'k'), r(6, 11, 4, 1, 'k'), d(4, 11, 1, 'b'), d(12, 11, 1, 'b'), r(7, 1, 2, 2, 'a')]),
    // ---- forêt ----
    S('branche', 'Branche', 'foret', 'plateforme', '#4caf50', '#7a4a20', [r(0, 6, 16, 4, 'b'), r(0, 5, 16, 3, 'a'), r(2, 9, 2, 1, 'c'), r(9, 8, 3, 1, 'c')]),
    S('pomme', 'Pomme', 'foret', 'bon', '#e03030', '#4caf50', [d(8, 9, 5, 'a'), r(8, 2, 1, 3, '#5a3a10'), t(9, 3, 13, 2, 11, 6, 'b'), d(6, 7, 1, 'w')]),
    S('champignon', 'Champignon', 'foret', 'mal', '#d83030', '#ffffff', [r(6, 10, 4, 5, 'b'), t(8, 1, 1, 10, 15, 10, 'a'), r(1, 9, 14, 2, 'a'), d(6, 7, 1, 'b'), d(10, 6, 1, 'b')]),
    S('herisson', 'Hérisson', 'foret', 'ennemi', '#8a5a30', '#d8b080', [t(1, 12, 3, 3, 6, 10, 'c'), t(4, 12, 6, 1, 9, 10, 'c'), t(8, 12, 11, 2, 13, 10, 'c'), t(11, 12, 15, 5, 15, 12, 'c'), d(8, 11, 5, 'a'), d(13, 12, 2, 'b'), r(14, 11, 1, 1, 'k')]),
    S('sapin', 'Sapin', 'foret', 'decor', '#2e7d32', '#7a4a20', [r(7, 12, 2, 4, 'b'), t(8, 0, 3, 8, 13, 8, 'a'), t(8, 4, 1, 12, 15, 12, 'a')]),
    S('fleur', 'Fleur', 'foret', 'decor', '#ff80b0', '#ffe060', [r(8, 8, 1, 8, '#3a8a3a'), d(8, 2, 2, 'a'), d(8, 8, 2, 'a'), d(5, 5, 2, 'a'), d(11, 5, 2, 'a'), d(8, 5, 2, 'b')]),
    // ---- ville ----
    S('toit', 'Toit', 'ville', 'plateforme', '#6b7280', '#9aa3af', [r(0, 6, 16, 4, 'a'), r(0, 6, 16, 1, 'b'), r(2, 10, 3, 2, 'c'), r(10, 10, 3, 2, 'c')]),
    S('piece', 'Pièce', 'ville', 'bon', '#ffd040', '#e0a820', [d(8, 8, 6, 'b'), d(8, 8, 5, 'a'), r(7, 5, 2, 6, 'b')]),
    S('voiture', 'Voiture', 'ville', 'mal', '#e03030', '#a02020', [r(1, 8, 14, 5, 'a'), r(4, 4, 8, 4, 'b'), r(5, 5, 2, 2, 'w'), r(9, 5, 2, 2, 'w'), d(4, 13, 2, 'k'), d(12, 13, 2, 'k'), r(14, 9, 2, 2, '#ffee80')]),
    S('drone', 'Drone', 'ville', 'ennemi', '#4a5568', '#ff3030', [r(1, 5, 5, 1, 'c'), r(10, 5, 5, 1, 'c'), r(7, 5, 2, 2, 'c'), d(8, 8, 3, 'a'), d(8, 8, 1, 'b'), r(3, 6, 1, 2, 'c'), r(12, 6, 1, 2, 'c')]),
    S('immeuble', 'Immeuble', 'ville', 'decor', '#3a4a6a', '#ffe680', [r(3, 1, 10, 15, 'a'), r(5, 3, 2, 2, 'b'), r(9, 3, 2, 2, 'b'), r(5, 7, 2, 2, 'b'), r(9, 7, 2, 2, 'b'), r(5, 11, 2, 2, 'b'), r(9, 11, 2, 2, 'b')]),
    S('lampadaire', 'Lampadaire', 'ville', 'decor', '#444a55', '#ffe680', [r(7, 4, 2, 12, 'a'), r(5, 15, 6, 1, 'a'), d(8, 3, 3, 'b')]),
    // ---- espace ----
    S('asteroide-plat', 'Roche flottante', 'espace', 'plateforme', '#7a7f8a', '#a0a6b0', [r(1, 6, 14, 5, 'a'), r(1, 6, 14, 1, 'b'), d(4, 8, 1, 'c'), d(10, 7, 2, 'c'), r(3, 11, 10, 1, 'c')]),
    S('etoile', 'Étoile', 'espace', 'bon', '#ffe060', '#ffffff', [t(8, 0, 4, 11, 12, 11, 'a'), t(8, 16, 4, 5, 12, 5, 'a'), d(8, 8, 2, 'b')]),
    S('meteorite', 'Météorite', 'espace', 'mal', '#8a5a40', '#ff8030', [d(8, 8, 6, 'a'), d(6, 6, 2, 'c'), d(10, 10, 2, 'c'), d(11, 5, 1, 'c'), t(1, 12, 4, 14, 0, 16, 'b')]),
    S('alien', 'Alien', 'espace', 'ennemi', '#60d060', '#ff60c0', [l(5, 3, 6, 5, 'b'), l(11, 3, 10, 5, 'b'), d(5, 3, 1, 'b'), d(11, 3, 1, 'b'), d(8, 9, 5, 'a'), d(6, 8, 1, 'w'), d(10, 8, 1, 'w'), r(6, 12, 4, 1, 'k')]),
    S('planete', 'Planète', 'espace', 'decor', '#c07040', '#f0c080', [d(8, 8, 6, 'a'), r(0, 7, 16, 2, 'b'), d(6, 5, 1, 'c')]),
    S('lune', 'Lune', 'espace', 'decor', '#e8e8f0', '#c0c0d0', [d(8, 8, 6, 'a'), d(6, 6, 2, 'b'), d(10, 10, 1, 'b'), d(11, 5, 1, 'b')]),
    // ---- sous l'eau ----
    S('corail', 'Corail', 'eau', 'plateforme', '#ff7a90', '#ffd0d8', [r(0, 9, 16, 7, 'a'), r(2, 5, 2, 5, 'a'), r(7, 3, 2, 7, 'a'), r(12, 6, 2, 4, 'a'), d(5, 12, 1, 'b'), d(11, 13, 1, 'b')]),
    S('perle', 'Perle', 'eau', 'bon', '#f4f0ff', '#c0b8e0', [d(8, 8, 5, 'b'), d(8, 8, 4, 'a'), d(6, 6, 1, 'w')]),
    S('poisson', 'Poisson', 'eau', 'bon', '#ff9a30', '#ffd060', [d(7, 8, 5, 'a'), t(11, 8, 16, 3, 16, 13, 'b'), d(5, 7, 1, 'k'), r(7, 5, 1, 6, 'b')]),
    S('meduse', 'Méduse', 'eau', 'mal', '#d070e0', '#f0a0f8', [d(8, 6, 5, 'a'), l(4, 9, 4, 14, 'b'), l(7, 10, 7, 15, 'b'), l(10, 10, 10, 15, 'b'), l(12, 9, 12, 14, 'b'), d(6, 5, 1, 'w'), d(10, 5, 1, 'w')]),
    S('requin', 'Requin', 'eau', 'ennemi', '#6a8aa8', '#ffffff', [r(1, 6, 12, 5, 'a'), t(13, 8, 16, 4, 16, 12, 'a'), t(5, 6, 8, 2, 9, 6, 'c'), r(1, 9, 12, 2, 'b'), d(3, 8, 1, 'k')]),
    S('algue', 'Algue', 'eau', 'decor', '#2e9a5a', '#60d090', [r(3, 6, 2, 10, 'a'), r(8, 2, 2, 14, 'a'), r(12, 8, 2, 8, 'a'), r(4, 6, 1, 3, 'b'), r(9, 2, 1, 5, 'b')]),
    // ---- désert ----
    S('rocher', 'Rocher plat', 'desert', 'plateforme', '#c08a50', '#e0b070', [r(0, 7, 16, 6, 'a'), r(0, 7, 16, 2, 'b'), r(3, 10, 2, 1, 'c'), r(10, 11, 3, 1, 'c')]),
    S('gemme', 'Gemme', 'desert', 'bon', '#40c0e0', '#a0f0ff', [t(8, 1, 3, 7, 13, 7, 'a'), t(3, 7, 13, 7, 8, 15, 'c'), t(8, 1, 6, 7, 10, 7, 'b')]),
    S('cactus', 'Cactus', 'desert', 'mal', '#3a9a4a', '#ffe060', [r(6, 3, 4, 13, 'a'), r(2, 6, 3, 5, 'a'), r(2, 9, 5, 2, 'a'), r(11, 5, 3, 5, 'a'), r(9, 8, 5, 2, 'a'), d(8, 2, 1, 'b')]),
    S('scorpion', 'Scorpion', 'desert', 'ennemi', '#a05030', '#d08060', [d(6, 11, 4, 'a'), r(10, 5, 2, 6, 'a'), r(8, 3, 3, 2, 'a'), t(8, 1, 10, 3, 7, 3, 'k'), d(2, 10, 2, 'b'), d(2, 13, 2, 'b'), r(5, 10, 1, 1, 'k')]),
    S('dune', 'Dune', 'desert', 'decor', '#e8c070', '#d0a850', [t(0, 16, 6, 5, 16, 16, 'a'), t(8, 16, 13, 9, 16, 16, 'b')]),
    S('soleil', 'Soleil', 'desert', 'decor', '#ffb020', '#ffe060', [d(8, 8, 5, 'a'), d(8, 8, 3, 'b'), r(7, 0, 2, 2, 'a'), r(7, 14, 2, 2, 'a'), r(0, 7, 2, 2, 'a'), r(14, 7, 2, 2, 'a')]),
    // ---- neige ----
    S('banquise', 'Banquise', 'neige', 'plateforme', '#bfe3ff', '#8fc4f0', [r(0, 7, 16, 4, 'a'), r(0, 7, 16, 2, 'w'), t(3, 11, 6, 11, 4, 14, 'b'), t(10, 11, 13, 11, 12, 14, 'b')]),
    S('flocon', 'Flocon', 'neige', 'bon', '#bfe8ff', '#ffffff', [r(7, 1, 2, 14, 'a'), r(1, 7, 14, 2, 'a'), l(3, 3, 13, 13, 'a'), l(13, 3, 3, 13, 'a'), d(8, 8, 2, 'b')]),
    S('glacon', 'Glaçon pointu', 'neige', 'mal', '#8fd0ff', '#e0f4ff', [t(3, 0, 13, 0, 8, 15, 'a'), t(5, 0, 8, 0, 7, 10, 'b')]),
    S('bonhomme-neige', 'Bonhomme de neige', 'neige', 'ennemi', '#ffffff', '#303040', [d(8, 12, 4, 'a'), d(8, 6, 3, 'a'), r(5, 2, 6, 2, 'b'), r(6, 0, 4, 3, 'b'), r(7, 5, 1, 1, 'k'), r(9, 5, 1, 1, 'k'), t(8, 6, 12, 7, 8, 8, '#ff8030'), d(8, 11, 1, 'k')]),
    S('sapin-neige', 'Sapin enneigé', 'neige', 'decor', '#2e6a4a', '#7a4a20', [r(7, 12, 2, 4, 'b'), t(8, 0, 3, 8, 13, 8, 'a'), t(8, 4, 1, 12, 15, 12, 'a'), t(8, 0, 6, 3, 10, 3, 'w'), t(8, 4, 5, 7, 11, 7, 'w')]),
    S('igloo', 'Igloo', 'neige', 'decor', '#e8f4ff', '#202838', [d(8, 12, 6, 'a'), r(2, 10, 12, 1, 'w'), r(6, 11, 4, 5, 'b')]),
    // ---- pièces de jeu (valables partout) ----
    S('balle', 'Balle ronde', 'pieces', 'piece', '#ffdc3c', '#ffffff', [d(8, 8, 4, 'a'), d(7, 7, 1, 'b')]),
    S('balle-carree', 'Balle carrée', 'pieces', 'piece', '#ffdc3c', '#ffffff', [r(4, 4, 8, 8, 'a'), r(5, 5, 2, 2, 'b')]),
    S('raquette', 'Raquette', 'pieces', 'piece', '#f0f0f0', '#ffffff', [r(1, 6, 14, 4, 'a'), r(1, 6, 14, 1, 'b'), r(1, 9, 14, 1, 'c')]),
    S('brique', 'Brique', 'pieces', 'piece', '#e8322b', '#ff8070', [r(0, 3, 16, 10, 'a'), r(0, 3, 16, 2, 'b'), r(0, 12, 16, 1, 'c'), r(8, 5, 1, 7, 'c')]),
    S('brique-dure', 'Brique d’acier', 'pieces', 'piece', '#a0a8b8', '#e0e4f0', [r(0, 3, 16, 10, 'a'), r(0, 3, 16, 1, 'b'), r(0, 12, 16, 1, 'c'), d(3, 8, 1, 'c'), d(13, 8, 1, 'c')]),
    S('tuyau', 'Tuyau', 'pieces', 'piece', '#46aa46', '#32883c', [r(3, 0, 10, 16, 'a'), r(1, 0, 14, 3, 'b'), r(5, 3, 1, 13, 'w')]),
    S('note', 'Note de musique', 'pieces', 'piece', '#e05080', '#ff90b0', [d(5, 12, 3, 'a'), r(7, 3, 2, 10, 'a'), r(7, 3, 6, 2, 'b'), r(11, 3, 2, 4, 'b')]),
    S('caisse', 'Caisse à casser', 'pieces', 'piece', '#c0823c', '#ffe080', [r(1, 1, 14, 14, 'a'), r(1, 1, 14, 2, 'c'), r(1, 13, 14, 2, 'c'), r(7, 5, 2, 2, 'b'), r(8, 8, 1, 2, 'b'), r(7, 11, 2, 1, 'b')]),
    S('trampoline', 'Trampoline', 'pieces', 'piece', '#ff7840', '#444444', [r(1, 9, 14, 2, 'a'), r(2, 11, 12, 1, 'c'), r(3, 12, 1, 4, 'b'), r(12, 12, 1, 4, 'b')]),
    S('coeur', 'Cœur', 'pieces', 'piece', '#f04050', '#ffffff', [d(5, 6, 3, 'a'), d(11, 6, 3, 'a'), t(2, 8, 14, 8, 8, 15, 'a'), d(4, 5, 1, 'b')]),
    S('drapeau', 'Drapeau', 'pieces', 'piece', '#e03030', '#8a8a90', [r(3, 1, 1, 15, 'b'), t(4, 1, 13, 4, 4, 8, 'a')]),
    S('panier', 'Panier', 'pieces', 'piece', '#b06a30', '#e0a060', [t(1, 5, 15, 5, 13, 14, 'a'), t(1, 5, 3, 14, 13, 14, 'a'), r(0, 4, 16, 2, 'b')]),
    S('laser', 'Laser', 'pieces', 'tir', '#ffdc3c', '#ffffff', [r(7, 0, 2, 12, 'a'), r(7, 0, 2, 3, 'b')]),
    S('tir-rond', 'Tir rond', 'pieces', 'tir', '#60e0ff', '#ffffff', [d(8, 8, 3, 'a'), d(8, 8, 1, 'b')])
  ];
  M.forme = id => M.FORMES.find(f => f.id === id);

  // ---- couleurs ----
  const hex = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
  const fonce = h => '#' + hex(h).map(v => Math.round(v * 0.6).toString(16).padStart(2, '0')).join('');
  M.couleur = (c, a, b) => c === 'a' ? a : c === 'b' ? b : c === 'c' ? fonce(a) : c === 'w' ? '#ffffff' : c === 'k' ? '#1a1a22' : c;

  // ---- SVG pour l'Établi ----
  M.svg = (f, a, b, px) => {
    a = a || f.a; b = b || f.b;
    const C = c => M.couleur(c, a, b), el = p => p[0] === 'r' ? `<rect x="${p[1]}" y="${p[2]}" width="${p[3]}" height="${p[4]}" fill="${C(p[5])}"/>`
      : p[0] === 'd' ? `<circle cx="${p[1]}" cy="${p[2]}" r="${p[3]}" fill="${C(p[4])}"/>`
      : p[0] === 't' ? `<polygon points="${p[1]},${p[2]} ${p[3]},${p[4]} ${p[5]},${p[6]}" fill="${C(p[7])}"/>`
      : `<line x1="${p[1]}" y1="${p[2]}" x2="${p[3]}" y2="${p[4]}" stroke="${C(p[5])}" stroke-width="1"/>`;
    return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="${px || 64}" height="${px || 64}" role="img" aria-label="${f.nom}" shape-rendering="crispEdges">${f.p.map(el).join('')}</svg>`;
  };

  // ---- Berry : une fonction forme_<id>(x, y, z) qui dessine la forme au coin (x, y), zoom z ----
  M.berry = (f, a, b) => {
    a = a || f.a; b = b || f.b;
    const C = c => { const v = hex(M.couleur(c, a, b)); return `rgb(${v[0]}, ${v[1]}, ${v[2]})`; }, P = (n, k) => n ? `${n} * z` : '0';
    const X = n => n ? `x + ${n} * z` : 'x', Y = n => n ? `y + ${n} * z` : 'y';
    const lignes = f.p.map(p => p[0] === 'r' ? `rect(${X(p[1])}, ${Y(p[2])}, ${p[3]} * z, ${p[4]} * z, ${C(p[5])})`
      : p[0] === 'd' ? `disque(${X(p[1])}, ${Y(p[2])}, ${p[3]} * z, ${C(p[4])})`
      : p[0] === 't' ? `tri(${X(p[1])}, ${Y(p[2])}, ${X(p[3])}, ${Y(p[4])}, ${X(p[5])}, ${Y(p[6])}, ${C(p[7])})`
      : `ligne(${X(p[1])}, ${Y(p[2])}, ${X(p[3])}, ${Y(p[4])}, ${C(p[5])})`);
    return `def forme_${f.id.replace(/-/g, '_')}(x, y, z)\n  ${lignes.join('\n  ')}\nend`;
  };

  // ---- la décomposition des jeux : chaque élément = un rôle (les formes du bestiaire qui peuvent le remplacer) ; f = formes précises, sinon toutes celles du rôle ----
  // rôle 'fond' = couleur seulement (haut et bas du dégradé) ; 'texte' = couleur seulement. Toujours personnalisable : la couleur (a, b) ; la forme quand le rôle a des formes.
  const E = (n, role, note, f) => ({ n, role, note: note || '', f: f || null });
  const fond = E('Fond', 'fond', 'ciel ou mur du fond : 2 couleurs en dégradé, selon le monde'), decor = E('Décor', 'decor', 'arbres, immeubles, planètes… posés derrière, 2 plans'), vies = E('Vies', 'piece', 'une icône par vie', ['coeur']), score = E('Score', 'texte', 'couleur du texte et de son ombre');
  M.JEUX = [
    { id: 'plateforme', nom: 'Plateforme (drapeau, étoiles, monstres, course)', verbe: 'je saute', fichier: 'saut.be · exemples/4-plateforme.be', el: [fond, decor, E('Plateformes', 'plateforme', 'le sol et les marches'), E('Personnage', 'heros', 'taille fixe'), E('À ramasser', 'bon', 'étoiles ou fruits'), E('Monstres', 'ennemi', 'ils patrouillent'), E('Arrivée', 'piece', 'le drapeau', ['drapeau']), vies, score] },
    { id: 'attrape', nom: 'Attrape (personnage en bas)', verbe: 'j’attrape', fichier: 'jeu-attrape.be', el: [fond, decor, E('Personnage / panier', 'heros', 'ou le panier', ['panier']), E('Bon objet', 'bon', 'tombe, on l’attrape'), E('Mauvais objet', 'mal', 'tombe, on l’évite'), vies, score] },
    { id: 'vaisseau-centre', nom: 'Vaisseau central qui tourne', verbe: 'je tire', fichier: 'jeu-vaisseau-centre.be', el: [E('Fond', 'fond', 'espace : étoiles'), E('Vaisseau', 'heros', 'au centre, tourne', ['fusee']), E('Astéroïdes', 'mal', 'arrivent de tous les bords', ['meteorite']), E('Tir', 'tir', 'part dans la direction du vaisseau'), vies, score] },
    { id: 'shmup', nom: 'Vaisseau sur fond qui défile', verbe: 'je tire', fichier: 'jeu-shmup.be', el: [fond, E('Étoiles', 'decor', 'défilent en 2 couches (lente, rapide)', ['etoile', 'planete', 'lune']), E('Vaisseau', 'heros', 'à gauche, monte et descend'), E('Ennemis', 'ennemi', 'arrivent de la droite'), E('Tir', 'tir', 'vers la droite'), vies, score] },
    { id: 'casse-briques', nom: 'Casse-briques', verbe: 'je casse', fichier: 'jeu-casse-briques.be', el: [fond, E('Raquette', 'piece', 'G et D', ['raquette']), E('Balle', 'piece', 'ronde ou carrée', ['balle', 'balle-carree']), E('Briques', 'piece', '3 rangées, 3 couleurs', ['brique', 'brique-dure']), vies, score] },
    { id: 'plateforme-casse', nom: 'Plateforme : casser et découvrir', verbe: 'je casse', fichier: 'jeu-plateforme-casse.be', el: [fond, decor, E('Sol', 'plateforme', 'sol continu'), E('Personnage', 'heros', 'la tête casse'), E('Caisses', 'piece', 'à casser par en dessous', ['caisse', 'brique']), E('Objets cachés', 'bon', 'apparaissent quand la caisse casse'), score] },
    { id: 'flappy', nom: 'Flappy', verbe: 'je rebondis', fichier: 'jeu-flappy.be', el: [E('Ciel', 'fond', 'dégradé selon le monde'), decor, E('Sol', 'plateforme', 'bande du bas'), E('Oiseau', 'heros', 'une touche : il bat des ailes', ['oiseau', 'fusee', 'fantome', 'chat']), E('Tuyaux', 'piece', 'le trou se déplace', ['tuyau']), score] },
    { id: 'pong', nom: 'Pong', verbe: 'je rebondis', fichier: 'jeu-pong.be', el: [E('Terrain', 'fond', 'fond du terrain'), E('Ma raquette', 'piece', 'à gauche', ['raquette']), E('Raquette adverse', 'piece', 'à droite, jouée par la console', ['raquette']), E('Balle', 'piece', 'carrée ou ronde (ROND)', ['balle', 'balle-carree']), E('Filet', 'texte', 'trait pointillé : couleur seulement'), score] },
    { id: 'plateforme-rebond', nom: 'Plateforme qui rebondit', verbe: 'je rebondis', fichier: 'jeu-plateforme-rebond.be', el: [fond, decor, E('Plateformes', 'plateforme', '8 en colonne, recyclées'), E('Trampolines', 'piece', 'rebond plus haut', ['trampoline']), E('Personnage', 'heros', 'rebondit tout seul'), score] },
    { id: 'plateforme-quiz', nom: 'Plateforme à questions', verbe: 'j’apprends', fichier: 'jeu-plateforme-quiz.be', el: [fond, E('Sol', 'plateforme', ''), E('Plateformes', 'plateforme', '3 en l’air'), E('Personnage', 'heros', ''), E('Objets-réponses', 'bon', 'un par plateforme, porte la réponse'), E('Question', 'texte', 'texte en haut'), vies, score] },
    { id: 'tamagotchi', nom: 'Tamagotchi', verbe: 'je fais grandir', fichier: 'jeu-tamagotchi.be', el: [E('Pièce', 'fond', 'mur et sol'), E('Bestiole', 'heros', 'grandit avec le temps (4 tailles)', ['blob', 'chat', 'grenouille', 'hibou', 'dragon']), E('Jauge manger', 'bon', 'icône + barre', ['pomme', 'poisson']), E('Jauge jouer', 'piece', 'icône + barre', ['balle', 'balle-carree']), E('Jauge dormir', 'decor', 'icône + barre', ['lune']), E('Câlin', 'piece', 'petit cœur', ['coeur'])] },
    { id: 'plateforme-grandit', nom: 'Plateforme qui fait grandir', verbe: 'je fais grandir', fichier: 'jeu-plateforme-grandit.be', el: [fond, decor, E('Plateformes', 'plateforme', ''), E('Personnage', 'heros', '4 tailles, couleur qui change'), E('Fruit', 'bon', 'chaque fruit le fait grandir'), score] },
    { id: 'rythme', nom: 'Rythme (guitar hero)', verbe: 'je réagis', fichier: 'jeu-rythme.be', el: [E('Scène', 'fond', 'fond sombre'), E('Couloirs', 'texte', '3 couleurs, une par bouton : couleurs seulement'), E('Notes', 'piece', 'tombent dans leur couloir', ['note', 'balle', 'etoile']), E('Ligne de frappe', 'texte', 'couleur seulement'), E('Combo et score', 'texte', '')] },
    { id: 'alignement', nom: 'Plateformes à aligner', verbe: 'je réagis', fichier: 'jeu-alignement.be', el: [fond, E('Étages', 'plateforme', 'chaque étage change de teinte'), E('Bloc qui glisse', 'piece', 'même forme que les étages', ['brique', 'brique-dure', 'caisse']), score] }
  ];
  M.ROLES_SANS_FORME = { fond: 1, texte: 1 };
})(typeof module !== 'undefined' ? module.exports : (window.Bestiaire = {}));
