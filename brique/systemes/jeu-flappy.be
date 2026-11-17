# Flappy
# exemple: JEU flappy (P3 bat des ailes, l'oiseau tombe tout seul, passer entre des tuyaux qui defilent, un point par tuyau passe) | mots: flappy, flappy bird, oiseau, voler, vol, ailes, tuyau, tuyaux, passer, trou, gravite, un seul bouton, flap, bird, fly, pipe, pipes, gap, one button
var y = 110.0 var vy = 0.0 var pts = 0 var fin = false
var PX = [] var PG = [] var PS = []                 # 3 tuyaux : x, hauteur du haut du trou, deja compte

def reset()
  y = 110.0 vy = 0.0 pts = 0 fin = false
  for i: 0 .. 2 PX[i] = 320.0 + i * 120 PG[i] = hasard(50, 150) PS[i] = false end
end
def debut() for i: 0 .. 2 PX.push(0.0) PG.push(0) PS.push(false) end reset() end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if appui(P3) vy = -210.0 son(700, 20) end
  vy += 700 * dt y += vy * dt
  if y < 6 y = 6.0 vy = 0.0 end
  if y > 234 fin = true son(150, 300) end
  for i: 0 .. 2
    PX[i] -= 90 * dt
    if PX[i] < -40 PX[i] += 360 PG[i] = hasard(50, 150) PS[i] = false end   # le tuyau sorti revient a droite
    if !PS[i] && PX[i] + 30 < 80 PS[i] = true pts += 1 score(pts) son(988, 40) end
    if 88 > PX[i] && 72 < PX[i] + 30 && (y - 6 < PG[i] || y + 6 > PG[i] + 80) fin = true son(150, 300) end   # dans le tuyau, hors du trou
  end
end

def dessine()
  fond(rgb(110, 200, 230))
  rect(0, 226, 320, 14, rgb(200, 170, 90))
  for i: 0 .. 2
    var x = int(PX[i])
    rect(x, 0, 30, PG[i], rgb(70, 170, 70)) rect(x - 3, PG[i] - 8, 36, 8, rgb(50, 140, 60))
    rect(x, PG[i] + 80, 30, 146 - PG[i], rgb(70, 170, 70)) rect(x - 3, PG[i] + 80, 36, 8, rgb(50, 140, 60))
  end
  var iy = int(y)
  disque(80, iy, 8, rgb(250, 210, 50)) disque(83, iy - 3, 2, rgb(20, 20, 20)) tri(87, iy, 95, iy + 2, 87, iy + 4, rgb(240, 120, 40))
  texte(150, 8, str(pts), rgb(255, 255, 255), 4)
  if fin texte(80, 100, "PERDU !", rgb(255, 80, 60), 4) texte(90, 150, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
