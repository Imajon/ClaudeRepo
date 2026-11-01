# Drift : on conduit dans un circuit vu de dessus. P3 tenu = accelerer, G / D = tourner. La voiture derape dans les virages pris vite.
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
def depart()
  x = P[0][0] + 20.0 y = P[0][1] + 0.0 vx = 0.0 vy = 0.0 cap = 0.0 t0 = temps() seg = 0 moitie = false
end
def debut() depart() end
def proche(px, py)              # segment le plus proche : [indice, distance, point x, point y]
  var best = [0, 99999.0, 0.0, 0.0]
  for i: 0 .. N - 1
    var a = P[i] var b = P[(i + 1) % N]
    var dx = b[0] - a[0] var dy = b[1] - a[1]
    var u = ((px - a[0]) * dx + (py - a[1]) * dy) / real(dx * dx + dy * dy)
    if u < 0 u = 0.0 elif u > 1 u = 1.0 end
    var qx = a[0] + dx * u var qy = a[1] + dy * u
    var d = math.sqrt((px - qx) * (px - qx) + (py - qy) * (py - qy))
    if d < best[1] best = [i, d, qx, qy] end
  end
  return best
end
def maj(dt)
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
  if p[1] > LARG - 5                           # bord : on revient sur la piste, rebond amorti
    var nx = (x - p[2]) / p[1] var ny = (y - p[3]) / p[1]
    x = p[2] + nx * (LARG - 5) y = p[3] + ny * (LARG - 5)
    var vn = vx * nx + vy * ny
    if vn > 0 vx -= 1.6 * vn * nx vy -= 1.6 * vn * ny vx *= 0.7 vy *= 0.7 son(180, 40) end
  end
  if p[0] > N / 2 moitie = true end            # un tour : passer la moitie puis revenir au debut
  if moitie && p[0] == 0 && seg == N - 1
    tour = temps() - t0 if record == 0 || tour < record record = tour end
    t0 = temps() moitie = false son(1319, 150)
  end
  seg = p[0]
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
  for i: 0 .. N - 1                            # bords de la piste
    var a = P[i] var b = P[(i + 1) % N]
    var dx = b[0] - a[0] var dy = b[1] - a[1] var l = math.sqrt(dx * dx + dy * dy)
    var nx = -dy / l * LARG var ny = dx / l * LARG
    ligne(int(a[0] + nx), int(a[1] + ny), int(b[0] + nx), int(b[1] + ny), bord)
    ligne(int(a[0] - nx), int(a[1] - ny), int(b[0] - nx), int(b[1] - ny), bord)
  end
  for k: 0 .. 6 rect(P[0][0] - 2, P[0][1] - LARG + k * 10, 5, 5, k % 2 ? rgb(255, 255, 255) : rgb(40, 40, 40)) end
  for t: traces rect(t[0], t[1], 2, 2, rgb(70, 70, 90)) end
  for id: autres.keys() var o = autres[id] voiture(o[0], o[1], o[2], rgb(255, 140, 40)) end
  voiture(x, y, cap, rgb(232, 50, 43))
  camera(0, 0)
  texte(4, 4, string.format("%.2f", temps() - t0), rgb(255, 255, 255), 2)
  if record > 0 texte(220, 4, string.format("record %.2f", record), rgb(255, 210, 60), 1) end
end
