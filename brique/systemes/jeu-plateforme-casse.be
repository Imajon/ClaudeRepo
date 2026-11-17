# Plateforme casse
# exemple: JEU plateforme ou on casse des caisses en sautant dessous pour decouvrir des objets caches (G et D courent, P3 saute, la tete casse la caisse, l'objet apparait, tout decouvrir pour gagner) | mots: casse, casser, caisse, caisses, bloc, blocs, decouvrir, decouvre, cache, caches, cachee, tresor, tresors, surprise, mario, plateforme, plateformes, saut, smash, break, crate, crates, block, blocks, discover, hidden, treasure, platform
var px = 20.0 var py = 200.0 var vy = 0.0 var sol = true var pts = 0 var fin = false
var CX = [90, 150, 210, 40, 250] var CY = [150, 120, 150, 110, 110]   # 5 caisses : coin haut gauche
var CE = [0, 0, 0, 0, 0]                            # 0 caisse, 1 cassee (objet visible), 2 objet pris

def reset()
  px = 20.0 py = 200.0 vy = 0.0 sol = true pts = 0 fin = false
  for i: 0 .. 4 CE[i] = 0 end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if bouton(G) px -= 110 * dt end
  if bouton(D) px += 110 * dt end
  if px < 0 px = 0.0 elif px > 310 px = 310.0 end
  if appui(P3) && sol vy = -330.0 sol = false son(660, 40) end
  vy += 900 * dt
  var ny = py + vy * dt
  sol = false
  if ny >= 200 ny = 200.0 vy = 0.0 sol = true end   # le sol
  for i: 0 .. 4
    if CE[i] == 0 && px + 10 > CX[i] && px < CX[i] + 24
      if vy < 0 && py - 16 >= CY[i] + 16 && ny - 16 < CY[i] + 16   # la tete touche le dessous : elle casse
        CE[i] = 1 vy = 60.0 ny = real(CY[i] + 32) son(300, 60)
      elif vy >= 0 && py <= CY[i] && ny >= CY[i] ny = real(CY[i]) vy = 0.0 sol = true end   # on peut marcher dessus
    elif CE[i] == 1 && px + 10 > CX[i] + 4 && px < CX[i] + 20 && ny > CY[i] && ny - 16 < CY[i] + 16
      CE[i] = 2 pts += 1 score(pts) son(988, 80)   # on prend l'objet decouvert
    end
  end
  py = ny
  var tout = true
  for i: 0 .. 4 if CE[i] != 2 tout = false end end
  if tout fin = true son(1200, 300) end
end

def dessine()
  fond(rgb(120, 190, 240))
  rect(0, 200, 320, 40, rgb(90, 150, 70)) rect(0, 200, 320, 4, rgb(60, 120, 50))
  for i: 0 .. 4
    if CE[i] == 0 rect(CX[i], CY[i], 24, 16, rgb(190, 130, 60)) cadre(CX[i], CY[i], 24, 16, rgb(110, 70, 30)) texte(CX[i] + 8, CY[i] + 4, "?", rgb(255, 230, 120), 1)
    elif CE[i] == 1 var t = int(temps() * 4) % 2 disque(CX[i] + 12, CY[i] + 8 - t, 6, rgb(255, 215, 60)) end
  end
  var x = int(px) var y = int(py)
  rect(x, y - 16, 10, 16, rgb(232, 50, 43)) rect(x + 2, y - 13, 2, 3, rgb(255, 255, 255)) rect(x + 6, y - 13, 2, 3, rgb(255, 255, 255))
  texte(6, 6, str(pts) + " / 5", rgb(255, 255, 255), 2)
  if fin texte(60, 90, "TOUT TROUVE !", rgb(255, 220, 60), 3) texte(90, 130, "P3 : rejouer", rgb(255, 255, 255), 2) end
end
