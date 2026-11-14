# Bonus
# exemple: SYSTEME briques a plusieurs coups et bonus (chaque brique a ses points de vie, l'acier ne casse jamais ; une capsule tombe parfois, l'attraper change une regle pendant quelques secondes) | mots: bonus, capsule, pouvoir, power-up, briques, casse-briques, resistant, plusieurs coups, acier, indestructible, raquette geante, multi, pendant quelques secondes, brick, breakout, tough, several hits, steel, giant paddle, for a few seconds
var BR = []                                         # vie de chaque brique : 0 = cassee, 9 = acier (jamais)
var bx = 160.0 var by = 150.0 var vx = 90.0 var vy = -140.0 var rx = 130.0 var rl = 60
var cx = 0.0 var cy = -1.0 var geant = 0.0           # capsule (cy < 0 = aucune), temps restant du bonus
var COUL = []

def debut()
  COUL = [0, rgb(232, 50, 43), rgb(240, 140, 40), rgb(255, 220, 60)]
  for i: 0 .. 23 BR.push(i < 8 ? 3 : i < 16 ? 2 : 1) end
  BR[10] = 9 BR[13] = 9
end

def maj(dt)
  if bouton(G) rx -= 200 * dt end
  if bouton(D) rx += 200 * dt end
  if geant > 0 geant -= dt end
  rl = geant > 0 ? 100 : 60                         # la regle changee par le bonus
  if rx < 0 rx = 0.0 elif rx > 320 - rl rx = real(320 - rl) end
  bx += vx * dt by += vy * dt
  if bx < 4 bx = 4.0 vx = -vx elif bx > 316 bx = 316.0 vx = -vx end
  if by < 4 by = 4.0 vy = -vy end
  if vy > 0 && by > 216 && by < 226 && bx > rx && bx < rx + rl by = 216.0 vy = -vy vx = (bx - rx - rl / 2) * 5 son(900, 20) end
  if by > 250 bx = 160.0 by = 150.0 vy = -140.0 end
  var c = int(bx / 40) var r = int((by - 20) / 14)
  if by >= 20 && r < 3 && c >= 0 && c < 8 && BR[r * 8 + c] > 0
    var i = r * 8 + c
    vy = -vy
    if BR[i] == 9 son(1500, 10)                     # acier
    else
      BR[i] -= 1 son(1200, 20)
      if BR[i] == 0 && cy < 0 && hasard(0, 3) == 0 cx = c * 40 + 12.0 cy = r * 14 + 20.0 end   # 1 fois sur 4, une capsule
    end
  end
  if cy >= 0
    cy += 70 * dt
    if cy > 214 && cy < 226 && cx + 16 > rx && cx < rx + rl cy = -1.0 geant = 6.0 son(1320, 150)
    elif cy > 240 cy = -1.0 end
  end
end

def dessine()
  fond(rgb(20, 20, 40))
  for i: 0 .. 23
    if BR[i] > 0 rect((i % 8) * 40 + 1, 20 + (i / 8) * 14, 38, 12, BR[i] == 9 ? rgb(150, 150, 170) : COUL[BR[i]]) end
  end
  if cy >= 0 rect(int(cx), int(cy), 16, 8, rgb(80, 200, 255)) end
  rect(int(rx), 220, rl, 6, rgb(255, 255, 255))
  disque(int(bx), int(by), 4, rgb(255, 255, 255))
  if geant > 0 texte(4, 4, "GEANT " + str(int(geant) + 1), rgb(80, 200, 255), 1) end
end
