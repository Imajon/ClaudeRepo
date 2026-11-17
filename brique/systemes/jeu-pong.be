# Pong
# exemple: JEU pong simple (ma raquette a gauche : G monte, D descend ; la console joue a droite ; balle carree ou ronde avec ROND ; premier a 5 gagne) | mots: pong, tennis, ping-pong, ping pong, raquette, balle, carre, carree, ronde, rond, adversaire, duel, match, paddle, ball, square, round, opponent
var ROND = false                                    # true : balle ronde, false : balle carree
var ry = 100.0 var oy = 100.0 var bx = 160.0 var by = 120.0 var vx = 150.0 var vy = 90.0
var moi_pts = 0 var lui = 0 var fin = 0

def serve(sens)
  bx = 160.0 by = 120.0 vx = 150.0 * sens vy = real(hasard(-100, 100))
end
def reset() ry = 100.0 oy = 100.0 moi_pts = 0 lui = 0 fin = 0 serve(1) end
def debut() reset() end

def maj(dt)
  if fin > 0
    if appui(P3) reset() end
    return
  end
  if bouton(G) ry -= 180 * dt end
  if bouton(D) ry += 180 * dt end
  if ry < 0 ry = 0.0 elif ry > 190 ry = 190.0 end
  if by > oy + 25 oy += 110 * dt elif by < oy + 25 oy -= 110 * dt end   # la console suit la balle, un peu moins vite
  if oy < 0 oy = 0.0 elif oy > 190 oy = 190.0 end
  bx += vx * dt by += vy * dt
  if by < 4 by = 4.0 vy = -vy elif by > 236 by = 236.0 vy = -vy end
  if vx < 0 && bx < 22 && bx > 10 && by > ry - 4 && by < ry + 54
    bx = 22.0 vx = -vx * 1.05 vy = (by - ry - 25) * 5 son(900, 20)   # l'angle depend de l'endroit touche
  end
  if vx > 0 && bx > 298 && bx < 310 && by > oy - 4 && by < oy + 54
    bx = 298.0 vx = -vx * 1.05 vy = (by - oy - 25) * 5 son(900, 20)
  end
  if bx < -6 lui += 1 son(150, 200) serve(1)
  elif bx > 326 moi_pts += 1 score(moi_pts) son(988, 100) serve(-1) end
  if moi_pts >= 5 fin = 2 elif lui >= 5 fin = 1 end
end

def dessine()
  fond(rgb(15, 25, 20))
  for i: 0 .. 11 rect(158, i * 20 + 4, 4, 10, rgb(60, 90, 70)) end   # filet
  rect(10, int(ry), 8, 50, rgb(255, 255, 255)) rect(302, int(oy), 8, 50, rgb(230, 120, 100))
  if ROND disque(int(bx), int(by), 5, rgb(255, 220, 60)) else rect(int(bx) - 4, int(by) - 4, 8, 8, rgb(255, 220, 60)) end
  texte(120, 8, str(moi_pts), rgb(255, 255, 255), 3) texte(184, 8, str(lui), rgb(230, 120, 100), 3)
  if fin == 1 texte(80, 100, "PERDU !", rgb(255, 80, 60), 4) end
  if fin == 2 texte(80, 100, "BRAVO !", rgb(255, 220, 60), 4) end
  if fin > 0 texte(90, 150, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
