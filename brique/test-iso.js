// node test-iso.js : genererIso() (index.html) fait des niveaux que nivValide() (server.js) accepte et qu'on peut finir avec les regles de la console
const fs = require('fs'), assert = require('assert');
const page = fs.readFileSync(__dirname + '/index.html', 'utf8'), serv = fs.readFileSync(__dirname + '/server.js', 'utf8');
const bout = (src, debut) => { const i = src.indexOf(debut); let j = src.indexOf('\n}\n', i); return src.slice(i, j + 2); };
const { genererIso, nivValide } = new Function(`${bout(page, 'function rnd(')}\n${bout(page, 'function genererIso(')}\n${bout(serv, 'function nivValide(')}\nreturn { genererIso, nivValide };`)();
for (let seed = 1; seed <= 2000; seed++) {
  const p = { seed, taille: 6 + seed % 7, relief: ['plat', 'collines', 'montagnes'][seed % 3], trous: seed % 4, difficulte: 1 + seed % 3, etoiles: seed % 9, briques: { pic: seed % 2 === 0, cache: seed % 3 === 0 } };
  const rows = genererIso(p), niv = { params: { jeu: 'iso', ...p }, rows };
  assert(nivValide(niv), 'nivValide ' + seed);
  assert(JSON.stringify(niv).length < 3600, 'trop long ' + seed);
  const N = rows.length, h = p.haut.map(l => [...l].map(Number)), vu = new Set([N * N - 1]), file = [[N - 1, N - 1]];
  while (file.length) {                                     // memes regles que isoStep() : une marche a monter au plus, trou et pics fixes interdits
    const [y, x] = file.shift();
    for (const [dy, dx] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const Y = y + dy, X = x + dx;
      if (Y < 0 || X < 0 || Y >= N || X >= N || vu.has(Y * N + X) || '.^'.includes(rows[Y][X]) || h[Y][X] > h[y][x] + 1) continue;
      vu.add(Y * N + X); file.push([Y, X]);
    }
  }
  assert(vu.has(0) && rows[0][0] === 'G', 'arrivee inaccessible ' + seed);
}
console.log('OK genererIso : 2000 niveaux valides et faisables');
