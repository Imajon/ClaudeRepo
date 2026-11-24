# Tetris
# exemple: JEU tetris (7 pieces qui tombent dans un puits, G et D deplacent, P3 tourne et tenu fait descendre vite, lignes pleines effacees, score, niveau, perdu quand le tas touche le haut) | mots: tetris, blocs, bloc, pieces, piece, empiler, empile, lignes, puits, ligne, tourner, tomber, stack, block, blocks, falling, rotate, line, puzzle
var W = 10 var H = 20 var S = 11 var BX = 100 var BY = 10
var BASE = [[0, 1, 1, 1, 2, 1, 3, 1], [0, 0, 1, 0, 0, 1, 1, 1], [1, 0, 0, 1, 1, 1, 2, 1], [1, 0, 2, 0, 0, 1, 1, 1], [0, 0, 1, 0, 1, 1, 2, 1], [0, 0, 0, 1, 1, 1, 2, 1], [2, 0, 0, 1, 1, 1, 2, 1]]
var NB = [4, 2, 3, 3, 3, 3, 3]                      # taille de la boite de chaque piece (I, O, T, S, Z, J, L)
var PTSL = [0, 100, 300, 500, 800]
var SH = []                                          # SH[piece * 4 + rotation] = 4 cases (x, y) dans la boite, precalculees
var GRI = []                                         # la grille : 0 vide, sinon 1 + numero de piece
var pc = 0 var pr = 0 var qx = 3 var qy = 0 var nx = 0 var tg = 0.0
var pts = 0 var lignes = 0 var niv = 0 var fin = false var tm = 0.0 var ldir = 0 var rep = 0.0 var ph = 0.0
var PAL = [] var FOND = 0 var BL = 0 var GR = 0 var BORD = 0 var NOIR = 0

def libre(p, r, x, y)
  var c = SH[p * 4 + r] var k = 0
  while k < 4
    var cx = x + c[2 * k] var cy = y + c[2 * k + 1]
    if cx < 0 || cx >= W || cy >= H return false end
    if cy >= 0 && GRI[cy * W + cx] != 0 return false end
    k += 1
  end
  return true
end

def nouveau()
  pc = nx nx = hasard(0, 6) pr = 0 qx = 3 qy = 0 tg = 0.0
  if !libre(pc, pr, qx, qy) fin = true tm = 0.0 son(150, 300) end
end

def fige()                                           # pose la piece, efface les lignes pleines, compte les points
  var c = SH[pc * 4 + pr] var k = 0
  while k < 4
    var yy = qy + c[2 * k + 1]
    if yy >= 0 GRI[yy * W + qx + c[2 * k]] = pc + 1 end
    k += 1
  end
  var n = 0 var y = H - 1
  while y >= 0
    var plein = true var x = 0
    while x < W
      if GRI[y * W + x] == 0 plein = false break end
      x += 1
    end
    if plein
      n += 1
      var r = y
      while r > 0
        var x2 = 0
        while x2 < W GRI[r * W + x2] = GRI[(r - 1) * W + x2] x2 += 1 end
        r -= 1
      end
      var x3 = 0
      while x3 < W GRI[x3] = 0 x3 += 1 end
    else y -= 1 end
  end
  if n > 0
    pts += PTSL[n] * (niv + 1) lignes += n niv = lignes / 10 score(pts) son(n == 4 ? 1320 : 880, 120)
  else son(220, 30) end
end

def reset()
  var i = 0
  while i < W * H GRI[i] = 0 i += 1 end
  pts = 0 lignes = 0 niv = 0 fin = false ldir = 0 rep = 0.0 ph = 0.0
  nx = hasard(0, 6)
  nouveau()
  score(0)
end

def debut()
  var p = 0
  while p < 7
    var c = BASE[p] var r = 0
    while r < 4
      SH.push(c)
      var d = [0, 0, 0, 0, 0, 0, 0, 0] var k = 0
      while k < 4 d[2 * k] = NB[p] - 1 - c[2 * k + 1] d[2 * k + 1] = c[2 * k] k += 1 end   # un quart de tour : (x, y) -> (n - 1 - y, x)
      c = d r += 1
    end
    p += 1
  end
  var i = 0
  while i < W * H GRI.push(0) i += 1 end
  PAL = [rgb(60, 210, 230), rgb(250, 220, 60), rgb(180, 90, 220), rgb(80, 200, 90), rgb(230, 70, 70), rgb(70, 110, 230), rgb(250, 150, 50)]
  FOND = rgb(18, 20, 36) BL = rgb(240, 244, 255) GR = rgb(120, 130, 160) BORD = rgb(70, 80, 120) NOIR = rgb(8, 10, 20)
  reset()
end

def bouger(d)
  if libre(pc, pr, qx + d, qy) qx += d son(330, 10) end
end

def tourner()                                        # un quart de tour ; si ca gene le mur ou le tas, on essaie de decaler un peu
  var r = (pr + 1) % 4
  var dx = [0, -1, 1, -2, 2] var k = 0
  while k < 5
    if libre(pc, r, qx + dx[k], qy) pr = r qx += dx[k] son(520, 15) return end
    k += 1
  end
end

def maj(dt)
  if fin
    tm += dt
    if tm > 0.7 && appui(P3) reset() end
    return
  end
  var dir = 0
  if bouton(G) && !bouton(D) dir = -1 elif bouton(D) && !bouton(G) dir = 1 end
  if dir != 0
    if dir != ldir bouger(dir) rep = 0.22
    else
      rep -= dt
      if rep <= 0 bouger(dir) rep = 0.07 end
    end
  end
  ldir = dir
  if appui(P3) tourner() end
  if bouton(P3) ph += dt else ph = 0.0 end
  var iv = 0.8 - niv * 0.07
  if iv < 0.08 iv = 0.08 end
  if ph > 0.2 && iv > 0.04 iv = 0.04 end
  tg += dt
  if tg >= iv
    tg = 0.0
    if libre(pc, pr, qx, qy + 1) qy += 1 else fige() nouveau() end
  end
end

def dessine()
  fond(FOND)
  rect(BX - 3, BY - 3, W * S + 5, H * S + 5, BORD) rect(BX, BY, W * S - 1, H * S - 1, NOIR)
  var y = 0
  while y < H
    var x = 0
    while x < W
      var v = GRI[y * W + x]
      if v > 0 rect(BX + x * S, BY + y * S, S - 1, S - 1, PAL[v - 1]) end
      x += 1
    end
    y += 1
  end
  var c = SH[pc * 4 + pr] var k = 0
  while k < 4
    rect(BX + (qx + c[2 * k]) * S, BY + (qy + c[2 * k + 1]) * S, S - 1, S - 1, PAL[pc])
    k += 1
  end
  texte(8, 14, "SCORE", GR, 1) texte(8, 26, str(pts), BL, 2)
  texte(8, 56, "LIGNES", GR, 1) texte(8, 68, str(lignes), BL, 2)
  texte(8, 98, "NIVEAU", GR, 1) texte(8, 110, str(niv + 1), BL, 2)
  texte(4, 170, "G / D bouger", GR, 1) texte(4, 184, "P3 tourner", GR, 1) texte(4, 198, "P3 tenu : vite", GR, 1)
  texte(226, 14, "SUIVANT", GR, 1)
  var n = SH[nx * 4] k = 0
  while k < 4
    rect(236 + n[2 * k] * S, 34 + n[2 * k + 1] * S, S - 1, S - 1, PAL[nx])
    k += 1
  end
  if fin
    rect(BX, 80, W * S - 1, 70, NOIR)
    texte(BX + 20, 90, "PERDU", rgb(240, 80, 80), 3)
    if tm > 0.7 texte(BX + 9, 125, "P3 : rejouer", BL, 1) end
  end
end
