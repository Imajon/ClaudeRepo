# Formation
# exemple: SYSTEME formation d'ennemis (une grille qui se deplace en bloc, descend a chaque bord, accelere quand il en reste peu, ennemis qui ripostent avec des bombes) | mots: envahisseurs, invaders, formation, armee, aliens, extraterrestres, descendent, riposte, ennemis qui tirent, tirent, bombe, bombes, defendre, vague, invader, army, coming down, shoot back, enemies that shoot, bomb, defend, wave
var CO = 8 var LI = 3 var VIV = []
var fx = 20.0 var fy = 30.0 var dir = 1 var reste = 0
var x = 152.0 var tx = 0.0 var ty = -1.0            # mon tir, un seul a la fois (ty < 0 = pas de tir)
var BX = [0.0, 0.0, 0.0] var BY = [-1.0, -1.0, -1.0]   # bombes ennemies, BY < 0 = libre
var tir_e = 1.0

def vague()
  VIV = []
  for i: 0 .. CO * LI - 1 VIV.push(true) end
  reste = CO * LI fx = 20.0 fy = 30.0
  for k: 0 .. 2 BY[k] = -1.0 end
end
def debut() vague() end

def maj(dt)
  if bouton(G) x -= 140 * dt end
  if bouton(D) x += 140 * dt end
  if x < 0 x = 0.0 elif x > 304 x = 304.0 end
  if appui(P3) && ty < 0 tx = x + 7 ty = 210.0 son(1200, 20) end
  if ty >= 0 ty -= 320 * dt end
  var morts = CO * LI - reste
  fx += dir * (20 + morts * 5) * dt                 # moins il en reste, plus le bloc va vite
  if fx < 4 || fx + CO * 30 > 316 dir = -dir fx += dir * 4 fy += 10 end   # un bord : tout le bloc descend
  for i: 0 .. CO * LI - 1
    var ex = fx + (i % CO) * 30 var ey = fy + (i / CO) * 22
    if VIV[i] && ty >= 0 && tx > ex && tx < ex + 20 && ty > ey && ty < ey + 14
      VIV[i] = false ty = -1.0 reste -= 1 score(morts + 1) son(440, 50)
    end
  end
  if reste == 0 vague() return end
  tir_e -= dt
  if tir_e <= 0                                     # riposte de plus en plus frequente
    tir_e = 1.2 - morts * 0.03
    var j = hasard(0, CO * LI - 1)
    for k: 0 .. 2
      if VIV[j] && BY[k] < 0 BX[k] = fx + (j % CO) * 30 + 10 BY[k] = fy + (j / CO) * 22 + 14 break end
    end
  end
  for k: 0 .. 2
    if BY[k] >= 0
      BY[k] += 120 * dt
      if BY[k] > 240 BY[k] = -1.0 end
      if BY[k] > 212 && BY[k] < 228 && BX[k] > x && BX[k] < x + 16 son(120, 300) vague() return end   # touche : on recommence
    end
  end
  if fy + LI * 22 > 205 son(120, 300) vague() end   # ils sont arrives en bas
end

def dessine()
  fond(rgb(5, 5, 20))
  for i: 0 .. CO * LI - 1
    if VIV[i] rect(int(fx) + (i % CO) * 30, int(fy) + (i / CO) * 22, 20, 14, rgb(120, 230, 120)) end
  end
  for k: 0 .. 2 if BY[k] >= 0 rect(int(BX[k]), int(BY[k]), 3, 8, rgb(255, 120, 60)) end end
  if ty >= 0 rect(int(tx), int(ty), 2, 6, rgb(255, 255, 255)) end
  tri(int(x) + 8, 210, int(x), 228, int(x) + 16, 228, rgb(232, 50, 43))
end
