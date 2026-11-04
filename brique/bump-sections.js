// Monte d'un cran (1.4 -> 1.5) le data-version de chaque <main> de index.html touché par le commit en cours.
// Lancé par .git/hooks/pre-commit (installer : node brique/bump-sections.js --installer).
// ponytail: lit index.html sur le disque, suppose qu'il est commité en entier (pas de staging partiel).
const fs = require('fs'), path = require('path'), { execSync } = require('child_process');
const racine = execSync('git rev-parse --show-toplevel').toString().trim();

if (process.argv[2] === '--installer') {
  fs.writeFileSync(path.join(racine, '.git/hooks/pre-commit'), '#!/bin/sh\nnode brique/bump-sections.js || exit 1\n');
  return console.log('hook pre-commit installé');
}

const diff = execSync('git diff --cached -U0 -- brique/index.html', { cwd: racine }).toString();
const touchees = [];                                      // lignes du nouveau fichier modifiées
for (const [, debut, nb = '1'] of diff.matchAll(/^@@ -\S+ \+(\d+)(?:,(\d+))? @@/gm))
  for (let i = 0; i < Math.max(+nb, 1); i++) touchees.push(+debut + i);
if (!touchees.length) return;

const f = path.join(racine, 'brique/index.html');
const lignes = fs.readFileSync(f, 'utf8').split('\n');
let ouverte = -1, montees = [];
lignes.forEach((l, i) => {
  if (/^<main [^>]*data-version="/.test(l)) ouverte = i;
  else if (ouverte >= 0 && l.startsWith('</main>')) {
    if (touchees.some(n => n - 1 >= ouverte && n - 1 <= i)) {
      lignes[ouverte] = lignes[ouverte].replace(/data-version="(\d+)\.(\d+)"/, (_, a, b) => `data-version="${a}.${+b + 1}"`);
      montees.push(lignes[ouverte].match(/id="(\w+)"[^>]*data-version="([\d.]+)"/).slice(1).join(' v'));
    }
    ouverte = -1;
  }
});
if (!montees.length) return;
fs.writeFileSync(f, lignes.join('\n'));
execSync('git add brique/index.html', { cwd: racine });
console.log('sections montées : ' + montees.join(', '));
