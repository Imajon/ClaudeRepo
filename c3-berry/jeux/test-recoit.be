# test a deux : chaque console envoie 25 messages/s et compte ceux qu'elle recoit des autres
var n = 0 var recus = 0 var t = 0.0 var e = 0.0 var de = 0
def maj(dt)
  t += dt e += dt
  if e > 0.04 e = 0.0 n += 1 envoie(str(n)) end
  for m: recois() recus += 1 de = m[0] end
  if t > 1 print("recu " + str(recus) + "/s de " + str(de) + ", moi " + str(moi())) recus = 0 t = 0.0 end
end
def dessine()
  fond(rgb(10, 20, 40))
  texte(10, 10, "test reseau", rgb(255, 255, 255), 2)
  for i: 0 .. 40 rect(i * 8, 120, 6, 6, rgb(80, 180, 255)) end
end
