# Moteur de fiche, NUIT DES PELUCHES (format FNAF, Berry) : joue la fiche FICHE (modele "fnaf", voir fiche.js : nettoyerFnaf). La fiche est ajoutee AVANT ce texte par fiche-berry.js.
# La "matrice" = rows : une lettre par salle, "." = mur ; deux salles collees (haut/bas/gauche/droite) sont reliees. Les salles "porte" gauche/droite donnent sur le bureau.
# Ecran 320 x 240. Une peluche avance vers sa porte (BFS precalcule), attend devant, et gagne si la porte est ouverte. 90 s (par defaut) = de minuit a 6 h.
# Memoire : les donnees de la fiche sont rendues au ramasse-miettes apres debut() et gc.collect() tourne toutes les 12 images (voir moteur-fiche.be).
import math
import string
import gc
var HE = FICHE["heros"] var EN = FICHE["energie"] var NU = FICHE["nuit"] var RG = FICHE["regles"] var BUT = FICHE["but"]
var ACT = [HE["boutons"]["G"], HE["boutons"]["D"], HE["boutons"]["P3"]] var BTN = [G, D, P3]
var DUREE = NU["duree"] + 0.0 var NUITS = NU["nuits"] var RAGE = NU["rage"] var HEURES = ["12 AM", "1 AM", "2 AM", "3 AM", "4 AM", "5 AM"]
var CYC = true var HASL = false var HASC = false
var NR = 0 var RL = [] var RNOM = [] var RPT = [] var RX = [] var RY = [] var NB = [] var DI = [[], []] var NC = 1 var NRW = 1 var MC = 20 var MX = 6 var MY = 40
var PN = [] var PFM = [] var PCL = [] var PDEP = [] var PDR = [] var PAG = [] var PD1 = [] var PD2 = [] var PAT = [] var PTI = []
var PRM = [] var PST = [] var PT = [] var PW = []
var CN = ["marron", "bleu", "orange", "jaune", "violet", "vert", "rose", "gris"] var CA = [] var CB = []
var DC = [false, false] var DP = [0.0, 0.0]
var pile = 100.0 var hor = 0.0 var nuit = 1 var etat = 0 var tet = 0.0 var lamp = false var cam = false var ci = 0 var tueur = 0 var panne = 0.0
var MSG = "" var MSGT = 0.0 var RFAIT = [] var RAGEX = 0 var GCN = 0
var NO = 0 var BL = 0 var RO = 0 var GR = 0 var VE = 0 var MU = 0 var MU2 = 0 var SOL = 0 var BU = 0 var BU2 = 0 var ME = 0 var ME2 = 0 var JA = 0

def couleurs()
  NO = rgb(8, 8, 12) BL = rgb(240, 238, 230) RO = rgb(190, 30, 30) GR = rgb(130, 130, 145) VE = rgb(70, 190, 90) JA = rgb(255, 220, 60)
  MU = rgb(30, 28, 40) MU2 = rgb(58, 54, 74) SOL = rgb(20, 18, 24) BU = rgb(64, 44, 34) BU2 = rgb(96, 70, 52) ME = rgb(96, 102, 118) ME2 = rgb(60, 64, 78)
  CA = [rgb(150, 98, 52), rgb(120, 150, 210), rgb(214, 84, 28), rgb(235, 190, 40), rgb(150, 100, 200), rgb(80, 170, 90), rgb(235, 130, 170), rgb(150, 150, 160)]
  CB = [rgb(222, 184, 130), rgb(220, 230, 250), rgb(250, 236, 216), rgb(255, 230, 130), rgb(220, 200, 250), rgb(190, 230, 190), rgb(250, 210, 225), rgb(220, 220, 225)]
end

def nom_de(l) var i = 0 while i < NR if RL[i] == l return i end i += 1 end return -1 end
def distances(d)                                   # nombre de pas jusqu'a la salle la plus proche qui donne sur la porte d (99 = pas de chemin)
  var D = [] var f = [] var i = 0
  while i < NR
    if RPT[i] == d D.push(0) f.push(i) else D.push(99) end
    i += 1
  end
  var k = 0
  while k < size(f)
    var r = f[k] k += 1
    for v: NB[r] if D[v] == 99 D[v] = D[r] + 1 f.push(v) end end
  end
  return D
end
def debut()
  couleurs()
  for k: 0 .. 2 if ACT[k] == "camera_suivante" CYC = false end if ACT[k] == "lampe" HASL = true end if ACT[k] == "camera" || ACT[k] == "camera_suivante" HASC = true end end
  var rows = FICHE["rows"] var S = FICHE["salles"]
  for r: rows if size(r) > NC NC = size(r) end end
  NRW = size(rows)
  var G2 = []
  for y: 0 .. NRW - 1
    for x: 0 .. NC - 1
      var l = x < size(rows[y]) ? rows[y][x] : "."
      if l != "."
        var s = S.find(l, {})
        G2.push(NR) RL.push(l) RNOM.push(s.find("nom", l)) RPT.push(s.find("porte", "") == "gauche" ? 0 : (s.find("porte", "") == "droite" ? 1 : -1)) RX.push(x) RY.push(y) NR += 1
      else G2.push(-1) end
    end
  end
  for i: 0 .. NR - 1
    var v = []
    var x = RX[i] var y = RY[i]
    if x > 0 && G2[y * NC + x - 1] >= 0 v.push(G2[y * NC + x - 1]) end
    if x < NC - 1 && G2[y * NC + x + 1] >= 0 v.push(G2[y * NC + x + 1]) end
    if y > 0 && G2[(y - 1) * NC + x] >= 0 v.push(G2[(y - 1) * NC + x]) end
    if y < NRW - 1 && G2[(y + 1) * NC + x] >= 0 v.push(G2[(y + 1) * NC + x]) end
    NB.push(v)
  end
  DI = [distances(0), distances(1)]
  MC = int(150 / NC) var m2 = int(112 / NRW) if m2 < MC MC = m2 end if MC > 34 MC = 34 end
  for p: FICHE["peluches"]
    var ci2 = 0 for j: 0 .. 7 if CN[j] == p["couleur"] ci2 = j end end
    PN.push(p["nom"]) PFM.push(p["forme"] == "lapin" ? 1 : (p["forme"] == "renard" ? 2 : 0)) PCL.push(ci2)
    var d = nom_de(p["depart"]) if d < 0 d = 0 end
    PDEP.push(d) PRM.push(d) PDR.push(p["porte"] == "droite" ? 1 : 0) PAG.push(p["agressivite"]) PD1.push(int(p["delai"][0] * 10)) PD2.push(int(p["delai"][1] * 10)) PAT.push(p["attente"] + 0.0) PTI.push(p["timide"] ? true : false) PST.push(0) PT.push(5.0) PW.push(0.0)
  end
  FICHE = nil HE = nil NU = nil S = nil
  gc.collect()
end

def delai(i) var t = (hasard(PD1[i], PD2[i]) - nuit * 4) / 10.0 return t < 1.0 ? 1.0 : t end
def recommence()
  for i: 0 .. size(PN) - 1 PRM[i] = PDEP[i] PST[i] = 0 PT[i] = delai(i) + 2.0 PW[i] = 0.0 end
  DC[0] = false DC[1] = false DP[0] = 0.0 DP[1] = 0.0
  pile = 100.0 hor = 0.0 panne = 0.0 lamp = false cam = false ci = 0 MSG = "" RAGEX = 0
  RFAIT = [] for i: 0 .. size(RG) - 1 RFAIT.push(false) end
  etat = 1 son(660, 80)
end
def perdu(i) tueur = i < size(PN) ? i : -1 etat = 2 tet = 1.4 end                 # sans peluche (panne, regle "perd") : -1 = le noir
def aube() etat = 4 tet = 3.5 score(nuit * 100 + int(pile)) son(1320, 300) end

def vers(i, delta)                                 # un voisin au hasard, plus pres (delta -1) ou plus loin (+1) de la porte de la peluche i ; -1 s'il n'y en a pas
  var d = PDR[i] var r = PRM[i] var c = []
  for v: NB[r] if DI[d][v] == DI[d][r] + delta c.push(v) end end
  return size(c) > 0 ? c[hasard(0, size(c) - 1)] : -1
end
def peluche(i, dt)
  if PST[i] == 0
    PT[i] -= dt
    if PT[i] <= 0
      PT[i] = delai(i)
      if PTI[i] && cam && ci == PRM[i] return end    # timide : on le regarde, il ne bouge pas
      if hasard(0, 19) < PAG[i] + nuit * RAGE + RAGEX
        var v = vers(i, -1)
        if v >= 0
          PRM[i] = v
          if DI[PDR[i]][v] == 0 PST[i] = 1 PW[i] = PAT[i] + hasard(0, 20) / 10.0 son(90, 120) else son(200, 40) end
        end
      elif DI[PDR[i]][PRM[i]] < 99 && PRM[i] != PDEP[i] && hasard(0, 4) == 0
        var v = vers(i, 1) if v >= 0 PRM[i] = v end
      end
    end
  else
    PW[i] -= dt
    if PW[i] <= 0
      if DC[PDR[i]] PRM[i] = PDEP[i] PST[i] = 0 PT[i] = delai(i) + 2.0 son(250, 80)
      else perdu(i) end
    end
  end
end
def agit(a, av)
  if a == "gagne" aube()
  elif a == "perd" perdu(0)
  elif a == "message" MSG = av MSGT = temps() + 2.5
  elif a == "rage" RAGEX += int(av)
  elif a == "energie" pile += av + 0.0 if pile > 100 pile = 100.0 end if pile < 0 pile = 0.0 end
  end
end
def regles()
  for i: 0 .. size(RG) - 1
    if !RFAIT[i]
      var r = RG[i] var q = r["q"] var ok = false
      if q == "temps" ok = hor >= r["qv"]
      elif q == "pile" ok = pile <= r["qv"]
      elif q == "nuit" ok = nuit >= r["qv"]
      end
      if ok RFAIT[i] = true agit(r["a"], r["av"]) end
    end
  end
end

def maj_jeu(dt)
  hor += dt
  if BUT == "aube" && hor >= DUREE aube() return end
  if pile <= 0
    DC[0] = false DC[1] = false lamp = false cam = false panne += dt
    if panne > 15 perdu(0) end                       # plus de courant : la premiere peluche de la fiche arrive dans le noir
    return
  end
  for k: 0 .. 2
    var a = ACT[k]
    if a == "porte_gauche" && appui(BTN[k]) DC[0] = !DC[0] son(110, 50)
    elif a == "porte_droite" && appui(BTN[k]) DC[1] = !DC[1] son(110, 50)
    elif a == "lampe" lamp = bouton(BTN[k])
    elif a == "camera" && appui(BTN[k])
      if !cam cam = true ci = 0 elif CYC ci += 1 if ci >= NR cam = false ci = 0 end else cam = false end
      son(500, 30)
    elif a == "camera_suivante" && appui(BTN[k])
      if !cam cam = true ci = 0 else ci = (ci + 1) % NR end
      son(500, 30)
    end
  end
  pile -= (EN["base"] + (DC[0] ? EN["porte"] : 0) + (DC[1] ? EN["porte"] : 0) + (lamp ? EN["lampe"] : 0) + (cam ? EN["camera"] : 0)) * (1.0 + EN["montee"] * hor / 60.0 + (nuit - 1) * 0.1) * dt
  if pile < 0 pile = 0.0 end
  for k: 0 .. 1
    var c = DC[k] ? 1.0 : 0.0
    if DP[k] < c DP[k] = DP[k] + 4 * dt > c ? c : DP[k] + 4 * dt
    elif DP[k] > c DP[k] = DP[k] - 4 * dt < c ? c : DP[k] - 4 * dt end
  end
  var i = 0
  while i < size(PN) && etat == 1 peluche(i, dt) i += 1 end
  if etat == 1 regles() end
end

def maj(dt)
  GCN += 1 if GCN >= 12 GCN = 0 gc.collect() end
  if dt > 0.05 dt = 0.05 end
  if etat == 0
    if appui(P3) || appui(G) || appui(D) recommence() end
  elif etat == 1 maj_jeu(dt)
  elif etat == 2
    tet -= dt son(hasard(80, 500), 40)
    if tet <= 0 etat = 3 tet = 0.0 son(0, 0) end
  elif etat == 3
    tet += dt
    if tet > 0.8 && (appui(P3) || appui(G) || appui(D)) recommence() end
  elif etat == 4
    tet -= dt
    if tet <= 0 if nuit < NUITS nuit += 1 etat = 0 else etat = 5 end end
  elif etat == 5
    if appui(P3) nuit = 1 etat = 0 end
  end
end

def face(x, y, r, i)
  var c = CA[PCL[i]] var f = PFM[i]
  if f == 0
    disque(x - r * 3 / 4, y - r * 3 / 4, r / 3 + 1, c) disque(x + r * 3 / 4, y - r * 3 / 4, r / 3 + 1, c)
  elif f == 1
    rect(x - r * 3 / 5 - r / 6, y - r * 2, r / 3 + 1, r * 5 / 4, c) rect(x + r * 3 / 5 - r / 6, y - r * 2, r / 3 + 1, r * 5 / 4, c)
  else
    tri(x - r, y - r / 3, x - r * 3 / 4, y - r * 3 / 2, x - r / 5, y - r * 3 / 4, c) tri(x + r, y - r / 3, x + r * 3 / 4, y - r * 3 / 2, x + r / 5, y - r * 3 / 4, c)
  end
  disque(x, y, r, c) disque(x, y + r / 3, r / 2, CB[PCL[i]])
  var er = r / 4 + 1 var ex = r / 2 var ey = y - r / 5 var pc = r > 40 ? RO : NO
  disque(x - ex, ey, er, BL) disque(x + ex, ey, er, BL) disque(x - ex, ey, er / 2 + 1, pc) disque(x + ex, ey, er / 2 + 1, pc)
  disque(x, y + r / 5, r / 8 + 1, NO)
  if r > 20
    rect(x - r / 3, y + r / 2, r * 2 / 3, r / 5, NO)
    rect(x - r / 4, y + r / 2, r / 6 + 1, r / 8 + 1, BL) rect(x + r / 4 - r / 6, y + r / 2, r / 6 + 1, r / 8 + 1, BL)
  end
end

def couloir(s)
  var x0 = s == 0 ? 0 : 244
  if lamp || !HASL
    rect(x0, 28, 76, 162, rgb(86, 74, 62)) rect(x0 + 16, 56, 44, 100, rgb(52, 44, 38)) rect(x0 + 28, 78, 20, 56, rgb(22, 18, 18))
    for i: 0 .. size(PN) - 1 if PST[i] == 1 && PDR[i] == s face(x0 + 38, 120, 17, i) end end
  else rect(x0, 28, 76, 162, rgb(5, 5, 8)) end
  var h = int(162 * DP[s])
  if h > 0
    rect(x0, 28, 76, h, ME)
    var y = 28 while y < 28 + h rect(x0, y, 76, 3, ME2) y += 18 end
    rect(x0, 28 + h - 4, 76, 4, JA)
  end
end

def vue_camera()
  fond(rgb(10, 14, 12))
  for i: 0 .. NR - 1
    var x = MX + RX[i] * MC var y = MY + RY[i] * MC
    rect(x + 1, y + 1, MC - 2, MC - 2, i == ci ? rgb(60, 140, 80) : rgb(34, 40, 46))
    if RPT[i] >= 0 rect(x + 1, y + (RPT[i] == 0 ? 1 : MC - 4), MC - 2, 3, JA) end
  end
  rect(168, 30, 148, 140, rgb(24, 30, 26)) cadre(168, 30, 148, 140, GR)
  texte(172, 34, RNOM[ci], VE, 1)
  var n = 0
  for i: 0 .. size(PN) - 1 if PRM[i] == ci face(186 + n * 30, 106, 12, i) n += 1 end end
  if n == 0 texte(200, 100, "(vide)", GR, 1) end
  for j: 0 .. 3 rect(168, 30 + hasard(0, 138), 148, 1, rgb(60, 70, 62)) end
end

def dessine()
  if etat == 0
    fond(NO)
    texte(100, 36, "NUIT " + str(nuit), RO, 3)
    texte(70, 90, "Survis jusqu'a 6 AM", BL, 2) texte(82, 126, "Appuie sur un bouton", GR, 1)
    var y = 150
    for k: 0 .. 2
      if ACT[k] != "rien" texte(40, y, ["G", "D", "P3"][k] + " : " + ACT[k], GR, 1) y += 14 end
    end
  elif etat == 2
    camera(hasard(-6, 6), hasard(-6, 6))
    fond(int(temps() * 20) % 2 == 0 ? rgb(120, 0, 0) : NO)
    if tueur >= 0 face(160, 130, 50 + int((1.4 - tet) * 80), tueur) end
    camera(0, 0)
  elif etat == 3
    fond(NO) texte(85, 50, "PERDU", RO, 5) texte(40, 120, (tueur >= 0 ? PN[tueur] : "Le noir") + " t'a attrape", BL, 2)
    if tet > 0.8 texte(70, 180, "Appuie : recommencer", GR, 1) end
  elif etat == 4
    fond(NO) texte(88, 60, "6 AM", BL, 6) texte(100, 140, "Bravo !", JA, 3)
  elif etat == 5
    fond(NO) texte(40, 80, "Tu as survecu !", JA, 2) texte(70, 160, "P3 : rejouer", GR, 2)
  else
    if pile <= 0
      fond(NO) texte(64, 70, "PLUS DE COURANT", RO, 2)
      rect(0, 28, 76, 162, rgb(16, 14, 22)) rect(244, 28, 76, 162, rgb(16, 14, 22))
    elif cam vue_camera()
    else
      fond(SOL)
      rect(78, 28, 164, 162, lamp ? MU2 : MU)
      rect(130, 44, 60, 50, BU) cadre(130, 44, 60, 50, BU2)
      couloir(0) couloir(1)
      rect(70, 188, 180, 52, BU) rect(70, 188, 180, 4, BU2)
      rect(130, 150, 60, 38, rgb(14, 14, 18)) rect(134, 154, 52, 30, rgb(30, 60, 110))
      var h2 = int(hor / DUREE * 6) if h2 > 5 h2 = 5 end
      texte(146, 165, HEURES[h2], BL, 1)
    end
    var h = int(hor / DUREE * 6) if h > 5 h = 5 end
    texte(238, 6, HEURES[h], BL, 2) texte(6, 6, "Nuit " + str(nuit), BL, 1)
    texte(6, 206, "Pile " + str(int(pile)) + "%", BL, 1) cadre(6, 218, 62, 9, GR)
    var w = int(pile * 60 / 100)
    if w > 0 rect(7, 219, w, 7, pile > 30 ? VE : RO) end
    if MSG != "" && temps() < MSGT texte(21, 101, MSG, NO, 2) texte(20, 100, MSG, BL, 2) end
  end
end
