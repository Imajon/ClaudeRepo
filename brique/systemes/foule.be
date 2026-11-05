# Foule
# exemple: SYSTEME foule et groupes de soldats (plusieurs groupes, chacun garde un masque de bits = quels soldats sont encore vivants : le tir retire CELUI qu'il touche, pas le dernier, et le dessin lit le masque, jamais un compteur ; foule en desordre autour du joueur, pas une grille) | mots: foule, soldat, soldats, troupe, armee, groupe, horde, gens, disparai, la ou, carre de, en ligne
import math
var NF = 40                                         # soldats du joueur affiches au plus (au-dela, seul le nombre grossit)
var FX = [] var FY = [] var FR = []                 # foule : decalage autour du joueur + rang (0 = au centre)
var NG = 12                                         # places dans un groupe (12 bits)
var PX = [] var PY = []                             # place k dans un groupe (la meme pour tous les groupes)
var NGR = 3                                         # groupes a l'ecran en meme temps
var GM = [] var GX = [] var GY = []                 # GM[g] = masque : bit k a 1 = le soldat k du groupe g est vivant
var NB = 8 var BX = [] var BY = []
var x = 160.0 var armee = 5 var tir = 0.0

def nouveau_groupe(g)
  GX[g] = real(hasard(10, 240)) GY[g] = real(-40 - g * 90)
  GM[g] = (1 << NG) - 1                             # les 12 vivants ; groupe de n ennemis : (1 << n) - 1
end

def debut()
  for i: 0 .. NF - 1                                # tournesol : le rang i s'eloigne du centre, angle d'or -> pas de lignes
    var r = 3.6 * math.sqrt(i) + hasard(0, 2)
    FX.push(int(r * math.cos(i * 2.39996))) FY.push(int(r * math.sin(i * 2.39996) * 0.5)) FR.push(i)
  end
  for i: 1 .. NF - 1                                # tri par y : ceux de devant sont dessines par-dessus
    var k = i
    while k > 0 && FY[k - 1] > FY[k]
      var t = FX[k] FX[k] = FX[k - 1] FX[k - 1] = t
      t = FY[k] FY[k] = FY[k - 1] FY[k - 1] = t
      t = FR[k] FR[k] = FR[k - 1] FR[k - 1] = t
      k -= 1
    end
  end
  for k: 0 .. NG - 1                                # places fixes dans un groupe (un peu en desordre aussi)
    PX.push((k % 4) * 18 + hasard(-3, 3)) PY.push(int(k / 4) * 14 + hasard(-2, 2))
  end
  for g: 0 .. NGR - 1 GM.push(0) GX.push(0.0) GY.push(0.0) nouveau_groupe(g) end
  for i: 0 .. NB - 1 BX.push(0.0) BY.push(-10.0) end
end

def soldat(px, py, c)
  rect(px - 2, py - 6, 5, 3, rgb(90, 90, 90))       # casque
  rect(px - 2, py - 3, 5, 6, c)                     # corps
end

def maj(dt)
  if bouton(G) x -= 150 * dt end
  if bouton(D) x += 150 * dt end
  if x < 20 x = 20.0 elif x > 300 x = 300.0 end
  if appui(P3) armee += 5 end                       # (dans un jeu : une porte +5, x2...)
  tir -= dt
  if tir <= 0
    tir = 0.25
    for i: 0 .. NB - 1 if BY[i] < 0 BX[i] = x + hasard(-12, 12) BY[i] = 190.0 break end end
  end
  for g: 0 .. NGR - 1
    GY[g] += 30 * dt
    if GY[g] > 240 || GM[g] == 0 nouveau_groupe(g) end   # masque a 0 = groupe detruit
  end
  for i: 0 .. NB - 1
    if BY[i] >= 0
      BY[i] -= 300 * dt
      for g: 0 .. NGR - 1                           # on cherche LE soldat touche (distance au carre) et on eteint son bit, lui seul
        for k: 0 .. NG - 1
          var dx = BX[i] - (GX[g] + PX[k]) var dy = BY[i] - (GY[g] + PY[k])
          if BY[i] >= 0 && (GM[g] & (1 << k)) && dx * dx + dy * dy < 50 GM[g] = GM[g] & ~(1 << k) BY[i] = -10.0 son(440, 30) end
        end
      end
    end
  end
end

def dessine()
  fond(rgb(46, 84, 52))
  for g: 0 .. NGR - 1                               # le dessin lit le masque (qui est vivant), jamais un compteur
    for k: 0 .. NG - 1
      if GM[g] & (1 << k) soldat(int(GX[g] + PX[k]), int(GY[g] + PY[k]), rgb(200, 40, 60)) end
    end
  end
  for i: 0 .. NB - 1 if BY[i] >= 0 rect(int(BX[i]), int(BY[i]), 2, 5, rgb(255, 230, 80)) end end
  var n = armee > NF ? NF : armee
  for i: 0 .. NF - 1                                # les n premiers rangs : la foule s'elargit en cercle quand l'armee grandit
    if FR[i] < n soldat(int(x) + FX[i], 212 + FY[i] - (FR[i] + int(temps() * 6)) % 2, rgb(70, 160, 255)) end
  end
  texte(int(x) - 6, 186, str(armee), rgb(255, 255, 255), 1)
end
