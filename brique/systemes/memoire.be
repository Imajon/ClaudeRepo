# Memoire
# exemple: SYSTEME memoire de sequence (la console montre une suite de couleurs, une par bouton, puis on la repete ; elle s'allonge d'une couleur a chaque reussite) | mots: memoire, se souvenir, retenir, sequence, suite, repeter, couleurs, simon, dans l'ordre, de plus en plus longue, paires, cartes
var SEQ = [] var etat = 0 var i = 0 var t = 0.0     # etat 0 = la console montre, 1 = a toi, 2 = rate
var allume = -1 var joue = -1
var TOUCHE = [] var COUL = [] var NOTE = [330, 440, 554]

def ajoute() SEQ.push(hasard(0, 2)) etat = 0 i = 0 joue = -1 t = -0.6 end   # petite pause avant de montrer
def debut()
  TOUCHE = [G, P3, D] COUL = [rgb(232, 50, 43), rgb(60, 200, 80), rgb(60, 120, 255)]
  ajoute()
end

def maj(dt)
  t += dt
  if etat == 0                                      # 0,5 s par couleur : 0,35 allumee, 0,15 eteinte
    allume = -1
    if t >= 0
      if i >= size(SEQ) etat = 1 i = 0 return end
      if t < 0.35 allume = SEQ[i] if joue != i joue = i son(NOTE[SEQ[i]], 300) end   # le son une seule fois par couleur
      elif t >= 0.5 i += 1 t = 0.0 end
    end
  elif etat == 1
    allume = -1
    for c: 0 .. 2
      if bouton(TOUCHE[c]) allume = c end
      if appui(TOUCHE[c])
        son(NOTE[c], 150)
        if c != SEQ[i] etat = 2 t = 0.0 son(110, 500) score(size(SEQ) - 1) return end
        i += 1
        if i >= size(SEQ) ajoute() return end
      end
    end
  elif t > 1 && appui(P3) SEQ = [] ajoute() end
end

def dessine()
  fond(rgb(20, 20, 30))
  for c: 0 .. 2
    var x = 30 + c * 95
    if allume == c disque(x + 35, 120, 40, COUL[c]) else cercle(x + 35, 120, 40, COUL[c]) end
  end
  texte(4, 4, str(size(SEQ)), rgb(255, 255, 255), 2)
  if etat == 1 texte(120, 200, "A toi !", rgb(255, 255, 255), 2) end
  if etat == 2 texte(80, 200, "Rate ! P3", rgb(255, 255, 255), 2) end
end
