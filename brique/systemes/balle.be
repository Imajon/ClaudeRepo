# Balle
# exemple: SYSTEME balle qui rebondit (murs, raquette qui renvoie selon l'endroit touche, briques a casser) | mots: balle, rebond, rebondit, raquette, pong, tennis, casse-briques, briques, ping-pong, foot, ballon, palet, renvoyer, ball, bounce, paddle, racket, brick, breakout, football, soccer, puck
var bx = 160.0 var by = 120.0 var vx = 90.0 var vy = -140.0 var rx = 130.0
var BR = []                                         # 2 rangees de 8 briques : true = encore la

def debut() for i: 0 .. 15 BR.push(true) end end

def maj(dt)
  if bouton(G) rx -= 200 * dt end
  if bouton(D) rx += 200 * dt end
  if rx < 0 rx = 0.0 elif rx > 260 rx = 260.0 end
  bx += vx * dt by += vy * dt
  if bx < 4 bx = 4.0 vx = -vx son(600, 10) elif bx > 316 bx = 316.0 vx = -vx son(600, 10) end   # la remettre DANS le terrain puis inverser, sinon elle reste coincee
  if by < 4 by = 4.0 vy = -vy end
  if vy > 0 && by > 216 && by < 226 && bx > rx && bx < rx + 60
    by = 216.0 vy = -vy vx = (bx - rx - 30) * 5 son(900, 20)   # l'angle depend de l'endroit touche
  end
  if by > 250 bx = 160.0 by = 120.0 vy = -140.0 end
  var c = int(bx / 40) var r = int((by - 20) / 14)
  if by >= 20 && r < 2 && c >= 0 && c < 8 && BR[r * 8 + c] BR[r * 8 + c] = false vy = -vy son(1200, 20) end
end

def dessine()
  fond(rgb(20, 20, 40))
  for i: 0 .. 15 if BR[i] rect((i % 8) * 40 + 1, 20 + (i / 8) * 14, 38, 12, rgb(232, 50, 43)) end end
  rect(int(rx), 220, 60, 6, rgb(255, 255, 255))
  disque(int(bx), int(by), 4, rgb(255, 220, 60))
end
