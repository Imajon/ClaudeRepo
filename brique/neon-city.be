# Neon City
import math
# Garage (dernier choix du menu) : vehicule, couleur, carte. Objets : caisses ? ramassees en roulant (turbo, bouclier, coeur, temps, obus).
# Modes (menu G / D, P3 = jouer) : TAXI (clients), COURSE / PERMIS (circuit, tours, rivaux, medailles or / argent / bronze), BATTLE (ballons : G + D ensemble = tirer un obus, caisses ? = munitions). Les scenarios sont dans la liste SC.
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
# scenarios : nom, description, mode (0 taxi, 1 course, 2 battle), tours, rivaux, vitesse des rivaux, circuit, temps or / argent / bronze (course seule)
var SC = [["TAXI", "Clients a deposer vite, 1000 $ = gagne", 0, 0, 0, 0, 0, 0, 0, 0], ["PERMIS", "1 tour chrono, petit circuit", 1, 1, 0, 0, 0, 11, 14, 19], ["PERMIS GT", "1 tour chrono, grand circuit", 1, 1, 0, 0, 1, 19, 24, 32], ["COURSE", "3 tours contre 3 rivaux", 1, 3, 3, 90, 0, 0, 0, 0], ["GRAND PRIX", "2 tours contre 5 rivaux rapides", 1, 2, 5, 115, 1, 0, 0, 0], ["BATTLE 2", "Fais eclater les ballons de 2 rivaux", 2, 0, 2, 70, 0, 3, 0, 0], ["BATTLE 4", "Fais eclater les ballons de 4 rivaux", 2, 0, 4, 85, 0, 2, 0, 0]]
var CIR = [[1, 1, 3, 3], [1, 1, 6, 4]]
var mode = 0 var sel = 0 var nt = 4 var tours = 0 var nriv = 0 var vriv = 0.0 var cx0 = 0 var cy0 = 0 var cx1 = 0 var cy1 = 0
# vehicules : nom, vitesse max, virage, acceleration
var VEH = [["TAXI", 165.0, 1.0, 220], ["SPORT", 190.0, 0.85, 260], ["KART", 150.0, 1.4, 300], ["4X4", 145.0, 1.2, 200]]
var THEMES = ["COUCHER", "NUIT", "NEIGE"] var VCOL = []
var ve = 0 var vc = 0 var tm = 0 var gl = 0 var acc = 220 var vir = 1.0 var turbo = 0.0
var ball = 3 var cp = 0 var rang = 1 var cd = 0.0 var mun = 0 var tir = 0.0 var kills = 0 var ft = "" var fs = "" var fc = 0
var CK = [] var BR = [] var OX = [] var OY = [] var OA = [] var OL = [] var OQ = [] var IX = [] var IY = []
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
        if CT[i] == 1 || CT[i] == 3 sc = d2(nx * 128 + 24, ny * 128 + 24, x, y) else sc = hasard(0, 100) - (k == CD[i] ? 40 : 0) end
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
    if d2(gx * 128 + 24, gy * 128 + 24, x, y) > (t == 1 || t == 3 ? 90000 : 40000) break end
  end
  CX.push(gx * 128 + 24.0) CY.push(gy * 128 + 24.0) CD.push(hasard(0, 3)) GX.push(gx) GY.push(gy) CT.push(t) CW.push(0.0)
  CC.push(t == 1 ? FLIC : (t == 3 ? RC[hasard(0, 3)] : TC[hasard(0, 4)])) CK.push(t == 3 ? ball : 0) BR.push(4.0 + hasard(0, 20) * 0.1)
  var i = size(CX) - 1
  choisir(i)
  var d = CD[i]
  CA.push(math.atan2(DY[d], DX[d])) LX.push(-DY[d] * 9.0) LY.push(DX[d] * 9.0)
end

# coins du circuit (0 = ligne de depart en haut a gauche, puis dans le sens des aiguilles d une montre)
def corx(k) return (k == 1 || k == 2 ? cx1 : cx0) * 128 + 24 end
def cory(k) return (k >= 2 ? cy1 : cy0) * 128 + 24 end

# rival de course : a chaque croisement il suit le tour du circuit ; compte les coins passes
def arrive(i)
  var nc = (CK[i] + 1) % 4
  if GX[i] * 128 + 24 == corx(nc) && GY[i] * 128 + 24 == cory(nc) CK[i] += 1 end
  var gx = GX[i] var gy = GY[i] var d = 0
  if gy == cy0 && gx < cx1 d = 1 elif gx == cx1 && gy < cy1 d = 2 elif gy == cy1 && gx > cx0 d = 3 end
  CD[i] = d GX[i] += DX[d] GY[i] += DY[d]
end

def rival(i)
  CX.push(cx0 * 128 + 46.0 + i * 22) CY.push(cy0 * 128 + 24.0) CD.push(1) GX.push(cx0 + 1) GY.push(cy0) CT.push(2) CW.push(0.0) CK.push(0) BR.push(0.0)
  CC.push(RC[i % 4]) CA.push(0.0) LX.push(0.0) LY.push(9.0)
end

def caisse(k)
  if mode == 1                                   # sur le circuit : un point au hasard d un des 4 cotes
    var c = hasard(0, 3) var w = (cx1 - cx0) * 128 var e = (cy1 - cy0) * 128
    if c == 0 IX[k] = cx0 * 128 + 24 + hasard(0, w) IY[k] = cy0 * 128 + 24 elif c == 1 IX[k] = cx1 * 128 + 24 IY[k] = cy0 * 128 + 24 + hasard(0, e) elif c == 2 IX[k] = cx0 * 128 + 24 + hasard(0, w) IY[k] = cy1 * 128 + 24 else IX[k] = cx0 * 128 + 24 IY[k] = cy0 * 128 + 24 + hasard(0, e) end
  else
    IX[k] = hasard(0, NX - 1) * 128 + 24 IY[k] = hasard(0, NY - 1) * 128 + 24
  end
end

# objet ramasse : turbo, bouclier, coeur, temps en plus (taxi avec client), obus (battle)
def objet()
  var r = hasard(0, mode == 2 ? 4 : 3)
  if mode == 1 r = 0 end
  if r == 0
    turbo = 3.0 msg = "TURBO !"
  elif r == 1
    inv = 5.0 msg = "BOUCLIER !"
  elif r == 2 || (r == 3 && mode == 0 && ph == 0)
    if pv < 3 pv += 1 end
    msg = "+1 COEUR"
  elif r == 3 && mode == 0
    tl += 8.0 msg = "+8 s"
  else
    if mun < 3 mun += 2 end
    if mun > 3 mun = 3 end
    msg = "OBUS +2"
  end
  msgt = 1.2 son(1200, 60)
end

def caisses()
  var i = 0
  while i < size(IX)
    if d2(IX[i], IY[i], x, y) < 500 objet() caisse(i) end
    i += 1
  end
end

def reset()
  var s = SC[sel]
  VMAX = VEH[ve][1] vir = VEH[ve][2] acc = VEH[ve][3] TAXI = VCOL[vc] turbo = 0.0
  mode = s[2] ball = s[7] tours = s[3] nriv = s[4] vriv = s[5] + 0.0 nt = mode == 0 ? NT : (mode == 2 ? 2 : 0)
  CX = [] CY = [] CD = [] GX = [] GY = [] CT = [] CW = [] CA = [] LX = [] LY = [] CC = [] CK = [] BR = [] OX = [] OY = [] OA = [] OL = [] OQ = [] IX = [0, 0, 0] IY = [0, 0, 0]
  x = 408.0 y = 280.0 h = 0.0 v = 0.0 g = 1 sg = 1 tap = -9.0 pv = 3 argent = 0 fait = 0 ph = 0 inv = 0.0 T = 0.0 cp = 0 rang = 1 cd = 0.0 mun = 2 tir = 0.0 kills = 0
  var i = 0
  if mode == 1
    var c = CIR[s[6]]
    cx0 = c[0] cy0 = c[1] cx1 = c[2] cy1 = c[3]
    x = cx0 * 128 + 30.0 y = cy0 * 128 + 15.0
    ph = 1 cd = 3.0 tx = corx(1) ty = cory(1)
    while i < nriv rival(i) i += 1 end
    msg = "PREPARE-TOI !"
  else
    while i < nt spawn(0) i += 1 end
    if mode == 0
      cible()
      msg = "PRENDS LE CLIENT !"
    else
      ph = 1 cd = 2.0
      i = 0
      while i < nriv spawn(3) i += 1 end
      msg = "BATTLE ! G + D = TIRER"
    end
  end
  i = 0
  if mode == 1 && nriv == 0 IX = [] IY = [] end   # permis : pas de caisses, le chrono reste une affaire d adresse
  while i < size(IX) caisse(i) i += 1 end
  camX = x - 160 camY = y - 120
  msgt = 3.0
  score(0)
end

# carte : couleurs de la ville (0 coucher de soleil, 1 nuit neon, 2 neige)
def theme(k)
  if k == 1
    ROAD = rgb(14, 12, 28) DASH = rgb(60, 230, 230) SIDE = rgb(40, 34, 70) GRASS = rgb(16, 60, 70) PALM = rgb(10, 90, 90) TRUNK = rgb(70, 50, 60) SHAD = rgb(6, 4, 14)
    RC = [rgb(255, 40, 150), rgb(0, 220, 255), rgb(255, 120, 0), rgb(160, 60, 255), rgb(120, 255, 120)]
    RL = [rgb(255, 120, 200), rgb(120, 240, 255), rgb(255, 180, 90), rgb(200, 140, 255), rgb(190, 255, 190)]
    CIEL = [rgb(4, 2, 20), rgb(10, 4, 34), rgb(20, 6, 50), rgb(36, 10, 70), rgb(60, 14, 90), rgb(90, 20, 100), rgb(130, 30, 110), rgb(170, 50, 120)]
  elif k == 2
    ROAD = rgb(70, 76, 96) DASH = rgb(255, 255, 255) SIDE = rgb(225, 235, 248) GRASS = rgb(190, 215, 235) PALM = rgb(120, 170, 200) TRUNK = rgb(120, 100, 90) SHAD = rgb(150, 165, 195)
    RC = [rgb(120, 170, 255), rgb(255, 150, 150), rgb(255, 210, 120), rgb(150, 210, 170), rgb(200, 170, 240)]
    RL = [rgb(200, 225, 255), rgb(255, 205, 205), rgb(255, 235, 180), rgb(205, 235, 215), rgb(230, 215, 250)]
    CIEL = [rgb(120, 150, 200), rgb(140, 170, 215), rgb(160, 190, 225), rgb(180, 205, 235), rgb(200, 220, 242), rgb(215, 230, 248), rgb(230, 240, 252), rgb(245, 248, 255)]
  else
    ROAD = rgb(44, 38, 62) DASH = rgb(255, 210, 70) SIDE = rgb(226, 196, 170) GRASS = rgb(40, 150, 90) PALM = rgb(20, 110, 60) TRUNK = rgb(120, 80, 50) SHAD = rgb(30, 24, 46)
    RC = [rgb(255, 90, 170), rgb(40, 210, 205), rgb(255, 150, 70), rgb(150, 100, 235), rgb(240, 240, 250)]
    RL = [rgb(255, 150, 210), rgb(130, 240, 235), rgb(255, 200, 130), rgb(200, 170, 255), rgb(200, 205, 225)]
    CIEL = [rgb(24, 10, 70), rgb(52, 16, 100), rgb(100, 24, 130), rgb(170, 40, 140), rgb(240, 70, 130), rgb(255, 110, 110), rgb(255, 160, 80), rgb(255, 205, 100)]
  end
end

def debut()
  ROUGE = rgb(240, 40, 60) BLEU = rgb(40, 90, 255) BLANC = rgb(255, 255, 255) NOIR = rgb(10, 8, 20) GRIS = rgb(160, 150, 170)
  PINK = rgb(255, 70, 170) VERT = rgb(60, 240, 120) GOLD = rgb(255, 215, 60) VITRE = rgb(30, 40, 70) JAUNE = rgb(255, 245, 160) FLIC = rgb(235, 235, 245)
  VCOL = [rgb(255, 205, 40), rgb(240, 40, 60), rgb(40, 110, 255), rgb(60, 220, 120), rgb(255, 90, 190), rgb(235, 235, 245)]
  TC = [rgb(60, 120, 255), rgb(240, 90, 90), rgb(60, 200, 150), rgb(200, 90, 230), rgb(250, 250, 250)]
  theme(tm)
  reset()
end

def finir(e, t, sous, c, pts)
  etat = e tfin = 0.0 ft = t fs = sous fc = c score(pts)
end

def hurt()
  if inv > 0 return end
  pv -= 1 inv = 1.2 v *= 0.3 son(120, 150)
  if pv <= 0 finir(2, "ARRETE !", mode == 0 ? "$" + str(argent) : str(kills) + " ballons", ROUGE, mode == 0 ? argent : kills * 100) end
end

def voitures(dt)
  var i = 0
  while i < size(CX)
    var d = CD[i]
    if CW[i] > 0
      CW[i] -= dt
    else
      var vi = 55.0
      if CT[i] == 1 vi = 95.0 + 13 * nivo() elif CT[i] >= 2 vi = vriv - (i % 3) * 6 end
      CX[i] += DX[d] * vi * dt CY[i] += DY[d] * vi * dt
      var ex = GX[i] * 128 + 24 var ey = GY[i] * 128 + 24
      if (ex - CX[i]) * DX[d] + (ey - CY[i]) * DY[d] <= 0
        CX[i] = ex + 0.0 CY[i] = ey + 0.0
        if CT[i] == 2 arrive(i) else choisir(i) end
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
    var ok = CT[i] >= 2 ? (CW[i] <= 0) : inv <= 0
    if ok && d2(CX[i] + LX[i], CY[i] + LY[i], x, y) < 150
      CW[i] = CT[i] == 2 ? 0.5 : 1.0
      var bx = x - (CX[i] + LX[i]) var by = y - (CY[i] + LY[i])
      var l = math.sqrt(bx * bx + by * by) if l < 1 l = 1.0 end
      if libre(x + bx / l * 8, y + by / l * 8) x += bx / l * 8 y += by / l * 8 end
      if CT[i] >= 2 v *= 0.5 else hurt() end
    end
    i += 1
  end
end

def conduit(dt)
  var vm = turbo > 0 ? VMAX * 1.35 : VMAX
  if appui(P3)                                   # double appui = change de vitesse (avant / arriere)
    if T - tap < 0.35 g = -g tap = -9.0 son(500, 30) else tap = T end
  end
  if bouton(P3)                                  # gros bouton tenu = gaz (dans la vitesse choisie ; a contre-sens = frein fort)
    if g > 0
      v += (v < 0 ? 420 : acc * (turbo > 0 ? 2 : 1)) * dt
      if v > vm v -= 400 * dt if v < vm v = vm end end
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
  h += s * 2.4 * vir * (0.3 + 0.7 * ab(v) / VMAX) * (v < 0 ? -1 : 1) * dt
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
      if size(CX) - nt < nivo() spawn(1) end
    else
      var bonus = int(tl * 6) if bonus > 100 bonus = 100 end
      argent += 100 + bonus fait += 1
      if pv < 3 pv += 1 end
      msg = "+" + str(100 + bonus) + " $" msgt = 1.5 ph = 0 cible() son(1047, 120)
      if argent >= 1000 finir(3, "RICHE !", "$" + str(argent) + " en " + str(int(T)) + " s", GOLD, argent) son(880, 400) end
    end
  end
end

def medaille(t)
  var s = SC[sel]
  if t <= s[7] return 0 elif t <= s[8] return 1 elif t <= s[9] return 2 end
  return 3
end

def course(dt)
  var nc = (cp + 1) % 4
  tx = corx(nc) ty = cory(nc)
  if d2(tx, ty, x, y) < 900
    cp += 1 nc = (cp + 1) % 4 tx = corx(nc) ty = cory(nc)
    son(660, 40)
    if cp % 4 == 0 && cp < 4 * tours msg = "TOUR " + str(cp / 4 + 1) msgt = 1.2 end
    if cp >= 4 * tours
      var r = 1 var i = 0
      while i < size(CX) if CK[i] >= 4 * tours r += 1 end i += 1 end
      rang = r
      var tt = str(int(T)) + "." + str(int(T * 10) % 10) + " s"
      var pts = int(6000 / (T + 1)) + (nriv + 1 - r) * 100
      var m = r - 1
      if nriv == 0 m = medaille(T) end
      var noms = ["OR !", "ARGENT !", "BRONZE !", "FINI !"]
      var cols = [GOLD, GRIS, rgb(230, 140, 70), BLANC]
      if m > 3 m = 3 end
      finir(3, noms[m], (nriv > 0 ? str(r) + "e sur " + str(nriv + 1) + "  -  " : "") + tt, cols[m], pts) son(880, 400)
      return
    end
  end
  if nriv == 0 && T > SC[sel][9]
    finir(2, "TROP LENT", "bronze : " + str(SC[sel][9]) + " s", ROUGE, 0)
    return
  end
  var pp = cp * 10000 - math.sqrt(d2(tx, ty, x, y)) var r = 1 var i = 0
  while i < size(CX)
    var nr = (CK[i] + 1) % 4
    if CK[i] * 10000 - ab(corx(nr) - CX[i]) - ab(cory(nr) - CY[i]) > pp r += 1 end
    i += 1
  end
  rang = r
end

def tire(px, py, a, q)
  OX.push(px + 0.0) OY.push(py + 0.0) OA.push(a) OL.push(1.5) OQ.push(q)
end

def touche(i)
  CK[i] -= 1 CW[i] = 1.2 son(300, 80)
  if CK[i] <= 0
    CT[i] = 9 CX[i] = -9999.0 CY[i] = -9999.0 CW[i] = 1000000.0 kills += 1 if pv < 3 pv += 1 end msg = "RIVAL OUT !" msgt = 1.5 son(1047, 120)
  end
end

def obus(dt)
  var j = 0
  while j < size(OX)
    var hit = false
    OX[j] += math.cos(OA[j]) * (OQ[j] == 0 ? 300 : 240) * dt OY[j] += math.sin(OA[j]) * (OQ[j] == 0 ? 300 : 240) * dt OL[j] -= dt
    if OL[j] <= 0 || solide(OX[j], OY[j])
      hit = true
    elif OQ[j] == 1
      if d2(OX[j], OY[j], x, y) < 120 hit = true hurt() end
    else
      var i = nt var bi = -1 var bdd = 19600
      while i < size(CX)
        if CT[i] == 3
          var dd = d2(OX[j], OY[j], CX[i] + LX[i], CY[i] + LY[i])
          if dd < 220 hit = true touche(i) end
          if dd < bdd bdd = dd bi = i end
        end
        i += 1
      end
      if bi >= 0 && !hit                         # l'obus se courbe un peu vers le rival proche (visee facile)
        var df = math.atan2(CY[bi] + LY[bi] - OY[j], CX[bi] + LX[bi] - OX[j]) - OA[j]
        while df > 3.14159 df -= 6.28318 end
        while df < -3.14159 df += 6.28318 end
        if ab(df) < 1.2 OA[j] += df > 0 ? (df > 5 * dt ? 5 * dt : df) : (df < -5 * dt ? -5 * dt : df) end
      end
    end
    if hit
      OX.pop(j) OY.pop(j) OA.pop(j) OL.pop(j) OQ.pop(j)
    else
      j += 1
    end
  end
end

def bataille(dt)
  if tir > 0 tir -= dt end
  if mun > 0 && tir <= 0 && bouton(G) && bouton(D)
    tire(x + math.cos(h) * 14, y + math.sin(h) * 14, h, 0) mun -= 1 tir = 0.5 son(900, 40)
  end
  obus(dt)
  var i = 0
  var n = 0 var bd = 1000000000
  i = nt
  while i < size(CX)
    if CT[i] == 3
      n += 1
      var bx = CX[i] + LX[i] var by = CY[i] + LY[i]
      var dd = d2(bx, by, x, y)
      if dd < bd bd = dd tx = int(bx) ty = int(by) end
      BR[i] -= dt
      if BR[i] <= 0 && CW[i] <= 0 && dd < 48400 && dd > 900
        var a = math.atan2(y - by, x - bx) var df = a - CA[i]
        while df > 3.14159 df -= 6.28318 end
        while df < -3.14159 df += 6.28318 end
        if ab(df) < 0.45 tire(bx, by, a, 1) BR[i] = 4.5 + hasard(0, 30) * 0.1 end
      end
    end
    i += 1
  end
  if n == 0 && etat == 1
    finir(3, "VICTOIRE !", str(int(T)) + " s  -  " + str(pv) + " coeurs", GOLD, 300 + pv * 100 + (T < 300 ? 300 - int(T) : 0)) son(880, 400)
  end
end

def maj(dt)
  if dt > 0.05 dt = 0.05 end
  if etat != 1
    tfin += dt
    if etat == 0
      var n = size(SC) + 1                       # le dernier choix du menu est le garage
      if appui(D) sel = (sel + 1) % n elif appui(G) sel = (sel + n - 1) % n end
      if appui(P3)
        if sel == n - 1 etat = 4 gl = 0 else reset() etat = 1 end
      end
    elif etat == 4
      var dl = 0
      if appui(D) dl = 1 elif appui(G) dl = -1 end
      if dl != 0
        if gl == 0
          ve = (ve + dl + size(VEH)) % size(VEH)
        elif gl == 1
          vc = (vc + dl + size(VCOL)) % size(VCOL) TAXI = VCOL[vc]
        else
          tm = (tm + dl + 3) % 3 theme(tm)
        end
      end
      if appui(P3) gl += 1 if gl > 2 etat = 0 end end
    elif appui(P3) && tfin > 1.0
      etat = 0
    end
    return
  end
  if inv > 0 inv -= dt end
  if msgt > 0 msgt -= dt end
  if cd > 0
    cd -= dt
    msg = cd > 0 ? str(int(cd) + 1) : "GO !" msgt = 0.8
  else
    T += dt
    if turbo > 0 turbo -= dt end
    conduit(dt)
    voitures(dt)
    caisses()
    if mode == 0 mission(dt) elif mode == 1 course(dt) else bataille(dt) end
  end
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
    if mode != 2
      var r = 13 + int(3 * math.sin(T * 6))
      rect(mx - 1, my - 50, 3, 50, c)
      disque(mx, my, r, c) disque(mx, my, r - 5, ROAD)
      texte(mx - 3, my - 4, mode == 1 ? "!" : (ph == 0 ? "P" : "$"), c, 1)
    end
  else
    var a = math.atan2(ty - y, tx - x) var co = math.cos(a) var si = math.sin(a)
    var ax = 160 + co * 104 var ay = 120 + si * 96
    tri(int(ax + co * 9), int(ay + si * 9), int(ax - co * 5 - si * 7), int(ay - si * 5 + co * 7), int(ax - co * 5 + si * 7), int(ay - si * 5 - co * 7), c)
  end
end

def hud()
  var i = 0
  if mode == 0
    ombre(6, 5, "$" + str(argent), GOLD, 2)
    var nf = size(CX) - nt
    while i < 3
      disque(262 + i * 18, 12, 6, i < nf ? GOLD : SHAD)
      i += 1
    end
    if ph == 1 ombre(250, 24, str(int(tl)) + " s", tl < 5 ? ROUGE : BLANC, 2) end
  elif mode == 1
    ombre(6, 5, str(int(T)) + "." + str(int(T * 10) % 10) + " s", BLANC, 2)
    ombre(6, 24, "TOUR " + str(cp / 4 + 1 > tours ? tours : cp / 4 + 1) + "/" + str(tours), GOLD, 1)
    if nriv > 0 ombre(262, 5, str(rang) + "/" + str(nriv + 1), VERT, 2) else ombre(256, 5, "OR " + str(SC[sel][7]) + " s", GOLD, 1) end
  else
    while i < 3
      rect(262 + i * 16, 8, 12, 12, i < mun ? VERT : SHAD)
      i += 1
    end
    ombre(256, 24, "RIVAUX " + str(nriv - kills), BLANC, 1)
  end
  ombre(6, 218, g > 0 ? "AV" : "AR", g > 0 ? VERT : ROUGE, 2)
  if turbo > 0 ombre(56, 218, "TURBO", GOLD, 2) end
  if inv > 1.3 ombre(130, 218, "BOUCLIER", VERT, 2) end
  if mode != 1
    i = 0
    while i < 3
      rect(6 + i * 13, mode == 0 ? 24 : 5, 10, 10, i < pv ? ROUGE : SHAD)
      i += 1
    end
  end
  if msgt > 0 ombre(160 - size(msg) * 6, 52, msg, BLANC, 2) end
  # carte
  rect(252, 186, 64, 48, NOIR)
  i = 0
  while i < NX ligne(256 + (i * 128 + 24) / 16, 188, 256 + (i * 128 + 24) / 16, 232, GRIS) i += 1 end
  i = 0
  while i < NY ligne(254, 190 + (i * 128 + 24) / 16, 314, 190 + (i * 128 + 24) / 16, GRIS) i += 1 end
  disque(256 + tx / 16, 190 + ty / 16, 3, ph == 0 ? VERT : PINK)
  i = nt
  while i < size(CX)
    if CT[i] != 9 disque(256 + int(CX[i]) / 16, 190 + int(CY[i]) / 16, 2, CT[i] == 1 ? ROUGE : CC[i]) end
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
    if px > -20 && px < 340 && py > -20 && py < 260
      auto(px, py, CA[i], CC[i], CT[i])
      if CT[i] == 3
        var b = 0
        while b < CK[i] disque(int(px) - 6 + b * 6, int(py) - 14, 3, CC[i]) b += 1 end
      end
    end
    i += 1
  end
  if size(IX) > 0
    i = 0
    while i < size(IX)
      var bx = IX[i] - ox var by = IY[i] - oy
      if bx > -10 && bx < 330 && by > -10 && by < 250 rect(bx - 6, by - 6, 12, 12, GOLD) texte(bx - 3, by - 4, "?", NOIR, 1) end
      i += 1
    end
    i = 0
    while i < size(OX)
      disque(int(OX[i] - ox), int(OY[i] - oy), 3, OQ[i] == 0 ? VERT : ROUGE)
      i += 1
    end
  end
  if inv <= 0 || int(T * 14) % 2 == 0 auto(x - ox, y - oy, h, TAXI, 0) end
  hud()
end

def garage()
  fond(CIEL[0])
  ombre(106, 6, "GARAGE", PINK, 3)
  var lib = ["VEHICULE", "COULEUR", "CARTE"]
  var k = 0
  while k < 3
    ombre(26, 44 + k * 38, lib[k], k == gl ? GOLD : GRIS, 2)
    k += 1
  end
  ombre(8, 44 + gl * 38, ">", GOLD, 2)
  ombre(150, 44, "< " + VEH[ve][0] + " >", BLANC, 2)
  rect(150, 62, int(VEH[ve][1] / 2.5), 4, VERT) rect(150, 68, int(VEH[ve][2] * 50), 4, GOLD)
  texte(222, 60, "vitesse", GRIS, 1) texte(222, 67, "virage", GRIS, 1)
  rect(150, 82, 40, 14, VCOL[vc]) texte(196, 84, "< >", GRIS, 1)
  ombre(150, 120, "< " + THEMES[tm] + " >", BLANC, 2)
  rect(0, 160, 320, 56, ROAD)
  var xx = 0
  while xx < 320 rect(xx, 187, 12, 2, DASH) xx += 24 end
  rect(0, 160, 320, 6, SIDE) rect(0, 210, 320, 6, SIDE)
  rect(20, 142, 70, 18, RC[1]) rect(120, 142, 70, 18, RC[2]) rect(220, 142, 70, 18, RC[0]) rect(28, 146, 22, 6, RL[1]) rect(128, 146, 22, 6, RL[2]) rect(228, 146, 22, 6, RL[0])
  auto(160, 188, 0.0, VCOL[vc], 0)
  texte(60, 226, "G / D : changer     P3 : suivant", BLANC, 1)
end

def dessine()
  if etat == 0
    var n = size(SC)
    var nm = sel == n ? "GARAGE" : SC[sel][0]
    var ds = sel == n ? "Vehicule, couleur et carte" : SC[sel][1]
    var ak = sel == n ? "P3 : ouvrir le garage" : (SC[sel][2] == 2 ? "G + D ensemble : tirer   P3 : gaz   (G / D : tourner)" : "G / D : tourner   P3 : gaz   P3 x2 : marche arriere")
    ecran("NEON CITY", PINK, ds, ak)
    ombre(160 - size(nm) * 9, 76, nm, BLANC, 3)
    ombre(10, 76, "<", GOLD, 3) ombre(296, 76, ">", GOLD, 3)
    ombre(150, 106, str(sel + 1) + "/" + str(n + 1), GRIS, 1)
    ombre(86, 226, "G / D : choisir   P3 : jouer", BLANC, 1)
  elif etat == 4
    garage()
  elif etat != 1
    ecran(ft, fc, fs, "P3 : menu")
  else
    jeu()
  end
end
