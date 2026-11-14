# Etats
# exemple: SYSTEME etats de partie (vies, perdu, P3 pour rejouer, chrono, gagne, niveau suivant plus dur) | mots: perdu, vies, vie, rejouer, game over, recommencer, chrono, temps, gagne, gagner, niveau suivant, plus dur, record, manche, lost, lose, lives, life, play again, restart, timer, win, next level, harder, round
import string
var etat = 0                                        # 0 = on joue, 1 = perdu, 2 = gagne
var vies = 3 var niveau = 1 var chrono = 0.0 var attente = 0.0 var pts = 0

def manche() chrono = 0.0 etat = 0 end             # remet le niveau en place (plus dur selon niveau)
def partie() vies = 3 niveau = 1 pts = 0 manche() end
def perdre()
  vies -= 1 son(150, 300)
  if vies <= 0 etat = 1 attente = 0.8 score(pts) else manche() end
end
def debut() partie() end

def maj(dt)
  if etat != 0
    attente -= dt
    if attente <= 0 && appui(P3)                    # attente : l'appui qui a fait perdre ne relance pas aussitot
      if etat == 1 partie() else niveau += 1 manche() end
    end
    return
  end
  chrono += dt
  if appui(P3) pts += niveau son(880, 30) end       # (ici le vrai jeu)
  if chrono > 10 perdre() end
  if pts >= 10 * niveau etat = 2 attente = 0.5 son(1320, 200) end
end

def dessine()
  fond(rgb(20, 20, 40))
  var b = rgb(255, 255, 255)
  texte(4, 4, string.format("Vies %d  Niv %d  %.1f s  %d", vies, niveau, chrono, pts), b, 1)
  if etat == 1 texte(70, 100, "Perdu !", b, 4) texte(60, 150, "P3 pour rejouer", b, 2) end
  if etat == 2 texte(60, 100, "Gagne !", b, 4) texte(40, 150, "P3 : niveau suivant", b, 2) end
end
