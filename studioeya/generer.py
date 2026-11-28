"""Génère le site Studio Eya : index.html, animations/*.html, style.css, main.js.
Usage : python generer.py   (depuis le dossier studioeya)
Toutes les animations passent par le même gabarit : modifier la liste ANIMS ci-dessous.
Les sections communes de l'accueil sont reprises de versions/index-avant-pages.html.
"""
import os, re, html

ICI = os.path.dirname(os.path.abspath(__file__))
vieux = open(os.path.join(ICI, "versions", "index-avant-pages.html"), encoding="utf-8").read()


def entre(a, b):
    i = vieux.index(a)
    j = vieux.index(b, i)
    return vieux[i:j]


e = html.escape

# ---------------------------------------------------------------- données
# slug, titre, groupe, accroche, desc, étapes, puces, usage, image, alt, légende, galerie, extra
ANIMS = [
    dict(slug="modules-pixel", titre="Modules Pixel", groupe="Animation phare",
         accroche="Au repos, les matrices habillent l’espace. Dès qu’on s’approche, elles se jouent.",
         desc="Des tableaux lumineux de 64 × 64 points, conçus, codés et assemblés dans nos ateliers. Quand personne ne joue, ils affichent vos visuels, votre logo, vos messages. Un bouton, et la borne devient un jeu, en solo ou en duel.",
         etapes=["On pose les bornes, sur table ou sur pied. Sur batterie, rien à brancher.",
                 "Elles s’animent seules : vos visuels, votre logo, vos messages.",
                 "On appuie sur un bouton : la borne devient un jeu, en solo ou en duel."],
         puces=[("2 + 1", "joueurs : duo sur pied, solo sur table"), ("8 h", "d’autonomie sur batterie"), ("64 × 64", "points")],
         usage="Cocktail, dîner, soirée, salon : le même objet habille la salle et fait jouer vos invités.",
         img="pixel-module.jpg", alt="Un Module Pixel sur pied, écran en pixels allumé", pos="center 55%",
         gal=[("pixel-trois.jpg", "Trois Modules Pixel alignés"), ("pixel-cartes.jpg", "Un Module Pixel aux couleurs des cartes"), ("pixel-gros-plan.jpg", "Gros plan sur l’écran et les boutons")],
         extra="pixel"),
    dict(slug="sign", titre="Sign", groupe="Animation phare",
         accroche="En deux minutes, vos invités apprennent à épeler un mot en langue des signes française, guidés par une intelligence artificielle.",
         desc="Une animation qui sensibilise au handicap auditif : un vrai sujet pour vos actions RSE, vos journées inclusion et vos stands. Les mots sont à votre vocabulaire : vos valeurs, vos produits, votre ville.",
         etapes=["Le signe de la lettre à reproduire s’affiche sur la borne.",
                 "Le joueur signe : l’IA reconnaît son geste en temps réel.",
                 "Chaque mot réussi fait gagner des points. Photo souvenir envoyée par mail."],
         puces=[("25 à 30", "joueurs par heure, 2 min chacun"), ("Vos mots", "valeurs, produits, ville"), ("Animateur", "compris, prise 220 V")],
         usage="Journée inclusion, action RSE, stand, cocktail d’accueil : une animation qui porte un message.",
         img="sign.jpg", alt="Un joueur signe devant la borne Sign", pos="center 30%"),
    dict(slug="brique", titre="Brique", groupe="Atelier · Nouveau",
         accroche="La console qu’on assemble, les jeux qu’on invente.",
         desc="L’atelier « Ma Console » : des jeunes soudent leur console, puis créent leur jeu en le décrivant à une IA. Chacun repart avec sa console, et ses jeux dedans.",
         etapes=["Fabriquer : fer à souder en main, on assemble sa console, un écran couleur, trois boutons, un cerveau ESP32-C3.",
                 "Comprendre : des commandes, des règles « quand… alors… », un but, un univers. Avec trois boutons, la contrainte fait inventer.",
                 "Créer : on décrit son jeu, l’agent IA l’écrit, le teste et l’envoie sur la console par le câble USB."],
         puces=[("2 h", "par session"), ("10", "participants"), ("CM1 → adultes", "trois niveaux")],
         usage="Fête publique, famille, atelier pour les jeunes : on repart avec sa console.",
         img="brique-console-main.jpg", alt="La console Brique allumée, tenue en main", pos="center 55%",
         gal=[("brique-soudure.jpg", "Un jeune soude sa console"), ("brique-console-bleue.jpg", "La carte hexagonale de la console")],
         extra="brique"),
    dict(slug="bornes-arcade", titre="Bornes d’arcade", groupe="Arcade et rétro",
         accroche="Sur pied ou sur table, les classiques qu’on connaît tous.",
         desc="Joystick, boutons, jeux rétro : tout le monde sait jouer. Plusieurs bornes côte à côte pour les tournois. Elles voyagent avec les Modules Pixel : pas de livraison en plus.",
         etapes=["On installe les bornes, sur pied ou sur table, côte à côte.",
                 "Joystick, boutons, jeux rétro : on prend les commandes, tout le monde sait jouer.",
                 "En libre-service toute la soirée, avec des tournois si vous le souhaitez."],
         puces=[("1 à 2", "joueurs par machine"), ("Libre-service", "pas d’animateur à prévoir"), ("220 V", "prise nécessaire")],
         usage="Soirée, afterwork, fête publique : des parties courtes, beaucoup de passage.",
         img="arcade.jpg", alt="Un joueur devant une borne d’arcade", pos="center 40%",
         gal=[("arcade-noire.jpg", "Une borne d’arcade noire"), ("arcade-2.jpg", "Une borne d’arcade sur table")]),
    dict(slug="flipper-numerique", titre="Flipper numérique", groupe="Arcade et rétro",
         accroche="Le flipper sans limites : des dizaines de tables, à un ou deux joueurs.",
         desc="Un flipper numérique : une seule borne, des dizaines de tables au choix. Il voyage avec les Modules Pixel : pas de livraison en plus.",
         etapes=["On installe le flipper : une borne, des dizaines de tables.",
                 "Seul ou à deux, on lance la bille.",
                 "Parties courtes, beaucoup de passage, tournois possibles."],
         puces=[("1 à 2", "joueurs"), ("Des dizaines", "de tables au choix"), ("220 V", "prise nécessaire")],
         usage="Soirée et afterwork : un classique en libre-service, sans animateur.",
         img="flipper.jpg", alt="Le flipper numérique jaune", pos="center 40%"),
    dict(slug="game-watch-geant", titre="Game & Watch géant", groupe="Arcade et rétro",
         accroche="Une console de poche rétro, en version géante sur pied. Elle attire l’œil de loin.",
         desc="La console de poche d’autrefois, en version géante sur pied. Visible de loin, elle fait venir les curieux. Elle voyage avec les Modules Pixel : pas de livraison en plus.",
         etapes=["On dresse la console géante sur pied : elle attire l’œil de loin.",
                 "Comme sur la console de poche rétro, on joue seul ou à deux.",
                 "En libre-service, parties courtes, sans animateur."],
         puces=[("Géant", "sur pied, visible de loin"), ("1 à 2", "joueurs"), ("220 V", "prise nécessaire")],
         usage="Soirée, afterwork : l’objet rétro qu’on remarque dès l’entrée.",
         img="game-watch.jpg", alt="La console Game & Watch géante sur pied", pos="center 50%"),
    dict(slug="simulateur-cockpit", titre="Simulateur cockpit", groupe="Arcade et rétro",
         accroche="Une borne d’arcade dans un cockpit fermé : on s’installe, on pilote.",
         desc="Une borne d’arcade installée dans un cockpit fermé : on s’assoit, on ferme, on pilote. Le simulateur voyage avec les Modules Pixel : pas de livraison en plus.",
         etapes=["On s’installe dans le cockpit fermé.",
                 "On pilote : c’est une borne d’arcade dans un cockpit.",
                 "En libre-service, parties courtes, un joueur après l’autre."],
         puces=[("Cockpit fermé", "on s’installe, on pilote"), ("1 à 2", "joueurs"), ("220 V", "prise nécessaire")],
         usage="Soirée et afterwork : l’animation qu’on raconte le lendemain.",
         img="cockpit.jpg", alt="Un joueur dans le simulateur cockpit", pos="center 50%"),
    dict(slug="hovergame", titre="HoverGame", groupe="Interactif et IA",
         accroche="Une borne qui se joue sans la toucher : un capteur suit le survol de la main.",
         desc="Personne n’a besoin d’explication : on comprend en regardant jouer la personne d’avant. Course ou pilotage, plusieurs décors au choix.",
         etapes=["On tend la main au-dessus de la borne : la partie démarre.",
                 "On avance, on recule, on penche la main pour diriger.",
                 "Deux minutes de course, puis au suivant."],
         puces=[("1 ou 2", "joueurs, 2 min chacun"), ("Plusieurs décors", "course ou pilotage"), ("220 V", "sur table ou sur pied")],
         usage="Séminaire, pause, stand : 2 minutes, beaucoup de passage.",
         img="hovergame.jpg", alt="Une main au-dessus de la borne HoverGame", pos="center 40%"),
    dict(slug="escape-game-vr", titre="Escape game VR", groupe="Interactif et IA",
         accroche="Coopérez pour vous en sortir, entre monde virtuel et réalité.",
         desc="Casque sur la tête, votre équipe entre dans un environnement original et déconcertant. Quatre énigmes à résoudre ensemble, en se parlant et en se coordonnant.",
         etapes=["On s’équipe : un casque et deux manettes par joueur, dans un espace délimité.",
                 "L’équipe observe, manipule et déduit : chaque énigme résolue ouvre la suivante.",
                 "Mission réussie ou non, on en reparle toute la soirée."],
         puces=[("2 à 4", "joueurs, 4 casques"), ("15 à 30 min", "par session"), ("Technicien", "encadrement, prise 220 V")],
         usage="Team building, séminaire, soirée d’entreprise : l’épreuve qui se gagne en parlant.",
         img="vr.jpg", alt="Un joueur casque VR sur la tête, manette en main", pos="center 30%"),
    dict(slug="escape-game-pixel", titre="Escape game Pixel", groupe="Interactif et IA",
         accroche="Des énigmes à résoudre en équipe, autour d’une mallette en pixels.",
         desc="Une mallette fabriquée dans nos ateliers : des écrans en pixels, des boutons, une histoire. Cinq énigmes à résoudre pour ouvrir la boîte. Le scénario de la mallette, ou le vôtre sur devis.",
         etapes=["L’équipe ouvre la mallette : un message s’affiche, la mission commence.",
                 "Observer, manipuler, déduire : chaque bonne réponse déverrouille la suite.",
                 "Cinq énigmes plus tard, la boîte s’ouvre."],
         puces=[("1 à 3", "joueurs, autour d’une table"), ("5", "énigmes"), ("Bourges", "conçu et fabriqué")],
         usage="Séminaire, soirée d’entreprise, événement public : un défi d’équipe entre deux animations.",
         img="escape-pixel.jpg", alt="La mallette de l’escape game Pixel ouverte", pos="center 40%",
         gal=[("escape-pixel-2.jpg", "La mallette et ses écrans en pixels")]),
    dict(slug="led-game", titre="Led Game", groupe="Interactif et IA",
         accroche="Mémoire et réflexes, sur neuf boutons lumineux.",
         desc="Deux jeux dans une borne. Seul, on refait la séquence de couleurs, qui s’allonge à chaque tour. À deux, on se bat façon pong : la lumière file d’un camp à l’autre, il faut la renvoyer au bon moment.",
         etapes=["Mode séquence : les boutons s’allument dans un ordre, à vous de le refaire.",
                 "Mode pong : à deux, renvoyez la lumière dans le camp adverse.",
                 "Parties courtes, chacun son tour : la borne tourne toute seule."],
         puces=[("9", "boutons lumineux"), ("1 ou 2", "joueurs"), ("Sans animateur", "prise 220 V")],
         usage="Stand de salon, file d’attente, soirée : le jeu qui attire du monde et libère vite la place.",
         img="led-game.jpg", alt="Illustration : neuf boutons lumineux", cap="Illustration", pos="center"),
    dict(slug="sumo-robots", titre="Sumo robots", groupe="Interactif et IA",
         accroche="Poussez l’adversaire hors de l’arène.",
         desc="Deux petits robots, une arène ronde, une commande chacun. Le combat dure une minute : stratégie, feintes et coups de pression pour éjecter l’autre.",
         etapes=["Chaque joueur prend sa commande et son robot.",
                 "Au signal, on pousse, on esquive, on contourne.",
                 "Le premier robot sorti de l’arène a perdu. Revanche immédiate."],
         puces=[("2", "joueurs, 1 min par combat"), ("Tournoi", "possible entre collègues"), ("Une table", "suffit")],
         usage="Pause de séminaire, stand, soirée : le duel qui fait des spectateurs.",
         img="sumo-robots.jpg", alt="Illustration : deux robots dans une arène ronde", cap="Illustration", pos="center"),
    dict(slug="face-art", titre="Face Art", groupe="Interactif et IA",
         accroche="Un robot vous photographie, puis dessine votre portrait sous vos yeux.",
         desc="Un dispositif robotique doté d’une intelligence artificielle : la photo est transformée en croquis, puis le bras trace le portrait au feutre, sur papier. Chaque invité repart avec le sien.",
         etapes=["On s’installe face à l’appareil : une photo.",
                 "L’IA transforme la photo en croquis.",
                 "Le bras robotisé dessine, on regarde, on repart avec le papier."],
         puces=[("5 min", "par personne"), ("Un croquis", "à emporter"), ("220 V", "prise nécessaire")],
         usage="Salon, inauguration, soirée : l’animation qui fait s’arrêter les passants et laisse un objet.",
         img="face-art.jpg", alt="Illustration : un portrait dessiné au feutre", cap="Illustration", pos="center"),
    dict(slug="photo-emotion", titre="Photo Émotion", groupe="Interactif et IA",
         accroche="La borne photo qui se déclenche sur vos émotions.",
         desc="Une intelligence artificielle lit l’expression du visage. Elle demande une émotion, attend qu’elle apparaisse et prend la photo au bon moment. Pas de bouton : c’est l’expression qui déclenche.",
         etapes=["La borne demande une expression : surprise, joie, tristesse, air sérieux.",
                 "L’IA reconnaît l’émotion sur le visage et déclenche la photo.",
                 "Quatre expressions font une mosaïque : votre portrait par une IA, envoyé par mail."],
         puces=[("1 min", "par personne"), ("Vos émotions", "choisies avec vous"), ("220 V", "sur table ou sur pied")],
         usage="Accueil de salon, soirée d’entreprise, lancement : l’animation qui fait rire la file d’attente et casse la glace.",
         img="photo-emotion.jpg", alt="Illustration : mosaïque de quatre expressions", cap="Illustration", pos="center"),
]
GROUPES_ACCUEIL = [
    ("À la une", ["modules-pixel", "sign", "brique"]),
    ("Arcade et rétro", ["bornes-arcade", "flipper-numerique", "game-watch-geant", "simulateur-cockpit"]),
    ("Interactif et IA", ["hovergame", "escape-game-vr", "escape-game-pixel", "led-game", "sumo-robots", "face-art", "photo-emotion"]),
]
PAR_SLUG = {a["slug"]: a for a in ANIMS}


# ---------------------------------------------------------------- briques HTML
def entete(p):
    """p = préfixe vers la racine : '' (accueil) ou '../' (pages d'animation)."""
    return f'''<header>
  <div class="wrap">
    <a class="logo" href="{p}index.html">STUDIO <b>EYA</b></a>
    <nav>
      <a href="{p}index.html#catalogue">Animations</a>
      <a href="{p}animations/modules-pixel.html">Pixel</a>
      <a href="{p}animations/brique.html">Brique</a>
      <a href="{p}index.html#formules">Formules</a>
      <a class="btn" href="{p}index.html#contact">Demander un devis</a>
    </nav>
  </div>
</header>
'''


PIED = '''<footer>
  <div class="wrap">
    <span>Studio Eya · Bourges</span>
    <span>Conçu et fabriqué dans nos ateliers · Création Le Cercle</span>
  </div>
</footer>
'''


def carte(a, p):
    """La même carte pour toutes les animations."""
    return (f'<a class="card cat" href="{p}animations/{a["slug"]}.html">'
            f'<img src="{p}images/{a["img"]}" alt="{e(a["alt"])}" loading="lazy" style="object-position:{a.get("pos", "center")}">'
            f'<div><span class="tag">{e(a["groupe"])}</span><h3>{e(a["titre"])}</h3>'
            f'<p>{e(a["accroche"])}</p><span class="plus">Voir l’animation →</span></div></a>')


def page_html(titre, desc, p, corps, script=True):
    return f'''<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{e(titre)}</title>
<meta name="description" content="{e(desc)}">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;500;700&family=Pixelify+Sans:wght@500;600;700&display=swap" rel="stylesheet">
<link rel="stylesheet" href="{p}style.css">
</head>
<body>
{entete(p)}
<main id="top">
{corps}
</main>
{PIED}
<script src="{p}main.js"></script>
</body>
</html>
'''


EXTRA_PIXEL = '''<section class="alt">
  <div class="wrap">
    <div class="kicker">Pixel · mode jeu</div>
    <h2>Un bouton suffit pour jouer</h2>
    <p class="lead" style="margin-bottom:32px">Pas de notice : on comprend en regardant son voisin. Duels, parties libres, et en tournoi le classement s’affiche sur une tablette.</p>
    <div class="grid g4">
      <div class="game"><canvas id="g-snake" width="240" height="240" role="img" aria-label="Animation du jeu Snake"></canvas><h3>Snake</h3><p>Guidez le serpent et gobez les pommes. Ne vous mordez pas la queue !</p></div>
      <div class="game"><canvas id="g-briques" width="240" height="240" role="img" aria-label="Animation du jeu Briques"></canvas><h3>Briques</h3><p>Empilez les briques et complétez des lignes pour tenir le plus longtemps.</p></div>
      <div class="game"><canvas id="g-chenille" width="240" height="240" role="img" aria-label="Animation du jeu Chenillard"></canvas><h3>Chenillard</h3><p>Visez la tête de la chenille avec la bonne couleur pour la détruire.</p></div>
      <div class="game"><canvas id="g-palet" width="240" height="240" role="img" aria-label="Animation du jeu Palet"></canvas><h3>Palet</h3><p>Le duel : renvoyez le palet au bon moment. Le premier à 5 manches gagne.</p></div>
    </div>
    <div class="kicker" style="margin-top:56px">Pixel · mode déco</div>
    <h2>Vos couleurs, votre logo, vos messages</h2>
    <p class="lead">Quand personne ne joue, les tableaux s’animent seuls. Nous dessinons les visuels avec vous, vous les validez 7 jours avant l’événement. Avec la formule Pixel Plus, dix visuels à vos couleurs sont compris.</p>
    <p style="margin-top:22px"><a class="btn ghost" href="../index.html#formules">Voir les formules Pixel</a></p>
  </div>
</section>'''

EXTRA_BRIQUE = '''<section class="alt">
  <div class="wrap">
    <div class="kicker">Pour qui</div>
    <h2>Un même atelier, trois façons d’y entrer</h2>
    <div class="grid g3" style="margin-top:28px">
      <div class="card"><h3>Guidé · CM1, CM2</h3><p>Des boutons d’aide pour lancer son premier jeu. La console peut arriver déjà montée et testée.</p></div>
      <div class="card"><h3>Libre · 6e, 4e</h3><p>Tout part de la phrase qu’on écrit : aucune liste, aucun jeu tout fait.</p></div>
      <div class="card"><h3>Sous le capot · ados et adultes</h3><p>On lit le programme écrit par l’IA, on le modifie, on peut brancher son propre agent.</p></div>
    </div>
    <div class="note"><b>Une IA cadrée.</b> Celle de Mistral AI, qui ne sert ici qu’à créer des jeux. Aucune donnée personnelle : un pseudo et le code du groupe, ni nom, ni mail, ni photo. L’animateur garde la main sur tout ce qui est publié.</div>
    <p style="margin-top:26px"><a class="btn" href="https://bricode.fr" target="_blank" rel="noopener">Découvrir Brique sur bricode.fr</a></p>
  </div>
</section>'''
EXTRAS = {"pixel": EXTRA_PIXEL, "brique": EXTRA_BRIQUE}


def page_animation(a):
    p = "../"
    i = ANIMS.index(a)
    autres = [ANIMS[(i + k) % len(ANIMS)] for k in (1, 2, 3)]
    puces = "".join(f'<div class="chip"><b>{e(g)}</b>{e(pt)}</div>' for g, pt in a["puces"])
    etapes = "".join(f'<div class="card"><span class="n">{k + 1}</span><p>{e(t)}</p></div>' for k, t in enumerate(a["etapes"]))
    cap = f'<figcaption>{e(a["cap"])}</figcaption>' if a.get("cap") else ""
    gal = ""
    if a.get("gal"):
        figs = "".join(f'<figure><img src="{p}images/{f}" alt="{e(t)}" loading="lazy"><figcaption>{e(t)}</figcaption></figure>' for f, t in a["gal"])
        gal = f'<section><div class="wrap"><div class="kicker">En images</div><div class="galerie">{figs}</div></div></section>'
    corps = f'''<section class="page-hero">
  <div class="wrap">
    <div>
      <a class="crumb" href="{p}index.html#catalogue">← Toutes les animations</a>
      <div class="kicker">{e(a["groupe"])}</div>
      <h1>{e(a["titre"])}</h1>
      <p class="lead">{e(a["accroche"])}</p>
      <div class="chips">{puces}</div>
      <div class="cta"><a class="btn" href="{p}index.html#contact">Demander un devis</a></div>
    </div>
    <figure class="hero-fig"><img src="{p}images/{a["img"]}" alt="{e(a["alt"])}" style="object-position:{a.get("pos", "center")}">{cap}</figure>
  </div>
</section>

<section class="alt">
  <div class="wrap">
    <div class="split">
      <div><div class="kicker">En deux mots</div><h2>L’animation</h2><p class="lead">{e(a["desc"])}</p></div>
      <div class="card ideal"><span class="tag">Idéal pour</span><p>{e(a["usage"])}</p></div>
    </div>
  </div>
</section>

<section>
  <div class="wrap">
    <div class="kicker">Comment ça marche</div>
    <h2>En trois étapes</h2>
    <div class="grid g3" style="margin-top:28px">{etapes}</div>
  </div>
</section>

{gal}
{EXTRAS.get(a.get("extra"), "")}

<section class="alt">
  <div class="wrap">
    <div class="head"><div><div class="kicker">Et aussi</div><h2>Autres animations</h2></div></div>
    <div class="grid g3">{"".join(carte(x, "../") for x in autres)}</div>
  </div>
</section>

<section class="bandeau">
  <div class="wrap">
    <h2>{e(a["titre"])} pour votre événement ?</h2>
    <p class="lead">Donnez-nous la date, le lieu et le nombre d’invités : nous vous répondons avec une formule et un devis détaillé.</p>
    <p style="margin-top:22px"><a class="btn" href="{p}index.html#contact">Demander un devis</a></p>
  </div>
</section>'''
    return page_html(f'{a["titre"]} — Studio Eya', a["accroche"], p, corps)


def accueil():
    hero = entre("<!-- HERO -->", "<!-- POURQUOI -->").replace('href="#pixel"', 'href="animations/modules-pixel.html"')
    blocs = ""
    for titre, slugs in GROUPES_ACCUEIL:
        cartes = "".join(carte(PAR_SLUG[s], "") for s in slugs)
        cols = "g3"
        blocs += f'<h3 class="grp">{e(titre)}</h3><div class="grid {cols}">{cartes}</div>\n'
    catalogue = f'''<!-- CATALOGUE -->
<section id="catalogue">
  <div class="wrap">
    <div class="head">
      <div><div class="kicker">Le catalogue</div><h2>Nos animations<br>en un coup d’œil</h2></div>
      <p class="lead">Chaque animation a sa page : comment ça marche, pour quel moment, ce qu’il faut prévoir. Tout se combine librement.</p>
    </div>
    {blocs}
    <div class="note"><b>★</b> Et sur demande, à l’intérieur de nos formules : baby-foot, air hockey, machines à pince, jeux forains, jeux sportifs, borne photo.</div>
  </div>
</section>

'''
    suite = entre("<!-- PERSONNALISATION -->", "</main>")
    suite = suite.replace('<section class="alt">', '<section class="alt">', 1)
    corps = hero + entre("<!-- POURQUOI -->", "<!-- CATALOGUE -->") + catalogue + suite
    return page_html("Studio Eya — Faites jouer vos invités",
                     "Tableaux lumineux Pixel, bornes rétro, jeux interactifs et IA, et l'atelier Brique : conçus et fabriqués à Bourges. Montage, technicien et démontage compris.",
                     "", corps)


# ---------------------------------------------------------------- style et script
css = re.search(r"<style>(.*?)</style>", vieux, re.S).group(1)
css += '''
/* pages d'animation */
.cat p{color:var(--mut);font-size:14.5px;margin-bottom:10px}
.cat .plus{color:var(--amb);font-size:14px;font-weight:700}
.cat img{aspect-ratio:16/10}
.grp{margin:44px 0 16px;color:var(--amb)}
.grp:first-of-type{margin-top:0}
.page-hero{padding:48px 0 70px}
.page-hero .wrap{display:grid;grid-template-columns:1.05fr .95fr;gap:44px;align-items:center}
.page-hero h1{font-size:clamp(38px,5.4vw,64px);margin-bottom:18px}
.page-hero .chips{margin:26px 0}
.crumb{display:inline-block;color:var(--mut);text-decoration:none;font-size:14px;margin-bottom:22px}
.crumb:hover{color:var(--amb)}
.hero-fig{margin:0}
.hero-fig img{width:100%;aspect-ratio:4/3;object-fit:cover;border-radius:16px;border:1px solid var(--line);box-shadow:0 30px 80px rgba(0,0,0,.45)}
figcaption{font-size:13px;color:var(--mut);margin-top:8px}
.galerie{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;margin-top:6px}
.galerie img{width:100%;aspect-ratio:4/3;object-fit:cover;border-radius:12px;border:1px solid var(--line)}
.galerie figure{margin:0}
.ideal{align-self:center}
.ideal p{color:var(--txt);font-size:18px;margin-top:8px}
.bandeau{text-align:center;padding:70px 0}
.bandeau .lead{margin:0 auto}
@media(max-width:860px){.page-hero .wrap{grid-template-columns:1fr}}
'''
js = vieux[vieux.index("(function(){"): vieux.index("</script>")]
js = js.replace("const cv=document.getElementById(id), c=cv.getContext('2d')",
                "const cv=document.getElementById(id); if(!cv) return; const c=cv.getContext('2d')")
js = js.replace("const f=document.getElementById('form'), etat=document.getElementById('form-etat');",
                "const f=document.getElementById('form'), etat=document.getElementById('form-etat'); if(!f) return;")

# ---------------------------------------------------------------- écriture
def ecrire(chemin, texte):
    chemin = os.path.join(ICI, chemin)
    os.makedirs(os.path.dirname(chemin), exist_ok=True)
    open(chemin, "w", encoding="utf-8", newline="\n").write(texte)


ecrire("style.css", css)
ecrire("main.js", js)
ecrire("index.html", accueil())
for a in ANIMS:
    ecrire(f"animations/{a['slug']}.html", page_animation(a))
print("ok :", len(ANIMS), "pages d'animation")
