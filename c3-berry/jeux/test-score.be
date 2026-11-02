# Test score
var t = 0.0 var pts = 0
def maj(dt) t += dt if t > 0.2 t = 0.0 pts += 1 score(pts) end end
def dessine() fond(rgb(10, 20, 40)) texte(10, 100, str(pts), rgb(255, 255, 0), 4) end
