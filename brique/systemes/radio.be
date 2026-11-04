# Radio
# exemple: SYSTEME plusieurs consoles (chaque console envoie sa position, dessine les autres, oublie celles qui se taisent, la plus petite id decide pour tout le monde) | mots: deux consoles, plusieurs consoles, multijoueur, a deux, a plusieurs, ensemble, radio, copain, ami, contre un ami, equipe, joueurs
import string
var x = 150.0 var envoi = 0.0
var AUTRES = {}                                     # id -> [x, temps du dernier message]

def chef()                                          # la plus petite id presente decide (ou tombe le fruit, qui a gagne...)
  var c = moi()
  for k: AUTRES.keys() if k < c c = k end end
  return c == moi()
end

def maj(dt)
  if bouton(G) x -= 120 * dt end
  if bouton(D) x += 120 * dt end
  envoi -= dt
  if envoi <= 0 envoi = 0.05 envoie("p," + str(int(x))) end   # 20 messages par seconde au plus
  for m: recois()
    if m[0] != moi()
      var p = string.split(m[1], ",")
      if p[0] == "p" AUTRES[m[0]] = [int(p[1]), temps()] end
    end
  end
  for k: AUTRES.keys() if temps() - AUTRES[k][1] > 2 AUTRES.remove(k) break end end   # silencieuse depuis 2 s = partie
end

def dessine()
  fond(rgb(15, 25, 20))
  for k: AUTRES.keys() rect(AUTRES[k][0], 150, 16, 16, rgb(80, 150, 255)) end
  rect(int(x), 180, 16, 16, rgb(232, 50, 43))
  texte(4, 4, chef() ? "je decide" : "j'ecoute", rgb(255, 255, 255), 1)
end
