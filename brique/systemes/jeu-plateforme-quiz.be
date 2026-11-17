# Plateforme quiz
# exemple: JEU plateforme avec questions (une question en haut, trois objets portent les reponses, on saute pour prendre la bonne, 3 vies) | mots: quiz, question, questions, reponse, reponses, apprendre, apprends, calcul, maths, plateforme, plateformes, objets, prendre, ecole, saute, learn, answer, answers, math, school, items, platform, collect
var px = 150.0 var py = 200.0 var vy = 0.0 var sol = true var pts = 0 var vies = 3 var fin = false
var a = 0 var b = 0 var bonne = 0 var REP = [0, 0, 0] var msg = "" var tmsg = 0.0
var PLX = [30, 130, 230, 0] var PLY = [170, 130, 170, 200] var PLW = [60, 60, 60, 320]   # 3 plateformes en l'air + le sol

def nouvelle()
  a = hasard(1, 9 + pts) b = hasard(1, 9 + pts) bonne = hasard(0, 2)
  for i: 0 .. 2 REP[i] = a + b + (i - bonne) * (i == bonne ? 0 : hasard(1, 3)) end   # fausses reponses proches de la bonne
end
def reset() px = 150.0 py = 200.0 vy = 0.0 pts = 0 vies = 3 fin = false nouvelle() end
def debut() reset() end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  if tmsg > 0 tmsg -= dt end
  if bouton(G) px -= 110 * dt end
  if bouton(D) px += 110 * dt end
  if px < 0 px = 0.0 elif px > 310 px = 310.0 end
  if appui(P3) && sol vy = -330.0 sol = false son(660, 40) end
  vy += 900 * dt
  var ny = py + vy * dt
  sol = false
  if vy >= 0
    for i: 0 .. 3
      if px + 10 > PLX[i] && px < PLX[i] + PLW[i] && py <= PLY[i] && ny >= PLY[i] ny = real(PLY[i]) vy = 0.0 sol = true end
    end
  end
  py = ny
  for i: 0 .. 2                                      # objets : 20 px au-dessus du milieu de chaque plateforme
    var dx = px + 5 - (PLX[i] + 30) var dy = py - 8 - (PLY[i] - 20)
    if dx * dx + dy * dy < 250
      if i == bonne pts += 1 score(pts) msg = "Bravo !" son(988, 80) else vies -= 1 msg = "Non, " + str(a + b) son(150, 200) end
      tmsg = 1.0 nouvelle() px = 150.0 py = 200.0 vy = 0.0
      if vies <= 0 fin = true end
      return
    end
  end
end

def dessine()
  var bl = rgb(255, 255, 255)
  fond(rgb(30, 40, 80))
  rect(0, 200, 320, 40, rgb(60, 120, 80))
  for i: 0 .. 2
    rect(PLX[i], PLY[i], PLW[i], 8, rgb(90, 160, 90))
    disque(PLX[i] + 30, PLY[i] - 20, 14, rgb(240, 200, 60)) texte(PLX[i] + 22, PLY[i] - 25, str(REP[i]), rgb(40, 30, 10), 2)
  end
  rect(int(px), int(py) - 16, 10, 16, rgb(232, 50, 43))
  texte(100, 20, str(a) + " + " + str(b) + " ?", bl, 4)
  for i: 1 .. vies disque(8 + i * 14, 12, 5, rgb(240, 70, 80)) end
  texte(270, 6, str(pts), bl, 2)
  if tmsg > 0 texte(100, 70, msg, rgb(255, 220, 60), 2) end
  if fin texte(80, 100, "FINI !", rgb(255, 220, 60), 4) texte(90, 150, "P3 : rejouer", bl, 2) end
end
