// Brique - serveur V1 : sert la page, garde les niveaux signes (pseudo), les parties jouees et en tire classement + stats.
// Aucune dependance : `node server.js` (port 8080, ou PORT=...). Donnees dans data.json a cote.
// Aucun compte, aucune donnee perso : un pseudo libre (2-12 caracteres, pas d'espace -> pas de "Prenom Nom").
const http = require('http'), fs = require('fs'), path = require('path');
const PORT = process.env.PORT || 8080, FICHIER = path.join(__dirname, 'data.json');
let db = fs.existsSync(FICHIER) ? JSON.parse(fs.readFileSync(FICHIER, 'utf8')) : { niveaux: [], parties: [] };
const sauver = () => { fs.writeFileSync(FICHIER + '.tmp', JSON.stringify(db)); fs.renameSync(FICHIER + '.tmp', FICHIER); };

// ---------- validation (meme regles que briqueParse() du firmware) ----------
const PSEUDO = /^[A-Za-z0-9_-]{2,12}$/;
function nivValide(n) {
  if (!n || !Array.isArray(n.rows) || n.rows.length < 3 || n.rows.length > 80) return false;
  if (!n.rows.every(r => typeof r === 'string' && /^[.#SG*R^WCI]{0,20}$/.test(r))) return false;
  const tout = n.rows.join('');
  return tout.split('S').length === 2 && tout.split('G').length === 2;
}
const ICONE = /^([0-9a-f]{3}|---){256}$/;               // 16x16, meme format que la page et le firmware
const icValides = ic => Object.fromEntries(['h', 'e', 'g'].filter(k => ICONE.test(ic?.[k] || '')).map(k => [k, ic[k]]));
const entier =(v, max) => Number.isInteger(v) && v >= 0 && v <= max;

// ---------- stats d'un niveau ----------
function stats(id) {
  const ps = db.parties.filter(p => p.niveau === id);
  if (!ps.length) return { parties: 0 };
  const rec = ps.reduce((a, b) => b.t < a.t ? b : a), parJoueur = {};
  for (const p of ps) parJoueur[p.pseudo] = (parJoueur[p.pseudo] || 0) + 1;
  const champion = Object.entries(parJoueur).sort((a, b) => b[1] - a[1])[0];
  return {
    parties: ps.length,                                   // nombre de fois fini
    joueurs: Object.keys(parJoueur).length,
    record: { t: rec.t, pseudo: rec.pseudo },             // le plus rapide
    champion: { pseudo: champion[0], fois: champion[1] }, // celui qui l'a fini le plus de fois
    mortsMoy: +(ps.reduce((s, p) => s + p.morts, 0) / ps.length).toFixed(1),
  };
}

// ---------- limite de requetes : 30 ecritures / minute / IP ----------
// ponytail: compteur en memoire, remis a zero par minute ; suffit pour un atelier, un vrai limiteur si le serveur est public a grande echelle
const ecritures = new Map(); setInterval(() => ecritures.clear(), 60000);
const tropDeRequetes = ip => { const n = (ecritures.get(ip) || 0) + 1; ecritures.set(ip, n); return n > 30; };

const repondre = (res, code, obj) => { res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8' }); res.end(JSON.stringify(obj)); };
function lireJSON(req) {
  return new Promise((ok, ko) => {
    let s = ''; req.on('data', c => { s += c; if (s.length > 12288) { ko(new Error('trop gros')); req.destroy(); } });
    req.on('end', () => { try { ok(JSON.parse(s)); } catch (e) { ko(e); } });
  });
}

http.createServer(async (req, res) => {
  const url = req.url.split('?')[0];
  if (req.method === 'GET' && (url === '/' || url === '/index.html')) {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); return fs.createReadStream(path.join(__dirname, 'index.html')).pipe(res);
  }
  if (req.method === 'GET' && url === '/api/niveaux')
    return repondre(res, 200, db.niveaux.map(n => ({ ...n, stats: stats(n.id) })));

  if (req.method !== 'POST' || !url.startsWith('/api/')) return repondre(res, 404, { erreur: 'introuvable' });
  if (tropDeRequetes(req.socket.remoteAddress)) return repondre(res, 429, { erreur: 'Doucement ! Réessaie dans une minute.' });
  let b; try { b = await lireJSON(req); } catch { return repondre(res, 400, { erreur: 'requête illisible' }); }
  if (!PSEUDO.test(b.pseudo || '')) return repondre(res, 400, { erreur: 'Pseudo : 2 à 12 lettres ou chiffres, sans espace.' });

  if (url === '/api/niveaux') {                       // publier un jeu (commun=false) ou ajouter un niveau au jeu commun (commun=true)
    if (!nivValide(b.niv)) return repondre(res, 400, { erreur: 'niveau invalide' });
    const titre = String(b.titre || '').replace(/[<>]/g, '').trim().slice(0, 30) || 'Sans titre';
    const n = { id: Date.now().toString(36) + Math.random().toString(36).slice(2, 5), pseudo: b.pseudo, titre, commun: !!b.commun,
                cree: Date.now(), niv: { format: 'brique-v1', params: b.niv.params || {}, rows: b.niv.rows, icones: icValides(b.niv.icones) } };
    if (JSON.stringify(n.niv.params).length > 1000) n.niv.params = {};
    db.niveaux.push(n); sauver(); return repondre(res, 200, { id: n.id });
  }
  if (url === '/api/parties') {                       // un niveau fini (console ou aperçu web)
    if (!db.niveaux.some(n => n.id === b.niveau)) return repondre(res, 400, { erreur: 'niveau inconnu' });
    if (!entier(b.t, 3600000) || !entier(b.morts, 9999) || !entier(b.etoiles, 50)) return repondre(res, 400, { erreur: 'stats invalides' });
    db.parties.push({ niveau: b.niveau, pseudo: b.pseudo, t: b.t, morts: b.morts, etoiles: b.etoiles,
                      source: b.source === 'console' ? 'console' : 'web', date: Date.now() });
    sauver(); return repondre(res, 200, { ok: true, stats: stats(b.niveau) });
  }
  repondre(res, 404, { erreur: 'introuvable' });
}).listen(PORT, () => console.log(`Brique sur http://localhost:${PORT}`));
