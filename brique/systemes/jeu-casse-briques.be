# Casse-briques
# exemple: JEU casse-briques (raquette G et D, P3 lance la balle, 3 rangees de briques de couleurs, 3 vies, gagne quand tout est casse) | mots: casse-briques, casse briques, casse, briques, brique, raquette, balle, breakout, arkanoid, casser, detruire, mur, brick, bricks, break, paddle, ball, smash
var bx = 160.0 var by = 210.0 var vx = 0.0 var vy = 0.0 var rx = 130.0 var colle = true
var pts = 0 var vies = 3 var fin = 0                 # fin : 0 jeu, 1 perdu, 2 gagne
var BR = []                                         # 3 rangees de 8 briques : true = encore la
var COUL = [[232, 50, 43], [240, 170, 50], [90, 190, 90]]

def reset()
  bx = 160.0 by = 210.0 vx = 0.0 vy = 0.0 rx = 130.0 colle = true pts = 0 vies = 3 fin = 0
  for i: 0 .. 23 BR[i] = true end
end
def debut() for i: 0 .. 23 BR.push(true) end end

def maj(dt)
  if fin > 0
    if appui(P3) reset() end
    return
  end
  if bouton(G) rx -= 220 * dt end
  if bouton(D) rx += 220 * dt end
  if rx < 0 rx = 0.0 elif rx > 260 rx = 260.0 end
  if colle
    bx = rx + 30 by = 214.0
    if appui(P3) colle = false vx = 80.0 vy = -170.0 end
    return
  end
  bx += vx * dt by += vy * dt
  if bx < 4 bx = 4.0 vx = -vx son(600, 10) elif bx > 316 bx = 316.0 vx = -vx son(600, 10) end
  if by < 4 by = 4.0 vy = -vy end
  if vy > 0 && by > 214 && by < 226 && bx > rx && bx < rx + 60
    by = 214.0 vy = -vy vx = (bx - rx - 30) * 5 son(900, 20)   # l'angle depend de l'endroit touche
  end
  if by > 245 vies -= 1 colle = true son(150, 200) if vies <= 0 fin = 1 end end
  var c = int(bx / 40) var r = int((by - 30) / 16)
  if by >= 30 && r < 3 && c >= 0 && c < 8 && BR[r * 8 + c]
    BR[r * 8 + c] = false vy = -vy pts += 10 score(pts) son(1200, 20)
    var reste = 0
    for i: 0 .. 23 if BR[i] reste += 1 end end
    if reste == 0 fin = 2 son(988, 300) end
  end
end

def dessine()
  fond(rgb(20, 20, 40))
  for i: 0 .. 23
    if BR[i] var k = COUL[i / 8] rect((i % 8) * 40 + 1, 30 + (i / 8) * 16, 38, 14, rgb(k[0], k[1], k[2])) end
  end
  rect(int(rx), 220, 60, 6, rgb(255, 255, 255))
  disque(int(bx), int(by), 4, rgb(255, 220, 60))
  for i: 1 .. vies disque(8 + i * 14, 12, 4, rgb(240, 70, 80)) end
  texte(250, 4, str(pts), rgb(255, 255, 255), 2)
  if fin == 1 texte(80, 120, "PERDU !", rgb(255, 220, 60), 4) end
  if fin == 2 texte(80, 120, "BRAVO !", rgb(255, 220, 60), 4) end
  if fin > 0 texte(90, 160, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
