# Iso
var CARTE = [
"........",
"..#.....",
"..#.....",
"......#.",
"....#...",
"....#...",
"......#.",
"........"]
var N = 8
var SX = [] var SY = []
var DX = [1, 0, -1, 0] var DY = [0, 1, 0, -1]
var hx = 0 var hy = 0 var ax = 0 var ay = 0 var dir = 0 var anim = 1.0
var gx = 0 var gy = 0 var pts = 0
var SOL1 = 0 var SOL2 = 0 var DESSUS = 0 var GAUCHE = 0 var DROITE = 0 var OR = 0 var ROUGE = 0 var BLANC = 0

def bloc(x, y)
  if x < 0 || y < 0 || x >= N || y >= N return true end
  return CARTE[y][x] == "#"
end
def placer()
  gx = hasard(0, N - 1) gy = hasard(0, N - 1)
  while bloc(gx, gy) || (gx == hx && gy == hy) gx = hasard(0, N - 1) gy = hasard(0, N - 1) end
end

def debut()
  SOL1 = rgb(80, 160, 90) SOL2 = rgb(70, 145, 80) DESSUS = rgb(200, 170, 120) GAUCHE = rgb(150, 110, 70) DROITE = rgb(120, 85, 55)
  OR = rgb(255, 220, 60) ROUGE = rgb(232, 50, 43) BLANC = rgb(255, 255, 255)
  for y: 0 .. N - 1
    for x: 0 .. N - 1 SX.push(160 + (x - y) * 16) SY.push(60 + (x + y) * 8) end
  end
  placer()
end

def maj(dt)
  if appui(G) dir = (dir + 3) % 4 end
  if appui(D) dir = (dir + 1) % 4 end
  if anim < 1 anim += 5 * dt end
  if anim >= 1 && bouton(P3)
    var tx = hx + DX[dir] var ty = hy + DY[dir]
    if !bloc(tx, ty) ax = hx ay = hy hx = tx hy = ty anim = 0.0 son(330, 20) end
  end
  if anim >= 1 && hx == gx && hy == gy pts += 1 score(pts) son(988, 80) placer() end
end

def cube(sx, sy)
  tri(sx - 16, sy - 8, sx, sy, sx, sy + 16, GAUCHE) tri(sx - 16, sy - 8, sx, sy + 16, sx - 16, sy + 8, GAUCHE)
  tri(sx, sy, sx + 16, sy - 8, sx + 16, sy + 8, DROITE) tri(sx, sy, sx + 16, sy + 8, sx, sy + 16, DROITE)
  tri(sx, sy - 16, sx + 16, sy - 8, sx, sy, DESSUS) tri(sx, sy - 16, sx, sy, sx - 16, sy - 8, DESSUS)
end

def dessine()
  fond(rgb(20, 30, 60))
  for i: 0 .. N * N - 1
    var sx = SX[i] var sy = SY[i] var c = ((i % N) + (i / N)) % 2 ? SOL1 : SOL2
    tri(sx, sy, sx + 16, sy + 8, sx, sy + 16, c) tri(sx, sy, sx, sy + 16, sx - 16, sy + 8, c)
  end
  var a = anim > 1 ? 1.0 : anim
  var px = 160 + int(((ax - ay) + ((hx - hy) - (ax - ay)) * a) * 16)
  var py = 60 + int(((ax + ay) + ((hx + hy) - (ax + ay)) * a) * 8) + 8
  for y: 0 .. N - 1                                 # ligne par ligne = du fond vers l'avant
    for x: 0 .. N - 1
      if CARTE[y][x] == "#" cube(SX[y * N + x], SY[y * N + x]) end
      if x == gx && y == gy disque(SX[y * N + x], SY[y * N + x] + 4, 4, OR) end
      if x == hx && y == hy
        disque(px, py - 6, 6, ROUGE)
        disque(px + (DX[dir] - DY[dir]) * 5, py - 6 + (DX[dir] + DY[dir]) * 3, 2, BLANC)
      end
    end
  end
  texte(4, 4, str(pts), BLANC, 2)
end
