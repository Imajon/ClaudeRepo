# c3-tft-meatboy — PCB polygon ajouré C (ESP32-C3 SuperMini)

Généré par `../make_kicad_polygon_c3.py C` (routeur `../route_tab.py`). Aperçu : `c3-tft-meatboy-polygon-ajoure-C.svg`.
Esquisse de départ de la languette : `../polygon-ajoure-C-accroche.svg` (`../make_accroche_svg.py`).

**= ajouré B +** :
- **Languette d'accroche** détachable (façon Pocket Operator) : oblong 66 × 10,75 mm, trou 26 × 4 mm + demi-cercle R3,5 vers le haut
  (crochet de rayon). Tenue par **3 attaches de 2 mm** sur les 3 pointes du haut, avec petites encoches d'amorce ; se casse à la pince.
  Les pointes sont tronquées de 0,55 mm (rien ne dépasse après cassure). Bord cuivre nu de 1 mm + stries sur la languette (sans net).
- **Stries partout où elles tiennent** sur la face avant (hors écran, pistes, pastilles, sérigraphie) : cuivre nu relié au GND
  (zones soudées au plan de masse) ; les quelques stries enfermées entre des pistes restent en cuivre sans net.
- **Pastille GND** à côté de chaque pastille de fil (IO21, IO20, IO0, IO8), côté intérieur.
- **Pastilles de l'ESP32-C3** allongées de 1 mm vers l'extérieur (soudure à la main), sauf la rangée du bas (trop près de la coupe USB-C).
- Les ajours qui gênaient les pastilles GND sont retirés (3 restent).

- **QR code** cuivre au dos, 35,4 mm → https://studioeya.com.
- **LED optionnelles** en haut (GAUCHE, DROITE, SAUT), CMS 1206 : +3V3 → R → LED → fil du bouton. Bouton appuyé = LED allumée.
  R : LED rouge/jaune/verte 470 Ω (≈ 3 mA) ; bleue/blanche 68 Ω. Trait sérigraphié = cathode.
- BL de l'écran (J3 broche 8) relié au +3V3.
- DRC : 0 non connecté ; 1 remarque (frein thermique de la broche GND de l'écran au dos, déjà reliée par la face avant).
