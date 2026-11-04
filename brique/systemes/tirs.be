# Tirs
# exemple: SYSTEME tirs et ennemis (listes de taille fixe reutilisees, apparition de plus en plus rapide, collision par distance au carre) | mots: tir, tirer, ennemi, ennemis, meteorite, balle, laser, apparait, vague, esquive, eviter, tombe, monstre, zombie, vaisseau, cible, espace
var NB = 6 var NE = 8
var BX = [] var BY = [] var EX = [] var EY = [] var EA = []
var x = 152.0 var rythme = 1.0 var prochain = 1.0

def debut()                                         # listes creees une fois ; BY < 0 = balle libre, EA = ennemi actif
  for i: 0 .. NB - 1 BX.push(0.0) BY.push(-10.0) end
  for i: 0 .. NE - 1 EX.push(0.0) EY.push(0.0) EA.push(false) end
end

def maj(dt)
  if bouton(G) x -= 160 * dt end
  if bouton(D) x += 160 * dt end
  if x < 0 x = 0.0 elif x > 304 x = 304.0 end
  if appui(P3)
    for i: 0 .. NB - 1
      if BY[i] < 0 BX[i] = x + 7 BY[i] = 212.0 son(1200, 20) break end   # prend la premiere balle libre
    end
  end
  for i: 0 .. NB - 1 if BY[i] >= 0 BY[i] -= 300 * dt end end
  rythme += 0.05 * dt prochain -= dt * rythme      # de plus en plus d'ennemis
  if prochain <= 0
    prochain = 1.0
    for i: 0 .. NE - 1
      if !EA[i] EA[i] = true EX[i] = real(hasard(10, 310)) EY[i] = -10.0 break end
    end
  end
  for i: 0 .. NE - 1
    if EA[i]
      EY[i] += 60 * dt
      if EY[i] > 250 EA[i] = false end
      for j: 0 .. NB - 1
        var dx = BX[j] - EX[i] var dy = BY[j] - EY[i]
        if BY[j] >= 0 && dx * dx + dy * dy < 100 EA[i] = false BY[j] = -10.0 son(440, 50) end   # 100 = 10 pixels au carre, pas de sqrt
      end
    end
  end
end

def dessine()
  fond(rgb(10, 10, 30))
  for i: 0 .. NE - 1 if EA[i] disque(int(EX[i]), int(EY[i]), 8, rgb(150, 120, 100)) end end
  for i: 0 .. NB - 1 if BY[i] >= 0 rect(int(BX[i]), int(BY[i]), 2, 6, rgb(255, 220, 60)) end end
  tri(int(x) + 8, 210, int(x), 228, int(x) + 16, 228, rgb(232, 50, 43))
end
