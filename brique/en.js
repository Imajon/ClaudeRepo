// Version anglaise du site : index.html est découpé en segments de texte (HTML et chaînes du JS),
// ceux qui ont une entrée dans en.json sont remplacés ; server.js sert le résultat sur /en. Rien d'autre à maintenir :
// un texte français modifié ou ajouté reste français sur /en tant que en.json n'a pas sa traduction.
//   node en.js            segments sans traduction, entrées de en.json qui ne servent plus, scripts de la page anglaise compilés
//   node en.js --tout     tous les segments (JSON), pour traduire
// Dans une clé : {1}, {2}… = une balise à attributs ou un ${…} du JS, à reprendre tels quels dans la traduction (ordre libre).
// Une entrée identique à sa clé = « à laisser tel quel ».
const fs = require('fs'), path = require('path'), vm = require('vm');
const INLINE = /^(b|strong|em|i|u|s|small|a|span|br|code|kbd|sup|sub|tspan|wbr)$/i;
const BALISE = /<!--[\s\S]*?-->|<\/?[a-zA-Z](?:"[^"]*"|'[^']*'|[^"'<>])*>/g;
const ATTR = /(\s(?:title|placeholder|alt|aria-label|data-en)=")([^"]*)(")/g;
const META = /^<meta\s[^>]*(?:name="description"|property="og:(?:title|description|image:alt)"|name="twitter:(?:title|description)")/i;
const LETTRES = /[A-Za-zÀ-ÿ]{2}/, ATOME = /(\d+)/g;

// un texte (région HTML, ou contenu d'une chaîne JS) : exprs = les ${…} d'un gabarit, déjà traduits, présents dans src sous forme d'atomes
function texte(src, exprs, tr) {
  const jetons = []; let k = 0, m;
  BALISE.lastIndex = 0;
  while ((m = BALISE.exec(src))) {
    if (m.index > k) jetons.push([src.slice(k, m.index), 0]);
    const nom = (/^<\/?([a-zA-Z0-9-]+)/.exec(m[0]) || [])[1];
    jetons.push([attributs(m[0], tr), nom && INLINE.test(nom) ? 1 : 2]); k = BALISE.lastIndex;   // 0 texte, 1 balise en ligne, 2 coupure
  }
  if (k < src.length) jetons.push([src.slice(k), 0]);
  let out = '', suite = [];
  const vider = () => {
    const brut = suite.map(j => j[0]).join(''), atomes = [];
    const num = a => { atomes.push(a); return '{' + atomes.length + '}'; };
    const cle = suite.map(j => j[1] ? (/\s/.test(j[0]) ? num(j[0]) : j[0]) : j[0].replace(ATOME, num)).join('').replace(/\s+/g, ' ').trim();
    const en = suite.some(j => !j[1] && LETTRES.test(j[0].replace(ATOME, ''))) ? tr(cle) : null;
    out += en == null ? brut : /^\s*/.exec(brut)[0] + en.replace(/\{(\d+)\}/g, (x, n) => atomes[n - 1] || x) + /\s*$/.exec(brut)[0];
    suite = [];
  };
  for (const j of jetons) if (j[1] === 2) { vider(); out += j[0]; } else suite.push(j);
  vider();
  return out.replace(ATOME, (x, n) => exprs[n]);
}
// title, placeholder, alt, aria-label d'une balise (et content des meta de description)
function attributs(balise, tr) {
  const un = (x, a, v, z) => { if (!LETTRES.test(v.replace(ATOME, ''))) return x; const at = []; const en = tr(v.replace(ATOME, s => { at.push(s); return '{' + at.length + '}'; }).replace(/\s+/g, ' ').trim()); return en == null ? x : a + en.replace(/\{(\d+)\}/g, (y, n) => at[n - 1] || y) + z; };
  balise = balise.replace(ATTR, un);
  return META.test(balise) ? balise.replace(/(\scontent=")([^"]*)(")/, un) : balise;
}
// du JS : seules les chaînes '…' "…" `…` passent par texte() ; commentaires et expressions régulières sont recopiés
const AVANT_REGEX = /^(|[(,=:\[!&|?{;+\-*%<>~^]|return|typeof|instanceof|in|of|new|delete|void|throw|case|do|else|yield|await)$/;
function js(src, tr, i = 0, dansExpr = false) {          // -> [source traduite, position de fin]
  let out = '', prec = '', prof = 0;
  while (i < src.length) {
    const c = src[i], d = src[i + 1];
    if (c === '/' && (d === '/' || d === '*')) { let f = d === '/' ? src.indexOf('\n', i) : src.indexOf('*/', i + 2) + 2; if (f < (d === '/' ? 0 : 2)) f = src.length; out += src.slice(i, f); i = f; continue; }
    if (c === "'" || c === '"') { let j = i + 1; while (j < src.length && src[j] !== c) j += src[j] === '\\' ? 2 : 1; out += c + texte(src.slice(i + 1, j), [], tr) + c; i = j + 1; prec = c; continue; }
    if (c === '`') {
      let j = i + 1, contenu = ''; const exprs = [];
      while (j < src.length && src[j] !== '`') {
        if (src[j] === '\\') { contenu += src.slice(j, j + 2); j += 2; }
        else if (src[j] === '$' && src[j + 1] === '{') { const [t, f] = js(src, tr, j + 2, true); contenu += '' + exprs.length + ''; exprs.push('${' + t + '}'); j = f + 1; }
        else contenu += src[j++];
      }
      out += '`' + texte(contenu, exprs, tr) + '`'; i = j + 1; prec = '`'; continue;
    }
    if (c === '/' && AVANT_REGEX.test(prec)) {
      let j = i + 1, classe = false;
      while (j < src.length && (classe || src[j] !== '/') && src[j] !== '\n') { if (src[j] === '\\') j++; else if (src[j] === '[') classe = true; else if (src[j] === ']') classe = false; j++; }
      out += src.slice(i, j + 1); i = j + 1; prec = ')'; continue;
    }
    if (dansExpr) { if (c === '{') prof++; else if (c === '}') { if (!prof) return [out, i]; prof--; } }
    if (/[\w$]/.test(c)) { const mot = /^[\w$]+/.exec(src.slice(i, i + 80))[0]; out += mot; i += mot.length; prec = mot; continue; }
    if (!/\s/.test(c)) prec = c;
    out += c; i++;
  }
  return [out, i];
}
// la page entière : <style> et scripts externes/JSON recopiés, scripts en ligne par js(), le reste par texte()
function page(html, tr) {
  let out = '', k = 0, m; const bloc = /<(script|style)\b([^>]*)>([\s\S]*?)<\/\1>/gi;
  while ((m = bloc.exec(html))) {
    out += texte(html.slice(k, m.index), [], tr);
    const code = m[1].toLowerCase() === 'script' && !/\bsrc=|application\/json/.test(m[2]);
    out += '<' + m[1] + m[2] + '>' + (code ? js(m[3], tr)[0] : m[3]) + '</' + m[1] + '>'; k = bloc.lastIndex;
  }
  return out + texte(html.slice(k), [], tr);
}

let dico = null;
const lireDico = () => dico || (dico = JSON.parse(fs.readFileSync(path.join(__dirname, 'en.json'), 'utf8')));
// la page anglaise : lang="en", le lien de langue de l'en-tête ramène au français,
// et chaque appel à l'API du site porte X-Lang: en (messages du serveur, mail, Stripe et réponses de l'IA en anglais)
const LIEN_FR = '<a id="langue" href="en" title="English version"', LIEN_EN = '<a id="langue" href="./" title="Version française"';
function traduire(html) {
  const d = lireDico();
  return page(html, cle => d[cle])
    .replace('<html lang="fr">', '<html lang="en">').replace('content="fr_FR"', 'content="en_GB"')
    .replace(/(<meta property="og:url" content="[^"]*\/)(">)/, '$1en$2').replace(LIEN_FR, LIEN_EN).replace('>EN</a>', '>FR</a>')
    .replace('<head>', '<head>\n<script>{ const f = fetch; window.fetch = (u, o = {}) => /^https?:/.test(u) ? f(u, o) : f(u, { ...o, headers: { ...o.headers, "X-Lang": "en" } }); }</script>');
}
module.exports = { traduire, page, dico: lireDico };

if (require.main === module) {
  const html = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8'), d = fs.existsSync(path.join(__dirname, 'en.json')) ? lireDico() : (dico = {});
  if (page(html, () => null) !== html) { console.error('ÉCHEC : la page sans traduction ne ressort pas identique (découpage faux)'); process.exit(1); }
  const vus = new Set(); page(html, cle => { vus.add(cle); return d[cle]; });
  if (process.argv[2] === '--tout') return console.log(JSON.stringify([...vus], null, 1));
  // server.js : ses messages d'erreur passent par le même dictionnaire (tr()), cherchés tels qu'ils partent (sans \ devant ')
  const serveur = fs.readFileSync(path.join(__dirname, 'server.js'), 'utf8'), duServeur = new Set();
  js(serveur, cle => { duServeur.add(cle.replace(/\\'/g, "'")); });
  for (const f of fs.readdirSync(path.join(__dirname, 'exemples'))) for (const l of fs.readFileSync(path.join(__dirname, 'exemples', f), 'utf8').split('\n', 2))
    duServeur.add(l.replace(/^# (exemple: )?/, '').split(' | mots:')[0].trim());   // titre et description des jeux de Brique (api/exemples)
  for (const m of serveur.matchAll(/(?:erreur: |\btr\()'((?:\\.|[^'\\])*)'[,) ]/g)) vus.add(m[1].replace(/\\'/g, "'"));
  // à signaler : ce qui ressemble à une phrase française (accent, apostrophe, ou deux mots), pas les bouts de code
  const phrase = s => /[À-ÿ’…«»]|[A-Za-z]{2,} [a-z]{2,} [a-z]{2,}/.test(s) && !/[;{}=]|^\S*$/.test(s.replace(/\{\d+\}/g, ''));
  const manque = [...vus].filter(c => !(c in d) && phrase(c)), inutiles = Object.keys(d).filter(c => !vus.has(c) && !duServeur.has(c));
  manque.forEach(c => console.log('sans traduction : ' + c));
  inutiles.forEach(c => console.log('ne sert plus    : ' + c));
  let ko = 0;
  for (const m of traduire(html).matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)) if (!/\bsrc=|application\/json/.test(m[1]))
    try { new vm.Script(m[2]); } catch (e) { ko++; console.error('ÉCHEC : script de la page anglaise invalide : ' + e.message); }
  console.log(`${vus.size} segments, ${Object.keys(d).length} entrées, ${manque.length} sans traduction, ${inutiles.length} en trop`);
  process.exit(ko ? 1 : 0);
}
