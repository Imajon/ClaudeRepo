# Attrape
# exemple: JEU attraper ce qui tombe (le personnage reste en bas et attrape les bons objets, evite les mauvais, 3 vies, P3 pour rejouer) | mots: attraper, attrape, attrape-tout, panier, tombe, tomber, tombent, pluie, pomme, pommes, fruits, bon, mauvais, bombe, catch, catching, falling, basket, rain, fruit, bomb
var x = 140.0 var pts = 0 var vies = 3 var fin = false var rythme = 1.0 var prochain = 0.5
var OX = [] var OY = [] var OB = []                  # 8 objets : OY < 0 = libre, OB = bon (true) ou mauvais (false)

def reset()
  x = 140.0 pts = 0 vies = 3 fin = false rythme = 1.0 prochain = 0.5
  for i: 0 .. 7 OY[i] = -20.0 end
end
def debut() for i: 0 .. 7 OX.push(0.0) OY.push(-20.0) OB.push(true) end end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) x -= 200 * dt end
  if bouton(D) x += 200 * dt end
  if x < 0 x = 0.0 elif x > 280 x = 280.0 end
  rythme += 0.03 * dt prochain -= dt * rythme       # de plus en plus d'objets
  if prochain <= 0
    prochain = 1.0
    for i: 0 .. 7
      if OY[i] < 0 OX[i] = real(hasard(10, 300)) OY[i] = 0.0 OB[i] = hasard(0, 3) > 0 break end   # 3 sur 4 sont bons
    end
  end
  for i: 0 .. 7
    if OY[i] >= 0
      OY[i] += (70 + pts * 2) * dt                  # tombe de plus en plus vite
      if OY[i] > 204 && OX[i] > x - 4 && OX[i] < x + 44       # dans le panier
        OY[i] = -20.0
        if OB[i] pts += 1 score(pts) son(880, 40) else vies -= 1 son(150, 150) end
      elif OY[i] > 240
        OY[i] = -20.0
        if OB[i] vies -= 1 son(150, 150) end        # un bon objet rate coute une vie
      end
    end
  end
  if vies <= 0 fin = true son(200, 300) end
end

def dessine()
  fond(rgb(40, 90, 160))
  rect(0, 120, 320, 120, rgb(60, 120, 180))
  for i: 0 .. 7
    if OY[i] >= 0
      var ox = int(OX[i]) var oy = int(OY[i])
      if OB[i] disque(ox, oy, 8, rgb(240, 200, 60)) disque(ox - 3, oy - 3, 2, rgb(255, 250, 200))
      else disque(ox, oy, 8, rgb(50, 50, 60)) rect(ox - 1, oy - 12, 2, 5, rgb(230, 80, 60)) end
    end
  end
  rect(int(x), 214, 40, 14, rgb(170, 110, 60)) rect(int(x) - 2, 212, 44, 4, rgb(210, 150, 90))
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), rgb(255, 255, 255), 2)
  if fin texte(80, 100, "PERDU !", rgb(255, 220, 60), 4) texte(90, 150, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
