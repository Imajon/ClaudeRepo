// ================================ DRIFT ================================
// Mini-jeu solo (menu "Drift") : on conduit un pointeur de souris dans un circuit dont les bords sont des traits, du depart a l'arrivee, au chrono.
// SAUT (gros bouton) tenu = accelerer progressivement, relache = la vitesse retombe doucement ; GAUCHE / DROITE = tourner.
// Derapage : tourner change le cap, pas la vitesse ; la vitesse laterale ne s'eteint qu'avec l'adherence, qui baisse avec la vitesse -> le pointeur glisse
// dans les virages pris vite (traces de derapage au sol). Toucher un bord : rebond amorti, perte de vitesse, etincelles.
// Choix du circuit : GAUCHE / DROITE, SAUT = partir ; a l'arrivee SAUT = rejouer, GAUCHE / DROITE = autre circuit. GAUCHE + DROITE tenus 1 s = quitter.
// Rendu : double tampon DMA d'Alto (alDmaBegin / alSendHalf), bandes de 60 lignes. Records en RAM.
// Multijoueur (dr2P, 2 a 4 consoles) : l'hote choisit le circuit (M_DSEL, renvoye toutes les 0,5 s) et donne le depart (M_DSEL x = 1) ; chacun envoie sa position et son cap
// toutes les 40 ms (M_POS) -> pointeur orange de l'adversaire (fantome, pas de collision) ; a l'arrivee M_FIN (temps en ms) -> GAGNE / PERDU.
// L'hote relance (SAUT) ou change de circuit (G / D) quand l'adversaire est arrive (ou 8 s apres sa propre arrivee). G + D 1 s = quitter (l'autre revient au menu).

#define DR_SUB   6                // points par troncon de la ligne centrale (lissage Catmull-Rom)
#define DR_MAXP  200
#define DR_SK    160              // points de traces de derapage
#define DR_NP    24
#define DR_ACC   330.0f           // acceleration bouton tenu (px/s2)
#define DR_VMAX  290.0f
#define DR_FRIC  150.0f           // ralentissement bouton relache
#define DR_R     4.0f             // rayon de collision du pointeur

struct DrPt { float x, y, vx, vy, life; };
struct DrRun {
  int n; float w; float cx[DR_MAXP], cy[DR_MAXP];        // ligne centrale lissee
  int16_t lx[DR_MAXP], ly[DR_MAXP], rx[DR_MAXP], ry[DR_MAXP];   // bords gauche / droit
  int16_t sax[DR_SK], say[DR_SK], sbx[DR_SK], sby[DR_SK]; uint8_t sbrk[DR_SK]; int skHead, skN;
  DrPt p[DR_NP]; int pi;
  float trX[8], trY[8], trA[8];                           // trainee du pointeur (souris "a trainees")
};
struct DrTrack { const char *name; int16_t w; uint8_t col[3]; uint8_t n; int16_t pts[26][2]; };
static const DrTrack DR_TR[3] = {
  { "RUBAN", 72, { 80, 230, 255 }, 16,
    { { 75, 700 }, { 325, 700 }, { 525, 675 }, { 650, 575 }, { 650, 425 }, { 525, 338 }, { 325, 328 }, { 200, 245 }, { 212, 120 }, { 412, 78 }, { 650, 100 }, { 850, 162 }, { 950, 300 }, { 950, 475 }, { 988, 650 }, { 1150, 725 } } },
  { "EPINGLES", 64, { 255, 90, 200 }, 22,
    { { 81, 891 }, { 81, 270 }, { 113, 151 }, { 176, 108 }, { 238, 151 }, { 270, 270 }, { 270, 756 }, { 302, 875 }, { 364, 918 }, { 427, 875 }, { 459, 756 }, { 459, 270 }, { 491, 151 }, { 554, 108 }, { 616, 151 }, { 648, 270 }, { 648, 756 }, { 680, 875 }, { 742, 918 }, { 805, 875 }, { 837, 756 }, { 837, 135 } } },
  { "SPIRALE", 58, { 255, 210, 60 }, 25,
    { { 112, 84 }, { 588, 84 }, { 1008, 98 }, { 1106, 210 }, { 1120, 462 }, { 1092, 784 }, { 980, 882 }, { 812, 896 }, { 658, 860 }, { 504, 896 }, { 210, 882 }, { 112, 784 }, { 112, 420 }, { 182, 280 }, { 504, 266 }, { 854, 280 }, { 938, 378 }, { 931, 616 }, { 882, 707 }, { 784, 728 }, { 364, 721 }, { 287, 630 }, { 301, 511 }, { 392, 455 }, { 616, 462 } } },
};

DrRun *drR = nullptr;
int drTk = 0, drPh = 0, drHits = 0, drPi = 0, drFps = 0;      // phase : 0 choix, 1 compte a rebours, 2 course, 3 arrivee
float drX, drY, drVX, drVY, drH, drCamX, drCamY, drT, drQuit, drShake, drCd;
float drBest[3] = { 0, 0, 0 };
bool drPL, drPR, drJLock, drSkOn;
bool dr2P = false, drFinSent = false;                    // a deux
uint32_t drFinMs = 0;
int drNext = 0;                                           // circuit de la prochaine course, choisi a l'arrivee (G / D)                                     // instant de mon arrivee (les adversaires : mpX/mpY/mpH/mpFin du multijoueur)

uint16_t drC(const uint8_t *c, int k) { return RGB565(c[0] * k / 16, c[1] * k / 16, c[2] * k / 16); }   // couleur du circuit, intensite k/16

void drBuild() {                                         // ligne centrale lissee (Catmull-Rom) + bords decales de w/2
  const DrTrack &T = DR_TR[drTk]; DrRun &R = *drR; R.n = 0; R.w = T.w;
  for (int i = 0; i < T.n - 1; i++) {
    const int i0 = max(0, i - 1), i2 = i + 1, i3 = min(T.n - 1, i + 2);
    for (int s = 0; s < DR_SUB && R.n < DR_MAXP; s++) {
      const float t = s / (float)DR_SUB, t2 = t * t, t3 = t2 * t;
      const float b0 = -0.5f * t3 + t2 - 0.5f * t, b1 = 1.5f * t3 - 2.5f * t2 + 1, b2 = -1.5f * t3 + 2 * t2 + 0.5f * t, b3 = 0.5f * t3 - 0.5f * t2;
      R.cx[R.n] = b0 * T.pts[i0][0] + b1 * T.pts[i][0] + b2 * T.pts[i2][0] + b3 * T.pts[i3][0];
      R.cy[R.n] = b0 * T.pts[i0][1] + b1 * T.pts[i][1] + b2 * T.pts[i2][1] + b3 * T.pts[i3][1]; R.n++;
    }
  }
  if (R.n < DR_MAXP) { R.cx[R.n] = T.pts[T.n - 1][0]; R.cy[R.n] = T.pts[T.n - 1][1]; R.n++; }
  for (int k = 0; k < R.n; k++) {
    const int a = max(0, k - 1), b = min(R.n - 1, k + 1);
    float tx = R.cx[b] - R.cx[a], ty = R.cy[b] - R.cy[a], l = sqrtf(tx * tx + ty * ty); if (l < 0.01f) l = 1;
    const float nx = -ty / l * R.w * 0.5f, ny = tx / l * R.w * 0.5f;
    R.lx[k] = (int16_t)(R.cx[k] + nx); R.ly[k] = (int16_t)(R.cy[k] + ny); R.rx[k] = (int16_t)(R.cx[k] - nx); R.ry[k] = (int16_t)(R.cy[k] - ny);
  }
}
void drReset() {                                         // pointeur sur la ligne de depart, cap le long du circuit
  DrRun &R = *drR; drFinSent = false; drNext = drTk; vFin = false; vPos = false; for (int k = 0; k < MP_MAX; k++) { mpFin[k] = false; mpPos[k] = false; }
  drX = R.cx[1]; drY = R.cy[1]; drVX = drVY = 0; drH = atan2f(R.cy[3] - R.cy[0], R.cx[3] - R.cx[0]);
  drCamX = drX; drCamY = drY; drT = 0; drHits = 0; drPi = 1; drShake = 0;
  R.skHead = R.skN = 0; drSkOn = false; R.pi = 0; for (int i = 0; i < DR_NP; i++) R.p[i].life = 0;
  for (int i = 0; i < 8; i++) { R.trX[i] = drX; R.trY[i] = drY; R.trA[i] = drH; }
}
void drSpark(float x, float y, int n, float sp) {
  for (int i = 0; i < n; i++) { DrPt &q = drR->p[drR->pi]; drR->pi = (drR->pi + 1) % DR_NP; float a = random(0, 628) / 100.0f, v = sp * random(30, 100) / 100;
    q.x = x; q.y = y; q.vx = alCos(a) * v; q.vy = alSin(a) * v; q.life = 0.25f + random(0, 25) / 100.0f; }
}
// collision cercle / segment : repousse le pointeur hors du trait, rebond amorti
void drWall(float ax, float ay, float bx, float by) {
  const float ex = bx - ax, ey = by - ay, l2 = ex * ex + ey * ey; if (l2 < 0.01f) return;
  float t = ((drX - ax) * ex + (drY - ay) * ey) / l2; t = constrain(t, 0.0f, 1.0f);
  const float qx = ax + ex * t, qy = ay + ey * t, dx = drX - qx, dy = drY - qy, d2 = dx * dx + dy * dy;
  if (d2 >= DR_R * DR_R) return;
  float d = sqrtf(d2), nx, ny; if (d < 0.01f) { nx = -ey; ny = ex; d = sqrtf(l2); nx /= d; ny /= d; d = 0; } else { nx = dx / d; ny = dy / d; }
  drX += nx * (DR_R - d); drY += ny * (DR_R - d);
  const float vn = drVX * nx + drVY * ny;
  if (vn < 0) {
    drVX -= 1.35f * vn * nx; drVY -= 1.35f * vn * ny; drVX *= 0.86f; drVY *= 0.86f;   // rebond : 35 % de la vitesse d'impact, frottement
    if (vn < -50) { drHits++; drShake = min(4.0f, -vn / 60); drSpark(qx, qy, 6, 90 - vn * 0.3f); if (vn < -110) snd(SND_CLICK); }
  }
}
// filet de securite : le pointeur reste toujours a moins de (largeur / 2 - rayon) de la ligne centrale ; s'il en sort (trait saute, angle de deux
// segments), il est ramene dedans et sa vitesse vers l'exterieur est renvoyee vers l'interieur -> impossible de traverser un bord
void drInside() {
  DrRun &R = *drR; float bd = 1e18f, qx = 0, qy = 0;
  for (int i = max(0, drPi - 8); i <= min(R.n - 2, drPi + 10); i++) {
    const float ax = R.cx[i], ay = R.cy[i], ex = R.cx[i + 1] - ax, ey = R.cy[i + 1] - ay, l2 = ex * ex + ey * ey; if (l2 < 0.01f) continue;
    float t = ((drX - ax) * ex + (drY - ay) * ey) / l2; t = constrain(t, 0.0f, 1.0f);
    const float px = ax + ex * t, py = ay + ey * t, d = (drX - px) * (drX - px) + (drY - py) * (drY - py);
    if (d < bd) { bd = d; qx = px; qy = py; }
  }
  const float lim = R.w * 0.5f - DR_R * 0.5f, d = sqrtf(bd);
  if (d <= lim || d < 0.01f) return;
  const float nx = (drX - qx) / d, ny = (drY - qy) / d;
  drX = qx + nx * (lim - 0.5f); drY = qy + ny * (lim - 0.5f);
  const float vn = drVX * nx + drVY * ny; if (vn > 0) { drVX -= 1.35f * vn * nx; drVY -= 1.35f * vn * ny; drVX *= 0.86f; drVY *= 0.86f; }
}
void drPhys(float dt, bool bl, bool br, bool bj) {
  DrRun &R = *drR;
  const float sp = sqrtf(drVX * drVX + drVY * drVY), k = min(1.0f, sp / DR_VMAX);
  const int dir = (br ? 1 : 0) - (bl ? 1 : 0);
  drH += dir * (3.9f - 1.3f * k) * dt;                   // on tourne un peu moins vite a pleine vitesse
  const float hx = alCos(drH), hy = alSin(drH);
  float vl = drVX * hx + drVY * hy, vt = -drVX * hy + drVY * hx;   // vitesse le long du cap / laterale
  if (bj) vl += DR_ACC * (1 - 0.55f * k) * dt;           // acceleration progressive (de plus en plus douce vers la vitesse max)
  else vl -= min(fabsf(vl), DR_FRIC * dt) * (vl > 0 ? 1 : -1);
  vl = constrain(vl, -30.0f, DR_VMAX);
  vt *= max(0.0f, 1 - (9.0f - 6.6f * k) * dt);           // adherence : forte a basse vitesse, faible a haute -> derapage
  drVX = hx * vl - hy * vt; drVY = hy * vl + hx * vt;
  // deplacement en sous-pas de 2 px au plus : a pleine vitesse un pas d'image (~12 px a 25 img/s) depasse le rayon du pointeur et laissait sauter un trait
  const float mv = sqrtf(drVX * drVX + drVY * drVY) * dt; const int ns = min(16, (int)(mv / 2.0f) + 1); const float sdt = dt / ns;
  for (int st = 0; st < ns; st++) {
    drX += drVX * sdt; drY += drVY * sdt;
    const int a = max(0, drPi - 12), b = min(R.n - 2, drPi + 14);   // murs proches seulement
    for (int i = a; i <= b; i++) { drWall(R.lx[i], R.ly[i], R.lx[i + 1], R.ly[i + 1]); drWall(R.rx[i], R.ry[i], R.rx[i + 1], R.ry[i + 1]); }
    if (a == 0) drWall(R.lx[0], R.ly[0], R.rx[0], R.ry[0]);  // fond de la ligne de depart
    drInside();
  }
  float bd = 1e9f; int bi = drPi;                        // avancement : point de la ligne centrale le plus proche
  for (int i = max(0, drPi - 6); i <= min(R.n - 1, drPi + 12); i++) { float dx = R.cx[i] - drX, dy = R.cy[i] - drY, d = dx * dx + dy * dy; if (d < bd) { bd = d; bi = i; } }
  drPi = bi;
  if (fabsf(vt) > 42 && sp > 70) {                       // traces de derapage (deux roues)
    const int h = R.skHead; R.sax[h] = (int16_t)(drX - hy * 3); R.say[h] = (int16_t)(drY + hx * 3); R.sbx[h] = (int16_t)(drX + hy * 3); R.sby[h] = (int16_t)(drY - hx * 3);
    R.sbrk[h] = drSkOn ? 0 : 1; drSkOn = true;           // 1 = debut d'une trace (pas de trait depuis le point precedent)
    R.skHead = (h + 1) % DR_SK; if (R.skN < DR_SK) R.skN++;
  } else drSkOn = false;
  if (drPi >= R.n - 2) { float dx = R.cx[R.n - 1] - drX, dy = R.cy[R.n - 1] - drY; if (dx * dx + dy * dy < R.w * R.w * 0.36f) {
    drPh = 3; drJLock = true; drFinMs = millis(); if (drBest[drTk] == 0 || drT < drBest[drTk]) drBest[drTk] = drT; snd(SND_WIN);
    if (dr2P) for (int k = 0; k < 3; k++) espSend(M_FIN, (uint8_t)drTk, 0, 0, (uint32_t)(drT * 1000)); } }
}

// ---- dessin ----
int drSX(float wx) { return (int)(wx - drCamX) + SCREEN_W / 2; }
int drSY(float wy) { return (int)(wy - drCamY) + SCREEN_H / 2; }
void drLine2(int x0, int y0, int x1, int y1, int sTop, uint16_t c, uint16_t c2) {   // trait de bord : 2 px + halo
  y0 -= sTop; y1 -= sTop;
  if ((y0 < -3 && y1 < -3) || (y0 > AL_SH + 3 && y1 > AL_SH + 3) || (x0 < -3 && x1 < -3) || (x0 > SCREEN_W + 3 && x1 > SCREEN_W + 3)) return;
  const bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) { alG->drawLine(x0 - 1, y0, x1 - 1, y1, c2); alG->drawLine(x0 + 2, y0, x1 + 2, y1, c2); alG->drawLine(x0, y0, x1, y1, c); alG->drawLine(x0 + 1, y0, x1 + 1, y1, c); }
  else { alG->drawLine(x0, y0 - 1, x1, y1 - 1, c2); alG->drawLine(x0, y0 + 2, x1, y1 + 2, c2); alG->drawLine(x0, y0, x1, y1, c); alG->drawLine(x0, y0 + 1, x1, y1 + 1, c); }
}
void drPointer(float wx, float wy, float h, int sTop, uint16_t fill, uint16_t edge, bool filled) {   // pointeur de souris classique, tourne vers le cap
  static const int8_t P[7][2] = { { 0, 0 }, { 0, 15 }, { 4, 11 }, { 7, 17 }, { 9, 16 }, { 6, 10 }, { 11, 10 } };
  const float a = h - atan2f(-9.0f, -4.0f), c = alCos(a), s = alSin(a);   // l'axe centre -> pointe de l'image suit le cap
  const int ox = drSX(wx), oy = drSY(wy) - sTop; if (oy < -16 || oy > AL_SH + 16) return;
  int X[7], Y[7];
  for (int i = 0; i < 7; i++) { const float lx = (P[i][0] - 4) * 1.15f, ly = (P[i][1] - 9) * 1.15f; X[i] = ox + (int)(lx * c - ly * s); Y[i] = oy + (int)(lx * s + ly * c); }
  if (filled) {
    alG->fillTriangle(X[0], Y[0], X[1], Y[1], X[2], Y[2], fill); alG->fillTriangle(X[0], Y[0], X[2], Y[2], X[5], Y[5], fill);
    alG->fillTriangle(X[2], Y[2], X[3], Y[3], X[4], Y[4], fill); alG->fillTriangle(X[2], Y[2], X[4], Y[4], X[5], Y[5], fill); alG->fillTriangle(X[0], Y[0], X[5], Y[5], X[6], Y[6], fill);
  }
  for (int i = 0; i < 7; i++) alG->drawLine(X[i], Y[i], X[(i + 1) % 7], Y[(i + 1) % 7], edge);
}
void drChecker(int i, int sTop) {                        // ligne en damier en travers du circuit au point i
  DrRun &R = *drR;
  for (int k = 0; k < 8; k++) {
    const float t0 = k / 8.0f, t1 = (k + 1) / 8.0f;
    const int x0 = drSX(R.lx[i] + (R.rx[i] - R.lx[i]) * t0), y0 = drSY(R.ly[i] + (R.ry[i] - R.ly[i]) * t0) - sTop, x1 = drSX(R.lx[i] + (R.rx[i] - R.lx[i]) * t1), y1 = drSY(R.ly[i] + (R.ry[i] - R.ly[i]) * t1) - sTop;
    if (y0 < -4 && y1 < -4) return; if (y0 > AL_SH + 4 && y1 > AL_SH + 4) return;
    alG->drawLine(x0, y0, x1, y1, (k & 1) ? 0xFFFF : 0x0000); alG->drawLine(x0, y0 + 1, x1, y1 + 1, (k & 1) ? 0x0000 : 0xFFFF);
  }
}
void drText(int n, int cx, int y, const char *s, uint16_t c, int sTop) {
  const int x = cx - textW(*alG, n, s) / 2;
  alG->setTextColor(0x0000); gfxPrintAt(*alG, n, x + 1, y + 1 - sTop, s); alG->setTextColor(c); gfxPrintAt(*alG, n, x, y - sTop, s);
}
void drFmt(char *b, size_t n, float t) { int cs = (int)(t * 100 + 0.5f); snprintf(b, n, "%d.%02d", cs / 100, cs % 100); }

void drPreview(int sTop) {                               // choix du circuit : trace entier, reduit pour tenir dans l'ecran
  DrRun &R = *drR; const DrTrack &T = DR_TR[drTk];
  int mx0 = 9999, my0 = 9999, mx1 = -9999, my1 = -9999;
  for (int i = 0; i < R.n; i++) { mx0 = min(mx0, (int)min(R.lx[i], R.rx[i])); mx1 = max(mx1, (int)max(R.lx[i], R.rx[i])); my0 = min(my0, (int)min(R.ly[i], R.ry[i])); my1 = max(my1, (int)max(R.ly[i], R.ry[i])); }
  const float sc = min(280.0f / (mx1 - mx0), 150.0f / (my1 - my0)); const int ox = (SCREEN_W - (int)((mx1 - mx0) * sc)) / 2, oy = 58;
  const uint16_t c = drC(T.col, 16);
  for (int i = 0; i + 1 < R.n; i++) {
    alG->drawLine(ox + (int)((R.lx[i] - mx0) * sc), oy + (int)((R.ly[i] - my0) * sc) - sTop, ox + (int)((R.lx[i + 1] - mx0) * sc), oy + (int)((R.ly[i + 1] - my0) * sc) - sTop, c);
    alG->drawLine(ox + (int)((R.rx[i] - mx0) * sc), oy + (int)((R.ry[i] - my0) * sc) - sTop, ox + (int)((R.rx[i + 1] - mx0) * sc), oy + (int)((R.ry[i + 1] - my0) * sc) - sTop, c);
  }
  alG->fillCircle(ox + (int)((R.cx[0] - mx0) * sc), oy + (int)((R.cy[0] - my0) * sc) - sTop, 3, 0xFFFF);                               // depart
  alG->fillCircle(ox + (int)((R.cx[R.n - 1] - mx0) * sc), oy + (int)((R.cy[R.n - 1] - my0) * sc) - sTop, 3, RGB565(255, 60, 60));     // arrivee
  char b[40], t[12];
  snprintf(b, sizeof b, "<  %s  >", T.name); drText(3, SCREEN_W / 2, 14, b, c, sTop);
  if (drBest[drTk] > 0) { drFmt(t, sizeof t, drBest[drTk]); snprintf(b, sizeof b, "circuit %d / 3   record %s s", drTk + 1, t); } else snprintf(b, sizeof b, "circuit %d / 3   pas encore de record", drTk + 1);
  drText(1, SCREEN_W / 2, 42, b, RGB565(200, 200, 220), sTop);
  drText(1, SCREEN_W / 2, 214, "SAUT tenu = accelerer   G / D = tourner", RGB565(200, 200, 220), sTop);
  drText(1, SCREEN_W / 2, 226, dr2P && !isHost ? "l'hote choisit le circuit   G + D 1 s = quitter" : "SAUT = partir   G + D 1 s = quitter", c, sTop);
}

void drDraw() {
  DrRun &R = *drR; const DrTrack &T = DR_TR[drTk]; const float t = millis() / 1000.0f;
  const uint16_t bg = RGB565(14, 14, 24), dot = RGB565(40, 40, 62), wall = drC(T.col, 16), halo = drC(T.col, 5), skid = RGB565(46, 44, 60);
  const float shx = drShake > 0 ? alSin(t * 80) * drShake : 0, savX = drCamX; drCamX += shx;
  for (int sTop = 0; sTop < SCREEN_H; sTop += AL_SH) {
    const int hh = (sTop / AL_SH) & 1;
    alDmaWait(2); alG = alHf[hh];
    alG->fillRect(0, 0, SCREEN_W, AL_SH, bg);
    if (drPh == 0) { drPreview(sTop); alSendHalf(hh, sTop); continue; }
    const int gx0 = ((int)drCamX - SCREEN_W / 2) / 24 * 24 - 24, gy0 = ((int)drCamY - SCREEN_H / 2 + sTop) / 24 * 24 - 24;   // grille de points (le "bureau")
    for (int gy = gy0; gy < gy0 + AL_SH + 48; gy += 24) { const int sy = drSY(gy) - sTop; if (sy < 0 || sy >= AL_SH) continue; for (int gx = gx0; gx < gx0 + SCREEN_W + 48; gx += 24) alG->drawPixel(drSX(gx), sy, dot); }
    for (int j = 1; j < R.skN; j++) {                    // traces de derapage
      const int i = (R.skHead - R.skN + j + DR_SK) % DR_SK, p = (i + DR_SK - 1) % DR_SK; if (R.sbrk[i]) continue;
      const int y0 = drSY(R.say[p]) - sTop, y1 = drSY(R.say[i]) - sTop; if ((y0 < -2 && y1 < -2) || (y0 > AL_SH + 2 && y1 > AL_SH + 2)) continue;
      alG->drawLine(drSX(R.sax[p]), y0, drSX(R.sax[i]), y1, skid); alG->drawLine(drSX(R.sbx[p]), drSY(R.sby[p]) - sTop, drSX(R.sbx[i]), drSY(R.sby[i]) - sTop, skid);
    }
    for (int i = 0; i + 1 < R.n; i++) {                  // bords du circuit
      drLine2(drSX(R.lx[i]), drSY(R.ly[i]), drSX(R.lx[i + 1]), drSY(R.ly[i + 1]), sTop, wall, halo);
      drLine2(drSX(R.rx[i]), drSY(R.ry[i]), drSX(R.rx[i + 1]), drSY(R.ry[i + 1]), sTop, wall, halo);
    }
    drLine2(drSX(R.lx[0]), drSY(R.ly[0]), drSX(R.rx[0]), drSY(R.ry[0]), sTop, wall, halo);
    drChecker(2, sTop); drChecker(R.n - 1, sTop);        // depart / arrivee
    for (int i = 0; i < DR_NP; i++) { const DrPt &q = R.p[i]; if (q.life > 0) alG->fillRect(drSX(q.x), drSY(q.y) - sTop, 2, 2, q.life > 0.2f ? RGB565(255, 240, 160) : RGB565(255, 140, 40)); }
    if (dr2P) for (int k = 0; k < mpN; k++) {             // adversaires : pointeur plein a la couleur de leur numero
      const uint32_t pm = mpPosMs[k];
      if (mpIn[k] && mpOn[k] && mpPos[k] && millis() - pm < 700) drPointer(mpX[k], mpY[k], mpH[k] / 1000.0f, sTop, MP_COL[mpCol[k] & 3], RGB565(30, 20, 30), true);
    }
    for (int i = 7; i >= 1; i -= 2) drPointer(R.trX[i], R.trY[i], R.trA[i], sTop, 0, drC(T.col, 12 - i), false);   // trainee
    drPointer(drX, drY, drH, sTop, dr2P ? MP_COL[myCol & 3] : 0xFFFF, dr2P ? 0xFFFF : 0x0000, true);   // a plusieurs : mon pointeur a ma couleur (bord blanc)
    if (sTop == 0) {                                     // chrono, record
      char b[24], t2[12]; drFmt(t2, sizeof t2, drT); drText(3, SCREEN_W / 2, 4, t2, 0xFFFF, 0);
      if (dr2P) { int na = drPh == 3, nt = 1; for (int k = 0; k < mpN; k++) if (mpIn[k] && mpOn[k]) { nt++; if (mpFin[k]) na++; }   // joueurs encore en partie
        snprintf(b, sizeof b, "%s  %d/%d arrives", MP_NAME[myCol & 3], na, nt); drText(1, SCREEN_W - 60, 8, b, MP_COL[myCol & 3], 0); }
      else if (drBest[drTk] > 0) { drFmt(t2, sizeof t2, drBest[drTk]); snprintf(b, sizeof b, "record %s", t2); drText(1, SCREEN_W - 50, 8, b, wall, 0); }
      drText(1, 40, 8, T.name, wall, 0);
    }
    if (sTop == SCREEN_H - AL_SH) {                      // jauge de vitesse
      const float sp = sqrtf(drVX * drVX + drVY * drVY); alG->drawRect(110, 48, 100, 6, halo); alG->fillRect(111, 49, (int)(98 * min(1.0f, sp / DR_VMAX)), 4, wall);
    }
    if (drPh == 1) {                                     // compte a rebours
      const int n = (int)ceilf(drCd); char b[4]; snprintf(b, sizeof b, "%d", n); if (n <= 3 && n >= 1) drText(8, SCREEN_W / 2, 70, b, wall, sTop);
    } else if (drPh == 2 && drT < 0.7f) drText(8, SCREEN_W / 2, 70, "GO", wall, sTop);
    else if (drPh == 3) {                                // arrivee
      char b[40], t2[12];
      if (!dr2P) {
        alG->fillRoundRect(60, 60 - sTop, 200, 120, 10, RGB565(24, 22, 40)); alG->drawRoundRect(60, 60 - sTop, 200, 120, 10, wall);
        drText(2, SCREEN_W / 2, 68, "ARRIVEE", wall, sTop);
        drFmt(t2, sizeof t2, drT); snprintf(b, sizeof b, "%s s", t2); drText(5, SCREEN_W / 2, 90, b, 0xFFFF, sTop);
        if (drT <= drBest[drTk]) snprintf(b, sizeof b, "NOUVEAU RECORD !   %d chocs", drHits); else { drFmt(t2, sizeof t2, drBest[drTk]); snprintf(b, sizeof b, "record %s s   %d chocs", t2, drHits); }
        drText(1, SCREEN_W / 2, 134, b, wall, sTop);
        snprintf(b, sizeof b, "<  %s  >   SAUT = jouer", DR_TR[drNext].name); drText(1, SCREEN_W / 2, 156, b, drC(DR_TR[drNext].col, 16), sTop);
      } else drRanking(sTop, wall);
    }
    alSendHalf(hh, sTop);
  }
  drCamX = savX;
}

int driftTrack() { return drPh == 3 ? drNext : drTk; }    // circuit a donner a un joueur qui rejoint
bool drAllIn() { for (int k = 0; k < mpN; k++) if (mpIn[k] && mpOn[k] && !mpFin[k]) return false; return true; }
bool drHostCanGo() { return isHost && (drAllIn() || millis() - drFinMs > 8000); }   // multijoueur : l'hote relance quand tous les joueurs restants sont arrives
void drRanking(int sTop, uint16_t wall) {                // multijoueur : classement J1..J4 (arrives par temps, puis en course, puis partis)
  struct Row { int j; float t; int st; } rows[4]; int n = 0;   // j : couleur ; st : 0 arrive, 1 en course, 2 parti
  rows[n++] = { myCol & 3, drT, 0 };
  for (int k = 0; k < mpN && n < 4; k++) if (mpIn[k]) rows[n++] = { mpCol[k] & 3, mpFin[k] ? mpFinT[k] / 1000.0f : 0, mpFin[k] ? 0 : mpOn[k] ? 1 : 2 };
  for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (rows[b].st < rows[a].st || (rows[b].st == rows[a].st && rows[b].t < rows[a].t)) { Row x = rows[a]; rows[a] = rows[b]; rows[b] = x; }
  int myPos = 1; for (int a = 0; a < n; a++) if (rows[a].j == (myCol & 3)) myPos = a + 1;
  alG->fillRoundRect(50, 46 - sTop, 220, 150, 10, RGB565(24, 22, 40)); alG->drawRoundRect(50, 46 - sTop, 220, 150, 10, wall);
  char b[40], t2[12];
  const bool done = drAllIn();
  if (done) snprintf(b, sizeof b, myPos == 1 ? "1ER !" : "%de", myPos); else snprintf(b, sizeof b, "ARRIVEE  (%de pour l'instant)", myPos);
  drText(2, SCREEN_W / 2, 54, b, myPos == 1 && done ? RGB565(60, 230, 110) : wall, sTop);
  for (int a = 0; a < n; a++) {
    const int y = 82 + a * 20; const bool me = rows[a].j == (myCol & 3);
    if (rows[a].st == 0) { drFmt(t2, sizeof t2, rows[a].t); snprintf(b, sizeof b, "%d. %s%s   %s s", a + 1, MP_NAME[rows[a].j], me ? " (toi)" : "", t2); }
    else snprintf(b, sizeof b, "%d. %s   %s", a + 1, MP_NAME[rows[a].j], rows[a].st == 1 ? "en course..." : "parti");
    drText(2, SCREEN_W / 2, y, b, MP_COL[rows[a].j], sTop);
  }
  snprintf(b, sizeof b, "%d chocs", drHits); drText(1, SCREEN_W / 2, 164, b, wall, sTop);
  if (isHost) snprintf(b, sizeof b, "<  %s  >   %s", DR_TR[drNext].name, drHostCanGo() ? "SAUT = jouer" : "attente des autres...");
  else snprintf(b, sizeof b, "prochain : %s  (l'hote choisit)", DR_TR[drNext].name);
  drText(1, SCREEN_W / 2, 178, b, drC(DR_TR[drNext].col, 16), sTop);
}
void startDrift() {
  alDmaBegin();
  if (!drR) drR = (DrRun *)calloc(1, sizeof(DrRun));
  if (!drR) { alDmaEnd(); tft.setSPISpeed(40000000); state = ST_MODE; drawMode(); return; }
  dr2P = twoP; vDSelNew = false; drPh = 0; drQuit = 0; drPL = drPR = true; drJLock = true; drBuild(); drReset(); state = ST_DRIFT;
}
void driftFree() { alDmaEnd(); free(drR); drR = nullptr; }

bool driftStep(bool bl, bool br, bool bj, bool je) {    // false = on quitte
  static unsigned long last = 0; const unsigned long now = millis();
  float dt = (now - last) / 1000.0f; last = now; if (dt <= 0) dt = 0.016f; if (dt > 0.05f) dt = 0.05f;
  { static unsigned long t0 = 0; static int n = 0; n++; if (now - t0 >= 1000) { drFps = n; n = 0; t0 = now; }
    if (Serial.available()) { while (Serial.available()) Serial.read(); Serial.printf("[DRIFT] fps=%d ph=%d pi=%d/%d t=%.2f\n", drFps, drPh, drPi, drR->n, drT); } }   // mesure a la demande
  if (bl && br) { drQuit += dt; if (drQuit > 1.0f) return false; } else drQuit = 0;
  const bool eL = bl && !drPL, eR = br && !drPR; drPL = bl; drPR = br;
  if (!bj) drJLock = false;
  bool j = bj && !drJLock;
#ifdef DRIFT_BENCH
  if (drPh == 0) { drPh = 1; drCd = 0.01f; }
  if (drPh == 3) { drTk = (drTk + 1) % 3; drBuild(); drReset(); drPh = 1; drCd = 0.01f; }
  if (drPh == 2) {                                       // pilote : vise un point de la ligne centrale un peu plus loin
    const int k = min(drR->n - 1, drPi + 4); float d = alWrap(atan2f(drR->cy[k] - drY, drR->cx[k] - drX) - drH);
    bl = d < -0.12f; br = d > 0.12f; j = fabsf(d) < 0.9f;
  }
#endif
#ifdef LOBBY_BENCH
  if (dr2P && drPh == 2) { const int k = min(drR->n - 1, drPi + 4); float d = alWrap(atan2f(drR->cy[k] - drY, drR->cx[k] - drX) - drH); bl = d < -0.12f; br = d > 0.12f; j = fabsf(d) < 0.9f; }
  { static uint32_t pt = 0; if (dr2P && now - pt > 1000) { pt = now; Serial.printf("[D2] fps=%d host=%d ph=%d tk=%d t=%.1f pi=%d arrives=%d/%d rx=%u txerr=%d heap=%u max=%u\n", drFps, isHost ? 1 : 0, drPh, drTk, drT, drPi, (int)(drPh == 3) + [] { int n = 0; for (int k = 0; k < mpN; k++) if (mpIn[k] && mpFin[k]) n++; return n; }(), 1 + [] { int n = 0; for (int k = 0; k < mpN; k++) if (mpIn[k]) n++; return n; }(), (unsigned)espRxN, espTxErr, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap()); } }
#endif
  if (dr2P) {
    vPos = false; vFin = false;                          // positions et arrivees des adversaires : tableaux mp* (ecrits par la reception)
    static uint32_t lastPos = 0, lastSel = 0;
    if (drPh >= 1 && now - lastPos >= (uint32_t)(mpActive() > 1 ? 60 : 40)) { lastPos = now; espSend2(M_POS, (uint8_t)drTk, (int16_t)drX, (int16_t)drY, 0, (int16_t)(alWrap(drH) * 1000)); }
    if (isHost) { if (drPh == 0 && now - lastSel >= 500) { lastSel = now; espSend2(M_DSEL, (uint8_t)drTk, 0, 0, 0, 0); } }
    else if (vDSelNew) {                                 // invite : suit l'hote (circuit, depart)
      vDSelNew = false;
      if (vDGo == 2) drNext = vDSel % 3;                 // l'hote montre le prochain circuit (a l'arrivee)
      else {
        if (vDSel != drTk) { drTk = vDSel % 3; drBuild(); drReset(); }
        if (vDGo == 1) { drReset(); drPh = 1; drCd = 3.0f; } else if (drPh != 0 && drPh != 2) { drReset(); drPh = 0; }
      }
    }
  }
  if (drPh == 0) {
    if (!dr2P || isHost) {
      if ((eL && !br) || (eR && !bl)) { drTk = (drTk + (eR ? 1 : 2)) % 3; drBuild(); drReset(); if (dr2P) espSend2(M_DSEL, (uint8_t)drTk, 0, 0, 0, 0); }
      if (je && !drJLock) { drPh = 1; drCd = 3.0f; drJLock = true; if (dr2P) for (int k = 0; k < 3; k++) espSend2(M_DSEL, (uint8_t)drTk, 1, 0, 0, 0); }
    }
  } else if (drPh == 1) {
    drCd -= dt; if (drCd <= 0) { drPh = 2; drT = 0; }
  } else if (drPh == 2) {
    drT += dt; drPhys(dt, bl, br, j);
  } else {
    drVX *= 1 - min(1.0f, dt * 2); drVY *= 1 - min(1.0f, dt * 2); drX += drVX * dt; drY += drVY * dt;   // on finit sur sa lancee
    if (!dr2P || isHost) {
      if ((eL && !br) || (eR && !bl)) { drNext = (drNext + (eR ? 1 : 2)) % 3; if (dr2P) espSend2(M_DSEL, (uint8_t)drNext, 2, 0, 0, 0); }   // prochain circuit
      if (je && !drJLock && (!dr2P || drHostCanGo())) {
        if (drNext != drTk) { drTk = drNext; drBuild(); }
        drReset(); drPh = 1; drCd = 3.0f; drJLock = true; if (dr2P) for (int k = 0; k < 3; k++) espSend2(M_DSEL, (uint8_t)drTk, 1, 0, 0, 0);
      }
    }
  }
  DrRun &R = *drR;
  for (int i = 0; i < DR_NP; i++) { DrPt &q = R.p[i]; if (q.life <= 0) continue; q.life -= dt; q.x += q.vx * dt; q.y += q.vy * dt; q.vx *= 0.9f; q.vy *= 0.9f; }
  static int tf = 0; if (++tf >= 2) { tf = 0; for (int i = 7; i > 0; i--) { R.trX[i] = R.trX[i - 1]; R.trY[i] = R.trY[i - 1]; R.trA[i] = R.trA[i - 1]; } R.trX[0] = drX; R.trY[0] = drY; R.trA[0] = drH; }
  const float lx = drX + drVX * 0.35f, ly = drY + drVY * 0.35f;   // camera : en avance sur la vitesse
  drCamX += (lx - drCamX) * min(1.0f, dt * 5); drCamY += (ly - drCamY) * min(1.0f, dt * 5);
  if (drShake > 0) drShake = max(0.0f, drShake - dt * 20);
  drDraw();
  return true;
}
