// Brique : la fiche de jeu (fiche.js) jouee par un moteur BERRY (moteur-fiche.be) au lieu du moteur en C du firmware.
// L'IA ne rend que des donnees (la fiche, du JSON : pas de syntaxe a rater) ; ici on les verifie (Fiche.nettoyer) et on les colle devant le moteur.
// assembler(fiche) -> { script, refus } : le script part tel quel sur une console Berry. Plateforme seulement (cote et iso restent au firmware en C).
const fs = require('fs'), path = require('path'), Fiche = require('./fiche.js');
const serre = src => src.split('\n').map(l => { let q = false, i = 0; for (; i < l.length; i++) { if (l[i] === '"') q = !q; else if (l[i] === '#' && !q) break; } return l.slice(0, i).trim(); }).filter(Boolean).join('\n');   // sans commentaires ni retraits : moins de memoire pour compiler sur la console
const MOTEUR = serre(fs.readFileSync(path.join(__dirname, 'moteur-fiche.be'), 'utf8'));

function assembler(brute) {
  const { fiche: f, refus } = Fiche.nettoyer(brute);
  if (f.modele !== 'plateforme') return { refus: [...refus, `le moteur Berry ne sait jouer que la plateforme (pas « ${f.modele} »)`] };
  const lire = x => typeof x === 'string' ? [x, undefined] : Object.entries(x)[0];                     // "mot" ou {"mot": valeur}
  const regles = f.regles.map(r => { const [q, qv] = lire(r.quand), [a, av] = lire(r.alors); return { q, qv: qv ?? 0, a, av: av ?? 0 }; });   // a plat : le moteur ne teste pas de types
  const [but, butv] = lire(f.but);
  const donnees = { rows: f.rows, monde: f.monde, heros: f.heros, briques: f.briques, objets: f.objets, regles, but, butv: butv ?? 0 };
  // du JSON est deja du Berry : tables { "cle": valeur }, listes [...], chaines "..." (la fiche n'a que de l'ASCII sans " ni \)
  return { refus, script: '# Jeu declare : les donnees (fiche) puis le moteur (moteur-fiche.be)\nvar FICHE = ' + JSON.stringify(donnees) + '\n' + MOTEUR };
}
module.exports = { assembler, MOTEUR };
if (require.main === module) {   // node fiche-berry.js ma-fiche.json > jeu.be : un jeu pret a envoyer sur une console Berry (page Creer en mode script, ou tester-consoles.ps1)
  const r = assembler(JSON.parse(fs.readFileSync(process.argv[2] || 0, 'utf8')));
  r.refus.forEach(x => console.error('refus : ' + x)); if (r.script) process.stdout.write(r.script); else process.exit(1);
}
