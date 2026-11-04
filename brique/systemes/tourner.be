# Tourner
# exemple: SYSTEME tourner et avancer (G et D tournent, P3 accelere dans la direction, elan, tir dans la direction en lachant P3) | mots: tourner, avancer, tank, vaisseau, direction, angle, rotation, dessus, bateau, asteroide, viser, pivoter
import math
var x = 160.0 var y = 120.0 var ang = 0.0 var vit = 0.0
var bx = 0.0 var by = 0.0 var bvx = 0.0 var bvy = 0.0 var bvie = 0.0

def maj(dt)
  if bouton(G) ang -= 3 * dt end
  if bouton(D) ang += 3 * dt end
  if bouton(P3) vit += 200 * dt else vit -= 120 * dt end   # elan : accelere puis ralentit
  if vit < 0 vit = 0.0 elif vit > 120 vit = 120.0 end
  var c = math.cos(ang) var s = math.sin(ang)       # une fois par image, pas dans les boucles
  x += c * vit * dt y += s * vit * dt
  if x < 8 x = 8.0 elif x > 312 x = 312.0 end
  if y < 8 y = 8.0 elif y > 232 y = 232.0 end
  if lache(P3) && bvie <= 0 bx = x by = y bvx = c * 250 bvy = s * 250 bvie = 1.0 son(900, 20) end
  if bvie > 0 bvie -= dt bx += bvx * dt by += bvy * dt end
end

def dessine()
  fond(rgb(20, 30, 40))
  var c = math.cos(ang) var s = math.sin(ang)
  var ix = int(x) var iy = int(y)
  tri(ix + int(c * 10), iy + int(s * 10), ix + int(-c * 7 - s * 6), iy + int(-s * 7 + c * 6), ix + int(-c * 7 + s * 6), iy + int(-s * 7 - c * 6), rgb(232, 50, 43))   # triangle pointe vers ang
  if bvie > 0 disque(int(bx), int(by), 2, rgb(255, 220, 60)) end
end
