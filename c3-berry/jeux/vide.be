# mesure : un fond et un carre qui bouge avec G / D
var x = 150
def maj(dt)
  if bouton(G) x -= 200 * dt end
  if bouton(D) x += 200 * dt end
end
def dessine()
  fond(rgb(20, 30, 60))
  rect(int(x), 110, 20, 20, rgb(232, 50, 43))
end
