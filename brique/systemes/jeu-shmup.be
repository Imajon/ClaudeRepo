# Vaisseau sur fond qui defile
# exemple: JEU vaisseau sur un fond qui defile (G monte, D descend, P3 tire vers la droite, les ennemis arrivent de la droite, etoiles en 2 couches, 3 vies) | mots: shmup, vaisseau, defile, defilement, scrolling, horizontal, fond, etoiles, tir, tirer, tire, ennemis, espace, avion, shoot em up, scroll, starfield, enemies, spaceship, plane
var y = 110.0 var pts = 0 var vies = 3 var fin = false var inv = 0.0 var prochain = 1.0 var rythme = 1.0
var BX = [] var BY = []                             # 5 tirs : BX < 0 = libre
var EX = [] var EY = [] var EP = [] var EA = []     # 6 ennemis : EP = phase de l'ondulation
var SX = [] var SY = []                             # 24 etoiles

def reset()
  y = 110.0 pts = 0 vies = 3 fin = false inv = 0.0 prochain = 1.0 rythme = 1.0
  for i: 0 .. 4 BX[i] = -10.0 end
  for i: 0 .. 5 EA[i] = false end
end
def debut()
  for i: 0 .. 4 BX.push(-10.0) BY.push(0.0) end
  for i: 0 .. 5 EX.push(0.0) EY.push(0.0) EP.push(0.0) EA.push(false) end
  for i: 0 .. 23 SX.push(hasard(0, 319)) SY.push(hasard(0, 239)) end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) y -= 130 * dt end
  if bouton(D) y += 130 * dt end
  if y < 8 y = 8.0 elif y > 232 y = 232.0 end
  if inv > 0 inv -= dt end
  for i: 0 .. 23 SX[i] -= (i % 2 == 0 ? 30 : 80) * dt if SX[i] < 0 SX[i] += 320 end end   # 2 couches : lente et rapide
  if appui(P3)
    for i: 0 .. 4 if BX[i] < 0 BX[i] = 50.0 BY[i] = y son(1200, 20) break end end
  end
  for i: 0 .. 4 if BX[i] >= 0 BX[i] += 320 * dt if BX[i] > 320 BX[i] = -10.0 end end end
  rythme += 0.02 * dt prochain -= dt * rythme
  if prochain <= 0
    prochain = 1.0
    for i: 0 .. 5 if !EA[i] EA[i] = true EX[i] = 330.0 EY[i] = real(hasard(20, 220)) EP[i] = real(hasard(0, 6)) break end end
  end
  for i: 0 .. 5
    if EA[i]
      EX[i] -= (60 + pts * 2) * dt EP[i] += 3 * dt
      var dy = EY[i] - y var dx = EX[i] - 34
      if EX[i] < -20 EA[i] = false
      elif inv <= 0 && dx * dx + dy * dy < 300 EA[i] = false vies -= 1 inv = 1.5 son(150, 200) end
      for j: 0 .. 4
        var ex = BX[j] - EX[i] var ey = BY[j] - EY[i]
        if BX[j] >= 0 && EA[i] && ex * ex + ey * ey < 200 EA[i] = false BX[j] = -10.0 pts += 1 score(pts) son(440, 50) end
      end
    end
  end
  if vies <= 0 fin = true son(200, 300) end
end

def dessine()
  fond(rgb(8, 8, 28))
  for i: 0 .. 23 var c = i % 2 == 0 ? 90 : 220 rect(int(SX[i]), SY[i], 2, 2, rgb(c, c, c + 30)) end
  for i: 0 .. 5
    if EA[i] var ey = int(EY[i] + 0) disque(int(EX[i]), ey, 9, rgb(120, 200, 90)) disque(int(EX[i]) - 3, ey - 2, 2, rgb(20, 20, 20)) end
  end
  for i: 0 .. 4 if BX[i] >= 0 rect(int(BX[i]), int(BY[i]) - 1, 8, 3, rgb(255, 220, 60)) end end
  if inv <= 0 || int(temps() * 10) % 2 == 0
    var iy = int(y)
    tri(52, iy, 28, iy - 10, 28, iy + 10, rgb(232, 50, 43)) rect(24, iy - 3, 8, 6, rgb(255, 160, 40))
  end
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), rgb(255, 255, 255), 2)
  if fin texte(80, 100, "PERDU !", rgb(255, 220, 60), 4) texte(90, 150, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
