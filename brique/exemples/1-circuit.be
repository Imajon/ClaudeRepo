# Circuit
# exemple: Course sur circuit (vue de dessus, camera qui suit, piste en boucle) | mots: course, circuit, voiture, piste, route, kart, moto, derape, virage, tours
import math
import string
var P = [[160, 120], [420, 90], [620, 160], [680, 340], [560, 480], [340, 430], [200, 520], [60, 420], [40, 240]]
var LARG = 34
var N = 0
var BG = []
var BD = []
var px = 0.0 var py = 0.0 var ang = 0.0 var vit = 0.0
var seg = 0
var tours = 0
var chrono = 0.0
var record = 0.0
var qx = 0.0 var qy = 0.0
var ROUTE = 0 var HERBE = 0 var BLANC = 0 var ROUGE = 0

def debut()
  N = size(P)
  for i: 0 .. N - 1
    var a = P[i] var b = P[(i + 1) % N]
    var dx = b[0] - a[0] var dy = b[1] - a[1]
    var l = math.sqrt(dx * dx + dy * dy)
    var nx = -dy / l * LARG var ny = dx / l * LARG
    BG.push([int(a[0] + nx), int(a[1] + ny), int(b[0] + nx), int(b[1] + ny)])
    BD.push([int(a[0] - nx), int(a[1] - ny), int(b[0] - nx), int(b[1] - ny)])
  end
  px = real(P[0][0]) py = real(P[0][1])
  ang = math.atan2(P[1][1] - P[0][1], P[1][0] - P[0][0])
  ROUTE = rgb(70, 70, 80) HERBE = rgb(40, 120, 50) BLANC = rgb(255, 255, 255) ROUGE = rgb(232, 50, 43)
end

def dist2(i)
  var a = P[i] var b = P[(i + 1) % N]
  var dx = b[0] - a[0] var dy = b[1] - a[1]
  var u = ((px - a[0]) * dx + (py - a[1]) * dy) / (dx * dx + dy * dy + 0.0)
  if u < 0 u = 0.0 elif u > 1 u = 1.0 end
  qx = a[0] + u * dx qy = a[1] + u * dy
  var ex = px - qx var ey = py - qy
  return ex * ex + ey * ey
end

def maj(dt)
  if bouton(G) ang -= 2.6 * dt end
  if bouton(D) ang += 2.6 * dt end
  if bouton(P3) vit += 160 * dt else vit -= 90 * dt end
  if vit < 0 vit = 0.0 elif vit > 190 vit = 190.0 end
  px += math.cos(ang) * vit * dt
  py += math.sin(ang) * vit * dt
  var best = 99999999.0 var bi = seg
  for k: -2 .. 2
    var i = (seg + k + N) % N
    var d = dist2(i)
    if d < best best = d bi = i end
  end
  if bi == 0 && seg == N - 1
    tours += 1 score(tours) son(988, 120)
    if record == 0 || chrono < record record = chrono end
    chrono = 0.0
  elif bi == N - 1 && seg == 0
    tours -= 1
  end
  seg = bi
  dist2(seg)
  var lim = LARG - 6
  if best > lim * lim
    var d = math.sqrt(best)
    px = qx + (px - qx) / d * (lim - 1)
    py = qy + (py - qy) / d * (lim - 1)
    vit = vit * 0.5
    son(120, 40)
  end
  chrono += dt
end

def dessine()
  fond(HERBE)
  camera(int(px) - 160, int(py) - 120)
  for i: 0 .. N - 1 disque(P[i][0], P[i][1], LARG, ROUTE) end
  for i: 0 .. N - 1
    var g = BG[i] var d = BD[i]
    tri(g[0], g[1], g[2], g[3], d[2], d[3], ROUTE)
    tri(g[0], g[1], d[2], d[3], d[0], d[1], ROUTE)
    ligne(g[0], g[1], g[2], g[3], BLANC)
    ligne(d[0], d[1], d[2], d[3], BLANC)
  end
  ligne(BG[0][0], BG[0][1], BD[0][0], BD[0][1], BLANC)
  var c = math.cos(ang) var s = math.sin(ang)
  var x = int(px) var y = int(py)
  tri(x + int(c * 10), y + int(s * 10), x + int(-c * 7 - s * 6), y + int(-s * 7 + c * 6), x + int(-c * 7 + s * 6), y + int(-s * 7 - c * 6), ROUGE)
  camera(0, 0)
  texte(4, 4, "Tours " + str(tours), BLANC, 2)
  texte(4, 24, string.format("%.1f s", chrono), BLANC, 2)
  if record > 0 texte(200, 4, string.format("Rec %.1f", record), BLANC, 2) end
end
