# Tir a la corde
# exemple: SYSTEME duel a deux consoles en martelant (chaque console compte ses appuis et les envoie, la corde penche vers celui qui appuie le plus, manche numerotee pour que les deux consoles restent d'accord) | mots: tir a la corde, marteler, appuyer vite, le plus vite, deux consoles, a deux, duel, contre un ami, bras de fer, course a deux, clic, tug of war, mash, press fast, fastest, two consoles, two players, against a friend, arm wrestling
import string
var mes = 0 var autre = 0 var manche = 1 var envoi = 0.0 var fin = 0.0 var gagne = false var vu = -10.0

def nouvelle(m) manche = m mes = 0 autre = 0 fin = 0.0 end

def maj(dt)
  for m: recois()                                   # "c,<manche>,<appuis>"
    if m[0] != moi()
      var p = string.split(m[1], ",")
      var mm = int(p[1])
      if mm > manche nouvelle(mm) end               # l'autre a deja commence la manche suivante : on le suit
      if mm == manche autre = int(p[2]) vu = temps() end
    end
  end
  if fin > 0
    fin -= dt
    if fin <= 0 nouvelle(manche + 1) end
    return
  end
  if appui(P3) mes += 1 son(600 + mes * 5, 15) end
  envoi -= dt
  if envoi <= 0 envoi = 0.1 envoie("c," + str(manche) + "," + str(mes)) end   # 10 fois par seconde
  var d = mes - autre                               # les deux consoles calculent la meme chose a l'envers
  if d >= 30 || d <= -30 gagne = d > 0 fin = 2.0 son(gagne ? 1320 : 110, 300) if gagne score(manche) end end
end

def dessine()
  var b = rgb(255, 255, 255)
  fond(rgb(30, 40, 30))
  var d = mes - autre
  if d > 30 d = 30 elif d < -30 d = -30 end
  var x = 160 - d * 4                               # le noeud part vers moi (a gauche) quand j'appuie plus
  ligne(20, 120, 300, 120, rgb(200, 160, 100))
  rect(x - 4, 110, 8, 20, rgb(232, 50, 43))
  rect(38, 100, 2, 40, b) rect(280, 100, 2, 40, b)
  texte(4, 4, str(mes) + " / " + str(autre), b, 2)
  if temps() - vu > 2 texte(60, 200, "en attente d'un copain", b, 1) end
  if fin > 0 texte(90, 60, gagne ? "GAGNE !" : "PERDU", b, 3) end
end
