# Question
# exemple: SYSTEME question a 3 reponses (une reponse par bouton G, P3, D, la bonne placee au hasard, temps limite, la difficulte monte quand on reussit et baisse quand on rate) | mots: quiz, question, questions, reponse, reponses, calcul, calcul rapide, maths, vrai ou faux, devinette, ecole, tables, addition, multiplication, answer, sum, true or false, riddle, school, times table
import string
var a = 0 var b = 0 var REP = [0, 0, 0] var bonne = 0 var niveau = 1 var t = 0.0 var pts = 0 var msg = "" var pause = 0.0
var TOUCHE = []

def nouvelle()
  var m = 5 + niveau * 5                            # nombres plus grands quand le niveau monte
  a = hasard(1, m) b = hasard(1, m)
  bonne = hasard(0, 2)
  for i: 0 .. 2 REP[i] = a + b + (i - bonne) * hasard(1, 3) end   # fausses reponses proches de la bonne
  t = 8.0
end
def debut() TOUCHE = [G, P3, D] nouvelle() end
def repondre(i)
  if i == bonne pts += niveau niveau += 1 msg = "Bravo" son(988, 80) score(pts)
  else niveau = niveau > 1 ? niveau - 1 : 1 msg = "C'etait " + str(a + b) son(150, 200) end
  pause = 1.0
end

def maj(dt)
  if pause > 0
    pause -= dt
    if pause <= 0 msg = "" nouvelle() end
    return
  end
  t -= dt
  if t <= 0 repondre(-1) return end                 # temps ecoule = rate
  for i: 0 .. 2 if appui(TOUCHE[i]) repondre(i) return end end
end

def dessine()
  var bl = rgb(255, 255, 255)
  fond(rgb(25, 30, 60))
  texte(100, 50, str(a) + " + " + str(b), bl, 4)
  for i: 0 .. 2
    rect(15 + i * 102, 150, 90, 50, rgb(60, 70, 130))
    texte(35 + i * 102, 165, str(REP[i]), bl, 3)
  end
  rect(0, 230, int(t / 8 * 320), 10, rgb(255, 220, 60))   # temps qui reste
  texte(4, 4, "Niv " + str(niveau) + "  " + str(pts), bl, 1)
  if msg != "" texte(90, 110, msg, rgb(255, 220, 60), 2) end
end
