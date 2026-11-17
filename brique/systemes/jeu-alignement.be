# Alignement
# exemple: JEU plateformes a aligner (une plateforme glisse de gauche a droite, P3 l'arrete au bon moment pour qu'elle tombe pile sur la precedente, ce qui depasse est coupe, la tour monte, de plus en plus vite) | mots: reflexe, reflexes, aligner, alignement, aligne, empiler, empile, tour, pile, plateforme, plateformes, bon moment, timing, precision, stack, align, tower, stacking, reflex, precision
var W = 100.0 var X = 110.0 var etage = 0 var bx = 0.0 var bw = 100.0 var sens = 1 var vit = 90.0 var fin = 0 var msg = "" var tmsg = 0.0
var PX = [] var PW = []                             # les etages deja poses : x et largeur

def reset()
  etage = 1 bx = 0.0 bw = 100.0 sens = 1 vit = 90.0 fin = 0 msg = "" tmsg = 0.0
  PX[0] = 110.0 PW[0] = 100.0                       # la base
end
def debut() for i: 0 .. 12 PX.push(0.0) PW.push(0.0) end reset() end

def maj(dt)
  if fin > 0
    if appui(P3) reset() end
    return
  end
  tmsg -= dt
  bx += sens * vit * dt
  if bx < 0 bx = 0.0 sens = 1 elif bx + bw > 320 bx = 320.0 - bw sens = -1 end
  if appui(P3)
    var px = PX[etage - 1] var pw = PW[etage - 1]
    var g = bx > px ? bx : px var d = bx + bw < px + pw ? bx + bw : px + pw   # le bout commun
    if d - g <= 2 fin = 1 son(150, 400) return end                           # plus rien en commun : perdu
    if bx - px < 5 && px - bx < 5 g = px d = px + pw msg = "Parfait !" son(988, 80) else msg = "Bien" son(600, 40) end   # presque pile : on garde tout
    tmsg = 0.8 PX[etage] = g PW[etage] = d - g bw = d - g
    etage += 1 vit += 12 score(etage)
    if etage > 12 fin = 2 son(1200, 400) end
  end
end

def dessine()
  var bl = rgb(255, 255, 255)
  fond(rgb(30, 50, 90))
  for i: 0 .. etage - 1 rect(int(PX[i]), 224 - i * 16, int(PW[i]), 14, rgb(80 + i * 12, 200 - i * 8, 120)) end
  if fin == 0 rect(int(bx), 224 - etage * 16, int(bw), 14, rgb(255, 220, 60)) end
  texte(4, 4, "Etage " + str(etage - 1) + " / 12", bl, 2)
  if tmsg > 0 texte(100, 30, msg, rgb(255, 220, 60), 2) end
  if fin == 1 texte(70, 70, "TROP LOIN !", rgb(255, 90, 70), 3) end
  if fin == 2 texte(70, 70, "TOUR FINIE !", rgb(255, 220, 60), 3) end
  if fin > 0 texte(70, 110, "P3 : rejouer", bl, 2) end
end
