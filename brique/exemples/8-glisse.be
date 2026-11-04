# Glisse
# exemple: Glisse (sol fabrique au fur et a mesure dans un anneau, pente, saut, figures en l'air, atterrissage) | mots: glisse, ski, snow, skate, surf, pente, descente, neige, figure, saut, rampe, colline
import math
import string
var NB = 64 var PAS = 16
var HT = []
var fait = 0 var pente = 0.3
var px = 40.0 var py = 0.0 var vx = 120.0 var vy = 0.0
var rot = 0.0 var air = false var tourne = 0.0
var pts = 0 var chute = 0.0 var camy = 0.0
var CIEL = 0 var NEIGE = 0 var BORD = 0 var SOLEIL = 0 var PLANCHE = 0 var CORPS = 0 var BLANC = 0

def genere(jusque)                                  # le sol est fabrique au fur et a mesure, dans un anneau de NB points
  while fait <= jusque
    if fait == 0 HT[0] = 120.0
    else
      pente += (0.35 - pente) * 0.1 + hasard(-12, 12) / 100.0   # revient toujours vers une descente
      if pente < -0.15 pente = -0.15 elif pente > 0.8 pente = 0.8 end
      if hasard(0, 25) == 0 pente = -0.5 end        # une bosse = un tremplin
      HT[fait % NB] = HT[(fait - 1) % NB] + pente * PAS
    end
    fait += 1
  end
end
def sol(x)                                          # hauteur du sol sous x
  var i = int(x / PAS) var f = x / PAS - i
  var a = HT[i % NB] var b = HT[(i + 1) % NB]
  return a + (b - a) * f
end

def debut()
  CIEL = rgb(110, 170, 230) NEIGE = rgb(235, 242, 255) BORD = rgb(150, 180, 220) SOLEIL = rgb(255, 230, 120)
  PLANCHE = rgb(232, 50, 43) CORPS = rgb(40, 40, 60) BLANC = rgb(255, 255, 255)
  for i: 0 .. NB - 1 HT.push(0.0) end
  genere(30)
  py = sol(px) camy = py - 130
end

def maj(dt)
  genere(int(px / PAS) + 22)
  var ang = math.atan2(sol(px + 4) - sol(px), 4)
  if chute > 0 chute -= dt end
  if !air
    vx += (600 * math.sin(ang) - 30) * dt
    if vx < 80 vx = 80.0 elif vx > 420 vx = 420.0 end
    rot = ang
    if appui(P3) && chute <= 0 vy -= 260 air = true son(500, 40) end
  end
  px += vx * dt
  var gn = sol(px)
  if air
    vy += 700 * dt
    py += vy * dt
    if bouton(P3) && chute <= 0 rot -= 7 * dt tourne += 7 * dt end
    if py >= gn                                     # atterrissage : planche a peu pres dans la pente ?
      py = gn air = false
      var d = rot - math.atan2(sol(px + 4) - gn, 4)
      while d > math.pi d -= 2 * math.pi end
      while d < -math.pi d += 2 * math.pi end
      if math.abs(d) < 0.7
        var n = int((tourne + 1) / 6.283)
        if n > 0 pts += 100 * n son(1320, 120) end
      else
        chute = 1.0 vx = 80.0 son(100, 300)
      end
      tourne = 0.0
      score(pts + int(px / 10))
    end
  else
    var balle = py + vy * dt + 700 * dt * dt          # ou serait-il s'il volait ?
    if gn > balle + 0.5 air = true vy += 700 * dt py = balle   # le sol tombe plus vite que lui : il decolle
    else vy = (gn - py) / dt py = gn
    end
  end
  camy += (py - 130 - camy) * 4 * dt
end

def dessine()
  fond(CIEL)
  disque(260, 40, 18, SOLEIL)
  var cy = int(camy)
  camera(int(px) - 80, cy)
  var bas = cy + 240
  var i0 = int((px - 80) / PAS)
  for i: i0 .. i0 + 21
    var x0 = i * PAS var x1 = x0 + PAS
    var y0 = int(HT[i % NB]) var y1 = int(HT[(i + 1) % NB])
    tri(x0, y0, x1, y1, x1, bas, NEIGE) tri(x0, y0, x1, bas, x0, bas, NEIGE)
    ligne(x0, y0, x1, y1, BORD)
  end
  var c = math.cos(rot) var s = math.sin(rot)
  var x = int(px) var y = int(py)
  ligne(x - int(c * 9), y - int(s * 9), x + int(c * 9), y + int(s * 9), PLANCHE)
  disque(x + int(s * 8), y - int(c * 8), 4, chute > 0 ? PLANCHE : CORPS)
  disque(x + int(s * 15), y - int(c * 15), 3, CORPS)
  camera(0, 0)
  texte(4, 4, string.format("%d m", int(px / 10)), BLANC, 2)
  if pts > 0 texte(4, 24, "Figures " + str(pts), BLANC, 1) end
end
