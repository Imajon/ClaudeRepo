// Mesures precises dans verif (meme Berry que la console) : des boutons SCRIPTES (verif joue sa propre partie sinon) et des valeurs lues dans le jeu a des instants donnes.
// Sert aux tests des moteurs de fiche (test-fiche-berry.js, test-casse-berry.js).
// essai(nom, script, { scen, mesure, quand, duree }) :
//   scen   = corps Berry de _sc(k, t) : k = 0 G, 1 D, 2 P3 ; t = secondes depuis le debut ; peut lire les variables du jeu (ex. un pilote automatique : "return k == 1 && bx > rx")
//   mesure = expression Berry notee a chacun des instants `quand` (secondes) ; rend les nombres
const assert = require('assert'), { execFileSync } = require('child_process'), path = require('path');
const VERIF = path.join(__dirname, 'verif', process.platform === 'win32' ? 'verif.exe' : 'verif');
function essai(nom, script, { scen = 'return false', mesure, quand, duree }) {
  const fin = `
var _m = maj
var _smp = ${JSON.stringify(quand)} var _i = 0 var _log = ""
def _sc(k, t) ${scen} end
bouton = def (k) return _sc(k, temps() - 2.0) end
appui = def (k) var t = temps() - 2.0 return _sc(k, t) && !_sc(k, t - 0.0167) end
maj = def (dt)
  _m(dt) var t = temps() - 2.0
  if _i < size(_smp) && t >= _smp[_i] _log += str(${mesure}) + " " _i += 1 end
  if t >= ${duree ?? quand[quand.length - 1] + 0.1} raise "mesure", _log end
end
`;
  const v = JSON.parse(execFileSync(VERIF, [], { input: script + fin })), m = /mesure: (.*)/.exec(v.erreur || '');
  assert(m, `${nom} : pas de mesure (${v.erreur})`);
  return m[1].trim().split(' ').map(Number);
}
const pres = (nom, got, att, tol) => { att.forEach((a, i) => assert(Math.abs(got[i] - a) <= tol, `${nom} : instant ${i} = ${got[i]}, attendu ${a} +/- ${tol} (tout : ${got})`)); console.log(`  ${nom} : ${got.join(' ')}`); };
module.exports = { VERIF, essai, pres };
