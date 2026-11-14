# Rythme
# exemple: SYSTEME rythme (des notes descendent dans 3 couloirs, un par bouton, au tempo ; appuyer quand la note passe sur la ligne : parfait, bien, rate ; serie) | mots: rythme, musique, note, notes, tempo, danse, mesure, guitare, piano, batterie, battement, en rythme, chanson, rhythm, music, dance, beat, guitar, drum, song
var NB = 12
var NC = [] var NT = [] var NA = []                 # couloir (0 = G, 1 = P3, 2 = D), instant ou la note touche la ligne, active
var t = 0.0 var prochaine = 1.5 var tempo = 0.5 var pts = 0 var serie = 0 var msg = ""
var LIGNE = 200 var VIT = 160                       # une note est a y = LIGNE - (son instant - t) * VIT
var TOUCHE = [] var NOTE = [523, 659, 784]

def debut()
  TOUCHE = [G, P3, D]
  for i: 0 .. NB - 1 NC.push(0) NT.push(0.0) NA.push(false) end
end
def juger(c)                                        # la note active du couloir c la plus proche de la ligne
  var best = -1 var ecart = 0.25                    # a plus de 0,25 s : l'appui ne compte pour aucune note
  for i: 0 .. NB - 1
    var e = NT[i] - t
    if e < 0 e = -e end
    if NA[i] && NC[i] == c && e < ecart best = i ecart = e end
  end
  if best < 0 serie = 0 msg = "rate" son(120, 60) return end
  NA[best] = false serie += 1 son(NOTE[c], 80)
  if ecart < 0.08 pts += 3 msg = "parfait" else pts += 1 msg = "bien" end
  score(pts)
end

def maj(dt)
  t += dt
  if t + 1.3 >= prochaine                           # creee 1,3 s avant la ligne : le temps de descendre
    for i: 0 .. NB - 1
      if !NA[i] NA[i] = true NC[i] = hasard(0, 2) NT[i] = prochaine break end
    end
    prochaine += tempo
  end
  for c: 0 .. 2 if appui(TOUCHE[c]) juger(c) end end
  for i: 0 .. NB - 1
    if NA[i] && t - NT[i] > 0.25 NA[i] = false serie = 0 msg = "rate" end   # passee sans etre jouee
  end
end

def dessine()
  fond(rgb(20, 10, 40))
  for c: 0 .. 2 rect(70 + c * 70, 0, 40, 240, rgb(35, 25, 60)) end
  rect(60, LIGNE, 200, 3, rgb(255, 255, 255))
  for i: 0 .. NB - 1
    if NA[i] rect(72 + NC[i] * 70, LIGNE - int((NT[i] - t) * VIT) - 6, 36, 12, rgb(255, 220, 60)) end
  end
  texte(4, 4, str(pts) + "  x" + str(serie), rgb(255, 255, 255), 2)
  texte(130, 215, msg, rgb(255, 255, 255), 2)
end
