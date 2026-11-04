# Adversaire
# exemple: SYSTEME adversaire joue par la console (il decide toutes les 0,25 a 0,6 s selon la distance : approcher, attaquer, se proteger, reculer ; un coup touche une seule fois pendant une courte fenetre ; recul et etourdissement) | mots: adversaire, ordinateur, contre la console, ennemi intelligent, boss, duel, combat, bagarre, poing, frapper, coup, garde, ninja, robot, karate, poursuit, fantome
import math
var X = [90.0, 230.0] var VIE = [5, 5] var COUP = [0.0, 0.0] var TOUCHE = [false, false] var SONNE = [0.0, 0.0] var GARDE = [false, false]
var pense = 0.0 var envie = 0 var tenu = 0.0

def sens(a) return X[1 - a] > X[a] ? 1 : -1 end    # chacun regarde l'autre
def frapper(a) if COUP[a] <= 0 && SONNE[a] <= 0 COUP[a] = 0.3 TOUCHE[a] = false son(300, 30) end end
def coup(a, dt)                                     # le poing touche entre 0.22 et 0.1 s restantes, une seule fois
  if COUP[a] <= 0 return end
  COUP[a] -= dt
  var b = 1 - a
  if !TOUCHE[a] && COUP[a] < 0.22 && COUP[a] > 0.1 && math.abs(X[b] - X[a]) < 50
    TOUCHE[a] = true
    if GARDE[b] X[b] += sens(a) * 6 son(1200, 30)   # garde : juste repousse
    else VIE[b] -= 1 SONNE[b] = 0.35 X[b] += sens(a) * 20 son(150, 80) end
  end
end

def maj(dt)
  for a: 0 .. 1 if SONNE[a] > 0 SONNE[a] -= dt end end
  if appui(P3) frapper(0) end                       # P3 tape = coup, P3 tenu = garde
  if bouton(P3) tenu += dt else tenu = 0.0 end
  GARDE[0] = tenu > 0.35 && COUP[0] <= 0
  if bouton(G) X[0] -= 100 * dt end
  if bouton(D) X[0] += 100 * dt end
  pense -= dt                                       # la console ne decide pas a chaque image : elle "reflechit"
  var d = math.abs(X[1] - X[0])
  if pense <= 0
    pense = hasard(25, 60) / 100.0
    if d > 60 envie = hasard(0, 3) == 0 ? 0 : 1    # loin : 0 = attendre, 1 = s'approcher
    else envie = hasard(2, 5) end                   # pres : 2, 3 = frapper, 4 = garde, 5 = reculer
    if envie == 2 || envie == 3 frapper(1) end
  end
  GARDE[1] = envie == 4
  if SONNE[1] <= 0 && COUP[1] <= 0
    if envie == 1 X[1] += sens(1) * 80 * dt elif envie == 5 X[1] -= sens(1) * 80 * dt end
  end
  for a: 0 .. 1 if X[a] < 20 X[a] = 20.0 elif X[a] > 300 X[a] = 300.0 end end
  coup(0, dt) coup(1, dt)
  if VIE[0] <= 0 || VIE[1] <= 0 VIE[0] = 5 VIE[1] = 5 X[0] = 90.0 X[1] = 230.0 end
end

def dessine()
  fond(rgb(40, 30, 70))
  for a: 0 .. 1
    var x = int(X[a]) var s = sens(a) var c = a == 0 ? rgb(232, 50, 43) : rgb(60, 140, 255)
    rect(x - 10, 150, 20, 50, c)
    if COUP[a] < 0.22 && COUP[a] > 0.1 rect(x + (s > 0 ? 10 : -34), 160, 24, 6, c) end   # bras tendu
    if GARDE[a] rect(x + s * 12 - 3, 150, 6, 24, rgb(255, 220, 60)) end
    rect(a == 0 ? 10 : 190, 10, VIE[a] * 24, 10, c)
  end
end
