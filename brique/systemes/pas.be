# Pas a pas
# exemple: SYSTEME deplacement case par case (grille, un pas a intervalle regulier, G et D tournent d'un quart de tour, glissement doux d'une case a l'autre, bords qui bouclent) | mots: case, cases, grille, pas, serpent, snake, plateau, damier, pacman, quart de tour, tour par tour
var C = 16 var W = 20 var H = 15
var DX = [1, 0, -1, 0] var DY = [0, 1, 0, -1]     # droite, bas, gauche, haut
var gx = 5 var gy = 5 var ax = 5 var ay = 5 var dir = 0 var pas = 0.0

def maj(dt)
  if appui(G) dir = (dir + 3) % 4 end
  if appui(D) dir = (dir + 1) % 4 end
  pas += (bouton(P3) ? 10 : 5) * dt                 # 5 cases par seconde, 10 si P3 tenu
  if pas >= 1
    pas -= 1
    ax = gx ay = gy                                 # ancienne case, pour le glissement
    gx = (gx + DX[dir] + W) % W gy = (gy + DY[dir] + H) % H
  end
end

def dessine()
  fond(rgb(15, 25, 20))
  var x = ax * C + int((gx - ax) * C * pas) var y = ay * C + int((gy - ay) * C * pas)
  if (gx - ax) * (gx - ax) + (gy - ay) * (gy - ay) > 1 x = gx * C y = gy * C end   # passage par le bord : pas de glissement
  rect(x + 1, y + 1, C - 2, C - 2, rgb(80, 220, 90))
end
