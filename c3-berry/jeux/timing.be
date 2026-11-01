# Timing : une cible s'affiche ; P3 lance le chrono, qui se cache apres 1 s ; P3 quand on pense avoir atteint la cible.
# A plusieurs : chacun envoie son ecart, le plus petit gagne la manche.
import math
import string
var etat = 0                     # 0 attente, 1 chrono, 2 resultat
var cible = 5.0
var t0 = 0.0
var fait = 0.0
var meilleur = 99.0
var autres = {}                  # ecarts des autres consoles (id -> ecart)
def nouvelle() cible = hasard(6, 20) * 0.5 autres = {} end
def debut() nouvelle() end
def maj(dt)
  for m: recois() autres[m[0]] = real(m[1]) end
  if appui(P3)
    if etat == 0
      t0 = temps() etat = 1 son(880, 60)
    elif etat == 1
      fait = temps() - t0 etat = 2
      var e = math.abs(fait - cible)
      if e < meilleur meilleur = e end
      son(e <= 0.05 ? 1568 : 440, 200)
      envoie(string.format("%.3f", e))
    else
      nouvelle() etat = 0
    end
  end
end
def dessine()
  fond(rgb(28, 22, 36))
  texte(20, 16, "CIBLE", rgb(255, 200, 40), 2)
  texte(20, 40, string.format("%.2f s", cible), rgb(255, 200, 40), 5)
  if etat == 0
    texte(20, 130, "P3 = lancer le chrono", rgb(200, 200, 220), 2)
  elif etat == 1
    var t = temps() - t0
    texte(20, 110, t < 1 ? string.format("%.2f", t) : "?.??", rgb(60, 170, 255), 6)
    texte(20, 200, "P3 quand tu penses y etre", rgb(200, 200, 220), 1)
  else
    var e = fait - cible
    var c = math.abs(e) <= 0.05 ? rgb(80, 255, 120) : math.abs(e) <= 0.25 ? rgb(80, 220, 80) : math.abs(e) <= 1 ? rgb(255, 160, 40) : rgb(255, 60, 60)
    texte(20, 100, string.format("%.2f s", fait), c, 5)
    texte(20, 150, math.abs(e) <= 0.05 ? "PARFAIT !" : string.format("ecart %+.2f s", e), c, 2)
    texte(20, 180, string.format("meilleur ecart %.2f s", meilleur), rgb(200, 200, 220), 1)
    var y = 196
    for id: autres.keys()
      texte(20, y, string.format("console %d : %.2f s", id, autres[id]), rgb(150, 150, 200), 1) y += 10
    end
  end
end
