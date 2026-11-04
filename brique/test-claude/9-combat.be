# Combat de robots
import math
var SOL = 200
var X = [90.0, 230.0] var VX = [0.0, 0.0]
var VIE = [10, 10] var COUP = [0.0, 0.0] var TOUCHE = [false, false]
var SONNE = [0.0, 0.0] var GARDE = [false, false]
var tenu = 0.0 var pense = 0.0 var envie = 0
var fin = 0.0 var gagnes = 0
var FOND = 0 var SOLC = 0 var MOI = 0 var LUI = 0 var GANT = 0 var VIDE = 0 var BLANC = 0

def manche()
  X[0] = 90.0 X[1] = 230.0 VX[0] = 0.0 VX[1] = 0.0
  VIE[0] = 10 VIE[1] = 10 COUP[0] = 0.0 COUP[1] = 0.0 fin = 0.0
end
def sens(a) return X[1 - a] > X[a] ? 1 : -1 end    # chaque robot regarde l'autre
def frapper(a)
  if COUP[a] <= 0 && SONNE[a] <= 0 COUP[a] = 0.3 TOUCHE[a] = false son(300, 30) end
end
def bouger(a, v, dt)
  if SONNE[a] > 0 || GARDE[a] || COUP[a] > 0 v = 0 end   # sonne, en garde ou en train de frapper : il ne marche pas
  X[a] += (v + VX[a]) * dt
  VX[a] -= VX[a] * 6 * dt                             # le recul s'amortit
  if X[a] < 20 X[a] = 20.0 elif X[a] > 300 X[a] = 300.0 end
end
def coup(a, dt)                                       # le poing est sorti entre 0.22 et 0.1 s restantes : il touche une seule fois
  if COUP[a] <= 0 return end
  COUP[a] -= dt
  var b = 1 - a
  if !TOUCHE[a] && COUP[a] < 0.22 && COUP[a] > 0.1 && math.abs(X[b] - X[a]) < 50
    TOUCHE[a] = true
    if GARDE[b] VX[b] = sens(a) * 80.0 son(1200, 30)   # parade : juste repousse
    else VIE[b] -= 1 SONNE[b] = 0.35 VX[b] = sens(a) * 220.0 son(150, 80)
    end
  end
end

def debut()
  FOND = rgb(40, 30, 70) SOLC = rgb(90, 70, 50) MOI = rgb(232, 50, 43) LUI = rgb(60, 140, 255)
  GANT = rgb(255, 220, 60) VIDE = rgb(70, 60, 90) BLANC = rgb(255, 255, 255)
  manche()
end

def maj(dt)
  if fin > 0                                          # fin de manche : on montre le resultat 2 s
    fin -= dt
    if fin <= 0 manche() end
    return
  end
  for a: 0 .. 1 if SONNE[a] > 0 SONNE[a] -= dt end end
  if appui(P3) frapper(0) end                         # toi : P3 tape = coup de poing, P3 tenu = garde
  if bouton(P3) tenu += dt else tenu = 0.0 end
  GARDE[0] = tenu > 0.35 && COUP[0] <= 0
  var v = 0
  if bouton(G) v -= 100 end
  if bouton(D) v += 100 end
  pense -= dt                                         # la console decide quoi faire toutes les 0,25 a 0,6 s
  var d = math.abs(X[1] - X[0])
  if pense <= 0
    pense = hasard(25, 60) / 100.0
    if d > 60 envie = hasard(0, 3) == 0 ? 0 : 1      # 0 = attendre, 1 = s'approcher
    else envie = hasard(2, 5)                         # 2, 3 = frapper, 4 = garde, 5 = reculer
    end
    if envie == 2 || envie == 3 frapper(1) end
  end
  GARDE[1] = envie == 4
  var v1 = 0
  if envie == 1 v1 = sens(1) * 80 elif envie == 5 v1 = -sens(1) * 80 end
  bouger(0, v, dt) bouger(1, v1, dt)
  if math.abs(X[1] - X[0]) < 28                       # ils ne passent pas l'un a travers l'autre
    var m = (X[0] + X[1]) / 2 var s = sens(0)
    X[0] = m - s * 14 X[1] = m + s * 14
  end
  coup(0, dt) coup(1, dt)
  if VIE[1] <= 0 gagnes += 1 score(gagnes) fin = 2.0 son(988, 300)
  elif VIE[0] <= 0 fin = 2.0 son(110, 400)
  end
end

def robot(a, c)
  var x = int(X[a]) var s = sens(a) var y = SOL
  if SONNE[a] > 0 x -= s * 3 end
  rect(x - 6, y - 12, 4, 12, c) rect(x + 2, y - 12, 4, 12, c)   # jambes
  rect(x - 10, y - 40, 20, 28, c)                                # corps
  rect(x - 8, y - 56, 16, 14, c)                                 # tete
  rect(x + s * 3 - 2, y - 52, 4, 4, BLANC)                       # oeil tourne vers l'autre
  if GARDE[a] rect(x + s * 12 - 3, y - 50, 6, 24, GANT)
  elif COUP[a] < 0.22 && COUP[a] > 0.1 rect(x + (s > 0 ? 10 : -34), y - 34, 24, 6, c) disque(x + s * 36, y - 31, 6, GANT)
  else disque(x + s * 14, y - 30, 6, GANT)
  end
end
def barre(x, n, c) rect(x, 10, 120, 10, VIDE) rect(x, 10, n * 12, 10, c) end

def dessine()
  fond(FOND)
  rect(0, SOL, 320, 40, SOLC)
  robot(0, MOI) robot(1, LUI)
  barre(10, VIE[0], MOI) barre(190, VIE[1], LUI)
  texte(154, 8, str(gagnes), BLANC, 2)
  if fin > 0 texte(97, 100, VIE[0] > 0 ? "GAGNE !" : "PERDU", BLANC, 3) end
end
