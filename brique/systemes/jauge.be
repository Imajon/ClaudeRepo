# Jauge
# exemple: SYSTEME jauge qui oscille (un curseur va et vient, P3 l'arrete, plus il est pres du centre de la zone verte plus on marque, la zone retrecit et le curseur accelere) | mots: jauge, curseur, oscille, zone, zone verte, viser, precision, force, puissance, golf, lancer, bon moment, arreter, barre, fronde
var pos = 0.0 var vit = 1.2 var dir = 1 var zone = 0.3 var arret = 0.0 var pts = 0 var dernier = 0

def maj(dt)
  if arret > 0 arret -= dt return end               # le resultat reste affiche un moment
  pos += dir * vit * dt
  if pos > 1 pos = 1.0 dir = -1 elif pos < 0 pos = 0.0 dir = 1 end   # va et vient entre 0 et 1
  if appui(P3)
    var ecart = pos - 0.5
    if ecart < 0 ecart = -ecart end
    dernier = ecart < zone / 2 ? int(100 - ecart / (zone / 2) * 100) : 0   # 100 au centre, 0 hors de la zone
    if dernier > 0 pts += dernier score(pts) son(880 + dernier * 4, 80) zone *= 0.9 vit += 0.15
    else son(150, 200) end
    arret = 0.8
  end
end

def dessine()
  fond(rgb(20, 20, 40))
  var x0 = 40 var l = 240
  rect(x0, 110, l, 20, rgb(70, 60, 90))
  rect(x0 + int((0.5 - zone / 2) * l), 110, int(zone * l), 20, rgb(60, 180, 80))
  rect(x0 + int(pos * l) - 2, 100, 4, 40, rgb(255, 255, 255))
  texte(4, 4, str(pts), rgb(255, 255, 255), 2)
  if arret > 0 texte(140, 160, str(dernier), rgb(255, 220, 60), 3) end
end
