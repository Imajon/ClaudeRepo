# Drift : on conduit dans un circuit vu de dessus. P3 tenu = accelerer, G / D = tourner. La voiture derape dans les virages pris vite.
# Version optimisee : bords precalcules, recherche du segment proche limitee aux voisins.
# Toucher un bord : rebond et perte de vitesse. Un tour au chrono. A plusieurs : on voit les autres voitures (fantomes).
import math
import string
var P = [[80,700],[330,700],[530,675],[650,575],[650,425],[525,340],[325,330],[200,245],[212,120],[412,78],[650,100],[850,160],[950,300],[950,475],[990,650],[1150,700],[1300,600],[1320,350],[1250,120],[1050,40],[700,20],[300,30],[60,150],[40,450]]
var N = size(P)
var LARG = 34                    # demi-largeur de la piste
var x = 0.0 var y = 0.0 var vx = 0.0 var vy = 0.0 var cap = 0.0
var t0 = 0.0 var tour = 0.0 var record = 0.0 var seg = 0 var moitie = false
var traces = []                  # traces de derapage
var autres = {}                  # id -> [x, y, cap]
var envoiT = 0.0
var ips = 0 var ipsN = 0 var ipsT = 0.0   # images par seconde, affichees a l'ecran (diagnostic)
var BX = [] var BY = []         # bords precalcules : 4 nombres par segment (gauche x1 y1 x2 y2 puis droite)
var px_ = 0.0 var py_ = 0.0 var pd = 0.0
def depart()
  x = P[0][0] + 20.0 y = P[0][1] + 0.0 vx = 0.0 vy = 0.0 cap = 0.0 t0 = temps() seg = 0 moitie = false
end
def debut()
  for i: 0 .. N - 1
    var a = P[i] var b = P[(i + 1) % N]
    var dx = b[0] - a[0] var dy = b[1] - a[1] var l = math.sqrt(dx * dx + dy * dy)
    var nx = -dy / l * LARG var ny = dx / l * LARG
    BX.push(int(a[0] + nx)) BY.push(int(a[1] + ny)) BX.push(int(b[0] + nx)) BY.push(int(b[1] + ny))
    BX.push(int(a[0] - nx)) BY.push(int(a[1] - ny)) BX.push(int(b[0] - nx)) BY.push(int(b[1] - ny))
  end
  depart()
end
def proche(px, py)              # segment le plus proche parmi les voisins du segment actuel ; resultat dans px_, py_, pd ; renvoie l'indice
  var bi = seg var bd = 1e9
  for k: -2 .. 2
    var i = (seg + k + N) % N
    var a = P[i] var b = P[(i + 1) % N]
    var ax = a[0] var ay = a[1] var dx = b[0] - ax var dy = b[1] - ay
    var u = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy + 0.0)
    if u < 0 u = 0.0 elif u > 1 u = 1.0 end
    var qx = ax + dx * u var qy = ay + dy * u
    var d = (px - qx) * (px - qx) + (py - qy) * (py - qy)
    if d < bd bd = d bi = i px_ = qx py_ = qy end
  end
  pd = math.sqrt(bd)
  return bi
end
def maj(dt)
  ipsN += 1 ipsT += dt
  if ipsT >= 1 ips = ipsN ipsN = 0 ipsT = 0.0 end
  for m: recois()
    var v = string.split(m[1], " ")
    if size(v) == 3 autres[m[0]] = [int(v[0]), int(v[1]), real(v[2])] end
  end
  if bouton(G) cap -= 3.2 * dt end
  if bouton(D) cap += 3.2 * dt end
  var fx = math.cos(cap) var fy = math.sin(cap)
  var av = vx * fx + vy * fy                   # vitesse vers l'avant
  var la = -vx * fy + vy * fx                  # vitesse de cote (le derapage)
  if bouton(P3) av += 330 * dt else av -= 150 * dt end
  if av < 0 av = 0.0 elif av > 290 av = 290.0 end
  var adh = 9.0 - av / 45.0                    # moins d'adherence quand on va vite
  la = la * math.exp(-adh * dt)
  if math.abs(la) > 40 && size(traces) < 120 traces.push([int(x), int(y)]) end
  vx = av * fx - la * fy vy = av * fy + la * fx
  x += vx * dt y += vy * dt
  var p = proche(x, y)
  if pd > LARG - 5                             # bord : on revient sur la piste, rebond amorti
    var nx = (x - px_) / pd var ny = (y - py_) / pd
    x = px_ + nx * (LARG - 5) y = py_ + ny * (LARG - 5)
    var vn = vx * nx + vy * ny
    if vn > 0 vx -= 1.6 * vn * nx vy -= 1.6 * vn * ny vx *= 0.7 vy *= 0.7 son(180, 40) end
  end
  if p > N / 2 moitie = true end            # un tour : passer la moitie puis revenir au debut
  if moitie && p == 0 && seg == N - 1
    tour = temps() - t0 if record == 0 || tour < record record = tour end
    t0 = temps() moitie = false son(1319, 150)
  end
  seg = p
  if size(traces) > 110 traces.remove(0) end
  if temps() - envoiT > 0.04 envoiT = temps() envoie(string.format("%d %d %.2f", int(x), int(y), cap)) end
end
def voiture(px, py, a, c)
  var fx = math.cos(a) var fy = math.sin(a)
  tri(int(px + fx * 9), int(py + fy * 9), int(px - fx * 6 - fy * 6), int(py - fy * 6 + fx * 6), int(px - fx * 6 + fy * 6), int(py - fy * 6 - fx * 6), c)
end
def dessine()
  fond(rgb(18, 20, 34))
  camera(int(x) - 160, int(y) - 120)
  var bord = rgb(80, 230, 255)
  for i: 0 .. 2 * N - 1 var j = 2 * i ligne(BX[j], BY[j], BX[j + 1], BY[j + 1], bord) end   # bords de la piste
  for k: 0 .. 6 rect(P[0][0] - 2, P[0][1] - LARG + k * 10, 5, 5, k % 2 ? rgb(255, 255, 255) : rgb(40, 40, 40)) end
  for t: traces rect(t[0], t[1], 2, 2, rgb(70, 70, 90)) end
  for id: autres.keys() var o = autres[id] voiture(o[0], o[1], o[2], rgb(255, 140, 40)) end
  voiture(x, y, cap, rgb(232, 50, 43))
  camera(0, 0)
  texte(4, 4, string.format("%.2f", temps() - t0), rgb(255, 255, 255), 2)
  texte(4, 226, str(ips) + " img/s", rgb(255, 255, 0), 1)
  if record > 0 texte(220, 4, string.format("record %.2f", record), rgb(255, 210, 60), 1) end
end
