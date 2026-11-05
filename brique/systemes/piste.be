# Piste
# exemple: SYSTEME piste decrite a la tortue (avance / virage au lieu de coordonnees, circuit ouvert ou en boucle, bords precalcules, ne dessiner que ce qui est a l'ecran, camera, rester sur la piste, arrivee au chrono) | mots: course, circuit, chrono, arrivee, voiture, piste, route, kart, moto, virage, epingle, spirale, zigzag, tours, riviere, couloir, slalom
import math
import string
# La piste se DECRIT comme une tortue : ne jamais deviner de coordonnees. ["a", longueur] = tout droit ; ["v", degres, rayon] = virage (+ a droite, - a gauche).
# Epingle = ["v", 180, 60] ; zigzag = virages alternes ; spirale = virages de meme sens au rayon qui diminue ; boucle = angles qui font 360 en tout.
var TRACE = [["a", 250], ["v", 90, 140], ["a", 150], ["v", -180, 70], ["a", 150], ["v", 180, 70], ["a", 200], ["v", -90, 120], ["a", 250]]
var LARG = 32                                  # demi-largeur ; un virage plus serre que LARG + 12 est agrandi tout seul
var CX = [] var CY = [] var LX = [] var LY = [] var RX = [] var RY = [] var BB = []   # centre, bords, boite de chaque troncon de 6 points
var N = 0
var px = 0.0 var py = 0.0 var ang = 0.0 var vit = 0.0 var seg = 0
var chrono = 0.0 var record = 0.0 var fini = false

def construit()                                # la tortue pose un point tous les 20 px environ
  var x = 0.0 var y = 0.0 var a = 0.0
  CX.push(x) CY.push(y)
  for c: TRACE
    var n = 0 var da = 0.0 var ds = 0.0
    if c[0] == "a" n = int(c[1] / 20) + 1 ds = c[1] / real(n)
    else
      var r = math.max(c[2], LARG + 12) var rad = c[1] * math.pi / 180
      n = int(math.abs(rad) * r / 20) + 2 da = rad / n ds = math.abs(rad) * r / n
    end
    for k: 1 .. n
      a += da / 2 x += math.cos(a) * ds y += math.sin(a) * ds a += da / 2
      CX.push(x) CY.push(y)
    end
  end
  N = size(CX)
  for k: 0 .. N - 1                            # bords : decales de LARG de part et d'autre de la direction locale
    var i = k > 0 ? k - 1 : 0 var j = k < N - 1 ? k + 1 : N - 1
    var dx = CX[j] - CX[i] var dy = CY[j] - CY[i] var l = math.sqrt(dx * dx + dy * dy)
    var nx = -dy / l * LARG var ny = dx / l * LARG
    LX.push(int(CX[k] + nx)) LY.push(int(CY[k] + ny)) RX.push(int(CX[k] - nx)) RY.push(int(CY[k] - ny))
  end
  var b = 0
  while b < N - 1                              # boite de chaque troncon : dessine() saute ceux hors de l'ecran
    var x0 = 99999 var y0 = 99999 var x1 = -99999 var y1 = -99999 var i = b
    while i <= b + 6 && i < N
      x0 = math.min(x0, LX[i], RX[i]) x1 = math.max(x1, LX[i], RX[i]) y0 = math.min(y0, LY[i], RY[i]) y1 = math.max(y1, LY[i], RY[i]) i += 1
    end
    BB.push(x0) BB.push(x1) BB.push(y0) BB.push(y1)
    b += 6
  end
end
def depart() px = CX[1] py = CY[1] ang = math.atan2(CY[2] - CY[0], CX[2] - CX[0]) vit = 0.0 seg = 1 chrono = 0.0 fini = false end
def debut() construit() depart() end

def maj(dt)
  if fini if appui(P3) depart() end return end
  if bouton(G) ang -= 2.8 * dt end
  if bouton(D) ang += 2.8 * dt end
  if bouton(P3) vit += 200 * dt else vit -= 120 * dt end
  if vit < 0 vit = 0.0 elif vit > 200 vit = 200.0 end
  px += math.cos(ang) * vit * dt py += math.sin(ang) * vit * dt
  chrono += dt
  var bd = 1e9 var qx = 0.0 var qy = 0.0       # segment du centre le plus proche, parmi les voisins de seg seulement
  var i = math.max(0, seg - 3) var fin = math.min(N - 2, seg + 4)
  while i <= fin
    var ax = CX[i] var ay = CY[i] var ex = CX[i + 1] - ax var ey = CY[i + 1] - ay
    var u = ((px - ax) * ex + (py - ay) * ey) / (ex * ex + ey * ey + 0.001)
    if u < 0 u = 0.0 elif u > 1 u = 1.0 end
    var x = ax + ex * u var y = ay + ey * u var d = (px - x) * (px - x) + (py - y) * (py - y)
    if d < bd bd = d qx = x qy = y seg = i end
    i += 1
  end
  var lim = LARG - 6
  if bd > lim * lim                            # sorti : ramene sur le bord (jamais bloque), freine
    var d = math.sqrt(bd) px = qx + (px - qx) / d * lim py = qy + (py - qy) / d * lim vit *= 0.6 son(140, 30)
  end
  if seg >= N - 3 fini = true if record == 0 || chrono < record record = chrono end son(1047, 200) end
end

def dessine()
  fond(rgb(40, 110, 50))
  var X0 = int(px) - 160 var Y0 = int(py) - 120
  camera(X0, Y0)
  var route = rgb(70, 70, 80) var bord = rgb(255, 255, 255)
  var b = 0 var nb = size(BB)
  while b < nb                                 # seulement les troncons a l'ecran
    if BB[b + 1] >= X0 && BB[b] <= X0 + 320 && BB[b + 3] >= Y0 && BB[b + 2] <= Y0 + 240
      var i = b / 4 * 6 var e = math.min(i + 6, N - 1)
      while i < e
        tri(LX[i], LY[i], LX[i + 1], LY[i + 1], RX[i + 1], RY[i + 1], route) tri(LX[i], LY[i], RX[i + 1], RY[i + 1], RX[i], RY[i], route)
        ligne(LX[i], LY[i], LX[i + 1], LY[i + 1], bord) ligne(RX[i], RY[i], RX[i + 1], RY[i + 1], bord)
        i += 1
      end
    end
    b += 4
  end
  ligne(LX[N - 1], LY[N - 1], RX[N - 1], RY[N - 1], rgb(232, 50, 43))   # arrivee
  var c = math.cos(ang) var s = math.sin(ang) var x = int(px) var y = int(py)
  tri(x + int(c * 10), y + int(s * 10), x + int(-c * 7 - s * 6), y + int(-s * 7 + c * 6), x + int(-c * 7 + s * 6), y + int(-s * 7 - c * 6), rgb(232, 50, 43))
  camera(0, 0)
  texte(4, 4, string.format("%.2f", chrono), bord, 2)
  if record > 0 texte(200, 4, string.format("rec %.2f", record), bord, 1) end
  if fini texte(70, 110, "ARRIVEE ! P3 = rejouer", bord, 2) end
end
