# Jeu declare : les donnees (fiche) puis le moteur (moteur-fiche.be)
var FICHE = {"rows":["XXXXXXXXXXXX","BBBBBBBBBBBB","CCVCCLCCMCCT","CCCCCCCCCCCC","AAAAAAAAAAAA","............"],"heros":{"boutons":{"G":"aller_gauche","D":"aller_droite","P3":"lancer"},"vitesse":12,"largeur":3,"vies":3},"balle":{"vitesse":10,"nombre":1,"acceleration":0.5},"briques":{"X":{"indestructible":1},"B":{"coups":2},"C":{},"A":{},"V":{"bonus":"vie"},"L":{"bonus":"large"},"M":{"bonus":"multi"},"T":{"bonus":"lent"}},"regles":[{"q":"points","qv":20,"a":"message","av":"Bien joue !"}],"but":"casser_tout","butv":0}
import math
import string
import gc
var BH = 12 var TOP = 28 var PY = 222 var PH = 6
var HE = FICHE["heros"] var BA = FICHE["balle"] var RG = FICHE["regles"] var BUT = FICHE["but"] var BUTV = FICHE["butv"]
var VP = HE["vitesse"] * 16.0 var PW = HE["largeur"] * 16.0 var VIES = HE["vies"] var NBALLES = BA["nombre"] var SP0 = BA["vitesse"] * 16.0 var ACC = BA["acceleration"] + 0.0
var ACT = [HE["boutons"]["G"], HE["boutons"]["D"], HE["boutons"]["P3"]] var BTN = [G, D, P3] var LB = -1
var NC = 4 var NR = size(FICHE["rows"]) var CW = 80.0
var GL = [] var GR = [] var BK = {} var COLB = {} var NBR = 0 var NBR0 = 0 var DEF = [1, 0, ""]
var BX = [] var BY = [] var BVX = [] var BVY = [] var IX = [] var IY = [] var IT = []
var rx = 0.0 var colle = true var tcolle = 0.0 var SP = SP0 var VM = 1.0 var tlarge = 0.0 var tlent = 0.0 var pw = PW
var lives = 0 var pts = 0 var gagne = false var perdu = false var chrono = 0.0 var tv0 = 0.0 var MSG = "" var MSGT = 0.0 var GCN = 0 var RFAIT = []
var FOND = 0 var BLANC = 0 var JAUNE = 0 var ROUGE = 0 var METAL = 0
def cellule(l) return BK.contains(l) ? BK[l] : DEF end
def remplis()
NBR = 0
for i: 0 .. size(GL) - 1
var l = GL[i]
if l == "." GR[i] = 0
else
var b = cellule(l)
if b[1] != 0 GR[i] = 99 else GR[i] = b[0] NBR += 1 end
end
end
NBR0 = NBR
end
def pose()
BX = [] BY = [] BVX = [] BVY = []
for i: 1 .. NBALLES BX.push(rx + pw / 2 + (i - 1 - (NBALLES - 1) / 2.0) * 10) BY.push(PY - 4.0) BVX.push(0.0) BVY.push(0.0) end
colle = true tcolle = temps()
end
def recommence()
rx = 160.0 - PW / 2 pw = PW SP = SP0 VM = 1.0 tlarge = 0.0 tlent = 0.0 lives = VIES pts = 0 gagne = false perdu = false MSG = "" IX = [] IY = [] IT = []
RFAIT = [] for i: 0 .. size(RG) - 1 RFAIT.push(false) end
remplis() pose() tv0 = temps()
end
def debut()
FOND = rgb(20, 20, 40) BLANC = rgb(255, 255, 255) JAUNE = rgb(255, 220, 60) ROUGE = rgb(240, 70, 80) METAL = rgb(150, 150, 165)
for k: 0 .. 2 if ACT[k] == "lancer" LB = k end end
var rows = FICHE["rows"]
for r: rows if size(r) > NC NC = size(r) end end
CW = 320.0 / NC
for r: rows for c: 0 .. NC - 1 GL.push(c < size(r) ? r[c] : ".") GR.push(0) end end
for l: FICHE["briques"].keys()
var p = FICHE["briques"][l]
BK[l] = [p.find("coups", 1), p.find("indestructible", 0), p.find("bonus", "")]
end
for l: GL
if l != "." && !COLB.contains(l) var b = string.byte(l) COLB[l] = rgb(80 + (b * 37) % 150, 70 + (b * 71) % 150, 90 + (b * 23) % 150) end
end
recommence()
FICHE = nil HE = nil BA = nil
gc.collect()
end
def ib(x, y)
if y < TOP return -1 end
var c = int(x / CW) var r = int((y - TOP) / BH)
if x < 0 || c >= NC || r >= NR return -1 end
var i = r * NC + c
return GR[i] > 0 ? i : -1
end
def agit(a, av)
if a == "gagne" gagne = true son(1320, 300)
elif a == "perd" lives = 0 perdu = true son(150, 300)
elif a == "vie_plus" lives += 1
elif a == "message" MSG = av MSGT = temps() + 2.5
elif a == "vitesse" VM = av + 0.0
end
end
def casse(k)
NBR -= 1 pts += 1 score(pts) son(1200, 20)
var b = cellule(GL[k])
if b[2] != ""
IX.push(int(k % NC * CW + CW / 2)) IY.push(TOP + int(k / NC) * BH + 6.0) IT.push(b[2])
end
if ACC > 0 && SP < SP0 * 2 SP += ACC * 5.0 end
end
def touche(k)
if GR[k] == 99 son(300, 10) return end
GR[k] -= 1
if GR[k] == 0 casse(k) else son(700, 10) end
end
def bonus(t)
if t == "vie" lives += 1 son(988, 100)
elif t == "large" tlarge = temps() + 10.0 son(880, 100)
elif t == "lent" tlent = temps() + 8.0 son(440, 100)
elif t == "multi" && size(BX) > 0 && size(BX) < 3
BX.push(BX[0]) BY.push(BY[0]) BVX.push(-BVX[0]) BVY.push(BVY[0]) son(660, 100)
end
end
def pas(i, h)
var ox = BX[i] var oy = BY[i] var x = ox + BVX[i] * h var y = oy + BVY[i] * h
if x < 4 x = 4.0 BVX[i] = math.abs(BVX[i]) son(600, 10) elif x > 316 x = 316.0 BVX[i] = -math.abs(BVX[i]) son(600, 10) end
if y < 4 y = 4.0 BVY[i] = math.abs(BVY[i]) son(600, 10) end
var k = ib(x + (BVX[i] > 0 ? 4 : -4), oy)
if k >= 0 touche(k) BVX[i] = -BVX[i] x = ox end
k = ib(x, y + (BVY[i] > 0 ? 4 : -4))
if k >= 0 touche(k) BVY[i] = -BVY[i] y = oy end
if BVY[i] > 0 && y + 4 >= PY && y < PY + PH && x >= rx - 3 && x <= rx + pw + 3
var q = (x - rx - pw / 2) / (pw / 2) if q > 1 q = 1.0 elif q < -1 q = -1.0 end
var m = math.sqrt(BVX[i] * BVX[i] + BVY[i] * BVY[i])
BVX[i] = m * math.sin(q * 1.05) BVY[i] = -m * math.cos(q * 1.05) y = PY - 4.0 son(900, 20)
end
BX[i] = x BY[i] = y
end
def regles()
for i: 0 .. size(RG) - 1
if !RFAIT[i]
var r = RG[i] var q = r["q"] var ok = false
if q == "casse_tout" ok = NBR0 > 0 && NBR == 0
elif q == "temps" ok = chrono >= r["qv"]
elif q == "points" ok = pts >= r["qv"]
end
if ok RFAIT[i] = true agit(r["a"], r["av"]) end
end
end
end
def maj(dt)
GCN += 1 if GCN >= 12 GCN = 0 gc.collect() end
if gagne || perdu
if appui(P3) recommence() end
return
end
if dt > 0.05 dt = 0.05 end
var t = temps() chrono = t - tv0
pw = t < tlarge ? PW + 16 : PW
var dir = 0.0 var lance = false
for k: 0 .. 2
if bouton(BTN[k]) if ACT[k] == "aller_gauche" dir -= 1.0 elif ACT[k] == "aller_droite" dir += 1.0 end end
if k == LB && appui(BTN[k]) lance = true end
end
rx += dir * VP * dt
if rx < 0 rx = 0.0 elif rx > 320 - pw rx = 320.0 - pw end
if colle
for i: 0 .. size(BX) - 1 BX[i] = rx + pw / 2 + (i - (size(BX) - 1) / 2.0) * 10 BY[i] = PY - 4.0 end
if lance || (LB < 0 && t - tcolle > 2.0)
colle = false
for i: 0 .. size(BX) - 1 var a = 0.26 - 0.26 * i BVX[i] = SP * math.sin(a) BVY[i] = -SP * math.cos(a) end
end
else
var cible = SP * VM * (t < tlent ? 0.7 : 1.0)
var i = 0
while i < size(BX)
var m = math.sqrt(BVX[i] * BVX[i] + BVY[i] * BVY[i])
if m > 0 BVX[i] = BVX[i] / m * cible BVY[i] = BVY[i] / m * cible end
var n = int(cible * dt / 4.0) + 1 var h = dt / n
for j: 1 .. n pas(i, h) end
if BY[i] > 244 BX.remove(i) BY.remove(i) BVX.remove(i) BVY.remove(i) else i += 1 end
end
if size(BX) == 0
lives -= 1 son(150, 200)
if lives <= 0 perdu = true else pose() end
end
end
var j = 0
while j < size(IT)
IY[j] += 90 * dt
if IY[j] > PY - 4 && IY[j] < PY + PH + 6 && IX[j] > rx - 6 && IX[j] < rx + pw + 6 bonus(IT[j]) IX.remove(j) IY.remove(j) IT.remove(j)
elif IY[j] > 244 IX.remove(j) IY.remove(j) IT.remove(j)
else j += 1 end
end
regles()
if perdu || gagne return end
if BUT == "casser_tout" && NBR0 > 0 && NBR == 0 gagne = true son(1320, 300)
elif BUT == "points" && pts >= BUTV gagne = true son(1320, 300)
elif BUT == "survivre" && chrono >= BUTV gagne = true son(1320, 300)
end
end
def dessine()
fond(FOND)
var i = 0 var n = size(GL) var w = int(CW) - 1
while i < n
var g = GR[i]
if g > 0
var l = GL[i] var x = int(i % NC * CW) + 1 var y = TOP + int(i / NC) * BH + 1
if g == 99 rect(x, y, w, BH - 2, METAL)
else
rect(x, y, w, BH - 2, COLB[l])
if g < cellule(l)[0] rect(x, y, w, 3, BLANC) end
if cellule(l)[2] != "" rect(x + w / 2 - 2, y + 3, 4, 4, BLANC) end
end
end
i += 1
end
rect(int(rx), PY, int(pw), PH, BLANC)
for j: 0 .. size(BX) - 1 disque(int(BX[j]), int(BY[j]), 4, JAUNE) end
for j: 0 .. size(IT) - 1 rect(int(IX[j]) - 4, int(IY[j]) - 4, 8, 8, IT[j] == "vie" ? ROUGE : IT[j] == "large" ? rgb(90, 200, 90) : IT[j] == "lent" ? rgb(90, 150, 255) : JAUNE) end
for j: 1 .. lives disque(8 + j * 12, 12, 4, ROUGE) end
var s = string.format("%d", pts)
texte(281, 5, s, FOND, 2) texte(280, 4, s, BLANC, 2)
if MSG != "" && temps() < MSGT texte(21, 121, MSG, FOND, 2) texte(20, 120, MSG, BLANC, 2) end
if gagne texte(81, 101, "BRAVO !", FOND, 4) texte(80, 100, "BRAVO !", JAUNE, 4) end
if perdu texte(81, 101, "PERDU", FOND, 4) texte(80, 100, "PERDU", ROUGE, 4) end
if gagne || perdu texte(91, 161, "P3 : rejouer", BLANC, 2) end
end