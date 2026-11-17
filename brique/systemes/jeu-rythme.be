# Rythme
# exemple: JEU rythme façon guitar hero (des notes tombent dans 3 couloirs, un par bouton G, P3, D ; appuyer quand la note passe sur la ligne, combo, score, 40 secondes) | mots: reflexe, reflexes, rythme, guitar hero, guitare, musique, notes, tempo, couloir, couloirs, appuyer, bon moment, combo, rhythm, music, beat, notes, timing, reflex, lane
var pts = 0 var combo = 0 var rates = 0 var reste = 40.0 var prochain = 0.5 var msg = "" var tmsg = 0.0 var fin = false
var NY = [] var NL = [] var NA = []                 # 8 notes : hauteur, couloir (0 G, 1 P3, 2 D), active
var TOUCHE = [] var COUL = [[230, 80, 80], [90, 200, 110], [90, 140, 240]]

def reset()
  pts = 0 combo = 0 rates = 0 reste = 40.0 prochain = 0.5 msg = "" tmsg = 0.0 fin = false
  for i: 0 .. 7 NA[i] = false end
end
def debut()
  TOUCHE = [G, P3, D]
  for i: 0 .. 7 NY.push(0.0) NL.push(0) NA.push(false) end
end

def maj(dt)
  if fin
    if appui(P3) reset() end
    return
  end
  reste -= dt tmsg -= dt
  if reste <= 0 fin = true score(pts) return end
  prochain -= dt
  if prochain <= 0
    prochain = 0.45
    for i: 0 .. 7 if !NA[i] NA[i] = true NY[i] = -10.0 NL[i] = hasard(0, 2) break end end
  end
  for i: 0 .. 7
    if NA[i]
      NY[i] += 150 * dt
      if NY[i] > 236 NA[i] = false combo = 0 rates += 1 msg = "Rate" tmsg = 0.5 end   # la note est passee
    end
  end
  for l: 0 .. 2
    if appui(TOUCHE[l])
      var meilleur = -1 var ecart = 24.0              # la note la plus proche de la ligne (y = 200), a 24 pixels au plus
      for i: 0 .. 7
        if NA[i] && NL[i] == l
          var d = NY[i] - 200 if d < 0 d = -d end
          if d < ecart ecart = d meilleur = i end
        end
      end
      if meilleur >= 0
        NA[meilleur] = false combo += 1 pts += 10 + combo * 2 msg = ecart < 8 ? "Parfait !" : "Bien" tmsg = 0.5 son(500 + l * 250, 40)
      else combo = 0 msg = "Trop tot" tmsg = 0.5 son(150, 60) end
    end
  end
end

def dessine()
  var bl = rgb(255, 255, 255)
  fond(rgb(20, 15, 40))
  for l: 0 .. 2
    var k = COUL[l]
    rect(40 + l * 80, 0, 70, 240, rgb(k[0] / 6, k[1] / 6, k[2] / 6))
    rect(40 + l * 80, 192, 70, 16, rgb(k[0] / 2, k[1] / 2, k[2] / 2))
    texte(68 + l * 80, 222, l == 0 ? "G" : (l == 1 ? "P3" : "D"), rgb(k[0], k[1], k[2]), 2)
  end
  rect(40, 199, 230, 2, bl)
  for i: 0 .. 7
    if NA[i] var k = COUL[NL[i]] disque(75 + NL[i] * 80, int(NY[i]), 12, rgb(k[0], k[1], k[2])) disque(75 + NL[i] * 80, int(NY[i]), 5, bl) end
  end
  texte(4, 4, str(pts), bl, 2) texte(4, 24, "x" + str(combo), rgb(255, 220, 60), 2)
  texte(280, 4, str(int(reste)), bl, 2)
  if tmsg > 0 texte(100, 100, msg, rgb(255, 220, 60), 2) end
  if fin texte(60, 80, "FINI !", rgb(255, 220, 60), 4) texte(40, 130, str(pts) + " points", bl, 3) texte(70, 170, "P3 : rejouer", bl, 2) end
end
