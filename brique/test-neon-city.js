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

const course = (n, fin, pre = '') => lancer(`sel = ${n}\n${pre}\n${FREQ}
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
    if size(IX) > 0 && etat == 1 IX = [] IY = [] end   # le pilote ignore les caisses (le turbo le sort des virages)
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
// garage : P3 ouvre le garage (dernier choix), D change vehicule, couleur, carte ; P3 passe a la ligne suivante, puis retour au menu
const g = lancer(`
var _m = maj var _tt = 0.0 var _c = 0 var _n = 0 var _cur = -1
var _seq = [2, 1, 2, 1, 2, 1, 2, 4, 2]
bouton = def (k) return false end
appui = def (k) return k == _cur end
maj = def (dt)
  _n += 1
  if _n == 1 sel = size(SC) end
  if _n % 3 == 0 && _c < size(_seq)
    _cur = _seq[_c] _c += 1
    if _cur == 4 _cur = -1 sel = 1 end
  else
    _cur = -1
  end
  _m(dt)
  if _c >= size(_seq) && _n % 3 == 1 raise "mesure", str(etat) + " " + str(ve) + " " + str(vc) + " " + str(tm) + " " + str(VMAX == 190.0 ? 1 : 0) + " " + str(TAXI == VCOL[1] ? 1 : 0) end
end`);
console.log('GARAGE    ', JSON.stringify(g));
// etat 1 (parti), vehicule 1 (SPORT), couleur 1, carte 1, vitesse 190, couleur appliquee (lus dans les champs rang / temps / kills / cp du decodeur)
assert(g.etat === 1 && g.rang === 1 && g.temps === 0.1 && g.kills === 1 && g.cp === 1, 'garage : ' + JSON.stringify(g));

// objets : tous les objets sortent, selon le mode
const objets = (n) => {
  const v = JSON.parse(execFileSync(VERIF, [], { input: src + `\nimport string\nsel = ${n}\nvar _m = maj var _all = ""\nmaj = def (dt)\n  _m(dt)\n  if etat == 0 etat = 1 reset() end\n  var i = 0\n  while i < 200 objet() if string.find(_all, msg) < 0 _all += msg + "/" end i += 1 end\n  raise "mesure", _all\nend\n` })), m = /mesure: (.*)/.exec(v.erreur || '');
  assert(m, 'objets : ' + (v.erreur || JSON.stringify(v).slice(0, 200)));
  return m[1].trim();
};
const o0 = objets(0), o1 = objets(3), o2 = objets(5);
console.log('OBJETS    ', o0, '|', o1, '|', o2);
['TURBO !', 'BOUCLIER !', '+1 COEUR'].forEach(t => assert(o0.includes(t), 'taxi : ' + t));
assert(o1 === 'TURBO !/', 'course : turbo seulement');
['TURBO !', 'BOUCLIER !', 'OBUS +2'].forEach(t => assert(o2.includes(t), 'battle : ' + t));

// vehicules et cartes : chaque combinaison se joue sans erreur ; chaque vehicule finit le permis
for (let t = 0; t < 3; t++) for (let k = 0; k < 4; k++) for (const n of [0, 3, 5]) {
  const v = JSON.parse(execFileSync(VERIF, [], { input: src + `\nve = ${k}\ntm = ${t}\nsel = ${n}\n` }));
  assert(v.ok, `vehicule ${k} carte ${t} scenario ${n} : ${v.erreur}`);
}
for (const k of [1, 2, 3]) {
  r = course(1, 60, `ve = ${k}`);
  console.log('PERMIS veh', k, JSON.stringify(r));
  assert(r.etat === 3 || r.etat === 2, 'le permis doit se terminer');
}
console.log('ok');
