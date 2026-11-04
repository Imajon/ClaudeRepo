# Meteorites
# exemple: Esquive et tir (objets qui tombent, tirs vers le haut, listes de taille fixe, difficulte qui monte) | mots: meteorite, esquive, eviter, tombe, pluie, tir, tirer, vaisseau, espace, asteroide, balle, laser
var x = 152.0
var BX = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
var BY = [-10.0, -10.0, -10.0, -10.0, -10.0, -10.0]
var MX = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
var MY = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
var MR = [0, 0, 0, 0, 0, 0, 0, 0]
var MA = [false, false, false, false, false, false, false, false]
var vitm = 50.0 var prochaine = 0.0 var pts = 0
var perdu = false var attente = 0.0
var NOIR = 0 var BLANC = 0 var ROUGE = 0 var GRIS = 0 var JAUNE = 0

def nouvelle()
  for i: 0 .. 7 MA[i] = false end
  for i: 0 .. 5 BY[i] = -10.0 end
  x = 152.0 vitm = 50.0 prochaine = 0.5 pts = 0 perdu = false
end

def debut()
  NOIR = rgb(10, 10, 30) BLANC = rgb(255, 255, 255) ROUGE = rgb(232, 50, 43) GRIS = rgb(150, 120, 100) JAUNE = rgb(255, 220, 60)
  nouvelle()
end

def maj(dt)
  if perdu
    attente -= dt
    if attente <= 0 && appui(P3) nouvelle() end
  else
    if bouton(G) x -= 160 * dt end
    if bouton(D) x += 160 * dt end
    if x < 0 x = 0.0 elif x > 304 x = 304.0 end
    if appui(P3)
      for i: 0 .. 5
        if BY[i] < 0 BX[i] = x + 7 BY[i] = 212.0 son(1200, 20) break end
      end
    end
    for i: 0 .. 5
      if BY[i] >= 0 BY[i] -= 300 * dt end
    end
    vitm += 3 * dt
    prochaine -= dt
    if prochaine <= 0
      for i: 0 .. 7
        if !MA[i] MA[i] = true MX[i] = real(hasard(10, 310)) MY[i] = -10.0 MR[i] = hasard(6, 14) break end
      end
      prochaine = 60.0 / vitm
    end
    for i: 0 .. 7
      if MA[i]
        MY[i] += vitm * dt
        if MY[i] > 250 MA[i] = false end
        for j: 0 .. 5
          if BY[j] >= 0
            var dx = BX[j] - MX[i] var dy = BY[j] - MY[i]
            if dx * dx + dy * dy < MR[i] * MR[i] MA[i] = false BY[j] = -10.0 pts += 1 score(pts) son(440, 50) end
          end
        end
        var ex = x + 8 - MX[i] var ey = 220 - MY[i] var r = MR[i] + 7
        if MA[i] && ex * ex + ey * ey < r * r
          perdu = true attente = 0.8 son(120, 400)
        end
      end
    end
  end
end

def dessine()
  fond(NOIR)
  for i: 0 .. 7
    if MA[i] disque(int(MX[i]), int(MY[i]), MR[i], GRIS) end
  end
  for i: 0 .. 5
    if BY[i] >= 0 rect(int(BX[i]), int(BY[i]), 2, 6, JAUNE) end
  end
  tri(int(x) + 8, 210, int(x), 228, int(x) + 16, 228, ROUGE)
  texte(4, 4, str(pts), BLANC, 2)
  if perdu texte(70, 100, "Perdu !", BLANC, 4) texte(60, 150, "P3 pour rejouer", BLANC, 2) end
end
