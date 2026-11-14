# Reaction
# exemple: SYSTEME reaction (le signal arrive a un moment au hasard, appuyer le plus vite possible, faux depart si on appuie trop tot, temps en millisecondes) | mots: reaction, reflexe, reflexes, signal, le plus vite, rapide, faux depart, duel, cowboy, degainer, top depart, feu vert, attendre, reflex, fastest, quick, false start, quick draw, green light, wait
var etat = 0                                        # 0 = attendre le signal, 1 = signal : appuie !, 2 = resultat
var attente = 0.0 var t = 0.0 var msg = "" var meilleur = 0

def manche() etat = 0 attente = hasard(15, 40) / 10.0 t = 0.0 end   # signal entre 1,5 et 4 s : impossible a deviner
def debut() manche() end

def maj(dt)
  t += dt
  if etat == 0
    if appui(P3) msg = "Trop tot !" son(150, 300) etat = 2 t = 0.0   # faux depart
    elif t >= attente etat = 1 t = 0.0 son(1320, 80) end
  elif etat == 1
    if appui(P3)
      var ms = int(t * 1000)
      msg = str(ms) + " ms" son(988, 60) etat = 2 t = 0.0
      if meilleur == 0 || ms < meilleur meilleur = ms score(ms < 1000 ? 1000 - ms : 0) end
    end
  elif t > 1 && appui(P3) manche() end              # 1 s de pause avant de pouvoir rejouer
end

def dessine()
  var b = rgb(255, 255, 255)
  fond(etat == 1 ? rgb(40, 170, 70) : rgb(120, 30, 30))   # le signal = tout l'ecran change
  if etat == 0 texte(90, 110, "Attends...", b, 3)
  elif etat == 1 texte(80, 110, "APPUIE !", b, 4)
  else texte(70, 100, msg, b, 3) texte(60, 160, "P3 pour rejouer", b, 2) end
  if meilleur > 0 texte(4, 4, "record " + str(meilleur) + " ms", b, 1) end
end
