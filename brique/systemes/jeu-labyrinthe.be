# Labyrinthe
# exemple: JEU labyrinthe façon Pac-Man (le heros avance tout seul dans un labyrinthe, G tourne a gauche, D tourne a droite, P3 demi-tour ; manger toutes les pastilles, 2 fantomes qui poursuivent, 3 vies) | mots: labyrinthe, pacman, pac-man, pac man, fantome, fantomes, pastille, pastilles, dedale, maze, ghost, ghosts, dots, chase, poursuite, manger, tourner, turn
var NO = 0 var A1 = 0 var A2 = 0 var TIRC = 0 var TXT = 0 var BAND = []
var pts = 0 var vies = 3 var fin = false var gagne = false var reste = 0 var want = -1
var MAP = ["#############", "#.....#.....#", "#.###.#.###.#", "#...........#", "#.##.###.##.#", "#...........#", "#.###.#.###.#", "#.....#.....#", "#############"]
var WL = [] var DT = []                              # 13 x 9 cases de 24 pixels : mur (1), pastille (1)
var EX = [] var EY = [] var ED = [] var EP = [] var EM = []   # 0 = le heros, 1 et 2 = les fantomes : case, direction (0 droite, 1 bas, 2 gauche, 3 haut), avancee vers la case suivante, en route
var DX = [1, 0, -1, 0] var DY = [0, 1, 0, -1]

def libre(x, y)
  return x >= 0 && x < 13 && y >= 0 && y < 9 && WL[y * 13 + x] == 0
end
def place()                                          # chacun retourne a son depart
  EX[0] = 6 EY[0] = 5 ED[0] = 0 EX[1] = 1 EY[1] = 1 ED[1] = 0 EX[2] = 11 EY[2] = 1 ED[2] = 2
  for i: 0 .. 2 EP[i] = 0.0 EM[i] = false end
  want = -1
end
def reset()
  pts = 0 vies = 3 fin = false gagne = false reste = 0
  for y: 0 .. 8
    for x: 0 .. 12
      var k = y * 13 + x
      WL[k] = MAP[y][x] == "#" ? 1 : 0
      DT[k] = MAP[y][x] == "." ? 1 : 0
      reste += DT[k]
    end
  end
  DT[5 * 13 + 6] = 0 reste -= 1
  place()
end
def objet(x, y, k)                                   # un fantome
  disque(x, y - 2, 8, rgb(240, 90, 120)) rect(x - 8, y - 2, 17, 9, rgb(240, 90, 120))
  rect(x - 5, y - 4, 4, 4, rgb(255, 255, 255)) rect(x + 2, y - 4, 4, 4, rgb(255, 255, 255))
end
def perso(x, y)                                      # le heros
  disque(x + 12, y + 12, 10, rgb(255, 220, 60)) rect(x + 14, y + 11, 10, 3, NO)
  disque(x + 14, y + 7, 2, NO)
end
def debut()
  NO = rgb(30, 30, 45) A1 = rgb(60, 90, 220) A2 = rgb(120, 150, 255) TIRC = rgb(255, 220, 60) TXT = rgb(255, 255, 255)
  BAND = [rgb(30, 30, 60), rgb(30, 32, 66), rgb(30, 34, 72), rgb(32, 36, 78), rgb(32, 38, 84), rgb(34, 40, 90), rgb(34, 42, 96), rgb(36, 44, 102), rgb(36, 46, 108), rgb(38, 48, 114)]
  for i: 0 .. 116 WL.push(0) DT.push(0) end
  for i: 0 .. 2 EX.push(0) EY.push(0) ED.push(0) EP.push(0.0) EM.push(false) end
  reset()
end

def px(i) return (EX[i] + DX[ED[i]] * EP[i]) * 24 + 16 end
def py(i) return (EY[i] + DY[ED[i]] * EP[i]) * 24 + 34 end

def tourne0()                                        # le heros prend la direction demandee si elle est libre, sinon il continue tout droit (ou attend)
  if want >= 0 && libre(EX[0] + DX[want], EY[0] + DY[want]) ED[0] = want end
  want = -1
  EM[0] = libre(EX[0] + DX[ED[0]], EY[0] + DY[ED[0]])
end
def tourne(i)                                        # un fantome : pas de demi-tour sauf cul-de-sac ; vers le heros (le 1er, avec un peu de hasard) ou au hasard (le 2e)
  var best = -1 var bd = 9999
  for d: 0 .. 3
    if d != (ED[i] + 2) % 4 && libre(EX[i] + DX[d], EY[i] + DY[d])
      var a = EX[i] + DX[d] - EX[0] var b = EY[i] + DY[d] - EY[0]
      if a < 0 a = -a end
      if b < 0 b = -b end
      var s = a + b
      if i == 2 || hasard(0, 99) < 25 s = hasard(0, 50) end
      if s < bd bd = s best = d end
    end
  end
  if best < 0 best = (ED[i] + 2) % 4 end
  ED[i] = best EM[i] = libre(EX[i] + DX[best], EY[i] + DY[best])
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if appui(G) want = (ED[0] + 3) % 4 end
  if appui(D) want = (ED[0] + 1) % 4 end
  if appui(P3)                                       # demi-tour, meme en route
    if EM[0] EX[0] += DX[ED[0]] EY[0] += DY[ED[0]] EP[0] = 1.0 - EP[0] end
    ED[0] = (ED[0] + 2) % 4
    if !EM[0] tourne0() end
  end
  for i: 0 .. 2
    if EM[i]
      EP[i] += (i == 0 ? 5.5 : 4.2) * dt
      if EP[i] >= 1.0
        EX[i] += DX[ED[i]] EY[i] += DY[ED[i]] EP[i] = 0.0 EM[i] = false
        if i == 0
          var k = EY[0] * 13 + EX[0]
          if DT[k] == 1 DT[k] = 0 reste -= 1 pts += 10 score(pts) son(660, 15) end
          tourne0()
        else tourne(i) end
      end
    elif i == 0 tourne0()
    else tourne(i) end
  end
  for i: 1 .. 2
    var a = px(0) - px(i) var b = py(0) - py(i)
    if a < 0 a = -a end
    if b < 0 b = -b end
    if a < 14 && b < 14
      vies -= 1 son(150, 300)
      if vies <= 0 fin = true else place() end
      return
    end
  end
  if reste <= 0 fin = true gagne = true pts += 100 score(pts) son(988, 200) end
end

def dessine()
  for i: 0 .. 9 rect(0, i * 24, 320, 24, BAND[i]) end
  rect(4, 22, 312, 216, NO)
  for y: 0 .. 8
    for x: 0 .. 12
      var k = y * 13 + x
      if WL[k] == 1
        rect(4 + x * 24, 22 + y * 24, 24, 24, A2) rect(6 + x * 24, 24 + y * 24, 20, 20, A1)
      elif DT[k] == 1 rect(14 + x * 24, 32 + y * 24, 4, 4, TIRC) end
    end
  end
  perso(int(px(0)) - 12, int(py(0)) - 12)
  for i: 1 .. 2 objet(int(px(i)), int(py(i)), 1) end
  texte(6, 4, str(pts), TXT, 2) texte(240, 4, "vies " + str(vies), TXT, 2)
  if fin
    rect(40, 80, 240, 90, NO)
    if gagne texte(90, 90, "BRAVO !", TIRC, 4) else texte(80, 90, "PERDU !", TIRC, 4) end
    texte(100, 140, "P3 : rejouer", TXT, 2)
  end
end
