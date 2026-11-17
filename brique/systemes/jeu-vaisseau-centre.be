# Vaisseau central
# exemple: JEU vaisseau au centre qui tourne (G et D tournent, P3 tire dans la direction du vaisseau, des asteroides arrivent de tous les bords, 3 vies) | mots: vaisseau, tourne, tourner, centre, asteroides, asteroide, asteroids, rotation, tir, tirer, tire, espace, tourelle, canon, defendre, shooter, spaceship, rotate, turret, shoot, space
import math
var ang = 0.0 var pts = 0 var vies = 3 var fin = false var inv = 0.0 var prochain = 1.0 var rythme = 1.0
var BX = [] var BY = [] var BVX = [] var BVY = [] var BV = []   # 6 tirs : BV = duree de vie restante (<= 0 : libre)
var AX = [] var AY = [] var AVX = [] var AVY = [] var AA = []   # 6 asteroides, AA = actif

def reset()
  ang = 0.0 pts = 0 vies = 3 fin = false inv = 0.0 prochain = 1.0 rythme = 1.0
  for i: 0 .. 5 BV[i] = 0.0 AA[i] = false end
end
def debut()
  for i: 0 .. 5
    BX.push(0.0) BY.push(0.0) BVX.push(0.0) BVY.push(0.0) BV.push(0.0)
    AX.push(0.0) AY.push(0.0) AVX.push(0.0) AVY.push(0.0) AA.push(false)
  end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) ang -= 3.5 * dt end
  if bouton(D) ang += 3.5 * dt end
  if inv > 0 inv -= dt end
  if appui(P3)
    for i: 0 .. 5
      if BV[i] <= 0
        BX[i] = 160.0 BY[i] = 120.0 BV[i] = 1.2
        BVX[i] = math.cos(ang) * 260 BVY[i] = math.sin(ang) * 260 son(1200, 20) break
      end
    end
  end
  rythme += 0.02 * dt prochain -= dt * rythme
  if prochain <= 0
    prochain = 1.0
    for i: 0 .. 5
      if !AA[i]
        var a = real(hasard(0, 628)) / 100.0         # arrive d'un point du bord, vise le centre
        AX[i] = 160.0 + math.cos(a) * 200 AY[i] = 120.0 + math.sin(a) * 200
        AVX[i] = -math.cos(a) * 40 AVY[i] = -math.sin(a) * 40 AA[i] = true
        break
      end
    end
  end
  for i: 0 .. 5
    if BV[i] > 0 BV[i] -= dt BX[i] += BVX[i] * dt BY[i] += BVY[i] * dt end
    if AA[i]
      AX[i] += AVX[i] * dt AY[i] += AVY[i] * dt
      var dx = AX[i] - 160 var dy = AY[i] - 120
      if dx * dx + dy * dy < 200 && inv <= 0        # touche le vaisseau (distance au carre)
        AA[i] = false vies -= 1 inv = 1.5 son(150, 200)
      end
      for j: 0 .. 5
        var ex = BX[j] - AX[i] var ey = BY[j] - AY[i]
        if BV[j] > 0 && AA[i] && ex * ex + ey * ey < 144 AA[i] = false BV[j] = 0.0 pts += 1 score(pts) son(440, 50) end
      end
    end
  end
  if vies <= 0 fin = true son(200, 300) end
end

def dessine()
  fond(rgb(10, 10, 30))
  for i: 0 .. 5
    if AA[i] disque(int(AX[i]), int(AY[i]), 10, rgb(150, 120, 100)) disque(int(AX[i]) - 3, int(AY[i]) - 3, 3, rgb(190, 160, 130)) end
    if BV[i] > 0 rect(int(BX[i]) - 1, int(BY[i]) - 1, 3, 3, rgb(255, 220, 60)) end
  end
  if inv <= 0 || int(temps() * 10) % 2 == 0
    tri(160 + int(math.cos(ang) * 14), 120 + int(math.sin(ang) * 14), 160 + int(math.cos(ang + 2.5) * 10), 120 + int(math.sin(ang + 2.5) * 10), 160 + int(math.cos(ang - 2.5) * 10), 120 + int(math.sin(ang - 2.5) * 10), rgb(232, 50, 43))
  end
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(250, 6, str(pts), rgb(255, 255, 255), 2)
  if fin texte(80, 100, "PERDU !", rgb(255, 220, 60), 4) texte(90, 150, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
