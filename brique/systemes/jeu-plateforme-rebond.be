# Plateforme qui rebondit
# exemple: JEU plateforme qui rebondit (le personnage rebondit tout seul sur les plateformes, G et D le deplacent, trampolines qui rebondissent plus haut, l'ecran monte avec lui, tomber en bas = perdu) | mots: rebondit, rebondir, rebond, rebondissant, rebondissante, trampoline, doodle, doodle jump, monter, grimper, hauteur, plateforme, plateformes, saute tout seul, bounce, bouncing, bounces, trampoline, climb, height, platform, platforms
var px = 150.0 var py = 200.0 var vy = -420.0 var haut = 0 var fin = false
var PX = [] var PY = [] var PT = []                 # 8 plateformes espacees de 30 : PT = trampoline

def reset()
  px = 150.0 py = 200.0 vy = -420.0 haut = 0 fin = false
  for i: 0 .. 7 PX[i] = real(hasard(0, 260)) PY[i] = 230.0 - i * 30 PT[i] = false end
  PX[0] = 130.0
end
def debut() for i: 0 .. 7 PX.push(0.0) PY.push(0.0) PT.push(false) end reset() end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) px -= 140 * dt end
  if bouton(D) px += 140 * dt end
  if px < -10 px = 320.0 elif px > 320 px = -10.0 end   # sort a droite, revient a gauche
  vy += 900 * dt
  var ny = py + vy * dt
  if vy > 0
    for i: 0 .. 7
      if px + 10 > PX[i] && px < PX[i] + 60 && py <= PY[i] && ny >= PY[i]
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
  fond(rgb(150, 210, 240))
  for i: 0 .. 7
    var c = PT[i] ? rgb(240, 120, 60) : rgb(80, 170, 80)
    rect(int(PX[i]), int(PY[i]), 60, 8, c) rect(int(PX[i]), int(PY[i]) + 6, 60, 2, rgb(50, 120, 50))
  end
  var x = int(px) var y = int(py)
  rect(x, y - 16, 10, 16, rgb(232, 50, 43)) rect(x + 2, y - 13, 2, 3, rgb(255, 255, 255)) rect(x + 6, y - 13, 2, 3, rgb(255, 255, 255))
  texte(6, 6, str(haut), rgb(255, 255, 255), 2)
  if fin texte(80, 100, "PERDU !", rgb(255, 80, 60), 4) texte(90, 150, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
