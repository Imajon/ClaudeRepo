// Test de la page Mon compte : jeux publiés, chat avec le support, mot de passe, suppression.
// Serveur jetable : ADMIN_EMAILS=admin@test.fr PORT=8099 DATA=%TEMP%/t.json node server.js > %TEMP%/srv.log ; puis node test-compte-page.js [journal]
const fs = require('fs'), assert = require('assert');
const B = 'http://localhost:' + (process.env.PORT || 8099), JOURNAL = process.argv[2] || process.env.TEMP + '/srv.log';
const api = async (u, corps, session) => { const r = await fetch(B + u, { method: 'POST', headers: { 'Content-Type': 'application/json', ...(session && { 'X-Session': session }) }, body: JSON.stringify(corps) }); return { st: r.status, ...(await r.json()) }; };
const lien = param => [...fs.readFileSync(JOURNAL, 'utf8').matchAll(new RegExp('\\?' + param + '=([0-9a-f]{48})', 'g'))].at(-1)[1];
const compte = async (email, pseudo) => { await api('/api/inscription', { email, pseudo, mdp: 'secret1' }); return api('/api/confirmer', { jeton: lien('confirmer') }); };
(async () => {
  const t = Date.now() % 100000, admin = await compte('admin@test.fr', 'Admin' + t), kid = await compte('kid' + t + '@test.fr', 'Kid' + t);
  const id = { pseudo: kid.pseudo, jeton: kid.jeton };
  assert.equal((await api('/api/compte', { pseudo: kid.pseudo, jeton: 'faux' })).st, 401);
  const c = await api('/api/compte', id); assert.equal(c.pseudo, kid.pseudo); assert.equal(c.email, 'kid' + t + '@test.fr'); assert.equal(c.mdp, true);
  // jeux publiés : liste, puis rouvrir
  assert.equal((await api('/api/mesjeux', id)).jeux.length, 0);
  const p = await api('/api/niveaux', { ...id, titre: 'Mon jeu', script: 'print("salut")' }); assert.equal(p.st, 200);
  const l = await api('/api/mesjeux', id); assert.equal(l.jeux.length, 1); assert.equal(l.jeux[0].rouvrable, true);
  const j = await api('/api/mesjeux', { ...id, id: l.jeux[0].id }); assert.equal(j.niv.script, 'print("salut")');
  const autre = await compte('autre' + t + '@test.fr', 'Autre' + t);
  assert.equal((await api('/api/mesjeux', { pseudo: autre.pseudo, jeton: autre.jeton, id: l.jeux[0].id })).st, 404, 'pas le jeu d\'un autre');
  // chat : message, réponse de l'admin, non lu, lu
  assert.equal((await api('/api/support', id)).messages.length, 0);
  const m = await api('/api/support', { ...id, texte: 'Bonjour <b>équipe</b>' }); assert.equal(m.messages.length, 1); assert.ok(!m.messages[0].texte.includes('<'));
  const rep = await api('/api/animateur/supportrep', { pseudo: kid.pseudo, texte: 'Salut !' }, admin.session);
  assert.equal(rep.st, 200); assert.equal(rep.support[0].aRepondre, false);
  assert.equal((await api('/api/compte', id)).nonLus, 1);
  assert.equal((await api('/api/support', { ...id, ouvert: true })).nonLus, 0, 'ouvrir le chat = lu');
  assert.equal((await api('/api/animateur/supportrep', { pseudo: kid.pseudo, texte: 'pirate' }, kid.session)).st, 404, 'seul l\'admin répond');
  for (let i = 0; i < 5; i++) await api('/api/support', { ...id, texte: 'encore ' + i });
  assert.equal((await api('/api/support', { ...id, texte: 'trop' })).st, 429, '5 messages sans réponse : stop');
  // mot de passe
  assert.equal((await api('/api/compte/mdp', { ...id, ancien: 'faux!!', nouveau: 'nouveau2' })).st, 401);
  assert.equal((await api('/api/compte/mdp', { ...id, ancien: 'secret1', nouveau: '1' })).st, 400);
  const n = await api('/api/compte/mdp', { ...id, ancien: 'secret1', nouveau: 'nouveau2' }); assert.equal(n.st, 200); assert.ok(n.jeton && n.jeton !== kid.jeton);
  assert.equal((await api('/api/compte', id)).st, 401, 'ancienne session fermée');
  assert.equal((await api('/api/reconnexion', { identifiant: 'kid' + t + '@test.fr', mdp: 'nouveau2' })).st, 200);
  // un compte avec un groupe ne se supprime pas seul ; les autres, si
  await api('/api/animateur/groupe', { nom: 'G' }, autre.session);
  assert.equal((await api('/api/compte/supprimer', { pseudo: autre.pseudo, jeton: autre.jeton })).st, 400);
  assert.equal((await api('/api/compte/supprimer', { pseudo: kid.pseudo, jeton: n.jeton })).st, 200);
  assert.equal((await api('/api/reconnexion', { identifiant: 'kid' + t + '@test.fr', mdp: 'nouveau2' })).st, 401, 'compte supprimé');
  assert.equal((await api('/api/animateur/supportrep', { pseudo: kid.pseudo, texte: 'x' }, admin.session)).st, 400, 'fil supprimé');
  console.log('page compte : tout passe');
})().catch(e => { console.error('ÉCHEC', e.message); process.exit(1); });
