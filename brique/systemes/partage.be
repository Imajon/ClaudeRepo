# Objet partage
# exemple: SYSTEME objet partage entre consoles (sa place vient d'un compteur commun : toutes les consoles le voient au meme endroit ; celui qui l'attrape augmente le compteur et le dit aux autres) | mots: partage, partager, meme objet, tous les joueurs, deux consoles, plusieurs consoles, a deux, course au tresor, premier qui attrape, fruit, piece, ensemble, shared, share, same object, all players, two consoles, several consoles, two players, treasure hunt, first to catch, together
import string
var k = 0 var ox = 0 var oy = 0 var x = 150.0 var y = 120.0 var pts = 0

def placer() ox = (k * 97 + 31) % 280 + 20 oy = (k * 53 + 17) % 180 + 30 end   # meme formule partout : pas besoin d'envoyer la place

def debut() placer() end

def maj(dt)
  if bouton(G) x -= 100 * dt end
  if bouton(D) x += 100 * dt end
  y += (bouton(P3) ? -100 : 100) * dt
  if y < 10 y = 10.0 elif y > 230 y = 230.0 end
  var dx = x - ox var dy = y - oy
  if dx * dx + dy * dy < 144
    k += 1 pts += 1 score(pts) son(988, 60) placer()
    envoie("k," + str(k))                           # les autres passent au meme objet suivant
  end
  for m: recois()
    if m[0] != moi()
      var p = string.split(m[1], ",")
      if p[0] == "k" && int(p[1]) > k k = int(p[1]) placer() end   # on garde toujours le plus grand compteur
    end
  end
end

def dessine()
  fond(rgb(15, 25, 20))
  disque(ox, oy, 6, rgb(255, 220, 60))
  disque(int(x), int(y), 6, rgb(232, 50, 43))
  texte(4, 4, str(pts), rgb(255, 255, 255), 2)
end
