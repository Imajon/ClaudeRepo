# Neon City
import math
# Un taxi dans une ville de bord de mer au coucher du soleil. G / D = tourner. Gros bouton (P3) tenu = gaz ; double appui = change de vitesse (avant / arriere) ; relache = la voiture ralentit.
# Prends le client (rond vert), depose-le (rond rose) vite : plus c'est rapide, plus tu gagnes. Chaque course attire des flics. 3 coeurs. 1000 $ = gagne.
var NX = 8 var NY = 6 var WW = 944 var WH = 688 var NT = 4 var VMAX = 165.0
var DX = [0, 1, 0, -1] var DY = [-1, 0, 1, 0]
var ROAD = 0 var DASH = 0 var SIDE = 0 var GRASS = 0 var PALM = 0 var TRUNK = 0 var SHAD = 0 var ROUGE = 0 var BLEU = 0 var BLANC = 0 var NOIR = 0
var PINK = 0 var VERT = 0 var GOLD = 0 var VITRE = 0 var JAUNE = 0 var FLIC = 0 var TAXI = 0 var GRIS = 0
var RC = [] var RL = [] var TC = [] var CIEL = []
var etat = 0 var tfin = 0.0 var T = 0.0
var x = 408.0 var y = 280.0 var h = 0.0 var v = 0.0 var camX = 0.0 var camY = 0.0 var inv = 0.0
var VR = 70.0 var g = 1 var sg = 1 var tap = -9.0
var pv = 3 var argent = 0 var fait = 0 var ph = 0 var tx = 0 var ty = 0 var tl = 0.0 var msg = "" var msgt = 0.0
# voitures : trafic (les NT premieres) puis flics. Position sur la ligne centrale de la route + decalage de voie
var CX = [] var CY = [] var CD = [] var GX = [] var GY = [] var CT = [] var CW = [] var CA = [] var LX = [] var LY = [] var CC = []

def ab(a) return a < 0 ? -a : a end
def d2(ax, ay, bx, by) return (ax - bx) * (ax - bx) + (ay - by) * (ay - by) end
def nivo() var n = 1 + fait / 2 return n > 3 ? 3 : n end

def solide(px, py)
  if px < 0 || py < 0 || px >= WW || py >= WH return true end
  return int(px) % 128 >= 48 && int(py) % 128 >= 48
end

# la voiture (nez et deux coins avant) est-elle sur la route en (px, py), cap h ?
def libre(px, py)
  var co = math.cos(h) * sg var si = math.sin(h) * sg
  var fx = px + co * 9 var fy = py + si * 9
  return !(solide(px, py) || solide(fx - si * 5, fy + co * 5) || solide(fx + si * 5, fy - co * 5))
end

def cible()
  var k = 0
  while k < 40
    k += 1
    tx = hasard(0, NX - 1) * 128 + 24 ty = hasard(0, NY - 1) * 128 + 24
    if d2(tx, ty, x, y) > 62500 break end
  end
end

# arrive a une intersection : trafic = au hasard (tout droit favorise), flic = le voisin le plus proche du joueur ; jamais demi-tour sauf cul-de-sac
def choisir(i)
  var best = -1 var bd = 0.0 var k = 0
  while k < 4
    if k != (CD[i] + 2) % 4
      var nx = GX[i] + DX[k] var ny = GY[i] + DY[k]
      if nx >= 0 && nx < NX && ny >= 0 && ny < NY
        var sc = 0.0
        if CT[i] == 1 sc = d2(nx * 128 + 24, ny * 128 + 24, x, y) else sc = hasard(0, 100) - (k == CD[i] ? 40 : 0) end
        if best < 0 || sc < bd best = k bd = sc end
      end
    end
    k += 1
  end
  if best < 0 best = (CD[i] + 2) % 4 end
  CD[i] = best GX[i] += DX[best] GY[i] += DY[best]
end

def spawn(t)
  var k = 0 var gx = 0 var gy = 0
  while k < 40
    k += 1
    gx = hasard(0, NX - 1) gy = hasard(0, NY - 1)
    if d2(gx * 128 + 24, gy * 128 + 24, x, y) > (t == 1 ? 90000 : 40000) break end
  end
  CX.push(gx * 128 + 24.0) CY.push(gy * 128 + 24.0) CD.push(hasard(0, 3)) GX.push(gx) GY.push(gy) CT.push(t) CW.push(0.0)
  CC.push(t == 1 ? FLIC : TC[hasard(0, 4)])
  var i = size(CX) - 1
  choisir(i)
  var d = CD[i]
  CA.push(math.atan2(DY[d], DX[d])) LX.push(-DY[d] * 9.0) LY.push(DX[d] * 9.0)
end

def reset()
  CX = [] CY = [] CD = [] GX = [] GY = [] CT = [] CW = [] CA = [] LX = [] LY = [] CC = []
  x = 408.0 y = 280.0 h = 0.0 v = 0.0 g = 1 sg = 1 tap = -9.0 pv = 3 argent = 0 fait = 0 ph = 0 inv = 0.0 T = 0.0
  var i = 0
  while i < NT spawn(0) i += 1 end
  cible()
  camX = x - 160 camY = y - 120
  msg = "PRENDS LE CLIENT !" msgt = 3.0
  score(0)
end

def debut()
  ROAD = rgb(44, 38, 62) DASH = rgb(255, 210, 70) SIDE = rgb(226, 196, 170) GRASS = rgb(40, 150, 90) PALM = rgb(20, 110, 60) TRUNK = rgb(120, 80, 50)
  SHAD = rgb(30, 24, 46) ROUGE = rgb(240, 40, 60) BLEU = rgb(40, 90, 255) BLANC = rgb(255, 255, 255) NOIR = rgb(10, 8, 20) GRIS = rgb(160, 150, 170)
  PINK = rgb(255, 70, 170) VERT = rgb(60, 240, 120) GOLD = rgb(255, 215, 60) VITRE = rgb(30, 40, 70) JAUNE = rgb(255, 245, 160) FLIC = rgb(235, 235, 245) TAXI = rgb(255, 205, 40)
  RC = [rgb(255, 90, 170), rgb(40, 210, 205), rgb(255, 150, 70), rgb(150, 100, 235), rgb(240, 240, 250)]
  RL = [rgb(255, 150, 210), rgb(130, 240, 235), rgb(255, 200, 130), rgb(200, 170, 255), rgb(200, 205, 225)]
  TC = [rgb(60, 120, 255), rgb(240, 90, 90), rgb(60, 200, 150), rgb(200, 90, 230), rgb(250, 250, 250)]
  CIEL = [rgb(24, 10, 70), rgb(52, 16, 100), rgb(100, 24, 130), rgb(170, 40, 140), rgb(240, 70, 130), rgb(255, 110, 110), rgb(255, 160, 80), rgb(255, 205, 100)]
  reset()
end

def hurt()
  if inv > 0 return end
  pv -= 1 inv = 1.2 v *= 0.3 son(120, 150)
  if pv <= 0 etat = 2 tfin = 0.0 score(argent) end
end

def voitures(dt)
  var i = 0
  while i < size(CX)
    var d = CD[i]
    if CW[i] > 0
      CW[i] -= dt
    else
      var vi = CT[i] == 1 ? 95.0 + 13 * nivo() : 55.0
      CX[i] += DX[d] * vi * dt CY[i] += DY[d] * vi * dt
      var ex = GX[i] * 128 + 24 var ey = GY[i] * 128 + 24
      if (ex - CX[i]) * DX[d] + (ey - CY[i]) * DY[d] <= 0
        CX[i] = ex + 0.0 CY[i] = ey + 0.0
        choisir(i)
        d = CD[i]
      end
    end
    var k = 8 * dt if k > 1 k = 1.0 end
    LX[i] += (-DY[d] * 9 - LX[i]) * k LY[i] += (DX[d] * 9 - LY[i]) * k
    var df = math.atan2(DY[d], DX[d]) - CA[i]
    while df > 3.14159 df -= 6.28318 end
    while df < -3.14159 df += 6.28318 end
    var st = 9 * dt
    if df > st df = st elif df < -st df = -st end
    CA[i] += df
    # choc avec le joueur
    if d2(CX[i] + LX[i], CY[i] + LY[i], x, y) < 150 && inv <= 0
      CW[i] = 1.0
      var bx = x - (CX[i] + LX[i]) var by = y - (CY[i] + LY[i])
      var l = math.sqrt(bx * bx + by * by) if l < 1 l = 1.0 end
      if libre(x + bx / l * 8, y + by / l * 8) x += bx / l * 8 y += by / l * 8 end
      hurt()
    end
    i += 1
  end
end

def conduit(dt)
  if appui(P3)                                   # double appui = change de vitesse (avant / arriere)
    if T - tap < 0.35 g = -g tap = -9.0 son(500, 30) else tap = T end
  end
  if bouton(P3)                                  # gros bouton tenu = gaz (dans la vitesse choisie ; a contre-sens = frein fort)
    if g > 0
      v += (v < 0 ? 420 : 220) * dt
      if v > VMAX v = VMAX end
    else
      v -= (v > 0 ? 420 : 140) * dt
      if v < -VR v = -VR end
    end
  elif v > 0
    v -= 130 * dt
    if v < 0 v = 0.0 end
  elif v < 0
    v += 130 * dt
    if v > 0 v = 0.0 end
  end
  sg = v < -1 ? -1 : (v > 1 ? 1 : g)             # sens de la marche : les points testes sont devant
  var s = 0
  if bouton(D) s += 1 end
  if bouton(G) s -= 1 end
  var oh = h
  h += s * 2.4 * (0.3 + 0.7 * ab(v) / VMAX) * (v < 0 ? -1 : 1) * dt
  if !libre(x, y) h = oh end
  var mx = x + math.cos(h) * v * dt var my = y + math.sin(h) * v * dt
  if libre(mx, my)
    x = mx y = my
  else
    if libre(mx, y)
      x = mx
    elif libre(x, my)
      y = my
    end
    v -= v * 3 * dt
  end
end

def mission(dt)
  if ph == 1
    tl -= dt
    if tl <= 0
      ph = 0 msg = "TROP TARD !" msgt = 2.0 son(150, 250) cible()
    end
  end
  if d2(tx, ty, x, y) < 900
    if ph == 0
      ph = 1 cible()
      tl = 6.0 + math.sqrt(d2(tx, ty, x, y)) * 1.3 / 100
      msg = "CLIENT A BORD !" msgt = 1.5 son(880, 80)
      if size(CX) - NT < nivo() spawn(1) end
    else
      var bonus = int(tl * 6) if bonus > 100 bonus = 100 end
      argent += 100 + bonus fait += 1
      if pv < 3 pv += 1 end
      msg = "+" + str(100 + bonus) + " $" msgt = 1.5 ph = 0 cible() son(1047, 120)
      if argent >= 1000 etat = 3 tfin = 0.0 score(argent) son(880, 400) end
    end
  end
end

def maj(dt)
  if dt > 0.05 dt = 0.05 end
  if etat != 1
    tfin += dt
    if appui(P3) && (etat == 0 || tfin > 1.0)
      if etat != 0 reset() end
      etat = 1
    end
    return
  end
  T += dt
  if inv > 0 inv -= dt end
  if msgt > 0 msgt -= dt end
  conduit(dt)
  voitures(dt)
  mission(dt)
  var k = 5 * dt if k > 1 k = 1.0 end
  var gx = x + math.cos(h) * v * 0.25 - 160 var gy = y + math.sin(h) * v * 0.25 - 120
  if gx < 0 gx = 0.0 elif gx > WW - 320 gx = WW - 320.0 end
  if gy < 0 gy = 0.0 elif gy > WH - 240 gy = WH - 240.0 end
  camX += (gx - camX) * k camY += (gy - camY) * k
end

# ---------- dessin ----------
def ombre(px, py, s, c, n) texte(px + 1, py + 1, s, NOIR, n) texte(px, py, s, c, n) end

def palme(px, py)
  disque(px + 2, py + 3, 6, SHAD)
  disque(px, py, 6, PALM) disque(px - 2, py - 1, 3, GRASS)
end

def bloc(sx, sy, t)
  rect(sx, sy, 80, 80, SIDE)
  if t == 5
    rect(sx + 4, sy + 4, 72, 72, GRASS)
    palme(sx + 18, sy + 20) palme(sx + 58, sy + 24) palme(sx + 30, sy + 58) palme(sx + 62, sy + 62)
  else
    rect(sx + 10, sy + 10, 66, 66, SHAD)
    rect(sx + 6, sy + 6, 66, 66, RC[t])
    rect(sx + 14, sy + 14, 22, 14, RL[t]) rect(sx + 44, sy + 40, 20, 24, RL[t])
    if t == 1 palme(sx + 4, sy + 74) end
  end
end

def ville()
  var ox = int(camX) var oy = int(camY)
  fond(ROAD)
  var gx = 0
  while gx < NX
    var sx = gx * 128 + 24 - ox
    if sx > -4 && sx < 324
      var yy = -(oy % 24)
      while yy < 240
        if (yy + oy) % 128 >= 48 rect(sx - 1, yy, 2, 10, DASH) end
        yy += 24
      end
    end
    gx += 1
  end
  var gy = 0
  while gy < NY
    var sy = gy * 128 + 24 - oy
    if sy > -4 && sy < 244
      var xx = -(ox % 24)
      while xx < 320
        if (xx + ox) % 128 >= 48 rect(xx, sy - 1, 10, 2, DASH) end
        xx += 24
      end
    end
    gy += 1
  end
  var by = 0
  while by < 5
    var bx = 0
    while bx < 7
      var sx = bx * 128 + 48 - ox var sy = by * 128 + 48 - oy
      if sx > -80 && sx < 320 && sy > -80 && sy < 240 bloc(sx, sy, (bx * 3 + by * 5 + bx * by) % 6) end
      bx += 1
    end
    by += 1
  end
end

def auto(cx, cy, a, c, k)
  var co = math.cos(a) var si = math.sin(a)
  var fx = co * 9 var fy = si * 9 var sx = -si * 5 var sy = co * 5
  tri(int(cx + fx + sx), int(cy + fy + sy), int(cx + fx - sx), int(cy + fy - sy), int(cx - fx - sx), int(cy - fy - sy), c)
  tri(int(cx + fx + sx), int(cy + fy + sy), int(cx - fx - sx), int(cy - fy - sy), int(cx - fx + sx), int(cy - fy + sy), c)
  disque(int(cx - co * 1.5), int(cy - si * 1.5), 3, VITRE)
  disque(int(cx + fx + sx * 0.6), int(cy + fy + sy * 0.6), 1, JAUNE) disque(int(cx + fx - sx * 0.6), int(cy + fy - sy * 0.6), 1, JAUNE)
  if k == 1 disque(int(cx), int(cy), 3, int(T * 8) % 2 == 0 ? ROUGE : BLEU) end
end

def marque()
  var mx = tx - int(camX) var my = ty - int(camY)
  var c = ph == 0 ? VERT : PINK
  if mx > -20 && mx < 340 && my > -20 && my < 260
    var r = 13 + int(3 * math.sin(T * 6))
    rect(mx - 1, my - 50, 3, 50, c)
    disque(mx, my, r, c) disque(mx, my, r - 5, ROAD)
    texte(mx - 3, my - 4, ph == 0 ? "P" : "$", c, 1)
  else
    var a = math.atan2(ty - y, tx - x) var co = math.cos(a) var si = math.sin(a)
    var ax = 160 + co * 104 var ay = 120 + si * 96
    tri(int(ax + co * 9), int(ay + si * 9), int(ax - co * 5 - si * 7), int(ay - si * 5 + co * 7), int(ax - co * 5 + si * 7), int(ay - si * 5 - co * 7), c)
  end
end

def hud()
  ombre(6, 5, "$" + str(argent), GOLD, 2)
  ombre(6, 218, g > 0 ? "AV" : "AR", g > 0 ? VERT : ROUGE, 2)
  var i = 0
  while i < 3
    rect(6 + i * 13, 24, 10, 10, i < pv ? ROUGE : SHAD)
    i += 1
  end
  var nf = size(CX) - NT
  i = 0
  while i < 3
    disque(262 + i * 18, 12, 6, i < nf ? GOLD : SHAD)
    i += 1
  end
  if ph == 1 ombre(250, 24, str(int(tl)) + " s", tl < 5 ? ROUGE : BLANC, 2) end
  if msgt > 0 ombre(160 - size(msg) * 6, 52, msg, BLANC, 2) end
  # carte
  rect(252, 186, 64, 48, NOIR)
  i = 0
  while i < NX ligne(256 + (i * 128 + 24) / 16, 188, 256 + (i * 128 + 24) / 16, 232, GRIS) i += 1 end
  i = 0
  while i < NY ligne(254, 190 + (i * 128 + 24) / 16, 314, 190 + (i * 128 + 24) / 16, GRIS) i += 1 end
  disque(256 + tx / 16, 190 + ty / 16, 3, ph == 0 ? VERT : PINK)
  i = NT
  while i < size(CX)
    disque(256 + int(CX[i]) / 16, 190 + int(CY[i]) / 16, 2, ROUGE)
    i += 1
  end
  disque(256 + int(x) / 16, 190 + int(y) / 16, 2, TAXI)
end

def palmier(px, py, n)
  ligne(px, py, px - n, py - 3 * n, NOIR) ligne(px + 1, py, px - n + 1, py - 3 * n, NOIR)
  var qx = px - n var qy = py - 3 * n
  ligne(qx, qy, qx - 4 * n, qy + n, NOIR) ligne(qx, qy, qx + 4 * n, qy + n, NOIR) ligne(qx, qy, qx - 2 * n, qy - 2 * n, NOIR) ligne(qx, qy, qx + 3 * n, qy - 2 * n, NOIR)
end

def ecran(titre, c, sous, t2)
  var i = 0
  while i < 8 rect(0, i * 17, 320, 17, CIEL[i]) i += 1 end
  disque(160, 136, 46, CIEL[7])
  rect(0, 136, 320, 104, rgb(18, 12, 50))
  i = 0
  while i < 5 rect(160 - 40 + i * 6, 142 + i * 8, 80 - i * 12, 3, rgb(255, 110 + i * 15, 130)) i += 1 end
  palmier(34, 200, 6) palmier(290, 206, 7)
  texte(160 - size(titre) * 12 + 3, 33, titre, rgb(40, 220, 215), 4)
  texte(160 - size(titre) * 12, 30, titre, c, 4)
  texte(160 - size(sous) * 3, 190, sous, BLANC, 1)
  texte(160 - size(t2) * 3, 214, t2, GRIS, 1)
end

def jeu()
  ville()
  marque()
  var ox = int(camX) var oy = int(camY)
  var i = 0
  while i < size(CX)
    var px = CX[i] + LX[i] - ox var py = CY[i] + LY[i] - oy
    if px > -20 && px < 340 && py > -20 && py < 260 auto(px, py, CA[i], CC[i], CT[i]) end
    i += 1
  end
  if inv <= 0 || int(T * 14) % 2 == 0 auto(x - ox, y - oy, h, TAXI, 0) end
  hud()
end

def dessine()
  if etat == 0
    ecran("NEON CITY", PINK, "G/D : tourner  P3 : gaz  P3 x2 : marche arriere", "Prends le client, depose-le, seme la police.   P3 : jouer")
  elif etat == 2
    ecran("ARRETE !", ROUGE, "$" + str(argent), "P3 : rejouer")
  elif etat == 3
    ecran("RICHE !", GOLD, "$" + str(argent) + " en " + str(int(T)) + " s", "P3 : rejouer")
  else
    jeu()
  end
end
