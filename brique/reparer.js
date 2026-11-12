// Reparations mecaniques des programmes Berry ecrits par l'IA, sans la rappeler (gratuit, instantane). Test : node test-reparer.js
// Banc nov. 2026 : 12 des 39 programmes qui ne tournaient pas tombaient sur une de ces erreurs.
const horsTextes = (l, f) => l.replace(/("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|#.*$)|[^"'#]+/g, (m, t) => t ? t : f(m));   // f ne touche que le code (pas les chaines, pas les commentaires)
const MATHS = ['min', 'max', 'abs', 'sqrt', 'floor', 'ceil', 'sin', 'cos', 'atan2', 'pow'];

function texte(s) {                                      // erreurs reconnaissables a la lecture
  const defs = new Set([...s.matchAll(/\bdef\s+(\w+)/g)].map(m => m[1])), fs = MATHS.filter(f => !defs.has(f));
  const sansMath = new RegExp(`(?<![.\\w])(${fs.join('|')})\\s*\\(`, 'g');   // max(a, b) -> math.max(a, b) (sauf si le jeu a son propre def max)
  s = s.split('\n').map(l => horsTextes(l, c => c.replace(/\band\b/g, '&&').replace(/\bor\b/g, '||').replace(/\bnot\s+/g, '!')   // Berry : && || ! (and / or / not = "undeclared")
    .replace(sansMath, 'math.$1(')
    .replace(/(\bfor\s+\w+\s*:\s*)(.+?)\s*\.\.\s*(.+?)\s+step\s+(\S+)/g, '$1range($2, $3, $4)'))).join('\n');   // "for i: 0 .. 360 step 10" n'existe pas -> range(debut, fin, pas)
  for (const m of ['math', 'string'])                    // "math.sin" sans "import math" : on ajoute l'import (apres la ligne du nom du jeu)
    if (new RegExp(`\\b${m}\\.`).test(s) && !new RegExp(`^\\s*import\\s+${m}\\b`, 'm').test(s))
      s = s.startsWith('#') ? s.replace(/^(.*\n?)/, `$1import ${m}\n`) : `import ${m}\n` + s;
  return s;
}

function redefinition(s, erreur) {                       // "jeu:221: redefinition of 'i'" -> le 2e "var i" devient "i" ; null si autre erreur
  const m = /jeu:(\d+): redefinition of '(\w+)'/.exec(erreur || ''); if (!m) return null;
  const L = s.split('\n'), n = +m[1] - 1, re = new RegExp(`\\bvar\\s+${m[2]}\\b`, 'g');
  const sur = i => [...(L[i] || '').matchAll(re)];       // Berry signale la ligne du mot qui suit : la ligne elle-meme (1re occurrence) ou celle d'avant (derniere)
  const [i, o] = sur(n).length ? [n, sur(n)[0]] : sur(n - 1).length ? [n - 1, sur(n - 1).at(-1)] : [];
  if (o === undefined) return null;
  L[i] = L[i].slice(0, o.index) + m[2] + L[i].slice(o.index + o[0].length);
  return L.join('\n');
}

module.exports = { texte, redefinition };
