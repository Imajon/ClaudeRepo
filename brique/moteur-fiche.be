# Moteur de fiche (Berry) : joue la fiche de jeu FICHE (FICHE-JEU.md, modele plateforme).
# La fiche est ajoutee AVANT ce texte par fiche-berry.js ("var FICHE = {...}") : les donnees sont a l'IA, le moteur est a nous.
# La fiche parle en cases (T = 16 pixels) et en secondes ; ici tout est en pixels. Le heros : px, py, 10 x 14 pixels.
import math
import string
var T = 16
var ROWS = FICHE["rows"] var H = size(ROWS)
var MO = FICHE["monde"] var HE = FICHE["heros"] var BR = FICHE["briques"] var OB = FICHE["objets"] var RG = FICHE["regles"]
var BUT = FICHE["but"] var BUTV = FICHE["butv"]
var GRAV = MO.find("gravite", 0) * T + 0.0
var BORD = MO.find("bord", "vide") var CAM = MO.find("camera", "suit")
var VIT = HE.find("vitesse", 0) * T + 0.0 var ELAN = HE.find("elan", 0.2) + 0.0
var SAUT = HE.find("saut", {}) var MUR = HE.find("mur", {})
var VSAUT = 0.0
if GRAV > 0 && SAUT.find("hauteur", 0) > 0 VSAUT = math.sqrt(2.0 * GRAV * SAUT["hauteur"] * T) end
var NSAUT = SAUT.find("en_l_air", 0) var VIES = HE.find("vies", 0)
var ACT = [HE["boutons"].find("G", "rien"), HE["boutons"].find("D", "rien"), HE["boutons"].find("P3", "rien")]
var BTN = [G, D, P3]
var NIV = [] var KB = {} var OBJS = [] var PLAT = [] var EX = [] var EY = [] var PRIS = []
var CASSE = {} var OUV = {} var FERME = {} var RFAIT = [] var SOLID = {} var DT = {} var COL = {} var NPLAT = 0
var SX = 0.0 var SY = 0.0 var px = 0.0 var py = 0.0 var vx = 0.0 var vy = 0.0 var sol = false var mur = 0 var poussee = 0.0 var nsl = 0 var SUR = nil var VMUL = 1.0
var fin = false var lives = 0 var pts = 0 var NR = 0 var ECR = 0 var TOTR = 0 var TOTE = 0 var gagne = false var perdu = false var morts = 0 var tv0 = 0.0 var chrono = 0.0 var camy = 0 var MSG = "" var MSGT = 0.0
var CIEL = 0 var CIEL2 = 0 var BLOC = 0 var BORDC = 0 var FRAG = 0 var PIC = 0 var RESS = 0 var GLACE = 0 var OR = 0 var HERO = 0 var OEIL = 0 var BLANC = 0 var OMBRE = 0 var ROUGE = 0

def case(c, r)                                    # la brique de la case (c, r) ; ce qui est ouvert disparait
  if c < 0 || c >= 20 return BORD == "boucle" ? "." : "#" end
  if r < 0 return "." end
  if r >= H return BORD == "mur" ? "#" : "." end
  var k = NIV[r][c]
  if k != "." && (OUV.contains(k) || (CASSE.contains(r * 20 + c) && temps() > CASSE[r * 20 + c])) return "." end
  return k
end
def brute(c, r) return (c < 0 || c >= 20 || r < 0 || r >= H) ? "." : NIV[r][c] end
def prop(k, i) return KB.contains(k) ? KB[k][i] : 0 end
def plein(c, r)                                   # SOLID = les lettres solides en ce moment (ouvre / ferme la modifient)
  if c < 0 || c >= 20 return BORD != "boucle" end
  if r < 0 return false end
  if r >= H return BORD == "mur" end
  var k = NIV[r][c]
  if k == "." || !SOLID.contains(k) return false end
  return !(CASSE.contains(r * 20 + c) && temps() > CASSE[r * 20 + c])
end
def libreT(x, y)                                  # le heros a cette place ne touche aucune brique solide
  if x < 0 && BORD != "boucle" return false end      # le bord gauche est un mur (int() arrondit vers 0 : -5 / 16 = case 0)
  var c0 = int(x / T) var c1 = int((x + 9) / T) var r0 = int(y / T) var r1 = int((y + 13) / T)
  return !(plein(c0, r0) || plein(c1, r0) || plein(c0, r1) || plein(c1, r1))
end
def libre(x, y)                                   # ... ni plateforme mobile
  if !libreT(x, y) return false end
  if NPLAT == 0 return true end
  for o: PLAT
    if x < o["x"] + 16 && x + 10 > o["x"] && y < o["y"] + 16 && y + 14 > o["y"] return false end
  end
  return true
end
def touche(o) return px < o["x"] + 14 && px + 10 > o["x"] + 2 && py < o["y"] + 14 && py + 14 > o["y"] + 2 end
def recompte() NR = 0 var i = 0 while i < size(PRIS) if PRIS[i] NR += 1 end i += 1 end pts = NR + ECR end

def resolid() SOLID = {} for k: BR.keys() if BR[k].find("solide", 0) != 0 SOLID[k] = true end end end
def depart()                                      # une nouvelle vie : le heros, les objets, les regles et les portes reviennent
  px = SX py = SY vx = 0.0 vy = 0.0 poussee = 0.0 SUR = nil nsl = NSAUT VMUL = 1.0
  CASSE = {} OUV = {} FERME = {} MSG = "" ECR = 0 RFAIT = [] resolid()
  var i = 0 while i < size(RG) RFAIT.push(false) i += 1 end
  for o: OBJS
    o["x"] = o["x0"] o["y"] = o["y0"] o["vivant"] = true o["t0"] = temps() o["dir"] = 1.0 o["vy"] = 0.0 o["dx"] = 0.0 o["dy"] = 0.0
    o["on"] = o["dc"] == "toujours"
  end
  tv0 = temps() recompte()
end
def charge()
  NIV = [] OBJS = [] PLAT = [] NPLAT = 0 EX = [] EY = [] PRIS = [] TOTR = 0 TOTE = 0
  for r: 0 .. H - 1
    var row = ROWS[r] var s = ""
    for c: 0 .. 19
      var k = c < size(row) ? row[c] : "."
      if k == "S" SX = c * T + 3.0 SY = r * T + 2.0 k = "."
      elif OB.contains(k)
        var sp = OB[k]
        var o = {"m": sp.find("mouvement", "fixe"), "f": sp.find("forme", "bloc"), "ct": sp.find("contact", "perd"), "ec": sp.find("dessus", "") == "ecrase", "ca": sp.find("cache", 0) != 0, "dc": sp.find("declencheur", "toujours"), "l": k, "s": sp, "x0": c * T + 0.0, "y0": r * T + 0.0, "x": c * T + 0.0, "y": r * T + 0.0, "vivant": true, "on": false, "t0": 0.0, "dir": 1.0, "vy": 0.0, "dx": 0.0, "dy": 0.0}
        OBJS.push(o)
        var ct = sp.find("contact", "perd")
        if sp.find("forme", "bloc") == "bloc" && ct == "rien" PLAT.push(o) NPLAT += 1 end
        if ct == "ramasse" TOTR += 1 end
        if sp.find("forme", "bloc") == "ennemi" || sp.find("dessus", "") == "ecrase" TOTE += 1 end
        k = "."
      elif prop(k, 5) != 0
        EX.push(c) EY.push(r) PRIS.push(false) TOTR += 1
      end
      s += k
    end
    NIV.push(s)
  end
  lives = VIES
  depart()
end
def mort()
  fin = true morts += 1 son(150, 200)
  if VIES > 0
    lives -= 1
    if lives <= 0 perdu = true return end
  end
  depart()
end

def debut()
  CIEL = rgb(40, 52, 96) CIEL2 = rgb(70, 86, 140) BLOC = rgb(109, 91, 208) BORDC = rgb(160, 146, 240) FRAG = rgb(160, 112, 60)
  PIC = rgb(230, 230, 236) RESS = rgb(255, 212, 0) GLACE = rgb(150, 230, 255) OR = rgb(255, 212, 0) HERO = rgb(232, 70, 60) OEIL = rgb(255, 255, 255) BLANC = rgb(255, 255, 255) OMBRE = rgb(20, 24, 40) ROUGE = rgb(220, 60, 80)
  for k: BR.keys()                                # [solide, mortel, glisse, rebond (cases), disparait (s), ramasse, arrivee]
    var p = BR[k]
    var sl = p.find("solide", 0) != 0 && p.find("mortel", 0) == 0 && p.find("glisse", 0) < 0.5 && p.find("rebond", 0) == 0 && p.find("disparait", 0) == 0
    DT[k] = sl ? 1 : p.find("arrivee", 0) != 0 ? 2 : p.find("mortel", 0) != 0 ? 3 : p.find("rebond", 0) != 0 ? 4 : p.find("disparait", 0) != 0 ? 5 : p.find("glisse", 0) >= 0.5 ? 6 : p.find("ramasse", 0) != 0 ? 0 : 7     # 1 bloc 2 arrivee 3 pic 4 ressort 5 fragile 6 glace 7 decor 0 rien
    COL[k] = k == "#" ? BLOC : rgb(90 + (string.byte(k) * 37) % 120, 80 + (string.byte(k) * 71) % 100, 110 + (string.byte(k) * 23) % 110)
    KB[k] = [p.find("solide", 0), p.find("mortel", 0), p.find("glisse", 0), p.find("rebond", 0), p.find("disparait", 0), p.find("ramasse", 0), p.find("arrivee", 0)]
  end
  charge()
end

def chemin(o, s, m)                               # aller_retour / boucle : le trajet de la fiche, duree et pause a chaque point
  var lp = s.find("points", [[0, 0]]) var n = size(lp)
  if n < 2 return end
  var d = s.find("duree", 1.0) + 0.0 var p = s.find("pause", 0.0) + 0.0 var seg = d + p
  var ns = m == "boucle" ? n : 2 * (n - 1)
  var cyc = ns * seg var tt = temps() - o["t0"]
  tt = tt - int(tt / cyc) * cyc
  var i = int(tt / seg) var r = tt - i * seg var f = r < d ? r / d : 1.0
  var a = i var b = i + 1
  if m == "boucle" b = (i + 1) % n
  elif i >= n - 1 a = 2 * (n - 1) - i b = a - 1
  end
  o["x"] = o["x0"] + (lp[a][0] + (lp[b][0] - lp[a][0]) * f) * T
  o["y"] = o["y0"] + (lp[a][1] + (lp[b][1] - lp[a][1]) * f) * T
end
def bouge(o, dt)
  var s = o["s"] var m = o["m"] var ox = o["x"] var oy = o["y"]
  if !o["on"]                                     # pas encore declenche
    var d = o["dc"]
    var hx = px + 5 - (o["x0"] + 8) var hy = py + 7 - (o["y0"] + 8)
    if (d == "proche" && math.sqrt(hx * hx + hy * hy) < s.find("distance", 3) * T) || (d == "touche" && touche(o)) o["on"] = true o["t0"] = temps() end
  end
  if o["on"]
    if m == "aller_retour" || m == "boucle" chemin(o, s, m)
    elif m == "patrouille"                        # marche sur sa plateforme, fait demi-tour au mur ou au bord
      var v = s.find("vitesse", 2) * T * o["dir"] * dt var nx = o["x"] + v
      var fc = int((nx + (o["dir"] > 0 ? 15 : 0)) / T) var fr = int((o["y"] + 8) / T)
      if plein(fc, fr) || !plein(fc, fr + 1) o["dir"] = -o["dir"] else o["x"] = nx end
    elif m == "suivre"                            # va vers le heros (sans tomber : en haut / bas seulement sans gravite)
      var dx = px - o["x"] var dy = GRAV > 0 ? 0.0 : py - o["y"] var dist = math.sqrt(dx * dx + dy * dy)
      if dist > 2 var v = s.find("vitesse", 3) * T * dt o["x"] += dx / dist * v o["y"] += dy / dist * v end
    elif m == "tomber"
      o["vy"] += (GRAV > 0 ? GRAV : 300.0) * dt var ny = o["y"] + o["vy"] * dt
      var fr = int((ny + 16) / T)
      if plein(int((o["x"] + 8) / T), fr) o["y"] = fr * T - 16.0 o["vy"] = 0.0 else o["y"] = ny end
    end
  end
  o["dx"] = o["x"] - ox o["dy"] = o["y"] - oy
end

def agit(a, av)                                   # le "alors" d'une regle
  if a == "gagne" gagne = true son(1320, 300)
  elif a == "perd" mort()
  elif a == "ouvre" OUV[av] = true if SOLID.contains(av) SOLID.remove(av) end
  elif a == "ferme" FERME[av] = true SOLID[av] = true
  elif a == "vie_plus" lives += 1
  elif a == "message" MSG = av MSGT = temps() + 2.5
  elif a == "vitesse" VMUL = av + 0.0
  end
end
def regles(rl)
  var i = 0
  while i < size(RG)
    var r = RG[i]
    if !RFAIT[i]
      var q = r["q"] var qv = r["qv"] var ok = false
      if q == "touche"
        ok = rl == qv
        for o: OBJS if o["vivant"] && o["l"] == qv && touche(o) ok = true end end
      elif q == "ramasse_tout" ok = TOTR > 0 && NR >= TOTR
      elif q == "ecrase_tout" ok = TOTE > 0 && ECR >= TOTE
      elif q == "temps" ok = chrono >= qv
      elif q == "points" ok = pts >= qv
      end
      if ok RFAIT[i] = true agit(r["a"], r["av"]) if perdu || gagne return end end
    end
    i += 1
  end
end
def contacts()                                    # le heros touche un objet de la fiche
  for o: OBJS
    if o["vivant"] && (!o["ca"] || o["on"]) && touche(o)
      var c = o["ct"]
      if o["f"] == "bloc" && c == "rien"
      elif o["ec"] && vy > 0 && py + 14 < o["y"] + 10
        o["vivant"] = false ECR += 1 pts = NR + ECR vy = -300.0 score(pts) son(500, 60)
      elif c == "perd" mort() return
      elif c == "rebond" vy = -560.0 son(990, 60)
      elif c == "ramasse" o["vivant"] = false NR += 1 pts = NR + ECR score(pts) son(988, 60)
      elif c == "vie_moins"
        o["vivant"] = false
        if VIES > 0 lives -= 1 son(150, 200) if lives <= 0 perdu = true return end else mort() return end
      end
    end
  end
end

def maj(dt)
  if gagne || perdu
    if appui(P3) gagne = false perdu = false morts = 0 charge() end
    return
  end
  if dt > 0.05 dt = 0.05 end
  fin = false
  chrono = temps() - tv0
  for o: OBJS if o["vivant"] bouge(o, dt) end end
  if SUR != nil                                   # une plateforme mobile emporte le heros
    var ny = py + SUR["dy"] if libre(px, ny) py = ny end
    var nx = px + SUR["dx"] if libre(nx, py) px = nx end
  end
  var but = 0.0 var buty = 0.0 var sb = -1
  for k: 0 .. 2
    var a = ACT[k]
    if a == "sauter" sb = k end
    if bouton(BTN[k])
      if a == "aller_gauche" but -= VIT * VMUL
      elif a == "aller_droite" but += VIT * VMUL
      elif a == "aller_haut" buty -= VIT * VMUL
      elif a == "aller_bas" buty += VIT * VMUL
      end
    end
  end
  var sk = case(int((px + 5) / T), int((py + 15) / T))
  var gl = prop(sk, 2) + 0.0 var e = ELAN > gl ? ELAN : gl var kk = 16.0 - 14.0 * e      # elan : 0 = s'arrete net, 1 = glisse longtemps
  vx += (but - vx) * kk * dt
  if poussee != 0 vx = poussee poussee *= 0.9 if poussee * poussee < 400 poussee = 0.0 end end
  mur = 0
  if GRAV > 0 && !sol
    if but < 0 && !libre(px - 1, py) mur = -1 end
    if but > 0 && !libre(px + 1, py) mur = 1 end
  end
  if mur != 0 && MUR.find("glisse", 0) != 0 && vy > 70 vy = 70.0 end
  if sb >= 0 && VSAUT > 0
    if appui(BTN[sb])
      if sol vy = -VSAUT son(660, 40)
      elif mur != 0 && MUR.find("saut_mural", 0) != 0 vy = -VSAUT * 0.93 poussee = -mur * VIT * 0.95 son(780, 40)
      elif nsl > 0 nsl -= 1 vy = -VSAUT son(660, 40)
      end
    end
    if !bouton(BTN[sb]) && vy < -VSAUT * 0.36 vy = -VSAUT * 0.36 end     # bouton lache tot = petit saut
  end
  if GRAV > 0 vy += GRAV * dt if vy > 500 vy = 500.0 end else vy += (buty - vy) * kk * dt end
  var nx = px + vx * dt
  if libre(nx, py) px = nx
  elif !libreT(nx, py)
    if nx < 0 px = 0.0 elif vx > 0 px = int((nx + 9) / T) * T - 10.0 else px = (int(nx / T) + 1) * T + 0.0 end
    vx = 0.0 poussee = 0.0
  else vx = 0.0
  end
  var ny = py + vy * dt
  if libre(px, ny) py = ny
  elif vy > 0
    var haut = -1.0
    for o: PLAT if px + 10 > o["x"] && px < o["x"] + 16 && ny + 14 > o["y"] && py + 14 <= o["y"] + 4 haut = o["y"] end end
    if haut >= 0 py = haut - 14 else py = int((ny + 13) / T) * T - 14.0 end
    vy = 0.0
    var r = int((py + 15) / T)
    for c: [int(px / T), int((px + 9) / T)]
      var k = case(c, r)
      var rb = prop(k, 3) if rb > 0 && GRAV > 0 vy = -math.sqrt(2.0 * GRAV * rb * T) son(990, 60) end
      var di = prop(k, 4) if di > 0 && !CASSE.contains(r * 20 + c) CASSE[r * 20 + c] = temps() + di end
    end
  else py = (int(ny / T) + 1) * T + 0.0 vy = 0.0
  end
  sol = vy >= 0 && !libre(px, py + 1)
  if sol nsl = NSAUT end
  SUR = nil
  if vy >= 0 for o: PLAT if px + 10 > o["x"] && px < o["x"] + 16 && py + 14 >= o["y"] - 1 && py + 14 <= o["y"] + 3 SUR = o end end end
  if BORD == "boucle"
    if px < -10 px = 320.0 elif px > 320 px = -10.0 end
    if py > H * T py = -14.0 end
  end
  var cc = int((px + 5) / T) var cr = int((py + 7) / T)
  var bas = case(cc, int((py + 13) / T)) var ici = case(cc, cr)
  if prop(ici, 1) != 0 || prop(bas, 1) != 0 || (BORD == "vide" && py > H * T) mort() return end
  var i = 0
  while i < size(EX)
    if !PRIS[i] && EX[i] == cc && EY[i] == cr PRIS[i] = true NR += 1 pts = NR + ECR score(pts) son(988, 60) end
    i += 1
  end
  contacts()
  if fin || perdu return end
  regles(brute(cc, cr))
  if fin || perdu || gagne return end
  if BUT == "arriver" && prop(ici, 6) != 0 gagne = true son(1320, 300)
  elif BUT == "ramasser_tout" && TOTR > 0 && NR >= TOTR gagne = true son(1320, 300)
  elif BUT == "ecraser_tout" && TOTE > 0 && ECR >= TOTE gagne = true son(1320, 300)
  elif BUT == "survivre" && chrono >= BUTV gagne = true son(1320, 300)
  end
  camy = CAM == "suit" ? int(py) - 140 : 0
  if camy < 0 camy = 0 end
  if camy > H * T - 240 camy = H * T - 240 end      # niveau moins haut que l'ecran : camy < 0, le sol reste en bas
end

def dessine()
  fond(CIEL)
  var b = 0 while b < 6 rect(0, b * 40 + 20, 320, 20, CIEL2) b += 1 end
  camera(0, camy)
  var r = int(camy / T) var rf = r + 16
  if r < 0 r = 0 end
  if rf > H rf = H end
  var nO = size(OUV) var nF = size(FERME)
  while r < rf
    var row = NIV[r] var c = 0 var y = r * T
    while c < 20
      var k = row[c]
      if k != "." && !(nO > 0 && OUV.contains(k))
        var x = c * T var t = nF > 0 && FERME.contains(k) ? 1 : DT.find(k, 7)
        if t == 1 rect(x, y, T, T, COL.find(k, BLOC)) rect(x, y, T, 2, BORDC)
        elif t == 2 rect(x + 7, y, 2, T, BLANC) tri(x + 9, y, x + 9, y + 8, x + T, y + 4, HERO)
        elif t == 3 tri(x, y + T, x + 8, y + 4, x + T, y + T, PIC)
        elif t == 4 rect(x, y + 8, T, 8, BLOC) rect(x + 2, y + 4, 12, 4, RESS)
        elif t == 5 rect(x, y, T, 6, (CASSE.contains(r * 20 + c) && int(temps() * 20) % 2) ? OMBRE : FRAG)
        elif t == 6 rect(x, y, T, T, GLACE)
        elif t == 7 rect(x + 4, y + 4, 8, 8, CIEL2)      # du decor
        end
      end
      c += 1
    end
    r += 1
  end
  var i = 0
  while i < size(EX)
    if !PRIS[i] disque(EX[i] * T + 8, EY[i] * T + 8 + int(temps() * 4) % 2, 4, OR) end
    i += 1
  end
  for o: OBJS
    if o["vivant"] && (!o["ca"] || o["on"])
      var x = int(o["x"]) var y = int(o["y"]) var f = o["f"]
      if f == "scie" disque(x + 8, y + 8, 7, PIC) disque(x + 8, y + 8, 3, OMBRE)
      elif f == "pic" tri(x, y + T, x + 8, y + 4, x + T, y + T, PIC)
      elif f == "ennemi" rect(x + 1, y + 2, 14, 14, ROUGE) rect(x + 3, y + 5, 3, 3, OEIL) rect(x + 9, y + 5, 3, 3, OEIL)
      elif f == "bloc" rect(x, y, T, T, BLOC) rect(x, y, T, 2, BORDC)
      end
    end
  end
  var x = int(px) var y = int(py)
  rect(x + 2, y + 2, 10, 14, OMBRE) rect(x, y, 10, 14, HERO)
  rect(x + (vx < 0 ? 1 : 5), y + 3, 4, 4, OEIL)
  camera(0, 0)
  var s = string.format("%d * %.1f s", pts, chrono)
  texte(5, 5, s, OMBRE, 2) texte(4, 4, s, BLANC, 2)
  if VIES > 0 var v = string.format("vies %d", lives) texte(5, 25, v, OMBRE, 1) texte(4, 24, v, BLANC, 1) end
  if MSG != "" && temps() < MSGT texte(21, 121, MSG, OMBRE, 2) texte(20, 120, MSG, BLANC, 2) end
  if gagne texte(61, 101, "Gagne !", OMBRE, 4) texte(60, 100, "Gagne !", BLANC, 4) end
  if perdu texte(61, 101, "Perdu", OMBRE, 4) texte(60, 100, "Perdu", ROUGE, 4) end
end
