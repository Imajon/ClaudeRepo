# Saut
# exemple: SYSTEME saut (gravite, saut plus haut si P3 tenu, atterrir sur des plateformes par le dessus) | mots: saut, sauter, gravite, tomber, plateforme, plateformes, rebondir, sol, mario
var PLX = [0, 120, 220] var PLY = [200, 160, 120] var PLW = [320, 70, 70]   # plateformes : x, y du dessus, largeur
var px = 40.0 var py = 200.0 var vy = 0.0 var sol = true                   # py = les pieds

def maj(dt)
  if bouton(G) px -= 100 * dt end
  if bouton(D) px += 100 * dt end
  if appui(P3) && sol vy = -330.0 sol = false son(660, 40) end   # impulsion vers le haut
  if !bouton(P3) && vy < -120 vy = -120.0 end                    # P3 lache tot = petit saut
  vy += 900 * dt                                                 # gravite
  var ny = py + vy * dt
  sol = false
  if vy >= 0                                                     # on n'atterrit qu'en descendant : on traverse par dessous
    for i: 0 .. size(PLX) - 1
      if px + 10 > PLX[i] && px < PLX[i] + PLW[i] && py <= PLY[i] && ny >= PLY[i] ny = real(PLY[i]) vy = 0.0 sol = true end
    end
  end
  py = ny
  if py > 260 px = 40.0 py = 200.0 vy = 0.0 end                  # tombe dans le vide : retour au depart
end

def dessine()
  fond(rgb(120, 190, 240))
  for i: 0 .. size(PLX) - 1 rect(PLX[i], PLY[i], PLW[i], 8, rgb(60, 160, 70)) end
  rect(int(px), int(py) - 16, 10, 16, rgb(232, 50, 43))
end
