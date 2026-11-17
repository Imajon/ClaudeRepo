# Tamagotchi
# exemple: JEU s'occuper d'un personnage (3 jauges faim, joie, sommeil qui baissent ; G le nourrit, P3 joue avec lui, D le fait dormir ; il grandit avec le temps, gagne a 45 secondes) | mots: tamagotchi, tamagochi, animal, personnage, s'occuper, occuper, soigner, nourrir, manger, dormir, jouer, jauge, jauges, faim, grandir, grandit, bebe, pet, care, feed, sleep, play, hunger, gauge, baby, grow
import math
var J = [80.0, 80.0, 80.0]                          # faim, joie, sommeil : 0 = vide, 100 = plein
var VIT = [6.0, 8.0, 5.0]                           # vitesse a laquelle chaque jauge baisse (par seconde)
var age = 0.0 var fin = 0 var tact = 0.0            # fin : 0 jeu, 1 perdu, 2 gagne

def reset() J = [80.0, 80.0, 80.0] age = 0.0 fin = 0 tact = 0.0 end

def maj(dt)
  if fin > 0
    if appui(P3) reset() end
    return
  end
  age += dt tact -= dt
  for i: 0 .. 2 J[i] -= VIT[i] * dt if J[i] <= 0 fin = 1 son(150, 400) end end
  if appui(G) J[0] += 30 tact = 0.4 son(660, 60) end
  if appui(P3) J[1] += 30 tact = 0.4 son(880, 60) end
  if appui(D) J[2] += 30 tact = 0.4 son(440, 60) end
  for i: 0 .. 2 if J[i] > 100 J[i] = 100.0 end end
  if age >= 45 fin = 2 score(1) son(1200, 400) end
end

def dessine()
  var bl = rgb(255, 255, 255)
  fond(rgb(255, 235, 200))
  rect(0, 190, 320, 50, rgb(200, 230, 170))
  var mini = J[0] for i: 1 .. 2 if J[i] < mini mini = J[i] end end
  var taille = 20 + int(age / 45 * 24)               # il grandit avec le temps
  var cx = 160 var cy = 170 - taille / 2 + int(math.sin(temps() * 4) * 2)
  disque(cx, cy, taille, rgb(250, 200, 90))
  disque(cx - taille / 3, cy - taille / 4, 3 + taille / 12, rgb(40, 30, 20)) disque(cx + taille / 3, cy - taille / 4, 3 + taille / 12, rgb(40, 30, 20))
  if mini > 35 rect(cx - taille / 4, cy + taille / 3, taille / 2, 3, rgb(40, 30, 20)) else rect(cx - taille / 4, cy + taille / 2, taille / 2, 3, rgb(40, 30, 20)) end   # sourit ou fait la tete
  if tact > 0 texte(cx + taille, cy - taille, "<3", rgb(230, 60, 90), 2) end
  var NOM = ["G manger", "P3 jouer", "D dormir"] var COUL = [[230, 120, 60], [240, 190, 40], [90, 120, 220]]
  for i: 0 .. 2
    var k = COUL[i]
    rect(20 + i * 100, 200, 80, 10, rgb(220, 210, 190)) rect(20 + i * 100, 200, int(J[i] * 0.8), 10, rgb(k[0], k[1], k[2]))
    texte(20 + i * 100, 214, NOM[i], rgb(80, 60, 40), 1)
  end
  rect(0, 232, int(age / 45 * 320), 8, rgb(90, 190, 90))
  if fin == 1 texte(80, 60, "OUPS !", rgb(230, 60, 60), 4) end
  if fin == 2 texte(60, 60, "IL A GRANDI !", rgb(60, 150, 60), 3) end
  if fin > 0 texte(90, 100, "P3 : rejouer", rgb(80, 60, 40), 2) end
end
