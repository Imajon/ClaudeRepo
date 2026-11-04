# Plateforme
var NIV = [
"................................................",
"................................................",
"................................................",
"................................................",
"...................................o............",
"..................................###...........",
"..........................o.....................",
".........................###............o.......",
"...............o......................####......",
"..............###............................F..",
".......o...........#####.....................#..",
"......###...................................##..",
"..................................###......###..",
"##########...##########...###########...########",
"##########...##########...###########...########"]
var T = 16 var LC = 0
var COL = []
var PCX = [] var PCY = [] var PRIS = []
var FX = 0 var FY = 0
var px = 0.0 var py = 0.0 var vx = 0.0 var vy = 0.0 var sol = false
var camx = 0 var pts = 0 var gagne = false
var CIEL = 0 var TERRE = 0 var HERBE = 0 var OR = 0 var ROUGE = 0 var BLANC = 0

def plein(c, r)
  if c < 0 || c >= LC return true end
  if r < 0 || r >= 15 return false end
  return NIV[r][c] == "#"
end
def libre(x, y)
  var c0 = int(x / T) var c1 = int((x + 9) / T) var r0 = int(y / T) var r1 = int((y + 13) / T)
  return !(plein(c0, r0) || plein(c1, r0) || plein(c0, r1) || plein(c1, r1))
end
def depart() px = 32.0 py = 194.0 vx = 0.0 vy = 0.0 end

def debut()
  CIEL = rgb(120, 190, 240) TERRE = rgb(140, 90, 50) HERBE = rgb(60, 170, 70) OR = rgb(255, 220, 60) ROUGE = rgb(232, 50, 43) BLANC = rgb(255, 255, 255)
  LC = size(NIV[0])
  for c: 0 .. LC - 1
    var col = [] var r = 0
    while r < 15
      if NIV[r][c] == "#"
        var h = 0
        while r + h < 15 && NIV[r + h][c] == "#" h += 1 end
        col.push([r, h]) r += h
      else
        if NIV[r][c] == "o" PCX.push(c) PCY.push(r) PRIS.push(false) end
        if NIV[r][c] == "F" FX = c FY = r end
        r += 1
      end
    end
    COL.push(col)
  end
  depart()
end

def maj(dt)
  if gagne
    if appui(P3)
      for i: 0 .. size(PRIS) - 1 PRIS[i] = false end
      pts = 0 gagne = false depart()
    end
  else
    vx = 0.0
    if bouton(G) vx = -110.0 end
    if bouton(D) vx = 110.0 end
    if appui(P3) && sol vy = -330.0 son(660, 40) end
    if !bouton(P3) && vy < -120 vy = -120.0 end
    vy += 900 * dt
    if vy > 400 vy = 400.0 end
    var nx = px + vx * dt
    if nx < 0 nx = 0.0 end
    if libre(nx, py) px = nx
    elif vx > 0 px = int((nx + 9) / T) * T - 10.0
    elif vx < 0 px = (int(nx / T) + 1) * T + 0.0
    end
    var ny = py + vy * dt
    if libre(px, ny) py = ny
    elif vy > 0 py = int((ny + 13) / T) * T - 14.0 vy = 0.0
    else py = (int(ny / T) + 1) * T + 0.0 vy = 0.0
    end
    sol = vy >= 0 && !libre(px, py + 1)
    if sol vy = 0.0 end
    if py > 250 depart() son(150, 200) end
    var cc = int((px + 5) / T) var cr = int((py + 7) / T)
    for i: 0 .. size(PCX) - 1
      if !PRIS[i] && PCX[i] == cc && PCY[i] == cr PRIS[i] = true pts += 1 score(pts) son(988, 60) end
    end
    if cc == FX && cr == FY gagne = true son(1320, 300) end
  end
  camx = int(px) - 150
  if camx < 0 camx = 0 elif camx > LC * T - 320 camx = LC * T - 320 end
end

def dessine()
  fond(CIEL)
  camera(camx, 0)
  var c0 = camx / T
  for c: c0 .. c0 + 20
    if c < LC
      for b: COL[c]
        rect(c * T, b[0] * T, T, b[1] * T, TERRE)
        rect(c * T, b[0] * T, T, 3, HERBE)
      end
    end
  end
  for i: 0 .. size(PCX) - 1
    if !PRIS[i] disque(PCX[i] * T + 8, PCY[i] * T + 8, 4, OR) end
  end
  rect(FX * T + 7, FY * T, 2, 16, BLANC)
  tri(FX * T + 9, FY * T, FX * T + 9, FY * T + 8, FX * T + 16, FY * T + 4, ROUGE)
  rect(int(px), int(py), 10, 14, ROUGE)
  camera(0, 0)
  texte(4, 4, str(pts), BLANC, 2)
  if gagne texte(70, 100, "Gagne !", BLANC, 4) end
end
