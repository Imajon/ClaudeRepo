# Saut charge
# exemple: SYSTEME saut charge et double saut (au sol, tenir P3 remplit une jauge et le saut part au relache, plus haut si la jauge est pleine ; en l'air, un appui donne un deuxieme saut) | mots: saut charge, charger, jauge de saut, double saut, deuxieme saut, sauter deux fois, ressort, grenouille, kangourou, bondir, plus haut, charged jump, charge, jump gauge, double jump, second jump, jump twice, spring, frog, kangaroo, leap, higher
var SOL = 200
var px = 60.0 var py = 200.0 var vy = 0.0 var sol = true var charge = 0.0 var sauts = 0

def maj(dt)
  if bouton(G) px -= 100 * dt end
  if bouton(D) px += 100 * dt end
  if sol
    if bouton(P3) charge += dt if charge > 0.6 charge = 0.6 end end   # au plus 0,6 s de charge
    if lache(P3) vy = -(200 + charge / 0.6 * 300) sol = false sauts = 1 charge = 0.0 son(500, 60) end
  elif appui(P3) && sauts == 1
    vy = -260.0 sauts = 2 son(800, 40)                # le double saut repart de zero, meme en tombant
  end
  if !sol
    vy += 900 * dt py += vy * dt
    if py >= SOL py = real(SOL) vy = 0.0 sol = true sauts = 0 end
  end
end

def dessine()
  fond(rgb(120, 190, 240))
  rect(0, SOL, 320, 40, rgb(60, 160, 70))
  var h = 16 - int(charge * 10)                     # il s'ecrase en chargeant
  rect(int(px), int(py) - h, 12, h, sauts == 2 ? rgb(255, 150, 60) : rgb(232, 50, 43))
  if charge > 0 rect(int(px) - 4, int(py) - 30, int(charge / 0.6 * 20), 4, rgb(255, 220, 60)) end
end
