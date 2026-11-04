# Labyrinthe
# exemple: Vue de dessus (labyrinthe en texte, tourner et avancer avec 3 boutons) | mots: labyrinthe, dessus, tourner, avancer, mur, sortie, cle, chemin, couloir, tank
import math
var NIV = [
"################",
"#o.....#.......#",
"#.####.#.#####.#",
"#.#..o.....#o..#",
"#.#.######.#.###",
"#......#.......#",
"###.##.#.####.##",
"#o..#......#...#",
"#.###.####.#.#.#",
"#.....#o.....#o#",
"#.###...####...#",
"################"]
var T = 20
var MURS = []
var GX = [] var GY = [] var PRIS = []
var hx = 0.0 var hy = 0.0 var ang = 0.0
var reste = 0 var chrono = 0.0 var fini = false
var SOL = 0 var MUR = 0 var OR = 0 var ROUGE = 0 var BLANC = 0

def mur(x, y)
  if x < 0 || y < 0 return true end
  var c = int(x / T) var r = int(y / T)
  if c > 15 || r > 11 return true end
  return NIV[r][c] == "#"
end
def libre(x, y)
  return !(mur(x - 6, y - 6) || mur(x + 6, y - 6) || mur(x - 6, y + 6) || mur(x + 6, y + 6))
end
def nouvelle()
  for i: 0 .. size(PRIS) - 1 PRIS[i] = false end
  reste = size(PRIS) hx = 30.0 hy = 110.0 ang = 0.0 chrono = 0.0 fini = false
end

def debut()
  SOL = rgb(30, 30, 50) MUR = rgb(60, 90, 200) OR = rgb(255, 220, 60) ROUGE = rgb(232, 50, 43) BLANC = rgb(255, 255, 255)
  for r: 0 .. 11
    var c = 0
    while c < 16
      if NIV[r][c] == "#"
        var l = 0
        while c + l < 16 && NIV[r][c + l] == "#" l += 1 end
        MURS.push([c * T, r * T, l * T]) c += l
      else
        if NIV[r][c] == "o" GX.push(c * T + 10) GY.push(r * T + 10) PRIS.push(false) end
        c += 1
      end
    end
  end
  nouvelle()
end

def maj(dt)
  if fini
    if appui(P3) nouvelle() end
  else
    if bouton(G) ang -= 3.5 * dt end
    if bouton(D) ang += 3.5 * dt end
    if bouton(P3)
      var nx = hx + math.cos(ang) * 70 * dt
      var ny = hy + math.sin(ang) * 70 * dt
      if libre(nx, hy) hx = nx end
      if libre(hx, ny) hy = ny end
    end
    chrono += dt
    for i: 0 .. size(GX) - 1
      var dx = GX[i] - hx var dy = GY[i] - hy
      if !PRIS[i] && dx * dx + dy * dy < 100 PRIS[i] = true reste -= 1 son(988, 60) end
    end
    if reste == 0
      fini = true son(1320, 300)
      var p = 1000 - int(chrono * 10)
      score(p < 0 ? 0 : p)
    end
  end
end

def dessine()
  fond(SOL)
  for m: MURS rect(m[0], m[1], m[2], T, MUR) end
  for i: 0 .. size(GX) - 1
    if !PRIS[i] disque(GX[i], GY[i], 4, OR) end
  end
  var x = int(hx) var y = int(hy)
  disque(x, y, 6, ROUGE)
  ligne(x, y, x + int(math.cos(ang) * 9), y + int(math.sin(ang) * 9), BLANC)
  texte(4, 6, str(reste) + "  " + str(int(chrono)) + " s", BLANC, 1)
  if fini texte(60, 100, "Bravo !", BLANC, 4) end
end
