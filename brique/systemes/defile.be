# Defile
# exemple: SYSTEME monde sans fin (obstacles recycles : celui qui sort a gauche revient a droite, vitesse qui monte, distance parcourue) | mots: sans fin, infini, runner, courir, defile, defilement, distance, obstacle, obstacles, tuyau, flappy, oiseau, voler, plus vite
var N = 4
var OX = [] var OH = []                             # x de chaque obstacle, hauteur du trou
var vit = 90.0 var dist = 0.0

def recycle(i, x) OX[i] = real(x) OH[i] = hasard(40, 140) end   # nouvelle position, nouveau trou
def debut() for i: 0 .. N - 1 OX.push(0.0) OH.push(0) recycle(i, 320 + i * 100) end end

def maj(dt)
  vit += 3 * dt dist += vit * dt
  for i: 0 .. N - 1
    OX[i] -= vit * dt
    if OX[i] < -30 recycle(i, OX[(i + N - 1) % N] + 100) end   # repart derriere le dernier
  end
end

def dessine()
  fond(rgb(120, 190, 240))
  for i: 0 .. N - 1
    if OX[i] < 320 rect(int(OX[i]), 0, 30, OH[i], rgb(60, 160, 70)) rect(int(OX[i]), OH[i] + 80, 30, 240, rgb(60, 160, 70)) end
  end
  texte(4, 4, str(int(dist / 10)) + " m", rgb(255, 255, 255), 2)
end
