# test : un message par image pendant le dessin (DMA de l'ecran actif) ; combien partent ?
var ok = 0 var ko = 0 var t = 0.0
def maj(dt)
  if envoie("12 34 0.50") ok += 1 else ko += 1 end
  t += dt
  if t > 1 print("reseau : " + str(ok) + " envoyes, " + str(ko) + " refuses") ok = 0 ko = 0 t = 0.0 end
end
def dessine()
  fond(rgb(10, 40, 20))
  for i: 0 .. 40 rect(i * 8, 100 + (i % 5) * 10, 6, 6, rgb(80, 230, 120)) end
end
