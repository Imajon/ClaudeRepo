// Neon City : joue les scenarios dans verif. node test-neon-city.js
//  - permis / courses : pilote automatique qui vise le prochain coin et freine avant (verif ne joue que 600 images : maj() est appelee 16 fois par image = 160 s de jeu)
//  - battle : duel scripte (2 rivaux alignes devant le joueur, a 1 ballon chacun) : les obus touchent, la victoire arrive
const assert = require('assert'), fs = require('fs'), path = require('path'), { execFileSync } = require('child_process'), { VERIF } = require('./essai-verif');
const src = fs.readFileSync(path.join(__dirname, 'neon-city.be'), 'utf8');
const lancer = (fin) => {
  const v = JSON.parse(execFileSync(VERIF, [], { input: src + fin })), m = /mesure: (.*)/.exec(v.erreur || '');
  assert(m, 'pas de mesure : ' + (v.erreur || JSON.stringify(v).slice(0, 300)));
  const [etat, rang, t10, kills, cp] = m[1].trim().split(' ').map(Number);
  return { etat, rang, temps: t10 / 10, kills, cp };
};
const MESURE = 'str(etat) + " " + str(rang) + " " + str(int(T * 10)) + " " + str(kills) + " " + str(cp)';
const FREQ = 'var _m = maj var _tt = 0.0 var _st = 0.0 var _rv = 0.0 var _px = 0.0 var _py = 0.0\n';

const course = (n, fin) => lancer(`sel = ${n}\n${FREQ}
def _sc(k, t)
  var df = math.atan2(ty - y, tx - x) - h
  while df > 3.14159 df -= 6.28318 end
  while df < -3.14159 df += 6.28318 end
  if k == 0 return etat == 1 && df < -0.06 end
  if k == 1 return etat == 1 && df > 0.06 end
  if etat != 1 return t > 0.3 && t < 0.5 end
  if math.sqrt(d2(tx, ty, x, y)) < 150 && v > 85 return false end   # freine avant le coin
  return df < 0.7 && df > -0.7 || v < 70
end
bouton = def (k) return _sc(k, _tt) end
appui = def (k) return _sc(k, _tt) && !_sc(k, _tt - 0.0167) end
maj = def (dt)
  var i = 0
  while i < 16
    _tt += dt _m(dt)
    # coince contre un mur ou un rival : marche arriere 0,8 s (le vrai joueur double-appuie sur P3)
    _st += dt
    if _st > 1.0
      if etat == 1 && cd <= 0 && _rv <= 0 && d2(x, y, _px, _py) < 100 g = -1 _rv = 0.8 end
      _st = 0.0 _px = x _py = y
    end
    if _rv > 0 _rv -= dt if _rv <= 0 g = 1 end end
    i += 1
  end
  if etat >= 2 || _tt > ${fin} raise "mesure", ${MESURE} end
end`);

const duel = () => lancer(`sel = 5\n${FREQ}
var _set = false
bouton = def (k) return (k == 2 && etat != 1) || (etat == 1 && _set && k < 2 && tir <= 0 && mun > 0) end
appui = def (k) return k == 2 && _tt > 0.3 && _tt < 0.35 end
maj = def (dt)
  var i = 0
  while i < 3
    _tt += dt _m(dt) pv = 3 mun = 3
    if etat == 1 && cd <= 0 && !_set
      _set = true h = 1.5708 x = 408.0 y = 280.0
      var b = nt
      while b < size(CX)
        CK[b] = 1 CW[b] = 5.0 CX[b] = 408.0 CY[b] = 400.0 + (b - nt) * 80 CD[b] = 0 GX[b] = 3 GY[b] = 3 LX[b] = 0.0 LY[b] = 0.0
        b += 1
      end
    end
    i += 1
  end
  if etat >= 2 || _tt > 20 raise "mesure", ${MESURE} end
end`);

let r = course(1, 40);
console.log('PERMIS    ', JSON.stringify(r));
assert(r.etat === 3 && r.temps <= 11, 'le pilote doit finir le permis en or');
r = course(2, 60);
console.log('PERMIS GT ', JSON.stringify(r));
assert(r.etat === 3 && r.temps <= 19, 'le pilote doit finir le grand permis en or');
r = course(3, 90);
console.log('COURSE    ', JSON.stringify(r));
assert(r.etat === 3 && r.rang === 1, 'le pilote doit gagner la course locale');
r = course(4, 120);
console.log('GRAND PRIX', JSON.stringify(r));
assert(r.etat === 3, 'le pilote doit finir le grand prix');
r = duel();
console.log('BATTLE    ', JSON.stringify(r));
assert(r.etat === 3 && r.kills === 2, 'les obus doivent faire eclater les 2 rivaux');
console.log('ok');
