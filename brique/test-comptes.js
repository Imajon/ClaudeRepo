// Test des comptes : node server.js (PORT=8099 DATA=fichier-jetable) puis node test-comptes.js [journal-du-serveur]
// e-mail + mot de passe (lien de confirmation lu dans le journal du serveur), animateur, codes d'enfants, mot de passe oublié.
const fs = require('fs'), assert = require('assert');
const B = 'http://localhost:' + (process.env.PORT || 8099), JOURNAL = process.argv[2] || process.env.TEMP + '/srv.log';
const api = async (u, corps, session) => { const r = await fetch(B + u, { method: 'POST', headers: { 'Content-Type': 'application/json', ...(session && { 'X-Session': session }) }, body: JSON.stringify(corps) }); return { st: r.status, ...(await r.json()) }; };
const lien = param => { const m = [...fs.readFileSync(JOURNAL, 'utf8').matchAll(new RegExp('\\?' + param + '=([0-9a-f]{48})', 'g'))]; return m.at(-1)[1]; };
(async () => {
  const mail = 'anim' + Date.now() + '@test.fr';
  assert.equal((await api('/api/inscription', { email: 'pasunmail', pseudo: 'Prof', mdp: 'secret1' })).st, 400);
  assert.equal((await api('/api/inscription', { email: mail, pseudo: 'Prof' + (Date.now() % 1000), mdp: 'court' })).st, 400, 'mot de passe trop court');
  const pseudo = 'Prof' + (Date.now() % 1000);
  assert.equal((await api('/api/inscription', { email: mail, pseudo, mdp: 'secret1' })).st, 200);
  assert.equal((await api('/api/reconnexion', { identifiant: mail, mdp: 'secret1' })).st, 401, 'pas de compte avant confirmation');
  const r = await api('/api/confirmer', { jeton: lien('confirmer') });
  assert.equal(r.st, 200); assert.equal(r.pseudo, pseudo); assert.ok(r.jeton && r.session, 'session joueur + animateur');
  assert.equal((await api('/api/confirmer', { jeton: lien('confirmer') })).st, 400, 'lien à usage unique');
  // connexion e-mail + mot de passe, puis espace animateur avec le même compte
  const c = await api('/api/reconnexion', { identifiant: mail.toUpperCase(), mdp: 'secret1' });
  assert.equal(c.st, 200); assert.ok(c.session);
  assert.equal((await api('/api/reconnexion', { identifiant: mail, mdp: 'faux!!' })).st, 401);
  let esp = await api('/api/animateur/groupe', { nom: 'CM2' }, c.session);
  const g = esp.groupes[0]; assert.equal(esp.email, mail);
  // codes d'enfants
  esp = await api('/api/animateur/codes', { code: g.code, n: 3 }, c.session);
  const codes = esp.groupes[0].codes; assert.equal(codes.length, 3); assert.ok(codes.every(x => !x.pseudo));
  assert.equal((await api('/api/code', { code: 'XXXX-0000' })).st, 401);
  assert.equal((await api('/api/code', { code: codes[0].c.toLowerCase() })).pseudoRequis, true, 'première fois : demande le pseudo');
  const e1 = await api('/api/code', { code: codes[0].c, pseudo: 'Kid' + (Date.now() % 1000) });
  assert.equal(e1.st, 200); assert.equal(e1.groupe, 'CM2'); assert.ok(!e1.session, 'pas de droits animateur');
  const e2 = await api('/api/code', { code: codes[0].c });                       // le code seul reconnecte
  assert.equal(e2.st, 200); assert.equal(e2.pseudo, e1.pseudo);
  assert.equal((await api('/api/code', { code: codes[1].c, pseudo: e1.pseudo })).st, 409, 'pseudo pris');
  assert.equal((await api('/api/animateur/groupe', { nom: 'x' }, e1.jeton)).st, 401, 'un enfant n\'est pas animateur');
  esp = await api('/api/animateur/codesuppr', { code: g.code, c: codes[0].c }, c.session);
  assert.equal(esp.groupes[0].codes.length, 2);
  assert.equal((await api('/api/code', { code: codes[0].c })).st, 401, 'code supprimé');
  // mot de passe oublié
  await new Promise(r => setTimeout(r, 61000));   // 1 lien par minute et par adresse
  assert.equal((await api('/api/oubli', { email: mail })).st, 200);
  assert.equal((await api('/api/confirmer', { jeton: lien('reinit'), mdp: '1' })).st, 400, 'mot de passe trop court, lien gardé');
  const n = await api('/api/confirmer', { jeton: lien('reinit'), mdp: 'nouveau2' }); assert.equal(n.st, 200);
  assert.equal((await api('/api/reconnexion', { identifiant: mail, mdp: 'secret1' })).st, 401);
  assert.equal((await api('/api/reconnexion', { identifiant: mail, mdp: 'nouveau2' })).st, 200);
  console.log('comptes : tout passe');
})().catch(e => { console.error('ÉCHEC', e.message); process.exit(1); });
