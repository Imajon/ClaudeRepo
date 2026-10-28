// ================================ TIMING ================================
// Mini-jeu (menu "Timing"), repris de matrice-duel/matrice_timing (dalle HUB75 + badge RFID) : le badge devient le bouton SAUT.
// 1) Une CIBLE s'affiche (3,00 a 10,00 s, pas de 0,50 s). SAUT = le chrono part.
// 2) Le chrono est visible TM_VISIBLE_MS puis masque ("?.??"). SAUT quand on pense avoir atteint la cible.
// 3) Resultat : temps reellement fait + ecart (vert / orange / rouge, PARFAIT a 0,05 s pres) + meilleur ecart de la session.
// Solo : SAUT = rejouer (nouvelle cible), DROITE = autre cible (en attente), GAUCHE = retour au menu (en attente / resultat).
// Multijoueur (tm2P, 2 a 4 consoles) : l'hote tire la cible et l'envoie (M_START TIMING_SENTINEL) ; chacun lance et arrete son chrono, envoie son resultat
// (M_TMRES) ; quand tous les joueurs restants ont repondu, le plus petit ecart marque 1 point (ex aequo : chacun le sien) ; tableau J1..J4 sur chaque console ;
// l'hote lance la manche suivante (SAUT) ; GAUCHE = quitter avec le score final (les autres continuent, ou reviennent au menu si c'etait l'hote).
// L'instant de l'appui est pris au front du bouton dans loop() (aucun delai dans cet etat) : precision ~ quelques ms.

#define TM_VISIBLE_MS 1000   // chrono visible au debut, puis masque
#define TM_MIN_CS     300    // cible mini 3,00 s (centiemes)
#define TM_MAX_CS     1000   // cible maxi 10,00 s
#define TM_STEP_CS    50
#define TM_PERFECT_CS 5      // |ecart| <= 0,05 s -> PARFAIT
#define TM_GOOD_CS    25     // |ecart| <= 0,25 s -> vert ; <= 1,00 s -> orange ; au-dela rouge

enum { TM_WAIT, TM_RUN, TM_RES };
int tmPh = TM_WAIT;
uint32_t tmTarget = 500, tmStart = 0, tmRes = 0, tmBest = 99999, tmTries = 0;
bool tmHidden = false, tm2P = false, tmHost = false, tmDecided = false;
uint16_t tmPId[8]; int tmPV[8], tmPN = 0, tmRounds = 0;   // points par identifiant de console (les numeros J1..J4 bougent quand quelqu'un rejoint)

const uint16_t TM_PANEL = RGB565(28, 22, 36), TM_GOLD = RGB565(255, 200, 40), TM_BLUE = RGB565(60, 170, 255);

void tmFmt(char *b, size_t n, uint32_t cs) { if (cs > 9999) cs = 9999; snprintf(b, n, "%lu.%02lu", (unsigned long)(cs / 100), (unsigned long)(cs % 100)); }
void tmNewTarget() { tmTarget = TM_MIN_CS + TM_STEP_CS * random(0, (TM_MAX_CS - TM_MIN_CS) / TM_STEP_CS + 1); }
void tmClear(int y, int h) { tft.fillRect(30, y, SCREEN_W - 60, h, TM_PANEL); }
long tmGap(int32_t r) { return labs((long)r - (long)tmTarget); }
uint16_t tmCol(long ad) { return ad <= TM_GOOD_CS ? RGB565(40, 230, 90) : ad <= TM_GOOD_CS * 4 ? RGB565(255, 150, 20) : RGB565(240, 50, 50); }
int tmMyJ() { return mpRank(mpMe); }
uint32_t timingTarget() { return tmTarget; }              // cible en cours (renvoyee a un joueur qui rejoint)
int tmPtsOf(uint16_t id) { for (int i = 0; i < tmPN; i++) if (tmPId[i] == id) return tmPV[i]; return 0; }
void tmPtsAdd(uint16_t id) { for (int i = 0; i < tmPN; i++) if (tmPId[i] == id) { tmPV[i]++; return; } if (tmPN < 8) { tmPId[tmPN] = id; tmPV[tmPN++] = 1; } }
int tmNPl() { int n = 1; for (int k = 0; k < mpN; k++) if (mpIn[k]) n++; return n; }   // joueurs de la partie (partis compris : les numeros ne bougent pas)
int tmWho(int j) { for (int k = 0; k < mpN; k++) if (mpIn[k] && mpRankK(k) == j) return k; return -1; }   // indice de l'adversaire Jj (-1 = moi ou personne)
uint16_t tmIdOfJ(int j) { if (j == tmMyJ()) return mpMe; const int k = tmWho(j); return k >= 0 ? mpId[k] : 0; }
uint8_t tmColOfJ(int j) { if (j == tmMyJ()) return myCol & 3; const int k = tmWho(j); return k >= 0 ? mpCol[k] & 3 : 0; }   // couleur du joueur Jj
int32_t tmResOf(int j) {                                 // resultat du joueur Jj : centiemes, -1 = en cours, -2 = parti
  if (j == tmMyJ()) return tmPh == TM_RES ? (int32_t)tmRes : -1;
  const int k = tmWho(j); if (k < 0) return -2;
  if (mpTmOk[k]) return mpTm[k]; return mpOn[k] ? -1 : -2;
}

void tmDrawTarget() {
  char t[12], b[24]; tmFmt(t, sizeof t, tmTarget); snprintf(b, sizeof b, "%s s", t);
  tmClear(28, 64);
  tft.setTextSize(2); tft.setTextColor(RGB565(170, 160, 190));
  if (tm2P) { char h[40]; snprintf(h, sizeof h, "CIBLE  (%d joueurs, tu es %s)", tmNPl(), MP_NAME[myCol & 3]); tft.setTextColor(MP_COL[myCol & 3]); ctext(30, h); } else ctext(30, "CIBLE");
  tft.setTextSize(5); tft.setTextColor(TM_GOLD); ctext(52, b);
}
void tmHint(const char *a, const char *b) {
  tmClear(190, 26);
  tft.setTextSize(1); tft.setTextColor(RGB565(150, 150, 175)); ctext(192, a); if (b) ctext(204, b);
}
void tmDrawChrono(uint32_t el) {                         // grand chrono (visible la 1re seconde, puis "?.??")
  char t[12]; tmClear(100, 56); tft.setTextSize(8);
  if (!tmHidden) { tmFmt(t, sizeof t, el / 10); tft.setTextColor(COL_TXT); ctext(104, t); }
  else { tft.setTextColor(TM_BLUE); ctext(104, "?.??"); }
}
void tmSpinner(uint32_t el) {                            // 12 points en anneau, un point qui tourne : le chrono tourne
  static int last = -1; const int k = (el / 80) % 12; if (k == last) return; last = k;
  for (int i = 0; i < 12; i++) {
    float a = i * (2 * PI / 12) - PI / 2; int x = SCREEN_W / 2 + (int)(11 * cosf(a)), y = 172 + (int)(11 * sinf(a));
    tft.fillCircle(x, y, 2, i == k ? COL_TXT : (i == (k + 11) % 12 ? RGB565(120, 120, 150) : RGB565(60, 56, 78)));
  }
}
// score final (affiche par celui qui quitte, et par les autres dans peerLost) ; faux si rien a montrer
bool timingFinalScore(char *b, size_t n) {
  if (state != ST_TIMING || !tm2P || tmRounds == 0) return false;
  int o = snprintf(b, n, "SCORE");
  for (int j = 1; j <= tmNPl() && o < (int)n; j++) o += snprintf(b + o, n - o, "  %s %d", MP_NAME[tmColOfJ(j)], tmPtsOf(tmIdOfJ(j)));
  return true;
}
void tmFinalScreen() {
  char b[48]; if (!timingFinalScore(b, sizeof b)) return;
  int best = 0, nb = 0; for (int j = 1; j <= tmNPl(); j++) { const int v = tmPtsOf(tmIdOfJ(j)); if (v > best) { best = v; nb = 1; } else if (v == best) nb++; }
  const int me = tmPtsOf(mpMe);
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextSize(3); tft.setTextColor(me < best ? RGB565(240, 50, 50) : nb == 1 ? RGB565(40, 230, 90) : TM_GOLD);
  ctext(64, me < best ? "DEFAITE" : nb == 1 ? "VICTOIRE !" : "EGALITE");
  tft.setTextSize(2); tft.setTextColor(COL_TXT); ctext(116, b);
  tft.setTextSize(2); tft.setTextColor(MP_COL[myCol & 3]); char t[24]; snprintf(t, sizeof t, "(tu es %s)", MP_NAME[myCol & 3]); ctext(146, t);
  delay(2500);
}
void tmTable() {                                         // multijoueur : une ligne par joueur J1..J4 : temps, ecart, points
  char t[12], m[12], e[16], p[8];
  tft.setTextSize(1); tft.setTextColor(RGB565(150, 140, 170));
  ltextAt(36, 96, "JOUEUR (* = toi)"); ltextAt(124, 96, "TEMPS"); ltextAt(186, 96, "ECART"); ltextAt(250, 96, "PTS");
  for (int j = 1; j <= tmNPl(); j++) {
    const int y = 108 + (j - 1) * 17; const int32_t r = tmResOf(j); const bool me = j == tmMyJ();
    tft.setTextSize(2); tft.setTextColor(MP_COL[tmColOfJ(j)]); snprintf(e, sizeof e, me ? "%s*" : "%s", MP_NAME[tmColOfJ(j)]); ltextAt(36, y, e);
    if (r == -1) { tft.setTextColor(RGB565(120, 110, 140)); ltextAt(124, y, "en cours..."); }
    else if (r == -2) { tft.setTextColor(RGB565(120, 110, 140)); ltextAt(124, y, "parti"); }
    else {
      const long d = (long)r - (long)tmTarget, ad = labs(d);
      tmFmt(t, sizeof t, (uint32_t)r); tft.setTextColor(COL_TXT); ltextAt(124, y, t);
      tmFmt(m, sizeof m, (uint32_t)ad); snprintf(e, sizeof e, "%c%s", d < 0 ? '-' : '+', m); tft.setTextColor(tmCol(ad)); ltextAt(186, y, e);
    }
    snprintf(p, sizeof p, "%d", tmPtsOf(tmIdOfJ(j))); tft.setTextColor(COL_TXT); ltextAt(256, y, p);
  }
}
void tmScreen() {                                        // ecran complet de la phase en cours
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.fillRoundRect(20, 18, SCREEN_W - 40, SCREEN_H - 36, 12, TM_PANEL); tft.drawRoundRect(20, 18, SCREEN_W - 40, SCREEN_H - 36, 12, COL_MEAT);
  tmDrawTarget();
  if (tmPh == TM_WAIT) {
    tft.setTextSize(3); tft.setTextColor(RGB565(200, 200, 220)); ctext(118, "PRET ?");
    if (tm2P) tmHint("SAUT = lancer le chrono", "meme cible pour tous   GAUCHE = quitter");
    else tmHint("SAUT = lancer le chrono", "DROITE = autre cible   GAUCHE = menu");
  } else if (tmPh == TM_RUN) {
    tmDrawChrono(0); tmHint("SAUT = stop", NULL);
  } else {
    const long d = (long)tmRes - (long)tmTarget, ad = labs(d); char t[12], e[16], m[12], b[48];
    tmClear(96, 94);
    if (!tm2P) {
      const uint16_t c = tmCol(ad);
      tmFmt(t, sizeof t, tmRes); tft.setTextSize(8); tft.setTextColor(COL_TXT); ctext(98, t);            // temps reellement fait
      tmFmt(m, sizeof m, (uint32_t)ad); snprintf(e, sizeof e, "%c%s s", d < 0 ? '-' : '+', m);
      tft.setTextSize(3); tft.setTextColor(c); ctext(146, e);                                              // ecart
      tft.setTextSize(2); ctext(172, ad <= TM_PERFECT_CS ? "PARFAIT !" : d < 0 ? "TROP TOT" : "TROP TARD");
      tmFmt(m, sizeof m, tmBest); snprintf(b, sizeof b, "Meilleur ecart %s s  (%lu essais)", m, (unsigned long)tmTries);
      tmHint(b, "SAUT = rejouer   GAUCHE = menu");
    } else {
      tmTable();
      if (tmDecided) {                                   // verdict de la manche pour moi
        long best = 99999; int nb = 0;
        for (int j = 1; j <= tmNPl(); j++) { const int32_t r = tmResOf(j); if (r < 0) continue; const long g = tmGap(r); if (g < best) { best = g; nb = 1; } else if (g == best) nb++; }
        tft.setTextSize(2); tft.setTextColor(ad > best ? RGB565(240, 50, 50) : nb == 1 ? RGB565(40, 230, 90) : TM_GOLD);
        ctext(176, ad > best ? "PERDU" : nb == 1 ? "MANCHE GAGNEE !" : "EGALITE");
      } else { tft.setTextSize(1); tft.setTextColor(RGB565(150, 140, 170)); ctext(180, "attente des autres joueurs..."); }
      tmHint(tmHost ? "SAUT = manche suivante" : "l'hote lance la manche suivante", "GAUCHE = quitter (score final)");
    }
  }
}
void tmRound(uint16_t target) { tmTarget = target; tmPh = TM_WAIT; tmDecided = false; for (int k = 0; k < MP_MAX; k++) mpTmOk[k] = false; tmScreen(); }
void startTiming() { tm2P = false; tmPh = TM_WAIT; tmNewTarget(); state = ST_TIMING; tmScreen(); }
void startTiming2P(bool host, int16_t target) { tm2P = true; tmHost = host; tmPN = 0; tmRounds = 0; vStart = false; state = ST_TIMING; tmRound(target); }

bool timingStep(bool bl, bool br, bool bj, bool je) {   // false = retour au menu
  const uint32_t now = millis(); (void)bj;
  static bool pl = true, pr = true; const bool eL = bl && !pl, eR = br && !pr; pl = bl; pr = br;
  if (tm2P) {
    if (!tmHost && vStart && vStartLvl == TIMING_SENTINEL) { vStart = false; tmRound(vStartX); return true; }   // l'hote lance une manche
    static uint32_t seen = 0, gone = 0; uint32_t sNow = 0, gNow = 0;   // resultats recus / joueurs partis depuis le dernier dessin -> redessiner
    for (int k = 0; k < mpN; k++) if (mpIn[k]) { if (mpTmOk[k]) sNow |= 1 << k; if (!mpOn[k]) gNow |= 1 << k; }
    if (tmPh == TM_RES && !tmDecided) {                  // tous les joueurs restants ont repondu : on compte la manche une seule fois
      bool all = true; for (int k = 0; k < mpN; k++) if (mpIn[k] && mpOn[k] && !mpTmOk[k]) all = false;
      if (all) {
        tmDecided = true; tmRounds++; long best = 99999;
        for (int j = 1; j <= tmNPl(); j++) { const int32_t r = tmResOf(j); if (r >= 0 && tmGap(r) < best) best = tmGap(r); }
        for (int j = 1; j <= tmNPl(); j++) { const int32_t r = tmResOf(j); if (r >= 0 && tmGap(r) == best) tmPtsAdd(tmIdOfJ(j)); }
        tmScreen(); seen = sNow; gone = gNow;
      }
    }
    if (tmPh == TM_RES && (sNow != seen || gNow != gone)) { seen = sNow; gone = gNow; tmScreen(); }
#ifdef LOBBY_BENCH
    { static uint32_t pt = 0; if (now - pt > 1000) { pt = now; char b[48]; timingFinalScore(b, sizeof b);
      Serial.printf("[T2] host=%d J%d/%d ph=%d cible=%lu moi=%lu decide=%d manches=%d %s\n", tmHost ? 1 : 0, tmMyJ(), tmNPl(), tmPh, (unsigned long)tmTarget, (unsigned long)tmRes, tmDecided ? 1 : 0, tmRounds, tmRounds ? b : ""); } }
#endif
  }
  if (tmPh == TM_WAIT) {
    if (eL) { if (tm2P) { for (int k = 0; k < 3; k++) espSend(M_PQUIT, 0, 0, 0, 0); tmFinalScreen(); } return false; }
    if (eR && !tm2P) { tmNewTarget(); tmDrawTarget(); }
    if (je) { tmStart = now; tmHidden = false; tmPh = TM_RUN; tmScreen(); }
  } else if (tmPh == TM_RUN) {
    const uint32_t el = now - tmStart;
    if (je) {                                            // arret : temps arrondi au centieme
      tmRes = (el + 5) / 10; tmTries++;
      const uint32_t ad = (uint32_t)tmGap(tmRes); if (ad < tmBest) tmBest = ad;
      if (tm2P) for (int k = 0; k < 2; k++) espSend(M_TMRES, 0, (int16_t)tmRes, 0, 0);
      snd(ad <= TM_PERFECT_CS ? SND_STAR : ad <= TM_GOOD_CS ? SND_COIN : SND_DIE);
      tmPh = TM_RES; tmScreen(); return true;
    }
    static uint32_t lastDraw = 0;
    if (!tmHidden && el >= TM_VISIBLE_MS) { tmHidden = true; tmDrawChrono(el); }
    else if (!tmHidden && now - lastDraw >= 30) { lastDraw = now; tmDrawChrono(el); }
    tmSpinner(el);
  } else {
    if (eL) { if (tm2P) { for (int k = 0; k < 3; k++) espSend(M_PQUIT, 0, 0, 0, 0); tmFinalScreen(); } return false; }
    if (je && (!tm2P || tmHost)) {
      tmNewTarget();
      if (tm2P) for (int k = 0; k < 4; k++) { espSend(M_START, TIMING_SENTINEL, (int16_t)tmTarget, 0, 0); delay(5); }
      tmRound(tmTarget);
    }
  }
  return true;
}
