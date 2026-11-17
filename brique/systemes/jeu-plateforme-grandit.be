# Plateforme grandit
# exemple: JEU plateforme ou l'avatar grandit (chaque fruit ramasse le rend plus gros, 4 tailles, G et D courent, P3 saute, devenir geant pour gagner) | mots: grandir, grandit, grossir, grossit, grand, geant, taille, evoluer, evolue, avatar, fruit, fruits, manger, plateforme, plateformes, saute, grow, grows, growing, bigger, giant, size, evolve, eat, platform
import math
var px = 20.0 var py = 200.0 var vy = 0.0 var sol = true var niv = 0 var fin = false
var PLX = [0, 70, 160, 250] var PLY = [200, 160, 125, 160] var PLW = [320, 70, 70, 60]   # sol + 3 plateformes
var FX = 100.0 var FY = 140.0                       # le fruit a ramasser

def place()                                         # un fruit sur une plateforme au hasard
  var i = hasard(1, 3)
  FX = PLX[i] + 30.0 FY = PLY[i] - 12.0
end
def reset() px = 20.0 py = 200.0 vy = 0.0 niv = 0 fin = false place() end
def debut() reset() end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  var w = 8 + niv * 5 var h = 12 + niv * 8          # le personnage grossit
  if bouton(G) px -= 110 * dt end
  if bouton(D) px += 110 * dt end
  if px < 0 px = 0.0 elif px > 320 - w px = 320.0 - w end
  if appui(P3) && sol vy = -340.0 sol = false son(660, 40) end
  vy += 900 * dt
  var ny = py + vy * dt
  sol = false
  if vy >= 0
    for i: 0 .. 3
      if px + w > PLX[i] && px < PLX[i] + PLW[i] && py <= PLY[i] && ny >= PLY[i] ny = real(PLY[i]) vy = 0.0 sol = true end
    end
  end
  py = ny
  if px + w > FX - 6 && px < FX + 6 && py > FY - 6 && py - h < FY + 6
    niv += 1 son(988, 100)
    if niv >= 3 fin = true score(niv) son(1200, 300) else place() end
  end
end

def dessine()
  fond(rgb(150, 210, 240))
  for i: 0 .. 3 rect(PLX[i], PLY[i], PLW[i], 8, rgb(90, 160, 90)) end
  if !fin disque(int(FX), int(FY) + int(math.sin(temps()) * 2), 6, rgb(230, 60, 70)) rect(int(FX), int(FY) - 9, 2, 4, rgb(60, 140, 60)) end
  var w = 8 + niv * 5 var h = 12 + niv * 8 var x = int(px) var y = int(py)
  rect(x, y - h, w, h, rgb(100 + niv * 40, 200 - niv * 30, 90))
  rect(x + w / 4, y - h + h / 4, 2 + niv, 2 + niv, rgb(255, 255, 255)) rect(x + w * 3 / 4 - 1, y - h + h / 4, 2 + niv, 2 + niv, rgb(255, 255, 255))
  texte(6, 6, "Taille " + str(niv + 1) + " / 4", rgb(255, 255, 255), 2)
  if fin texte(70, 70, "GEANT !", rgb(255, 220, 60), 4) texte(90, 110, "P3 : rejouer", rgb(40, 40, 60), 2) end
end
