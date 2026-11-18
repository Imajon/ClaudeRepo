# Echo
import math
# D = avancer, G = reculer, P3 = tourner (et rejouer a la fin)
# Chaque pas lance une onde sonore devant toi : si elle touche un garde, il vient voir.

var C = 16                       # taille d'une case
var W = 20 var H = 14            # grille 20 x 14, sous une bande de 16 px pour le texte
var OY = 16
var NMURS = 30
var NENN = 2
var NOND = 6

var c_fond = rgb(14, 18, 34) var c_case = rgb(22, 28, 50) var c_mur = rgb(70, 80, 120)
var c_murh = rgb(110, 120, 170) var c_joueur = rgb(235, 235, 245) var c_enn = rgb(225, 80, 90)
var c_onde = rgb(120, 200, 255) var c_but = rgb(90, 210, 130)

var MUR = []                     # MUR[y * W + x] = true si mur
var VU = [] var FILE = []        # pour verifier qu'un chemin existe (remplis dans debut)
var px = 0 var py = 0 var pdir = 0   # 0 haut, 1 droite, 2 bas, 3 gauche
var bx = 0 var by = 0            # sortie
var EX = [0, 0] var EY = [0, 0] var ED = [0, 0]
var EALERTE = [0.0, 0.0] var EAX = [0, 0] var EAY = [0, 0] var ETIC = [0.0, 0.0]
var OX = [0, 0, 0, 0, 0, 0] var OYY = [0, 0, 0, 0, 0, 0] var OD = [0, 0, 0, 0, 0, 0]
var ODIST = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0] var OVIE = [false, false, false, false, false, false]
var DX = [0, 1, 0, -1] var DY = [-1, 0, 1, 0]
var etat = 0                     # 0 en jeu, 1 gagne, 2 perdu
var t_aide = 0.0

def libre(x, y)
  return x >= 0 && x < W && y >= 0 && y < H && !MUR[y * W + x]
end

def atteignable()                # parcours en largeur du joueur vers la sortie
  var i = 0
  while i < W * H VU[i] = false i += 1 end
  FILE[0] = py * W + px VU[FILE[0]] = true
  var n = 1
  i = 0
  while i < n
    var x = FILE[i] % W var y = int(FILE[i] / W)
    if x == bx && y == by return true end
    for d: 0 .. 3
      var nx = x + DX[d] var ny = y + DY[d]
      if libre(nx, ny) && !VU[ny * W + nx]
        VU[ny * W + nx] = true FILE[n] = ny * W + nx n += 1
      end
    end
    i += 1
  end
  return false
end

def nouvelle_partie()
  px = int(W / 2) py = int(H / 2) pdir = 0
  bx = W - 1 by = H - 1
  var ok = false
  while !ok
    var k = 0
    while k < W * H MUR[k] = false k += 1 end
    var n = 0
    while n < NMURS
      var x = hasard(0, W - 1) var y = hasard(0, H - 1)
      var loin = math.abs(x - px) + math.abs(y - py) > 1
      if loin && !(x == bx && y == by) && !MUR[y * W + x]
        MUR[y * W + x] = true n += 1
      end
    end
    ok = atteignable()
  end
  for e: 0 .. NENN - 1
    var x = 0 var y = 0
    while true
      x = hasard(0, W - 1) y = hasard(0, H - 1)
      if libre(x, y) && math.abs(x - px) + math.abs(y - py) > 5 && !(x == bx && y == by) break end
    end
    EX[e] = x EY[e] = y ED[e] = hasard(0, 3)
    EALERTE[e] = 0.0 ETIC[e] = 0.0
  end
  for o: 0 .. NOND - 1 OVIE[o] = false end
  etat = 0
  t_aide = 3.0
end

def lancer_onde(d)
  for o: 0 .. NOND - 1
    if !OVIE[o]
      OVIE[o] = true OX[o] = px OYY[o] = py OD[o] = d ODIST[o] = 0.0
      son(660, 25)
      return
    end
  end
end

def bouger_joueur(sens)
  var d = sens > 0 ? pdir : (pdir + 2) % 4
  var nx = px + DX[d] var ny = py + DY[d]
  if !libre(nx, ny) son(120, 40) return end
  for e: 0 .. NENN - 1
    if EX[e] == nx && EY[e] == ny etat = 2 son(110, 300) return end
  end
  px = nx py = ny
  lancer_onde(d)
  if px == bx && py == by etat = 1 son(1046, 200) end
end

def maj_ondes(dt)
  var o = 0
  while o < NOND
    if OVIE[o]
      var avant = int(ODIST[o])
      ODIST[o] += 12 * dt          # 12 cases par seconde
      if ODIST[o] >= 10 OVIE[o] = false
      elif int(ODIST[o]) > avant
        var nx = OX[o] + DX[OD[o]] var ny = OYY[o] + DY[OD[o]]
        if !libre(nx, ny) OVIE[o] = false
        else
          OX[o] = nx OYY[o] = ny
          var e = 0
          while e < NENN
            if EX[e] == nx && EY[e] == ny
              EALERTE[e] = 4.0 EAX[e] = px EAY[e] = py   # le garde va la ou tu as fait du bruit
              OVIE[o] = false son(330, 80)
            end
            e += 1
          end
        end
      end
    end
    o += 1
  end
end

def maj_garde(e, dt)
  if EALERTE[e] > 0 EALERTE[e] -= dt end
  ETIC[e] += dt
  var pause = EALERTE[e] > 0 ? 0.35 : 0.6
  if ETIC[e] < pause return end
  ETIC[e] = 0.0
  if EALERTE[e] > 0
    var dx = EAX[e] - EX[e] var dy = EAY[e] - EY[e]
    if dx == 0 && dy == 0 EALERTE[e] = 0.0
    elif dx != 0 && libre(EX[e] + (dx > 0 ? 1 : -1), EY[e]) ED[e] = dx > 0 ? 1 : 3
    elif dy != 0 ED[e] = dy > 0 ? 2 : 0
    else ED[e] = dx > 0 ? 1 : 3 end
  elif hasard(0, 9) == 0
    ED[e] = hasard(0, 3)
  end
  var nx = EX[e] + DX[ED[e]] var ny = EY[e] + DY[ED[e]]
  if !libre(nx, ny) ED[e] = hasard(0, 3) return end
  EX[e] = nx EY[e] = ny
  if nx == px && ny == py etat = 2 son(110, 300) end
end

def debut()
  for i: 0 .. W * H - 1 MUR.push(false) VU.push(false) FILE.push(0) end
  nouvelle_partie()
end

def maj(dt)
  if t_aide > 0 t_aide -= dt end
  if etat != 0
    if appui(P3) nouvelle_partie() end
    return
  end
  if appui(P3) pdir = (pdir + 1) % 4 son(440, 15) end
  if appui(D) bouger_joueur(1) end
  if appui(G) bouger_joueur(-1) end
  if etat != 0 return end
  maj_ondes(dt)
  var e = 0
  while e < NENN if etat == 0 maj_garde(e, dt) end e += 1 end
end

def pas(x, y, d, c)              # deux pieds cote a cote, tournes vers d
  var cx = x * C + 8 var cy = OY + y * C + 8
  if d == 0 || d == 2
    var av = d == 0 ? -2 : 2
    rect(cx - 5, cy - 3 + av, 3, 6, c) rect(cx + 2, cy - 3 - av, 3, 6, c)
  else
    var av = d == 1 ? 2 : -2
    rect(cx - 3 + av, cy - 5, 6, 3, c) rect(cx - 3 - av, cy + 2, 6, 3, c)
  end
end

def dessine()
  fond(c_fond)
  rect(0, OY, W * C, H * C, c_case)
  var i = 0
  while i < W * H
    if MUR[i]
      var x = (i % W) * C var y = OY + int(i / W) * C
      rect(x, y, C, C, c_mur) rect(x, y, C, 3, c_murh)
    end
    i += 1
  end
  var pulse = int(temps() * 6) % 2
  cadre(bx * C + 2 - pulse, OY + by * C + 2 - pulse, 12 + 2 * pulse, 12 + 2 * pulse, c_but)
  disque(bx * C + 8, OY + by * C + 8, 3, c_but)
  var o = 0
  while o < NOND
    if OVIE[o] cercle(OX[o] * C + 8, OY + OYY[o] * C + 8, 3 + int(ODIST[o] * 3) % 5, c_onde) end
    o += 1
  end
  var e = 0
  while e < NENN
    pas(EX[e], EY[e], ED[e], c_enn)
    if EALERTE[e] > 0 && pulse == 1 cercle(EX[e] * C + 8, OY + EY[e] * C + 8, 8, c_enn) end
    e += 1
  end
  pas(px, py, pdir, c_joueur)
  var cx = px * C + 8 var cy = OY + py * C + 8
  ligne(cx, cy, cx + DX[pdir] * 8, cy + DY[pdir] * 8, c_onde)
  if t_aide > 0 texte(4, 4, "P3 tourne  D avance  G recule", c_joueur, 1)
  else texte(4, 4, "Echo : rejoins la sortie verte", c_murh, 1) end
  if etat == 1
    texte(101, 101, "GAGNE !", rgb(0, 0, 0), 3) texte(100, 100, "GAGNE !", c_but, 3)
    texte(100, 140, "P3 pour rejouer", c_joueur, 1)
  elif etat == 2
    texte(71, 101, "ATTRAPE !", rgb(0, 0, 0), 3) texte(70, 100, "ATTRAPE !", c_enn, 3)
    texte(100, 140, "P3 pour rejouer", c_joueur, 1)
  end
end
