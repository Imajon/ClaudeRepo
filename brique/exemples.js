// Les jeux d'exemple du mode script : exemples/*.be, jeux complets verifies (node test-exemples.js).
// 2e ligne de chaque fichier = "# exemple: <titre> | mots: a, b, ..." ; l'IA ne recoit que les 2 exemples les plus proches de la demande.
const fs = require('fs'), path = require('path');
const DOSSIER = path.join(__dirname, 'exemples');
const sansAccent = t => t.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase();
const EXEMPLES = fs.readdirSync(DOSSIER).filter(f => f.endsWith('.be')).sort().map(f => {
  const t = fs.readFileSync(path.join(DOSSIER, f), 'utf8').replace(/\r/g, ''), m = /^# exemple: (.*?) \| mots: (.*)\n/m.exec(t);
  if (!m) throw new Error(`exemples/${f} : 2e ligne "# exemple: ... | mots: ..." manquante`);
  return { f, titre: m[1], mots: m[2].split(',').map(s => s.trim()).filter(Boolean), code: t.replace(m[0], '').trim() };
});
for (const e of EXEMPLES) e.re = e.mots.map(s => new RegExp('\\b' + sansAccent(s)));

function choisir(...textes) {                            // ponytail: choix par mots-cles, a remplacer par un tri fait par l'IA s'il se trompe souvent
  const s = sansAccent(textes.flat().filter(t => typeof t === 'string').join(' '));
  return EXEMPLES.map(e => ({ e, n: e.re.filter(r => r.test(s)).length })).filter(x => x.n).sort((a, b) => b.n - a.n).slice(0, 2).map(x => x.e);
}
const texte = ex => ex.length ? '\nEXEMPLES COMPLETS proches de la demande (ils tournent sans erreur). Ne les recopie pas : prends seulement la technique utile a la demande, et garde le plus petit jeu possible au debut.\n\n'
  + ex.map(e => `--- ${e.titre}\n${e.code}\n`).join('\n') : '';

module.exports = { EXEMPLES, choisir, texte };
