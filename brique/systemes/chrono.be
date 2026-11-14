# Chrono cache
# exemple: SYSTEME chrono cache (le temps s'affiche 1 s puis disparait, il faut appuyer pile a la seconde cible, ecart en centiemes) | mots: chrono cache, chrono, bonne seconde, pile, compter dans sa tete, cible, secondes, arreter le temps, deviner le temps, hidden timer, timer, stopwatch, right second, count in your head, target, stop the clock, guess the time
import string
var CIBLE = 5.0 var t = 0.0 var marche = false var fini = false var ecart = 0.0

def maj(dt)
  if marche t += dt end
  if !appui(P3) return end
  if marche                                         # 2e appui : on arrete et on juge
    marche = false fini = true ecart = t - CIBLE son(880, 100)
    var e = ecart < 0 ? -ecart : ecart
    score(e < 1 ? int(1000 - e * 1000) : 0)
  elif fini fini = false
  else marche = true t = 0.0 end
end

def dessine()
  var b = rgb(255, 255, 255)
  fond(rgb(20, 20, 40))
  texte(40, 30, string.format("Arrete a %d s", int(CIBLE)), b, 2)
  if marche && t < 1 texte(100, 100, string.format("%.2f", t), b, 4)   # visible 1 s seulement
  elif marche texte(150, 100, "?", b, 4)
  elif fini texte(70, 100, string.format("%.2f s", t), b, 4) texte(70, 160, string.format("ecart %.2f", ecart), b, 2)
  else texte(60, 100, "P3 pour partir", b, 2) end
end
