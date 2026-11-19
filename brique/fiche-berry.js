// Brique : la fiche de jeu (fiche.js) jouee par un moteur BERRY (moteur-fiche.be) au lieu du moteur en C du firmware.
// L'IA ne rend que des donnees (la fiche, du JSON : pas de syntaxe a rater) ; ici on les verifie (Fiche.nettoyer) et on les colle devant le moteur.
// assembler(fiche) -> { script, refus } : le script part tel quel sur une console Berry. Plateforme seulement (cote et iso restent au firmware en C).
const fs = require('fs'), path = require('path'), Fiche = require('./fiche.js');
const serre = src => src.split('\n').map(l => { let q = false, i = 0; for (; i < l.length; i++) { if (l[i] === '"') q = !q; else if (l[i] === '#' && !q) break; } return l.slice(0, i).trim(); }).filter(Boolean).join('\n');   // sans commentaires ni retraits : moins de memoire pour compiler sur la console
const lireMoteur = f => serre(fs.readFileSync(path.join(__dirname, f), 'utf8'));
const MOTEURS = { plateforme: lireMoteur('moteur-fiche.be'), casse: lireMoteur('moteur-casse.be') };   // un moteur par genre : le modele de la fiche choisit
const MOTEUR = MOTEURS.plateforme;

function assembler(brute) {
  const { fiche: f, refus } = Fiche.nettoyer(brute);
  if (!MOTEURS[f.modele]) return { refus: [...refus, `le moteur Berry ne sait pas encore jouer « ${f.modele} » (plateforme et casse seulement)`] };
  const bloquants = Fiche.lire(f).bloquants; if (bloquants.length) return { refus: [...refus, ...bloquants] };   // une lettre inconnue, pas de depart : la page ne l'enverrait pas non plus
  const lire = x => typeof x === 'string' ? [x, undefined] : Object.entries(x)[0];                     // "mot" ou {"mot": valeur}
  const regles = f.regles.map(r => { const [q, qv] = lire(r.quand), [a, av] = lire(r.alors); return { q, qv: qv ?? 0, a, av: av ?? 0 }; });   // a plat : le moteur ne teste pas de types
  const [but, butv] = lire(f.but);
  const donnees = f.modele === 'casse' ? { rows: f.rows, heros: f.heros, balle: f.balle, briques: f.briques, regles, but, butv: butv ?? 0 }
    : { rows: f.rows, monde: f.monde, heros: f.heros, briques: f.briques, objets: f.objets, regles, but, butv: butv ?? 0 };
  // du JSON est deja du Berry : tables { "cle": valeur }, listes [...], chaines "..." (la fiche n'a que de l'ASCII sans " ni \)
  return { refus, script: '# Jeu declare : les donnees (fiche) puis le moteur (moteur-fiche.be)\nvar FICHE = ' + JSON.stringify(donnees) + '\n' + MOTEURS[f.modele] };
}
module.exports = { assembler, MOTEUR };
if (require.main === module) {   // node fiche-berry.js ma-fiche.json jeu.be : un jeu pret a envoyer sur une console Berry (page Creer en mode script, ou tester-consoles.ps1)
  const r = assembler(JSON.parse(fs.readFileSync(process.argv[2] || 0, 'utf8')));
  r.refus.forEach(x => console.error('refus : ' + x)); if (!r.script) process.exit(1); if (process.argv[3]) fs.writeFileSync(process.argv[3], r.script); else process.stdout.write(r.script);   // avec un 2e argument on ecrit le fichier nous-memes : le ">" de PowerShell 5.1 l'ecrit en UTF-16
}
