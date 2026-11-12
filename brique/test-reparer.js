// node test-reparer.js : les reparations mecaniques de reparer.js
const assert = require('assert'), R = require('./reparer.js');
assert.strictEqual(R.texte('# Jeu\nif a or b and not c x = max(1, 2) end'), '# Jeu\nimport math\nif a || b && !c x = math.max(1, 2) end');
assert.strictEqual(R.texte('texte("or and", 1, 2) # or'), 'texte("or and", 1, 2) # or');           // chaines et commentaires intacts
assert.strictEqual(R.texte('def max(a, b) return a end\nx = max(1, 2) y = math.min(1, 2)'), 'import math\ndef max(a, b) return a end\nx = max(1, 2) y = math.min(1, 2)');
assert.strictEqual(R.texte('for i: 0 .. 360 step 10 end'), 'for i: range(0, 360, 10) end');
assert.strictEqual(R.redefinition('var i = 0\nvar i = a\nwhile i < 3', "jeu:3: redefinition of 'i'"), 'var i = 0\ni = a\nwhile i < 3');   // signale une ligne apres
assert.strictEqual(R.redefinition('var i = 0\nvar i = 0 while i < 3', "jeu:2: redefinition of 'i'"), 'var i = 0\ni = 0 while i < 3');  // sur la ligne meme
assert.strictEqual(R.redefinition('x', "jeu:1: 'x' undeclared"), null);
console.log('reparer.js : ok');
