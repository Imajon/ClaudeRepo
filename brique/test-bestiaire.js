// node test-bestiaire.js : les formes sont bien faites (dans la grille 16 x 16, ids uniques, jeux qui pointent vers des formes qui existent) et leur Berry tourne dans verif
const assert = require('assert'), { execFileSync } = require('child_process'), path = require('path'), fs = require('fs');
const B = require('./bestiaire.js');
const ids = new Set();
for (const f of B.FORMES) {
  assert(!ids.has(f.id), 'id en double ' + f.id); ids.add(f.id);
  assert(B.THEMES.some(t => t.id === f.theme) && B.ROLES[f.role], f.id + ' : theme ou role inconnu');
  assert(/^#[0-9a-f]{6}$/.test(f.a) && /^#[0-9a-f]{6}$/.test(f.b), f.id + ' : couleurs');
  for (const p of f.p) for (const n of p.slice(1, p[0] === 'r' ? 5 : p[0] === 'd' ? 4 : p[0] === 't' ? 7 : 5)) assert(Number.isInteger(n) && n >= 0 && n <= 16, `${f.id} : ${JSON.stringify(p)} hors grille`);
}
for (const j of B.JEUX) for (const e of j.el) {
  for (const id of e.f || []) assert(B.forme(id), `${j.id}/${e.n} : forme ${id} inconnue`);
  assert(B.ROLES[e.role] || B.ROLES_SANS_FORME[e.role], `${j.id}/${e.n} : role ${e.role}`);
  if (!e.f && B.ROLES[e.role]) assert(B.FORMES.some(f => f.role === e.role), `${j.id}/${e.n} : aucune forme pour ${e.role}`);
}
const VERIF = path.join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');
if (fs.existsSync(VERIF)) {
  const code = '# Bestiaire\n' + B.FORMES.map(f => B.berry(f)).join('\n') + '\ndef dessine()\n  fond(rgb(40, 40, 60))\n  ' + B.FORMES.map((f, i) => `forme_${f.id.replace(/-/g, '_')}(${(i % 16) * 20}, ${int(i / 16) * 20}, 1)`.replace('int(', 'int(')).join('\n  ') + '\nend\n';
  const v = JSON.parse(execFileSync(VERIF, [], { input: code }));
  assert(v.ok, v.erreur);
  console.log(`ok : ${B.FORMES.length} formes, ${B.JEUX.length} jeux, Berry joue (${v.ops} dessins)`);
} else console.log('verif non compile : Berry non joue');
function int(x) { return Math.floor(x); }
