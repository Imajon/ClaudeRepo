# Serpent
import string
var C = 16 var W = 20 var H = 14
var DX = [1, 0, -1, 0] var DY = [0, 1, 0, -1]
var SX = [] var SY = []
var dir = 0 var nd = 0 var lon = 4 var pas = 0.0
var AX = [] var AY = [] var alon = 0 var vu = -10.0
var k = 0 var fx = 0 var fy = 0
var FOND = 0 var BORD = 0 var MOI = 0 var AUTRE = 0 var FRUIT = 0 var BLANC = 0

def fruit() fx = (k * 37 + 11) % W fy = (k * 53 + 7) % H end
def touche(lx, ly, x, y)
  for i: 0 .. size(lx) - 1
    if lx[i] == x && ly[i] == y return true end
  end
  return false
end
def nouveau()
  SX = [hasard(3, W - 4)] SY = [hasard(2, H - 3)]
  lon = 4 dir = hasard(0, 3) nd = dir
end

def debut()
  FOND = rgb(15, 25, 20) BORD = rgb(60, 80, 70) MOI = rgb(80, 220, 90) AUTRE = rgb(80, 150, 255) FRUIT = rgb(232, 50, 43) BLANC = rgb(255, 255, 255)
  fruit() nouveau()
end

def maj(dt)
  if appui(G) && nd == dir nd = (dir + 3) % 4 end
  if appui(D) && nd == dir nd = (dir + 1) % 4 end
  pas += (bouton(P3) ? 12 : 7) * dt
  if pas >= 1
    pas -= 1
    dir = nd
    var n = size(SX)
    var x = SX[n - 1] + DX[dir] var y = SY[n - 1] + DY[dir]
    if x < 0 || y < 0 || x >= W || y >= H || touche(SX, SY, x, y) || touche(AX, AY, x, y)
      son(120, 300) envoie("m") nouveau()
    else
      SX.push(x) SY.push(y)
      if x == fx && y == fy lon += 1 score(lon) son(988, 60) k += 1 fruit() envoie("f," + str(k)) end
      if size(SX) > lon SX.remove(0) SY.remove(0) end
      envoie("t," + str(x) + "," + str(y) + "," + str(lon))
    end
  end
  for m: recois()                                   # l'autre console : sa tete a chaque pas, le fruit mange, sa mort
    if m[0] != moi()
      var p = string.split(m[1], ",")
      if p[0] == "t"
        AX.push(int(p[1])) AY.push(int(p[2])) alon = int(p[3]) vu = temps()
        while size(AX) > alon AX.remove(0) AY.remove(0) end
      elif p[0] == "f"
        if int(p[1]) > k k = int(p[1]) fruit() end
      elif p[0] == "m"
        AX = [] AY = []
      end
    end
  end
  if size(AX) > 0 && temps() - vu > 2 AX = [] AY = [] end   # l'autre console est partie
end

def dessine()
  fond(FOND)
  cadre(0, 16, W * C, H * C, BORD)
  disque(fx * C + 8, fy * C + 24, 6, FRUIT)
  for i: 0 .. size(AX) - 1 rect(AX[i] * C + 1, AY[i] * C + 17, C - 2, C - 2, AUTRE) end
  for i: 0 .. size(SX) - 1 rect(SX[i] * C + 1, SY[i] * C + 17, C - 2, C - 2, MOI) end
  texte(4, 4, str(lon), MOI, 1)
  if size(AX) > 0 texte(280, 4, str(alon), AUTRE, 1) end
end
