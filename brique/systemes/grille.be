# Grille
# exemple: SYSTEME niveau en texte (murs et objets dans une grille de lettres, murs voisins fusionnes en un rectangle, collision de boite, glisser le long des murs, ramasser, camera qui suit dans un monde plus grand que l'ecran) | mots: niveau, mur, murs, labyrinthe, carte, case, decor, camera, monde, grand, cle, porte, sortie, piece, ramasser, tresor
var NIV = [
"##############################",
"#............#...............#",
"#..o.........#.......o.......#",
"#......####..#..######.......#",
"#............................#",
"#..######.........#....o.....#",
"#.......#.........#..........#",
"##############################"]
var T = 20 var LC = 30 var LR = 8
var PRIS = {}                                       # objets ramasses : cle = r * 100 + c
var MURS = []                                       # murs fusionnes : [x, y, largeur], calcules une fois
var px = 30.0 var py = 30.0 var camx = 0 var pts = 0

def plein(x, y)                                     # le pixel (x, y) est-il dans un mur ?
  var c = int(x / T) var r = int(y / T)
  if x < 0 || y < 0 || c >= LC || r >= LR return true end
  return NIV[r][c] == "#"
end
def libre(x, y) return !(plein(x, y) || plein(x + 11, y) || plein(x, y + 11) || plein(x + 11, y + 11)) end   # les 4 coins de la boite 12 x 12

def debut()                                         # les "#" voisins d'une ligne deviennent un seul rectangle : bien moins de dessins
  for r: 0 .. LR - 1
    var c = 0
    while c < LC
      if NIV[r][c] == "#"
        var l = 0
        while c + l < LC && NIV[r][c + l] == "#" l += 1 end
        MURS.push([c * T, r * T, l * T]) c += l
      else c += 1 end
    end
  end
end

def maj(dt)
  var vx = 0 var vy = bouton(P3) ? -90 : 60         # P3 tenu = monter, sinon descendre
  if bouton(G) vx = -90 end
  if bouton(D) vx = 90 end
  var nx = px + vx * dt var ny = py + vy * dt
  if libre(nx, py) px = nx end                      # x puis y separement : on glisse le long d'un mur au lieu de coller
  if libre(px, ny) py = ny end
  var c = int((px + 6) / T) var r = int((py + 6) / T)
  if NIV[r][c] == "o" && !PRIS.contains(r * 100 + c) PRIS[r * 100 + c] = true pts += 1 score(pts) son(988, 60) end
  camx = int(px) - 160
  if camx < 0 camx = 0 elif camx > LC * T - 320 camx = LC * T - 320 end   # camera bornee au monde
end

def dessine()
  fond(rgb(30, 30, 50))
  camera(camx, 0)                                   # tout ce qui suit est decale : on dessine en coordonnees du monde
  for m: MURS
    if m[0] < camx + 320 && m[0] + m[2] > camx rect(m[0], m[1], m[2], T, rgb(60, 90, 200)) end   # seulement ce qui est a l'ecran
  end
  var c0 = camx / T
  for r: 0 .. LR - 1
    for c: c0 .. c0 + 16                            # objets : seulement les colonnes visibles
      if c < LC && NIV[r][c] == "o" && !PRIS.contains(r * 100 + c) disque(c * T + 10, r * T + 10, 4, rgb(255, 220, 60)) end
    end
  end
  rect(int(px), int(py), 12, 12, rgb(232, 50, 43))
  camera(0, 0)                                      # le score par-dessus, sans camera
  texte(4, 200, str(pts), rgb(255, 255, 255), 2)
end
