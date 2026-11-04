# Saute plateformes
# exemple: Course sans fin sur des plateformes (plateformes creees au fur et a mesure, saut, defilement) | mots: sans fin, infini, runner, courir, saut, sauter, plateforme, defile, distance
var PX = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
var PY = [0, 0, 0, 0, 0, 0]
var PW = [0, 0, 0, 0, 0, 0]
var dern = 0
var HX = 60
var hy = 0.0 var vy = 0.0 var sol = false
var vit = 90.0 var dist = 0.0
var perdu = false var attente = 0.0
var CIEL = 0 var VERT = 0 var BLANC = 0 var PERSO = 0

def place(i)
  PX[i] = PX[dern] + PW[dern] + hasard(25, 55)
  var y = PY[dern] + hasard(-40, 40)
  if y < 110 y = 110 elif y > 210 y = 210 end
  PY[i] = y
  PW[i] = hasard(50, 110)
  dern = i
end

def nouvelle()
  PX[0] = 0.0 PY[0] = 180 PW[0] = 220 dern = 0
  for i: 1 .. 5 place(i) end
  hy = 180.0 vy = 0.0 sol = true
  vit = 90.0 dist = 0.0 perdu = false
end

def debut()
  CIEL = rgb(120, 190, 240) VERT = rgb(60, 160, 70) BLANC = rgb(255, 255, 255) PERSO = rgb(232, 50, 43)
  nouvelle()
end

def maj(dt)
  if perdu
    attente -= dt
    if attente <= 0 && appui(P3) nouvelle() end
  else
    vit += 4 * dt
    dist += vit * dt
    for i: 0 .. 5
      PX[i] -= vit * dt
      if PX[i] + PW[i] < 0 place(i) end
    end
    if appui(P3) && sol vy = -330.0 sol = false son(660, 40) end
    if !bouton(P3) && vy < -120 vy = -120.0 end
    vy += 900 * dt
    var ny = hy + vy * dt
    sol = false
    if vy >= 0
      for i: 0 .. 5
        if HX + 10 > PX[i] && HX < PX[i] + PW[i] && hy <= PY[i] && ny >= PY[i]
          ny = real(PY[i]) vy = 0.0 sol = true
        end
      end
    end
    hy = ny
    if hy > 260
      perdu = true attente = 0.8
      score(int(dist / 10)) son(150, 300)
    end
  end
end

def dessine()
  fond(CIEL)
  for i: 0 .. 5
    if PX[i] < 320 rect(int(PX[i]), PY[i], PW[i], 8, VERT) end
  end
  if !perdu rect(HX, int(hy) - 16, 10, 16, PERSO) end
  texte(4, 4, str(int(dist / 10)) + " m", BLANC, 2)
  if perdu texte(70, 100, "Perdu !", BLANC, 4) texte(60, 150, "P3 pour rejouer", BLANC, 2) end
end
