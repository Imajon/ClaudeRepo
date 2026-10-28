// ================================ ALTO ================================
// Mini-jeu solo (menu "Alto") : glisse au coucher du soleil, d'apres assets/alto*.jpg (Alto's Adventure).
// SAUT tenu = charger (0 -> 0,6 s, jauge au-dessus de la tete), relache = sauter (plus haut si la charge est longue).
// GAUCHE / DROITE en l'air = rotation arriere / avant : un tour complet = salto (+points, multiplicateur qui monte).
// Atterrir de travers (> ~50 degres de la pente) ou heurter un rocher = chute (multiplicateur perdu) ; tomber dans une fosse = "CHUTE", on repart apres.
// Course de 2 minutes ; le soleil descend pendant la course. 3 niveaux, chacun sa palette (rose, ambre, nuit bleue) et ses dosages :
// dunes, bosses, tremplins, fosses, double fosse, rochers, double niveau (corde a fanions en haut / sol avec fosse ou rochers en bas), falaises, arcs de diamants.
// Choix du niveau : GAUCHE / DROITE, SAUT = partir. GAUCHE + DROITE tenus 1 s = quitter (a tout moment).
// Monde = AL_NCH troncons de AL_CL px tires a l'entree (graine = niveau : meme parcours a chaque essai). Tout est alloue en entrant (alR, calloc) et libere en sortant.

#include "rider_sprite.h"   // silhouette du snowboarder (tools/tools_rider.py, assets/Snowborder2.png)

#define AL_CL   512
#define AL_NCH  110      // 56 000 px : plus que 2 min a la vitesse max (420 px/s)
#define AL_NIT  6
#define AL_NP   48
#define AL_NONE 1e9f
#define AL_G    1000.0f
#define AL_TIME 120.0f
#define AL_Z    0.7f     // zoom de la camera : le monde est dessine a 70 % (on voit plus loin) ; la physique reste en pixels du monde
#define AL_RZ   0.8f     // echelle du glisseur a l'ecran (un peu plus grand que le monde pour rester lisible)
#define AL_SH   60       // bandes de 60 lignes : le tampon fb (120 lignes) sert de double tampon, une moitie part en DMA pendant qu'on dessine l'autre
enum { AT_DUNE, AT_BUMPS, AT_KICK, AT_GAP, AT_ROCKS, AT_DOUBLE, AT_CLIFF, AT_ARC, AT_DGAP, AT_N };

struct AlCh { uint8_t t, var, ni, nr, got; float y0, sl; int16_t p0, p1; int16_t iu[AL_NIT], iy[AL_NIT], ru[3]; uint8_t rh[3]; };
struct AlPt { float x, y, vx, vy, life; uint16_t c; };
struct AlRun {
  AlCh ch[AL_NCH]; AlPt p[AL_NP];
  uint8_t skR[SCREEN_H], skG[SCREEN_H], skB[SCREEN_H], glX[SCREEN_W], glY[SCREEN_H];
  uint16_t ly[3][SCREEN_H], gd[64];
  uint8_t hA[1200], hB[1100], hC[2400];                // profils des 3 plans de montagnes (periodiques), tires a l'entree du niveau
  int16_t cA[SCREEN_W], cB[SCREEN_W], cC[SCREEN_W], cG[SCREEN_W];   // haut de chaque plan et du sol par colonne, calcule une fois par image
  int16_t stX[70], stY[70]; uint16_t stC[70];           // etoiles
  uint16_t skT[SCREEN_H][16];                           // ciel : par ligne, 16 niveaux de halo deja tramés (calcule a l'entree du niveau)
};
float AL_SIN[257];                                       // sinus tabule (le C3 n'a pas d'unite flottante : sinf coute tres cher)
// ciel haut, horizon, halo, soleil, brume, montagnes, sol, sol profond, liseré, echarpe ; soleil x, y, rayon ; horizon ; etoiles ; derive du soleil pendant la course
struct AlPal { const char *name; uint8_t top[3], hor[3], glow[3], sun[3], mist[3], lay[3], gnd[3], deep[3], rim[3], acc[3]; int16_t sx, sy, sr, hz, stars, drift; };
static const AlPal AL_PAL[3] = {
  { "AUBE ROSE",  {62, 40, 92}, {246, 140, 160}, {255, 190, 192}, {255, 246, 240}, {238, 154, 172}, {116, 62, 112}, {118, 56, 80}, {50, 22, 44}, {252, 178, 188}, {214, 40, 84},  176, 112, 26, 132, 40, 30 },
  { "AMBRE",      {66, 22, 36}, {250, 148, 80},  {255, 214, 128}, {255, 244, 180}, {232, 150, 92},  {128, 64, 48},  {116, 64, 44}, {44, 20, 20}, {255, 196, 126}, {200, 30, 30},  196, 116, 24, 134, 30, 34 },
  { "NUIT BLEUE", {10, 14, 42}, {84, 130, 184},  {170, 206, 250}, {236, 244, 255}, {104, 146, 196}, {34, 52, 100},  {30, 44, 82},  {8, 12, 30},  {176, 212, 255}, {250, 196, 80}, 150, 80, 20, 130, 70, -24 },
};
static const uint8_t AL_BLACK[3] = { 0, 0, 0 };
static const uint8_t AL_BAY[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

AlRun *alR = nullptr;
struct AlHalf : public FastCanvas { AlHalf(uint16_t *b) : FastCanvas(SCREEN_W, AL_SH, false) { buffer = b; } };   // canevas 320x60 pose sur une moitie de fb
FastCanvas *alG = &fb;                                   // canevas de la bande en cours de dessin
AlHalf *alHf[2];
int alQ = 0;                                             // transferts DMA en file
spi_transaction_t alTr[4];
const AlPal *alP = &AL_PAL[0];
int alLvl = 0, alPh = 0;                    // phase : 0 choix du niveau, 1 course, 2 resultats
float alX, alY, alV, alVX, alVY, alA, alAV, alChg, alRot, alAir, alCrash, alFall, alSquash, alT, alLook, alCamX, alCamY, alCamYS, alQuit, alShake, alMsgT, alScoreF, alSunY, alAuto;
bool alGnd, alRopeOn, alPL, alPR, alJLock;
int alDia, alFlips, alFalls, alCombo, alBest[3], alPi, alGlYFor = -999;
char alMsg[28];
uint32_t alSeed, alPerf[2], alPc[2];
float alCX, alCY;                                        // camera de l'image en cours (monde, secousse comprise)
#define AL_SC 8
float alScX[AL_SC], alScY[AL_SC];                        // echarpe : chaine de points dans le monde, accrochee au cou
int alSX(float wx) { return (int)floorf((wx - alCX) * AL_Z + 0.5f); }   // monde -> ecran
int alSY(float wy) { return (int)floorf((wy - alCY) * AL_Z + 0.5f); }
int alFps = 0, alFpsMin = 999;

float alSin(float a) { float f = a * (256 / (2 * PI)); int i = (int)floorf(f); float r = f - i; i &= 255; return AL_SIN[i] + (AL_SIN[i + 1] - AL_SIN[i]) * r; }
float alCos(float a) { return alSin(a + PI / 2); }
uint32_t alRand(uint32_t n) { alSeed = alSeed * 1664525u + 1013904223u; return (alSeed >> 8) % n; }
uint16_t alMix(const uint8_t *a, const uint8_t *b, float t) { return RGB565((int)(a[0] + (b[0] - a[0]) * t), (int)(a[1] + (b[1] - a[1]) * t), (int)(a[2] + (b[2] - a[2]) * t)); }
uint16_t alC(const uint8_t *a) { return RGB565(a[0], a[1], a[2]); }
float alWrap(float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
int alCi(float wx) { int c = (int)(wx / AL_CL); return c < 0 ? 0 : c >= AL_NCH ? AL_NCH - 1 : c; }
float alBaseY(float wx) { int c = alCi(wx); const AlCh &h = alR->ch[c]; return h.y0 + h.sl * (wx - c * AL_CL); }

// hauteur du sol (y vers le bas) ; AL_NONE = fosse
float alGround(float wx) {
  int c = alCi(wx); const AlCh &h = alR->ch[c];
  float u = wx - c * AL_CL, y = h.y0 + h.sl * u;
  switch (h.t) {
    case AT_DUNE: return y + h.p0 * alSin(u * (PI / 256));
    case AT_ROCKS: case AT_ARC: return y + 6 * alSin(u * (PI / 256));
    case AT_BUMPS: { if (u < 64 || u >= 64 + h.p0 * 128) return y; float s = alSin((u - 64) * (PI / 128)); return y - h.p1 * s * s; }   // bosses
    case AT_KICK: if (u >= 180 && u < 300) { float k = (u - 180) / 120; return y - h.p0 * k * k; }                                    // tremplin
      if (u >= 300 && u < 330) return y - h.p0 * (330 - u) / 30; return y;
    case AT_GAP: if (u >= 120 && u < 200) { float k = (u - 120) / 80; return y - h.p1 * k * k; }                                     // levre + fosse
      if (u >= 200 && u < 200 + h.p0) return AL_NONE; return y;
    case AT_DOUBLE: if (u < 90) { float k = u / 90; return y - 40 * k * k; } if (u < 110) return y - 40 * (110 - u) / 20;              // tremplin vers la corde
      if (h.var == 0 && u >= 280 && u < 280 + h.p0) return AL_NONE; return y;                                                         // en bas : fosse (ou rochers)
    case AT_CLIFF: { float k = constrain((u - 200) / 130, 0.0f, 1.0f); return y + h.p0 * k * k * (3 - 2 * k); }                      // falaise
    case AT_DGAP: if (u >= 60 && u < 140) { float k = (u - 60) / 80; return y - 26 * k * k; } if (u >= 140 && u < 140 + h.p0) return AL_NONE;   // double fosse
      if (u >= 260 && u < 330) { float k = (u - 260) / 70; return y - 26 * k * k; } if (u >= 330 && u < 330 + h.p0) return AL_NONE; return y;
  }
  return y;
}
// corde du double niveau (une seule face : on s'y pose en descendant)
float alRope(float wx) {
  int c = alCi(wx); const AlCh &h = alR->ch[c]; float u = wx - c * AL_CL;
  if (h.t != AT_DOUBLE || u < 130 || u > 500) return AL_NONE;
  return h.y0 + h.sl * u - 70;
}
float alSurfY(float x) { return alRopeOn ? alRope(x) : alGround(x); }
float alSlope(float x) { float a = alSurfY(x - 2), b = alSurfY(x + 2); return (a >= AL_NONE || b >= AL_NONE) ? alR->ch[alCi(x)].sl : (b - a) / 4; }

void alAddIt(int c, float u, float wy) { AlCh &h = alR->ch[c]; if (h.ni < AL_NIT) { h.iu[h.ni] = (int16_t)u; h.iy[h.ni] = (int16_t)(wy - h.y0); h.ni++; } }
void alAddRock(int c, int u, int hh) { AlCh &h = alR->ch[c]; if (h.nr < 3) { h.ru[h.nr] = u; h.rh[h.nr] = hh; h.nr++; } }

void alGen() {
  static const uint8_t W[3][AT_N] = { { 3, 3, 2, 2, 2, 2, 1, 2, 0 }, { 2, 2, 2, 3, 2, 3, 2, 2, 1 }, { 1, 2, 2, 3, 2, 3, 3, 2, 3 } };   // dosage des troncons par niveau
  alSeed = 0xA170u + alLvl * 7919u;
  float y = 0; int prev = -1, tot = 0;
  for (int k = 0; k < AT_N; k++) tot += W[alLvl][k];
  for (int c = 0; c < AL_NCH; c++) {
    AlCh &h = alR->ch[c]; memset(&h, 0, sizeof h);
    h.y0 = y; h.sl = 0.13f + alLvl * 0.04f + alRand(9) * 0.012f;
    int t = AT_DUNE;
    const bool calm = c < 2 || (c & 1);                 // un troncon sur deux : glisse calme, pour respirer entre les obstacles
    if (!calm) do { int r = alRand(tot); t = 0; while (r >= W[alLvl][t]) r -= W[alLvl][t++]; } while (t == prev || t == AT_DUNE);
    h.t = t; prev = t; h.var = alRand(2);
    const float x0 = c * AL_CL;
    switch (t) {
      case AT_DUNE: h.p0 = (alRand(2) ? 1 : -1) * (calm ? 4 + alRand(7) : 6 + alRand(10)); break;
      case AT_BUMPS: h.p0 = 2 + alRand(2); h.p1 = 16 + alLvl * 6 + alRand(10); break;
      case AT_KICK: h.p0 = 32 + alRand(14) + alLvl * 6; break;
      case AT_GAP: h.p0 = 80 + alLvl * 22 + alRand(26); h.p1 = 24 + alLvl * 4; break;
      case AT_DOUBLE: h.p0 = 70 + alLvl * 12; break;
      case AT_CLIFF: h.p0 = 80 + alLvl * 30 + alRand(30); break;
      case AT_DGAP: h.p0 = 76 + alRand(22); break;
    }
    switch (t) {                                        // diamants et rochers (le sol du troncon est pret)
      case AT_DUNE: if (h.var) for (int k = 0; k < 3; k++) alAddIt(c, 200 + k * 40, alGround(x0 + 200 + k * 40) - 14); break;
      case AT_BUMPS: for (int k = 0; k < h.p0; k++) alAddIt(c, 128 + k * 128, alGround(x0 + 128 + k * 128) - 24); break;
      case AT_KICK: for (int k = 0; k < 4; k++) alAddIt(c, 340 + k * 40, alGround(x0 + 300) - 34 + k * 8); break;
      case AT_GAP: for (int k = 1; k <= 3; k++) alAddIt(c, 200 + h.p0 * k / 4, alGround(x0 + 199) - 30 - 22 * alSin(PI * k / 4)); break;
      case AT_ROCKS: { int n = 2 + (alLvl > 0 && alRand(2)); for (int k = 0; k < n; k++) { int u = 110 + k * 150 + (int)alRand(30); int hh = 8 + alRand(5) + alLvl * 2; alAddRock(c, u, hh); alAddIt(c, u, alGround(x0 + u) - hh - 34); } } break;
      case AT_DOUBLE: for (int k = 0; k < 5; k++) alAddIt(c, 170 + k * 66, alRope(x0 + 170 + k * 66) - 14);
        if (h.var) { alAddRock(c, 270, 10 + alLvl * 2); alAddRock(c, 410, 10 + alLvl * 2); } break;
      case AT_CLIFF: for (int k = 0; k < 3; k++) alAddIt(c, 240 + k * 40, alGround(x0 + 240 + k * 40) - 30); break;
      case AT_ARC: for (int k = 0; k < 5; k++) alAddIt(c, 150 + k * 55, alBaseY(x0 + 150 + k * 55) - 26 - 30 * alSin(PI * k / 4)); break;
      case AT_DGAP: alAddIt(c, 140 + h.p0 / 2, alGround(x0 + 139) - 40); alAddIt(c, 330 + h.p0 / 2, alGround(x0 + 329) - 40); break;
    }
    y = h.y0 + h.sl * AL_CL + (t == AT_CLIFF ? h.p0 : 0);
  }
}

void alGlowY() {                                         // halo vertical, suit le soleil qui descend
  int sy = (int)alSunY; if (sy == alGlYFor) return; alGlYFor = sy;
  for (int y = 0; y < SCREEN_H; y++) { float d = (y - sy) / 75.0f; alR->glY[y] = (uint8_t)(255 * expf(-d * d)); }
}
void alPalInit() {
  alP = &AL_PAL[alLvl]; const AlPal &P = *alP; AlRun &R = *alR;
  for (int y = 0; y < SCREEN_H; y++) {
    float t = powf(constrain((float)y / (P.hz + 30), 0.0f, 1.0f), 1.4f);
    R.skR[y] = P.top[0] + (P.hor[0] - P.top[0]) * t; R.skG[y] = P.top[1] + (P.hor[1] - P.top[1]) * t; R.skB[y] = P.top[2] + (P.hor[2] - P.top[2]) * t;
    for (int k = 0; k < 3; k++) {                       // 3 plans de montagnes : plus sombres de pres, brume qui monte du bas
      float f = 0.35f + 0.25f * k, m = constrain((y - (P.hz + 6 + 22 * k)) / 120.0f, 0.0f, 0.5f);
      uint8_t b[3]; for (int i = 0; i < 3; i++) b[i] = P.hor[i] + (P.lay[i] - P.hor[i]) * f;
      R.ly[k][y] = alMix(b, P.mist, m);
    }
  }
  for (int y = 0; y < SCREEN_H; y++) for (int l = 0; l < 16; l++) {   // niveau l = l/15 du halo ; trame de Bayer selon la ligne et le niveau
    int r = R.skR[y] + (P.glow[0] - R.skR[y]) * l / 15, g = R.skG[y] + (P.glow[1] - R.skG[y]) * l / 15, b = R.skB[y] + (P.glow[2] - R.skB[y]) * l / 15, d = AL_BAY[((y & 3) << 2) | (l & 3)];
    r = min(255, r + (d >> 1)); g = min(255, g + (d >> 2)); b = min(255, b + (d >> 1));
    R.skT[y][l] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  for (int x = 0; x < SCREEN_W; x++) { float d = (x - P.sx) / 120.0f; R.glX[x] = (uint8_t)(255 * expf(-d * d)); }
  for (int d = 0; d < 64; d++) R.gd[d] = d < 2 ? alC(P.rim) : alMix(P.gnd, P.deep, min(1.0f, (d - 2) / 46.0f));
  alSunY = P.sy; alGlYFor = -999; alGlowY();
  for (int i = 0; i < 1200; i++) R.hA[i] = alHA(i);
  for (int i = 0; i < 1100; i++) R.hB[i] = alHB(i);
  for (int i = 0; i < 2400; i++) R.hC[i] = alHC(i);
  for (int i = 0; i < P.stars; i++) { uint32_t h = gHash(i * 7919 + 3); R.stX[i] = h % SCREEN_W; R.stY[i] = (h >> 9) % (P.hz - 64); R.stC[i] = alSkyAt(R.stX[i], R.stY[i]); }
}
uint16_t alSkyAt(int x, int y) {                         // couleur du ciel en un point (sans trame)
  const AlPal &P = *alP; y = constrain(y, 0, SCREEN_H - 1); x = constrain(x, 0, SCREEN_W - 1);
  int g = (alR->glX[x] * alR->glY[y]) >> 8, r = alR->skR[y], gg = alR->skG[y], b = alR->skB[y];
  return RGB565(r + (((P.glow[0] - r) * g) >> 8), gg + (((P.glow[1] - gg) * g) >> 8), b + (((P.glow[2] - b) * g) >> 8));
}
uint16_t alBlend(uint16_t a, uint16_t b, int t) {        // t sur 0..16
  int r = ((a >> 11) * (16 - t) + (b >> 11) * t) >> 4, g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) >> 4, bl = ((a & 31) * (16 - t) + (b & 31) * t) >> 4;
  return (r << 11) | (g << 5) | bl;
}

float alN(int32_t i, uint32_t s) { return (gHash((uint32_t)i * 2654435761u ^ s) & 1023) / 1023.0f; }
int alHA(int lx) { int k = lx / 40; float f = (lx - k * 40) / 40.0f; return 6 + (int)(46 * (alN(k % 30, 11 + alLvl) * (1 - f) + alN((k + 1) % 30, 11 + alLvl) * f)); }   // sommets anguleux (periode 1200)
int alHB(int lx) {                                       // silhouettes de temples / tours
  int s = lx / 22, m = lx - s * 22; uint32_t h = gHash((s % 50) * 31 + 7 + alLvl * 1000);
  int v = (h % 6 == 0) ? 3 : 10 + (int)(h % 24);
  if (h % 9 == 1 && m >= 8 && m < 14) v += 18 + (h >> 8) % 16 + (m >= 10 && m < 12 ? 7 : 0);
  else if (h % 7 == 2) v += 10 - abs(m - 11);
  return v;
}
int alHC(int lx) { int k = lx / 120; float f = (lx - k * 120) / 120.0f; f = (1 - alCos(f * PI)) * 0.5f; return 6 + (int)(34 * (alN(k % 20, 23 + alLvl) * (1 - f) + alN((k + 1) % 20, 23 + alLvl) * f)); }   // collines douces (periode 2400)

// haut des 3 plans et du sol par colonne (une fois par image) ; sol echantillonne tous les 4 px puis interpole, exact pres des fosses
void alColumns(int cx, int cy) {
  AlRun &R = *alR; const int hz = alP->hz; const float dv = (alCamY - alCamYS) * AL_Z; const int oA = (int)(dv * 0.06f), oB = (int)(dv * 0.14f), oC = (int)(dv * 0.3f);
  int ia = (cx / 20 + 120000) % 1200, ib = (cx * 3 / 25 + 110000) % 1100, ic = (cx * 3 / 10 + 120000) % 2400;
  for (int x = 0; x < SCREEN_W; x++) {
    R.cA[x] = hz - R.hA[ia] - oA; R.cB[x] = hz + 16 - R.hB[ib] - oB; R.cC[x] = hz + 50 - R.hC[ic] - oC;
    if (++ia == 1200) ia = 0; if (++ib == 1100) ib = 0; if (++ic == 2400) ic = 0;
  }
  (void)cy; const float iz = 1.0f / AL_Z;
  float gp = alGround(alCX);
  for (int x = 0; x < SCREEN_W; x += 4) {
    float gn = alGround(alCX + (x + 4) * iz);
    if (gp < AL_NONE && gn < AL_NONE) { int y0 = (int)((gp - alCY) * AL_Z * 16), d = (int)((gn - alCY) * AL_Z * 16) - y0; for (int k = 0; k < 4; k++) R.cG[x + k] = (y0 + d * k / 4) >> 4; }
    else for (int k = 0; k < 4; k++) { float g = k ? alGround(alCX + (x + k) * iz) : gp; R.cG[x + k] = g >= AL_NONE ? 32767 : (int)((g - alCY) * AL_Z); }
    gp = gn;
  }
}
// une colonne a la fois : ciel (degrade + halo + soleil, trame de Bayer), 3 plans de montagnes, sol (liseré clair puis degrade)
void alComposite(int sTop, int cx, int cy) {
  AlRun &R = *alR; const AlPal &P = *alP; uint16_t *b = alG->getBuffer();
  const int e = sTop + AL_SH, hz = P.hz, sy = (int)alSunY, r2 = P.sr * P.sr, r2e = (P.sr + 2) * (P.sr + 2);
  const uint16_t sunC = alC(P.sun); (void)cx; (void)cy; (void)hz;
  for (int x = 0; x < SCREEN_W; x++) {
    int tA = R.cA[x], tB = R.cB[x], tC = R.cC[x], tG = R.cG[x];
    if (tC > tG) tC = tG; if (tB > tC) tB = tC; if (tA > tB) tA = tB;
    uint16_t *p = b + x - sTop * SCREEN_W;
    int y = sTop, lim = min(e, tA); const int gx = R.glX[x], dx = x - P.sx; const bool sunCol = abs(dx) <= P.sr + 2;
    for (; y < lim; y++) {                               // niveau de halo tramé (Bayer) -> couleur de la table de la ligne
      int l = (gx * R.glY[y] + (AL_BAY[((y & 3) << 2) | (x & 3)] << 8)) >> 12; uint16_t c = R.skT[y][l > 15 ? 15 : l];
      if (sunCol) { int dy = y - sy, d2 = dx * dx + dy * dy; if (d2 <= r2) c = sunC; else if (d2 <= r2e) c = alBlend(c, sunC, 8); }   // soleil, bord adouci
      p[y * SCREEN_W] = c;
    }
    lim = min(e, tB); for (; y < lim; y++) p[y * SCREEN_W] = R.ly[0][y];
    lim = min(e, tC); for (; y < lim; y++) p[y * SCREEN_W] = R.ly[1][y];
    lim = min(e, tG); for (; y < lim; y++) p[y * SCREEN_W] = R.ly[2][y];
    for (; y < e; y++) { int d = y - tG; p[y * SCREEN_W] = R.gd[d > 63 ? 63 : d]; }
  }
}

void alSkyDecor(int sTop, int cx, float t) {             // etoiles, lune, nuages filants, montgolfieres, etoile filante
  if (sTop >= 120) return;                               // tout le decor du ciel est dans la moitie haute
  const AlPal &P = *alP; const uint32_t ms = millis();
  for (int i = 0; i < P.stars; i++) {                    // scintillement : onde triangle en entiers
    int ph = (int)((ms * (3 + (i & 3)) / 8 + i * 97) & 511), tw = ph < 256 ? ph : 511 - ph;
    alG->drawPixel(alR->stX[i], alR->stY[i], alBlend(alR->stC[i], 0xFFFF, 4 + tw * 10 / 256));
  }
  if (alLvl < 2) { alG->fillCircle(52, 34 - sTop, 8, alBlend(alC(P.sun), alSkyAt(52, 34), 4)); alG->fillCircle(56, 31 - sTop, 8, alSkyAt(56, 31)); }   // croissant de lune
  for (int k = 0; k < 4; k++) {                          // longs nuages fins
    int w = 70 + k * 26, y = 48 + k * 15 + (alLvl == 2 ? -12 : 0);
    int x = (int)fmodf(k * 137 + t * (2.0f + k) - cx * 0.02f + 100000, 520) - 110;
    uint16_t c = alBlend(alSkyAt(x + w / 2, y), alC(P.glow), 6);
    alG->fillRect(x, y - sTop, w, 2, c); alG->fillRect(x + 14, y + 2 - sTop, w - 34, 1, c);
  }
  for (int k = 0; k < 3; k++) {                          // montgolfieres
    int x = (int)fmodf(k * 170 + 60 + t * (2.0f + k * 0.7f) - cx * 0.03f + 100000, 430) - 55, y = 22 + k * 20 + (int)(alSin(t * 0.6f + k * 2) * 4), r = 8 - k * 2;
    uint16_t c = alBlend(alSkyAt(x, y), alC(P.lay), 9);
    alG->fillCircle(x, y - sTop, r, c); alG->fillTriangle(x - r + 1, y + 2 - sTop, x + r - 1, y + 2 - sTop, x, y + r + 3 - sTop, c);
    alG->drawFastVLine(x - 1, y + r + 2 - sTop, 3, c); alG->drawFastVLine(x + 1, y + r + 2 - sTop, 3, c); alG->fillRect(x - 1, y + r + 5 - sTop, 3, 2, c);
  }
  float u = fmodf(t, 9.0f) / 0.8f;                       // etoile filante toutes les 9 s
  if (u < 1) { int x = 230 - (int)(u * 110), y = 18 + (int)(u * 36); alG->drawLine(x, y - sTop, x + 14, y - 5 - sTop, alBlend(alSkyAt(x, y), 0xFFFF, (int)(14 * (1 - u)))); }
}

void alPine(int x, int yb, int H, uint16_t c) {          // sapin en silhouette
  alG->fillRect(x - 1, yb - 4, 2, 5, c);
  for (int k = 0; k < 3; k++) { int w = H / 3 + 1 - k * 2, y0 = yb - 3 - k * H / 4; alG->fillTriangle(x - w, y0, x + w, y0, x, y0 - H / 2, c); }
}

void alWorld(int sTop, int cx, int cy, float t) {        // decor pose sur le sol, corde, rochers, diamants
  const AlPal &P = *alP; const uint16_t dk = alMix(P.deep, P.gnd, 0.15f);
  (void)cx; (void)cy; const int c0 = alCi(alCX - 60), c1 = alCi(alCX + SCREEN_W / AL_Z + 60);
  for (int c = c0; c <= c1; c++) {
    const AlCh &h = alR->ch[c]; const int x0 = c * AL_CL;
    if (h.t == AT_DUNE || h.t == AT_ROCKS || h.t == AT_ARC) for (int k = 0; k < 3; k++) {     // sapins
      uint32_t hh = gHash(c * 13 + k * 101 + alLvl); if (hh % 3 == 0) continue;
      int u = 40 + k * 160 + hh % 60; if (h.t == AT_ROCKS && k == 1) continue;
      int sx = alSX(x0 + u); if (sx < -20 || sx >= SCREEN_W + 20) continue;
      int gy = alR->cG[constrain(sx, 0, SCREEN_W - 1)]; if (gy < 32000 && gy - 46 < sTop + AL_SH && gy + 3 > sTop) alPine(sx, gy - sTop + 2, (int)((20 + (hh >> 8) % 22) * AL_Z), dk);   // sol lu dans le cache des colonnes
    }
    if (h.t == AT_DOUBLE) {                               // corde a fanions sur deux poteaux
      int xa = alSX(x0 + 130), xb = alSX(x0 + 500), ya = alSY(alRope(x0 + 130)) - sTop, yb = alSY(alRope(x0 + 500)) - sTop;
      for (int s = 0; s < 2; s++) { int xp = s ? xb : xa, yp = s ? yb : ya; float g = alGround(x0 + (s ? 500 : 130)); int gy = g < AL_NONE ? alSY(g) - sTop : yp + 70;
        alG->fillRect(xp - 1, yp - 6, 3, gy - yp + 6, dk); alG->fillRect(xp - 2, yp - 8, 5, 2, dk); }
      alG->drawLine(xa, ya, xb, yb, dk);
      static const uint8_t *const FC[3] = { AL_PAL[0].acc, AL_PAL[1].rim, AL_PAL[2].rim };
      for (int u = 142; u < 495; u += 16) {
        int fx = alSX(x0 + u); if (fx < -8 || fx > SCREEN_W + 8) continue;
        int fy = alSY(alRope(x0 + u)) - sTop + 1; int sw = (int)(alSin(t * 3 + u * 0.3f) * 1.5f);
        uint16_t fc = ((u / 16) % 3 == 0) ? alC(P.acc) : alMix(FC[(u / 16) % 3], P.deep, 0.35f);
        alG->fillTriangle(fx - 2, fy, fx + 2, fy, fx + sw, fy + 5, fc);
      }
    }
    for (int k = 0; k < h.nr; k++) {                      // rochers anguleux
      int rx = x0 + h.ru[k], r = (int)(h.rh[k] * AL_Z + 0.5f); float g = alGround(rx); if (g >= AL_NONE) continue;
      int sx = alSX(rx), gy = alSY(g) - sTop + 2;
      alG->fillTriangle(sx - r, gy, sx + r * 4 / 5, gy, sx - r / 5, gy - r, dk); alG->fillTriangle(sx - r / 2, gy, sx + r, gy, sx + r * 3 / 10, gy - r * 3 / 4, dk);
      alG->drawLine(sx - r / 5, gy - r, sx + r * 3 / 10, gy - r * 3 / 4, alMix(P.deep, P.rim, 0.35f));
    }
    for (int k = 0; k < h.ni; k++) if (!(h.got >> k & 1)) {   // diamants
      int sx = alSX(x0 + h.iu[k]), sy = alSY(h.y0 + h.iy[k]) - sTop; if (sx < -12 || sx > SCREEN_W + 12) continue;
      int s = 3 + (int)(alSin(t * 5 + k + c) * 1.0f + 0.5f);
      alG->drawCircle(sx, sy, s + 3, alMix(P.glow, P.mist, 0.5f));
      alG->fillTriangle(sx, sy - s - 2, sx - s, sy, sx + s, sy, alC(P.sun)); alG->fillTriangle(sx - s, sy, sx + s, sy, sx, sy + s + 2, alC(P.glow));
      alG->drawPixel(sx - 1, sy - s + 1, 0xFFFF);
    }
  }
}

// cou du glisseur dans le monde (meme rotation / ecrasement que le sprite)
void alNeck(float &wx, float &wy) {
  const float spin = alCrash > 0.5f ? (alCrash - 0.5f) * 16 : 0, th = alA + spin - RIDER_ANG, c = alCos(th), s = alSin(th);
  const float k = max(alChg / 0.6f, max(0.0f, alSquash)), sq = 1.0f / (1 - 0.18f * k);
  const float a = (RIDER_NX2 - RIDER_AX2) * 0.5f * AL_RZ / AL_Z, b = (RIDER_NY2 - RIDER_AY2) * 0.5f * AL_RZ / sq / AL_Z;
  wx = alX + c * a - s * b; wy = alY + s * a + c * b;
}
void alScarfReset() { float x, y; alNeck(x, y); for (int i = 0; i < AL_SC; i++) { alScX[i] = x - i; alScY[i] = y + i; } }
void alScarfStep(float dt) {                             // chaque point reste ou il etait dans le monde puis est ramene a un pas du precedent : l'echarpe traine derriere le mouvement
  alNeck(alScX[0], alScY[0]);
  const float seg = 4.2f / AL_Z;
  for (int i = 1; i < AL_SC; i++) {
    alScY[i] += 70 * dt;                                 // un peu de poids : elle retombe quand on va lentement
    float dx = alScX[i] - alScX[i - 1], dy = alScY[i] - alScY[i - 1], d = sqrtf(dx * dx + dy * dy);
    if (d < 0.01f) { dx = -1; dy = 0; d = 1; }
    alScX[i] = alScX[i - 1] + dx * seg / d; alScY[i] = alScY[i - 1] + dy * seg / d;
  }
}
void alScarf(int sTop, float t) {                        // echarpe de la couleur d'accent du niveau, ondule au vent (plus fort vers le bout)
  const uint16_t col = alC(alP->acc);
  int px = alSX(alScX[0]), py = alSY(alScY[0]) - sTop;
  if (py < -60 || py > AL_SH + 60) return;
  for (int i = 1; i < AL_SC; i++) {
    const float w = alSin(t * 15 - i * 1.2f) * 0.45f * i;
    const int qx = alSX(alScX[i]), qy = alSY(alScY[i]) - sTop + (int)w;
    alG->drawLine(px, py, qx, qy, col); if (i < 5) alG->drawLine(px, py + 1, qx, qy + 1, col);
    px = qx; py = qy;
  }
}

void alRider(int sTop, int cx, int cy, float t) {       // silhouette du snowboarder, planche posee sur la pente ; bords adoucis (4 echantillons par pixel)
  const AlPal &P = *alP; (void)t;
  const float spin = alCrash > 0.5f ? (alCrash - 0.5f) * 16 : 0, th = alA + spin - RIDER_ANG;   // la planche de l'image prend l'angle de la pente
  const float k = max(alChg / 0.6f, max(0.0f, alSquash)), sq = 1.0f / (1 - 0.18f * k);            // accroupi pendant la charge / a l'atterrissage
  (void)cx; (void)cy; const int ox = alSX(alX), oy = alSY(alY) - sTop, RR = 48;
  if (oy < -RR || oy > AL_SH + RR) return;               // le glisseur n'est pas dans cette bande
  const float c = alCos(th), s = alSin(th);              // rotation inverse en virgule fixe 16.16 : ecran -> image (stockee a 2x)
  int bx0 = 999, by0 = 999, bx1 = -999, by1 = -999;      // cadre du sprite tourne (4 coins) : on ne balaie que lui
  for (int q = 0; q < 4; q++) {
    const float a = ((q & 1) ? RIDER_W2 - RIDER_AX2 : -RIDER_AX2) * 0.5f * AL_RZ, b = ((q & 2) ? RIDER_H2 - RIDER_AY2 : -RIDER_AY2) * 0.5f * AL_RZ / sq;
    const int px = (int)(c * a - s * b), py = (int)(s * a + c * b);
    bx0 = min(bx0, px); bx1 = max(bx1, px); by0 = min(by0, py); by1 = max(by1, py);
  }
  const int x0 = max(0, ox + bx0 - 1), x1 = min(SCREEN_W - 1, ox + bx1 + 1), y0 = max(0, oy + by0 - 1), y1 = min(AL_SH - 1, oy + by1 + 1);
  const float st = 2 / AL_RZ;                            // pas dans l'image (stockee a 2x) pour un pixel d'ecran
  const int32_t C = (int32_t)(c * st * 65536), Sn = (int32_t)(s * st * 65536), Cy = (int32_t)(c * st * sq * 65536), Sy = (int32_t)(s * st * sq * 65536);
  const int32_t hxX = C / 2, hxY = -Sy / 2, hyX = Sn / 2, hyY = Cy / 2;   // demi-pixel d'ecran en x et en y
  const int32_t AX = (int32_t)(RIDER_AX2 * 65536), AY = (int32_t)(RIDER_AY2 * 65536);
  const uint16_t body = alMix(P.deep, AL_BLACK, 0.45f);
  uint16_t *buf = alG->getBuffer();
  auto rb = [](int32_t u, int32_t v) -> int { int ix = u >> 16, iy = v >> 16; if ((unsigned)ix >= RIDER_W2 || (unsigned)iy >= RIDER_H2) return 0; return (RIDER_BITS[iy * RIDER_STRIDE + (ix >> 3)] >> (7 - (ix & 7))) & 1; };
  for (int y = y0; y <= y1; y++) {
    const float fdx = x0 + 0.25f - ox, fdy = y + 0.25f - oy;
    int32_t sx = AX + (int32_t)(fdx * C) + (int32_t)(fdy * Sn), sy = AY - (int32_t)(fdx * Sy) + (int32_t)(fdy * Cy);
    uint16_t *d = buf + y * SCREEN_W;
    for (int x = x0; x <= x1; x++, sx += C, sy -= Sy) {
      int cov = rb(sx, sy) + rb(sx + hxX, sy + hxY) + rb(sx + hyX, sy + hyY) + rb(sx + hxX + hyX, sy + hxY + hyY);
      if (cov) d[x] = cov == 4 ? body : alBlend(d[x], body, cov * 4);
    }
  }
  if (alChg > 0) {                                       // jauge de charge au-dessus de la tete
    int gx = ox - 12, gy = oy - 42; alG->drawRect(gx, gy, 24, 4, body); alG->fillRect(gx + 1, gy + 1, (int)(22 * alChg / 0.6f), 2, alC(alChg >= 0.6f ? P.sun : P.glow));
  }
}

void alFore(int sTop, int cx, float t) {                 // avant-plan sombre et rapide, lignes de vitesse, poussieres en suspension
  const AlPal &P = *alP; const uint16_t fc = alMix(P.deep, AL_BLACK, 0.35f);
  const int fx0 = (int)(cx * 1.4f);
  for (int k = fx0 / 230 - 1; sTop + AL_SH > SCREEN_H - 92 && k <= (fx0 + SCREEN_W) / 230 + 1; k++) {   // avant-plan : bas de l'ecran seulement
    uint32_t h = gHash(k * 97 + 5 + alLvl * 31); if (h % 3) continue;
    int sx = k * 230 + (int)((h >> 4) % 90) - fx0, H = 34 + (int)((h >> 10) % 40), yb = SCREEN_H + 4 - sTop;
    if (h >> 20 & 1) alPine(sx, yb, H + 16, fc);
    else { alG->fillTriangle(sx - H, yb, sx + H / 2, yb, sx - H / 4, yb - H * 2 / 3, fc); alG->fillTriangle(sx - H / 3, yb, sx + H, yb, sx + H / 4, yb - H / 2, fc); alG->fillRect(sx - H / 3, yb - H * 2 / 3 + 4, H / 3, H, fc); }
  }
  if (alPh == 1 && alV > 270) for (int i = 0; i < 7 && i < (int)((alV - 270) / 22); i++) {
    uint32_t h = gHash(i * 13 + 977); int y = 24 + (int)(h % 190), x = SCREEN_W - (int)fmodf(t * 900 + (h >> 8) % 400, 460);
    alG->drawFastHLine(x, y - sTop, 30 + (h >> 16) % 30, alBlend(alC(P.mist), 0xFFFF, 4));
  }
  for (int i = 0; i < 18; i++) {
    uint32_t h = gHash(i * 31 + 7); if ((int)(t * 2 + i) % 5 == 0) continue;
    { int y0 = 30 + (int)((h >> 9) % 190); if (y0 + 6 < sTop || y0 - 6 >= sTop + AL_SH) continue; }
    int x = (int)fmodf((h % 400) + t * (6 + i % 5) - cx * 0.5f + 100000, 400) - 40, y = 30 + (int)((h >> 9) % 190) + (int)(alSin(t + i) * 6);
    alG->drawPixel(x, y - sTop, alC(P.rim));
  }
}

void alText(int n, int cx, int y, const char *s, uint16_t c, int sTop) {   // texte centre avec ombre
  int x = cx - textW(*alG, n, s) / 2;
  alG->setTextColor(alC(alP->deep)); gfxPrintAt(*alG, n, x + 1, y + 1 - sTop, s);
  alG->setTextColor(c); gfxPrintAt(*alG, n, x, y - sTop, s);
}
void alHud(int sTop) {
  const AlPal &P = *alP; char b[40]; const uint16_t lc = alC(P.sun);
  if (alPh == 1) {
    if (sTop) return;                                    // le HUD de course est dans la bande du haut
    int s = (int)ceilf(alT); snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60); alText(2, SCREEN_W / 2, 6, b, lc, sTop);
    snprintf(b, sizeof b, "%d", (int)alScoreF); alText(1, SCREEN_W - 8 - textW(*alG, 1, b) / 2, 6, b, lc, sTop);
    if (alCombo > 0) { snprintf(b, sizeof b, "x%d", alCombo + 1); alText(1, SCREEN_W - 8 - textW(*alG, 1, b) / 2, 20, b, alC(P.glow), sTop); }
    alG->fillTriangle(10, 8 - sTop, 6, 12 - sTop, 14, 12 - sTop, lc); alG->fillTriangle(6, 12 - sTop, 14, 12 - sTop, 10, 17 - sTop, alC(P.glow));
    snprintf(b, sizeof b, "%d", alDia); alG->setTextColor(lc); gfxPrintAt(*alG, 1, 19, 6 - sTop, b);
    if (alMsgT > 0) alText(2, SCREEN_W / 2, 38, alMsg, alC(P.glow), sTop);
  } else if (alPh == 0) {
    alText(5, SCREEN_W / 2, 18, "ALTO", lc, sTop);
    snprintf(b, sizeof b, "<  %s  >", P.name); alText(2, SCREEN_W / 2, 78, b, alC(P.glow), sTop);
    snprintf(b, sizeof b, "niveau %d / 3   record %d", alLvl + 1, alBest[alLvl]); alText(1, SCREEN_W / 2, 100, b, lc, sTop);
    alText(1, SCREEN_W / 2, 196, "SAUT tenu = plus haut   G / D en l'air = saltos", lc, sTop);
    alText(1, SCREEN_W / 2, 212, "SAUT = partir   G + D 1 s = quitter", alC(P.glow), sTop);
  } else {
    alG->fillRoundRect(62, 44, 196, 150, 10, alMix(P.deep, P.gnd, 0.3f)); alG->drawRoundRect(62, 44, 196, 150, 10, alC(P.rim));
    alText(2, SCREEN_W / 2, 54, "FIN DE COURSE", alC(P.glow), sTop);
    snprintf(b, sizeof b, "Score  %d", (int)alScoreF); alText(2, SCREEN_W / 2, 84, b, lc, sTop);
    snprintf(b, sizeof b, "Diamants %d   Saltos %d   Chutes %d", alDia, alFlips, alFalls); alText(1, SCREEN_W / 2, 116, b, lc, sTop);
    snprintf(b, sizeof b, (int)alScoreF >= alBest[alLvl] ? "NOUVEAU RECORD !" : "Record  %d", alBest[alLvl]); alText(1, SCREEN_W / 2, 136, b, alC(P.glow), sTop);
    alText(1, SCREEN_W / 2, 170, "SAUT = rejouer", lc, sTop);
  }
}

// envoi : fenetre plein ecran ouverte une fois par image (RAMWR), puis chaque bande part en 2 transferts DMA a la suite, sans attendre
void alDmaWait(int keep) { while (alQ > keep) { spi_transaction_t *r; spi_device_get_trans_result(dDev, &r, portMAX_DELAY); alQ--; } }
void alSendHalf(int h, int sTop) {
  uint16_t *b = fb.getBuffer() + h * SCREEN_W * AL_SH;
  if (!dDev) { tft.drawRGBBitmap(0, sTop, b, SCREEN_W, AL_SH); return; }   // repli sans DMA
  uint32_t *q = (uint32_t *)b;
  for (int i = 0; i < SCREEN_W * AL_SH / 2; i++) { uint32_t v = q[i]; q[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu); }   // octets dans l'ordre de l'ecran
  if (sTop == 0) {
    alDmaWait(0); digitalWrite(TFT_CS, HIGH);
    int x0 = tft.ox(), x1 = x0 + SCREEN_W - 1, y0 = tft.oy(), y1 = y0 + SCREEN_H - 1;
    uint8_t ca[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 }, ra[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 }, c;
    digitalWrite(TFT_CS, LOW);
    c = 0x2A; dTx(&c, 1, 0); dTx(ca, 4, 1);
    c = 0x2B; dTx(&c, 1, 0); dTx(ra, 4, 1);
    c = 0x2C; dTx(&c, 1, 0);
    gpio_set_level((gpio_num_t)TFT_DC, 1);
  }
  for (int k = 0; k < 2; k++) {
    spi_transaction_t &t = alTr[h * 2 + k]; memset(&t, 0, sizeof t); t.length = SCREEN_W * 30 * 16; t.tx_buffer = b + k * SCREEN_W * 30;
    spi_device_queue_trans(dDev, &t, portMAX_DELAY); alQ++;
  }
}

void alDraw() {
  float t = millis() / 1000.0f;
  if (alPh > 0) alSunY = alP->sy + alP->drift * (AL_TIME - alT) / AL_TIME;   // le soleil se couche pendant la course (la lune monte)
  alGlowY();
  const int sh = alShake > 0 ? (int)(alSin(t * 70) * alShake) : 0;
  alCX = alCamX + sh / AL_Z; alCY = alCamY + sh / 2 / AL_Z;
  const int cx = (int)(alCX * AL_Z), cy = (int)(alCY * AL_Z);   // camera en pixels d'ecran (parallaxe du ciel, des montagnes, de l'avant-plan)
  alColumns(cx, cy);
  for (int sTop = 0; sTop < SCREEN_H; sTop += AL_SH) {
    const int h = (sTop / AL_SH) & 1;
    uint32_t t1 = micros(); alDmaWait(2); alPerf[1] += micros() - t1;   // la moitie a dessiner a fini de partir
    alG = alHf[h];
    uint32_t t0 = micros();
    alComposite(sTop, cx, cy);
    alPerf[0] += micros() - t0;
    alSkyDecor(sTop, cx, t);
    alWorld(sTop, cx, cy, t);
    for (int i = 0; i < AL_NP; i++) { const AlPt &q = alR->p[i]; if (q.life <= 0) continue; int x = alSX(q.x), y = alSY(q.y) - sTop; if (q.life > 0.25f) alG->fillRect(x, y, 2, 2, q.c); else alG->drawPixel(x, y, q.c); }
    if (alFall <= 0 || (alY - alCamY) * AL_Z < SCREEN_H + 40) { alScarf(sTop, t); alRider(sTop, cx, cy, t); }
    alFore(sTop, cx, t);
    alHud(sTop);
    t1 = micros(); alSendHalf(h, sTop); alPerf[1] += micros() - t1;
  }
}

void alSpawn(float x, float y, float vx, float vy, float life, uint16_t c) { AlPt &q = alR->p[alPi]; alPi = (alPi + 1) % AL_NP; q.x = x; q.y = y; q.vx = vx; q.vy = vy; q.life = life; q.c = c; }
void alBurst(int n, float sp, uint16_t c) { for (int i = 0; i < n; i++) { float a = random(0, 628) / 100.0f; alSpawn(alX, alY - 4, alCos(a) * sp * random(30, 100) / 100, alSin(a) * sp * random(30, 100) / 100 - sp * 0.4f, 0.5f + random(0, 40) / 100.0f, c); } }
void alSay(const char *m) { strncpy(alMsg, m, sizeof alMsg - 1); alMsg[sizeof alMsg - 1] = 0; alMsgT = 1.3f; }

void alDoCrash() { alCrash = 1.1f; alCombo = 0; if (alPh == 1) alFalls++; alShake = 5; alChg = 0; snd(SND_DIE); alBurst(14, 120, alC(alP->rim)); }
void alTouch() {                                         // on se pose : angle correct = atterrissage (saltos comptes), sinon chute
  const float sa = atanf(alSlope(alX)), d = alWrap(alA - sa);
  alGnd = true;
  if (alCrash <= 0 && fabsf(d) > 0.85f) { alV = max(60.0f, alVX * 0.4f); alA = sa; alDoCrash(); return; }
  alV = max(alVX * alCos(sa) + alVY * alSin(sa), 110.0f); alA = sa; alAV = 0;
  alSquash = min(1.0f, alVY / 700.0f); alBurst(6, 50, alC(alP->rim));
  if (alPh != 1 || alCrash > 0) return;
  int n = (int)((fabsf(alRot) + 0.9f) / (2 * PI)); char b[28];
  if (n > 0) {
    alCombo += n; alFlips += n; int pts = 100 * n * alCombo; alScoreF += pts;
    if (n == 1) snprintf(b, sizeof b, "%s +%d", alRot < 0 ? "SALTO ARRIERE" : "SALTO AVANT", pts); else snprintf(b, sizeof b, "%d SALTOS +%d", n, pts);
    alSay(b); snd(SND_STAR);
  } else if (alAir > 0.9f) { int pts = 30 * (1 + alCombo); alScoreF += pts; snprintf(b, sizeof b, "BEAU SAUT +%d", pts); alSay(b); }
}
void alRespawn() {                                       // apres une fosse : on repart sur le premier sol solide plus loin
  float x = alX; while (x < (AL_NCH - 1) * AL_CL && (alGround(x) >= AL_NONE || alGround(x + 30) >= AL_NONE)) x += 6;
  alX = x + 30; alY = alGround(alX); alGnd = true; alRopeOn = false; alV = 140; alA = atanf(alSlope(alX)); alAV = 0; alChg = 0; alCrash = 0;
  alCamY = alY - 128 / AL_Z; alScarfReset();
}
bool alRockHit() {
  int c0 = alCi(alX);
  for (int c = max(0, c0 - 1); c <= min(AL_NCH - 1, c0 + 1); c++) { const AlCh &h = alR->ch[c];
    for (int k = 0; k < h.nr; k++) { float wx = c * AL_CL + h.ru[k]; if (fabsf(alX - wx) < 9) { float g = alGround(wx); if (g < AL_NONE && alY > g - h.rh[k] + 3 && alY < g + 20) return true; } } }
  return false;
}
void alItems() {
  int c0 = alCi(alX);
  for (int c = max(0, c0 - 1); c <= min(AL_NCH - 1, c0 + 1); c++) { AlCh &h = alR->ch[c];
    for (int k = 0; k < h.ni; k++) if (!(h.got >> k & 1)) {
      float dx = c * AL_CL + h.iu[k] - alX, dy = h.y0 + h.iy[k] - (alY - 10);
      if (dx * dx + dy * dy < 18 * 18) { h.got |= 1 << k; alDia++; alScoreF += 25 * (1 + alCombo); snd(SND_COIN); alBurst(8, 90, alC(alP->sun)); }
    } }
}

void alPhys(float dt, bool bl, bool br, bool bj) {
  if (alFall > 0) { alFall -= dt; if (alFall <= 0) alRespawn(); return; }
  if (alCrash > 0) alCrash -= dt;
  if (alSquash > 0) alSquash -= dt * 4;
  if (alGnd) {
    const float a = atanf(alSlope(alX));
    if (alCrash > 0) alV = max(40.0f, alV - 260 * dt);
    else { alV += (AL_G * 0.55f * alSin(a) - (14 + 0.0012f * alV * alV)) * dt; alV = constrain(alV, 110.0f, 420.0f); }   // la pente accelere, l'air freine
    if (bj && alCrash <= 0) alChg = min(0.6f, alChg + dt);
    float vx = alV * alCos(a), vy = alV * alSin(a);
    if (!bj && alChg > 0) {                              // relache : saut, d'autant plus haut que la charge a dure
      float c = alChg / 0.6f; alChg = 0;
      const float up = constrain(-alSlope(alX) / 0.6f, 0.0f, 1.0f);   // terrain qui monte sous la planche = courbe de saut
      alVX = vx; alVY = vy - (180 + 180 * c) * (1 + 0.8f * up); alGnd = false; alRopeOn = false; alRot = 0; alAir = 0; alAV = 0; snd(SND_JUMP); alBurst(5, 40, alC(alP->rim)); return;
    }
    const float nx = alX + vx * dt, yb = alY + vy * dt + 0.5f * AL_G * dt * dt, gs = alSurfY(nx);
    if (gs >= AL_NONE || yb < gs - 0.8f) { alGnd = false; alRopeOn = false; alVX = vx; alVY = vy; alX = nx; alY = yb; alRot = 0; alAir = 0; alAV = 0; }   // le sol se derobe : on decolle
    else {
      alX = nx; alY = gs; alA += alWrap(atanf(alSlope(alX)) - alA) * min(1.0f, dt * 18);
      if (alV > 140 && alCrash <= 0 && random(0, 100) < 70) alSpawn(alX - 10 * alCos(alA), alY - 1, -alV * 0.15f + random(-20, 20), -30 - random(0, 60), 0.35f + random(0, 30) / 100.0f, alC(alP->rim));   // gerbe de neige / sable
    }
  } else {
    if (!bj) alChg = 0;
    alAir += dt;
    const float px = alX, py = alY;
    alVY = min(900.0f, alVY + AL_G * dt); alX += alVX * dt; alY += alVY * dt;
    int dir = (bl && br) || alCrash > 0 ? 0 : (br ? 1 : 0) - (bl ? 1 : 0);
    alAV += (dir * 8.5f - alAV) * min(1.0f, dt * 12); alA += alAV * dt; alRot += alAV * dt;
    if (!dir) { float va = constrain(atan2f(alVY, alVX), -0.7f, 0.9f); alA += alWrap(va - alA) * min(1.0f, dt * 1.6f); }   // sans rotation demandee, la planche suit doucement la trajectoire
    const float r0 = alRope(px), r1 = alRope(alX);
    if (alVY > 0 && r0 < AL_NONE && r1 < AL_NONE && py <= r0 + 1 && alY >= r1) { alY = r1; alRopeOn = true; alTouch(); }
    else { float g = alGround(alX);
      if (g < AL_NONE && alY >= g) {
        if (py <= g + 4) { alY = g; alRopeOn = false; alTouch(); }
        else alY = AL_NONE / 2;                          // on arrive par le flanc d'une fosse : chute
      } }
    if (!alGnd && alY > alBaseY(alX) + 280) { alFall = 0.9f; if (alPh == 1) alFalls++; alCombo = 0; alChg = 0; snd(SND_DIE); if (alPh == 1) alSay("CHUTE"); }
  }
  if (alCrash <= 0 && alFall <= 0 && alRockHit()) { if (alPh == 1) alSay("AIE !"); alDoCrash(); }
}

void alCam(float dt) {
  if (alFall > 0) return;                                // pendant une chute la camera reste : on voit le glisseur disparaitre
  alLook += ((alV - 150) * 0.12f - alLook) * min(1.0f, dt * 2);   // a grande vitesse on voit plus loin devant
  alCamX = alX - (104 - alLook) / AL_Z;
  float ga = alGround(alX + 210); if (ga >= AL_NONE) ga = alBaseY(alX + 210);
  alCamY += ((alY * 0.65f + ga * 0.35f - 128 / AL_Z) - alCamY) * min(1.0f, dt * 3.2f);   // retard vertical : vertige dans les descentes
  alCamYS += (alCamY - alCamYS) * min(1.0f, dt * 0.5f);
}

void alNew() {                                           // (re)tire le parcours du niveau et replace le glisseur au depart
  alGen(); alPalInit();
  alX = 60; alY = alGround(alX); alGnd = true; alRopeOn = false; alV = 0; alA = atanf(alSlope(alX)); alAV = 0; alChg = 0; alRot = 0; alAir = 0;
  alCrash = 0; alFall = 0; alSquash = 0; alT = AL_TIME; alLook = 0; alCamX = alX - 104 / AL_Z; alCamY = alY - 128 / AL_Z; alCamYS = alCamY; alShake = 0; alMsgT = 0;
  alFpsMin = 999; alScoreF = 0; alDia = 0; alFlips = 0; alFalls = 0; alCombo = 0; alAuto = 0;
  for (int i = 0; i < AL_NP; i++) alR->p[i].life = 0;
  alScarfReset();
}
// double tampon DMA partage (Alto, Drift) : 2 canevas 320x60 poses sur les moities de fb ; dessiner dans alG = alHf[h], envoyer avec alSendHalf(h, sTop)
void alDmaBegin() {
  if (!twoP) stopEspNow();                               // a deux (drift) ESP-NOW reste actif : fb sert de tampon DMA, rien a allouer
  static AlHalf h0(fb.getBuffer()), h1(fb.getBuffer() + SCREEN_W * AL_SH); alHf[0] = &h0; alHf[1] = &h1; alQ = 0;
  // a deux (drift) : PAS de DMA ; mesure 2026-10-22 : avec le DMA de l'ecran, l'hote ne peut plus emettre (esp_now_send -> NO_MEM) et l'adversaire
  // se deconnecte en 3 s ; sans DMA tout passe (envoi bloquant de chaque bande, moins d'images/s).
  if (twoP) tft.setSPISpeed(80000000);
  else {
    if (!dOb) dOb = fb.getBuffer();                      // dSpiStart n'alloue pas son tampon de 77 Ko : on envoie directement depuis fb
    if (!dSpiStart()) { if (dOb == fb.getBuffer()) dOb = nullptr; tft.setSPISpeed(80000000); }
  }
  if (AL_SIN[64] == 0) for (int i = 0; i <= 256; i++) AL_SIN[i] = sinf(i * (2 * PI / 256));
}
void alDmaEnd() {
  if (dDev) { alDmaWait(0); digitalWrite(TFT_CS, HIGH); }
  dSpiStop(); if (dOb == fb.getBuffer()) dOb = nullptr;  // rend le SPI aux menus ; fb n'est pas a nous
  alG = &fb;
}
void startAlto() {
  alDmaBegin();
  if (!alR) alR = (AlRun *)calloc(1, sizeof(AlRun));
  if (!alR) { altoFree(); tft.setSPISpeed(40000000); state = ST_MODE; drawMode(); return; }   // pas assez de memoire : retour au menu
  alPh = 0; alQuit = 0; alPL = alPR = true; alJLock = true; alNew(); state = ST_ALTO;
}
void altoFree() { alDmaEnd(); free(alR); alR = nullptr; }

bool altoStep(bool bl, bool br, bool bj, bool je) {     // false = on quitte
  static unsigned long last = 0; unsigned long now = millis();
  float dt = (now - last) / 1000.0f; last = now; if (dt <= 0) dt = 0.02f; if (dt > 0.05f) dt = 0.05f;
  { static unsigned long t0 = 0; static int n = 0; n++;                   // images/s sur le port serie (si un moniteur lit)
    if (now - t0 >= 1000) { alFps = n; if (alPh == 1 && n < alFpsMin) alFpsMin = n; alPc[0] = alPerf[0] / 1000 / n; alPc[1] = alPerf[1] / 1000 / n; n = 0; t0 = now; alPerf[0] = alPerf[1] = 0; }
    if (Serial.available()) {                            // mesure a la demande : on ne repond que si le PC ecrit (un print spontane bloque l'USB quand personne ne lit)
      while (Serial.available()) Serial.read();
      Serial.printf("[ALTO] fps=%d min=%d comp=%lums blit=%lums dma=%d heap=%u t=%lus\n", alFps, alFpsMin, (unsigned long)alPc[0], (unsigned long)alPc[1], dDev ? 1 : 0, (unsigned)ESP.getFreeHeap(), now / 1000);
    } }
  if (bl && br) { alQuit += dt; if (alQuit > 1.0f) return false; } else alQuit = 0;
  const bool eL = bl && !alPL, eR = br && !alPR; alPL = bl; alPR = br;
  if (!bj) alJLock = false;
  bool j = bj && !alJLock;
#ifdef ALTO_BENCH
  if (alPh == 0) { alPh = 1; alJLock = false; }
  if (alPh == 1) { static float ap = 0; if (alGnd && ap <= 0 && (alGround(alX + 70) >= AL_NONE || alRope(alX + 40) < AL_NONE)) ap = 0.35f; ap -= dt; j = bj = ap > 0; bl = false; br = !alGnd; }
#endif
  if (alPh == 0) {
    if ((eL && !br) || (eR && !bl)) { alLvl = (alLvl + (eR ? 1 : 2)) % 3; alNew(); }
    if (je && !alJLock) { alPh = 1; alJLock = true; alSay("GO !"); }
  } else if (alPh == 1) {
    alT -= dt;
    alPhys(dt, bl, br, j); if (alFall <= 0) { alItems(); alScoreF += alV * dt * 0.1f; }
    if (alT <= 0) { alT = 0; alPh = 2; alJLock = true; if ((int)alScoreF > alBest[alLvl]) alBest[alLvl] = (int)alScoreF; snd(SND_WIN); }
  } else {                                               // resultats : le glisseur continue seul (saute les fosses)
    if (alGnd && alAuto <= 0 && alGround(alX + 60) >= AL_NONE) alAuto = 0.3f;
    alAuto -= dt;
    alPhys(dt, false, false, alAuto > 0);
    if (je && !alJLock) { alNew(); alPh = 0; alJLock = true; }
  }
  for (int i = 0; i < AL_NP; i++) { AlPt &q = alR->p[i]; if (q.life <= 0) continue; q.life -= dt; q.vy += 260 * dt; q.x += q.vx * dt; q.y += q.vy * dt; }
  alScarfStep(dt);
  alCam(dt);
  if (alShake > 0) alShake = max(0.0f, alShake - dt * 12);
  if (alMsgT > 0) alMsgT -= dt;
  alDraw();
  return true;
}
