# Derapage
# exemple: SYSTEME derapage et murs solides (vitesse decoupee avant / cote, adherence qui baisse avec la vitesse, traces au sol, sous-pas de 2 px, rebond amorti sur des segments, etincelles) | mots: derape, derapage, drift, glisse, glace, rallye, kart, voiture, rebond, mur, bord, pilote, crash
import math
# Murs = segments x1, y1, x2, y2 (4 nombres chacun). Avec une piste : les murs sont ses bords LX/LY et RX/RY, en ne testant que les troncons proches du joueur.
var MURS = [10, 10, 310, 10,  310, 10, 310, 230,  310, 230, 10, 230,  10, 230, 10, 10,  110, 70, 210, 170,  220, 60, 260, 60]
var x = 60.0 var y = 180.0 var vx = 0.0 var vy = 0.0 var cap = -1.0
var R = 5.0                                    # rayon de la voiture
var TX = [] var TY = [] var tn = 0             # traces au sol : liste tournante de taille fixe
var EX = [] var EY = [] var EVX = [] var EVY = [] var EV = []   # etincelles
var ei = 0

def debut()
  var i = 0 while i < 60 TX.push(0) TY.push(0) i += 1 end
  i = 0 while i < 8 EX.push(0.0) EY.push(0.0) EVX.push(0.0) EVY.push(0.0) EV.push(0.0) i += 1 end
end
def mur(ax, ay, bx, by)                        # cercle contre segment : repousse hors du trait, rebond amorti
  if x < math.min(ax, bx) - R || x > math.max(ax, bx) + R || y < math.min(ay, by) - R || y > math.max(ay, by) + R return end   # test rapide
  var ex = bx - ax var ey = by - ay
  var u = ((x - ax) * ex + (y - ay) * ey) / (ex * ex + ey * ey + 0.001)
  if u < 0 u = 0.0 elif u > 1 u = 1.0 end
  var qx = ax + ex * u var qy = ay + ey * u var dx = x - qx var dy = y - qy var d2 = dx * dx + dy * dy
  if d2 >= R * R || d2 < 0.0001 return end
  var d = math.sqrt(d2) var nx = dx / d var ny = dy / d
  x += nx * (R - d) y += ny * (R - d)
  var vn = vx * nx + vy * ny
  if vn < 0
    vx -= 1.35 * vn * nx vy -= 1.35 * vn * ny vx *= 0.86 vy *= 0.86   # garde 35 % de la vitesse d'impact
    if vn < -60
      son(200, 20)
      var k = 0 while k < 3 EX[ei] = qx EY[ei] = qy EVX[ei] = hasard(-80, 80) EVY[ei] = hasard(-80, 80) EV[ei] = 0.3 ei = (ei + 1) % 8 k += 1 end
    end
  end
end

def maj(dt)
  var sp = math.sqrt(vx * vx + vy * vy) var k = math.min(1.0, sp / 260)
  if bouton(G) cap -= (3.9 - 1.3 * k) * dt end   # tourner change le cap, pas la vitesse
  if bouton(D) cap += (3.9 - 1.3 * k) * dt end
  var hx = math.cos(cap) var hy = math.sin(cap)
  var av = vx * hx + vy * hy var cote = -vx * hy + vy * hx   # vitesse vers l'avant / de cote
  if bouton(P3) av += 300 * (1 - 0.5 * k) * dt else av -= math.min(math.abs(av), 140 * dt) * (av > 0 ? 1 : -1) end
  if av > 260 av = 260.0 elif av < -30 av = -30.0 end
  cote *= math.max(0.0, 1 - (9 - 6.6 * k) * dt)   # adherence : forte lentement, faible vite -> la voiture glisse
  vx = hx * av - hy * cote vy = hy * av + hx * cote
  if math.abs(cote) > 40 && sp > 70 TX[tn] = int(x) TY[tn] = int(y) tn = (tn + 1) % 60 end   # trace au sol
  var ns = int(math.sqrt(vx * vx + vy * vy) * dt / 2) + 1   # sous-pas de 2 px : vite, on ne traverse jamais un mur
  var s = 0
  while s < ns
    x += vx * dt / ns y += vy * dt / ns
    var i = 0 var n = size(MURS)
    while i < n mur(MURS[i], MURS[i + 1], MURS[i + 2], MURS[i + 3]) i += 4 end
    s += 1
  end
  var i = 0
  while i < 8 if EV[i] > 0 EV[i] -= dt EX[i] += EVX[i] * dt EY[i] += EVY[i] * dt end i += 1 end
end

def dessine()
  fond(rgb(14, 14, 24))
  var i = 0 var gris = rgb(60, 58, 76)
  while i < 60 if TX[i] > 0 rect(TX[i], TY[i], 2, 2, gris) end i += 1 end
  var n = size(MURS) var c = rgb(80, 230, 255)
  i = 0 while i < n ligne(MURS[i], MURS[i + 1], MURS[i + 2], MURS[i + 3], c) i += 4 end
  i = 0 while i < 8 if EV[i] > 0 rect(int(EX[i]), int(EY[i]), 2, 2, rgb(255, 220, 120)) end i += 1 end
  var hx = math.cos(cap) var hy = math.sin(cap) var ix = int(x) var iy = int(y)
  tri(ix + int(hx * 10), iy + int(hy * 10), ix + int(-hx * 7 - hy * 6), iy + int(-hy * 7 + hx * 6), ix + int(-hx * 7 + hy * 6), iy + int(-hy * 7 - hx * 6), rgb(255, 255, 255))
end
