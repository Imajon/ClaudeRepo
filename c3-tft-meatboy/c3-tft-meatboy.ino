/*
 * c3-tft-meatboy  V13 (ESP32-C3 + ST7789 IPS 320x240)
 * + mode 2 joueurs (course + fantome) via ESP-NOW.
 * + recompenses (confettis), combos, stats/score sauvegardes.
 * + pieges dynamiques : pics surgissants, blocs qui tombent, drapeau fuyard.
 * + eclaboussures, deverrouillage progressif, plateformes fragiles (>= niv 11).
 * + Pong gauche/droite (solo IA ou a deux via ESP-NOW) et Casse-brique (solo, bonus), accessibles apres le choix 1/2 joueurs.
 * + Quiz CM2 (maths/francais/anglais) entre les niveaux : sac melange sans
 *   repetition, reponses interverties si redemandee, ecran de resultat avec
 *   stats par matiere, cape de recompense (drapee dans le dos, jamais dans un
 *   obstacle) qui grandit et le corps du joueur qui se teinte avec les
 *   bonnes reponses d'affilee.
 * + Niveaux 1-50 diversifies par dizaine (1-10 dessines a la main, 11-20
 *   puits/cheminees enchainees, 21-30 briques fragiles rouges, 31-40
 *   obstacles fantomes, 41-50 cumul) + palette (fond/blocs) propre a chaque
 *   dizaine.
 * Plateforme de precision facon Super Meat Boy.
 *   - 3 boutons : GPIO2 = GAUCHE , GPIO1 = DROITE , GPIO3 = SAUT
 *   - Son : buzzer passif/piezo sur GPIO9 (+ GND)
 *   - Menus au bouton : SELECTION de niveau (recap etoiles) + fin de niveau
 *     (Continuer / Rejouer / Niveaux).
 *   - Etoiles (3 par niveau) = note de reussite, sauvegardee (NVS).
 *   - Camera verticale : niveaux plus hauts que l'ecran (puits, ascensions).
 *   - Types : chaine / escalier / ilots / cheminee / PUITS (wall-jump alterne
 *     pour sortir) / ASCENSION (montee entiere) + tronconneuses.
 *
 * Cablage ecran ST7789 (SPI) :
 *   SCLK -> GPIO4 , MOSI -> GPIO6 , CS -> GPIO7 , DC -> GPIO5 ,
 *   RST -> GPIO10 , BLK -> 3V3 , VCC -> 3V3 , GND -> GND
 * Boutons : une patte sur la pin, l'autre sur GND (INPUT_PULLUP).
 * NB : GPIO2 est une pin de boot -> ne pas la maintenir a l'allumage.
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Preferences.h>
#include <LittleFS.h>         // BRIQUE : niveaux recus gardes dans la partition "spiffs" (896 Ko, inutilisee jusque-la)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "driver/spi_master.h"   // desert : envoi des images en DMA (le CPU dessine pendant que le SPI transmet)
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "perso_sprites.h"       // heroine rouge + fantome violet animes (assets/perso.gif), generes par tools_perso.py
#include "cape_sprites.h"        // avatar a cape (assets/avatarCape.png) : 6 poses + images intermediaires, genere par tools_cape.py
#include "niveau1_bg.h"           // decor du niveau 1 (assets/niveau1.png), genere par tools_niveau1.py
#include "boss_sprites.h"        // boss de fin (assets/starWars.gif), genere par tools_boss.py
#include "sam5_sprites.h"        // samourai a chapeau (assets/samourai5.gif), genere par tools_sam5.py
struct SmImg { const uint8_t *px, *px2; int blend; const uint16_t *pal; int W, H, AX, AY; float sxs, k; };   // dessin de l avatar dans sa pose courante (avant les prototypes auto de l IDE)
struct EcSeg { int16_t x0, y0, x1, y1; uint16_t t0, t1, dr; uint8_t in; };      // un morceau de rayon : t0/t1 = distance depuis la source (1 px = 8 ms : 125 px/s), in = eclat, dr = longueur totale du rayon   // (declare ici : avant les prototypes auto de l'IDE)
struct EcWave { bool en; uint8_t nr; uint16_t start, ns, life; uint32_t ms; };       // en-tete d'une onde : ses morceaux sont ecS[start .. start + ns) ; en : onde d'ennemi
struct CapeAv { int node = 0, edge = 0, dir = 1, target = 0; bool moving = false; float p = 0, off = 0, voff = 0, lean = 0, vlean = 0, face = 1; };   // etat de l'avatar a cape (ici : avant les prototypes auto de l'IDE)
struct __attribute__((packed)) Msg { uint8_t type; uint8_t lvl; int16_t x, y; uint32_t t; int16_t x2; uint8_t b1, b2; uint16_t from; };   // message ESP-NOW multijoueur (declare ici : avant les prototypes auto de l'IDE, des fonctions le prennent en parametre)
#include "wizard_sprites.h"      // 6 sorciers animes (assets/wizard1..5 + 7.gif), generes par tools_wizard.py : 0 = ecran titre, 1..5 = questions en jeu
#include "mountain_sprites.h"    // bande de montagnes enneigees (assets/montagne.png), generee par tools_montagnes.py
#include "tree_sprites.h"        // 3 arbres de la planche (assets/arbres.png), generes par tools_sprites.py
#include <string.h>
#include "font_chunky.h"       // police "Chunky" (voir font/make_font.py) : Chunky9/15/22/30/44
#include "hand_levels_data.h"  // struct HandLevel : doit etre connue avant les prototypes auto-generes

#define TFT_SCLK 4
#define TFT_MOSI 6
#define TFT_CS   7
#define TFT_DC   5
#define TFT_RST  10
// ecran qui memorise la taille de texte "classique" (1..12) : les textes de menu sont
// dessines avec la police Chunky, dont la hauteur est choisie d'apres cette taille (voir fontFor)
void dSpiStop(void);   // envoi DMA (voir plus bas) : rend le SPI a Arduino
class MeatTFT : public Adafruit_ST7789 {
 public:
  using Adafruit_ST7789::Adafruit_ST7789;
  int csz = 1;
  void setTextSize(uint8_t s) { csz = s; Adafruit_ST7789::setTextSize(s); }
  void setTextSize(uint8_t sx, uint8_t sy) { csz = sx; Adafruit_ST7789::setTextSize(sx, sy); }
  void startWrite(void) { dSpiStop(); Adafruit_ST7789::startWrite(); }   // tout dessin direct (menus) reprend le SPI s'il etait en DMA
  int16_t ox() { return _xstart; }   // decalage de la fenetre (envoi DMA du desert)
  int16_t oy() { return _ystart; }
};
MeatTFT tft = MeatTFT(&SPI, TFT_CS, TFT_DC, TFT_RST);
bool dNoDma;   // envoi DMA impossible (memoire) : repli sur l'envoi Adafruit
#ifdef GAME_BENCH
unsigned long gT[6], gq;
#endif

#define SCREEN_W 320
#define SCREEN_H 240
#define STRIP_H  120       // rendu en 2 bandes -> buffer ~75 Ko (s'alloue meme avec le WiFi)
// canevas dont fillRect remplit par lignes et par mots de 32 bits (celui d'Adafruit passe colonne par colonne : ~10x plus lent)
class FastCanvas : public GFXcanvas16 {
 public:
  using GFXcanvas16::GFXcanvas16;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > WIDTH ? WIDTH : x + w, y1 = y + h > HEIGHT ? HEIGHT : y + h;
    if (w <= 0 || h <= 0 || x0 >= x1 || y0 >= y1) return;
    uint16_t *b = getBuffer(); uint32_t v = (uint32_t)c | ((uint32_t)c << 16);
    for (int yy = y0; yy < y1; yy++) {
      uint16_t *p = b + yy * WIDTH + x0; int n = x1 - x0;
      if (((uintptr_t)p & 2) && n > 0) { *p++ = c; n--; }
      uint32_t *q = (uint32_t *)p;
      for (int k = n >> 1; k > 0; k--) *q++ = v;
      if (n & 1) *(uint16_t *)q = c;
    }
  }
};
FastCanvas fb(SCREEN_W, STRIP_H);
// classement partage : types definis ici (avant tout prototype auto-genere)
#define LB_MAX 16
// entree = id, score, nom (6 car., complete par des espaces), skin + niveau d'evolution (avatar), rev = version de
// l'entree (+1 a chaque changement de nom/avatar : la plus recente gagne quand les tables se melangent)
struct __attribute__((packed)) LbEnt { uint16_t id; uint32_t score; char name[6]; uint8_t skin, lv, rev; };
struct __attribute__((packed)) LbMsg { uint8_t type; uint8_t n; LbEnt e[LB_MAX]; };

#define BTN_LEFT  2
#define BTN_RIGHT 1
#define BTN_JUMP  3


#define SAVE_FORMAT 1       // format de la sauvegarde NVS : ne change que si la structure des donnees sauvegardees devient incompatible
#define FW_VERSION "v56.0"  // affiche a l'ecran titre pour reperer la version flashee -- a incrementer a chaque changement notable
const char* rstReason = "?";  // raison du dernier reset (affichee a l'ecran titre)
RTC_NOINIT_ATTR uint32_t dbgStage;  // dernier marqueur atteint (survit au reset)
uint32_t lastStage = 0;
#define DBG(x) do { dbgStage = (x); } while (0)   // actif solo ET course (diagnostic)

#define DEBUG_SERIAL 0   // passe a 1 pour reactiver les traces Serial (boot/heartbeat/course)
#define DPRINTF(...) do { if (DEBUG_SERIAL && Serial) Serial.printf(__VA_ARGS__); } while (0)

#define TILE 16
#define COLS 20            // largeur (tient dans l'ecran)
#define WROWS 80           // hauteur max d'un niveau (grille monde) -- 76 au niveau 20 (10 colonnes)
#define NUM_LEVELS 1000  // ponytail: 100000 n'entre pas (bestTime[] seul = 400KB, > la RAM du C3) ; 1000 est gratuit, les generateurs >=50 sont deja parametres par n sans limite

// --- physique (px/s) ---
const float GRAVITY     = 1400.0;
const float RUN         = 200.0;    // retour "encore plus rapide" -- 135 -> 160 -> 200
const float JUMP_V      = -450.0;   // 430 -> 450 : saut un peu plus vif
const float MAXFALL     = 540.0;
const float WSLIDE_MIN  = 28.0;
const float WSLIP_ACCEL = 430.0;
const float WJUMP_VY    = -400.0;
const float WJUMP_VX    = 185.0;    // 168 -> 185
const float JUMPLOCK    = 0.13;
const float BOUNCE_V    = -560.0;   // ressort 'R' (dizaine 1, niveaux "squishy") : rebond plus fort qu'un saut normal

// --- couleurs ---
uint16_t COL_BG, COL_BG2, COL_BLK, COL_BLKT, COL_SPK, COL_GOAL, COL_MEAT, COL_MEATD, COL_TXT, COL_SAW, COL_STAR, COL_STARD, COL_SPLAT, COL_SPLATD, COL_ENM, COL_ENMD;
// COL_MEAT/COL_MEATD restent reserves a l'UI (menus/HUD) -- le corps du joueur et les
// briques fragiles ont leurs propres couleurs (voir plus bas) pour pouvoir varier
// independamment sans toucher aux menus.
uint16_t COL_FRAG;                              // brique fragile 'C' -- rouge dedie, fixe quelle que soit la dizaine
uint16_t COL_BOUNCE, COL_BOUNCED;               // ressort 'R' -- vert/rose "gelatine", fixe quelle que soit la dizaine
#define NUM_DECADE_PAL 5                        // dizaines 1-10 .. 41-50 : un fond + une teinte de bloc chacune
uint16_t COL_DECADE_BG[NUM_DECADE_PAL], COL_DECADE_BLK[NUM_DECADE_PAL], COL_DECADE_BLKT[NUM_DECADE_PAL];
uint16_t COL_LVLBG, COL_LVLBLK, COL_LVLBLKT;    // palette du niveau courant (recalculee par setLevelPalette())
#define NUM_PLAYER_TIERS 5                      // palier 0 (aucune serie) .. 4 (diamant)
uint16_t COL_PLAYER_TIER[NUM_PLAYER_TIERS], COL_PLAYERD_TIER[NUM_PLAYER_TIERS];

// --- types (definis avant toute fonction : prototypes Arduino) ---
struct FontSel { const GFXfont* f; uint8_t scale; uint8_t cap; };
#define MS 12
struct Saw { bool active; int axis; float fixed, cur, lo, hi, spd, dir, r; };
struct SndNote { uint16_t f, ms; };   // f = Hz (0 = silence), ms = duree ; {0,0} termine

// ennemis : patrouillent sur une plateforme (x = centre, y = pied)
#define ME 10
struct Enemy { bool active; float x, y, lo, hi, dir, spd; };

// pieges dynamiques : KSPIKE = pics qui surgissent, KFALL = bloc qui tombe
#define MT 32
enum { KSPIKE, KFALL };
struct Trap { bool active; int kind; int tx, ty; float t, p1, p2; float y, vy; bool st, st2; int trig; bool hidden; };

// plateformes qui s'ecroulent (tuile 'C') : state 0 intacte, 1 declenchee, 2 effondree
#define MC 16
struct Crumble { int tx, ty, state; float t; };

// Pile de la tache loop agrandie : les IRQ WiFi (mode 2J) utilisent la pile de la
// tache courante -> 8 Ko debordent pendant drawGame. 24 Ko = marge sure.
SET_LOOP_TASK_STACK_SIZE(12 * 1024);   // 12 Ko : mesure 2026-10-24 (uxTaskGetStackHighWaterMark) : ~2,5 Ko utilises en multijoueur (drift) ; les interruptions Wi-Fi ont leur propre pile sur le C3 ; etait 24 Ko (le tas libere sert au Wi-Fi)

// --- niveau courant ---
char grid[WROWS][COLS + 1];
int  levH;                 // hauteur du niveau (en tuiles)

void clearGrid() {
  for (int r = 0; r < WROWS; r++) { for (int c = 0; c < COLS; c++) grid[r][c] = '.'; grid[r][COLS] = 0; }
}
// ---- Son : buzzer passif (ou petit haut-parleur) entre GPIO9 et GND ; sequences de notes non bloquantes (sndUpdate() dans loop) ----
#define SND_PIN 9            // GPIO9 = bouton BOOT du SuperMini : buzzer PIEZO conseille (un haut-parleur a bobine, bas-ohm, maintiendrait GPIO9 a 0 au demarrage -> mode telechargement)
const SndNote SND_JUMP[]   = {{520,30},{780,45},{0,0}};
const SndNote SND_BOUNCE[] = {{300,40},{600,40},{900,60},{0,0}};
const SndNote SND_COIN[]   = {{988,45},{1319,90},{0,0}};
const SndNote SND_STAR[]   = {{784,60},{988,60},{1175,60},{1568,140},{0,0}};
const SndNote SND_DIE[]    = {{440,70},{330,70},{247,90},{165,200},{0,0}};
const SndNote SND_WIN[]    = {{523,110},{659,110},{784,110},{1047,260},{0,0},{0,0}};
const SndNote SND_CLICK[]  = {{1200,12},{0,0}};
const SndNote *sndSeq = nullptr; uint8_t sndIdx = 0; unsigned long sndT = 0;
bool sndOn = true;
void snd(const SndNote *s) { if (sndOn) { sndSeq = s; sndIdx = 0; sndT = 0; } }
void sndUpdate() {
  if (!sndSeq || millis() < sndT) return;
  const SndNote &n = sndSeq[sndIdx++];
  if (!n.ms) { noTone(SND_PIN); sndSeq = nullptr; return; }
  if (n.f) tone(SND_PIN, n.f); else noTone(SND_PIN);
  sndT = millis() + n.ms;
}

void putH(int c0, int c1, int r, char ch) {
  if (r < 0 || r >= WROWS) return;
  for (int c = c0; c <= c1; c++) if (c >= 0 && c < COLS) grid[r][c] = ch;
}
void putV(int r0, int r1, int c, char ch) {
  if (c < 0 || c >= COLS) return;
  for (int r = r0; r <= r1; r++) if (r >= 0 && r < WROWS) grid[r][c] = ch;
}

// --- scies ---
Saw saws[MS];
int nSaws;
float sawPhase;
float sawX(const Saw &s) { return s.axis == 0 ? s.cur : s.fixed; }
float sawY(const Saw &s) { return s.axis == 0 ? s.fixed : s.cur; }
void addSaw(int axis, float fx, float lo, float hi, float spd, float r) {
  if (nSaws >= MS) return;
  Saw &s = saws[nSaws++];
  s.active = true; s.axis = axis; s.fixed = fx; s.lo = lo; s.hi = hi;
  s.cur = (lo + hi) / 2; s.spd = spd; s.dir = 1; s.r = r;
}

// --- ennemis ---
const float EW = 12, EH = 10;
float popX, popY, popT;                      // "+1" flottant apres un ennemi ecrase
Enemy enemies[ME];
int nEnemies;
// --- eclaboussures (facon Meat Boy) : restent sur le niveau ---
#define MSTAIN 28
float stainX[MSTAIN], stainY[MSTAIN]; int nStain;
void addStain(float x, float y) { stainX[nStain % MSTAIN] = x; stainY[nStain % MSTAIN] = y; nStain++; }

// --- trainee de particules (recompense du quiz) : s'estompe derriere le
// joueur en s'effacant petit a petit (chaque particule retrecit avec l'age),
// evolue avec le TAUX DE REUSSITE du quiz (pas la seule serie en cours) --
// voir computePlayerTier(). Ephemere : pas besoin de la vider au respawn,
// chaque particule disparait d'elle-meme en moins d'une seconde.
#define MTRAIL 72
float trailX[MTRAIL], trailY[MTRAIL], trailAge[MTRAIL], trailLife[MTRAIL], trailVY[MTRAIL];
uint8_t trailK[MTRAIL];                 // variante de couleur (arc-en-ciel au palier max)
int nTrail;
float trailSpawnT;

// --- pieges dynamiques ---
Trap traps[MT]; int nTraps;
int gTrapPct, gFallPct;                 // frequence (fixee par le generateur)

// --- plateformes qui s'ecroulent ---
Crumble crumbs[MC]; int nCrumbs;
int gCrumblePct;
const float CRUMB_DELAY = 0.8;
int crumbAt(int tx, int ty) { for (int i = 0; i < nCrumbs; i++) if (crumbs[i].tx == tx && crumbs[i].ty == ty) return i; return -1; }
// hidden=true : aucun avertissement visuel avant que le piege ne surgisse/tombe
// (mecanique "obstacle fantome", dizaine 31-40) -- cf drawGame().
void addTrapSpike(int tx, int ty, bool hidden = false) {
  if (nTraps >= MT) return;
  Trap &t = traps[nTraps++];
  t.active = true; t.kind = KSPIKE; t.tx = tx; t.ty = ty;
  t.p1 = 0.8; t.p2 = hidden ? 0.22 : 0.10; t.trig = 30; t.t = 0; t.st = false; t.st2 = false; t.hidden = hidden;
}
void addTrapFall(int tx, int ty, bool hidden = false) {
  if (nTraps >= MT) return;
  Trap &t = traps[nTraps++];
  t.active = true; t.kind = KFALL; t.tx = tx; t.ty = ty;
  t.y = ty * TILE; t.vy = 0; t.trig = 26; t.st = false; t.st2 = false; t.hidden = hidden;
}

// --- briques a pousser (dizaine 31-40) ---
// tuile 'B' dans grid[] (solide pour le joueur) + entite pour l'animation et le
// retour a la position de depart au respawn. 'under' = ce qu'il y avait sous la
// brique (ex. un pic '^' qu'elle recouvre en tombant dans une tranchee).
#define MPB 8
struct PushB { int tx, ty, sx, sy; char under, under0; float ox, oy; };
PushB pbs[MPB]; int nPB;
float pushT;                            // temps passe a pousser (une brique avance d'une case toutes les PUSH_DELAY s)
const float PUSH_DELAY = 0.12;
void addPushBlock(int tx, int ty) {
  if (nPB >= MPB || tx < 0 || tx >= COLS || ty < 0 || ty >= WROWS) return;
  PushB &b = pbs[nPB++];
  b.tx = b.sx = tx; b.ty = b.sy = ty; b.under = b.under0 = grid[ty][tx]; b.ox = b.oy = 0;
  grid[ty][tx] = 'B';
}
int pbAt(int tx, int ty) { for (int i = 0; i < nPB; i++) if (pbs[i].tx == tx && pbs[i].ty == ty) return i; return -1; }

// --- drapeau (entite, peut se deplacer) ---
int goalTX, goalTY;                     // position courante (tuiles)
int gpX[4], gpY[4], gpN, gpIdx;         // sequence de positions (fuite du drapeau)
float goalFlash;
#define POLE_H 80                       // mat du drapeau (px) : plus on s'accroche haut, plus le bonus est grand
bool flagSlide, flagDone;               // glissade le long du mat (facon Mario) puis fin de niveau
float happyT, fallPeakY, excitedT;      // mimiques : joie ; chute ; elan (ressort / saut mural)
float flagPopT, flagPopY; int flagPopPts;   // "+N" affiche au moment ou on s'accroche au mat
int happyKind;                          // 1 = piece (yeux dores), 2 = etoile (yeux etincelants)
struct Spark { float x, y, vx, vy, life; uint16_t c; };
Spark sparks[12];                       // etincelles lors d'une prise
void burstSparks(float x, float y, uint16_t c, int n) {
  for (int i = 0; i < 12; i++) {
    Spark &k = sparks[i]; if (k.life > 0 || n <= 0) continue;
    n--; float a = random(0, 628) / 100.0f, sp = 50 + random(0, 70);
    k.x = x; k.y = y; k.vx = cosf(a) * sp; k.vy = sinf(a) * sp - 40; k.life = 0.55f; k.c = c;
  }
}                // mimiques : joie (piece/etoile) ; point haut de la chute en cours

// --- etoiles ---
int  nStarPos;
int  starTX[3], starTY[3];
volatile bool starGot[3];  // volatile : ecrit aussi depuis onEspRecv (etoile prise par l'adversaire)
void addStar(int tx, int ty) {
  while (ty > 0 && tx >= 0 && tx < COLS && grid[ty][tx] != '.') ty--;   // jamais dans le solide : remonte jusqu'a une case vide
  if (nStarPos < 3 && ty >= 0) { starTX[nStarPos] = tx; starTY[nStarPos] = ty; starGot[nStarPos] = false; nStarPos++; }
}
// place une etoile au-dessus d'une plateforme, mais UNIQUEMENT sur une case vide
// (evite les etoiles coincees dans le solide, ex. niveaux en montee)
void addStarAbove(int tx, int topRow) {
  if (tx < 0 || tx >= COLS) return;
  int ty = topRow - 2;
  if (ty < 0 || grid[ty][tx] != '.') ty = topRow - 1;      // sinon un cran plus bas
  if (ty < 0 || ty >= WROWS || grid[ty][tx] != '.') return; // toujours pas de place -> on omet
  addStar(tx, ty);
}

// --- ancres de plateformes (pour semer les etoiles) ---
int aTX[48], aTY[48], nAnc;
void addAnchor(int tx, int ty) { if (nAnc < 48) { aTX[nAnc] = tx; aTY[nAnc] = ty; nAnc++; } }
void sprinkleStars() {
  if (nAnc <= 0) return;
  int idx[3] = { nAnc / 5, nAnc / 2, (nAnc * 4) / 5 };
  for (int k = 0; k < 3; k++) {
    int i = idx[k]; if (i < 0) i = 0; if (i >= nAnc) i = nAnc - 1;
    addStarAbove(aTX[i], aTY[i]);
  }
}

// ================= GENERATEURS =================
// hiddenHazards : mode "obstacle fantome" (dizaine 31-40) -- les pics/blocs
// deviennent invisibles (voir addTrapSpike/addTrapFall + drawGame), et un
// pic fixe devient un pic invisible 'X' au lieu de '^'. holePct : chance par
// plateforme de cacher un trou 'H' au milieu (visuellement identique a '#',
// mais non solide -> on tombe dedans sans l'avoir vu venir).
void buildChain(int n, int gLo, int gHi, int upMax, int dnMax,
                int wLo, int wHi, int spikePct, int sawPct, int climbBias,
                bool hiddenHazards = false, int holePct = 0) {
  float sawSpd = 40.0 + n * 0.7; if (sawSpd > 120) sawSpd = 120;
  int r = 11;
  putH(0, 2, r, '#'); putH(0, 2, r + 1, '#');
  grid[r - 1][1] = 'S';
  addAnchor(1, r);
  int prevR = r, x = 3, gLX = 0, gLR = r, gLW = 3;
  while (x < COLS - 3) {
    int gap = gLo + random(0, gHi - gLo + 1);
    int dy  = random(-upMax, dnMax + 1) - climbBias;
    int nr = prevR + dy; if (nr < 3) nr = 3; if (nr > 12) nr = 12;
    int pw = random(wLo, wHi + 1);
    int nx = x + gap;
    if (nx + pw > COLS - 1) pw = COLS - 1 - nx;
    if (pw < 2) break;
    bool crumble = (!hiddenHazards && gCrumblePct > 0 && (int)random(0, 100) < gCrumblePct);
    putH(nx, nx + pw - 1, nr, crumble ? 'C' : '#');
    addAnchor(nx + pw / 2, nr);
    if (!crumble && pw >= 4 && (int)random(0, 100) < spikePct) {
      int sc = nx + 1 + random(0, pw - 3);
      if ((int)random(0, 100) < gTrapPct) addTrapSpike(sc, nr - 1, hiddenHazards);   // pic surgissant
      else grid[nr - 1][sc] = hiddenHazards ? 'X' : '^';                             // pic fixe (X = invisible)
    }
    if (pw >= 3 && nr > 5 && (int)random(0, 100) < gFallPct)           // bloc qui tombe
      addTrapFall(nx + pw / 2, max(1, nr - 6), hiddenHazards);
    if (hiddenHazards && holePct > 0 && pw >= 3 && (int)random(0, 100) < holePct) {
      int hc = nx + 1 + random(0, pw - 2);
      grid[nr][hc] = 'H';                                              // trou invisible dans le sol
    }
    if (!hiddenHazards && (int)random(0, 100) < sawPct) {
      float sxpx = ((x + nx) / 2.0) * TILE + 8;
      int lr = min(prevR, nr) - 2, hr = max(prevR, nr) + 1;
      addSaw(1, sxpx, lr * TILE, hr * TILE, sawSpd, 9);
    } else if (!hiddenHazards && pw >= 4 && nx < COLS - 6 && (int)random(0, 100) < sawPct) {
      addSaw(0, nr * TILE - 10, nx * TILE + 8, (nx + pw) * TILE - 8, sawSpd, 9);
    }
    prevR = nr; x = nx + pw; gLX = nx; gLR = nr; gLW = pw;
  }
  putH(gLX, gLX + gLW - 1, gLR, '#');   // la derniere plateforme reste solide
  grid[gLR - 1][gLX + gLW / 2] = 'G';
}
void genChain(int n)   { levH = 15; gCrumblePct = n < 10 ? 0 : min(40, 10 + (n - 10) * 2); gTrapPct = n < 15 ? 0 : min(55, (n - 15) * 2); gFallPct = n < 20 ? 0 : min(40, (n - 20) * 2); buildChain(n, 2, min(4, 2 + n / 30), min(2, 1 + n / 50), 3, max(2, 4 - n / 40), 4, min(60, 5 + n / 2), (n <= 6 ? 0 : min(55, (n - 6) * 3)), 0); }
void genStairs(int n)  { levH = 15; gCrumblePct = n < 10 ? 0 : min(38, 8 + (n - 10) * 2); gTrapPct = n < 18 ? 0 : min(50, (n - 18) * 2); gFallPct = n < 22 ? 0 : min(35, (n - 22) * 2); buildChain(n, 2, 3, 2, 2, 3, 4, min(40, n / 2), (n <= 8 ? 0 : min(45, (n - 8) * 3)), 1); }
void genIslands(int n) { levH = 15; gCrumblePct = n < 10 ? 0 : min(35, 8 + (n - 10) * 2); gTrapPct = n < 18 ? 0 : min(50, (n - 18) * 2); gFallPct = n < 24 ? 0 : min(35, (n - 24) * 2); buildChain(n, 3, 4, 1, 2, 2, 2, min(50, n / 2), (n <= 8 ? 0 : min(50, (n - 8) * 3)), 0); }
// niveau "casse-tete" : pieges denses + drapeau qui fuit
void genTraps(int n) {
  levH = 15;
  gCrumblePct = n < 10 ? 0 : min(35, 8 + (n - 10) * 2);
  gTrapPct = min(75, 40 + n / 3);
  gFallPct = min(60, 30 + n / 4);
  buildChain(n, 2, 3, 2, 2, 3, 4, 30, min(40, 10 + n), 0);
  // drapeau fuyard : 2 leurres avant la vraie position (le 'G' final)
  if (nAnc >= 4) {
    gpN = 2;
    gpX[0] = aTX[nAnc / 2];      gpY[0] = aTY[nAnc / 2] - 1;
    gpX[1] = aTX[(nAnc * 3) / 4]; gpY[1] = aTY[(nAnc * 3) / 4] - 1;
  }
}

// dizaine "obstacles fantomes" (niveaux 31-40, n=30..39) : pos = position dans
// la dizaine (0..9). pos 0-1 : couloir simple, pieges avertis comme d'habitude.
// A partir de pos 2 (3e niveau) : plus aucun avertissement (pics/trous/blocs
// surgissent de nulle part), et le trajet devient de plus en plus sinueux.
void genPhantom(int n) {
  levH = 15;
  int pos = n % 10;
  bool hidden = pos >= 2;
  int sinuosity = min(3, 1 + pos / 3);       // denivele : 1 (couloir) -> 3 (sinueux)
  int gapMax = min(4, 2 + pos / 4);
  gCrumblePct = 0;
  gTrapPct  = hidden ? 60 : 15;
  gFallPct  = hidden ? 45 : 10;
  int spikePct = hidden ? (35 + pos * 3) : 15;
  int holePct  = hidden ? (15 + pos * 3) : 0;
  buildChain(n, 2, gapMax, sinuosity, sinuosity, 3, 5, spikePct, 0, 0, hidden, holePct);
}

// ---- dizaine 31-40 (refaite le 2026-09-28) : pos pair = briques a pousser, pos impair = tour fantome ----
// Tout est decrit dans un repere "local" ou l'etage progresse vers la droite ;
// les etages impairs sont en miroir (x -> 19-x) pour faire un zigzag.
static bool gMir;
static int mx(int x) { return gMir ? COLS - 1 - x : x; }
static void putHL(int x0, int x1, int r, char ch) { for (int x = x0; x <= x1; x++) if (x >= 0 && x < COLS && r >= 0 && r < WROWS) grid[r][mx(x)] = ch; }

// BRIQUES A POUSSER : chaque etage = sol + tranchee de pics trop large pour
// etre sautee (W=6 ou 7) + 1 ou 2 briques a pousser dedans (elles tombent dans
// la tranchee, recouvrent les pics et comblent le trou). Chaque brique fait
// gagner 1 case : il reste toujours 5 cases a sauter. Se jeter dans les pics =
// recommencer (les briques reviennent a leur place) : pas de blocage definitif.
// Entre deux etages : escalier de 2 marches dans le coin (x 16-19 local).
void genPushLevel(int n, int pos) {
  int k = pos / 2;                                  // 0..4
  int stages = (k < 2) ? 1 : (k < 4 ? 2 : 3);
  const int SP = 8;                                 // ecart vertical entre etages
  levH = 15 + SP * (stages - 1);
  int f0 = levH - 2;
  for (int st = 0; st < stages; st++) {
    gMir = (st % 2 == 1);
    int f = f0 - SP * st;
    int W = (k == 0 || (k == 2 && st == 0)) ? 6 : 7;
    int nb = W - 5;
    int a  = (st == 0) ? 6 : 9;                     // debut de la tranchee
    int x0 = (st == 0) ? 0 : 4;                     // debut du sol (a gauche : arrivee de l'escalier)
    putHL(x0, 19, f, '#');
    if (st == 0) putHL(0, 19, f + 1, '#');
    else         putHL(a, a + W - 1, f + 1, '#');   // fond de la tranchee
    putHL(a, a + W - 1, f, '^');
    for (int i = 0; i < nb; i++) addPushBlock(mx(a - 3 + i), f - 1);
    if (st == 0) grid[f - 1][mx(1)] = 'S';
    int land = a + W;                               // debut de la zone d'arrivee
    if (k >= 2 && 19 - land >= 5) addTrapSpike(mx(land + 2), f - 1, true);   // pic fantome (visible au dernier moment)
    if (k >= 4 && 19 - land >= 5) addTrapFall(mx(land + 1), f - 6, true);
    if (st < stages - 1) {                          // escalier vers l'etage suivant
      putHL(18, 19, f - 3, '#');
      putHL(16, 17, f - 6, '#');
    } else {
      grid[f - 1][mx(18)] = 'G';
    }
  }
  gMir = false;
}

// TOUR FANTOME : 2 a 4 etages de plateformes en zigzag, relies par un escalier
// de plateformes dans le coin ; obstacles fantomes (pics/trous/blocs caches)
// visibles au dernier moment seulement (voir drawGame).
void genPhantomTower(int n, int pos) {
  int k = pos / 2;                                  // 0..4 (pos 1,3,5,7,9)
  int tiers = 2 + k / 2;                            // 2,2,3,3,4
  const int SP = 7;
  levH = max(15, 8 + SP * (tiers - 1));
  int base0 = levH - 3;
  int gapMax = 2 + (k >= 2 ? 1 : 0);
  int spikePct = 30 + k * 8, holePct = 10 + k * 6, fallPct = 20 + k * 8;
  gTrapPct = 50;
  for (int t = 0; t < tiers; t++) {
    gMir = (t % 2 == 1);
    int base = base0 - SP * t;
    int e;
    if (t == 0) { putHL(0, 2, base, '#'); grid[base - 1][mx(1)] = 'S'; e = 2; }
    else        { putHL(4, 6, base, '#'); e = 6; }
    while (e < 12) {
      int gap = 2 + random(0, gapMax - 1);
      int w = 2 + random(0, 3);
      int x = e + 1 + gap;
      if (x + w - 1 > 14) w = 14 - x + 1;
      if (w < 2) { x = 13; w = 2; }
      int r = base + random(-1, 2);
      if (r > levH - 2) r = levH - 2;
      putHL(x, x + w - 1, r, '#');
      if (w >= 3 && (int)random(0, 100) < spikePct) {
        int sc = x + 1 + random(0, w - 2);
        if ((int)random(0, 100) < gTrapPct) addTrapSpike(mx(sc), r - 1, true);
        else grid[r - 1][mx(sc)] = 'X';
      } else if (w >= 3 && (int)random(0, 100) < holePct) {
        grid[r][mx(x + 1 + random(0, w - 2))] = 'H';
      }
      if ((int)random(0, 100) < fallPct) addTrapFall(mx(x + w / 2), max(0, r - 5), true);
      e = x + w - 1;
    }
    putHL(16, 19, base, '#');                        // palier du coin
    if (t < tiers - 1) {
      putHL(18, 19, base - 2, '#');
      putHL(16, 17, base - 5, '#');
    } else {
      grid[base - 1][mx(18)] = 'G';
    }
  }
  gMir = false;
}

void genDecade31_40(int n) {
  int pos = n % 10;
  if (pos % 2 == 0) genPushLevel(n, pos);
  else              genPhantomTower(n, pos);
}

// dizaine "colonnes & plateformes" (niveaux 11-20, n=10..19) : retour
// "la progression est assez lineaire" (avant : juste N = pos+1 colonnes en
// zigzag, une de plus a chaque niveau) -- remplace par une sequence DIFFERENTE
// par niveau, composee a partir de 2 briques reutilisables :
//  - buildColumn()   : une colonne de wall-jump (comme avant : mur exterieur +
//    interieur, passage de 2 tuiles sous le mur interieur, palier de sortie +
//    plateforme intermediaire). hasFloor=false => colonne "eliminatoire" :
//    pas de sol au fond, y tomber = mourir (pas de rattrapage).
//  - buildPlatChain() : une chaine de plateformes en zigzag ascendant (comme
//    la dizaine 1), derniere plateforme large pour rester "accrochable" par
//    la brique suivante.
// Sequence demandee : 11 simple, 12 = colonne + 3 plateformes, 13 = colonne +
// colonne eliminatoire, 14 = 3 colonnes, 15 = 2 colonnes + 5 plateformes,
// 16 = colonne + 3 plateformes + colonne plus longue ; 17-20 continuent dans
// le meme esprit non lineaire (mix colonnes/eliminatoires/plateformes).
int buildColumn(int F, bool rightSide, int segH, bool hasFloor, bool sawTrap, bool spikeTrap) {
  int xo = rightSide ? 18 : 1, xi = rightSide ? 14 : 5;
  int T = F - segH; if (T < 2) T = 2;
  if (hasFloor) putH(rightSide ? 12 : 1, rightSide ? 18 : 7, F, '#');
  else putH(rightSide ? 13 : 2, rightSide ? 17 : 6, F, '#');   // colonne "eliminatoire" : petit socle seulement, juste atteignable depuis le palier d'en face
  putV(T - 2, F - 1, xo, '#');
  putV(T, F - 3, xi, '#');                              // passage de 2 tuiles sous le mur interieur
  putH(rightSide ? 12 : 5, rightSide ? 14 : 7, T, '#');  // palier de sortie
  putH(9, 10, T - 1, '#');                               // plateforme intermediaire
  addStar((xo + xi) / 2, T + 2);
  if (sawTrap)   addSaw(1, (rightSide ? 8 : 11) * TILE + 8, (T - 4) * TILE, T * TILE, 55, 9);
  if (spikeTrap) addTrapSpike(rightSide ? 12 : 7, T - 1);
  return T;
}
int pcRow, pcCol;   // sortie de buildPlatChain() : position (large) de la derniere plateforme
void buildPlatChain(int row, int col, int dir, int count, bool goal) {
  int curRow = row, curCol = col;
  for (int i = 0; i < count; i++) {
    curRow -= (i % 3 == 2) ? 2 : 1;
    if (curRow < 2) curRow = 2;
    int width = (i == count - 1) ? 7 : 4;               // derniere plateforme large : facile a relier a la suite
    int gap = 3 + (i % 2);
    int nxt = curCol + dir * gap;
    if (nxt < 1 || nxt + width > COLS - 1) { dir = -dir; nxt = curCol + dir * gap; }
    nxt = max(1, min(nxt, COLS - 1 - width));
    // jamais une plateforme AU-DESSUS d'une autre (<= 3 rangees) sur 2 colonnes ou plus : le plafond empeche de sauter
    // depuis celle du dessous (niveaux 12 et 19 bloques). Sinon on essaie d'autres ecarts / l'autre cote.
    auto lowered = [&](int c0) {
      int ov = 0;
      for (int c = c0; c < c0 + width; c++) {
        bool hit = false;
        for (int rr = curRow + 1; rr <= curRow + 3 && rr < WROWS; rr++) if (grid[rr][c] == '#') hit = true;
        if (hit) ov++;
      }
      return ov >= 2;
    };
    if (lowered(nxt)) {
      bool found = false;
      for (int extra = 0; extra <= 3 && !found; extra++)
        for (int sgn = 0; sgn < 2 && !found; sgn++) {
          int d2 = sgn ? -dir : dir, n2 = curCol + d2 * (gap + extra);
          if (n2 < 1 || n2 + width > COLS - 1 || lowered(n2)) continue;
          nxt = n2; dir = d2; found = true;
        }
    }
    curCol = nxt;
    putH(curCol, curCol + width - 1, curRow, '#');
    addAnchor(curCol + width / 2, curRow);
    if (i == count / 2) addStar(curCol + width / 2, max(0, curRow - 2));
    if (goal && i == count - 1) grid[curRow - 1][curCol + width / 2] = 'G';
  }
  pcRow = curRow; pcCol = curCol;
}
int startFloorAndSpawn() {   // petit socle de depart, retourne sa rangee
  int F = levH - 2;
  putH(1, 6, F, '#');
  grid[F - 1][3] = 'S';
  return F;
}
void genDecade11_20(int n) {
  int pos = n % 10;
  clearGrid(); levH = 60;   // canvas large et fixe : simplifie l'enchainement des briques, la camera ne monte que jusqu'ou le niveau va vraiment
  int F = startFloorAndSpawn();
  switch (pos) {
    case 0: {   // 11 : intro -- une colonne puis 2 plateformes
      int T = buildColumn(F, false, 7, true, false, false);
      buildPlatChain(T - 1, 9, 1, 2, true);
      break;
    }
    case 1: {   // 12 : colonne + 3 plateformes apres
      int T = buildColumn(F, false, 7, true, true, false);
      buildPlatChain(T - 1, 9, 1, 3, true);
      break;
    }
    case 2: {   // 13 : colonne puis 2e colonne eliminatoire (sans sol)
      int T1 = buildColumn(F, false, 7, true, false, true);
      int T2 = buildColumn(T1, true, 7, false, true, false);
      buildPlatChain(T2 - 1, 9, -1, 2, true);
      break;
    }
    case 3: {   // 14 : trois colonnes
      int T1 = buildColumn(F, false, 7, true, false, true);
      int T2 = buildColumn(T1, true, 7, true, true, false);
      int T3 = buildColumn(T2, false, 7, true, false, true);
      buildPlatChain(T3 - 1, 9, 1, 1, true);
      break;
    }
    case 4: {   // 15 : COULOIR simple -- un pic surgit au dernier moment, puis un bloc tombe un peu plus loin
      clearGrid(); levH = 15;
      int Fc = levH - 2;
      putH(0, 19, Fc, '#'); putH(0, 19, Fc + 1, '#'); putH(0, 19, 6, '#');   // sol + plafond
      putV(6, Fc, 0, '#'); putV(6, Fc, 19, '#');                              // murs
      grid[Fc - 1][2] = 'S'; grid[Fc - 1][17] = 'G';
      addTrapSpike(8, Fc - 1);          // pic cache : surgit quand on arrive dessus -> sauter
      addTrapFall(14, Fc - 5);          // bloc suspendu : tombe quand on passe dessous -> ne pas s'arreter
      addStar(5, Fc - 3); addStar(11, Fc - 3); addStar(16, Fc - 3);
      break;
    }
    case 5: {   // 16 : colonne + 3 plateformes, puis une autre colonne plus longue
      int T1 = buildColumn(F, false, 7, true, false, false);
      buildPlatChain(T1 - 1, 9, 1, 3, false);
      int T2 = buildColumn(pcRow, pcCol > 9, 11, true, true, true);
      buildPlatChain(T2 - 1, 9, 1, 1, true);
      break;
    }
    case 6: {   // 17 : colonne eliminatoire, 4 plateformes, colonne
      int T1 = buildColumn(F, false, 7, false, true, true);
      buildPlatChain(T1 - 1, 9, 1, 4, false);
      int T2 = buildColumn(pcRow, pcCol > 9, 8, true, true, true);
      buildPlatChain(T2 - 1, 9, -1, 1, true);
      break;
    }
    case 7: {   // 18 : quatre colonnes, une eliminatoire au milieu
      int T1 = buildColumn(F, false, 7, true, false, true);
      int T2 = buildColumn(T1, true, 7, false, true, false);
      int T3 = buildColumn(T2, false, 7, true, true, true);
      int T4 = buildColumn(T3, true, 7, true, false, true);
      buildPlatChain(T4 - 1, 9, -1, 1, true);
      break;
    }
    case 8: {   // 19 : 6 plateformes puis colonne eliminatoire plus longue
      buildPlatChain(F - 1, 4, 1, 6, false);
      int T = buildColumn(pcRow, pcCol > 9, 12, false, true, true);
      buildPlatChain(T - 1, 9, 1, 1, true);
      break;
    }
    default: {   // 20 : finale -- colonne, 3 plateformes, colonne eliminatoire, 3 plateformes, colonne plus longue
      int T1 = buildColumn(F, false, 7, true, false, true);
      buildPlatChain(T1 - 1, 9, 1, 3, false);
      int T2 = buildColumn(pcRow, pcCol > 9, 7, false, true, true);
      buildPlatChain(T2 - 1, 9, -1, 3, false);
      int T3 = buildColumn(pcRow, pcCol > 9, 12, true, true, true);
      buildPlatChain(T3 - 1, 9, 1, 1, true);
      break;
    }
  }
}

// cheminee (wall-jump) - variee
void genChimney(int n) {
  levH = 15;
  int wl = 6 + random(0, 3);         // mur gauche 6..8
  int wr = wl + 4;                   // shaft de largeur 3
  int goalR = max(2, 8 - n / 15);
  for (int r = goalR + 1; r <= 13; r++) { grid[r][wl] = '#'; grid[r][wr] = '#'; }
  putH(0, COLS - 1, 14, '#');
  int mid = (wl + wr) / 2;
  grid[12][mid] = '#'; grid[11][mid] = 'S';
  putH(wl + 1, wr - 1, 13, '^');
  grid[goalR][mid] = 'G';
  addStar(mid, 9); addStar(wl + 1, goalR + 3); addStar(wr - 1, goalR + 1);
}

// PUITS : sortir en wall-jumpant alterne, puis rejoindre le drapeau
void genWell(int n) {
  clearGrid();
  levH = 26;
  int wl = 8, wr = 11;               // shaft cols 9,10 (largeur 2) -> murs 8 et 11
  int top = 5;                       // haut du puits
  int bot = levH - 2;                // fond
  putV(top, bot, wl, '#');
  putV(top, bot, wr, '#');
  putH(wl, wr, bot, '#');            // fond du puits (petit socle)
  grid[bot - 1][9] = 'S';            // depart au fond
  // pics au fond a cote du socle -> tomber = mort (le socle est sur)
  // sortie en haut : palier vers la droite
  putH(wr, 16, top, '#');
  grid[top - 1][15] = 'G';           // drapeau sur le palier
  // etoiles : dans le puits + a la sortie
  addStar(9, bot - 6);
  addStar(10, top + 6);
  addStar(15, top - 2);
  // quelques scies fixes ? non, on garde le puits "pur"
}

// ASCENSION : niveau entierement en montee, camera qui suit
void genClimb(int n) {
  clearGrid();
  levH = 30;
  int r = levH - 2;
  putH(1, 5, r, '#'); putH(1, 5, r + 1, '#');
  grid[r - 1][3] = 'S';
  addAnchor(3, r);
  int cx = 3;                        // colonne centrale courante
  int gLX = 3, gLR = r, gLW = 5;
  while (r > 3) {
    r -= 2; if (r < 2) r = 2;         // monte de 2 (a portee de saut)
    int pw = 3 + random(0, 2);        // largeur 3..4
    int dx = random(-3, 4);          // decalage -3..3 (toujours atteignable)
    int nx = cx + dx - pw / 2;
    if (nx < 1) nx = 1; if (nx + pw > COLS - 1) nx = COLS - 1 - pw;
    putH(nx, nx + pw - 1, r, '#');
    addAnchor(nx + pw / 2, r);
    cx = nx + pw / 2;
    if (n > 25 && random(0, 100) < 22)
      addSaw(0, r * TILE - 10, nx * TILE + 8, (nx + pw) * TILE - 8, 60, 8);
    gLX = nx; gLR = r; gLW = pw;
  }
  grid[gLR - 1][gLX + gLW / 2] = 'G';
  sprinkleStars();
}

// niveaux dessines a la main (hand_levels.txt / hand_levels_data.h) : les 3
// premiers de chaque dizaine (0,1,2,10,11,12,...). Reutilise directement le
// tableau grid[][] -> findSpawn()/findGoal()/le recensement des 'C' dans
// loadLevelData() fonctionnent sans changement. Pas de scies/pieges dynamiques
// dans les niveaux dessines a la main (v1) : seulement blocs/fragile/pic fixe.
const HandLevel* findHandLevel(int n) {
  for (int i = 0; i < N_HAND; i++) if (HAND_LEVELS[i].n == n) return &HAND_LEVELS[i];
  return nullptr;
}
void loadRows(const char* const* rows, int h);
void loadHandLevel(int n) {
  const HandLevel* hl = findHandLevel(n);
  if (!hl) { genChain(n); return; }   // filet de securite si la donnee manque
  loadRows(hl->rows, hl->h);
}

// ---- BRIQUE : niveau perso recu par le port serie (page web ../brique, bouton "Envoyer sur la console") ----
// Une ligne JSON terminee par '\n' : {...,"rows":["....","#S..",...]}. Seul "rows" est lu (format hand_levels.txt).
// Le niveau prend l'emplacement CUSTOM_IDX (dernier niveau) : Rejouer le recharge tel quel, pas d'ennemis ni de bonus ajoutes.
#define CUSTOM_IDX (NUM_LEVELS - 1)
char customRows[WROWS][COLS + 1];
const char* customPtr[WROWS];
int customH = 0;                      // 0 = aucun niveau perso recu depuis le demarrage
uint32_t raceCode = 0;                // BRIQUE : code du jeu publie de la course en cours (0 = aucun)
bool customDJ = false;                // BRIQUE : "doubleSaut":true dans les params -> double saut sans passer par la boutique
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))   // (meme definition que plus bas : utilisee des ici)
// reglages choisis sur la page ("params") : appliques au seul niveau perso (bqOn()), memes valeurs que l'apercu web
struct BqTheme { const char* nom; uint8_t c0[3], c1[3], sol[3], bord[3]; };   // ciel haut, ciel bas, sol, bord du sol
const BqTheme BQ_THEMES[] = {
  { "foret",  { 155, 231, 255 }, { 232, 255, 240 }, {  63, 143,  58 }, {  42,  94,  39 } },
  { "lave",   {  58,  11,  11 }, { 168,  54,  26 }, {  74,  59,  59 }, { 255, 106,   0 } },
  { "glace",  { 207, 233, 255 }, { 255, 255, 255 }, { 122, 166, 201 }, {  79, 127, 166 } },
  { "espace", {   5,   3,  26 }, {  43,  29,  92 }, {  92,  90, 122 }, { 166, 156, 255 } },
  { "bonbon", { 255, 209, 236 }, { 255, 244, 194 }, { 255, 122, 184 }, { 194,  56, 122 } } };
struct BqHero { const char* nom; uint16_t col; };
const BqHero BQ_HEROS[] = { { "cube", RGB565(232, 50, 43) }, { "ninja", RGB565(34, 34, 51) }, { "robot", RGB565(154, 164, 176) }, { "chat", RGB565(255, 153, 51) } };
int bqTheme = -1, bqHero = -1;        // -1 = pas choisi : dessin habituel de la console
float bqGrav = 1, bqVit = 1;
// BRIQUE "fiche" (decodee par bqFicheParse, brique/FICHE-JEU.md) : boutons, saut, murs, objets qui bougent, regles, but
#define FOBJ_MAX 16
#define FREG_MAX 8
extern int curLevel;                               // (declare plus bas) : pour bqOn() dans les fonctions qui suivent
char bqTmp[WROWS][COLS + 1];                       // grille en cours de decodage ; fiche plateforme : garde les lettres de l'enfant (regles, remise a zero)
char fTr[128];                                     // fiche : lettre -> brique du moteur
struct FObj { float x, y, cx, cy, vy, duree, pause, t, proche, vit, lo, hi; int8_t pts[4][2]; uint8_t n, forme, mvt, contact; char lettre; bool dessus, cache, on, alive; };
FObj fob[FOBJ_MAX]; int nFob = 0;
struct FReg { uint8_t quand, alors; char qL, aL; float qv, av; char txt[21]; };   // "quand ... alors ..." (+ lettre ou nombre de chaque cote)
FReg freg[FREG_MAX]; int nFreg = 0; uint8_t fFait = 0, fTouchK = 0;            // regles deja appliquees / "touche" vues dans cette vie
struct FTir { float x, y, vx; bool on; } ftir[3];  // projectiles du bouton "tirer"
bool bqFiche = false, fGlisse = true, fMural = true, fCamFixe = false, fTot = false; uint8_t fBtn[3], fBord = 1, fBut = 0;   // fBtn : indice dans F_ACT pour G, D, P3
float fSautK = 1, fRebondV = -560, fElanK = 1, fButT = 0, fVit0 = 1, fTirV = 0;
int fPts = 0, fOLeft = 0, fRam0 = 0, fEcr0 = 0; uint32_t fT0 = 0, fMsgFin = 0; const char* fMsg = "";
bool fH[10], fE[10];                               // actions de la fiche tenues / juste appuyees (indices de F_ACT)
long bqCiel = -1, bqSol = -1;
int bqVies = 0, bqChrono = 0; bool bqTout = false, bqTimeout = false;   // 0 = illimite / pas de chrono ; bqTout = tout ramasser avant le drapeau
char bqTitre[31] = "", bqMsg[41] = "";  // titre du jeu et message de victoire (ecran de fin)          // couleurs choisies sur la page (0xRRGGBB), -1 = celles du theme
#define bqOn() (curLevel == CUSTOM_IDX && customH)   // macro : curLevel est declare plus bas
int bqFind(const char* p, const char* cle, const char* const* noms, int n, int pas) {   // "cle":"nom" -> indice dans la table, -1 sinon
  const char* q = strstr(p, cle); if (!q) return -1; q = strchr(q + strlen(cle), '"'); if (!q) return -1; q++;
  for (int i = 0; i < n; i++) { const char* nm = *(const char* const*)((const char*)noms + i * pas); size_t l = strlen(nm); if (!strncmp(q, nm, l) && q[l] == '"') return i; }
  return -1;
}
long bqHex(const char* p, const char* cle) {   // "cle":"#rrggbb" -> 0xRRGGBB, -1 sinon
  const char* q = strstr(p, cle); if (!q) return -1; q = strchr(q + strlen(cle), '"'); if (!q || q[1] != '#') return -1;
  char h[7]; memcpy(h, q + 2, 6); h[6] = 0; for (int i = 0; i < 6; i++) if (!isxdigit((unsigned char)h[i])) return -1;
  return strtol(h, nullptr, 16);
}
float bqNum0(const char* p, const char* cle, float lo, float hi) {   // comme bqNum, mais 0 si absent
  const char* q = strstr(p, cle); if (!q) return 0; float v = atof(q + strlen(cle)); return v < lo ? lo : v > hi ? hi : v;
}
float bqNum(const char* p, const char* cle, float lo, float hi) {
  const char* q = strstr(p, cle); if (!q) return 1; float v = atof(q + strlen(cle)); return v < lo ? lo : v > hi ? hi : v;
}
void bqParams(const char* s) {
  const char* p = strstr(s, "\"params\"");
  bqTheme = bqHero = -1; bqGrav = bqVit = 1; bqCiel = bqSol = -1; bqVies = bqChrono = 0; bqTout = false;
  jsonStr(s, "titre", bqTitre, sizeof bqTitre); jsonStr(s, "message", bqMsg, sizeof bqMsg);
  if (!p) return;
  bqCiel = bqHex(p, "\"ciel\":"); bqSol = bqHex(p, "\"sol\":");
  bqVies = (int)bqNum0(p, "\"vies\":", 0, 9); bqChrono = (int)bqNum0(p, "\"chrono\":", 0, 600);
  bqTout = strstr(p, "\"objectif\":\"tout\"") != nullptr;
  bqTheme = bqFind(p, "\"theme\":", &BQ_THEMES[0].nom, 5, sizeof(BqTheme));
  bqHero  = bqFind(p, "\"hero\":",  &BQ_HEROS[0].nom, 4, sizeof(BqHero));
  bqGrav = bqNum(p, "\"gravite\":", 0.3f, 2.0f);
  bqVit  = bqNum(p, "\"vitesse\":", 0.5f, 2.0f);
}
uint16_t bqMix(const uint8_t* a, const uint8_t* b, int k, int n) {   // a -> b, k/n
  return RGB565(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n, a[2] + (b[2] - a[2]) * k / n);
}

void loadRows(const char* const* rows, int h) {
  levH = h;
  for (int r = 0; r < h; r++) {
    const char* row = rows[r];
    int rl = strlen(row);
    for (int c = 0; c < COLS; c++) {
      char ch = (c < rl) ? row[c] : '.';
      if (ch == '*') { addStar(c, r); ch = '.'; }
      else if (ch == 'H') { addTrapSpike(c, r, true); ch = '.'; }   // BRIQUE : pic cache, il surgit quand on arrive tout pres
      else if (ch == 'W') { addSaw(1, c * TILE + 8, (r - 3) * TILE, (r + 1) * TILE, 50, 9); ch = '.'; }  // tronconneuse verticale (marqueur, pas une tuile solide)
      grid[r][c] = ch;
    }
  }
}

// dizaine "briques fragiles" (niveaux 21-30, n=20..29) : rotation hand / puits
// / pieges / escaliers, densite de 'C' croissante dans la dizaine (20%->83%).
// pos 0-1 (21-22) : designs dedies demandes explicitement (retour) -- le
// reste alterne puits/pieges/escaliers dans un ordre non lineaire, avec un
// pic invisible en plus sur certains niveaux.
int fragPct(int pos) { return min(85, 20 + pos * 7); }

void genFrag21(int n) {   // 21 : 3 plateformes (1 fragile) + une colonne, scie
  clearGrid(); levH = 40;
  int F = startFloorAndSpawn();
  buildPlatChain(F - 1, 4, 1, 3, false);
  addSaw(1, max(0, pcCol - 3) * TILE + 8, (pcRow - 4) * TILE, pcRow * TILE, 50, 9);
  bool rightSide = pcCol > 9;
  int T = buildColumn(pcRow, rightSide, 8, true, false, true);
  grid[pcRow][min(pcCol + 2, COLS - 1)] = 'C';          // une des plateformes est fragile (apres buildColumn : son sol ne doit pas l'ecraser)
  grid[T - 1][rightSide ? 13 : 6] = 'G';
}
void genFrag22(int n) {   // 22 : 10 plateformes (en 2 segments), scie, fragile, pic invisible
  clearGrid(); levH = 55;
  int F = startFloorAndSpawn();
  buildPlatChain(F - 1, 4, 1, 5, false);
  int midRow = pcRow, midCol = pcCol;
  grid[midRow][min(midCol + 1, COLS - 1)] = 'C';                 // fragile sur la plateforme-palier
  grid[midRow - 1][min(midCol + 3, COLS - 1)] = 'X';             // pic invisible au-dessus du sol (comme genPhantom)
  addSaw(1, min(midCol + 5, COLS - 1) * TILE + 8, (midRow - 6) * TILE, midRow * TILE, 55, 9);
  buildPlatChain(midRow - 1, midCol + 3, 1, 5, true);
}

// ajoute un pic invisible isole (retour "non lineaire", un peu de variete en
// plus sur certains niveaux de la dizaine) : cherche un sol '#' avec la case
// juste au-dessus libre, loin de S/G.
// cherche une case d'air juste au-dessus d'un sol '#', loin de S/G -- reutilise
// par sprinklePhantomSpike et sprinkleCoin pour poser une tuile ponctuelle.
bool findAirSpot(int &outTy, int &outTx, int skip = 0) {
  for (int ty = 2; ty < levH - 2; ty++) {
    for (int tx = 2; tx < COLS - 2; tx++) {
      if (grid[ty][tx] != '#' || grid[ty - 1][tx] != '.') continue;
      bool nearSG = false;
      for (int dr = -1; dr <= 1 && !nearSG; dr++) for (int dc = -2; dc <= 2 && !nearSG; dc++) {
        int rr = ty + dr, cc = tx + dc;
        if (rr >= 0 && rr < levH && cc >= 0 && cc < COLS && (grid[rr][cc] == 'S' || grid[rr][cc] == 'G')) nearSG = true;
      }
      if (nearSG) continue;
      if (skip-- > 0) continue;
      outTy = ty - 1; outTx = tx; return true;
    }
  }
  return false;
}
void sprinklePhantomSpike(int n) { int ty, tx; if (findAirSpot(ty, tx)) grid[ty][tx] = 'X'; }
// 1 piece par niveau (retour "boutique/pieces") -- ramassee comme une case de
// grille, pas d'entite a part (cf collecte dans playStep, tuile 'O').
void sprinkleCoin(int n) {                       // 6 pieces par niveau, espacees (1 emplacement sur 3 ; skip=2*i car chaque piece posee retire un candidat)
  for (int i = 0; i < 6; i++) { int ty, tx; if (findAirSpot(ty, tx, 2 * i)) grid[ty][tx] = 'O'; }
}

void genFragileWell(int n, int pos) {
  clearGrid();
  levH = 24;
  int wl = 8, wr = 11, top = 5, bot = levH - 2;
  putV(top, bot, wl, '#');
  putV(top, bot, wr, '#');
  putH(wl, wr, bot, '#');
  grid[bot - 1][9] = 'S';
  int fp = fragPct(pos), exitLen = 7;
  for (int i = 0; i < exitLen; i++) {
    int c = wr + i; if (c >= COLS) break;
    grid[top][c] = ((int)random(0, 100) < fp) ? 'C' : '#';
  }
  int ge = wr + exitLen - 1; if (ge > COLS - 1) ge = COLS - 1;
  grid[top - 1][ge] = 'G';
  addStar(9, bot - 6);
  addStar(15, top - 2);
}

void genFragileTraps(int n, int pos) {
  levH = 15;
  gCrumblePct = fragPct(pos);
  gTrapPct = min(70, 35 + pos * 3);
  gFallPct = min(55, 25 + pos * 3);
  buildChain(n, 2, 3, 2, 2, 3, 4, 30, min(35, 8 + pos), 0);
  if (nAnc >= 4) {
    gpN = 2;
    gpX[0] = aTX[nAnc / 2];       gpY[0] = aTY[nAnc / 2] - 1;
    gpX[1] = aTX[(nAnc * 3) / 4]; gpY[1] = aTY[(nAnc * 3) / 4] - 1;
  }
}

void genFragileStairs(int n, int pos) {
  levH = 15;
  gCrumblePct = fragPct(pos);
  gTrapPct = min(45, 10 + pos * 3);
  gFallPct = min(30, pos * 3);
  buildChain(n, 2, 3, 2, 2, 3, 4, min(35, pos * 3), 0, 1);
}

void genDecade21_30(int n) {
  int pos = n % 10;
  if (pos == 0)                   genFrag21(n);
  else if (pos == 1)              genFrag22(n);
  else if (pos == 3 || pos == 7)  genFragileWell(n, pos);
  else if (pos == 5 || pos == 8)  genFragileTraps(n, pos);
  else                             genFragileStairs(n, pos);     // pos 2,4,6,9
  if (pos == 4 || pos == 9) sprinklePhantomSpike(n);   // retour "non lineaire" : un peu de variete en plus
}

// dizaine "cumul final" (niveaux 41-50, n=40..49) : alterne fantome durci
// (pos pairs) et fragile+pieges durci (pos impairs) pour rester jouable --
// combiner les deux mecaniques sur un meme niveau serait souvent injouable.
// Les 2 derniers niveaux (pos 8-9) cumulent tout : puits etroit, sortie en
// briques fragiles et un trou invisible dans la sortie.
void genHardPhantom(int n, int pos) {
  levH = 15;
  gCrumblePct = 0;
  gTrapPct = 65;
  gFallPct = 50;
  int spikePct = 45 + pos * 2;
  int holePct  = 25 + pos * 2;
  buildChain(n, 3, 4, 3, 3, 2, 4, spikePct, 0, 0, true, holePct);
}

void genHardFragile(int n, int pos) {
  levH = 15;
  gCrumblePct = min(85, 55 + pos * 3);
  gTrapPct = min(80, 50 + pos * 3);
  gFallPct = min(65, 35 + pos * 3);
  buildChain(n, 2, 3, 2, 2, 2, 3, 40, min(45, 15 + pos * 3), 0);
}

void genHardChainWell(int n, int pos) {
  clearGrid();
  levH = 26;
  int wl = 8, wr = 11, top = 4, bot = levH - 2;
  putV(top, bot, wl, '#');
  putV(top, bot, wr, '#');
  putH(wl, wr, bot, '#');
  grid[bot - 1][9] = 'S';
  grid[(top + bot) / 2][9] = '^';                 // force le wall-jump alterne
  int exitLen = 6;
  for (int i = 0; i < exitLen; i++) {
    int c = wr + i; if (c >= COLS) break;
    grid[top][c] = ((int)random(0, 100) < 60) ? 'C' : '#';
  }
  int holeC = wr + exitLen / 2; if (holeC > COLS - 1) holeC = COLS - 1;
  int ge = wr + exitLen - 1; if (ge > COLS - 1) ge = COLS - 1;
  grid[top][holeC] = 'H';                          // trou invisible dans la sortie fragile
  grid[top - 1][ge] = 'G';
  addStar(9, bot - 6);
  addStar(9, top + 6);
  addStar(ge, top - 2);
}

void genDecade41_50(int n) {
  int pos = n % 10;
  if (pos == 8 || pos == 9)      genHardChainWell(n, pos);
  else if (pos % 2 == 0)          genHardPhantom(n, pos);
  else                             genHardFragile(n, pos);
}

// palette (fond/blocs) du niveau n -- change tous les 10 niveaux (jamais
// niveau par niveau). Appelee par loadLevelData() pour le niveau reellement
// joue, et par drawLevelPreview() pour la miniature d'un niveau quelconque.
void setLevelPalette(int n) {
  int decade = (n / 10) % NUM_DECADE_PAL;
  COL_LVLBG = COL_DECADE_BG[decade];
  COL_LVLBLK = COL_DECADE_BLK[decade];
  COL_LVLBLKT = COL_DECADE_BLKT[decade];
}

// complete jusqu'a 3 etoiles (retour : "3 etoiles dans tous les niveaux") --
// candidats = case vide avec du vide au-dessus et un sol solide ('#'/'C')
// dessous, pas collee a une autre etoile/au depart/au drapeau, et hors piege.
// Parcours du bas vers le haut (sens de progression habituel), repartis sur
// la liste des candidats. Deterministe (pas de random) : meme niveau = memes etoiles.
bool starSpotOk(int c, int r) {
  if (r < 1 || r >= levH - 1 || grid[r][c] != '.' || grid[r - 1][c] != '.') return false;
  if (grid[r + 1][c] != '#' && grid[r + 1][c] != 'C') return false;
  for (int i = 0; i < nStarPos; i++) if (abs(starTX[i] - c) < 2 && abs(starTY[i] - r) < 2) return false;
  for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
    int rr = r + dr, cc = c + dc;
    if (rr < 0 || rr >= levH || cc < 0 || cc >= COLS) continue;
    if (grid[rr][cc] == 'S' || grid[rr][cc] == 'G') return false;
  }
  for (int i = 0; i < nTraps; i++) if (traps[i].tx == c && abs(traps[i].ty - r) <= 1) return false;
  return true;
}
void fillStars() {
  int nCand = 0;
  for (int r = levH - 2; r >= 1; r--) for (int c = 0; c < COLS; c++) if (starSpotOk(c, r)) nCand++;
  int need = 3 - nStarPos;
  for (int k = 0; k < need && nStarPos < 3 && nCand > 0; k++) {
    int target = (k + 1) * nCand / (need + 1), idx = 0;
    bool placed = false;
    for (int pass = 0; pass < 2 && !placed; pass++) {   // 2e passe : depuis le debut si rien apres la cible
      idx = 0;
      for (int r = levH - 2; r >= 1 && !placed; r--) for (int c = 0; c < COLS && !placed; c++) {
        if (!starSpotOk(c, r)) continue;
        if (pass == 0 && idx++ < target) continue;
        addStar(c, r); placed = true;
      }
    }
  }
}

// retour "ajouter des blocs rebond tous les trois niveaux depuis le debut" :
// un niveau sur trois (hors dizaine 1, qui a deja son propre systeme de
// ressorts dedie), convertit une tuile '#' existante en ressort 'R' --
// toujours solide donc jamais bloquant, juste un bonus de variete. Cherche
// une tuile avec au moins 5 cases d'air libre au-dessus (pas de plafond
// proche) et pas collee a S/G.
// plaques glissantes ('I') tous les 2 niveaux (niveaux affiches pairs) : des blocs choisis au hasard parmi ceux dont le dessus
// est libre (hors depart/drapeau) deviennent de la glace -- meme solidite, mais le joueur y derape (voir playStep)
void sprinkleIce(int n) {
  if (n % 2 == 0) return;
  for (int ty = 2; ty < levH; ty++) for (int tx = 0; tx < COLS; tx++) {
    if (grid[ty][tx] != '#' || grid[ty - 1][tx] != '.' || grid[ty - 2][tx] != '.') continue;   // dessus libre
    bool sg = false;
    for (int dr = -2; dr <= 0; dr++) for (int dc = -1; dc <= 1; dc++) {
      int rr = ty + dr, cc = tx + dc;
      if (cc >= 0 && cc < COLS && (grid[rr][cc] == 'S' || grid[rr][cc] == 'G')) sg = true;
    }
    if (!sg && random(100) < 30) grid[ty][tx] = 'I';       // ~30 % des blocs du dessus deviennent glace, au hasard
  }
}

void sprinkleBounce(int n) {
  if (n < 10 || (n + 1) % 3 != 0) return;
  for (int ty = levH - 3; ty >= 2; ty--) {
    for (int tx = 2; tx < COLS - 2; tx++) {
      if (grid[ty][tx] != '#') continue;
      bool clear = true;
      for (int k = 1; k <= 5 && clear; k++) { int yy = ty - k; if (yy < 0 || grid[yy][tx] != '.') clear = false; }
      if (!clear) continue;
      bool nearSG = false;
      for (int dr = -2; dr <= 0 && !nearSG; dr++) for (int dc = -1; dc <= 1 && !nearSG; dc++) {
        int rr = ty + dr, cc = tx + dc;
        if (rr >= 0 && rr < levH && cc >= 0 && cc < COLS && (grid[rr][cc] == 'S' || grid[rr][cc] == 'G')) nearSG = true;
      }
      if (nearSG) continue;
      grid[ty][tx] = 'R';
      return;
    }
  }
}

// boss (niveaux 100, 200... 900 : n % 100 == 99) : arene plate, boss a droite qui tire, laser P1+P2 (3 s, recharge 20 s)
bool bossOn, laserOn; float bossHp, bossMaxHp, bossT, bossShot, laserE, laserCd, bossHit;
struct BBul { bool a; float x, y; } bbul[4];
float bossCY() { return 138 + 58 * sinf(bossT * 0.9f); }
void genBoss(int n) {
  levH = 15;
  for (int c = 0; c < COLS; c++) grid[13][c] = grid[14][c] = '#';
  grid[12][1] = 'S'; grid[6][1] = 'G';
  for (int c = 0; c < 4; c++) grid[7][c] = '#';        // plateforme du drapeau (3 rangees au-dessus)
  for (int c = 3; c < 7; c++) grid[10][c] = '#';
  for (int c = 9; c < 13; c++) grid[10][c] = '#';
  bossOn = true; laserOn = false; bossT = 0; bossHit = 0; laserE = 3; laserCd = 0;
  bossMaxHp = min(10.0f, 3.0f + (n + 1) / 100.0f); bossHp = bossMaxHp;
}

void genLevel(int n) {
  bossOn = false;
  randomSeed((uint32_t)n * 2654435761u + 1u);
  nSaws = 0; for (int i = 0; i < MS; i++) saws[i].active = false;
  nTraps = 0; gTrapPct = 0; gFallPct = 0;
  nCrumbs = 0; gCrumblePct = 0;
  gpN = 0; gpIdx = 0; goalFlash = 0;
  nStarPos = 0; nAnc = 0;
  nPB = 0; pushT = 0;
  clearGrid(); levH = 15;

  if (n == CUSTOM_IDX && customH) { loadRows(customPtr, customH); return; }   // BRIQUE : le niveau de l'enfant, tel quel
  if (n % 100 == 99)      genBoss(n);          // boss : niveaux 100, 200... 900
  else if (n < 10)        loadHandLevel(n);     // dizaine 1 (1-10) : dessinee a la main, inchangee
  else if (n < 20)        genDecade11_20(n);    // dizaine 2 (11-20) : puits/cheminees enchainees
  else if (n < 30)        genDecade21_30(n);    // dizaine 3 (21-30) : hand/puits/pieges/escaliers + fragile rouge
  else if (n / 10 == 3)   genDecade31_40(n);    // dizaine 4 (31-40) : briques a pousser / tours fantomes
  else if (n < 50)        genDecade41_50(n);    // dizaine 5 (41-50) : cumul final
  else if (n % 10 < 3)    loadHandLevel(n);     // dizaines 6-10 (51-100) : inchangees (premier jet)
  else if (n % 10 == 3)   genWell(n);
  else if (n % 10 == 7)   genClimb(n);
  else if (n % 10 == 5)   genTraps(n);
  else if (n % 5 == 4)    genChimney(n);
  else {
    int a = n % 3;
    if (a == 0)      genChain(n);
    else if (a == 1) genStairs(n);
    else             genIslands(n);
  }
  for (int i = 0; i < nStarPos; i++)             // filet : etoile ecrasee apres coup par une structure -> retiree (refaite par fillStars)
    if (grid[starTY[i]][starTX[i]] != '.') { starTX[i] = starTX[nStarPos - 1]; starTY[i] = starTY[nStarPos - 1]; nStarPos--; i--; }
  if (nStarPos == 0) sprinkleStars();
  if (n == 25) {                                 // retour : "enlever le pique du niveau 26" -> aucun pic (fixe, fantome, surgissant)
    for (int r = 0; r < levH; r++) for (int c = 0; c < COLS; c++) if (grid[r][c] == '^' || grid[r][c] == 'X') grid[r][c] = '.';
    for (int i = 0; i < nTraps; i++) if (traps[i].kind == KSPIKE) traps[i].active = false;
  }
  if (!bossOn) sprinkleIce(n);                   // plaques glissantes un niveau sur deux (pas dans l'arene du boss)
  if (!bossOn) sprinkleBounce(n);                // un ressort bonus tous les 3 niveaux (hors dizaine 1)
  sprinkleCoin(n);                               // 1 piece par niveau (boutique)
  fillStars();                                  // toujours 3 etoiles
}

char tileAt(int tx, int ty) {
  const bool fi = bqFiche && bqOn();                 // BRIQUE fiche : "bord" boucle = cotes ouverts, mur = fond solide
  if (tx < 0 || tx >= COLS) return fi && fBord == 2 ? '.' : '#';
  if (ty >= levH && fi && fBord == 0) return '#';
  if (ty < 0 || ty >= levH) return '.';
  return grid[ty][tx];
}
bool solid(int tx, int ty) {
  char c = tileAt(tx, ty);
  if (c == '#' || c == 'B' || c == 'R' || c == 'I') return true;   // I = plaque glissante (solide, peu de frottement)
  if (c == 'C') { int i = crumbAt(tx, ty); return (i < 0) || crumbs[i].state != 2; }  // solide tant que pas effondree
  return false;
}

// ================= JOUEUR =================
const float PW = 12, PH = 14;
float px, py, vx, vy;
float spawnX, spawnY, camY;
bool onGround, wallSliding, wasSliding;
int  wallDir;
float wallTimer, jumpLock, coyote, jumpBuf, clingLock;
bool flying, airJumped; float flyTap;   // capacites boutique : vol en cours, saut aerien utilise, delai du double-appui
bool prevJump, prevLeft, prevRight;
bool playerFacingRight = true;
float sqX = 1, sqY = 1;                  // deformation (squash & stretch) : saut, atterrissage
bool pushVis;                            // en train de pousser une brique (penche/ecrase)   // derniere direction de deplacement -- sert a draper la cape a l'oppose (dans le dos)

bool overlapsType(char t) {
  int l = (int)floor(px / TILE), r = (int)floor((px + PW - 1) / TILE);
  int tp = (int)floor(py / TILE), bt = (int)floor((py + PH - 1) / TILE);
  for (int ty = tp; ty <= bt; ty++)
    for (int tx = l; tx <= r; tx++)
      if (tileAt(tx, ty) == t) return true;
  return false;
}

void findSpawn() {
  spawnX = TILE; spawnY = TILE;
  for (int ty = 0; ty < levH; ty++)
    for (int tx = 0; tx < COLS; tx++)
      if (grid[ty][tx] == 'S') { spawnX = tx * TILE + (TILE - PW) / 2; spawnY = ty * TILE + (TILE - PH); }
}

void findGoal() {
  int fx = 1, fy = 1;
  if (bqFiche && bqOn()) fx = fy = -9;               // BRIQUE fiche sans arrivee : drapeau hors du niveau
  for (int ty = 0; ty < levH; ty++)
    for (int tx = 0; tx < COLS; tx++)
      if (grid[ty][tx] == 'G') { fx = tx; fy = ty; grid[ty][tx] = '.'; }
  // la vraie position (le 'G') est la derniere de la sequence
  gpX[gpN] = fx; gpY[gpN] = fy; gpN++;
  gpIdx = 0; goalTX = gpX[0]; goalTY = gpY[0];
}

float camTarget() {
  float t = (bqFiche && bqOn() && fCamFixe ? spawnY : py) + PH / 2 - SCREEN_H / 2;   // BRIQUE fiche : camera fixe = l'ecran du depart
  float mx = levH * TILE - SCREEN_H; if (mx < 0) mx = 0;
  if (t < 0) t = 0; if (t > mx) t = mx;
  return t;
}

// une brique ne tombe pas sur le joueur (elle attend qu'il se pousse)
bool playerInCell(int tx, int ty) {
  return px + PW > tx * TILE && px < tx * TILE + TILE && py + PH > ty * TILE && py < ty * TILE + TILE;
}
void pbMove(int i, int nx, int ny) {
  PushB &b = pbs[i];
  grid[b.ty][b.tx] = b.under;
  b.under = grid[ny][nx]; grid[ny][nx] = 'B';
  b.ox += (b.tx - nx) * TILE; b.oy += (b.ty - ny) * TILE;   // l'affichage glisse vers la nouvelle case
  b.tx = nx; b.ty = ny;
}
// gravite : toutes les briques tombent tant que la case dessous est vide (ou un pic, qu'elles recouvrent)
void pbSettle() {
  bool moved = true;
  while (moved) {
    moved = false;
    for (int i = 0; i < nPB; i++) {
      PushB &b = pbs[i];
      if (b.ty + 1 >= levH) continue;
      char c = grid[b.ty + 1][b.tx];
      if ((c == '.' || c == '^' || c == 'X') && !playerInCell(b.tx, b.ty + 1)) { pbMove(i, b.tx, b.ty + 1); moved = true; }
    }
  }
}
// pousse la rangee de briques qui commence en (tx,ty) dans la direction dir, si la case apres est libre
bool pbPush(int tx, int ty, int dir) {
  int e = tx;
  while (e + dir >= 0 && e + dir < COLS && grid[ty][e + dir] == 'B') e += dir;
  int dest = e + dir;
  if (dest < 0 || dest >= COLS || grid[ty][dest] != '.') return false;
  for (int x = e; x != tx - dir; x -= dir) { int i = pbAt(x, ty); if (i >= 0) pbMove(i, x + dir, ty); }
  pbSettle();
  return true;
}

void respawn() {
  px = spawnX; py = spawnY; vx = vy = 0;
  onGround = wallSliding = wasSliding = false;
  flagSlide = flagDone = false; flagPopT = 0; happyT = excitedT = 0; for (int i = 0; i < 12; i++) sparks[i].life = 0; fallPeakY = py;
  wallDir = 0; wallTimer = 0; jumpLock = 0; coyote = 0; jumpBuf = 0; clingLock = 0; flying = false; airJumped = false; flyTap = 0;
  for (int i = 0; i < 4; i++) bbul[i].a = false;
  bossShot = 1.2f;
  for (int i = 0; i < nStarPos; i++) starGot[i] = false;
  for (int i = 0; i < nTraps; i++) {
    Trap &t = traps[i];
    if (t.kind == KSPIKE) { t.st = false; t.t = 0; }
    else { t.st = false; t.st2 = false; t.y = t.ty * TILE; t.vy = 0; }
  }
  gpIdx = 0; if (gpN > 0) { goalTX = gpX[0]; goalTY = gpY[0]; } goalFlash = 0;
  for (int i = 0; i < nCrumbs; i++) { crumbs[i].state = 0; crumbs[i].t = 0; }  // plateformes reformees
  if (bqFiche && bqOn()) ficheReset();               // BRIQUE fiche : objets et briques comme au depart, regles rearmees
  for (int i = 0; i < nPB; i++) grid[pbs[i].ty][pbs[i].tx] = pbs[i].under;       // briques remises au depart
  for (int i = 0; i < nPB; i++) { PushB &b = pbs[i]; b.tx = b.sx; b.ty = b.sy; b.under = b.under0; b.ox = b.oy = 0; grid[b.ty][b.tx] = 'B'; }
  pushT = 0;
  for (int i = 0; i < nEnemies; i++) enemies[i].active = true;   // ennemis revenus
  camY = camTarget();
}

// ================= ETAT / MENUS =================
enum GameState { ST_SPLASH, ST_HOME, ST_LOBBY, ST_MODE, ST_SELECT, ST_COUNTDOWN, ST_PLAY, ST_END, ST_WIN, ST_DEAD, ST_RACEEND, ST_PONG, ST_QUIZ, ST_QUIZRESULT, ST_BRICK, ST_INVADERS, ST_CPINTRO, ST_SHOP, ST_LB, ST_NAME, ST_DESERT, ST_SHADOW, ST_SAMU, ST_ALTO, ST_TIMING, ST_DRIFT, ST_MYLV, ST_SMASH, ST_BQQ, ST_ISO };
GameState state;
int curLevel, selLevel, endChoice, deadChoice, homeChoice, modeChoice;
unsigned int deaths;
unsigned long cdStart, raceStartMs, lastPosSend, lastHello, lastPongSend;
bool lobbyWasPeer;             // le salon a deja traite l'appairage (remis a false a chaque entree dans le salon)
uint32_t myHelloCount;         // nb de HELLO envoyes cette session lobby : sert a designer l'hote (arrive en 1er)
int raceResult;              // 0 en cours, 1 gagne, 2 perdu
uint32_t myFinT;

// --- stats & combos ---
unsigned long levelStartMs;
int  combo, bestCombo;
uint32_t totalScore, totalPlaytime, totalDeaths, totalWins;
int levelCoins;             // pieces ramassees dans ce niveau (recap)
uint32_t coins;              // boutique : 1 piece/niveau ramassee (tuile 'O'), prefs "coins"
uint8_t  speedBoostLv;       // boutique : palier de vitesse achete (0-5), prefs "boost"
// skins (boutique) : couleur du corps du joueur, achetee avec les pieces. 0 = classique
// (couleur evoluant avec les recompenses, gratuit), 7 = arc-en-ciel anime.
#define NSKIN 8
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
const char*    SKIN_NAME[NSKIN] = { "Classique", "Bleu", "Vert", "Jaune", "Violet", "Blanc", "Ombre", "Arc-en-ciel" };
const uint16_t SKIN_COL[NSKIN]  = { 0, RGB565(60,140,255), RGB565(80,220,110), RGB565(255,215,60), RGB565(170,100,240), RGB565(240,240,250), RGB565(55,55,70), RGB565(255,100,100) };
const uint16_t SKIN_COLD[NSKIN] = { 0, RGB565(30,70,160),  RGB565(30,130,60),  RGB565(170,130,10), RGB565(90,40,150),   RGB565(140,140,160), RGB565(170,170,200), RGB565(120,40,40) };
const uint8_t  SKIN_COST[NSKIN] = { 0, 40, 40, 60, 60, 90, 120, 200 };   // plus chers que les trainees
uint8_t  skinSel;            // skin equipe, prefs "skin"
uint32_t skinOwned = 1;      // masque de skins achetes (bit 0 toujours), prefs "skins"
// trainees de couleur (boutique) : prix croissants. 0 = auto (palier de recompense, gratuit), dernier = arc-en-ciel.
#define NTRAIL 6
const char*    TRAIL_NAME[NTRAIL] = { "Auto", "Rouge", "Cyan", "Rose", "Or", "Arc-en-ciel" };
const uint16_t TRAIL_COL[NTRAIL]  = { 0, RGB565(255,70,70), RGB565(70,230,255), RGB565(255,110,200), RGB565(255,210,60), 0 };
const uint8_t  TRAIL_COST[NTRAIL] = { 0, 10, 15, 25, 40, 60 };
uint8_t  trailSel;           // trainee equipee, prefs "trail"
uint32_t trailOwned = 1;     // masque achete (bit 0 toujours), prefs "trails"
// capacites temporaires (RAM, comptees seulement pendant le jeu) : 330 s par achat, cumulables
#define AB_N 3
#define AB_SEC 330
const char*   AB_NAME[AB_N] = { "Turbo", "Double saut", "Vol" };
const char*   AB_DESC[AB_N] = { "Course +35%", "Un 2e saut en l'air", "Double appui = voler (saut = monter)" };
const uint8_t AB_COST[AB_N] = { 30, 40, 60 };
float abilT[AB_N];           // secondes restantes : 0 turbo, 1 double saut, 2 vol
#define SK0 (1 + AB_N)               // premier skin
#define TR0 (SK0 + NSKIN - 1)        // trainee k = item - TR0 (k = 1..NTRAIL-1)
#define SHOP_N (TR0 + NTRAIL + 1)    // indices : 0 vitesse, 1..AB_N capacites, SK0.. skins, TR0+1.. trainees, SHOP_BACK
#define SHOP_BACK (SHOP_N - 1)
int shopSel;                 // carrousel boutique
// forme du corps selon le skin (sh = indice du skin) : 0 bloc, 1 rond, 2 oreilles de chat, 3 couronne, 4 cornes,
// 5 fantome (bas ondule), 6 robot (carre + antenne), 7 aureole. Purement visuel : la hitbox ne change pas.
// g = fb (jeu) ou tft (mascotte, boutique) ; (x0,y0,w,h) = boite du corps, rd = rayon des coins.
void skinBody(Adafruit_GFX &g, int sh, int x0, int y0, int w, int h, int rd, uint16_t c, uint16_t d) {
  int e = max(2, h / 4), hw = max(3, w / 3), hb = h;
  if (sh == 1 || sh == 5) rd = min(w, h) / 2;
  if (sh == 6) rd = max(1, rd / 3);
  if (sh == 5) hb = h - e / 2;
  if (sh == 2 || sh == 3 || sh == 4) {                    // pointes sur la tete : oreilles / couronne / cornes
    uint16_t pc = sh == 3 ? COL_STAR : sh == 4 ? RGB565(220, 50, 50) : c;
    int n = sh == 3 ? 3 : 2;
    for (int k = 0; k < n; k++) {
      int xk = n == 2 ? (k ? x0 + w - hw : x0) : x0 + k * (w - hw) / 2;
      int tipx = xk + hw / 2 + (sh == 4 ? (k ? 1 : -1) * hw / 3 : 0);
      g.fillTriangle(xk, y0 + 3, tipx, y0 - e, xk + hw, y0 + 3, pc);
      g.drawTriangle(xk, y0 + 3, tipx, y0 - e, xk + hw, y0 + 3, d);
    }
  } else if (sh == 6) { g.drawFastVLine(x0 + w / 2, y0 - e, e, d); g.fillCircle(x0 + w / 2, y0 - e, max(1, e / 3), RGB565(255, 80, 80)); }
  else if (sh == 7) g.drawRoundRect(x0 + w / 4, y0 - e, w / 2, max(2, e - 1), 1, COL_STAR);
  g.fillRoundRect(x0, y0, w, hb, rd, c);
  g.drawRoundRect(x0, y0, w, hb, rd, d);
  if (sh == 5) for (int k = 0; k < 3; k++) {              // bas de fantome : 3 pointes
    int xa = x0 + k * w / 3, xb = x0 + (k + 1) * w / 3;
    g.fillTriangle(xa, y0 + hb - 1, (xa + xb) / 2, y0 + h, xb, y0 + hb - 1, c);
    g.drawLine(xa, y0 + hb - 1, (xa + xb) / 2, y0 + h, d); g.drawLine((xa + xb) / 2, y0 + h, xb, y0 + hb - 1, d);
  }
}
uint16_t skinColor(int i) {
  if (i == 7) { static const uint16_t rb[5] = { 0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF }; return rb[(millis() / 160) % 5]; }
  return SKIN_COL[i];
}
#define SPEED_BOOST_MAX 5
#define SPEED_BOOST_COST 20  // pieces par palier
uint32_t bestTime[NUM_LEVELS];        // ms (0 = jamais fini)
int  lastScore, lastTimeMs, lastCombo, lastStars;
int  scBase, scTime, scStars, scCombo, scPen, scFlag;   // decomposition du score (recap)
bool lastBestTime, lastBestStars;

Preferences prefs;
uint8_t bestStars[NUM_LEVELS];
void saveStars() { prefs.putBytes("stars", bestStars, NUM_LEVELS); }
void saveTimes() { prefs.putBytes("times", bestTime, sizeof(bestTime)); }
void saveStats() {
  prefs.putUInt("score", totalScore); prefs.putUInt("play", totalPlaytime);
  prefs.putUInt("deaths", totalDeaths); prefs.putUInt("wins", totalWins);
  prefs.putInt("bcombo", bestCombo);
}

// --- melange niveaux/mini-jeux (retour) : une dizaine de niveaux de plateforme
// terminee -> mini-jeu bonus (Pong/Casse-brique/Invaders, en rotation) a
// jouer une fois avant de debloquer la dizaine suivante. decadeGateCleared[d]
// = la dizaine d (0-9, niveaux d*10+1 a d*10+10) a ete suivie de son mini-jeu.
#define NUM_DECADES (NUM_LEVELS / 10)
uint8_t decadeGateCleared[NUM_DECADES];
void saveGates() { prefs.putBytes("gates", decadeGateCleared, NUM_DECADES); }
bool checkpointActive = false;
int  checkpointDecade = -1, checkpointNextLevel = -1;
int  decadeOf(int idx) { return idx / 10; }
#define UNLOCKED_UP_TO 49                       // retour : les niveaux 1 a 50 (indices 0..UNLOCKED_UP_TO) sont disponibles d'emblee, sans mini-jeu bonus
bool gateBlocks(int idx) {                      // la 1ere dizaine n'est jamais bloquee
  if (idx <= UNLOCKED_UP_TO) return false;
  int d = decadeOf(idx);
  return d > 0 && !decadeGateCleared[d - 1];
}
void beginCheckpoint(int forLevel) {             // annonce le mini-jeu bonus avant de le lancer
  checkpointActive = true;
  checkpointDecade = decadeOf(forLevel) - 1;
  checkpointNextLevel = forLevel;
  state = ST_CPINTRO;
  drawCheckpointIntro();
}
// retour "il faut gagner un niveau pour passer, pas attendre de perdre" :
// appelee au moment precis ou le joueur GAGNE (niveau casse-brique termine,
// vague invaders videe, point marque au pong) pendant un palier bonus --
// debloque la dizaine tout de suite, pas besoin d'aller jusqu'au game over.
void checkpointWin() {
  if (checkpointDecade >= 0 && checkpointDecade < NUM_DECADES) { decadeGateCleared[checkpointDecade] = 1; saveGates(); }
  checkpointActive = false;
  startLevel(checkpointNextLevel, true);
}
// sortie normale (perdu/quitte) d'un mini-jeu : si c'etait un palier bonus
// pas encore gagne, la dizaine reste bloquee -- retour a l'accueil, on pourra
// retenter le palier en resselectionnant le niveau depuis "Niveaux".
void checkpointFinish() {
  checkpointActive = false;
  homeChoice = 0; state = ST_HOME; drawHome();
}

// ================= ESP-NOW (mode 2 joueurs) =================
static const uint8_t BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t myMac[6], peerMac[6];
bool espOn = false, twoP = false, isHost = false, raceMode = false;
#define M_HELLO 'H'
#define M_START 'S'
#define M_POS   'P'
#define M_FIN   'F'
#define M_STAR  'T'
#define M_COIN  'C'   // piece prise par l'adversaire (x,y = tuile) : disparait aussi chez moi
#define M_PONG  'O'   // etat autoritaire du pong reseau (hote -> invite)
#define M_PQUIT 'Q'   // un des deux quitte le pong reseau
#define PONG_SENTINEL 255   // valeur bidon de "niveau" dans M_START pour dire "lance le pong" plutot qu'un niveau
#define M_ALIVE 'A'   // signal de vie 2 joueurs (toutes les 400 ms, dans tous les etats) : aucun paquet depuis 3 s = adversaire deconnecte
#define M_TMRES 'R'   // timing a deux : mon resultat (x = centiemes)
#define M_DSEL  'D'   // drift a deux : circuit choisi par l'hote (lvl) ; x = 1 -> depart
#define TIMING_SENTINEL 254   // M_START : lance le timing a deux (x = cible en centiemes)
#define DRIFT_SENTINEL  253   // M_START : lance le drift a deux
#define BRIQUE_SENTINEL 252   // M_START : course sur un jeu Brique publie (t = son code de partage, 30 bits)
#define M_ROSTER 'W'  // multijoueur : l'hote fige la liste des joueurs (lvl = nombre, ids dans x, y, x2, t)
// from = identifiant de l'expediteur (2 derniers octets de sa MAC) : 2 a 4 consoles, chacune sait qui parle (et le banc peut simuler un joueur de plus)
volatile bool vPeer = false, vStart = false, vPos = false, vFin = false;
volatile uint8_t vStartLvl = 0;
volatile int16_t vOppX = 0, vOppY = 0;
volatile uint32_t vFinT = 0;
volatile uint32_t vLastRx = 0;                             // dernier paquet recu de l'adversaire (detection de deconnexion)
volatile uint32_t vHelloN = 0;                             // HELLO recus : l'adversaire est dans le lobby
volatile uint32_t vStartT = 0;        // M_START : code du jeu Brique
volatile int16_t vStartX = 0, vTmRes = 0, vOppH = 0;
volatile bool vTmResOk = false, vDSelNew = false; volatile uint8_t vDSel = 0, vDGo = 0;
volatile uint32_t vPeerHelloCount = 0;                     // pour designer l'hote = celui arrive en premier
volatile int16_t vPongBallX = 0, vPongBallY = 0, vPongHostX = 0;
volatile uint8_t vPongScoreNear = 0, vPongScoreFar = 0;
volatile bool vPongState = false, vPongQuit = false;
volatile int16_t vCoinX[8], vCoinY[8]; volatile uint8_t vCoinHead = 0, vCoinTail = 0;   // file des pieces prises par l'adversaire (ecrite par le callback WiFi, videe dans playStep)

// --- classement partage (mode "Classement") : chaque console diffuse sa table
// (id = 2 derniers octets de la MAC -> nom "C3-XXXX", score total) ; on fusionne en
// gardant le meilleur score par id, donc les scores se propagent de console en console.
#define M_LB 'L'
char playerName[7];                                        // mon nom (NVS "pname"), 6 car. max
int namePos, nameIdx[6]; unsigned long nameHoldStart, nameRep;   // saisie du nom (ST_NAME)
const char NAME_SET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 <>";   // < = retour arriere, > = valider
#define NAME_N ((int)sizeof(NAME_SET) - 1)
LbEnt lbTab[LB_MAX]; int lbN;                              // tri decroissant par score
LbMsg lbRx; volatile bool vLbRx = false;                   // dernier paquet recu (fusionne dans loop, pas dans le callback WiFi)
bool lbDirty, lbSave; unsigned long lbSentMs, lbSavedMs;

uint32_t espDiag[6];
volatile uint32_t espRxN = 0; volatile int espTxErr = 0;   // diagnostic 2 joueurs : paquets recus, dernier code d'envoi
// ---- multijoueur (2 a 4 consoles) : table des autres joueurs, indice k = ordre d'arrivee dans le lobby ----
#define MP_MAX 3                                           // 3 adversaires au plus (4 joueurs)
volatile uint8_t mpN = 0;                                  // autres joueurs vus
uint16_t mpId[MP_MAX]; uint8_t mpMac[MP_MAX][6];
volatile bool mpIn[MP_MAX], mpOn[MP_MAX], mpQuit[MP_MAX];  // mpIn : dans la partie (liste figee) ; mpOn : encore la (pas parti)
volatile uint32_t mpRx[MP_MAX], mpHello[MP_MAX], mpPosMs[MP_MAX], mpFinT[MP_MAX];
volatile int16_t mpX[MP_MAX], mpY[MP_MAX], mpH[MP_MAX], mpTm[MP_MAX];
volatile bool mpPos[MP_MAX], mpFin[MP_MAX], mpTmOk[MP_MAX];
volatile bool vRoster = false; volatile uint8_t vRosterN = 0; volatile uint16_t vRosterIds[4], vRosterFrom = 0;
volatile uint32_t mpHelloMs[MP_MAX];                      // dernier HELLO de chaque joueur (il est dans le salon : il veut (re)joindre)
volatile uint32_t vSessionMs = 0;                          // derniere trace d'une partie en cours autour de moi (signal de vie / liste d'un autre hote)
volatile int8_t vFinWho = -1;                              // course : premier adversaire arrive
bool mpFrozen = false;                                     // liste des joueurs figee (l'hote a lance)
uint16_t mpMe = 0, mpHostId = 0;                           // mon identifiant, celui de l'hote
const uint16_t MP_COL[4] = { RGB565(90, 200, 255), RGB565(255, 120, 40), RGB565(80, 230, 110), RGB565(200, 110, 255) };   // couleur de chaque joueur
const char *const MP_NAME[4] = { "BLEU", "ORANGE", "VERT", "VIOLET" };
uint8_t mpCol[MP_MAX], myCol = 0;                          // couleur fixe de chaque joueur, donnee par l'hote (dans la liste) : la meme sur toutes les consoles
volatile uint8_t vRosterCol = 0;                           // couleurs de la liste recue (2 bits par joueur, dans b1)
volatile bool vYield = false;
uint32_t lobbyInMs = 0;                                    // entree dans le salon (pas d'election d'hote pendant 1,5 s : une partie se lance peut-etre a cote)                              // un autre hote (identifiant plus petit) tient une partie : je lui cede la place
#ifndef MP_VIRT_DROP_MS
#define MP_VIRT_DROP_MS 0                                  // banc : le joueur simule disparait apres ce delai (0 = jamais)
#endif
#ifndef MP_VIRT_ID
#define MP_VIRT_ID 0xFFF0                                  // banc : identifiant du joueur simule (jamais hote)
#endif
int mpFind(uint16_t id) { for (int k = 0; k < mpN; k++) if (mpId[k] == id) return k; return -1; }
int mpActive() { int n = 0; for (int k = 0; k < mpN; k++) if (mpIn[k] && mpOn[k]) n++; return n; }   // adversaires encore en partie
int mpPlayers() { return 1 + (mpFrozen ? mpActive() : 0); }
int mpRank(uint16_t id) {                                  // numero de joueur J1..J4 : rang de l'identifiant parmi les joueurs de la partie (le meme sur toutes les consoles)
  int r = 1; if (mpMe < id) r++;
  for (int k = 0; k < mpN; k++) if (mpIn[k] && mpId[k] < id) r++;
  return r;
}
int mpRankK(int k) { return mpRank(mpId[k]); }
void onEspRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  espRxN++;
  if (len >= 2 && data[0] == M_LB) {                       // paquet classement (plus court que Msg : a traiter avant le test de taille)
    int n = min((int)data[1], LB_MAX);
    if (!vLbRx && len >= 2 + n * (int)sizeof(LbEnt)) { memcpy(&lbRx, data, 2 + n * sizeof(LbEnt)); lbRx.n = n; vLbRx = true; }
    return;
  }
  if (len < (int)sizeof(Msg)) return;
  Msg m; memcpy(&m, data, sizeof(Msg));
  mpHandle(m, info->src_addr);
}
void mpHandle(const Msg &m, const uint8_t *mac) {          // un message d'un autre joueur (radio, ou joueur simule du banc)
  if (m.from == mpMe) return;
  const uint32_t now = millis();
  int k = mpFind(m.from);
  if (k < 0) {                                             // nouveau : HELLO (salon), ou signal de vie / liste d'une partie en cours (pour pouvoir la rejoindre)
    if (m.type != M_HELLO && m.type != M_ALIVE && m.type != M_ROSTER) return;
    k = mpN < MP_MAX ? mpN : -1;
    if (k < 0) for (int q = 0; q < MP_MAX; q++) { const uint32_t r = mpRx[q]; if (!(mpIn[q] && mpOn[q]) && (int32_t)(now - r) > 5000) { k = q; break; } }   // place d'un joueur parti depuis longtemps
    if (k < 0) return;
    mpId[k] = m.from; memcpy(mpMac[k], mac, 6); mpHello[k] = 0; mpHelloMs[k] = 0; mpPos[k] = mpFin[k] = mpTmOk[k] = mpQuit[k] = mpIn[k] = false; mpOn[k] = true; mpRx[k] = now;
    if (k == mpN) mpN = k + 1;
  }
  mpRx[k] = now;
  if (m.type == M_HELLO) { mpHello[k]++; mpHelloMs[k] = now; mpQuit[k] = false; vHelloN++; vPeer = true; return; }
  if ((m.type == M_ALIVE || m.type == M_ROSTER) && !mpFrozen) vSessionMs = now;   // une partie est en cours autour : on attend d'y etre admis
  if (m.type == M_ROSTER) {
    if (mpFrozen && isHost) { if (m.from < mpMe) vYield = true; return; }   // deux hotes (entres au salon en meme temps) : le plus grand identifiant cede
    vRosterN = m.lvl; vRosterIds[0] = m.x; vRosterIds[1] = m.y; vRosterIds[2] = m.x2; vRosterIds[3] = (uint16_t)m.t; vRosterCol = m.b1; vRosterFrom = m.from; vRoster = true; return;
  }
  if (mpFrozen && !(mpIn[k] && mpOn[k])) return;           // pas (ou plus) dans la partie
  vLastRx = now;
  if (m.type == M_START) { vStartLvl = m.lvl; vStartX = m.x; vStartT = m.t; vStart = true; }
  else if (m.type == M_TMRES) { mpTm[k] = m.x; mpTmOk[k] = true; }
  else if (m.type == M_DSEL)  { vDSel = m.lvl; vDGo = (uint8_t)m.x; vDSelNew = true; }
  else if (m.type == M_POS)   { mpX[k] = m.x; mpY[k] = m.y; mpH[k] = m.x2; mpPos[k] = true; mpPosMs[k] = now; vOppX = m.x; vOppY = m.y; vOppH = m.x2; vPos = true; }
  else if (m.type == M_FIN)   { mpFinT[k] = m.t; mpFin[k] = true; if (vFinWho < 0) vFinWho = k; vFinT = m.t; vFin = true; }
  // etoile prise par un adversaire dans le meme niveau : disparait aussi chez moi
  else if (m.type == M_STAR)  { if (m.lvl == curLevel && m.x >= 0 && m.x < 3) starGot[m.x] = true; }
  else if (m.type == M_COIN)  { if (m.lvl == (uint8_t)curLevel) { uint8_t h = vCoinHead; vCoinX[h & 7] = m.x; vCoinY[h & 7] = m.y; vCoinHead = h + 1; } }
  else if (m.type == M_PONG)  { vPongBallX = m.x; vPongBallY = m.y; vPongHostX = m.x2; vPongScoreNear = m.b1; vPongScoreFar = m.b2; vPongState = true; }
  else if (m.type == M_PQUIT) { mpQuit[k] = true; vPongQuit = true; }
}
bool peerAdded = false;
// Enregistre l'adversaire comme peer unicast : les trames unicast beneficient
// d'un ACK + retransmission automatique au niveau WiFi, contrairement au
// broadcast (aucun ACK, aucune retry) -> beaucoup moins de paquets perdus.
void mpAddPeers() {                                       // unicast vers chaque joueur de la partie (ACK + retransmission, contrairement au broadcast)
  for (int k = 0; k < mpN; k++) if (mpIn[k]) { esp_now_peer_info_t p = {}; memcpy(p.peer_addr, mpMac[k], 6); p.channel = 1; p.encrypt = false; esp_now_add_peer(&p); }
}
void mpTx(const uint8_t *dst, const Msg &m) {             // file d'envoi pleine (NO_MEM) : on laisse 2 ms au Wi-Fi et on reessaie une fois
  esp_err_t r = esp_now_send(dst, (const uint8_t *)&m, sizeof(m));
  if (r == ESP_ERR_ESPNOW_NO_MEM) { delay(2); r = esp_now_send(dst, (const uint8_t *)&m, sizeof(m)); }
  espTxErr = (int)r;
}
void mpRadio(const Msg &m) {                               // HELLO / liste / avant le depart : broadcast ; ensuite une trame par console (meme MAC = une seule)
  if (!mpFrozen || m.type == M_HELLO || m.type == M_ROSTER || m.type == M_ALIVE) { mpTx(BCAST, m); return; }   // signal de vie en broadcast : une console qui arrive voit la partie
  for (int k = 0; k < mpN; k++) {
    if (!(mpIn[k] && mpOn[k]) || !memcmp(mpMac[k], myMac, 6)) continue;   // pas a soi-meme (joueur simule du banc : meme MAC)
    bool dup = false; for (int q = 0; q < k; q++) if (mpIn[q] && mpOn[q] && !memcmp(mpMac[q], mpMac[k], 6)) dup = true;
    if (!dup) mpTx(mpMac[k], m);
  }
}
void mpSendTo(int k, uint8_t type, uint8_t lvl, int16_t x, int16_t y, uint32_t t, int16_t x2) {   // a un seul joueur (celui qu'on vient d'admettre)
  if (!espOn) return;
  Msg m{}; m.type = type; m.lvl = lvl; m.x = x; m.y = y; m.t = t; m.x2 = x2; m.from = mpMe; mpTx(mpMac[k], m);
#ifdef MP_VIRT
  if (!memcmp(mpMac[k], myMac, 6)) mpHandle(m, myMac);   // banc : le joueur simule, c'est moi
#endif
}
void mpSendMsg(Msg &m) {
  if (!espOn) return;
  m.from = mpMe; mpRadio(m);
#ifdef MP_VIRT                                             // banc : un joueur de plus, simule par cette console (meme MAC, autre identifiant)
  static uint32_t virtGone = 0;
  if (MP_VIRT_DROP_MS > 0 && mpFrozen && virtGone == 0) virtGone = millis() + MP_VIRT_DROP_MS;
  if (!(virtGone && millis() > virtGone) && m.type != M_START && m.type != M_ROSTER && m.type != M_DSEL && m.type != M_PQUIT && (mpFrozen || m.type == M_HELLO)) {
    Msg v = m; v.from = MP_VIRT_ID;
    if (v.type == M_POS) v.x += 30; else if (v.type == M_TMRES) v.x += 9; else if (v.type == M_FIN) v.t += 700;
    mpRadio(v); mpHandle(v, myMac);
  }
#endif
}
void espSend(uint8_t type, uint8_t lvl, int16_t x, int16_t y, uint32_t t) { Msg m{}; m.type = type; m.lvl = lvl; m.x = x; m.y = y; m.t = t; mpSendMsg(m); }
void espSend2(uint8_t type, uint8_t lvl, int16_t x, int16_t y, uint32_t t, int16_t x2) {   // comme espSend, avec le champ x2 (cap du drift)
  Msg m{}; m.type = type; m.lvl = lvl; m.x = x; m.y = y; m.t = t; m.x2 = x2; mpSendMsg(m);
}
// etat autoritaire du pong (hote -> invite) : balle + raquette du bas + scores
void espSendPong(int16_t ballX, int16_t ballY, int16_t nearX, uint8_t scNear, uint8_t scFar) {
  Msg m{}; m.type = M_PONG; m.x = ballX; m.y = ballY; m.x2 = nearX; m.b1 = scNear; m.b2 = scFar; mpSendMsg(m);
}
void startEspNow() {
  if (espOn) return;
  dSpiStop(); dFreeOb();
  espDiag[0] = ESP.getFreeHeap(); espDiag[1] = ESP.getMaxAllocHeap();
  // Wi-Fi au minimum pour ESP-NOW : WiFi.mode() d'Arduino demarre aussi lwIP / esp_netif et prenait 54 Ko (il ne restait que 5 Ko : le pilote
  // lachait, ESP-NOW n'etait plus initialise, 2 joueurs casse). Ici : pas de pile reseau, peu de tampons, pas d'agregation (ampdu).
  esp_event_loop_create_default();                       // deja cree : ESP_ERR_INVALID_STATE, sans consequence
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.static_rx_buf_num = 4; cfg.dynamic_rx_buf_num = 8; cfg.tx_buf_type = 1; cfg.static_tx_buf_num = 0; cfg.dynamic_tx_buf_num = 8; cfg.cache_tx_buf_num = 0;
  cfg.ampdu_rx_enable = 0; cfg.ampdu_tx_enable = 0; cfg.amsdu_tx_enable = 0; cfg.nvs_enable = 0;
  esp_err_t we = esp_wifi_init(&cfg);
  if (we == ESP_OK) { esp_wifi_set_storage(WIFI_STORAGE_RAM); esp_wifi_set_mode(WIFI_MODE_STA); we = esp_wifi_start(); }
  espDiag[2] = (uint32_t)we; espDiag[3] = ESP.getFreeHeap();
  if (we != ESP_OK) { DPRINTF("[ESPNOW] wifi echec %d heap=%u\n", (int)we, (unsigned)ESP.getFreeHeap()); esp_wifi_deinit(); return; }
  esp_wifi_set_max_tx_power(60);                         // 15 dBm (unite 0,25 dBm) : etait 5 dBm, monte pour reduire les decrochages
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_get_mac(WIFI_IF_STA, myMac); mpMe = ((uint16_t)myMac[4] << 8) | myMac[5];
  { esp_err_t e = esp_now_init(); espDiag[4] = (uint32_t)e; espDiag[5] = ESP.getFreeHeap(); if (e != ESP_OK) { DPRINTF("[ESPNOW] init echec %d heap=%u\n", (int)e, (unsigned)ESP.getFreeHeap()); return; } }
  esp_now_register_recv_cb(onEspRecv);
  esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = 1; p.encrypt = false;
  esp_now_add_peer(&p);
  espOn = true;
}
void stopEspNow() {
  if (!espOn) return;
  esp_now_deinit(); esp_wifi_stop(); esp_wifi_deinit(); espOn = false;
}
uint16_t myId() { return ((uint16_t)myMac[4] << 8) | myMac[5]; }
void lbMerge(const LbEnt &e) {
  int i = 0; while (i < lbN && lbTab[i].id != e.id) i++;
  if (i < lbN) { LbEnt &c = lbTab[i]; if (e.score < c.score || (e.score == c.score && (int8_t)(e.rev - c.rev) <= 0)) return; c = e; }   // meilleur score, ou meme score et version plus recente
  else if (lbN < LB_MAX) lbTab[lbN++] = e;
  else if (e.score > lbTab[lbN - 1].score) { i = lbN - 1; lbTab[i] = e; }
  else return;
  for (; i > 0 && lbTab[i].score > lbTab[i - 1].score; i--) { LbEnt t = lbTab[i]; lbTab[i] = lbTab[i - 1]; lbTab[i - 1] = t; }
  lbDirty = lbSave = true;
}
void lbSaveNvs() { prefs.putBytes("pname", playerName, 6); prefs.putUChar("lbn", lbN); prefs.putBytes("lb", lbTab, lbN * sizeof(LbEnt)); lbSave = false; lbSavedMs = millis(); }
void lbLoad() {
  if (prefs.getBytes("pname", playerName, 6) != 6) playerName[0] = 0;
  playerName[6] = 0;
  lbN = min((int)prefs.getUChar("lbn", 0), LB_MAX);
  if (lbN) prefs.getBytes("lb", lbTab, lbN * sizeof(LbEnt));
}
int macCmp(const uint8_t *a, const uint8_t *b) { for (int i = 0; i < 6; i++) { if (a[i] < b[i]) return -1; if (a[i] > b[i]) return 1; } return 0; }

// ================= PONG (mini-jeu, choisi depuis l'accueil) =================
// Se joue de GAUCHE a DROITE : raquettes verticales, "near" = gauche, "far" = droite.
// GAUCHE (P2) = monter, DROITE (P1) = descendre.
// Solo : gauche = joueur, droite = IA. A deux : gauche = hote, droite = invite.
// L'hote simule tout (autoritaire) et diffuse l'etat (M_PONG) ; l'invite se
// contente d'afficher + d'envoyer la position Y de sa raquette (reutilise M_POS).
#define PONG_PW 8                  // epaisseur raquette
#define PONG_PL 50                 // longueur raquette
#define PONG_PLY_X 10              // raquette gauche
#define PONG_AI_X  (SCREEN_W - 10 - PONG_PW)   // raquette droite
bool pongTwoP = false;                                  // partie en reseau (sinon vs IA)
float pgMyY;                                             // MA raquette (entree locale)
float pgNearY, pgFarY;                                   // etat affiche : raquette gauche / droite
float pgBallX, pgBallY, pgBallVX, pgBallVY, pgServeT;
int pgScoreNear, pgScoreFar;

// --- options/capsules (retour : "ajoute des options (boost, etc)") --
// SOLO uniquement (pas de sync reseau necessaire) : une capsule apparait de
// temps en temps a hauteur aleatoire sur la colonne de la raquette joueur,
// a aller chercher en bougeant GAUCHE/DROITE (= monter/descendre) avant
// qu'elle n'expire. GROW/SHRINK changent la longueur de SA raquette
// (pgNearLen, la raquette IA garde toujours PONG_PL) ; FAST/SLOW changent
// la vitesse de la balle pour le reste de l'echange.
enum { PG_GROW, PG_SHRINK, PG_FAST, PG_SLOW, PG_N };
#define PONG_CAP_SZ 14
bool pgCapOn; float pgCapY, pgCapT, pgSpawnT; uint8_t pgCapType;
float pgNearLen;                    // longueur courante de la raquette joueur (PONG_PL +/- GROW/SHRINK)
float pgGrowT, pgShrinkT;           // minuteries des effets de longueur actifs
float pgCapMsgT; char pgCapMsg[16];

void pongReset(int dir) {                 // dir : +1 balle vers la droite, -1 vers la gauche
  pgBallX = SCREEN_W / 2.0f; pgBallY = SCREEN_H / 2.0f;
  float ang = random(-35, 36) * (3.14159f / 180.0f);
  pgBallVX = 190.0f * cos(ang) * dir;
  pgBallVY = 190.0f * sin(ang);
  pgServeT = 0.6f;
}
void startPong() {                        // solo, vs IA
  pongTwoP = false;
  pgMyY = pgNearY = pgFarY = SCREEN_H / 2.0f - PONG_PL / 2.0f;
  pgScoreNear = pgScoreFar = 0;
  pgNearLen = PONG_PL; pgGrowT = pgShrinkT = 0; pgCapOn = false; pgSpawnT = 3.0f; pgCapMsgT = 0;
  pongReset(random(0, 2) ? 1 : -1);
  state = ST_PONG;
}
void startPong2P(bool hostRole) {         // a deux via ESP-NOW ; hostRole : simule et diffuse l'etat
  pongTwoP = true;
  pgMyY = pgNearY = pgFarY = SCREEN_H / 2.0f - PONG_PL / 2.0f;
  pgScoreNear = pgScoreFar = 0;
  pgNearLen = PONG_PL; pgGrowT = pgShrinkT = 0; pgCapOn = false;   // pas de capsules en 2J (pas de sync reseau)
  vPos = false; vPongState = false; vPongQuit = false;
  if (hostRole) pongReset(random(0, 2) ? 1 : -1);
  state = ST_PONG;
}
uint16_t pongCapCol(uint8_t t) {
  switch (t) {
    case PG_GROW:  return tft.color565(120, 230, 90);    // bon : vert
    case PG_FAST:  return tft.color565(255, 160, 40);    // chaos : orange
    case PG_SLOW:  return tft.color565(90, 170, 255);    // bon (defense) : bleu
    default:       return tft.color565(230, 60, 60);     // piege (SHRINK) : rouge
  }
}
const char PONG_CAP_CH[PG_N] = { 'G', 'S', 'F', 'L' };
void drawPong() {
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {
    fb.fillRect(0, 0, SCREEN_W, STRIP_H, COL_BG);
    for (int y = 0; y < SCREEN_H; y += 12) fb.fillRect(SCREEN_W / 2 - 1, y - sTop, 2, 6, COL_BLK);   // filet central
    fb.fillRect(PONG_PLY_X, (int)pgNearY - sTop, PONG_PW, (int)pgNearLen, COL_MEAT);
    fb.fillRect(PONG_AI_X, (int)pgFarY - sTop, PONG_PW, PONG_PL, tft.color565(90, 200, 255));
    if (pgCapOn) {
      int cx = PONG_PLY_X + PONG_PW + 10, cy = (int)pgCapY - sTop;
      fb.fillRoundRect(cx, cy, PONG_CAP_SZ, PONG_CAP_SZ, 3, pongCapCol(pgCapType));
      fb.setTextColor(COL_BG); fb.setTextSize(1); fb.setCursor(cx + 4, cy + 3); fb.print(PONG_CAP_CH[pgCapType]);
    }
    if (pgServeT <= 0 || ((int)(pgServeT * 10)) & 1)
      fb.fillCircle((int)pgBallX, (int)pgBallY - sTop, 4, COL_TXT);
    fb.setTextColor(COL_TXT); fb.setTextSize(2);
    fb.setCursor(SCREEN_W / 2 - 40, 6 - sTop); fb.print(pgScoreNear);
    fb.setCursor(SCREEN_W / 2 + 28, 6 - sTop); fb.print(pgScoreFar);
    if (pgCapMsgT > 0) {
      fb.setTextSize(2); fb.setTextColor(COL_STAR);
      fb.setCursor(SCREEN_W / 2 - (int)strlen(pgCapMsg) * 6, 40 - sTop); fb.print(pgCapMsg);
    }
    fb.setTextSize(1);
    fb.setCursor(90, SCREEN_H - 10 - sTop); fb.print("P3 = quitter vers l'accueil");
    dBlitPlain(sTop);
  }
}
void pongStep(bool bLeft, bool br) {
  static unsigned long lastMs = 0;
  unsigned long now = millis();
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt <= 0 || dt > 0.045f) dt = 0.02f;

  bool amNear = !pongTwoP || isHost;      // solo ou hote = raquette gauche ; invite = raquette droite

  float pspd = 340.0f * dt;               // retour "trop lente" -> 240 -> 340
  if (bLeft) pgMyY -= pspd;               // GAUCHE = monter
  if (br)    pgMyY += pspd;               // DROITE = descendre
  float myLen = amNear ? pgNearLen : PONG_PL;
  if (pgMyY < 0) pgMyY = 0;
  if (pgMyY > SCREEN_H - myLen) pgMyY = SCREEN_H - myLen;
  if (amNear) pgNearY = pgMyY; else pgFarY = pgMyY;

  if (pgCapMsgT > 0) pgCapMsgT -= dt;
  if (pgGrowT > 0) pgGrowT -= dt;
  if (pgShrinkT > 0) pgShrinkT -= dt;
  pgNearLen = PONG_PL + (pgGrowT > 0 ? 18 : 0) - (pgShrinkT > 0 ? 15 : 0);

  if (!pongTwoP) {                        // capsules : solo uniquement (pas de sync reseau en 2J)
    if (pgCapOn) {
      pgCapT -= dt;
      bool hit = pgNearY < pgCapY + PONG_CAP_SZ && pgNearY + pgNearLen > pgCapY;
      if (hit || pgCapT <= 0) {
        if (hit) {
          switch (pgCapType) {
            case PG_GROW:   pgGrowT = 10.0f; pgShrinkT = 0; strcpy(pgCapMsg, "RAQUETTE XL"); break;
            case PG_SHRINK: pgShrinkT = 10.0f; pgGrowT = 0; strcpy(pgCapMsg, "RAQUETTE MINI"); break;
            case PG_FAST:   pgBallVX *= 1.35f; pgBallVY *= 1.35f; strcpy(pgCapMsg, "BOOST !"); break;
            case PG_SLOW:   pgBallVX *= 0.72f; pgBallVY *= 0.72f; strcpy(pgCapMsg, "RALENTI"); break;
          }
          pgCapMsgT = 1.0f;
        }
        pgCapOn = false; pgSpawnT = 6.0f + random(0, 4);
      }
    } else {
      pgSpawnT -= dt;
      if (pgSpawnT <= 0) {
        pgCapOn = true; pgCapType = random(0, PG_N);
        pgCapY = random(10, (int)(SCREEN_H - 10 - PONG_CAP_SZ));
        pgCapT = 5.0f;
      }
    }
  }

  if (pongTwoP && !isHost) {              // invite : pas de simulation, juste envoi + reception
    if (now - lastPongSend > 40) { lastPongSend = now; espSend(M_POS, 0, (int16_t)pgMyY, 0, 0); }
    if (vPongState) {
      vPongState = false;
      pgNearY = vPongHostX; pgBallX = vPongBallX; pgBallY = vPongBallY;
      pgScoreNear = vPongScoreNear; pgScoreFar = vPongScoreFar;
    }
    drawPong();
    return;
  }

  if (!pongTwoP) {                        // solo : IA suit la balle, vitesse limitee
    float target = pgBallY - PONG_PL / 2.0f, aispd = 140.0f * dt;
    if (pgBallVX < 0) target = SCREEN_H / 2.0f - PONG_PL / 2.0f;   // balle qui s'eloigne : l'IA se recentre
    if (pgFarY < target) { pgFarY += aispd; if (pgFarY > target) pgFarY = target; }
    else if (pgFarY > target) { pgFarY -= aispd; if (pgFarY < target) pgFarY = target; }
  } else {                                // hote en 2J : raquette d'en face = derniere position recue
    pgFarY = vOppX;
  }
  if (pgFarY < 0) pgFarY = 0;
  if (pgFarY > SCREEN_H - PONG_PL) pgFarY = SCREEN_H - PONG_PL;

  if (pongTwoP && now - lastPongSend > 40) {   // etat envoye en continu, meme pendant la pause de service
    lastPongSend = now;
    espSendPong((int16_t)pgBallX, (int16_t)pgBallY, (int16_t)pgNearY, (uint8_t)pgScoreNear, (uint8_t)pgScoreFar);
  }
  if (pgServeT > 0) { pgServeT -= dt; drawPong(); return; }

  pgBallX += pgBallVX * dt; pgBallY += pgBallVY * dt;
  if (pgBallY < 4) { pgBallY = 4; pgBallVY = fabs(pgBallVY); }
  if (pgBallY > SCREEN_H - 4) { pgBallY = SCREEN_H - 4; pgBallVY = -fabs(pgBallVY); }

  if (pgBallVX < 0 && pgBallX <= PONG_PLY_X + PONG_PW + 4 && pgBallX >= PONG_PLY_X - 4 &&
      pgBallY >= pgNearY - 4 && pgBallY <= pgNearY + pgNearLen + 4) {
    pgBallX = PONG_PLY_X + PONG_PW + 4; pgBallVX = fabs(pgBallVX) * 1.05f;
    pgBallVY += (pgBallY - (pgNearY + pgNearLen / 2.0f)) / (pgNearLen / 2.0f) * 90.0f;
  }
  if (pgBallVX > 0 && pgBallX >= PONG_AI_X - 4 && pgBallX <= PONG_AI_X + PONG_PW + 4 &&
      pgBallY >= pgFarY - 4 && pgBallY <= pgFarY + PONG_PL + 4) {
    pgBallX = PONG_AI_X - 4; pgBallVX = -fabs(pgBallVX) * 1.05f;
    pgBallVY += (pgBallY - (pgFarY + PONG_PL / 2.0f)) / (PONG_PL / 2.0f) * 90.0f;
  }
  if (pgBallVX > 520) pgBallVX = 520; if (pgBallVX < -520) pgBallVX = -520;   // plafond remonte (capsule FAST)

  if (pgBallX < -8)                { pgScoreFar++;  pongReset(1); }    // rate a gauche -> point pour la droite
  else if (pgBallX > SCREEN_W + 8) {
    pgScoreNear++;
    if (checkpointActive) { checkpointWin(); return; }   // palier bonus : 1er point marque = on passe tout de suite
    pongReset(-1);
  }

  drawPong();
}

// ---- BRIQUE : casse-brique / invaders crees sur la page ("params":{"jeu":"briques"|"invaders"}, decode par bqMiniParse) ----
// bqMini = jeu perso en cours (0 = mini-jeux habituels) : mis par bqStart(), remis a 0 en sortant (checkpointFinish)
// BRIQUE "cote" : jeu vu de cote cree sur la page, joue dans le moteur des mondes (defilement horizontal et vertical)
// 1 a 4 ecrans de large (20 cases chacun, 96 au plus) et 1 ou 2 de haut (15 cases chacun) ; 'T' = bloc qui disparait quand on arrive dessus
#define BQC_HMAX 30
bool bqCote = false; char bqCRows[BQC_HMAX][97]; int bqCW = 0, bqCH = 0;
#define ISO_N 12
bool bqIso = false;                                // BRIQUE "iso" : vue isometrique (plus bas, apres briqueParse)
struct BqTrou { int c, r; bool open; } bqT[16]; int nBqT = 0;
extern int bqNQ;                                   // questions de cours (declarees plus bas)
int bqMini = 0, bqMiniJeu = 0;              // bqMiniJeu = type du dernier jeu recu : 0 plateforme, 1 briques, 2 invaders
char bqMiniRows[7][11];                     // briques : 7 x 10 ('1'-'3' solidite, 'w' acier) ; invaders : 4 x 8 ('E' = ennemi)
float bqMiniVit = 1; int bqMiniVies = 0, bqMiniVagues = 1, bqMiniDiff = 2, bqMiniPerdues = 0; bool bqMiniBonus = true, bqMiniWin = false;
long bqMiniFond = -1; uint32_t bqMiniT0 = 0;
uint16_t bqBg() { return bqMini && bqMiniFond >= 0 ? RGB565(bqMiniFond >> 16, (bqMiniFond >> 8) & 255, bqMiniFond & 255) : COL_BG; }
void bqMiniReset() { bqMiniWin = false; bqMiniPerdues = 0; bqMiniT0 = millis(); }
void bqMiniGagne() {                        // jeu perso gagne : resultat a la page (meme ligne que les niveaux : temps, vies perdues, 0 etoile)
  bqMiniWin = true; snd(SND_WIN);
  if (Serial) Serial.printf("BRIQUE FIN %lu %d 0\n", (unsigned long)(millis() - bqMiniT0), bqMiniPerdues);
}
void bqMiniFin(int sTop) {                  // ecran de victoire par-dessus le jeu
  fb.setTextSize(3); fb.setTextColor(COL_GOAL); fb.setCursor(SCREEN_W / 2 - 63, 100 - sTop); fb.print("GAGNE !");
  fb.setTextSize(1); fb.setTextColor(COL_STAR); fb.setCursor(SCREEN_W / 2 - (int)strlen(bqMsg) * 3, 136 - sTop); fb.print(bqMsg);
  fb.setTextColor(COL_TXT); fb.setCursor(SCREEN_W / 2 - 69, 160 - sTop); fb.print("P3 = retour a l'accueil");
}

// ================= CASSE-BRIQUE (solo, raquette en bas) =================
// GAUCHE/DROITE = raquette. P3 = lancer la balle / relacher la balle collante.
// Maintenir P3 1,5 s = quitter. Briques a 1-3 coups ; certaines lachent une
// capsule a attraper avec la raquette :
//   M multi-balle (+2)   L raquette longue   S raquette courte (piege)
//   C balle collante (P3 pour la relacher quand on veut)
//   P balle percante (traverse les briques)   R ralenti   F acceleration (piege)
//   + vie supplementaire
#define BK_COLS 10
#define BK_ROWS 7
#define BK_W 30
#define BK_H 10
#define BK_X0 1
#define BK_Y0 26
#define BK_PY (SCREEN_H - 14)      // hauteur de la raquette
#define BK_PH 6
#define BK_MAXB 8
#define BK_MAXC 6
#define BK_R 3
enum { PU_MULTI, PU_LONG, PU_SHORT, PU_STICKY, PU_PIERCE, PU_SLOW, PU_FAST, PU_LIFE, PU_N };
const char PU_CH[PU_N] = { 'M', 'L', 'S', 'C', 'P', 'R', 'F', '+' };
uint8_t bkHp[BK_ROWS][BK_COLS];
struct BkBall { bool on, stuck; float x, y, vx, vy, off; };
struct BkCap  { bool on; float x, y; uint8_t t; };
BkBall bkB[BK_MAXB];
BkCap  bkC[BK_MAXC];
float bkPX, bkPW, bkWTarget;           // raquette : x, largeur, largeur visee
float bkSizeT, bkStickyT, bkPierceT, bkSpdT, bkSpdMul;
int bkLives, bkLevel, bkScore, bkBricks;
uint32_t bkBest;
float bkHoldT, bkMsgT; char bkMsg[20];
bool bkOver;

float bkSpeed() { float s = 175.0f + bkLevel * 10.0f; if (s > 280) s = 280; return s * bkSpdMul * (bqMini ? bqMiniVit : 1); }   // retour "la balle doit aller moins vite" : l'affichage est passe de ~20 a 50 images/s, la physique tourne maintenant en temps reel (300/480 -> 175/280)   // retour "toujours trop lente" -> base/plafond remontes encore (220->300, 380->480)

// dispositions 5-9 : 'w' = brique d'acier, INCASSABLE (la balle rebondit, meme percante) -> il faut la contourner ; 'b' = brique normale
static const char *const BK_PAT[5][BK_ROWS] = {
  { "bbbbbbbbbb", "bwwwbbwwwb", "bw.bbbb.wb", "bw.bbbb.wb", "bwwb..bwwb", "b.bbbbbb.b", "..bbbbbb.." },   // forteresse
  { "bbbbbbbbbb", "wwwwwww..b", "bbbbbbbbbb", "b..wwwwwww", "bbbbbbbbbb", "wwwwwww..b", "bbbbbbbbbb" },   // barres en zigzag
  { "bbbbbbbbbb", "bbwbbbbwbb", "bbbbwwbbbb", "bwbbbbbbwb", "bbbbwwbbbb", "bbwbbbbwbb", "bbbbbbbbbb" },   // ilots
  { "bbwbbbbwbb", "bbwbbbbwbb", "bbwbbbbwbb", "bbwbbbbwbb", "bbwbbbbwbb", "bbwbbbbwbb", "bbbbbbbbbb" },   // deux piliers
  { "bbbbwwbbbb", "bbbbwwbbbb", "bbbbbbbbbb", "wwb....bww", "bbbbbbbbbb", "bbbbwwbbbb", "bbbbbbbbbb" },   // croix
};
void bkSeal() {                           // une brique normale que la balle ne peut jamais atteindre (entouree d'acier) est retiree
  bool ok[BK_ROWS][BK_COLS] = {}; int st[BK_ROWS * BK_COLS], n = 0;
  for (int c = 0; c < BK_COLS; c++) if (bkHp[BK_ROWS - 1][c] != 9) { ok[BK_ROWS - 1][c] = true; st[n++] = (BK_ROWS - 1) * BK_COLS + c; }
  while (n > 0) {
    int v = st[--n], r = v / BK_COLS, c = v % BK_COLS;
    const int dr[4] = { -1, 1, 0, 0 }, dc[4] = { 0, 0, -1, 1 };
    for (int k = 0; k < 4; k++) { int rr = r + dr[k], cc = c + dc[k]; if (rr >= 0 && rr < BK_ROWS && cc >= 0 && cc < BK_COLS && !ok[rr][cc] && bkHp[rr][cc] != 9) { ok[rr][cc] = true; st[n++] = rr * BK_COLS + cc; } }
  }
  for (int r = 0; r < BK_ROWS; r++) for (int c = 0; c < BK_COLS; c++) if (bkHp[r][c] && bkHp[r][c] != 9 && !ok[r][c]) { bkHp[r][c] = 0; bkBricks--; }
}
void bkLoadLevel() {
  bkBricks = 0;
  if (bqMini) {                             // BRIQUE : la disposition de l'enfant
    for (int r = 0; r < BK_ROWS; r++) for (int c = 0; c < BK_COLS; c++) {
      const char q = bqMiniRows[r][c];
      bkHp[r][c] = q == 'w' ? 9 : q >= '1' && q <= '3' ? q - '0' : 0;
      if (bkHp[r][c] && bkHp[r][c] != 9) bkBricks++;
    }
    bkSeal(); return;
  }
  static const uint8_t ORD[10] = { 0, 5, 1, 6, 2, 7, 3, 8, 4, 9 };   // alterne les anciennes dispositions et les nouvelles (a l'acier)
  int pat = ORD[bkLevel % 10];
  int maxHp = 1 + min(2, bkLevel / 2);
  for (int r = 0; r < BK_ROWS; r++)
    for (int c = 0; c < BK_COLS; c++) {
      bool on;
      if (pat >= 5) {
        char q = BK_PAT[pat - 5][r][c];
        if (q == 'w') { bkHp[r][c] = 9; continue; }
        on = q == 'b';
      } else
      switch (pat) {
        case 0:  on = r < 5; break;                                             // rangees pleines
        case 1:  on = abs(c * 2 - (BK_COLS - 1)) <= (r + 1) * 2 - 1; break;   // pyramide
        case 2:  on = ((r + c) & 1) == 0; break;                               // damier
        case 3:  on = (r == 0 || r == BK_ROWS - 1 || c == 0 || c == BK_COLS - 1 || (r == 3 && c > 2 && c < 7)); break;   // cadre
        default: on = random(0, 100) < 70; break;                              // aleatoire
      }
      uint8_t hp = 0;
      if (on) { hp = 1 + (r < 2 ? maxHp - 1 : random(0, maxHp)); if (hp > 3) hp = 3; bkBricks++; }
      bkHp[r][c] = hp;
    }
  if (pat >= 5) bkSeal();
}
void bkServe() {                          // une balle collee au milieu de la raquette
  for (int i = 0; i < BK_MAXB; i++) bkB[i].on = false;
  BkBall &b = bkB[0];
  b.on = true; b.stuck = true; b.off = bkPW / 2; b.x = bkPX + b.off; b.y = BK_PY - BK_R - 1; b.vx = b.vy = 0;
}
void bkMessage(const char* m) { strncpy(bkMsg, m, sizeof(bkMsg) - 1); bkMsg[sizeof(bkMsg) - 1] = 0; bkMsgT = 1.2f; }
void startBreakout() {
  bkLevel = 0; bkScore = 0; bkLives = bqMini && bqMiniVies ? bqMiniVies : 3; bkOver = false; bqMiniReset();
  bkPW = bkWTarget = 48; bkPX = SCREEN_W / 2 - bkPW / 2;
  bkSizeT = bkStickyT = bkPierceT = bkSpdT = 0; bkSpdMul = 1; bkHoldT = 0; bkMsgT = 0;
  for (int i = 0; i < BK_MAXC; i++) bkC[i].on = false;
  bkBest = prefs.getUInt("bkbest", 0);
  bkLoadLevel(); bkServe();
  bkMessage(bqMini && bqTitre[0] ? bqTitre : "NIVEAU 1");
  state = ST_BRICK;
}
void bkLaunch(int bi) {                   // part vers le haut, angle selon la position sur la raquette
  BkBall &b = bkB[bi];
  float rel = (b.x - (bkPX + bkPW / 2)) / (bkPW / 2); if (rel < -1) rel = -1; if (rel > 1) rel = 1;
  float a = rel * 1.0f;                   // +-57 deg max
  if (fabs(a) < 0.15f) a = (a < 0 ? -0.15f : 0.15f);
  b.vx = bkSpeed() * sin(a); b.vy = -bkSpeed() * cos(a); b.stuck = false;
}
void bkSetSpeed(float mul) {              // change la vitesse de toutes les balles en vol
  for (int i = 0; i < BK_MAXB; i++) {
    BkBall &b = bkB[i]; if (!b.on || b.stuck) continue;
    float v = sqrt(b.vx * b.vx + b.vy * b.vy); if (v < 1) continue;
    float nv = bkSpeed() * mul / bkSpdMul; b.vx *= nv / v; b.vy *= nv / v;
  }
  bkSpdMul = mul;
}
void bkApply(uint8_t t) {
  switch (t) {
    case PU_MULTI: {
      int n = 0;
      for (int i = 0; i < BK_MAXB && n < 1; i++) if (bkB[i].on) {
        BkBall src = bkB[i]; if (src.stuck) { bkLaunch(i); src = bkB[i]; }
        for (int k = 0; k < 2; k++) for (int j = 0; j < BK_MAXB; j++) if (!bkB[j].on) {
          bkB[j] = src; float a = (k ? 0.5f : -0.5f);
          bkB[j].vx = src.vx * cos(a) - src.vy * sin(a); bkB[j].vy = src.vx * sin(a) + src.vy * cos(a);
          if (bkB[j].vy > -40) bkB[j].vy = -fabs(bkB[j].vy) - 40;
          break;
        }
        n++;
      }
      bkMessage("MULTI-BALLE !"); break;
    }
    case PU_LONG:   bkWTarget = 84; bkSizeT = 15; bkMessage("RAQUETTE XL"); break;
    case PU_SHORT:  bkWTarget = 26; bkSizeT = 12; bkMessage("RAQUETTE MINI"); break;
    case PU_STICKY: bkStickyT = 15; bkMessage("COLLANTE : P3"); break;
    case PU_PIERCE: bkPierceT = 8; bkMessage("BALLE PERCANTE"); break;
    case PU_SLOW:   bkSetSpeed(0.7f); bkSpdT = 10; bkMessage("RALENTI"); break;
    case PU_FAST:   bkSetSpeed(1.35f); bkSpdT = 8; bkMessage("TURBO !"); break;
    case PU_LIFE:   bkLives++; bkMessage("+1 VIE"); break;
  }
}
// touche la brique contenant (x,y) ; renvoie true si une brique a ete touchee
int bkHit(float x, float y) {      // 0 rien, 1 brique (cassee / abimee), 2 acier : rebond meme en balle percante
  if (y < BK_Y0 || x < BK_X0) return 0;
  int c = (int)(x - BK_X0) / (BK_W + 2), r = (int)(y - BK_Y0) / (BK_H + 2);
  if (c < 0 || c >= BK_COLS || r < 0 || r >= BK_ROWS || bkHp[r][c] == 0) return 0;
  if (bkHp[r][c] == 9) return 2;
  bkHp[r][c] = (bkPierceT > 0) ? 0 : bkHp[r][c] - 1;
  bkScore += 10;
  if (bkHp[r][c] == 0) {
    bkBricks--; bkScore += 10;
    if ((!bqMini || bqMiniBonus) && random(0, 100) < 22) for (int i = 0; i < BK_MAXC; i++) if (!bkC[i].on) {
      uint8_t w = random(0, 100), t;            // tirage pondere : bonus plus frequents que les pieges
      t = w < 18 ? PU_MULTI : w < 34 ? PU_LONG : w < 46 ? PU_STICKY : w < 58 ? PU_PIERCE : w < 70 ? PU_SLOW :
          w < 82 ? PU_SHORT : w < 94 ? PU_FAST : PU_LIFE;
      bkC[i] = { true, (float)(BK_X0 + c * (BK_W + 2) + BK_W / 2), (float)(BK_Y0 + r * (BK_H + 2)), t };
      break;
    }
  }
  return 1;
}
uint16_t bkBrickCol(int hp, int r) {
  if (hp >= 3) return tft.color565(160, 160, 175);
  if (hp == 2) return tft.color565(90, 200, 255);
  const uint8_t rc[BK_ROWS][3] = { {255,90,90}, {255,160,60}, {255,220,70}, {120,220,90}, {80,200,200}, {200,120,255}, {255,120,200} };
  return tft.color565(rc[r][0], rc[r][1], rc[r][2]);
}
uint16_t bkCapCol(uint8_t t) {
  switch (t) {
    case PU_SHORT: case PU_FAST: return tft.color565(230, 60, 60);   // pieges en rouge
    case PU_LIFE: return tft.color565(255, 120, 200);
    case PU_STICKY: return tft.color565(120, 230, 90);
    case PU_PIERCE: return tft.color565(255, 160, 40);
    default: return tft.color565(90, 170, 255);
  }
}
void drawBreakout() {
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {
    fb.fillRect(0, 0, SCREEN_W, STRIP_H, bqBg());
    for (int r = 0; r < BK_ROWS; r++) {
      int y = BK_Y0 + r * (BK_H + 2) - sTop; if (y < -BK_H || y > STRIP_H) continue;
      for (int c = 0; c < BK_COLS; c++) if (bkHp[r][c]) {
        int x = BK_X0 + c * (BK_W + 2);
        if (bkHp[r][c] == 9) {                                    // acier : sombre, hachures claires, rivets
          fb.fillRect(x, y, BK_W, BK_H, tft.color565(78, 84, 104)); fb.drawRect(x, y, BK_W, BK_H, tft.color565(150, 158, 182));
          for (int k = 4; k < BK_W - 4; k += 6) fb.drawLine(x + k, y + BK_H - 2, x + k + 5, y + 1, tft.color565(118, 126, 150));
          fb.drawPixel(x + 2, y + 2, COL_TXT); fb.drawPixel(x + BK_W - 3, y + 2, COL_TXT); fb.drawPixel(x + 2, y + BK_H - 3, COL_TXT); fb.drawPixel(x + BK_W - 3, y + BK_H - 3, COL_TXT);
          continue;
        }
        fb.fillRect(x, y, BK_W, BK_H, bkBrickCol(bkHp[r][c], r));
        if (bkHp[r][c] >= 2) fb.drawRect(x + 2, y + 2, BK_W - 4, BK_H - 4, COL_BG);
      }
    }
    for (int i = 0; i < BK_MAXC; i++) if (bkC[i].on) {
      int x = (int)bkC[i].x - 8, y = (int)bkC[i].y - sTop;
      fb.fillRoundRect(x, y, 16, 9, 3, bkCapCol(bkC[i].t));
      fb.setTextColor(COL_BG); fb.setTextSize(1); fb.setCursor(x + 5, y + 1); fb.print(PU_CH[bkC[i].t]);
    }
    uint16_t pc = bkStickyT > 0 ? tft.color565(120, 230, 90) : COL_MEAT;
    fb.fillRoundRect((int)bkPX, BK_PY - sTop, (int)bkPW, BK_PH, 3, pc);
    if (bkStickyT > 0) fb.drawFastHLine((int)bkPX + 2, BK_PY - sTop, (int)bkPW - 4, COL_TXT);
    for (int i = 0; i < BK_MAXB; i++) if (bkB[i].on)
      fb.fillCircle((int)bkB[i].x, (int)bkB[i].y - sTop, BK_R, bkPierceT > 0 ? tft.color565(255, 160, 40) : COL_TXT);
    fb.setTextSize(1); fb.setTextColor(COL_TXT);
    fb.setCursor(4, 6 - sTop); fb.print("SCORE "); fb.print(bkScore);
    fb.setCursor(120, 6 - sTop); fb.print("NIV "); fb.print(bkLevel + 1);
    fb.setCursor(180, 6 - sTop); fb.print("REC "); fb.print(bkBest);
    for (int i = 0; i < bkLives && i < 6; i++) fb.fillCircle(SCREEN_W - 10 - i * 10, 9 - sTop, 3, COL_MEAT);
    if (bqMiniWin) bqMiniFin(sTop);
    else if (bkMsgT > 0 || bkOver) {
      const char* m = bkOver ? "PERDU !" : bkMsg;
      fb.setTextSize(2); fb.setTextColor(COL_STAR);
      fb.setCursor(SCREEN_W / 2 - (int)strlen(m) * 6, 150 - sTop); fb.print(m);
      if (bkOver) { fb.setTextSize(1); fb.setTextColor(COL_TXT); fb.setCursor(96, 176 - sTop); fb.print("P3 = retour a l'accueil"); }
    }
    bool anyStuck = false; for (int i = 0; i < BK_MAXB; i++) if (bkB[i].on && bkB[i].stuck) anyStuck = true;
    if (anyStuck && !bkOver && !bqMiniWin) { fb.setTextSize(1); fb.setTextColor(tft.color565(150, 160, 180)); fb.setCursor(70, 200 - sTop); fb.print("P3 = lancer   (maintenir = quitter)"); }
    dBlitPlain(sTop);
  }
}
// renvoie false quand on quitte vers l'accueil
bool breakoutStep(bool bLeft, bool br, bool bj, bool eJ) {
  static unsigned long lastMs = 0;
  unsigned long now = millis();
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt <= 0 || dt > 0.045f) dt = 0.02f;

  if (bkOver || bqMiniWin) { if (eJ) return false; drawBreakout(); return true; }
  if (bj) { bkHoldT += dt; if (bkHoldT > 1.5f) return false; } else bkHoldT = 0;

  // raquette (taille qui glisse vers la cible)
  float pspd = 420.0f * dt, oldW = bkPW;   // retour "trop lente" -> 280 -> 420
  bkPW += (bkWTarget - bkPW) * min(1.0f, dt * 8);
  bkPX -= (bkPW - oldW) / 2;
  if (bLeft) bkPX -= pspd;
  if (br)    bkPX += pspd;
  if (bkPX < 0) bkPX = 0;
  if (bkPX > SCREEN_W - bkPW) bkPX = SCREEN_W - bkPW;

  // minuteries des bonus
  if (bkSizeT > 0)   { bkSizeT -= dt; if (bkSizeT <= 0) bkWTarget = 48; }
  if (bkStickyT > 0) { bkStickyT -= dt; if (bkStickyT <= 0) for (int i = 0; i < BK_MAXB; i++) if (bkB[i].on && bkB[i].stuck) bkLaunch(i); }
  if (bkPierceT > 0) bkPierceT -= dt;
  if (bkSpdT > 0)    { bkSpdT -= dt; if (bkSpdT <= 0) bkSetSpeed(1); }
  if (bkMsgT > 0)    bkMsgT -= dt;

  if (eJ) for (int i = 0; i < BK_MAXB; i++) if (bkB[i].on && bkB[i].stuck) bkLaunch(i);   // P3 : lance / relache

  int alive = 0;
  for (int i = 0; i < BK_MAXB; i++) {
    BkBall &b = bkB[i]; if (!b.on) continue;
    if (b.stuck) {
      if (b.off > bkPW - 2) b.off = bkPW - 2;
      b.x = bkPX + b.off; b.y = BK_PY - BK_R - 1; alive++; continue;
    }
    int steps = 1 + (int)(max(fabs(b.vx), fabs(b.vy)) * dt / 4);   // sous-pas : pas de traversee de brique
    float sdt = dt / steps;
    for (int s = 0; s < steps && b.on; s++) {
      b.x += b.vx * sdt;
      if (b.x < BK_R) { b.x = BK_R; b.vx = fabs(b.vx); }
      if (b.x > SCREEN_W - BK_R) { b.x = SCREEN_W - BK_R; b.vx = -fabs(b.vx); }
      int h1 = bkHit(b.x + (b.vx > 0 ? BK_R : -BK_R), b.y); if (h1 && (bkPierceT <= 0 || h1 == 2)) { b.x -= b.vx * sdt; b.vx = -b.vx; }
      b.y += b.vy * sdt;
      if (b.y < 18 + BK_R) { b.y = 18 + BK_R; b.vy = fabs(b.vy); }
      int h2 = bkHit(b.x, b.y + (b.vy > 0 ? BK_R : -BK_R)); if (h2 && (bkPierceT <= 0 || h2 == 2)) { b.y -= b.vy * sdt; b.vy = -b.vy; }
      // raquette
      if (b.vy > 0 && b.y + BK_R >= BK_PY && b.y - BK_R <= BK_PY + BK_PH && b.x >= bkPX - BK_R && b.x <= bkPX + bkPW + BK_R) {
        if (bkStickyT > 0) { b.stuck = true; b.off = b.x - bkPX; if (b.off < 2) b.off = 2; b.vx = b.vy = 0; b.y = BK_PY - BK_R - 1; }
        else { b.y = BK_PY - BK_R - 1; bkLaunch(i); }
      }
      if (b.y > SCREEN_H + 6) b.on = false;
    }
    if (b.on) alive++;
  }

  // capsules
  for (int i = 0; i < BK_MAXC; i++) {
    BkCap &c = bkC[i]; if (!c.on) continue;
    c.y += 75 * dt;
    if (c.y + 9 >= BK_PY && c.y <= BK_PY + BK_PH && c.x + 8 >= bkPX && c.x - 8 <= bkPX + bkPW) { c.on = false; bkApply(c.t); bkScore += 25; }
    else if (c.y > SCREEN_H) c.on = false;
  }

  if (alive == 0) {                         // plus de balle : une vie de moins
    bkLives--; bqMiniPerdues++;
    bkWTarget = 48; bkSizeT = bkStickyT = bkPierceT = 0; if (bkSpdT > 0) { bkSpdT = 0; bkSpdMul = 1; }
    for (int i = 0; i < BK_MAXC; i++) bkC[i].on = false;
    if (bkLives <= 0) {
      bkOver = true;
      if (!bqMini && (uint32_t)bkScore > bkBest) { bkBest = bkScore; prefs.putUInt("bkbest", bkBest); }
    } else { bkServe(); bkMessage("BALLE PERDUE"); }
  }
  if (bkBricks <= 0) {                      // niveau fini
    if (bqMini) { bqMiniGagne(); drawBreakout(); return true; }   // BRIQUE : un seul niveau, celui de l'enfant
    if (checkpointActive) { checkpointWin(); return true; }   // palier bonus : gagner = on passe tout de suite, pas besoin de perdre
    bkLevel++; bkScore += 100;
    for (int i = 0; i < BK_MAXC; i++) bkC[i].on = false;
    bkLoadLevel(); bkServe();
    char m[20]; snprintf(m, sizeof(m), "NIVEAU %d", bkLevel + 1); bkMessage(m);
  }
  drawBreakout();
  return true;
}

// ================= INVADERS (mini-jeu, choisi depuis l'accueil, solo) =================
// Vaisseau en bas (GAUCHE/DROITE), P3 = tirer. Grille d'envahisseurs qui se
// deplace en bloc (decalage de groupe invGX/invGY, pas de position par
// envahisseur) -- plus il en reste peu, plus le groupe va vite, comme dans
// l'original. Descend d'un cran et change de sens a chaque bord touche.
#define INV_COLS 8
#define INV_ROWS 4
#define INV_AW 20
#define INV_AH 12
#define INV_GAPX 10
#define INV_GAPY 10
#define INV_MAXPB 4   // retour "tirer plus souvent" -> plus de balles joueur en vol a la fois
#define INV_MAXEB 3
bool  invAlive[INV_ROWS][INV_COLS];
float invGX, invGY, invDirX, invSpeed;
float invShipX;
struct InvBullet { bool on; float x, y; };
InvBullet invPB[INV_MAXPB], invEB[INV_MAXEB];
int invScore, invLives, invWave;
uint32_t invBest;
bool invOver;
float invFireT, invEFireT;
int invN0;                                  // envahisseurs au debut de la vague (le groupe accelere a mesure qu'il se vide)

int invOX() { return (SCREEN_W - (INV_COLS * (INV_AW + INV_GAPX) - INV_GAPX)) / 2; }
int invAlienX(int c) { return invOX() + c * (INV_AW + INV_GAPX) + (int)invGX; }
int invAlienY(int r) { return 24 + r * (INV_AH + INV_GAPY) + (int)invGY; }
int invAliveCount() { int n = 0; for (int r = 0; r < INV_ROWS; r++) for (int c = 0; c < INV_COLS; c++) if (invAlive[r][c]) n++; return n; }

void invLoadWave() {
  for (int r = 0; r < INV_ROWS; r++) for (int c = 0; c < INV_COLS; c++) invAlive[r][c] = !bqMini || bqMiniRows[r][c] == 'E';   // BRIQUE : la formation de l'enfant
  invN0 = invAliveCount();
  invGX = 0; invGY = 0; invDirX = 1;
  invSpeed = (28.0f + invWave * 5.0f) * (bqMini ? bqMiniVit : 1);
  for (int i = 0; i < INV_MAXEB; i++) invEB[i].on = false;
}
void startInvaders() {
  invScore = 0; invLives = bqMini && bqMiniVies ? bqMiniVies : 3; invWave = 0; invOver = false; bqMiniReset();
  invShipX = SCREEN_W / 2 - 8;
  for (int i = 0; i < INV_MAXPB; i++) invPB[i].on = false;
  invFireT = 0; invEFireT = 0.6f;
  invBest = prefs.getUInt("invbest", 0);
  invLoadWave();
  state = ST_INVADERS;
}
// 3 silhouettes differentes, plus proches des envahisseurs originaux (retour
// "ennemis plus diversifies") : 0 = calmar (ligne du haut, etroit + antennes),
// 1 = crabe (lignes du milieu, pinces laterales), 2 = pieuvre (derniere
// ligne, base large a plusieurs jambes).
void drawInvaderSprite(int x, int y, int type, uint16_t col) {
  uint16_t eye = tft.color565(18, 16, 26);
  if (type == 0) {
    fb.fillRect(x + 7, y, 2, 3, col); fb.fillRect(x + 11, y, 2, 3, col);              // antennes
    fb.fillRoundRect(x + 4, y + 2, 12, 6, 2, col);                                     // corps etroit
    fb.fillRect(x + 2, y + 8, 3, 4, col); fb.fillRect(x + 15, y + 8, 3, 4, col);        // jambes ecartees
    fb.fillRect(x + 7, y + 8, 2, 4, col); fb.fillRect(x + 11, y + 8, 2, 4, col);
  } else if (type == 1) {
    fb.fillRoundRect(x + 3, y + 1, 14, 7, 2, col);                                     // corps + pinces
    fb.fillRect(x, y + 4, 4, 3, col); fb.fillRect(x + 16, y + 4, 4, 3, col);
    fb.fillRect(x + 2, y + 9, 3, 3, col); fb.fillRect(x + 15, y + 9, 3, 3, col);
    fb.fillRect(x + 8, y + 9, 4, 3, col);
  } else {
    fb.fillRoundRect(x + 2, y, 16, 7, 3, col);                                         // base large
    for (int k = 0; k < 4; k++) fb.fillRect(x + 2 + k * 5, y + 7, 3, 5, col);           // plusieurs jambes
  }
  fb.fillRect(x + 6, y + 3, 2, 2, eye); fb.fillRect(x + 12, y + 3, 2, 2, eye);          // yeux
}

void drawInvaders() {
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {
    fb.fillRect(0, 0, SCREEN_W, STRIP_H, bqBg());
    for (int r = 0; r < INV_ROWS; r++) {
      int y = invAlienY(r) - sTop; if (y < -INV_AH || y > STRIP_H) continue;
      int type = (r == 0) ? 0 : (r < INV_ROWS - 1 ? 1 : 2);
      uint16_t col = (type == 0) ? tft.color565(90, 220, 255) : (type == 1 ? tft.color565(255, 110, 200) : COL_GOAL);
      for (int c = 0; c < INV_COLS; c++) {
        if (!invAlive[r][c]) continue;
        if (bqIcon(3)) drawIcon(3, invAlienX(c) + 2, y - 2, false);   // BRIQUE : icone ennemi choisie sur la page
        else drawInvaderSprite(invAlienX(c), y, type, col);
      }
    }
    for (int i = 0; i < INV_MAXPB; i++) if (invPB[i].on) fb.fillRect((int)invPB[i].x - 1, (int)invPB[i].y - sTop - 5, 2, 7, COL_TXT);
    for (int i = 0; i < INV_MAXEB; i++) if (invEB[i].on) fb.fillRect((int)invEB[i].x - 1, (int)invEB[i].y - sTop - 5, 2, 7, COL_MEAT);
    if (bqIcon(0)) drawIcon(0, (int)invShipX, SCREEN_H - 26 - sTop, false);   // BRIQUE : icone heros = le vaisseau
    else { fb.fillRoundRect((int)invShipX, SCREEN_H - 20 - sTop, 16, 8, 2, COL_GOAL); fb.fillRect((int)invShipX + 6, SCREEN_H - 24 - sTop, 4, 4, COL_GOAL); }
    fb.setTextSize(1); fb.setTextColor(COL_TXT);
    fb.setCursor(4, 6 - sTop); fb.print("SCORE "); fb.print(invScore);
    fb.setCursor(120, 6 - sTop); fb.print("VAGUE "); fb.print(invWave + 1);
    fb.setCursor(220, 6 - sTop); fb.print("REC "); fb.print(invBest);
    for (int i = 0; i < invLives && i < 6; i++) fb.fillTriangle(SCREEN_W - 14 - i * 14, 14 - sTop, SCREEN_W - 20 - i * 14, 20 - sTop, SCREEN_W - 8 - i * 14, 20 - sTop, COL_GOAL);
    if (bqMiniWin) bqMiniFin(sTop);
    else if (invOver) {
      fb.setTextSize(3); fb.setTextColor(COL_MEAT);
      fb.setCursor(70, 100 - sTop); fb.print("PERDU !");
      fb.setTextSize(1); fb.setTextColor(COL_TXT);
      fb.setCursor(70, 140 - sTop); fb.print("P3 = retour a l'accueil");
    } else {
      fb.setTextSize(1); fb.setTextColor(tft.color565(150, 160, 180));
      fb.setCursor(60, 222 - sTop); fb.print("P3 = tirer   G+D maintenus = quitter");
    }
    dBlitPlain(sTop);
  }
}
bool invadersStep(bool bLeft, bool br, bool bj, bool eJ) {
  static unsigned long lastMs = 0;
  unsigned long now = millis();
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt <= 0 || dt > 0.045f) dt = 0.02f;

  if (invOver || bqMiniWin) { if (eJ) return false; drawInvaders(); return true; }
  static float invHoldT = 0;
  if (bLeft && br) { invHoldT += dt; if (invHoldT > 1.0f) { invHoldT = 0; return false; } } else invHoldT = 0;   // G + D 1 s = quitter (P3 maintenu = tir continu, ne doit plus quitter)

  float sspd = 220.0f * dt;
  if (bLeft) invShipX -= sspd;
  if (br)    invShipX += sspd;
  if (invShipX < 0) invShipX = 0;
  if (invShipX > SCREEN_W - 16) invShipX = SCREEN_W - 16;

  if (invFireT > 0) invFireT -= dt;
  if (bj && invFireT <= 0) {
    for (int i = 0; i < INV_MAXPB; i++) if (!invPB[i].on) { invPB[i] = { true, invShipX + 8, SCREEN_H - 24 }; invFireT = 0.15f; break; }   // retour "tirer plus souvent" -> 0.35 -> 0.15
  }
  for (int i = 0; i < INV_MAXPB; i++) if (invPB[i].on) { invPB[i].y -= 420 * dt; if (invPB[i].y < 0) invPB[i].on = false; }   // retour "tirs plus rapides" -> 260 -> 420
  for (int i = 0; i < INV_MAXEB; i++) if (invEB[i].on) { invEB[i].y += 150 * dt; if (invEB[i].y > SCREEN_H) invEB[i].on = false; }

  int alive = invAliveCount();
  if (alive == 0) {                           // vague terminee -> la suivante, un peu plus dure
    if (bqMini && invWave + 1 >= bqMiniVagues) { bqMiniGagne(); drawInvaders(); return true; }   // BRIQUE : toutes les vagues demandees
    if (checkpointActive) { checkpointWin(); return true; }   // palier bonus : vague gagnee = on passe tout de suite
    invWave++; invScore += 50;
    invLoadWave();
  } else {
    float mul = 1.0f + (invN0 - alive) * 0.05f;
    invGX += invDirX * invSpeed * mul * dt;
    int lc = -1, rc = -1;                     // colonnes vivantes la plus a gauche/droite
    for (int c = 0; c < INV_COLS; c++) for (int r = 0; r < INV_ROWS; r++) if (invAlive[r][c]) { if (lc < 0) lc = c; rc = c; break; }
    if (lc >= 0) {
      int xL = invAlienX(lc), xR = invAlienX(rc) + INV_AW;
      if ((xL <= 2 && invDirX < 0) || (xR >= SCREEN_W - 2 && invDirX > 0)) { invDirX = -invDirX; invGY += 10; }
    }
    invEFireT -= dt;
    if (invEFireT <= 0) {
      invEFireT = max(0.35f, 1.3f - invWave * 0.08f) * (!bqMini || bqMiniDiff == 2 ? 1.0f : bqMiniDiff == 1 ? 1.5f : 0.65f);   // BRIQUE : facile = tirs plus rares
      int tries = 0, c = random(0, INV_COLS);
      while (tries++ < INV_COLS) {
        int bottomR = -1;
        for (int r = INV_ROWS - 1; r >= 0; r--) if (invAlive[r][c]) { bottomR = r; break; }
        if (bottomR >= 0) {
          for (int i = 0; i < INV_MAXEB; i++) if (!invEB[i].on) { invEB[i] = { true, (float)(invAlienX(c) + INV_AW / 2), (float)(invAlienY(bottomR) + INV_AH) }; break; }
          break;
        }
        c = (c + 1) % INV_COLS;
      }
    }
    for (int r = INV_ROWS - 1; r >= 0; r--) for (int c = 0; c < INV_COLS; c++) if (invAlive[r][c] && invAlienY(r) + INV_AH >= SCREEN_H - 24) {
      invOver = true;                          // un envahisseur a atteint le vaisseau
      if (!bqMini && (uint32_t)invScore > invBest) { invBest = invScore; prefs.putUInt("invbest", invBest); }
    }
    // tirs joueur vs envahisseurs
    for (int i = 0; i < INV_MAXPB; i++) {
      if (!invPB[i].on) continue;
      for (int r = 0; r < INV_ROWS && invPB[i].on; r++) for (int c = 0; c < INV_COLS && invPB[i].on; c++) {
        if (!invAlive[r][c]) continue;
        int ax = invAlienX(c), ay = invAlienY(r);
        if (invPB[i].x >= ax && invPB[i].x <= ax + INV_AW && invPB[i].y >= ay && invPB[i].y <= ay + INV_AH) {
          invAlive[r][c] = false; invPB[i].on = false; invScore += 10;
        }
      }
    }
  }
  // tirs ennemis vs vaisseau
  for (int i = 0; i < INV_MAXEB; i++) {
    if (!invEB[i].on) continue;
    if (invEB[i].x >= invShipX - 2 && invEB[i].x <= invShipX + 18 && invEB[i].y >= SCREEN_H - 22 && invEB[i].y <= SCREEN_H - 10) {
      invEB[i].on = false; invLives--; bqMiniPerdues++;
      if (invLives <= 0) {
        invOver = true;
        if (!bqMini && (uint32_t)invScore > invBest) { invBest = invScore; prefs.putUInt("invbest", invBest); }
      }
    }
  }
  drawInvaders();
  return true;
}

// ================= QUIZ (entre deux niveaux) =================
// QUIZZES[]/Quiz/N_QUIZ viennent de quiz_data.h, genere par gen_quiz.py a partir
// de questions.txt (modifiable a la main) -- voir compiler-upload-c3-tft-meatboy.bat.
#include "quiz_data.h"
int quizChoice, quizPick, quizRetryLevel, quizTargetLevel, quizRot;
int quizLastPick = -1;
int quizPendingRetry = -1;  // question ratee -> reposee en priorite au prochain quiz (-1 = aucune), reponses re-melangees comme une relance normale (cf quizLastRot)
int8_t quizLastRot[N_QUIZ]; // derniere rotation des reponses utilisee pour chaque question (-1 = jamais posee)
bool quizOk;                          // reponse a la derniere question : juste ou faux
#define NCAT 4                        // matieres : 0 maths, 1 francais, 2 anglais, 3 chimie
int quizAskedByCat[NCAT], quizCorrectByCat[NCAT];   // stats par matiere
int quizDest;                         // apres la question : 0 = niveau (continuer/refaire), 1 = menu Niveaux, 2 = Accueil
int sessionWins;                      // niveaux reussis cette session (alimente la trainee)
int quizStreak;                       // bonnes reponses d'affilee (remis a 0 des qu'on se trompe) -- affiche sur l'ecran quiz uniquement
// quiz adaptatif : niveau de difficulte PAR MATIERE (1..5) -- monte quand on repond juste, baisse sinon ;
// plus il est haut, plus on tire des questions neuves (non encore vues) plutot que des revisions. NVS "qsk"/"qsn".
float quizSkill[NCAT] = { 1, 1, 1, 1 };
uint8_t quizSeen[(N_QUIZ + 7) / 8];   // bit i = question i deja posee
uint8_t quizDone[(N_QUIZ + 7) / 8];   // bit i = question i reussie depuis l allumage (RAM) : elle ne revient plus pendant la session
bool doneQ(int i) { return quizDone[i >> 3] & (1 << (i & 7)); }
int quizLastCat = -1;
// points de SAVOIR : cumules par les bonnes reponses (10 x difficulte + bonus de serie), niveau d'evolution de l'avatar. NVS "kn".
uint32_t knowledge, quizShown;        // quizShown = valeur affichee (monte progressivement pendant l'animation)
int quizGain; bool quizLvUp;
const uint32_t KN_AT[5] = { 0, 60, 200, 450, 900 };
const char* KN_NAME[5] = { "Eleve", "Curieux", "Malin", "Savant", "Genie" };
int knowLvl(uint32_t k) { int l = 0; for (int i = 1; i < 5; i++) if (k >= KN_AT[i]) l = i; return l; }
bool seenQ(int i) { return quizSeen[i >> 3] & (1 << (i & 7)); }
void saveQuizProg() { prefs.putUInt("kn", knowledge); prefs.putBytes("qsk", quizSkill, sizeof(quizSkill)); prefs.putBytes("qsn", quizSeen, sizeof(quizSeen)); }
void loadQuizProg() {
  knowledge = prefs.getUInt("kn", 0);
  if (prefs.getBytes("qsk", quizSkill, sizeof(quizSkill)) != sizeof(quizSkill)) for (int i = 0; i < NCAT; i++) quizSkill[i] = 1;
  if (prefs.getBytes("qsn", quizSeen, sizeof(quizSeen)) != sizeof(quizSeen)) memset(quizSeen, 0, sizeof(quizSeen));   // banque modifiee -> taille differente -> remise a zero
}

// palier (0..NUM_PLAYER_TIERS-1) derive du TAUX DE REUSSITE global de la session
// (total bonnes reponses / total questions), pas de la seule serie en cours --
// evolue donc progressivement, sans retomber a 0 sur une seule erreur. Pilote
// a la fois la teinte du corps et la trainee de particules (voir drawGame()).
// "puissance" de la recompense : chaque bonne reponse +3, chaque erreur -1,
// chaque niveau reussi +1. Cumulative -> la trainee grandit au fil de la session.
int rewardPower() {
  int asked = 0, correct = 0;
  for (int i = 0; i < NCAT; i++) { asked += quizAskedByCat[i]; correct += quizCorrectByCat[i]; }
  int p = correct * 3 - (asked - correct) + sessionWins;
  return p < 0 ? 0 : p;
}
// progression de la trainee sauvegardee (NVS) : reponses au quiz par matiere + niveaux reussis
void saveReward() {
  prefs.putBytes("qa", quizAskedByCat, sizeof(quizAskedByCat));
  prefs.putBytes("qc", quizCorrectByCat, sizeof(quizCorrectByCat));
  prefs.putInt("swins", sessionWins);
}
void loadReward() {
  if (prefs.getBytes("qa", quizAskedByCat, sizeof(quizAskedByCat)) != sizeof(quizAskedByCat)) for (int i = 0; i < NCAT; i++) quizAskedByCat[i] = 0;
  if (prefs.getBytes("qc", quizCorrectByCat, sizeof(quizCorrectByCat)) != sizeof(quizCorrectByCat)) for (int i = 0; i < NCAT; i++) quizCorrectByCat[i] = 0;
  sessionWins = prefs.getInt("swins", 0);
  loadQuizProg();
}
const int TIER_AT[NUM_PLAYER_TIERS] = { 0, 3, 9, 18, 30 };   // puissance mini de chaque palier
const char* TIER_NAME[NUM_PLAYER_TIERS] = { "aucune", "Bronze", "Argent", "Or", "Diamant" };
int computePlayerTier() {
  int p = rewardPower(), t = 0;
  for (int i = 1; i < NUM_PLAYER_TIERS; i++) if (p >= TIER_AT[i]) t = i;
  return t;
}
int effTier() { int t = computePlayerTier(); return (t == 0 && trailSel > 0) ? 1 : t; }   // une trainee achetee marche meme sans palier

// fait vieillir/retire les particules de la trainee, et en fait naitre de
// nouvelles derriere le joueur (cote oppose au sens de deplacement) tant
// qu'il bouge et que le palier courant est > 0. Chaque particule s'efface
// en retrecissant avec l'age (voir le rendu dans drawGame()).
void updateTrail(float dt) {
  for (int i = 0; i < nTrail; i++) {
    trailAge[i] += dt;
    trailY[i] += trailVY[i] * dt;
    if (trailAge[i] >= trailLife[i]) {          // retrait par swap avec la derniere -- l'ordre n'a pas d'importance
      int last = nTrail - 1;
      trailX[i] = trailX[last]; trailY[i] = trailY[last]; trailVY[i] = trailVY[last]; trailK[i] = trailK[last];
      trailAge[i] = trailAge[last]; trailLife[i] = trailLife[last];
      nTrail--; i--;
    }
  }
  int tier = effTier();
  if (tier <= 0) return;
  int pw = rewardPower(); if (pw > 60) pw = 60;
  bool moving = !(onGround && fabs(vx) < 20);
  if (!moving && tier < 3) return;               // a l'arret : etincelles seulement aux paliers Or/Diamant
  trailSpawnT -= dt;
  if (trailSpawnT > 0) return;
  if (moving) trailSpawnT = max(0.025f, 0.09f - pw * 0.0015f);   // plus de points avec la puissance
  else        trailSpawnT = 0.18f;
  int n = moving ? 1 + tier / 2 : 1;             // 1..3 particules a chaque fois
  int dir = playerFacingRight ? 1 : -1;
  for (int k = 0; k < n && nTrail < MTRAIL; k++) {
    int i = nTrail++;
    int spread = 2 + tier;
    if (moving) {
      trailX[i] = px + (dir > 0 ? 0 : PW) + (float)random(-spread, spread + 1);
      trailY[i] = py + PH - 3 - (float)random(0, PH - 2) * (tier >= 2 ? 1.0f : 0.3f);
    } else {                                     // etincelles autour du corps
      trailX[i] = px + (float)random(-4, PW + 4);
      trailY[i] = py + (float)random(-4, PH);
    }
    trailVY[i] = (tier >= 3) ? -(float)random(10, 40) : 0;   // Or/Diamant : les points montent
    trailK[i] = random(0, 5);
    trailAge[i] = 0;
    trailLife[i] = 0.35f + tier * 0.12f + pw * 0.005f;
  }
}

// pioche "sac melange" : chaque question ne revient qu'une fois que TOUTES les
// autres sont passees -> repetitions aussi rares que possible avec 36 questions.
// tire une question de la matiere (au hasard, jamais 2x la meme d'affilee) dont la difficulte est proche du niveau
// de CETTE matiere ; probabilite de question neuve = 35% au niveau 1 -> 95% au niveau 5
int pickAdaptive() {
  int cat; do cat = random(0, NCAT); while (cat == quizLastCat && random(0, 4));
  quizLastCat = cat;
  float sk = quizSkill[cat], best = 99;
  bool sd = false; for (int i = 0; i < N_QUIZ; i++) if (QUIZZES[i].cat == cat && !doneQ(i)) sd = true;   // reste-t-il des questions pas encore reussies dans cette matiere ?
  for (int i = 0; i < N_QUIZ; i++) if (QUIZZES[i].cat == cat && (!sd || !doneQ(i))) best = min(best, fabsf(QUIZZES[i].diff - sk));
  float lim = max(1.0f, best);
  int win[N_QUIZ], nw = 0, nf = 0;
  for (int i = 0; i < N_QUIZ; i++) if (QUIZZES[i].cat == cat && i != quizLastPick && (!sd || !doneQ(i)) && fabsf(QUIZZES[i].diff - sk) <= lim) { win[nw++] = i; if (!seenQ(i)) nf++; }
  if (nw == 0) return random(0, N_QUIZ);
  if (nf == 0) { for (int j = 0; j < nw; j++) quizSeen[win[j] >> 3] &= ~(1 << (win[j] & 7)); nf = nw; }   // tout vu a ce niveau -> on recommence
  bool useNew = nf == nw || random(100) < (int)(100 * min(0.95f, 0.35f + 0.15f * (sk - 1)));
  int pool[N_QUIZ], np = 0;
  for (int j = 0; j < nw; j++) if (seenQ(win[j]) != useNew) pool[np++] = win[j];
  return pool[random(0, np)];
}

// tire une question et prepare le passage vers le niveau suivant (retryLevel = celui
// qu'il faudra refaire si la reponse est fausse, targetLevel = celui vise si juste)
void beginQuiz(int retryLevel, int targetLevel, int dest) {
  quizRetryLevel = retryLevel; quizTargetLevel = targetLevel; quizDest = dest;
  if (quizPendingRetry >= 0) {                  // question ratee la derniere fois -> reposee en priorite
    quizPick = quizPendingRetry;
    quizPendingRetry = -1;
  } else {
    quizPick = pickAdaptive();
  }
  quizLastPick = quizPick; quizSeen[quizPick >> 3] |= 1 << (quizPick & 7);
  int rot;
  do { rot = random(0, 3); } while (rot == quizLastRot[quizPick]);  // reponses interverties si redemandee
  quizLastRot[quizPick] = rot;
  quizRot = rot;
  quizChoice = 0; quizShown = knowledge;
  state = ST_QUIZ;
  drawQuiz();
}
// --- texte en police Chunky : n = taille "classique" (1..12) -> (police, echelle) ---
static FontSel fontFor(int n) {
  if (n <= 1) return { &Chunky9, 1, 9 };
  if (n == 2) return { &Chunky15, 1, 15 };
  if (n == 3) return { &Chunky22, 1, 22 };
  if (n <= 5) return { &Chunky30, 1, 30 };
  if (n <= 8) return { &Chunky44, 1, 44 };
  return { &Chunky44, 2, 44 };
}
// largeur en pixels d'un texte
static int textW(Adafruit_GFX &d, int n, const char* t) {
  FontSel fs = fontFor(n); int16_t x1, y1; uint16_t w, h;
  d.setFont(fs.f); d.setTextSize(fs.scale); d.getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
  d.setFont(NULL); d.setTextSize(n);
  return (int)w + x1 * 2;
}
// (x, y) = coin haut-gauche de la ligne (comme l'ancienne police) ; la ligne de base est centree dans la hauteur 8*n
static void gfxPrintAt(Adafruit_GFX &d, int n, int x, int y, const char* t) {
  FontSel fs = fontFor(n);
  d.setFont(fs.f); d.setTextSize(fs.scale);
  d.setCursor(x, y + (8 * n + fs.cap * fs.scale) / 2);
  d.print(t);
  d.setFont(NULL); d.setTextSize(n);
}
void ltextAt(int x, int y, const char* t) { gfxPrintAt(tft, tft.csz, x, y, t); }                          // aligne a gauche
void rtextAt(int x, int y, const char* t) { gfxPrintAt(tft, tft.csz, x - textW(tft, tft.csz, t), y, t); } // aligne a droite (x = bord droit)
void ctextAt(int cx, int y, const char* t) { gfxPrintAt(tft, tft.csz, cx - textW(tft, tft.csz, t) / 2, y, t); }
void ctext(int y, const char* t) { ctextAt(SCREEN_W / 2, y, t); }
void ctextf(int y, const char* fmt, ...) {
  char b[96]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a); ctext(y, b);
}
void ltextf(int x, int y, const char* fmt, ...) {
  char b[96]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a); ltextAt(x, y, b);
}

// fond de briques rouge sombre (tous les ecrans de menu sauf l'ecran titre) : quelques briques sales
// (noires) reparties de facon deterministe -> redessiner un morceau redonne exactement le meme motif
void brickFill(int x, int y, int w, int h) {
  const int BW = 32, BH = 12;
  const uint16_t mortar = RGB565(34, 8, 12), brickA = RGB565(96, 22, 28), brickB = RGB565(84, 18, 25);
  const uint16_t dirtyA = RGB565(14, 6, 8), dirtyB = RGB565(30, 14, 16);
  tft.fillRect(x, y, w, h, mortar);                              // les joints = fond
  for (int r = y / BH; r * BH < y + h; r++) {
    int off = (r & 1) * (BW / 2), by = r * BH + 1;
    for (int c = (x - off - BW) / BW - 1; ; c++) {
      int bx = c * BW + off + 1;
      if (bx >= x + w) break;
      if (bx + BW - 1 <= x) continue;
      uint32_t hs = (uint32_t)(r * 73856093) ^ (uint32_t)(c * 19349663); hs ^= hs >> 13; hs *= 0x5bd1e995u; hs ^= hs >> 15;
      int k = hs % 100;
      uint16_t col = (k < 9) ? dirtyA : (k < 16) ? dirtyB : (k & 1) ? brickA : brickB;
      int ix0 = max(bx, x), ix1 = min(bx + BW - 1, x + w), iy0 = max(by, y), iy1 = min(by + BH - 1, y + h);
      if (ix1 > ix0 && iy1 > iy0) tft.fillRect(ix0, iy0, ix1 - ix0, iy1 - iy0, col);
    }
  }
}

// ---- ecran quiz : discussion entre le professeur (gauche) et l'avatar du joueur (droite), meme ecran pour la
// question, la reponse, les points de savoir et la suite (niveau suivant / on refait) ----
void quizAvatar(int cx, int feet, bool prof, int mood) {   // mood : 0 neutre, 1 content, 2 triste
  const int w = 24, h = 28;
  int x0 = cx - w / 2, y0 = feet - h, lv = knowLvl(quizShown);
  uint16_t c, d;
  if (prof) { c = RGB565(70, 100, 190); d = RGB565(25, 40, 110); }
  else {
    int tier = computePlayerTier(); c = COL_PLAYER_TIER[tier]; d = COL_PLAYERD_TIER[tier];
    if (skinSel > 0) { c = skinColor(skinSel); d = SKIN_COLD[skinSel]; }
  }
  skinBody(tft, prof ? 0 : skinSel, x0, y0, w, h, 6, c, d);
  if (prof) { tft.fillRect(x0 - 1, y0 + 6, 3, 10, COL_TXT); tft.fillRect(x0 + w - 2, y0 + 6, 3, 10, COL_TXT); }   // cheveux blancs
  int ey = y0 + 9, my = y0 + 20;
  tft.fillRect(x0 + 6, ey, 3, 4, d); tft.fillRect(x0 + w - 9, ey, 3, 4, d);
  tft.drawFastHLine(x0 + 8, my, 8, d);
  if (mood == 1) { tft.drawPixel(x0 + 7, my - 1, d); tft.drawPixel(x0 + 16, my - 1, d); tft.drawFastHLine(x0 + 9, my + 1, 6, d); }
  else if (mood == 2) { tft.drawPixel(x0 + 7, my + 1, d); tft.drawPixel(x0 + 16, my + 1, d); }
  if (prof || lv >= 1) { tft.drawRect(x0 + 4, ey - 2, 7, 7, COL_TXT); tft.drawRect(x0 + w - 11, ey - 2, 7, 7, COL_TXT); tft.drawFastHLine(x0 + 11, ey + 1, w - 22, COL_TXT); }   // lunettes
  if (!prof && lv >= 2) { tft.fillRect(x0 - 6, y0 + 17, 9, 11, COL_STAR); tft.drawRect(x0 - 6, y0 + 17, 9, 11, d); tft.drawFastVLine(x0 - 3, y0 + 17, 11, d); }   // livre
  if (prof || lv >= 3) {                                                      // toque
    tft.fillRect(x0 - 3, y0 - 5, w + 6, 3, RGB565(15, 15, 25)); tft.fillRect(x0 + 4, y0 - 2, w - 8, 3, RGB565(15, 15, 25));
    tft.drawFastHLine(x0 - 3, y0 - 5, w + 6, COL_TXT); tft.drawFastVLine(x0 + w + 2, y0 - 4, 7, COL_STAR);
  }
  if (!prof && lv >= 4) tft.fillCircle(cx, y0 - 10, 3, COL_STAR);             // etoile du genie
}
// une ligne de texte centree dans une bulle ; retombe en petite police si elle depasse
void bubbleLine(int cx, int y, int maxw, const char* t, uint16_t col) {
  int n = textW(tft, 2, t) > maxw ? 1 : 2;
  tft.setTextSize(n); tft.setTextColor(col); ctextAt(cx, y + (n == 1 ? 4 : 0), t); tft.setTextSize(2);
}
void drawQuizScore() {                                  // bandeau du haut : niveau d'evolution, barre, points de savoir
  int lv = knowLvl(quizShown);
  tft.fillRect(0, 0, SCREEN_W, 24, RGB565(18, 6, 10));
  tft.setTextSize(1); tft.setTextColor(COL_STAR); ltextAt(6, 3, KN_NAME[lv]);
  int w = 120; tft.drawRect(6, 14, w + 2, 7, COL_TXT);
  if (lv < 4) { int f = (int)((quizShown - KN_AT[lv]) * w / (KN_AT[lv + 1] - KN_AT[lv])); tft.fillRect(7, 15, f, 5, COL_GOAL); }
  else tft.fillRect(7, 15, w, 5, COL_STAR);
  tft.setTextSize(2); tft.setTextColor(COL_STAR);
  char b[16]; snprintf(b, sizeof(b), "%u pts", (unsigned)quizShown); rtextAt(SCREEN_W - 6, 4, b);
  if (state == ST_QUIZRESULT && quizGain > 0) { tft.setTextColor(COL_GOAL); snprintf(b, sizeof(b), "+%d", quizGain); ctextAt(168, 4, b); }
}
int quizChoiceY;                                        // haut de la bulle des reponses (pour ne redessiner qu'elles)
void drawQuizChoices() {                                // les 3 reponses seulement : changer de reponse ne redessine plus tout l'ecran (scintillement)
  const Quiz &q = QUIZZES[quizPick];
  const int PWD = 272, SX = 6;
  for (int i = 0; i < 3; i++) {
    int oi = (i + quizRot) % 3, yy = quizChoiceY + 6 + i * 24;   // ordre d'affichage tourne (reponses interverties)
    tft.fillRoundRect(SX + 8, yy - 2, PWD - 16, 22, 5, i == quizChoice ? COL_MEAT : COL_TXT);
    bubbleLine(SX + PWD / 2, yy, PWD - 30, q.opt[oi], i == quizChoice ? COL_TXT : COL_BG);
  }
}
void drawQuizScene(bool result) {
  const Quiz &q = QUIZZES[quizPick];
  const char* catName[NCAT] = { "MATHS", "FRANCAIS", "ANGLAIS", "CHIMIE" };
  uint16_t catCol[NCAT] = { tft.color565(90, 200, 255), COL_GOAL, tft.color565(200, 140, 255), tft.color565(255, 170, 60) };
  const uint16_t green = RGB565(20, 120, 50), red = RGB565(170, 30, 30), ink = COL_BG;
  const int PX = 42, PWD = 272, SX = 6;          // bulle du professeur (a droite de son avatar) / de l'eleve (a gauche du sien)
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  drawQuizScore();
  char l1[40], l2[40] = "", l3[40] = "";
  int n;
  uint16_t c1 = ink;
  if (!result) {
    snprintf(l1, sizeof(l1), "%s", q.q1); snprintf(l2, sizeof(l2), "%s", q.q2); n = q.q2[0] ? 2 : 1;
  } else if (quizOk) {
    snprintf(l1, sizeof(l1), "Bravo ! +%d points", quizGain); c1 = green;
    snprintf(l2, sizeof(l2), "%s", quizDest == 0 ? "Niveau suivant !" : "Bien joue !");
    if (quizLvUp) snprintf(l3, sizeof(l3), "Tu deviens %s !", KN_NAME[knowLvl(knowledge)]);
    n = l3[0] ? 3 : 2;
  } else {
    snprintf(l1, sizeof(l1), "Rate ! La reponse :"); c1 = red;
    snprintf(l2, sizeof(l2), "%s", q.opt[q.correct]);
    snprintf(l3, sizeof(l3), "%s", quizDest == 0 ? "On refait le niveau." : "Courage !");
    n = 3;
  }
  int ph = max(38, n * 20 + 12), py0 = 28;
  tft.fillRoundRect(PX, py0, PWD, ph, 8, COL_TXT);
  tft.fillTriangle(PX + 1, py0 + ph - 14, PX + 1, py0 + ph - 4, PX - 8, py0 + ph - 6, COL_TXT);     // queue vers le professeur
  int ty = py0 + (ph - n * 20) / 2 + 2, cx = PX + PWD / 2;
  bubbleLine(cx, ty, PWD - 14, l1, c1);
  bubbleLine(cx, ty + 20, PWD - 14, l2, result && !quizOk ? green : ink);
  if (n == 3) bubbleLine(cx, ty + 40, PWD - 14, l3, result && quizOk ? green : ink);
  int mood = !result ? 0 : quizOk ? 1 : 2;
  quizAvatar(22, py0 + ph - 2, true, mood);
  int sy = py0 + ph + 8;
  if (!result) {
    int sh = 3 * 24 + 10;
    tft.fillRoundRect(SX, sy, PWD, sh, 8, COL_TXT);
    tft.fillTriangle(SX + PWD - 1, sy + sh - 14, SX + PWD - 1, sy + sh - 4, SX + PWD + 8, sy + sh - 6, COL_TXT);   // queue vers l'avatar du joueur
    quizChoiceY = sy; drawQuizChoices();
    quizAvatar(298, sy + sh - 2, false, 0);
    tft.setTextSize(1); tft.setTextColor(catCol[q.cat]);
    ctextf(sy + sh + 10, "%s  -  difficulte %d/5", catName[q.cat], (int)q.diff);
  } else {
    int sh = 34;
    uint16_t bc = quizOk ? RGB565(170, 235, 180) : RGB565(240, 170, 170);
    tft.fillRoundRect(SX, sy, PWD, sh, 8, bc);
    tft.fillTriangle(SX + PWD - 1, sy + sh - 14, SX + PWD - 1, sy + sh - 4, SX + PWD + 8, sy + sh - 6, bc);
    bubbleLine(SX + PWD / 2, sy + 8, PWD - 20, q.opt[(quizChoice + quizRot) % 3], ink);
    quizAvatar(298, sy + sh - 2, false, mood);
    tft.setTextSize(1);
    for (int c = 0; c < NCAT; c++) {                       // niveau de difficulte par matiere
      tft.setTextColor(catCol[c]);
      ctextf(sy + 52 + c * 12, "%s  niv %d.%d   (%d/%d)", catName[c], (int)quizSkill[c], (int)(quizSkill[c] * 10) % 10, (int)quizCorrectByCat[c], (int)quizAskedByCat[c]);
    }
  }
  tft.setTextColor(tft.color565(150, 160, 180)); tft.setTextSize(1);
  ctext(229, result ? "P3 = continuer" : "P2/P1 = choisir   P3 = valider");
}
void drawQuiz() { drawQuizScene(false); }
void drawQuizResult() { drawQuizScene(true); }

// mode debug (retour) : consulter n'importe quel niveau sans avoir progresse.
// Variable RAM uniquement (jamais ecrite en Preferences/NVS) -> redevient
// false a chaque redemarrage, donc a chaque nouveau flash. Bascule : tenir
// les 3 boutons ensemble 2s a l'accueil (voir loop(), case ST_HOME).
bool debugUnlockAll = false;

// plus haut niveau selectionnable : les reussis (bestTime>0) + le prochain
// (tous debloques en mode debug)
int maxUnlockedIdx() {
  if (debugUnlockAll) return NUM_LEVELS - 1;
  int m = UNLOCKED_UP_TO;
  for (int i = 0; i < NUM_LEVELS; i++) if (bestTime[i] > 0 && i + 1 > m) m = i + 1;
  if (m > NUM_LEVELS - 1) m = NUM_LEVELS - 1;
  return m;
}

// mort du joueur : respawn immediat en course 2J, sinon ecran PERDU
void die() {
  snd(SND_DIE);
  deaths++; bqTimeout = false;
  if (raceMode) { respawn(); drawGame(); return; }   // course : on continue
  if (bqOn() && bqVies && (int)deaths < bqVies) { respawn(); drawGame(); return; }   // BRIQUE : encore des vies, on repart du depart
  combo = 0;                 // enchainement brise
  deadChoice = 0;
  state = ST_DEAD;
  drawDead();
}

// place des ennemis (aleatoires mais deterministes par niveau) sur les plateformes
// sures : >= 4 tuiles de sol continu, 2 tuiles de vide au-dessus, loin du depart/drapeau.
void spawnEnemies(int n) {
  nEnemies = 0; popT = 0;
  if (n == CUSTOM_IDX && customH) {                    // niveau perso : un ennemi par 'E', qui patrouille sur la plateforme du dessous
    auto dur = [](char b) { return b == '#' || b == 'I' || b == 'C'; };
    for (int r = 0; r + 1 < levH && nEnemies < ME; r++) for (int c = 0; c < COLS && nEnemies < ME; c++) {
      if (grid[r][c] != 'E') continue;
      grid[r][c] = '.';
      if (!dur(grid[r + 1][c])) continue;                // pose en l'air : ignore
      int c0 = c, c1 = c;
      while (c0 > 0 && dur(grid[r + 1][c0 - 1]) && grid[r][c0 - 1] != '#') c0--;
      while (c1 < COLS - 1 && dur(grid[r + 1][c1 + 1]) && grid[r][c1 + 1] != '#') c1++;
      Enemy &e = enemies[nEnemies++];
      e.active = true; e.lo = c0 * TILE + EW / 2 + 1; e.hi = max(e.lo, (float)((c1 + 1) * TILE - EW / 2 - 1));
      e.x = c * TILE + TILE / 2; e.y = (r + 1) * TILE; e.dir = (c & 1) ? 1 : -1; e.spd = 30;
    }
    return;
  }
  if (n < 2) return;   // les 2 premiers niveaux restent calmes
  randomSeed((uint32_t)n * 40503u + 77u);
  for (int pass = 0; pass < 2 && (pass == 0 || nEnemies < 3); pass++)   // 2e passe : garantit au moins 3 ennemis par niveau
  for (int r = 2; r < levH && nEnemies < ME; r++) {
    int c = 0;
    while (c < COLS && nEnemies < ME) {
      if (!(grid[r][c] == '#' && tileAt(c, r - 1) == '.' && tileAt(c, r - 2) == '.')) { c++; continue; }
      int c0 = c;
      while (c < COLS && grid[r][c] == '#' && tileAt(c, r - 1) == '.' && tileAt(c, r - 2) == '.') c++;
      int c1 = c - 1;
      if (c1 - c0 + 1 < 4) continue;
      if (random(100) >= (pass ? 100 : 65)) continue;
      bool taken = false;                                   // plateforme deja occupee (passe 2)
      for (int i = 0; i < nEnemies; i++) if (enemies[i].y == r * TILE && enemies[i].x >= c0 * TILE && enemies[i].x <= (c1 + 1) * TILE) taken = true;
      if (taken) continue;
      bool ok = true;
      for (int i = 0; i < nTraps && ok; i++) if (traps[i].ty >= r - 2 && traps[i].ty <= r - 1 && traps[i].tx >= c0 - 1 && traps[i].tx <= c1 + 1) ok = false;
      if (abs((int)(spawnX / TILE) - (c0 + c1) / 2) <= (c1 - c0) / 2 + 3 && abs((int)(spawnY / TILE) - r) < 4) ok = false;
      for (int i = 0; i < gpN && ok; i++) if (gpX[i] >= c0 - 3 && gpX[i] <= c1 + 3 && abs(gpY[i] - r) < 4) ok = false;
      if (!ok) continue;
      Enemy &e = enemies[nEnemies++];
      e.active = true; e.lo = c0 * TILE + EW / 2 + 1; e.hi = (c1 + 1) * TILE - EW / 2 - 1;
      e.x = e.lo + random((int)(e.hi - e.lo) + 1); e.y = r * TILE;
      e.dir = random(2) ? 1 : -1; e.spd = 22 + random(20);
    }
  }
}

// --- plateformes en cubes : chaque plateforme (ensemble de blocs '#'/'H' connectes) a SON
// ton, tous ses cubes ont la meme couleur (teinte du niveau melangee a une des 8 teintes) ---
uint8_t platIdx[WROWS][COLS];                 // 255 = pas une plateforme
uint16_t toneF[8], toneM[8], toneL[8], toneD[8];  // face, centre, lumiere, ombre
void buildPlatTones() {
  static const uint16_t T[8] = { RGB565(70,120,200), RGB565(50,165,165), RGB565(85,175,95), RGB565(175,165,60),
                                 RGB565(205,125,60), RGB565(195,85,95), RGB565(145,95,195), RGB565(195,105,165) };
  for (int i = 0; i < 8; i++) {
    toneF[i] = blendCol(COL_LVLBLK, T[i], 0.7f);
    toneM[i] = blendCol(toneF[i], 0xFFFF, 0.14f);
    toneL[i] = blendCol(toneF[i], 0xFFFF, 0.38f);
    toneD[i] = blendCol(toneF[i], 0x0000, 0.42f);
  }
  memset(platIdx, 255, sizeof(platIdx));
  static int16_t q[WROWS * COLS];
  int k = 0;
  for (int r = 0; r < levH; r++) for (int c = 0; c < COLS; c++) {
    if ((grid[r][c] != '#' && grid[r][c] != 'H') || platIdx[r][c] != 255) continue;
    uint8_t id = (k * 3 + curLevel) % 8; k++;
    int h = 0, t = 0; q[t++] = r * COLS + c; platIdx[r][c] = id;
    while (h < t) {                                   // remplissage 4-voisins
      int cr = q[h] / COLS, cc = q[h] % COLS; h++;
      const int dr[4] = { -1, 1, 0, 0 }, dc[4] = { 0, 0, -1, 1 };
      for (int d = 0; d < 4; d++) {
        int nr = cr + dr[d], nc = cc + dc[d];
        if (nr < 0 || nr >= levH || nc < 0 || nc >= COLS || platIdx[nr][nc] != 255) continue;
        if (grid[nr][nc] != '#' && grid[nr][nc] != 'H') continue;
        platIdx[nr][nc] = id; q[t++] = nr * COLS + nc;
      }
    }
  }
}

void loadLevelData(int idx) {
  DBG(20);
  scFlag = 0; curLevel = idx; deaths = 0; levelCoins = 0; nStain = 0; nTrail = 0; trailSpawnT = 0;
  setLevelPalette(idx);
  genLevel(idx); DBG(21); findSpawn(); findGoal(); DBG(22);
  spawnEnemies(idx);
  buildPlatTones();
  nCrumbs = 0;                                   // recense les plateformes fragiles
  for (int ty = 0; ty < levH; ty++)
    for (int tx = 0; tx < COLS; tx++)
      if (grid[ty][tx] == 'C' && nCrumbs < MC) { crumbs[nCrumbs] = { tx, ty, 0, 0 }; nCrumbs++; }
  DBG(23);
  respawn();
  DBG(24);
}

void startLevel(int idx, bool resetCombo) {
  dNoDma = false;                                  // nouvel essai d'envoi DMA
  if (resetCombo) combo = 0;
  raceMode = false;
  if (idx == CUSTOM_IDX && bqCote) { startBqCote(); return; }
  if (idx == CUSTOM_IDX && bqIso) { startBqIso(); return; }
  if (idx < 50) { startWorld(idx); return; }   // niveaux 1-10 exterieur, 11-20 donjon de feu, 21-30 glace, 31-40 desert, 41-50 jungle (mondes larges)
  loadLevelData(idx);
  levelStartMs = millis();
  state = ST_PLAY;
}

// prepare une course 2J : charge le niveau puis lance le compte a rebours
void beginRace(int lvl) {
  raceResult = 0; myFinT = 0;
  vFin = false; vPos = false; vFinWho = -1; for (int k = 0; k < MP_MAX; k++) { mpFin[k] = false; mpPos[k] = false; }
  loadLevelData(lvl);
  raceMode = false;               // active a la fin du compte a rebours
  cdStart = millis();
  state = ST_COUNTDOWN;
}

// ================= MONDES (niveaux 31-40 : desert, 41-50 : jungle) =================
// Mondes larges (96 tuiles ~ 5 ecrans), camera qui suit le joueur en X et en Y, vue zoomee x1.25, envoi DMA.
// DESERT : le joueur est une petite flamme bleue ; le soleil la fait rapetisser et palir (jauge de chaleur), l'ombre la ravive.
//   Les rayons arrivent en biais (haut a droite) : tout obstacle (bloc, caisse, cactus, acacia) projette son ombre.
// JUNGLE : lianes a saisir en sautant dessus, pour se balancer au-dessus des vides ; saut = lacher.
// Communs : cactus/epines = pics, petits lanceurs ecrasables en sautant dessus, pieces, 3 etoiles, panneaux = reprise, drapeau = fin.
// Chaque dizaine a 10 variantes (difficulte croissante). La fin de niveau passe par finishLevel() comme les niveaux classiques.
#define DW 96                       // largeur du monde en tuiles
#define DHMAX 34                    // hauteur max du monde en tuiles (tableau)
int dH = 18, dGY = 14, dWW = 96;    // hauteur, 1re rangee de sol, largeur utile : 18/14/96 sauf niveaux 1-9 (taille d'ecran qui grandit)
#define DH dH
#define DGY dGY
int dVW = 256, dVH = 192, dSH = 96; // vue logique : 256x192 zoomee x1.25 (16 x 12 tuiles), ou 320x240 sans zoom pour les niveaux 1-10 (20 x 15 tuiles)
#define DVW dVW
#define DVH dVH
#define DSH dSH                     // hauteur d'une bande logique (96 -> 120 lignes ecran, ou 120 sans zoom)
#define DPG 600.0f                  // gravite des projectiles
char dg[DHMAX][DW + 1];                // S sol, X caisse (desert) / rondin (jungle), T arbre, k/c/m cactus ou epines (pics), P panneau (reprise), A fleche, G drapeau, O piece, * etoile, g/b/u/r/s/F/p decor
float dcamX, dcamY, dpx, dpy, dvx, dvy, dCoyote, dJumpBuf, dHeat, dPhaseT, dHeatRate, dCoolRate, dThrowCd, dQs;   // dQs : temps passe dans le sable mouvant
bool dGround, dFacingR, dShaded, dWinFlag;
bool dSlide; float dPopT, dPopY; int dPopPts, dFlX = -1, dFlY = -1, dPoleHpx = 5 * TILE;   // mat du drapeau (mondes 1-50) : glissade, "+N", position de la case G, hauteur du mat (5 cases au plus)
int dChkX, dChkY, dPhase, dDeaths, dCoinsGot, dStars, dTheme, dVar;   // dPhase : 0 jeu, 2 pause apres une defaite ; dTheme : 0 desert, 1 jungle
unsigned long dStartMs;
struct DTree { int x, y; } dTrees[8]; int nDTrees;           // x = centre (px), y = sol sous le tronc (px)
struct DEn { float x, y, cd; bool alive, right, gl; float x0; } den[16]; int nDEn;   // x0 : centre de la ronde (cactus du desert)   // x = centre, y = pieds (px) ; gl = glitch apparu en cours de partie
struct DPr { float x, y, vx, vy, life; bool on, gl; } dpr[8];   // projectiles (pierres / noix de coco) ; gl = bloc corrompu qui tombe du ciel
struct DPt { float x, y, vx, vy, life; uint16_t c; uint8_t s; } dpt[16]; // etincelles ; s = taille de depart (0 = 2 px)
struct DVine { int ax, ay, len; } dVines[12]; int nDVines, dVineIdx;   // lianes : ancrage (px) et longueur
float dSpd; bool dWarm;                                      // vitesse (0.6 dans les niveaux < 29) ; au chaud (feu de camp)
struct DSrc { int x, y, r; } dSrc[24]; int nDSrc;            // sources de chaleur (torches du donjon)
struct DTrap { int tx; float t; int st; } dTraps[12]; int nDTraps;   // pics qui surgissent : st 0 repos, 1 sorti (tuile 'v' = plaque visible, 'y' = cache)
struct DDrop { int tx, row, st; float y, vy; } dDr[4]; int nDr;       // briques qui tombent quand on arrive dessous : st 0 attente, 1 chute, 2 posee (devient un bloc)
int dVineLast = -1; float dVineL, dTheta, dOmega, dVineCd;                       // liane saisie : longueur, angle, vitesse angulaire ; delai avant une nouvelle saisie
const char* dDeathMsg;                                       // ce que dit l'avatar quand il perd
// ---- SCENARIO : des glitchs veulent detruire le jeu, la flamme (l'avatar) pousse a le developper.
// glitchLvl (0..100, NVS "story", survit aux mises a jour) : monte a chaque mort, baisse a chaque niveau gagne
// et a chaque nouvelle version flashee. Il regle la taille de la zone corrompue (fin de chaque niveau) et la frequence des glitchs.
Preferences story;
int glitchLvl = 60;
int evo = 0;                                                 // evolution de l'avatar (NVS story "evo") : +1 par bonne reponse a une piece bleue
#define EVO_MAX 27                                           // 0-7 : 1 a 8 pixels ; 8-9 : couleurs ; 10-11 : trainee ; 12-16 : flamme qui grandit ; 17-20 bonhomme ; 21-23 cosmonaute ; 24-27 robot
#define EVO_STEP 4                                           // bonnes reponses par stade
int evoPts = 0; bool evoUp = false;                          // bonnes reponses vers le prochain stade ; stade gagne a la derniere reponse
const char *evoName(int e) {
  static char b[28];
  if (e < 8) snprintf(b, sizeof(b), "%d pixel%s", e + 1, e ? "s" : "");
  else if (e < 10) strcpy(b, e == 8 ? "Des couleurs" : "Plus de couleurs");
  else if (e < 12) strcpy(b, e == 10 ? "Une trainee" : "Trainee multicolore");
  else if (e < 17) strcpy(b, e == 12 ? "Une flamme !" : "Flamme plus grande");
  else {
    static const char *const N[11] = { "Meche de flamme", "Bonhomme", "Casquette", "Lunettes", "Casque cosmo", "Combinaison", "Jetpack", "Tete de robot", "Corps en metal", "Canon", "Robot final" };
    strcpy(b, N[min(e, 27) - 17]);
  }
  return b;
}
bool dQ; int dQPh; float dQX, dQY; unsigned long dQMs, dQAMs; int dQW; int dQvw, dQvh, dQsh; float dQcx, dQcy;   // + vue et camera d'avant le zoom      // question en jeu (piece bleue) : 0 on choisit, 1 resultat ; position du Dev ; debut de la pause
char updMsg[84];                                             // annonce de mise a jour (ecran titre), vide sinon
int gZone0;
int dBuild = 10;                                             // niveaux 1-10 : le decor se construit (0 = aplats unis ... 6 = complet)                                                  // 1re colonne de la zone corrompue du niveau
float gEvT, gSayT, gSayCd; int gSayE; char gSayTxt[48], gStartTxt[48];
struct GHole { int c; float t; bool on, open, up; } gH[4];    // glitchs du sol : trou (up=false) ou bloc qui surgit (up=true) ; clignotent 0,7 s, s'ouvrent, se referment
int tCol = -1, tA, tB; float gTransT, gAvT;                   // niveau de transition : colonne de bascule, monde de gauche / de droite ; rafale de glitch ; bulle de l'avatar
void setRates(int theme, int var);
uint16_t dTileSprB[6][256];                                  // tuiles du monde de droite (transition) / des incrustations
int pC0[2], pC1[2], nPatch;                                  // incrustations du monde suivant (colonnes), niveaux 8-9 et 18-19
bool dCorr; int dSX, dSY;
int dLives; float dInvT, dSafeT; float dSafeX[8], dSafeY[8]; int nSafe, iSafe;   // 2 vies par niveau ; invincibilite apres reapparition ; positions sures recentes (1 toutes les 0,25 s)                                    // niveau "couloir" (1-9, 11-19) ; point de depart (on y revient a chaque mort)
#define tMix (tCol >= 0 || nPatch > 0)
int colTheme(int tx) {                                       // monde dessine dans la colonne tx
  if (tCol >= 0) return tx >= tCol ? tB : tA;
  for (int i = 0; i < nPatch; i++) if (tx >= pC0[i] && tx <= pC1[i]) return tB;
  return tA;
}
uint16_t dysW[880];                                          // desert, ombre : pour chaque rayon (u = x + 0.7 y, pas de 2 px), hauteur monde du 1er obstacle -- calcule une fois par niveau
uint8_t dHump[DW]; bool dHumpOn;                             // desert : hauteur (px, 0..12) des dunes douces au-dessus de la rangee du sol, au centre de chaque colonne
float dHumpAt(float x) {                                     // relief lisse (interpolation entre les centres de colonnes)
  if (!dHumpOn) return 0;
  float f = x / TILE - 0.5f; int i = (int)floorf(f); float t = f - i;
  int a = constrain(i, 0, DW - 1), b = constrain(i + 1, 0, DW - 1);
  return dHump[a] * (1 - t) + dHump[b] * t;
}
int8_t dDune[760];                                           // relief des dunes lointaines (table)
bool dDma, dDmaBusy; spi_device_handle_t dDev; uint16_t *dOb;   // envoi DMA : tampon de sortie (320x120, octets deja dans l'ordre de l'ecran)
uint16_t dTileSpr[6][256];                                   // tuiles 16x16 pre-dessinees : sol dessus / interieur (2 grains), bloc (caisse ou rondin)

static inline int zPh(int ty) { return (int)((millis() + ty * 733) % 4000); }   // plateforme glitch 'z' : 0-2399 solide, 2400-2999 clignote (solide), 3000-3999 absente
bool dsolid(int tx, int ty) {
  if (tx < 0 || tx >= dWW) return true;
  if (ty < 0 || ty >= DH) return false;
  char c = dg[ty][tx];
  return c == 'S' || c == 'X' || c == 'Z' || (c == 'z' && zPh(ty) < 3000);
}
void dPuff(float x, float y, uint16_t c, int n) {
  for (int i = 0; i < 16 && n > 0; i++) if (dpt[i].life <= 0) {
    dpt[i] = { x, y, (float)random(-70, 71), (float)random(-120, -20), 0.5f, c }; n--;
  }
}
uint16_t dMix(uint16_t a, uint16_t b, int t) {           // melange RGB565 : t = 0..256 (256 = b)
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31, br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar * (256 - t) + br * t) >> 8) << 11) | (((ag * (256 - t) + bg * t) >> 8) << 5) | ((ab * (256 - t) + bb * t) >> 8));
}
// ---- generation : petites aides communes aux deux mondes ----
void wput(int c, int r, char ch) { if (c >= 0 && c < DW && r >= 0 && r < DH) dg[r][c] = ch; }
void wpit(int a, int b) { char f = dTheme == 3 ? 'L' : '.'; for (int c = a; c <= b; c++) for (int r = DGY; r < DH; r++) dg[r][c] = f; }   // donjon : le vide est de la lave
void wstack(int c, int h, char ch) { for (int i = 1; i <= h; i++) wput(c, DGY - i, ch); }
void wtree(int c, int baseRow) { if (dBuild < 1) return; wput(c, baseRow, 'T'); if (nDTrees < 8) { dTrees[nDTrees].x = c * TILE + 8; dTrees[nDTrees].y = (baseRow + 1) * TILE; nDTrees++; } }
void wguy(int c, int baseRow) { if (nDEn < 16) { den[nDEn] = { (float)(c * TILE + 8), (float)((baseRow + 1) * TILE), 1.0f + 0.3f * nDEn, true, false }; den[nDEn].x0 = den[nDEn].x; nDEn++; } }
void wcoin(int c, int k) {                                         // moins de pieces : groupes de 4 colonnes gardes ou retires ensemble (sans toucher a random())
  uint32_t h = (uint32_t)((c >> 2) * 2654435761u) ^ (uint32_t)(dTheme * 977 + dVar * 131 + k);
  h ^= h >> 15; h *= 0x2c1b3c6dU; h ^= h >> 12;
  if (dTheme == 4 && h % 5 >= 2) return;                          // pour l'instant : niveaux 1-10 seulement
  wput(c, DGY - k, 'O');
}
void wvine(int c, int row, int lenTiles) { if (nDVines < 12) { dVines[nDVines].ax = c * TILE + 8; dVines[nDVines].ay = row * TILE; dVines[nDVines].len = lenTiles * TILE; nDVines++; } }
void wlog(int c0, int c1, int row) { for (int c = c0; c <= c1; c++) wput(c, row, 'X'); }
void wclear() {
  for (int r = 0; r < DHMAX; r++) { for (int c = 0; c < DW; c++) dg[r][c] = '.'; dg[r][DW] = 0; }
  nDTrees = 0; nDEn = 0; nDVines = 0; nDSrc = 0; nDTraps = 0; nDr = 0; memset(dHump, 0, sizeof(dHump)); dHumpOn = false;
  for (int i = 0; i < 4; i++) gH[i].on = false;
  for (int c = 0; c < DW; c++) for (int r = DGY; r < DH; r++) dg[r][c] = 'S';
}
// ---- generation procedurale : 5 mondes x 10 variantes. Segments tires au hasard (graine = monde + variante), difficulte croissante :
// trous (puis larges trous a pierres), murs, escaliers, colonnes, epines, plateformes, lanceurs + specialites du monde.
// Milieu de dizaine (variantes 4-6) : des pics SURGISSENT mais sont visibles (plaque au sol) ; fin de dizaine (7-9) : pics et trous CACHES
// (reveles de pres) en plus. Vitesse 60 % (niveaux < 29) : vides et ecarts plus courts. ----
void wdec(int x, char ch) {                                       // decor pose sur le sol libre (les torches sont des sources de chaleur)
  if (dBuild < 5 || x < 0 || x >= DW || dg[DGY][x] != 'S' || dg[DGY - 1][x] != '.') return;
  dg[DGY - 1][x] = ch;
  if (ch == 'f' && nDSrc < 24) { dSrc[nDSrc].x = x * TILE + 8; dSrc[nDSrc].y = (DGY - 1) * TILE + 8; dSrc[nDSrc].r = 44; nDSrc++; }
}
void waddTraps(int var) {                                         // pics qui surgissent (visibles) / pics et trous caches
  int nPop = var >= 7 ? 2 : (var >= 4 ? var - 3 : 0), nHid = var >= 7 ? var - 5 : 0, nHole = var >= 7 ? var - 6 : 0;
  for (int tries = 0; tries < 400 && (nPop > 0 || nHid > 0 || nHole > 0); tries++) {
    int c = 10 + random(0, 70);
    bool ok = dg[DGY][c] == 'S' && dg[DGY][c - 1] == 'S' && dg[DGY][c + 1] == 'S';
    for (int d = -3; d <= 3 && ok; d++) { char q = dg[DGY - 1][c + d]; if (q != '.' && q != 'g' && q != 'u' && q != 'b') ok = false; if (dg[DGY - 2][c + d] == 'X' || dg[DGY - 2][c + d] == 'S') ok = false; }
    if (!ok) continue;
    if (nPop > 0 && nDTraps < 12) { dg[DGY - 1][c] = 'v'; dTraps[nDTraps].tx = c; dTraps[nDTraps].t = 0; dTraps[nDTraps].st = 0; nDTraps++; nPop--; }
    else if (nHid > 0) { dg[DGY - 1][c] = 'x'; nHid--; }
    else if (nHole > 0) { for (int r = DGY; r < DH; r++) dg[r][c] = 'h'; nHole--; }
  }
}
void desertRelief(int var) {                                      // desert : sable mouvant + dunes douces, sur les parties planes sans obstacle (hors depart et arrivee)
  const int GY = DGY;
  bool fr[DW];
  for (int c = 0; c < DW; c++) {
    bool ok = c >= 4 && c < 84 && dg[GY][c] == 'S';
    for (int r = GY - 3; r < GY && ok; r++) { char q = dg[r][c]; if (q != '.' && q != 'g' && q != 'u' && q != 'b' && q != 'r' && q != 's' && q != 'O') ok = false; }
    fr[c] = ok;
  }
  for (int i = 0; i < nDEn; i++) { int ec = (int)(den[i].x / TILE); for (int d = -3; d <= 3; d++) if (ec + d >= 0 && ec + d < DW) fr[ec + d] = false; }   // les cactus rodent sur du plat
  int nq = 1 + var / 3;                                            // sable mouvant : 3 a 5 cases, 1 a 4 par niveau
  for (int tries = 0; tries < 300 && nq > 0; tries++) {
    int L = 3 + random(0, 3), c = 12 + random(0, 64 - L);
    bool ok = true; for (int x = c - 2; x <= c + L + 1 && ok; x++) if (!fr[x]) ok = false;
    if (!ok) continue;
    for (int x = c - 1; x <= c + L; x++) fr[x] = false;
    for (int x = c; x < c + L; x++) {
      dg[GY][x] = 'q'; dg[GY + 1][x] = 'q';
      if (dg[GY - 1][x] != 'O') dg[GY - 1][x] = '.';
      if (dg[GY - 4][x] == '.') dg[GY - 4][x] = 'O';                // pieces au-dessus : il faut sauter
    }
    nq--;
  }
  int dist[DW], d = 0;                                             // distance a la colonne non plane la plus proche : le relief monte doucement (5 px par colonne au plus)
  for (int c = 0; c < DW; c++) { d = fr[c] ? min(d + 1, 99) : 0; dist[c] = d; }
  d = 0; for (int c = DW - 1; c >= 0; c--) { d = fr[c] ? min(d + 1, 99) : 0; dist[c] = min(dist[c], d); }
  for (int c = 0; c < DW; c++) if (fr[c]) {
    float v = 6.5f + 4.5f * sinf(c * 0.27f + 0.8f + var) + 3.5f * sinf(c * 0.63f + 2.1f);
    dHump[c] = (uint8_t)min(constrain((int)(v + 0.5f), 0, 12), dist[c] * 5);
  }
  dHumpOn = true;
}
void genProc(int theme, int var) {
  wclear();
  const int GY = DGY;
  const bool slow = dSpd < 1.0f;                                   // niveaux < 29 : course a 60 % -> sauts plus courts
  randomSeed(4000 + theme * 977 + var * 31);
  const int maxGap = slow ? 2 + var / 5 : 3 + var / 3;             // largeur de vide franchissable d'un saut (2..4 lent, 3..6 rapide)
  const int sp = slow ? 5 : 4;                                     // ecart mini entre deux pics
  int c = 9, lastP = 3, lastSpecial = 0, stars = 0, vinePits = 0;
  static const char DS[5][5] = { { 'g', 'r', 'b', 'u', 's' }, { 'g', 'F', 'p', 'b', 'u' }, { 'g', 'r', 'u', 'b', 'F' }, { 'r', 's', 'b', 'f', 'g' }, { 'g', 'u', 'r', 'o', 'e' } };
  auto decor = [&](int x) { wdec(x, DS[theme][random(0, 5)]); };
  auto coinArc = [&](int x0, int w, int k) { for (int i = 0; i < w; i++) wcoin(x0 + i, k + (i == w / 2 ? 1 : 0)); };
  auto flat = [&](int w, bool dec) {
    for (int i = 0; i < w; i++) if (dec && random(0, 3) == 0) decor(c + i);
    if (w >= 5 && var >= 1 && nDEn < 3 + var && random(0, 3) == 0) wguy(c + w / 2, GY - 1);
    if (dec && w >= 3) { wcoin(c + 1, 2); wcoin(c + 2, 2); }
    c += w;
  };
  auto pit = [&](int g) {
    wpit(c, c + g - 1); coinArc(c, g, 3);
    if (stars < 3 && g >= 3 && random(0, 3) == 0) { wput(c + g / 2, GY - 5, '*'); stars++; }
    c += g; flat(2, false);
  };
  auto wide = [&](int g) {                                         // grand vide : pierres ou on se pose (chaque saut <= maxGap)
    wpit(c, c + g - 1);
    int x = c;
    while (c + g - x > maxGap) {
      int step = 2 + random(0, maxGap - 1); if (step > maxGap) step = maxGap;
      int sw = 2 + random(0, 2), sx = x + step;
      if (sx + sw > c + g - 1) { sw = c + g - 1 - sx; if (sw < 1) break; }
      for (int i = 0; i < sw; i++) { wput(sx + i, GY - 1, 'S'); wput(sx + i, GY, 'S'); }
      coinArc(sx, sw, 3);
      if (var > 2 && nDEn < 14 && random(0, 4) == 0) wguy(sx, GY - 2);
      x = sx + sw;
    }
    if (stars < 3 && random(0, 2) == 0) { wput(c + g / 2, GY - 5, '*'); stars++; }
    c += g; flat(2, false);
  };
  auto wall = [&](int h) { wstack(c + 1, h, 'X'); coinArc(c, 3, h + 2); c += 4; flat(1, false); };
  auto stairs = [&]() {
    int hm = 1 + random(0, 3), x = c;
    for (int h = 1; h <= hm; h++) { wstack(x, h, 'S'); wstack(x + 1, h, 'S'); x += 2; }
    wstack(x, hm, 'S'); wstack(x + 1, hm, 'S'); x += 2;
    for (int h = hm; h >= 1; h--) { wstack(x, h, 'S'); wstack(x + 1, h, 'S'); x += 2; }
    coinArc(c + 2 * hm, 4, hm + 2);
    if (var >= 3 && nDEn < 14 && random(0, 2) == 0) wguy(c + 2 * hm + 1, GY - hm - 1);
    c = x; flat(1, false);
  };
  auto columns = [&](int n) {                                      // colonnes disseminees : piliers de 2 a 4 blocs de haut
    int x = c + 1;
    for (int i = 0; i < n; i++) { int h = 2 + random(0, 3); wstack(x, h, 'X'); wcoin(x, h + 1); x += 3 + random(0, 2); }
    c = x + 1;
  };
  auto thorns = [&](int n) {
    for (int i = 0; i < n; i++) { int x = c + 2 + i * sp; wput(x, GY - 1, "ckm"[random(0, 3)]); wcoin(x - 1, 4); wcoin(x, 5); wcoin(x + 1, 4); }
    c += n * sp + 3; flat(1, false);
  };
  auto platforms = [&](int n) {
    int k = 3, x = c + 1;
    for (int i = 0; i < n; i++) {
      int pw = 2 + random(0, 2); k += random(-1, 2); if (k < 2) k = 2; if (k > 5) k = 5; if (i == 0 && k > 4) k = 4;
      for (int j = 0; j < pw; j++) wput(x + j, GY - k, 'X');
      coinArc(x, pw, k + 2);
      if (var >= 2 && nDEn < 14 && random(0, 3) == 0) wguy(x + pw / 2, GY - k - 1);
      x += pw + 1 + random(0, maxGap > 3 ? 3 : 2);
    }
    if (stars < 3 && random(0, 2) == 0) { wput(x - 2, GY - (k + 3), '*'); stars++; }
    c = x + 1; flat(1, false);
  };
  auto arena = [&](int w) {
    int m = 1 + var / 3; if (m > 3) m = 3;
    for (int i = 0; i < m; i++) wguy(c + 2 + i * (w / (m + 1)), GY - 1);
    wcoin(c + 1, 2); wcoin(c + w - 2, 2); c += w;
  };
  auto checkpoint = [&]() { flat(2, false); wput(c, GY - 1, 'P'); lastP = c; c += 2; flat(2, true); };
  auto special = [&](int which) {
    lastSpecial = c;
    if (theme == 0) {                                              // desert : ombre
      if (which == 0) { wtree(c + 3, GY - 1); coinArc(c + 1, 5, 2); c += 7; }
      else { for (int x = c + 1; x <= c + 6; x++) wput(x, GY - 5, 'S'); wput(c + 3, GY - 1, 'k'); coinArc(c + 1, 5, 2); c += 8; }
    } else if (theme == 1) {                                       // jungle : lianes
      if (which == 0 && !slow) {
        bool w14 = random(0, 2) == 0; int W = w14 ? 14 : 12;
        wpit(c, c + W - 1);
        if (!w14) { if (var < 5) { wvine(c + 3, 2, 8); wvine(c + 8, 2, 8); } else { wvine(c + 2, 2, 8); wvine(c + 9, 2, 8); } }
        else { if (var < 5) { wvine(c + 3, 3, 7); wvine(c + 7, 3, 7); wvine(c + 11, 3, 7); } else { wvine(c + 2, 3, 7); wvine(c + 12, 3, 7); } }
        coinArc(c + 2, W - 4, 5);
        if (stars < 3) { wput(c + W / 2, GY - 9, '*'); stars++; }
        vinePits++; c += W; flat(2, false);
      } else { wtree(c + 3, GY - 1); coinArc(c + 1, 5, 2); c += 7; }
    } else if (theme == 2) {                                       // glace : feu de camp (au chaud) ou pics de glace
      if (which == 0) { wput(c + 2, GY - 1, 'W'); coinArc(c + 1, 3, 2); c += 6; }
      else thorns(2 + var / 5);
    } else if (theme == 3) {                                       // donjon : torches (chaleur) ou grand lac de lave
      if (which == 0) { wdec(c + 1, 'f'); wdec(c + 4, 'f'); wguy(c + 3, GY - 1); coinArc(c + 1, 4, 2); c += 7; }
      else wide(maxGap + 3 + random(0, 3));
    } else {                                                       // exterieur : maison / barils / clotures
      wdec(c + 1, 'o'); wtree(c + 4, GY - 1); wdec(c + 7, 'e'); coinArc(c + 1, 7, 2); c += 9;
    }
  };
  // depart
  wput(3, GY - 1, 'P'); wdec(5, DS[theme][0]); wdec(6, DS[theme][random(1, 5)]); wcoin(6, 2); wcoin(7, 2); wcoin(8, 2);
  if (theme == 2) wput(1, GY - 1, 'W');
  int lastKind = -1;
  while (84 - c >= 6) {                                              // on remplit jusqu'a la colonne 84 : chaque segment est choisi selon la place restante
    int room = 84 - c;
    if (c - lastP >= 22 && room >= 10) { checkpoint(); continue; }
    if (theme == 0 && c - lastSpecial >= 24 && room >= 10) { special(0); continue; }          // desert : un abri a l'ombre au moins tous les 24 tuiles
    if (theme == 2 && c - lastSpecial >= 22 && room >= 8) { special(0); continue; }           // glace : un feu de camp au moins tous les 22 tuiles
    if (theme == 1 && !slow && room >= 18 && ((c > 24 && vinePits < 1) || (c > 44 && vinePits < 2))) { special(0); continue; }   // jungle : au moins 2 lianes
    int sel[40], ns = 0;
    auto add = [&](int id, int w) { for (int i = 0; i < w && ns < 40; i++) sel[ns++] = id; };
    add(0, 2); add(1, 3); add(3, 2); add(4, 2); add(5, 1 + var / 3); add(6, 3); add(10, 2);
    if (var >= 2) add(2, 1 + var / 2);
    if (var >= 1) add(7, 1 + var / 3);
    add(8, 2); add(9, theme == 3 ? 3 : 1);
    int kind = sel[random(0, ns)];
    if (kind == lastKind && kind != 0) kind = 0;
    int nTh = 1 + random(0, 2 + var / 4), nPl = c > 44 ? 2 : 2 + random(0, 2), nCol = 3 + random(0, 2), wd = maxGap + 2 + random(0, 4), pg = 2 + random(0, maxGap - 1);
    int need = 6;
    switch (kind) { case 1: need = pg + 4; break; case 2: need = wd + 4; break; case 4: need = 17; break; case 5: need = nTh * sp + 5; break; case 6: need = nPl * 7 + 3; break; case 7: need = 11; break; case 8: case 9: need = 18; break; case 10: need = nCol * 5 + 3; break; default: need = 6; }
    if (need > room) kind = 0;
    lastKind = kind;
    switch (kind) {
      case 0: flat(3 + random(0, 3), true); break;
      case 1: pit(pg); break;
      case 2: wide(wd); break;
      case 3: wall(1 + random(0, 3)); break;
      case 4: stairs(); break;
      case 5: thorns(nTh); break;
      case 6: platforms(nPl); break;
      case 7: arena(8 + random(0, 2)); break;
      case 8: special(0); break;
      case 9: special(1); break;
      default: columns(nCol); break;
    }
    if (kind == 1 || kind == 2) flat(1, true);
  }
  flat(1, false);
  // arrivee : escalier, plateau, drapeau (jusqu'au bord)
  for (int x = 85; x <= 95; x++) { int h = x <= 86 ? 1 : x <= 88 ? 2 : 3; wstack(x, h, 'S'); }
  wtree(92, GY - 4); wput(94, GY - 4, 'G');
  if (theme == 4 && dBuild >= 6) { wput(89, GY - 4, 'N'); }
  int sx[3] = { 87, 91, 93 };
  for (int i = 0; stars < 3 && i < 3; i++) { wput(sx[i], GY - 6, '*'); stars++; }
  for (int x = 6; x < 84; x += 7 + random(0, 4)) if (dg[GY][x] == 'S' && dg[GY - 1][x] == '.' && random(0, 2) == 0) wcoin(x, 2);
  waddTraps(var);
  if (theme == 0) desertRelief(var);
  dChkX = 2 * TILE + 2; dChkY = GY * TILE - (int)PH;
  setRates(theme, var);
}
void setRates(int theme, int var) {
  dHeatRate = theme == 0 ? 12.0f + var : theme == 2 ? 9.0f + var * 0.9f : 30.0f + var * 1.6f;
  dCoolRate = theme == 0 ? 25.0f - var * 0.8f : 16.0f;
  dThrowCd = 1.9f - var * 0.09f;
}
// ---- niveaux 1-9 : taille d'ecran qui grandit. 1-3 = un seul ecran (16 x 12 tuiles), puis a chaque niveau
// + un demi-ecran en largeur (8 tuiles) ou en hauteur (6 tuiles), au hasard (suite fixe). Plus haut qu'un ecran = tour a gravir.
void corrSize(int v, int &W, int &H) {                            // un ecran = la vue (20 x 15 tuiles sans zoom), puis + un demi-ecran
  int W0 = DVW / TILE, H0 = DVH / TILE; W = W0; H = H0; uint32_t q = 12345;
  for (int i = 3; i <= v; i++) { q = q * 1103515245u + 12345u; if (((q >> 16) & 1) && H + H0 / 2 <= 30) H += H0 / 2; else W += W0 / 2; }
}
void genCorr(int var) {
  wclear();
  const int GY = DGY, W = dWW;
  randomSeed(7000 + var * 31 + dTheme * 977);                     // graine propre a chaque monde : le niveau 11 ne copie plus le niveau 1
  wput(1, GY - 1, 'P');
  bool tower = GY > 11;                                            // plus haut qu'un ecran
  int tw = tower ? min(W - 4, 14) : 0, gEnd = W - tw - 1, stars = 0;
  auto star = [&](int c, int r) { if (stars < 3 && c > 0 && c < W && dg[r][c] == '.') { dg[r][c] = '*'; stars++; } };
  int c = 3 + random(0, 3), last = -1, nClimb = 0;
  // sol : un segment tire au hasard a chaque fois (jamais deux fois le meme de suite), selon la place et la difficulte.
  // Sauts a 60 % de vitesse : vide franchissable <= 3 tuiles a plat, <= 2 en montant d'une ou deux tuiles ; colonne franchissable <= 3 de haut.
  while (c < gEnd - 3) {
    int room = gEnd - 3 - c, sel[24], ns = 0;
    auto add = [&](int id, int w, int need) { if (id != last && need <= room) for (int i = 0; i < w && ns < 24; i++) sel[ns++] = id; };
    add(0, 2, 3);                                                  // plat (decor, ennemi)
    add(1, 3, 4);                                                  // colonne a franchir (1 ou 2 de large, 2..3 de haut)
    if (var >= 1) add(2, 2 + var / 3, 8);                          // vide / lave avec plateformes au-dessus
    if (var >= 1) add(3, 2, 4);                                    // petit trou
    if (var >= 1) add(4, 1, 4);                                    // pile de blocs glitch
    if (var >= 2) add(5, 2, 9);                                    // deux colonnes avec un trou entre elles
    if (var >= 2) add(6, 2, 6);                                    // marche haute (on monte puis on redescend)
    if (var >= 3 && nClimb < 1 + var / 4) add(7, 2, 8);            // colonne a grimper (saut de mur en mur)
    if (var >= 4) add(8, 1, 6);                                    // epine
    int k = ns ? sel[random(0, ns)] : 0; last = k;
    switch (k) {
      case 1: { int w = random(0, 3) == 0 ? 2 : 1, h = w == 2 ? 2 : 2 + random(0, 2);   // 2 de large : 2 de haut (sinon trop long a sauter a 60 %)
        for (int i = 0; i < w; i++) wstack(c + 1 + i, h, 'X');
        wcoin(c + 1, h + 2); c += 3 + w; break; }
      case 2: { int g = min(room - 2, 4 + (int)random(0, 2 + var / 2)), np = (g + 1) / 4;   // vide de g tuiles, np plateformes de 2
        wpit(c + 1, c + g);
        for (int i = 0; i < np; i++) {
          int px = c + 1 + (g * (2 * i + 1)) / (2 * np) - 1, h = 1 + random(0, var >= 5 ? 3 : 2);
          wput(px, GY - h, 'X'); wput(px + 1, GY - h, 'X'); wcoin(px, h + 2); wcoin(px + 1, h + 2);
          if (var >= 6 && random(0, 3) == 0) wput(px + random(0, 2), GY - h, 'z');   // plus loin : une des deux tuiles glitche
        }
        if (g >= 6) star(c + g / 2, GY - 5);
        c += g + 2; break; }
      case 3: wpit(c + 1, c + 2); wcoin(c + 1, 3); wcoin(c + 2, 3); c += 4; break;
      case 4: { int h = 1 + random(0, var >= 4 ? 2 : 1); wstack(c + 1, h, 'z'); wcoin(c + 1, h + 2); c += 4; break; }
      case 5: { int h1 = 2 + random(0, 2), h2 = 2 + random(0, 2), g = 2 + random(0, 2);
        wstack(c + 1, h1, 'X'); wpit(c + 2, c + 1 + g); wstack(c + 2 + g, h2, 'X');
        for (int i = 0; i < g; i++) wcoin(c + 2 + i, max(h1, h2) + 2);
        c += g + 5; break; }
      case 6: { int h = 2, w = 3 + random(0, 2);
        wstack(c + 1, 1, 'X'); for (int i = 0; i < w; i++) wstack(c + 2 + i, h, 'X'); wstack(c + 2 + w, 1, 'X');
        for (int i = 0; i < w; i++) wcoin(c + 2 + i, h + 1);
        if (var >= 5 && random(0, 2) == 0) wguy(c + 2 + w / 2, GY - h - 1);
        c += w + 4; break; }
      case 7:                                                      // paroi suspendue (on passe dessous) + colonne : on monte en sautant de mur en mur
        for (int y = GY - 7; y <= GY - 3; y++) wput(c + 1, y, 'X');
        for (int y = GY - 7; y < GY; y++) { wput(c + 4, y, 'X'); wput(c + 5, y, 'X'); }
        wcoin(c + 2, 3); wcoin(c + 3, 5); nClimb++; c += 7; break;
      case 8: wput(c + 2, GY - 1, var >= 6 ? "ckm"[random(0, 3)] : 'c'); c += 5; break;
      default: { int w = 2 + random(0, 3);
        if (random(0, 3) == 0) wdec(c + 1, (dTheme == 3 ? "rsbfg" : dTheme == 2 ? "grubF" : "gurbo")[random(0, 5)]);
        if (var >= 3 && random(0, 3) == 0 && (dTheme != 4 || var >= 9)) wguy(c + 1, GY - 1);
        if (random(0, 4) == 0) wtree(c + 1, GY - 1);
        c += w; }
    }
  }
  if (var >= 1 && nDTrees == 0) for (int x = W / 3; x < W - 3; x++) if (dg[GY][x] == 'S' && dg[GY - 1][x] == '.') { wtree(x, GY - 1); break; }   // au moins un arbre
  if (var >= 7) {                                                  // le monde suivant s'incruste (glitche) : 1 puis 2 bandes
    int np = var == 7 ? 1 : 2, w = var == 7 ? 4 : 5;
    for (int i = 0; i < np; i++) {
      int c0 = 5 + (W - 10) * (i + 1) / (np + 1) - w / 2; if (c0 < 5 || c0 + w > W - 4) continue;
      pC0[nPatch] = c0; pC1[nPatch] = c0 + w - 1; nPatch++;
      for (int x = c0 + 1; x < c0 + w - 1; x += 2) if (dg[GY][x] == 'S' && dg[GY - 1][x] == '.') { if (dTheme == 4) wdec(x, 'f'); else wtree(x, GY - 1); }
    }
    tB = dTheme == 4 ? 3 : 2;
  }
  // pieges fixes (a memoriser) : un pic cache qui surgit juste avant l'arrivee, une brique qui tombe au milieu (2 a partir du niveau 5)
  auto clearCol = [&](int x) {
    if (x < 4 || x > W - 3 || dg[GY][x] != 'S' || dg[GY][x - 1] != 'S' || dg[GY][x + 1] != 'S') return false;
    for (int r = 0; r < GY; r++) if (dg[r][x] != '.') return false;
    return true;
  };
  for (int x = tower ? gEnd - 1 : W - 4; x > 4; x--) if (clearCol(x) && nDTraps < 12) { dg[GY - 1][x] = 'y'; dTraps[nDTraps].tx = x; dTraps[nDTraps].t = 0; dTraps[nDTraps].st = 0; nDTraps++; break; }
  for (int k = 0; k < (var >= 4 ? 2 : 1); k++) {
    int want = (tower ? gEnd : W) * (k == 0 ? 1 : 1) / (k == 0 ? 2 : 4);
    for (int d = 0; d < W; d++) { int x = want + ((d & 1) ? -(d + 1) / 2 : d / 2); if (clearCol(x) && dg[GY - 1][x] == '.' && nDr < 4) { dDr[nDr] = { x, 0, 0, 0, 0 }; nDr++; break; } }
  }
  if (!tower) {
    wput(W - 2, GY - 1, 'G');
    for (int k = 1; k <= 3; k++) star(W * k / 4, GY - 3);
  } else {                                                         // tour : plateformes en zigzag, +3 rangees a chaque fois, drapeau en haut
    star(max(3, gEnd / 2), GY - 3);
    int tl = W - tw, tr = W - 2, px = tl, dir = 1, row = GY, n = 0, nTot = (GY - 4) / 3;
    while (row - 3 >= 4) {
      row -= 3; n++;
      for (int j = 0; j < 3; j++) wput(px + j, row, 'z');   // plateformes glitch (pas de plateformes fixes pour l'instant)
      if (n == nTot / 2) star(px + 1, row - 2);
      if (row - 3 < 4) { wput(px + 1, row - 1, 'G'); star(px + 2, row - 3); break; }
      if (var >= 5 && n % 3 == 2 && (dTheme != 4 || var >= 9)) wguy(px + 1, row - 1);
      if (dir > 0) { px += 4; if (px + 2 > tr) { dir = -1; px -= 8; } } else { px -= 4; if (px < tl) { dir = 1; px += 8; } }
    }
    for (int k = 0; stars < 3 && k < 6; k++) star(tl + k * 2, GY - 3);
  }
  dChkX = 2 * TILE + 2; dChkY = GY * TILE - (int)PH;
  setRates(dTheme, var);
}
// ---- niveaux de transition (10, 20, 30, 40) : moitie gauche = monde actuel, moitie droite = monde suivant.
// Une couture glitchee les separe ; en la franchissant, le jeu "se recompile" (rafale de glitch) et passe au monde suivant.
void genTransition(int a, int b) {
  bool corr = a == 4 || a == 3;                      // exterieur -> donjon, donjon -> glace : construits comme les couloirs (64 de large)
  if (corr) dWW = 64;
  const int mid = dWW / 2;
  static DDrop bd[4]; int nb = 0;
  static char tb[DHMAX][DW]; static DEn be[16]; static DVine bv[12]; static DTrap btr[12]; static DSrc bs[24]; static DTree bt[8];
  int ne = 0, nv = 0, ntr = 0, ns = 0, nt = 0;
  int sb = dBuild; dBuild = 10; dTheme = b;
  if (corr) genCorr(3); else genProc(b, 0);
  for (int r = 0; r < DH; r++) for (int c = mid; c < DW; c++) tb[r][c] = dg[r][c];
  for (int i = 0; i < nDr; i++) if (dDr[i].tx >= mid) bd[nb++] = dDr[i];
  for (int i = 0; i < nDEn; i++) if (den[i].x >= mid * TILE) be[ne++] = den[i];
  for (int i = 0; i < nDVines; i++) if (dVines[i].ax >= mid * TILE) bv[nv++] = dVines[i];
  for (int i = 0; i < nDTraps; i++) if (dTraps[i].tx >= mid) btr[ntr++] = dTraps[i];
  for (int i = 0; i < nDSrc; i++) if (dSrc[i].x >= mid * TILE) bs[ns++] = dSrc[i];
  for (int i = 0; i < nDTrees; i++) if (dTrees[i].x >= mid * TILE) bt[nt++] = dTrees[i];
  dBuild = sb; dTheme = a;
  if (corr) { genCorr(9); nPatch = 0; } else genProc(a, 9);
  int k = 0;
  for (int i = 0; i < nDr; i++) if (dDr[i].tx < mid) dDr[k++] = dDr[i];
  nDr = k; for (int i = 0; i < nb && nDr < 4; i++) dDr[nDr++] = bd[i];
  for (int i = 0; i < nDEn; i++) if (den[i].x < mid * TILE) den[k++] = den[i];
  nDEn = k; for (int i = 0; i < ne && nDEn < 16; i++) den[nDEn++] = be[i];
  k = 0; for (int i = 0; i < nDVines; i++) if (dVines[i].ax < mid * TILE) dVines[k++] = dVines[i];
  nDVines = k; for (int i = 0; i < nv && nDVines < 12; i++) dVines[nDVines++] = bv[i];
  k = 0; for (int i = 0; i < nDTraps; i++) if (dTraps[i].tx < mid) dTraps[k++] = dTraps[i];
  nDTraps = k; for (int i = 0; i < ntr && nDTraps < 12; i++) dTraps[nDTraps++] = btr[i];
  k = 0; for (int i = 0; i < nDSrc; i++) if (dSrc[i].x < mid * TILE) dSrc[k++] = dSrc[i];
  nDSrc = k; for (int i = 0; i < ns && nDSrc < 24; i++) dSrc[nDSrc++] = bs[i];
  k = 0; for (int i = 0; i < nDTrees; i++) if (dTrees[i].x < mid * TILE) dTrees[k++] = dTrees[i];
  nDTrees = k; for (int i = 0; i < nt && nDTrees < 8; i++) dTrees[nDTrees++] = bt[i];
  for (int r = 0; r < DH; r++) for (int c = mid; c < DW; c++) dg[r][c] = tb[r][c];
  for (int c = mid - 3; c <= mid + 3; c++) for (int r = 0; r < DH; r++) dg[r][c] = r >= DGY ? 'S' : '.';   // couture : sol plat, rien au-dessus
  wput(mid + 3, DGY - 1, 'P');
  tCol = mid; tA = a; tB = b;
}
float dFireHeat() {                                                // donjon : proximite de la lave et des torches, 0..1
  float best = 0, cxp = dpx + PW / 2, cyp = dpy + PH / 2;
  int pc = (int)floor(cxp / TILE);
  for (int c = max(0, pc - 5); c <= min(DW - 1, pc + 5); c++) if (dg[DGY][c] == 'L') {
    float dx = fabsf(cxp - (c * TILE + 8)) - 8, dy = (DGY * TILE) - (dpy + PH);
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    float f = 1.0f - sqrtf(dx * dx + dy * dy) / 56.0f;
    if (f > best) best = f;
  }
  for (int i = 0; i < nDSrc; i++) {
    float dx = cxp - dSrc[i].x, dy = cyp - dSrc[i].y, f = 0.8f * (1.0f - sqrtf(dx * dx + dy * dy) / dSrc[i].r);
    if (f > best) best = f;
  }
  return best;
}
void gClose(int i) {                                 // remet le sol / le vide d'un glitch du sol
  GHole &h = gH[i];
  if (h.up) { if (h.open) for (int r = DGY - 2; r < DGY; r++) if (dg[r][h.c] == 'Z') dg[r][h.c] = '.'; }
  else for (int r = DGY; r < DH; r++) dg[r][h.c] = 'S';
  h.on = false;
}
void dRespawn() {
  dpx = dChkX; dpy = dChkY; dvx = dvy = 0; dHeat = 0; flying = false; airJumped = false; flyTap = 0; dGround = false; dCoyote = dJumpBuf = 0; dVineIdx = -1; dVineLast = -1; dVineCd = 0.3f; dQs = 0;
  for (int i = 0; i < 8; i++) dpr[i].on = false;
  for (int i = 0; i < nDTraps; i++) { dTraps[i].st = 0; dTraps[i].t = 0; }
  for (int i = 0; i < nDEn; i++) den[i].cd = 1.0f + 0.3f * i;
  for (int i = 0; i < 4; i++) if (gH[i].on) gClose(i);   // glitchs du sol refermes
  if (curLevel == CUSTOM_IDX) bqTrousReset();
  for (int i = 0; i < nDr; i++) { if (dDr[i].st == 2) dg[dDr[i].row][dDr[i].tx] = '.'; dDr[i].st = 0; }   // briques remises en place
  if (tCol >= 0) { int sd = dChkX + PW / 2 >= tCol * TILE ? tB : tA; if (dTheme != sd) { dTheme = sd; setRates(sd, sd == tB ? 0 : 9); } }   // transition : monde du point de reprise
  gEvT = 2.5f;
  dcamX = max(0.0f, min(dpx - DVW / 2, (float)(dWW * TILE - DVW)));
  dcamY = max(0.0f, min(dpy - DVH * 6 / 10, (float)((DGY + 1) * TILE - DVH)));   // plafond : une seule rangee de sol visible sous les pieds
}
void dBuildSprites(int theme);
void startBqCote() {                                 // BRIQUE : le jeu de cote de l'enfant dans le moteur des mondes (exterieur, sans glitchs ni histoire)
  curLevel = CUSTOM_IDX; dTheme = 4; dVar = 9; dSpd = 0.6f * bqVit; dBuild = 10;
  dVW = 320; dVH = 240; dSH = 120;
  dWW = bqCW; dGY = bqCH - 1; dH = bqCH + 3; tCol = -1; nPatch = 0; tA = tB = dTheme; dCorr = false;
  wclear(); nBqT = 0;
  for (int r = 0; r < bqCH; r++) for (int c = 0; c < dWW; c++) {
    char ch = bqCRows[r][c], o = '.';
    if (ch == '#') o = 'S';
    else if (ch == 'T') { o = 'S'; if (nBqT < 16) bqT[nBqT++] = { c, r, false }; }
    else if (ch == '^') o = 'c';
    else if (ch == 'H') {                            // pic cache : surgit du sol quand on approche (piege des mondes) ; ailleurs qu'au sol = pic fixe
      if (r == bqCH - 2 && nDTraps < 12) { o = 'y'; dTraps[nDTraps].tx = c; dTraps[nDTraps].t = 0; dTraps[nDTraps].st = 0; nDTraps++; } else o = 'c';
    }
    else if (ch == '*' || ch == 'G') o = ch;
    else if (ch == 'E') wguy(c, r);
    else if (ch == 'S') { dChkX = c * TILE + 2; dChkY = (r + 1) * TILE - (int)PH; }
    dg[r][c] = o;
  }
  for (int r = bqCH; r < dH; r++) for (int c = 0; c < dWW; c++) dg[r][c] = dg[bqCH - 1][c] == 'S' ? 'S' : '.';   // sous le sol : plein, ou vide sous un trou
  dSX = dChkX; dSY = dChkY;
  setRates(dTheme, dVar);
  worldRun();
  gZone0 = dWW + 10;                                 // pas de zone corrompue
  dPoleHpx = 2 * TILE;                               // une porte, pas un mat
  strcpy(gStartTxt, "Trouve la porte !");
  if (bqVies) dLives = bqVies - 1;
}
void bqTrous() {                                     // BRIQUE : un bloc 'T' disparait quand on arrive tout pres (au dernier moment)
  for (int i = 0; i < nBqT; i++) {
    BqTrou &t = bqT[i]; if (t.open) continue;
    if (fabsf(dpx + PW / 2 - (t.c * TILE + 8)) > 34 || fabsf(dpy + PH - t.r * TILE) > 48) continue;
    t.open = true;
    for (int r = t.r; r < (t.r == bqCH - 1 ? DH : t.r + 1); r++) dg[r][t.c] = '.';   // dans le sol : on tombe jusqu'en bas
    dPuff(t.c * TILE + 8, t.r * TILE + 4, RGB565(150, 140, 130), 6);
  }
}
void bqTrousReset() { for (int i = 0; i < nBqT; i++) if (bqT[i].open) { bqT[i].open = false; for (int r = bqT[i].r; r < (bqT[i].r == bqCH - 1 ? DH : bqT[i].r + 1); r++) dg[r][bqT[i].c] = 'S'; } }
void genWorld(int idx) {                             // genere le niveau idx (0..49) dans dg : jeu et miniature du menu
  dTheme = idx < 10 ? 4 : idx < 20 ? 3 : idx < 30 ? 2 : idx < 40 ? 0 : 1; dVar = idx % 10;
  dSpd = idx < 29 ? 0.6f : 1.0f;                     // niveaux 1-29 : vitesse reduite a 60 %
  bool far = idx < 20;                               // niveaux 1-20 : camera plus eloignee (pas de zoom)
  dVW = far ? 320 : 256; dVH = far ? 240 : 192; dSH = far ? 120 : 96;
  dBuild = dTheme == 4 ? dVar : 10;
  dWW = DW; dGY = 14; dH = 18; tCol = -1; nPatch = 0; tA = tB = dTheme;
  dCorr = (dTheme == 4 || dTheme == 3) && dVar < 9;
  if (dCorr) { int W, H; corrSize(dTheme == 3 ? dVar + 3 : dVar, W, H); dWW = W; dGY = H - 1; dH = H + 3; genCorr(dVar); }
  else if (dVar == 9 && idx < 49) genTransition(dTheme, idx < 19 ? 3 : idx < 29 ? 2 : idx < 39 ? 0 : 1);
  else genProc(dTheme, dVar);
  auto placeU = [&](int want) {                      // piece bleue (question) : en hauteur, il faut sauter pour l'avoir
    for (int d = 0; d < dWW; d++) {
      int x = want + ((d & 1) ? -(d + 1) / 2 : d / 2); if (x < 3 || x > dWW - 4) continue;
      bool ok = dg[DGY][x] == 'S'; for (int r = DGY - 5; r < DGY && ok; r++) if (dg[r][x] != '.') ok = false;
      if (ok) { dg[DGY - 4][x] = 'U'; return; }
    }
  };
  placeU(dWW * 2 / 5); if (dWW >= 40) placeU(dWW * 4 / 5);
  auto placeH = [&](int want) {                      // coeur : rend les vies (jusqu'a 2), en l'air au-dessus du sol plat
    for (int d = 0; d < dWW; d++) {
      int x = want + ((d & 1) ? -(d + 1) / 2 : d / 2); if (x < 6 || x > dWW - 6) continue;
      bool ok = dg[DGY][x] == 'S'; for (int r = DGY - 4; r < DGY && ok; r++) if (dg[r][x] != '.') ok = false;
      if (ok) { dg[DGY - 3][x] = 'H'; return; }
    }
  };
  placeH(dWW * 3 / 5); if (dWW >= 50) placeH(dWW / 5); if (dWW >= 90) placeH(dWW * 9 / 10);
  { int k = 0; for (int i = 0; i < nDEn; i++) if (i % 5 != 1 && i % 5 != 3) den[k++] = den[i]; nDEn = k; }   // 40 % d'ennemis fixes en moins (2 sur 5)
  dSX = dChkX; dSY = dChkY;
}
void startWorld(int idx) {                           // idx = niveau 0..49 : 1-10 exterieur, 11-20 donjon de feu, 21-30 glace, 31-40 desert, 41-50 jungle
  curLevel = idx; genWorld(idx); worldRun();
}
void worldRun() {                                    // lance le monde deja genere dans dg
  dFlX = dFlY = -1; for (int r = 0; r < DH; r++) for (int c = 0; c < dWW; c++) if (dg[r][c] == 'G') { dFlX = c; dFlY = r; }
  { int n = 0; while (dFlX >= 0 && n < 4 && dFlY - n - 1 >= 0 && !dsolid(dFlX, dFlY - n - 1)) n++; dPoleHpx = (n + 1) * TILE; }   // le mat monte de 5 cases (moins s'il y a un plafond)
  dSlide = false; dPopT = 0;
  scFlag = 0; deaths = 0; levelCoins = 0; gTransT = 0; gAvT = 2.8f;
  randomSeed(micros());                              // le niveau est fixe, les glitchs sont imprevisibles
  gZone0 = dWW - dWW * (6 + glitchLvl * 15 / 100) / 96;   // zone corrompue : ~16 % du niveau (glitchs a 60 %) gSayT = 0; gSayCd = 3.0f; gSayE = -1;
  { int k = random(0, 5);
    if (dTheme == 4 && dVar == 0) strcpy(gStartTxt, "Le jeu commence ici. Avance !");
    else if (tCol >= 0) strcpy(gStartTxt, "Au bout : le monde suivant. Attention !");
    else if (k == 0) snprintf(gStartTxt, sizeof(gStartTxt), "Version %s : rendons-la parfaite !", FW_VERSION);
    else if (k == 1) strcpy(gStartTxt, "Chaque niveau fini = moins de bugs.");
    else if (k == 2) strcpy(gStartTxt, "Des glitchs ! Ne les laisse pas gagner.");
    else if (k == 3) snprintf(gStartTxt, sizeof(gStartTxt), "Glitchs a %d%%. On nettoie !", glitchLvl);
    else strcpy(gStartTxt, "Allez, on code la suite du jeu !"); }
  if (dTheme == 0 || (tCol >= 0 && tB == 0)) { dBuildShadow(); dBuildDunes(); }
  if (tMix) { dBuildSprites(tB); memcpy(dTileSprB, dTileSpr, sizeof(dTileSprB)); }
  dLives = 2; dInvT = 0; nSafe = 0; iSafe = 0; dSafeT = 0;
  dBuildSprites(dTheme); dRespawn(); dFacingR = true; dPhase = 0; dDeaths = 0; dCoinsGot = 0; dStars = 0; dWinFlag = false;
  dStartMs = millis(); levelStartMs = dStartMs;
  for (int i = 0; i < 16; i++) dpt[i].life = 0;
  stopEspNow();                                      // solo : libere la memoire du wifi pour le tampon DMA
  dDma = dSpiStart();                                // envoi des images en DMA 80 MHz ; sinon SPI Arduino a 80 MHz
  if (!dDma) tft.setSPISpeed(80000000);
  state = ST_DESERT;
}
// Les rayons du soleil arrivent en biais depuis le haut a droite (ils descendent vers la gauche : x = u - 0.7 y).
// Un point est a l'ombre si un obstacle (bloc, caisse, cactus, houppier d'acacia) coupe son rayon. Le decor ne bouge pas :
// la table des ombres est calculee une seule fois par niveau (entiers seulement : pas de FPU sur l'ESP32-C3).
bool dCactusHit(char ch, int tx, int row, int x, int y) {
  int bx0, bw, bh;
  if (ch == 'k') { bx0 = 3; bw = 10; bh = 28; } else if (ch == 'c') { bx0 = 2; bw = 12; bh = 9; } else if (ch == 'm') { bx0 = 1; bw = 14; bh = 24; } else return false;
  return x >= tx * TILE + bx0 && x < tx * TILE + bx0 + bw && y >= (row + 1) * TILE - bh && y < (row + 1) * TILE;
}
bool dOccI(int x, int y) {
  int tx = x >> 4, ty = y >> 4;
  if (x >= 0 && tx < DW && ty >= 0 && ty < DH) {
    if (dsolid(tx, ty)) return true;
    for (int r = ty; r <= ty + 1 && r < DH; r++) if (dCactusHit(dg[r][tx], tx, r, x, y)) return true;
  }
  for (int i = 0; i < nDTrees; i++) if (abs(x - dTrees[i].x) < 42 && y > dTrees[i].y - 100 && y < dTrees[i].y - 56) return true;
  return false;
}
void dBuildShadow() {
  for (int i = 0; i < 880; i++) {
    int u = 2 * i; uint16_t ys = 30000;
    for (int y = 0; y < DH * TILE; y += 4) {
      int x = u - y * 7 / 10;
      if (x < 0) break;
      if (x < DW * TILE && dOccI(x, y)) { ys = y; break; }
    }
    dysW[i] = ys;
  }
}
bool dShadeAt(int x, int y) { int u = x + y * 7 / 10; return u >= 0 && (u >> 1) < 880 && y > (int)dysW[u >> 1]; }
void dBuildDunes() { for (int i = 0; i < 760; i++) { float ph = i * 0.021f; dDune[i] = (int8_t)(16 * sinf(ph) + 9 * sinf(ph * 2.3f + 1)); } }
// zone dangereuse d'un cactus / d'une epine : rect (x, y, w, h) en pixels monde ; false si la tuile n'en est pas un
bool dCactusBox(char ch, int tx, int ty, float &bx, float &by, float &bw, float &bh) {
  if (ch == 'k') { bx = tx * TILE + 3;  bw = 10; bh = 28; }
  else if (ch == 'c') { bx = tx * TILE + 2;  bw = 12; bh = 9; }
  else if (ch == 'm') { bx = tx * TILE + 1;  bw = 14; bh = 24; }
  else if (ch == 'x') { bx = tx * TILE + 3;  bw = 10; bh = 12; }   // pic cache
  else return false;
  by = (ty + 1) * TILE - bh;
  return true;
}
void glitchSave() { story.putUChar("gl", (uint8_t)glitchLvl); }
void dDie(const char* msg, bool heat) {
  if (dInvT > 0 && dpy < DH * TILE) return;          // invincible juste apres une reapparition (sauf chute dans le vide)
  if (glitchLvl < 100 && curLevel != CUSTOM_IDX) glitchLvl++;   // chaque mort nourrit les glitchs
  dDeathMsg = msg; dPhase = 2; dPhaseT = heat ? 1.1f : 0.6f; dDeaths++; dHeat = heat ? 100 : dHeat; dvx = dvy = 0; dVineIdx = -1;
  dPuff(dpx + 6, dpy + 7, dTheme == 0 ? RGB565(120, 190, 255) : RGB565(255, 120, 150), 8);
}
void worldQuit() { if (curLevel == CUSTOM_IDX) { dSpiStop(); tft.setSPISpeed(40000000); checkpointFinish(); return; } glitchSave(); dSpiStop(); tft.setSPISpeed(40000000); selLevel = curLevel; state = ST_SELECT; drawSelect(); }
void worldWin() {                                    // drapeau atteint : meme bilan que les niveaux classiques (score, etoiles, recap, menu de fin)
  deaths = dDeaths; levelCoins = dCoinsGot; levelStartMs = dStartMs;
  if (curLevel == CUSTOM_IDX) {                     // BRIQUE : question de cours avant la victoire
    if (bqNQ) { dSpiStop(); tft.setSPISpeed(40000000); bqStartQuiz(dStars); return; }
  } else { glitchLvl = max(0, glitchLvl - 4); glitchSave(); }   // un niveau fini = des bugs corriges
  finishLevel(dStars);
}
static inline uint32_t gHash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
void gGlitchLine(char *b, int n) {                   // ce que disent les glitchs : ils veulent que le jeu reste mauvais
  static const char *L[] = { "", "Ce jeu ne sortira jamais.", "On corrompt ta sauvegarde...", "Retourne en v0.1 !", "",
                             "", "Ton dev a abandonne.", "Personne ne jouera a ca.", "Encore une version ratee.", "ERREUR : joueur inutile", "La prochaine version ? Jamais." };
  int k = random(0, 11);
  if (k == 0) snprintf(b, n, "%s ? Deja obsolete.", FW_VERSION);
  else if (k == 4) snprintf(b, n, "Corruption : %d%%", glitchLvl);
  else if (k == 5) snprintf(b, n, "Compile le %s ? Perime.", __DATE__);
  else { strncpy(b, L[k], n - 1); b[n - 1] = 0; }
}
void gEvents(float dt) {                             // glitchs aleatoires : couloir 1-10 partout, ailleurs dans la zone corrompue
  for (int i = 0; i < 4; i++) if (gH[i].on) {        // glitchs du sol : clignotent, s'ouvrent, se referment (jamais sur le joueur)
    GHole &h = gH[i]; h.t += dt;
    bool overC = dpx + PW > h.c * TILE && dpx < (h.c + 1) * TILE;
    if (!h.open && h.t >= 0.7f) {
      if (!h.up) { h.open = true; for (int r = DGY; r < DH; r++) dg[r][h.c] = 'Q'; }
      else if (!(overC && dpy + PH > (DGY - 2) * TILE)) { h.open = true; dg[DGY - 1][h.c] = 'Z'; if (dVar >= 4 || dTheme != 4) dg[DGY - 2][h.c] = 'Z'; }
    }
    if (h.t >= 3.0f && (h.up || !(overC && dpy + PH > DGY * TILE))) gClose(i);
  }
  bool inZone = dpx / TILE >= gZone0;
  if (!dCorr && !inZone) return;
  gEvT -= dt;
  if (gEvT > 0) return;
  float base = (dCorr ? 5.0f - dVar * 0.25f : 6.0f) * (1.3f - glitchLvl * 0.006f) * (inZone ? 0.6f : 1.0f);
  gEvT = base + random(0, 100) / 50.0f;
  int c = (int)((dpx + PW / 2 + (dFacingR ? 1 : -1) * (100 + random(0, 60))) / TILE);
  c = constrain(c, 2, dWW - 3);
  if (abs(c * TILE + 8 - (int)(dpx + PW / 2)) < 40) return;   // jamais sur le joueur
  int kind = random(0, 4);                           // des le niveau 1 : bloc qui tombe, glitch qui apparait, trou, bloc qui surgit
  if (kind == 1 && dTheme == 4 && dVar < 5) kind = 0;   // les glitchs ennemis n'apparaissent qu'a partir du niveau 6
  if (kind == 1 && random(0, 5) < 2) return;              // 40 % de glitchs ennemis aleatoires en moins
  if (kind == 0) {                                   // bloc corrompu qui tombe du haut de l'ecran
    float y0 = dcamY - 6;                            // sous le plafond du couloir s'il y en a un
    for (int r = max(0, (int)(dcamY / TILE)); r < DGY - 1; r++) if (dsolid(c, r) && !dsolid(c, r + 1)) { y0 = (r + 1) * TILE + 4; break; }
    for (int k = 0; k < 8; k++) if (!dpr[k].on) { dpr[k] = { (float)(c * TILE + 8), y0, 0, 0, 3.0f, true, true }; break; }
  } else if (kind == 1) {                            // un glitch se materialise au sol
    for (int r = DH - 1; r > 0; r--) if (dsolid(c, r) && !dsolid(c, r - 1)) {
      if (dg[r - 1][c] != '.') break;
      int i = nDEn < 16 ? nDEn++ : -1;
      if (i < 0) for (int j = 0; j < 16; j++) if (!den[j].alive) { i = j; break; }
      if (i >= 0) { den[i] = { (float)(c * TILE + 8), (float)(r * TILE), 1.2f, true, false, true }; dPuff(c * TILE + 8, r * TILE - 8, RGB565(255, 40, 200), 6); }
      break;
    }
  } else if (dg[DGY][c - 1] == 'S' && dg[DGY][c] == 'S' && dg[DGY][c + 1] == 'S' && dg[DGY - 1][c] == '.' && dg[DGY - 2][c] == '.') {   // le sol se de-compile / un bloc surgit
    bool full = true; for (int r = DGY; r < DH; r++) if (dg[r][c] != 'S') full = false;
    for (int i = 0; i < 4 && full; i++) if (!gH[i].on) { gH[i] = { c, 0, true, false, kind == 3 }; break; }
  }
}
void worldQuestion() {                               // tire une question (meme choix adaptatif que le quiz) et met le jeu en pause
  quizPick = quizPendingRetry >= 0 ? quizPendingRetry : pickAdaptive(); quizPendingRetry = -1;
  quizLastPick = quizPick; quizSeen[quizPick >> 3] |= 1 << (quizPick & 7);
  int rot; do { rot = random(0, 3); } while (rot == quizLastRot[quizPick]);
  quizLastRot[quizPick] = rot; quizRot = rot; quizChoice = 0;
  dQ = true; dQPh = 0; dQMs = millis(); dvx = 0;
  dQW = random(1, 6);                                // un des 5 sorciers vient poser la question
  dQX = dpx + PW / 2 + (dFacingR ? 46 : -46); dQY = dpy + PH;
  if (dQX < 24) dQX = dpx + PW / 2 + 46; if (dQX > dWW * TILE - 24) dQX = dpx + PW / 2 - 46;
  dQvw = dVW; dQvh = dVH; dQsh = dSH; dQcx = dcamX; dQcy = dcamY;   // zoom sur les deux personnages : vue 200x150 agrandie x1,6
  dVW = 200; dVH = 150; dSH = 75;
  dcamX = max(0.0f, min((dpx + PW / 2 + dQX) / 2 - DVW / 2, (float)(dWW * TILE - DVW)));
  dcamY = max(0.0f, min(dQY - (DVH - 12), (float)((DGY + 1) * TILE - DVH)));
}
void worldAnswer() {                                 // meme bilan que le quiz ; bonne reponse = l'avatar evolue
  int oi = (quizChoice + quizRot) % 3, cat = QUIZZES[quizPick].cat;
  quizOk = oi == QUIZZES[quizPick].correct;
  if (quizOk) quizDone[quizPick >> 3] |= 1 << (quizPick & 7);
  quizAskedByCat[cat]++;
  quizSkill[cat] = quizOk ? min(5.0f, quizSkill[cat] + 0.4f) : max(1.0f, quizSkill[cat] - 0.6f);
  if (quizOk) { quizCorrectByCat[cat]++; quizStreak++; } else { quizStreak = 0; quizPendingRetry = quizPick; }
  quizGain = quizOk ? 10 * QUIZZES[quizPick].diff + min(quizStreak - 1, 5) * 2 : 0;
  knowledge += quizGain;
  evoUp = false;
  if (quizOk) {
    coins += 3; prefs.putUInt("coins", coins);
    if (evo < EVO_MAX && ++evoPts >= EVO_STEP) { evo++; evoPts = 0; evoUp = true; story.putUChar("evo", (uint8_t)evo); }
    story.putUChar("evp", (uint8_t)evoPts);
  }
  saveQuizProg(); saveReward();
  dQPh = 1; dQAMs = millis();
}
void desertSim(float dt, bool bl, bool br, bool bj, bool je) {   // un pas de simulation (dt <= 20 ms)
  static float quitT = 0;
  if (bl && br) quitT += dt; else quitT = 0;           // P2 + P1 maintenus ~1 s = quitter
  if (quitT > 1.0f) { quitT = 0; worldQuit(); return; }
  for (int i = 0; i < 16; i++) if (dpt[i].life > 0) { DPt &q = dpt[i]; q.life -= dt; q.x += q.vx * dt; q.y += q.vy * dt; q.vy += 300 * dt; }
  if ((trailSel > 0 || evo >= 10) && dPhase == 0 && (fabsf(dvx) > 20 || !dGround)) {   // traînee (boutique, ou gagnee par l'evolution)
    static float trT = 0; trT -= dt;
    if (trT <= 0) {
      trT = 0.035f;
      static const uint16_t TMC[4] = { RGB565(255, 150, 50), RGB565(255, 60, 200), RGB565(60, 230, 255), RGB565(255, 230, 70) };
      uint16_t tc = trailSel == 0 ? (evo >= 11 ? TMC[(millis() / 70) & 3] : TMC[0]) : trailSel == NTRAIL - 1 ? skinColor(7) : TRAIL_COL[trailSel];
      for (int i = 0; i < 16; i++) if (dpt[i].life <= 0) { dpt[i] = { dpx + PW / 2 - 2, dpy + PH - 5, -dvx * 0.15f, (float)random(-25, 6), 0.45f, tc, 5 }; break; }   // traînee : gros points qui retrecissent
    }
  }
  if (gSayCd > 0) gSayCd -= dt;
  if (gSayT > 0) gSayT -= dt;
  if (gAvT > 0) gAvT -= dt;
  if (gTransT > 0) gTransT -= dt;
  if (dPopT > 0) dPopT -= dt;
  if (tCol >= 0 && dPhase == 0) {                     // transition : on passe la couture -> le jeu se recompile dans l'autre monde
    int side = dpx + PW / 2 >= tCol * TILE ? tB : tA;
    if (side != dTheme) {
      dTheme = side; setRates(side, side == tB ? 0 : 9); dHeat = 0; gTransT = 0.8f; gAvT = 2.2f;
      strcpy(gStartTxt, side == tB ? "Le jeu se recompile... Nouveau monde !" : "Retour en arriere ?!");
    }
  }
  if (dPhase == 2) {                                 // extinction / chute : courte pause puis retour au panneau
    dPhaseT -= dt;
    if (dPhaseT <= 0) {
      dPhase = 0;
      if (dLives > 0 && nSafe > 0) {                 // une vie en moins : on repart la ou on vient de perdre (derniere position sure au sol)
        dLives--; int k = (iSafe + 7) % 8; dChkX = (int)dSafeX[k]; dChkY = (int)dSafeY[k];
        dRespawn(); dInvT = 1.2f;
      } else {                                       // plus de vies : on recommence le niveau depuis le debut (ennemis remis, glitchs effaces)
        dLives = 2; dChkX = dSX; dChkY = dSY;
        for (int i = 0; i < nDEn; i++) den[i].alive = !den[i].gl;
        dRespawn();
      }
      nSafe = 0;
    }
    return;
  }
  if (dSlide) {                                        // on a attrape le mat : glissade jusqu'en bas, puis fin du niveau
    float base = (dFlY + 1) * TILE; dvx = dvy = 0; dGround = false; dpy += 110 * dt;
    if (dpy + PH >= base) { dpy = base - PH; dSlide = false; dWinFlag = true; }
    return;
  }
  if (dVineCd > 0) dVineCd -= dt;
  bool swinging = dVineIdx >= 0;
  if (swinging) {                                    // accroche a une liane : pendule
    DVine &v = dVines[dVineIdx];
    int dir = (br ? 1 : 0) - (bl ? 1 : 0);
    if (dir) dFacingR = dir > 0;
    if (je) {                                        // saut = on lache : on part dans le sens du mouvement, avec un petit coup de pouce vers le haut
      dvx = dVineL * dOmega * cosf(dTheta) * 1.1f; dvy = -dVineL * dOmega * sinf(dTheta) - 160.0f;
      dVineLast = dVineIdx; dVineIdx = -1; dVineCd = 0.35f; dGround = false; dCoyote = 0; swinging = false;
    } else {
      dOmega += (-(900.0f / dVineL) * sinf(dTheta) + dir * 4.5f) * dt;   // pesanteur du pendule + on se balance en appuyant dans le sens du mouvement
      dOmega *= 1.0f - 0.2f * dt;
      dTheta += dOmega * dt;
      if (dTheta > 1.4f) { dTheta = 1.4f; if (dOmega > 0) dOmega = -dOmega * 0.4f; }
      if (dTheta < -1.4f) { dTheta = -1.4f; if (dOmega < 0) dOmega = -dOmega * 0.4f; }
      dpx = v.ax + dVineL * sinf(dTheta) - PW / 2; dpy = v.ay + dVineL * cosf(dTheta) - 2;
      dvx = dVineL * dOmega * cosf(dTheta); dvy = -dVineL * dOmega * sinf(dTheta);
      if (dsolid((int)floor((dpx + PW / 2) / TILE), (int)floor((dpy + PH / 2) / TILE))) { dVineLast = dVineIdx; dVineIdx = -1; dVineCd = 0.35f; swinging = false; }   // un bloc sur le trajet : on lache
    }
  }
  if (!swinging) {
    if (dGround) dCoyote = 0.08f; else if (dCoyote > 0) dCoyote -= dt;
    bool qs = false;                                                                  // sable mouvant : on s'enfonce, lent, saut affaibli, englouti si on s'attarde
    { int qc = (int)floor((dpx + PW / 2) / TILE), qr = (int)floor((dpy + PH - 3) / TILE); qs = qc >= 0 && qc < DW && qr >= 0 && qr < DH && dg[qr][qc] == 'q'; }
    if (qs) { dCoyote = 0.08f; dQs += dt; if (dQs > 1.3f && dPhase == 0) { dQs = 0; dDie("Englouti par le sable !", false); return; } }
    else if (dQs > 0) dQs = max(0.0f, dQs - 2 * dt);
    if (je) dJumpBuf = 0.10f; else if (dJumpBuf > 0) dJumpBuf -= dt;
    for (int i = 0; i < AB_N; i++) if (abilT[i] > 0) abilT[i] -= dt;                 // capacites de la boutique (comptees pendant le jeu)
    if (abilT[2] <= 0) flying = false;
    if (flyTap > 0) flyTap -= dt;
    if (dGround) airJumped = false;
    bool touchL = false, touchR = false;                                              // contact avec un mur (saut mural)
    for (int r = (int)floor((dpy + 2) / TILE); r <= (int)floor((dpy + PH - 3) / TILE); r++) {
      if (dsolid((int)floor((dpx - 1) / TILE), r)) touchL = true;
      if (dsolid((int)floor((dpx + PW) / TILE), r)) touchR = true;
    }
    if (dJumpBuf > 0 && dCoyote > 0) { dvy = JUMP_V * (bqFiche && bqOn() ? fSautK : 1); dGround = false; dCoyote = 0; dJumpBuf = 0; }
    else if (je && !dGround && !flying && (touchL || touchR) && (!(bqFiche && bqOn()) || fMural)) { dvy = JUMP_V * (bqFiche && bqOn() ? fSautK : 1); dvx = (touchL ? 1 : -1) * 190.0f * max(dSpd, 0.8f); dFacingR = touchL; dJumpBuf = 0; }   // saut mural
    else if (je && !dGround) {                                                        // en l'air : vol (2 appuis rapproches) ou double saut
      if (flying) flying = false;
      else if (abilT[2] > 0 && flyTap > 0) { flying = true; dvy = 0; }
      else if (abilT[1] > 0 && !airJumped) { dvy = JUMP_V; airJumped = true; }
    }
    if (qs && dvy == JUMP_V) dvy = JUMP_V * 0.85f;
    if (je) flyTap = 0.3f;
    int dir = (br ? 1 : 0) - (bl ? 1 : 0);
    if (dir) dFacingR = dir > 0;
    float target = dir * (RUN + speedBoostLv * 12.0f) * (abilT[0] > 0 ? 1.35f : 1.0f) * dSpd * (qs ? 0.35f : 1.0f), ax = (dGround ? (dTheme == 2 ? 520.0f : 2300.0f) : 1850.0f) * dt;   // glace : on glisse
    if (dvx < target) { dvx += ax; if (dvx > target) dvx = target; }
    else if (dvx > target) { dvx -= ax; if (dvx < target) dvx = target; }
    float g = GRAVITY * (bqFiche && bqOn() ? bqGrav : 1);   // BRIQUE fiche de cote : gravite de la fiche
    if (dvy < 0 && !bj) g *= 1.7f; else if (dvy > 0) g *= 1.12f;
    dvy += g * dt; if (dvy > MAXFALL) dvy = MAXFALL;
    if (qs && dvy > 26) dvy = 26;
    if (!dGround && dvy > 70 && ((touchL && dir < 0) || (touchR && dir > 0))) dvy = 70;   // glissade le long du mur
    if (flying) { if (dGround) flying = false; else dvy = bj ? -170 : 90; }          // vol : saut maintenu = monte, sinon plane
    bool wasG = dGround;
    // horizontal
    float nx = dpx + dvx * dt;
    int t0 = (int)floor(dpy / TILE), t1 = (int)floor((dpy + PH - 1) / TILE);
    if (dvx > 0) { int c = (int)floor((nx + PW - 1) / TILE); for (int r = t0; r <= t1; r++) if (dsolid(c, r)) { nx = c * TILE - PW; dvx = 0; break; } }
    else if (dvx < 0) { int c = (int)floor(nx / TILE); for (int r = t0; r <= t1; r++) if (dsolid(c, r)) { nx = (c + 1) * TILE; dvx = 0; break; } }
    dpx = nx;
    // vertical
    float ny = dpy + dvy * dt;
    int c0 = (int)floor(dpx / TILE), c1 = (int)floor((dpx + PW - 1) / TILE);
    dGround = false;
    if (dvy > 0) { int rb = (int)floor((ny + PH) / TILE); for (int c = c0; c <= c1; c++) if (dsolid(c, rb)) { ny = rb * TILE - PH; dvy = 0; dGround = true; break; } }
    else if (dvy < 0) { int rt = (int)floor(ny / TILE); for (int c = c0; c <= c1; c++) if (dsolid(c, rt)) { ny = (rt + 1) * TILE; dvy = 0; break; } }
    dpy = ny;
    if (dHumpOn && dvy >= 0) {                       // dunes : le sol lisse est au-dessus de la rangee du sol (on y reste colle en descendant)
      float hh = dHumpAt(dpx + PW / 2), sy = DGY * TILE - hh, f = dpy + PH;
      if (hh > 0.5f && f > sy - (wasG ? 7 : 0) && f <= DGY * TILE + 0.5f) { dpy = sy - PH; dvy = 0; dGround = true; }
    }
    if (dTheme == 1 && !dGround) {   // (apres un lacher, seule la liane quittee est bloquee 0,35 s : on peut enchainer la suivante)   // jungle : une liane a portee de main -> on s'accroche
      float hx = dpx + PW / 2, hy = dpy + 2;
      for (int i = 0; i < nDVines; i++) {
        DVine &v = dVines[i];
        if (dVineCd > 0 && (i == dVineLast || dVineLast < 0)) continue;   // meme liane (ou apres une mort) : delai
        if (fabsf(hx - v.ax) < 13 && hy > v.ay + 24 && hy < v.ay + v.len) {
          dVineIdx = i; dVineL = hy - v.ay; dTheta = asinf(constrain((hx - v.ax) / dVineL, -0.9f, 0.9f));
          dOmega = constrain((dvx * cosf(dTheta) - dvy * sinf(dTheta)) / dVineL, -4.0f, 4.0f);
          dPuff(hx, hy, RGB565(90, 190, 90), 4);
          break;
        }
      }
    }
  }
  if (dTheme == 3 && dpy + PH > DGY * TILE + 5) { int lc = (int)floor((dpx + PW / 2) / TILE); if (lc >= 0 && lc < DW && dg[DGY][lc] == 'L') dDie("Aie ! De la lave !", true); }
  if (dpy > DH * TILE + 8) dDie("Aaaah !", false);              // chute dans le vide

  // petits lanceurs : ils visent et lancent un projectile en cloche ; ecrases par le haut = vaincus (+1 piece)
  for (int i = 0; i < nDEn && dPhase == 0; i++) {
    DEn &e = den[i]; if (!e.alive) continue;
    const bool cac = !e.gl && (tMix ? colTheme((int)(e.x0 / TILE)) : dTheme) == 0;   // desert : cactus en boules, ils se dandinent sur place et ne lancent rien
    const float eh = cac ? 24.0f : 17.0f;
    if (cac) e.x = e.x0 + 10 * sinf(millis() * 0.0013f + i * 1.9f);
    float dx = dpx + PW / 2 - e.x, dy = dpy + PH / 2 - (e.y - 8);
    e.right = dx > 0;
    if (e.cd > 0) e.cd -= dt;
    if (!cac && e.cd <= 0 && fabsf(dx) < 150 && fabsf(dy) < 90) {
      for (int k = 0; k < 8; k++) if (!dpr[k].on) {
        const float T = 0.8f;
        dpr[k] = { e.x + (e.right ? 5 : -5), e.y - 17, constrain(dx / T, -170.0f, 170.0f), (dy + 9 - 0.5f * DPG * T * T) / T, 3.0f, true };
        break;
      }
      e.cd = dThrowCd + (millis() % 9) * 0.1f;
    }
    if (dpx + PW > e.x - 6 && dpx < e.x + 6 && dpy + PH > e.y - eh && dpy < e.y) {
      if (dvy > 0 && dpy + PH < e.y - eh + 11 && dVineIdx < 0) {
        e.alive = false; dvy = bj ? -400 : -300; dGround = false;
        snd(SND_COIN); coins++; prefs.putUInt("coins", coins); dCoinsGot++; dPuff(e.x, e.y - 8, RGB565(232, 182, 130), 8);
      } else dDie(e.gl ? "Bzzt ! Un glitch !" : cac ? "Aie ! Ca pique !" : "Oups !", false);
    }
    if (gSayCd <= 0 && fabsf(dx) < 120 && fabsf(dy) < 80) { gSayE = i; gSayT = 2.4f; gSayCd = 6.5f; gGlitchLine(gSayTxt, sizeof(gSayTxt)); }   // le glitch parle
  }
  for (int k = 0; k < 8; k++) {                      // projectiles en vol
    DPr &p = dpr[k]; if (!p.on) continue;
    p.vy += DPG * dt; p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
    if (p.life <= 0 || p.y > DH * TILE || dsolid((int)floor(p.x / TILE), (int)floor(p.y / TILE))) { p.on = false; continue; }
    if (dPhase == 0 && p.x > dpx - 3 && p.x < dpx + PW + 3 && p.y > dpy - 3 && p.y < dpy + PH + 3) { p.on = false; dDie(p.gl ? "Bzzt ! Un glitch !" : "Aie !", false); }
  }

  for (int i = 0; i < nDTraps && dPhase == 0; i++) {   // pics qui surgissent (plaque visible au sol) : montent quand on approche, retombent apres 0,9 s
    DTrap &tr = dTraps[i]; float dxp = fabsf(dpx + PW / 2 - (tr.tx * TILE + 8));
    if (tr.st == 0) { if (tr.t < 0) tr.t += dt; else if (dxp < 30 && dpy + PH > (DGY - 3) * TILE) { tr.t += dt; if (tr.t > 0.22f) { tr.st = 1; tr.t = 0; } } else tr.t = 0; }
    else { tr.t += dt; if (tr.t > 0.9f) { tr.st = 0; tr.t = -0.6f; } else if (dxp < 9 && dpy + PH > DGY * TILE - 14 && dpy < DGY * TILE) dDie(dg[DGY - 1][tr.tx] == 'y' ? "Un pic cache ! Retiens sa place." : "Aie ! Un piege !", false); }
  }
  for (int i = 0; i < nDr && dPhase == 0; i++) {       // briques qui tombent : partent quand on arrive juste avant, se posent et restent
    DDrop &d = dDr[i]; float bx = d.tx * TILE;
    if (d.st == 0) {
      float y0 = dcamY - 16, fall = (DGY - 1) * TILE - y0, lead = max(24.0f, fabsf(dvx) * sqrtf(2 * fall / 1400.0f)), pc = dpx + PW / 2;
      if (pc >= bx + 8 - lead && pc < bx + 8 && dvx >= 0) { d.st = 1; d.y = y0; d.vy = 0; }
    } else if (d.st == 1) {
      d.vy += 1400 * dt; d.y += d.vy * dt;
      int r = (int)floor((d.y + TILE) / TILE);
      if (r < DH && dsolid(d.tx, r) && d.y + TILE >= r * TILE) { d.y = (r - 1) * TILE; d.st = 2; d.row = r - 1; dg[d.row][d.tx] = 'X'; dPuff(bx + 8, d.y + 16, RGB565(150, 140, 130), 6); }
      if (dpx + PW > bx + 1 && dpx < bx + TILE - 1 && dpy + PH > d.y + 2 && dpy < d.y + TILE) dDie("Une brique ! Retiens sa place.", false);
    }
  }
  if (dPhase == 0) { if (curLevel == CUSTOM_IDX) bqTrous(); gEvents(dt); }
  if (dInvT > 0) dInvT -= dt;
  if (dPhase == 0 && dGround && (dSafeT -= dt) <= 0) {   // memorise une position sure : au sol sur un bloc fixe, sans piege a cote
    dSafeT = 0.25f;
    int c = (int)floor((dpx + PW / 2) / TILE), r = (int)floor((dpy + PH + 1 + dHumpAt(dpx + PW / 2)) / TILE);
    bool ok = r < DH && c >= 1 && c < dWW - 1 && (dg[r][c] == 'S' || dg[r][c] == 'X');
    for (int d = -1; d <= 1 && ok; d++) { char q = dg[r - 1][c + d]; if (q == 'k' || q == 'c' || q == 'm' || q == 'v' || q == 'x' || q == 'y' || dg[r][c + d] == 'h') ok = false; }
    if (ok) { dSafeX[iSafe] = dpx; dSafeY[iSafe] = dpy; iSafe = (iSafe + 1) % 8; if (nSafe < 8) nSafe++; }
  }
  // cactus / epines, panneaux (reprise), drapeau, pieces, etoiles
  int c0 = (int)floor(dpx / TILE) - 1, c1 = (int)floor((dpx + PW) / TILE) + 1, t0 = (int)floor(dpy / TILE) - 1, t1 = (int)floor((dpy + PH) / TILE) + 1;
  for (int ty = max(0, t0); ty <= min(DH - 1, t1) && dPhase == 0; ty++) for (int tx = max(0, c0); tx <= min(DW - 1, c1) && dPhase == 0; tx++) {
    char ch = dg[ty][tx]; float bx, by, bw, bh;
    if (dCactusBox(ch, tx, ty, bx, by, bw, bh)) {
      if (dpx + PW - 1 > bx && dpx + 1 < bx + bw && dpy + PH - 1 > by && dpy + 1 < by + bh) dDie(dg[ty][tx] == 'x' ? "Aie ! Un pic cache !" : dTheme == 0 ? "Aie ! Ca pique !" : dTheme == 2 ? "Aie ! Pic de glace !" : dTheme == 3 ? "Aie ! Ca brule !" : "Aie ! Des epines !", false);
    } else if (ch == 'P' || ch == 'W') {
      if (dpx + PW > tx * TILE + 3 && dpx < tx * TILE + 13 && dpy + PH > ty * TILE && dpy < (ty + 1) * TILE) { dChkX = tx * TILE + 2; dChkY = (ty + 1) * TILE - (int)PH; }
    } else if (ch == 'H') {                          // coeur : vies pleines (n'est pris que s'il en manque)
      if (dLives < 2 && fabsf(dpx + PW / 2 - (tx * TILE + 8)) < 11 && fabsf(dpy + PH / 2 - (ty * TILE + 8)) < 12) {
        dg[ty][tx] = '.'; dLives = 2; dPuff(tx * TILE + 8, ty * TILE + 8, RGB565(255, 70, 100), 10); strcpy(gStartTxt, "Vies renouvelees !"); gAvT = 1.8f;
      }
    } else if (ch == 'U') {                          // piece bleue : le Dev apparait et pose une question
      if (fabsf(dpx + PW / 2 - (tx * TILE + 8)) < 10 && fabsf(dpy + PH / 2 - (ty * TILE + 8)) < 11) { dg[ty][tx] = '.'; dPuff(tx * TILE + 8, ty * TILE + 8, RGB565(80, 160, 255), 10); worldQuestion(); }
    } else if (ch == 'K' || ch == 'Y') {             // fausse piece / fausse etoile : un glitch deguise
      if (fabsf(dpx + PW / 2 - (tx * TILE + 8)) < 10 && fabsf(dpy + PH / 2 - (ty * TILE + 8)) < 11) { dg[ty][tx] = '.'; dPuff(tx * TILE + 8, ty * TILE + 8, RGB565(255, 40, 200), 10); dDie(ch == 'K' ? "Fausse piece ! Un glitch..." : "Fausse etoile ! Un piege...", false); }
    } else if (ch == 'O' || ch == '*') {
      if (fabsf(dpx + PW / 2 - (tx * TILE + 8)) < 10 && fabsf(dpy + PH / 2 - (ty * TILE + 8)) < 11) {
        dg[ty][tx] = '.';
        if (ch == 'O') { snd(SND_COIN); coins++; prefs.putUInt("coins", coins); dCoinsGot++; dPuff(tx * TILE + 8, ty * TILE + 8, RGB565(255, 205, 60), 5); }
        else { dStars++; dPuff(tx * TILE + 8, ty * TILE + 8, RGB565(255, 235, 90), 10); }
      }
    }
  }
  if (dFlX >= 0 && !dSlide && dPhase == 0) {            // drapeau facon Mario : plus on attrape le mat haut, plus le bonus est grand (jusqu'a 300 points)
    float base = (dFlY + 1) * TILE;
    if (dpx + PW > dFlX * TILE + 4 && dpx < dFlX * TILE + 12 && dpy + PH > base - dPoleHpx && dpy < base) {
      if (curLevel == CUSTOM_IDX) { dWinFlag = true; dvx = dvy = 0; return; }   // BRIQUE : la porte
      float fr = constrain((base - (dpy + PH / 2)) / (float)dPoleHpx, 0.0f, 1.0f);
      scFlag = (int)(fr * 300); dSlide = true; dvx = dvy = 0; dpx = dFlX * TILE + 8 - PW / 2;
      dPopT = 1.8f; dPopY = dpy; dPopPts = scFlag; dPuff(dFlX * TILE + 8, dpy + 7, RGB565(255, 235, 90), 12);
    }
  }
  // jauge : desert = le soleil chauffe, l'ombre refroidit ; glace = attendre trop longtemps donne froid (feu de camp = au chaud) ; donjon = lave / torches
  if (dPhase == 0) {
    if (dTheme == 0) {
      dShaded = dShadeAt((int)(dpx + PW / 2), (int)(dpy + PH / 2));
      if (dShaded) dHeat = max(0.0f, dHeat - dCoolRate * dt); else dHeat = min(100.0f, dHeat + dHeatRate * dt);
      if (dHeat >= 100) dDie("Trop chaud...", true);
    } else if (dTheme == 2) {
      int pc = (int)floor((dpx + PW / 2) / TILE); dWarm = false;
      for (int cc = max(0, pc - 3); cc <= min(DW - 1, pc + 3); cc++) if (dg[DGY - 1][cc] == 'W') dWarm = true;
      bool still = fabsf(dvx) < 45 && dGround;
      if (dWarm) dHeat = max(0.0f, dHeat - 40 * dt); else if (still) dHeat = min(100.0f, dHeat + dHeatRate * dt); else dHeat = max(0.0f, dHeat - 5 * dt);
      if (dHeat >= 100) dDie("Brrr... trop froid !", true);
    } else if (dTheme == 3) {
      float f = dFireHeat(); dShaded = f <= 0.01f;
      if (f > 0.01f) dHeat = min(100.0f, dHeat + f * dHeatRate * dt); else dHeat = max(0.0f, dHeat - dCoolRate * dt);
      if (dHeat >= 100) dDie("Trop chaud...", true);
    }
  }
}
#ifdef DESERT_BENCH        // mesure des temps (compilation de test seulement : -DDESERT_BENCH)
unsigned long bT[10], bLast, tq; int bN;
#define BT(i, ...) { unsigned long t_ = micros(); __VA_ARGS__; bT[i] += micros() - t_; }
#else
#define BT(i, ...) __VA_ARGS__
#endif
void desertStep(bool bl, bool br, bool bj, bool je) {
  static unsigned long last = 0;
  unsigned long now = millis();
  float dt = (now - last) / 1000.0f; last = now;
  if (dt <= 0 || dt > 0.1f) dt = 0.03f;
  int n = (int)ceilf(dt / 0.02f); float sdt = dt / n;   // la simulation avance en pas de 20 ms max : meme vitesse quelle que soit la cadence d'affichage
  if (dQ) {                                          // question en cours : jeu en pause
    static bool pl, pr; bool el = bl && !pl, er = br && !pr; pl = bl; pr = br;
    if (dQPh == 0) { if (el || er) quizChoice = (quizChoice + (er ? 1 : 2)) % 3; if (je) worldAnswer(); }
    else if (je) { dQ = false; unsigned long pz = millis() - dQMs; dStartMs += pz; levelStartMs += pz; dVW = dQvw; dVH = dQvh; dSH = dQsh; dcamX = dQcx; dcamY = dQcy; }
    drawWorld(); return;
  }
  for (int k = 0; k < n && state == ST_DESERT && !dWinFlag; k++) desertSim(sdt, bl, br, bj, je && k == 0);
  if (state != ST_DESERT) return;
  if (dWinFlag) { dWinFlag = false; worldWin(); return; }
  // camera : suivi lisse, independant de la cadence d'image
  float tx = dpx + PW / 2 - DVW / 2 + (dFacingR ? 14 : -14), ty = dpy + PH / 2 - DVH * 6 / 10, a = 1.0f - expf(-dt * 9.0f);
  dcamX += (tx - dcamX) * a; dcamY += (ty - dcamY) * a;
  dcamX = max(0.0f, min(dcamX, (float)(dWW * TILE - DVW)));
  dcamY = max(0.0f, min(dcamY, (float)((DGY + 1) * TILE - DVH)));
  BT(0, drawWorld());
#ifdef DESERT_BENCH
  bN++;
  if (bLast == 0) bLast = millis();
  if (millis() - bLast > 3000) {
    float el = (millis() - bLast) / 1000.0f;
    Serial.printf("[BENCH] theme=%d dma=%d fps=%.1f frame=%.1fms draw=%lu pass=%lu scale=%lu send=%lu wait=%lu | sky=%lu tiles=%lu decor=%lu ents+hud=%lu heap=%u\n", dTheme, dDma ? 1 : 0, bN / el, 1000.0f * el / bN, bT[0] / bN, bT[2] / bN, bT[3] / bN, bT[4] / bN, bT[5] / bN, bT[6] / bN, bT[7] / bN, bT[8] / bN, bT[9] / bN, (unsigned)ESP.getFreeHeap());
    for (int i = 0; i < 10; i++) bT[i] = 0;
    bN = 0; bLast = millis();
    static int cyc; if (++cyc >= 2) { cyc = 0; Serial.println("[BENCH] sortie + relance (test du retour SPI Arduino)"); worldQuit(); return; }
  }
#endif
}

// ---- dessin : decor en primitives (desert : planche sable/cactus/acacia ; jungle : feuilles, rondins, lianes) ----
void dLeaf(float bx, float by, float tx, float ty, float hw, uint16_t c, uint16_t vein) {   // feuille pointue : base -> pointe, demi-largeur hw
  float dx = tx - bx, dy = ty - by, l = sqrtf(dx * dx + dy * dy); if (l < 1) return;
  float nx = -dy / l * hw, ny = dx / l * hw, mx = bx + dx * 0.45f, my = by + dy * 0.45f;
  fb.fillTriangle((int)bx, (int)by, (int)(mx + nx), (int)(my + ny), (int)tx, (int)ty, c);
  fb.fillTriangle((int)bx, (int)by, (int)(mx - nx), (int)(my - ny), (int)tx, (int)ty, c);
  fb.drawLine((int)bx, (int)by, (int)tx, (int)ty, vein);
}
void dsCactus(int x, int by, char k) {
  const uint16_t G = RGB565(62, 142, 58), D = RGB565(30, 90, 40), L = RGB565(150, 210, 110);
  if (k == 'k') {                                        // grand cactus a deux bras
    fb.fillRoundRect(x + 3, by - 30, 10, 30, 4, G); fb.drawRoundRect(x + 3, by - 30, 10, 30, 4, D);
    fb.fillRoundRect(x - 3, by - 20, 7, 4, 2, G); fb.fillRoundRect(x - 3, by - 27, 4, 11, 2, G); fb.drawRoundRect(x - 3, by - 27, 4, 11, 2, D);
    fb.fillRoundRect(x + 12, by - 16, 7, 4, 2, G); fb.fillRoundRect(x + 15, by - 24, 4, 12, 2, G); fb.drawRoundRect(x + 15, by - 24, 4, 12, 2, D);
    fb.drawPixel(x + 6, by - 24, L); fb.drawPixel(x + 10, by - 16, L); fb.drawPixel(x + 7, by - 9, L);
  } else if (k == 'c') {                                 // petit cactus rond
    fb.fillRoundRect(x + 2, by - 9, 12, 9, 5, G); fb.drawRoundRect(x + 2, by - 9, 12, 9, 5, D);
    fb.drawPixel(x + 5, by - 6, L); fb.drawPixel(x + 10, by - 5, L); fb.drawFastVLine(x + 8, by - 8, 7, D);
  } else {                                               // cactus a deux troncs
    fb.fillRoundRect(x + 1, by - 8, 15, 8, 3, G);
    fb.fillRoundRect(x + 1, by - 22, 6, 22, 3, G); fb.drawRoundRect(x + 1, by - 22, 6, 22, 3, D);
    fb.fillRoundRect(x + 9, by - 26, 7, 26, 3, G); fb.drawRoundRect(x + 9, by - 26, 7, 26, 3, D);
    fb.drawPixel(x + 3, by - 17, L); fb.drawPixel(x + 12, by - 20, L); fb.drawPixel(x + 11, by - 10, L);
  }
}
void dsThorn(int x, int by, char k) {                    // jungle : plantes a epines (memes zones que les cactus)
  const uint16_t G = RGB565(40, 110, 60), D = RGB565(20, 60, 40), T = RGB565(235, 220, 200);
  if (k == 'c') { for (int a = -3; a <= 3; a++) { int tx = x + 8 + a * 3, ty = by - 8 - (3 - abs(a)) * 2; fb.drawLine(x + 8, by - 1, tx, ty, G); fb.drawPixel(tx, ty, T); } fb.fillCircle(x + 8, by - 3, 3, D); }
  else if (k == 'k') { fb.fillRect(x + 7, by - 28, 3, 28, G); for (int i = 0; i < 5; i++) { fb.fillTriangle(x + 7, by - 5 - i * 5, x + 1, by - 8 - i * 5, x + 7, by - 8 - i * 5, G); fb.fillTriangle(x + 10, by - 8 - i * 5, x + 16, by - 11 - i * 5, x + 10, by - 11 - i * 5, G); } fb.fillTriangle(x + 6, by - 28, x + 8, by - 34, x + 10, by - 28, T); }
  else { for (int j = 0; j < 2; j++) { int ox = x + 1 + j * 8; fb.fillRect(ox + 2, by - 22 + j * 4, 3, 22 - j * 4, G); for (int i = 0; i < 4; i++) { fb.fillTriangle(ox + 2, by - 4 - i * 5, ox - 2, by - 6 - i * 5, ox + 2, by - 7 - i * 5, G); fb.fillTriangle(ox + 5, by - 6 - i * 5, ox + 9, by - 8 - i * 5, ox + 5, by - 9 - i * 5, G); } } }
}
void dsTree(int bx, int by) {                            // acacia : large houppier vert, tronc brun
  const uint16_t T1 = RGB565(168, 105, 60), T2 = RGB565(120, 70, 38);
  fb.fillTriangle(bx - 10, by, bx + 10, by, bx, by - 22, T2);
  fb.fillRect(bx - 4, by - 56, 9, 38, T1); fb.drawFastVLine(bx - 4, by - 56, 38, T2); fb.drawFastVLine(bx + 4, by - 56, 38, T2);
  fb.drawLine(bx, by - 44, bx - 22, by - 66, T2); fb.drawLine(bx + 1, by - 44, bx - 21, by - 66, T1);
  fb.drawLine(bx, by - 46, bx + 24, by - 64, T2); fb.drawLine(bx + 1, by - 46, bx + 25, by - 64, T1);
  fb.fillCircle(bx - 32, by - 68, 12, RGB565(125, 150, 48)); fb.fillCircle(bx - 12, by - 78, 15, RGB565(120, 180, 52));
  fb.fillCircle(bx + 12, by - 78, 16, RGB565(160, 185, 58)); fb.fillCircle(bx + 34, by - 66, 13, RGB565(125, 160, 48));
  fb.fillCircle(bx, by - 66, 13, RGB565(100, 155, 46));
  fb.fillCircle(bx - 14, by - 83, 7, RGB565(185, 210, 75)); fb.fillCircle(bx + 14, by - 84, 7, RGB565(195, 215, 80));
}
void dsJTree(int bx, int by) {                           // arbre de la jungle : tronc epais et grandes feuilles
  const uint16_t T1 = RGB565(112, 72, 40), T2 = RGB565(70, 44, 26), V1 = RGB565(50, 150, 70), V2 = RGB565(30, 100, 50), V3 = RGB565(110, 200, 90);
  fb.fillTriangle(bx - 12, by, bx + 12, by, bx, by - 26, T2);
  fb.fillRect(bx - 5, by - 70, 11, 48, T1); fb.drawFastVLine(bx - 5, by - 70, 48, T2); fb.drawFastVLine(bx + 5, by - 70, 48, T2);
  for (int i = 0; i < 4; i++) fb.drawLine(bx - 5, by - 60 + i * 11, bx + 5, by - 54 + i * 11, T2);
  dLeaf(bx, by - 66, bx - 40, by - 84, 9, V2, V3); dLeaf(bx, by - 66, bx + 40, by - 84, 9, V2, V3);
  dLeaf(bx, by - 68, bx - 26, by - 104, 10, V1, V3); dLeaf(bx, by - 68, bx + 26, by - 104, 10, V1, V3);
  dLeaf(bx, by - 70, bx, by - 112, 11, V1, V3);
}
void dsDecor(int x, int by, char ch) {                   // x = bord gauche de la tuile, by = bas de la tuile
  if (dTheme >= 2 && ch != 'P' && ch != 'A' && ch != 'G') { dsDecor2(x, by, ch); return; }
  const uint16_t W1 = RGB565(168, 105, 60), W2 = RGB565(110, 68, 34), SAND = RGB565(240, 224, 184);
  switch (ch) {
    case 'P': fb.fillRect(x + 7, by - 11, 3, 11, W2); fb.fillRect(x + 1, by - 21, 15, 10, W1); fb.drawRect(x + 1, by - 21, 15, 10, W2);
      fb.drawFastHLine(x + 4, by - 17, 9, W2); fb.drawFastHLine(x + 4, by - 14, 9, W2); break;
    case 'A': fb.fillRect(x + 7, by - 20, 3, 20, W2); fb.fillRect(x + 1, by - 21, 11, 7, W1); fb.fillTriangle(x + 12, by - 23, x + 12, by - 12, x + 17, by - 17, W1);
      fb.drawRect(x + 1, by - 21, 11, 7, W2); break;
    case 'r': if (dTheme == 0) { fb.fillRoundRect(x, by - 12, 16, 12, 5, SAND); fb.fillRoundRect(x + 3, by - 12, 8, 6, 3, RGB565(252, 244, 214)); fb.drawLine(x + 9, by - 12, x + 11, by - 1, RGB565(205, 185, 140)); }
      else { fb.fillRoundRect(x, by - 11, 16, 11, 5, RGB565(110, 120, 100)); fb.fillRoundRect(x + 2, by - 11, 9, 5, 3, RGB565(70, 150, 70)); } break;
    case 's': fb.fillRoundRect(x, by - 7, 16, 7, 3, SAND); fb.fillCircle(x + 4, by - 3, 2, RGB565(40, 30, 20)); fb.fillRect(x + 8, by - 8, 6, 3, RGB565(190, 160, 70)); break;
    case 'g': if (dTheme == 0) for (int i = 0; i < 5; i++) fb.drawLine(x + 3 + i * 3, by, x + 1 + i * 3 + (i & 1) * 2, by - 7 + (i % 3), RGB565(190, 190, 70));
      else for (int i = 0; i < 6; i++) fb.drawLine(x + 2 + i * 2, by, x + i * 3 + (i & 1) * 2, by - 8 + (i % 3), RGB565(70, 180, 70)); break;
    case 'u': fb.fillCircle(x + 5, by - 5, 5, RGB565(140, 190, 62)); fb.fillCircle(x + 11, by - 6, 6, RGB565(108, 160, 50)); fb.fillRect(x + 6, by - 2, 4, 2, W2); break;
    case 'b': if (dTheme == 0) for (int d = -7; d <= 7; d += 2) fb.drawLine(x + 8, by, x + 8 + d, by - 8 + abs(d) / 3, RGB565(110, 62, 40));
      else { dLeaf(x + 8, by, x + 1, by - 12, 3, RGB565(50, 150, 70), RGB565(110, 200, 90)); dLeaf(x + 8, by, x + 15, by - 12, 3, RGB565(50, 150, 70), RGB565(110, 200, 90)); dLeaf(x + 8, by, x + 8, by - 15, 3, RGB565(40, 130, 60), RGB565(110, 200, 90)); } break;
    case 'F': dLeaf(x + 8, by, x - 4, by - 10, 4, RGB565(40, 140, 70), RGB565(120, 210, 100)); dLeaf(x + 8, by, x + 20, by - 10, 4, RGB565(40, 140, 70), RGB565(120, 210, 100));
      dLeaf(x + 8, by, x + 2, by - 20, 4, RGB565(50, 160, 80), RGB565(120, 210, 100)); dLeaf(x + 8, by, x + 14, by - 20, 4, RGB565(50, 160, 80), RGB565(120, 210, 100)); dLeaf(x + 8, by, x + 8, by - 24, 4, RGB565(60, 175, 85), RGB565(130, 220, 110)); break;
    case 'p': fb.fillTriangle(x + 5, by - 3, x + 11, by - 3, x + 8, by - 34, RGB565(150, 190, 60)); fb.fillTriangle(x + 5, by - 3, x + 11, by - 3, x + 8, by - 16, RGB565(70, 150, 50));
      fb.fillRoundRect(x + 3, by - 5, 10, 5, 2, RGB565(150, 80, 40)); break;
    case 'G': if (curLevel == CUSTOM_IDX) {             // BRIQUE : une porte en bois
        fb.fillRect(x, by - 30, 16, 30, W1); fb.drawRect(x, by - 30, 16, 30, W2); fb.drawFastVLine(x + 8, by - 28, 27, W2);
        fb.fillCircle(x + 12, by - 14, 1, COL_STAR); break;
      } { int H = dPoleHpx; fb.fillRect(x + 7, by - H, 2, H, W2); fb.fillCircle(x + 8, by - H - 1, 3, COL_STAR);                       // mat de 5 cases, boule doree, drapeau en haut
      fb.fillRect(x + 9, by - H + 2, 15, 10, COL_GOAL); fb.fillTriangle(x + 24, by - H + 2, x + 24, by - H + 12, x + 29, by - H + 7, COL_GOAL); } break;
    default: break;
  }
}
void dsBallCactus(int cx, int by, bool right, int i) {    // desert : trois boules de cactus superposees, qui se dandinent
  const uint16_t G = RGB565(70, 150, 62), D = RGB565(30, 92, 42), L = RGB565(170, 220, 120), P = RGB565(240, 90, 140);
  float t = millis() * 0.004f + i * 1.7f;
  int x1 = cx + (int)(sinf(t) * 1.2f), x2 = cx + (int)(sinf(t - 0.6f) * 2.0f), x3 = cx + (int)(sinf(t - 1.2f) * 2.6f);   // chaque boule se decale un peu plus que celle du dessous
  fb.fillCircle(x1, by - 6, 6, G); fb.drawCircle(x1, by - 6, 6, D); fb.drawPixel(x1 - 3, by - 9, L); fb.drawPixel(x1 + 4, by - 4, L); fb.drawPixel(x1 - 4, by - 4, D);
  fb.fillCircle(x2, by - 14, 5, G); fb.drawCircle(x2, by - 14, 5, D); fb.drawPixel(x2 - 2, by - 17, L); fb.drawPixel(x2 + 3, by - 12, L);
  fb.fillCircle(x3, by - 21, 4, G); fb.drawCircle(x3, by - 21, 4, D); fb.drawPixel(x3 - 2, by - 23, L);
  int ex = right ? 1 : -1;
  fb.drawPixel(x2 - 2 + ex, by - 14, RGB565(20, 20, 20)); fb.drawPixel(x2 + 2 + ex, by - 14, RGB565(20, 20, 20));   // yeux, tournes vers le joueur
  fb.fillCircle(x3, by - 26, 2, P); fb.drawPixel(x3, by - 26, RGB565(255, 220, 90));                              // petite fleur
}
void dsQuick(int x, int y, int tx, bool top, float tm) {  // sable mouvant : brun sombre, rides qui glissent, bulles ; dessine apres le joueur (il s'enfonce)
  fb.fillRect(x, y, TILE, TILE, top ? RGB565(168, 118, 56) : RGB565(132, 90, 42));
  if (top) {
    fb.drawFastHLine(x, y, TILE, RGB565(250, 226, 150));
    for (int i = 0; i < 3; i++) { int w = 4 + (i & 1) * 2, o = (int)(tm * 9 + tx * 5 + i * 6) % (TILE - w); fb.drawFastHLine(x + o, y + 4 + i * 4, w, RGB565(206, 156, 84)); }
    uint32_t h = gHash(tx * 31 + (millis() / 400)); if ((h & 3) == 0) fb.drawCircle(x + 3 + (int)((h >> 4) % 10), y + 6, 2, RGB565(236, 200, 130));
  } else for (int i = 0; i < 3; i++) fb.drawPixel(x + 2 + (tx * 7 + i * 5) % 12, y + 3 + i * 4, RGB565(96, 62, 28));
}
void dHumpDraw(int cx, int cy) {                           // dunes douces : colonnes de sable au-dessus de la rangee du sol (face eclairee a gauche, face a l'ombre a droite)
  const uint16_t A = RGB565(238, 200, 118), B = RGB565(222, 178, 92), H = RGB565(252, 226, 140);
  const int gy = DGY * TILE - cy;
  if (gy < -2 || gy - 14 >= DSH) return;
  int c0 = max(0, cx / TILE - 1), c1 = min(DW - 1, (cx + DVW) / TILE + 1);
  for (int tx = c0; tx <= c1; tx++) {
    int hm = dHump[tx], hl = (dHump[tx > 0 ? tx - 1 : 0] + hm) / 2, hr = (hm + dHump[tx < DW - 1 ? tx + 1 : DW - 1]) / 2;
    if (!hm && !hl && !hr) continue;
    for (int k = 0; k < 16; k++) {
      int x = tx * TILE + k - cx; if (x < 0 || x >= DVW) continue;
      int h = k < 8 ? hl + (hm - hl) * k / 8 : hm + (hr - hm) * (k - 8) / 8, sl = k < 8 ? hm - hl : hr - hm;
      fb.drawFastVLine(x, gy - h, h + 3, sl < 0 ? B : A); fb.drawPixel(x, gy - h, H);
    }
  }
}
void dsGuy(int cx, int by, bool right, bool raised) {    // petit lanceur : desert = robe + turban rouge ; jungle = singe ; glace / donjon / exterieur : voir dsGuy2
  if (dTheme >= 2) { dsGuy2(cx, by, right, raised); return; }
  const uint16_t SK = RGB565(232, 182, 130);
  if (dTheme == 1) {
    const uint16_t FUR = RGB565(125, 80, 45), FACE = RGB565(236, 196, 150);
    fb.drawLine(cx + (right ? -5 : 5), by - 5, cx + (right ? -10 : 10), by - 12, FUR); fb.drawLine(cx + (right ? -5 : 5), by - 4, cx + (right ? -10 : 10), by - 11, FUR);   // queue
    fb.fillRoundRect(cx - 5, by - 10, 10, 10, 4, FUR); fb.fillCircle(cx, by - 13, 5, FUR);
    fb.fillCircle(cx - 5, by - 15, 2, FUR); fb.fillCircle(cx + 5, by - 15, 2, FUR);
    fb.fillRoundRect(cx - 3, by - 14, 6, 5, 2, FACE); fb.fillRect(cx - 2, by - 13, 1, 2, RGB565(30, 20, 20)); fb.fillRect(cx + 1, by - 13, 1, 2, RGB565(30, 20, 20));
    if (raised) { fb.fillRect(cx + (right ? 4 : -6), by - 19, 3, 7, FUR); fb.fillCircle(cx + (right ? 5 : -5), by - 21, 3, RGB565(90, 60, 30)); }
    else fb.fillRect(cx + (right ? 4 : -6), by - 8, 3, 5, FUR);
    return;
  }
  fb.fillRoundRect(cx - 5, by - 9, 10, 9, 3, RGB565(150, 70, 50));
  fb.fillRect(cx - 4, by - 2, 3, 2, RGB565(60, 40, 30)); fb.fillRect(cx + 1, by - 2, 3, 2, RGB565(60, 40, 30));
  fb.fillCircle(cx, by - 12, 4, SK);
  fb.fillRect(cx - 5, by - 17, 10, 4, RGB565(200, 55, 55)); fb.drawFastHLine(cx - 5, by - 15, 10, RGB565(140, 30, 30));
  fb.fillRect(cx - 1 + (right ? 2 : -2), by - 13, 2, 2, RGB565(30, 20, 20));
  if (raised) { fb.fillRect(cx + (right ? 4 : -6), by - 18, 3, 7, SK); fb.fillCircle(cx + (right ? 5 : -5), by - 20, 2, RGB565(120, 110, 100)); }
  else fb.fillRect(cx + (right ? 4 : -6), by - 8, 3, 5, SK);
}
int flameStage() { return evo < 12 ? 0 : evo < 17 ? 3 : 4; }   // 0 : avatar en pixels ; 3 : flamme vivante ; 4 : bonhomme / cosmonaute / robot
// bonhomme (stades 17-27) : style "petit bonhomme blanc a grosse tete", anime ; k = echelle (1 en jeu, 3 a l'accueil)
// pose : 0 repos, 1 course, 2 saut, 3 chute, 4 atterrissage, 5 perdu ; fr = image d'animation
void drawChibi(Adafruit_GFX &g, int cx, int by, int k, int pose, int fr, bool right, float t) {
  const uint16_t WH = RGB565(246, 246, 250), SHD = RGB565(200, 204, 222), EY = RGB565(20, 20, 30), BL = RGB565(40, 100, 230), BLD = RGB565(20, 50, 140);
  const uint16_t NAVY = RGB565(40, 46, 84), RIM = RGB565(225, 228, 238), GR = RGB565(160, 166, 182), GRD = RGB565(96, 102, 118);
  const uint16_t RU = RGB565(176, 84, 44), RUL = RGB565(210, 120, 70), CY = RGB565(60, 236, 255), OR = RGB565(255, 150, 40), YE = RGB565(255, 220, 80), RD = RGB565(220, 60, 60);
  int e = evo, f = right ? 1 : -1;
  auto R = [&](int x, int y, int w, int h, uint16_t c) { if (!right) x = -(x + w); g.fillRect(cx + x * k, by + y * k, w * k, h * k, c); };
  uint16_t body = e >= 25 ? GR : e >= 22 ? WH : BL;
  int hb = pose == 0 && ((int)(t * 2.5f) & 1) ? 1 : 0, sq = pose == 4 ? 1 : 0;   // tete qui respire ; ecrasement
  if (pose == 5) { R(-6, -7, 12, 3, body); R(-6, -12, 12, 6, WH); R(-3, -10, 1, 1, EY); R(-2, -9, 1, 1, EY); R(1, -10, 1, 1, EY); R(2, -9, 1, 1, EY); return; }   // perdu : a plat
  // jambes
  int la = -3, ra = 1, ly = -3, ry = -3, lh = 3, rh = 3;
  if (pose == 1) { if (fr & 1) { la = -4; ly = -4; lh = 2; ra = 2; } else { ra = 2; ry = -4; rh = 2; la = -4; } }
  else if (pose == 2) { ly = ry = -4; lh = rh = 2; la = -2; ra = 0; }
  else if (pose == 3) { la = -5; ra = 3; }
  R(la, ly, 2, lh, body == WH ? SHD : BLD); R(ra, ry, 2, rh, body == WH ? SHD : BLD);
  if (e >= 22 && e < 25) R(-8, -9, 3, 5, GR);                                         // sac a dos (dans le dos)
  if (e == 23 && (pose == 2 || pose == 3 || pose == 1)) { R(-8, -4, 3, 2 + (fr & 1), OR); R(-7, -2 + (fr & 1), 1, 2, YE); }   // jetpack
  R(-4, -7 + sq, 8, 4 - sq, body);                                                    // corps
  if (e >= 22) { R(-1, -6, 1, 1, RD); R(0, -6, 1, 1, CY); R(1, -6, 1, 1, YE); }           // panneau de poitrine
  // bras
  int ay1 = -7, ay2 = -7;
  if (pose == 1) { ay1 = -7 - (fr & 1); ay2 = -7 - ((fr + 1) & 1); }
  else if (pose == 2) ay1 = ay2 = -11;
  else if (pose == 3) { ay1 = -12; ay2 = -12; }
  int ax1 = pose == 3 ? -7 : -6, ax2 = pose == 3 ? 5 : 4;
  R(ax1, ay1, 2, 3, body); R(ax2, ay2, 2, 3, body);
  if (e >= 26) { R(-7, -8, 3, 1, GRD); R(4, -8, 3, 1, GRD); R(5, -5, 5, 2, GRD); R(10, -5, 1, 2, ((int)(t * 4) & 1) ? CY : WH); }   // epaulettes + canon
  if (e == 27 && (pose == 2 || pose == 3)) { R(-3, 0, 2, 2 + (fr & 1), OR); R(1, 0, 2, 2 + (fr & 1), OR); }   // propulseurs
  // tete
  int hy = -17 + hb + sq, hx = sq ? -7 : -6, hw = sq ? 14 : 12;
  if (e >= 24) {                                                                       // tete de robot
    R(-7, hy - 1, 14, 11, RU); R(-7, hy - 1, 14, 1, RUL); R(-5, hy + 1, 10, 6, RGB565(36, 30, 40));
    int eh = pose == 2 ? 3 : 2; R(-3, hy + 3, 2, eh, CY); R(1, hy + 3, 2, eh, CY);
    R(0, hy - 4, 1, 3, GRD); R(0, hy - 5, 1, 1, ((int)(t * 3) & 1) ? RD : RGB565(255, 160, 160));
  } else {
    g.fillRoundRect(cx + (right ? hx : -(hx + hw)) * k, by + hy * k, hw * k, 10 * k, 2 * k, WH);
    R(hx + 1, hy + 8, hw - 2, 1, SHD);
    if (e >= 21) {                                                                     // casque de cosmonaute
      g.fillRoundRect(cx - 7 * k, by + (hy - 1) * k, 14 * k, 11 * k, 4 * k, NAVY); g.drawRoundRect(cx - 6 * k, by + hy * k, 12 * k, 9 * k, 3 * k, RIM);   // casque de la taille de la tete
      R(-4, hy + 1, 1, 1, WH); R(-3, hy, 1, 1, WH);
      R(-1, hy + 5, 2, 2, BL); R(0, hy + 4, 1, 1, RGB565(150, 210, 255));
    } else {
      int eh = pose == 2 ? 3 : (((int)(t * 10) % 31) == 0 ? 1 : 2), ew = pose == 2 ? 2 : 1;
      R(-2 + 1, hy + 4, ew, eh, EY); R(2 + 1, hy + 4, ew, eh, EY);                         // yeux (tournes vers l'avant)
      if (e >= 20) { R(-4, hy + 4, 10, 2, EY); R(-3, hy + 4, 1, 1, RGB565(120, 120, 140)); }   // lunettes
      if (e >= 19) { R(-5, hy - 1, 11, 3, BL); R(4, hy + 1, 4, 1, BLD); R(-1, hy - 1, 1, 1, OR); R(0, hy - 1, 1, 1, YE); }   // casquette
      else {                                                                           // meche de flamme
        int fh = e == 17 ? 7 : 4;
        g.fillTriangle(cx - 3 * k, by + hy * k, cx + 3 * k, by + hy * k, cx + f * k, by + (hy - fh) * k, BL);
        g.fillTriangle(cx - 1 * k, by + hy * k, cx + 1 * k, by + hy * k, cx + f * k, by + (hy - fh + 3) * k, RGB565(150, 210, 255));
      }
    }
  }
}
int evoBlocks() { return evo < 8 ? evo + 1 : 8; }
int pixRows(int n) { return n <= 3 ? 1 : n <= 6 ? 2 : n == 7 ? 3 : 4; }
float evoScale() { static const float S[5] = { 0.65f, 0.8f, 1.0f, 1.15f, 1.3f }; return evo < 12 ? 1.0f : S[min(evo, 16) - 12]; }
void flameShape(Adafruit_GFX &g, int cx, int by, float h, float wr, float sway, uint16_t cO, uint16_t cM, uint16_t cC, float t);
void flameStaged(Adafruit_GFX &g, int cx, int by, float h, float wr, float sway, uint16_t cO, uint16_t cM, uint16_t cC, float t, bool right, bool moving) {
  if (evo >= 12) { flameShape(g, cx, by, h, wr, sway, cO, cM, cC, t); return; }
  static const int8_t BC[8] = { 1, 0, 2, 1, 0, 2, 1, 1 }, BR[8] = { 0, 0, 0, 1, 1, 1, 2, 3 };   // ordre d'apparition des pixels (colonne, rangee depuis le bas)
  int n = evoBlocks(), bw = max(1, (int)(wr * 0.62f + 0.5f)), bh = max(1, (int)(h * 0.19f + 0.5f)), x0 = cx - bw * 3 / 2;
  for (int i = 0; i < n; i++) {
    uint16_t c = cO;
    if (evo >= 8 && BC[i] == 1 && BR[i] >= 1) c = cM;     // des couleurs
    if (evo >= 9 && BC[i] == 1 && BR[i] == 1) c = cC;
    int sh = BR[i] >= 2 ? (sway > 1.5f ? bw / 2 : sway < -1.5f ? -bw / 2 : 0) : 0;
    g.fillRect(x0 + BC[i] * bw + sh, by - (BR[i] + 1) * bh + 1, bw, bh, c);
  }
}
void flameShape(Adafruit_GFX &g, int cx, int by, float h, float wr, float sway, uint16_t cO, uint16_t cM, uint16_t cC, float t) {   // flamme vivante : 3 couches + langues + braise
  for (int L = 0; L < 3; L++) {                           // 3 couches : teinte, teinte claire, coeur blanc
    float k = L == 0 ? 1.0f : L == 1 ? 0.68f : 0.38f; uint16_t c = L == 0 ? cO : L == 1 ? cM : cC;
    int r = (int)(wr * k + 0.5f), cy0 = by - r - L, tip = by - (int)(h * k), tipx = cx + (int)(sway * k);
    g.fillCircle(cx, cy0, r, c);
    g.fillTriangle(cx - r, cy0, cx + r, cy0, tipx, tip, c);
    if (L == 0) {                                         // langues laterales qui ondulent
      g.fillTriangle(cx - r, cy0 - 1, cx - r / 2, cy0 - r, cx - r - 1 + (int)(sinf(t * 7) * wr * 0.3f), by - (int)(h * 0.62f), c);
      g.fillTriangle(cx + r, cy0 - 1, cx + r / 2, cy0 - r, cx + r + 1 + (int)(sinf(t * 6 + 2) * wr * 0.3f), by - (int)(h * 0.5f), c);
    }
  }
  float ph = fmodf(t * 1.7f, 1.0f);                       // braise detachee au-dessus de la pointe
  if (ph < 0.7f) g.fillCircle(cx + (int)(sway + sinf(t * 11) * wr * 0.3f), by - (int)(h + 2 + ph * h * 0.33f), max(1, (int)(wr * 0.15f)), cM);
}
#define AVKEY 0x0020                                     // couleur "transparente" du tampon de l'avatar
FastCanvas avc(48, 56);                                  // l'avatar est dessine ici puis colle dans la bande avec un contour
void avBlit(FastCanvas &c, int cw, int ch, uint16_t *dst, int stride, int dw, int dh, int x0, int y0, uint16_t key, uint16_t oc) {   // copie + contour (pixels transparents voisins d'un pixel plein)
  const uint16_t *a = c.getBuffer();
  for (int y = 0; y < ch; y++) {
    int yy = y0 + y; if (yy < 0 || yy >= dh) continue;
    for (int x = 0; x < cw; x++) {
      int xx = x0 + x; if (xx < 0 || xx >= dw) continue;
      uint16_t v = a[y * cw + x];
      if (v != key) dst[yy * stride + xx] = v;
      else if ((x > 0 && a[y * cw + x - 1] != key) || (x < cw - 1 && a[y * cw + x + 1] != key) || (y > 0 && a[(y - 1) * cw + x] != key) || (y < ch - 1 && a[(y + 1) * cw + x] != key)) dst[yy * stride + xx] = oc;
    }
  }
}
void dsFlame(int cx, int by, float f) {                  // le joueur : dessine dans avc, puis colle dans la bande avec un contour (clair sur fond noir)
  avc.fillScreen(AVKEY);
  dsAvatarTo(24, 50, f);
  avBlit(avc, 48, 56, fb.getBuffer(), SCREEN_W, DVW, DSH, cx - 24, by - 50, AVKEY, dTheme == 4 && dBuild < 2 ? RGB565(210, 210, 230) : RGB565(20, 20, 36));
}
void dsAvatarTo(int cx, int by, float f) {                  // le joueur : petite flamme vivante ; f = jauge 0..1 (s'estompe, ou gonfle et rougit au donjon)
  float t = millis() / 1000.0f;
  float sc = dTheme == 3 ? 1.0f + 0.25f * f : 1.0f - 0.5f * f;
  const uint16_t bg = dTheme == 2 ? RGB565(200, 224, 244) : dTheme == 3 ? RGB565(255, 110, 40) : dTheme == 1 ? RGB565(40, 120, 66) : dTheme == 4 ? RGB565(150, 205, 236) : RGB565(232, 207, 168);
  int mk = (int)(f * 190);
  uint16_t cO = RGB565(30, 85, 230), cM = RGB565(80, 170, 255), cC = RGB565(225, 246, 255), cE = RGB565(8, 24, 100);
  if (skinSel > 0) { cO = skinColor(skinSel); cM = dMix(cO, RGB565(255, 255, 255), 100); cC = dMix(cO, RGB565(255, 255, 255), 215); cE = SKIN_COLD[skinSel]; }   // skin de la boutique = couleur de la flamme
  cO = dMix(cO, bg, mk); cM = dMix(cM, bg, mk); cC = dMix(cC, bg, mk); cE = dMix(cE, bg, mk);
  float st = -dvy * 0.025f; if (st > 5) st = 5; if (st < -3) st = -3;      // s'etire en montant, s'ecrase en tombant
  static bool wasG = true; static float landT = 0; static unsigned long lastMs = 0;   // ecrasement a l'atterrissage
  unsigned long nowMs = millis(); float fdt = min(0.1f, (nowMs - lastMs) / 1000.0f); lastMs = nowMs;
  if (dGround && !wasG && dPhase == 0) landT = 0.2f;
  wasG = dGround; if (landT > 0) landT -= fdt;
  float sq = landT > 0 ? landT / 0.2f : 0;
  if (evo >= 17) {                                       // bonhomme / cosmonaute / robot : pose selon l'action
    int pose = dPhase == 2 ? 5 : !dGround ? (dvy < 0 ? 2 : 3) : sq > 0 ? 4 : fabsf(dvx) > 30 ? 1 : 0;
    drawChibi(avc, cx, by, 1, pose, (int)(millis() / 90), dFacingR, t); return;
  }
  float es = evoScale(), h = (21 + st) * sc * es * (1 - 0.4f * sq), wr = 6.5f * sc * es * (1 + 0.4f * sq - (st > 0 ? st * 0.03f : 0));
  float sway = sinf(t * 9) * 2.2f + sinf(t * 5.3f + 1) * 1.4f - dvx * 0.03f;
  flameStaged(avc, cx, by, h, wr, sway, cO, cM, cC, t, dFacingR, fabsf(dvx) > 30);
  int ey = by - (int)(wr * 1.3f), fx = dFacingR ? 1 : -1, ex1 = cx - 3 + fx, ex2 = cx + 1 + fx;
  if (evo < 12) {                                        // avatar en pixels : yeux sur la rangee du haut
    int n = evoBlocks(), top = by - pixRows(n) * max(1, (int)(h * 0.19f + 0.5f)) + 1;
    if (n <= 3) { avc.drawPixel(cx - 2 + (fx > 0), top + 1, cE); avc.drawPixel(cx + (fx > 0), top + 1, cE); return; }
    ey = top + 1;
  }
  if (dPhase == 2) {                                     // perdu : yeux en croix
    for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1; avc.drawLine(ex - 1, ey, ex + 1, ey + 2, cE); avc.drawLine(ex - 1, ey + 2, ex + 1, ey, cE); }
  } else if (!dGround && dvy < -120) {                   // saut : grands yeux, bouche en O
    avc.fillRect(ex1, ey - 1, 2, 4, cE); avc.fillRect(ex2, ey - 1, 2, 4, cE); avc.drawRect(cx - 1 + fx, ey + 4, 3, 3, cE);
  } else if (!dGround && dvy > 220) {                    // chute : yeux affoles, bouche tordue
    avc.drawRect(ex1 - 1, ey - 1, 3, 3, cE); avc.drawRect(ex2, ey - 1, 3, 3, cE); avc.drawLine(cx - 2 + fx, ey + 5, cx + 2 + fx, ey + 4, cE);
  } else if (sq > 0) {                                   // atterrissage : yeux plisses ^ ^
    for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1; avc.drawPixel(ex - 1, ey + 2, cE); avc.drawPixel(ex, ey + 1, cE); avc.drawPixel(ex + 1, ey + 2, cE); }
  } else if ((nowMs % 3100) < 120) {                     // clignement
    avc.drawFastHLine(ex1, ey + 1, 2, cE); avc.drawFastHLine(ex2, ey + 1, 2, cE);
  } else {
    avc.fillRect(ex1, ey, 2, 3, cE); avc.fillRect(ex2, ey, 2, 3, cE);
    if (fabsf(dvx) > 60) avc.drawFastHLine(cx - 1 + fx, ey + 5, 3, cE);   // en course : petite bouche decidee
  }
}
void dsAvatar(int x0, int y0) {                          // jungle : l'avatar du joueur (skin), bras leves quand il tient une liane
  int tier = computePlayerTier();
  uint16_t bc = COL_PLAYER_TIER[tier], bd = COL_PLAYERD_TIER[tier];
  if (skinSel > 0) { bc = skinColor(skinSel); bd = SKIN_COLD[skinSel]; }
  if (dTheme == 2 && dHeat > 0) { int ht = (int)(dHeat * 2.4f); bc = dMix(bc, RGB565(196, 228, 255), ht); bd = dMix(bd, RGB565(100, 150, 210), ht); if (dHeat > 50 && ((millis() / 70) & 1)) x0 += 1; }   // glace : givre + frissons
  else if (dTheme == 3 && dHeat > 0) { int ht = (int)(dHeat * 2.4f); bc = dMix(bc, RGB565(255, 100, 40), ht); bd = dMix(bd, RGB565(150, 30, 10), ht); }   // donjon : rougit
  skinBody(fb, skinSel, x0, y0, 12, 14, 3, bc, bd);
  int ex = dFacingR ? x0 + 7 : x0 + 2;
  fb.fillRect(ex, y0 + 4, 2, 3, bd); fb.fillRect(ex + 3, y0 + 4, 2, 3, bd);
  if (dVineIdx >= 0) { fb.drawLine(x0 + 2, y0 + 4, x0 + 5, y0 - 3, bd); fb.drawLine(x0 + 10, y0 + 4, x0 + 7, y0 - 3, bd); }
}
// rayons de soleil et ombres sur le fond (avant les blocs) : sur chaque rayon, clair en bandes jusqu'au 1er obstacle,
// puis ombre (25 % plus sombre) derriere lui. 2 pixels a la fois (mots de 32 bits).
static inline uint32_t dHalf2(uint32_t c) { return (c & 0xF7DEF7DEu) >> 1; }
void dShadePass(int sTop) {
  uint16_t *b = fb.getBuffer();
  int camXi = (int)dcamX, camYi = (int)dcamY;
  for (int row = 0; row < DSH; row++) {
    int yw = camYi + sTop + row, u = camXi + yw * 7 / 10;
    uint32_t *p = (uint32_t *)(b + row * SCREEN_W);
    for (int x = 0; x < DVW; x += 2, u += 2) {
      uint32_t c = *p;
      if (yw > (int)dysW[u >> 1]) c = dHalf2(c) + ((c & 0xE79CE79Cu) >> 2);                                       // ombre
      else if ((u & 63) < 22) { uint32_t m = dHalf2(c) + 0x7BEF7BEFu; c = dHalf2(c) + dHalf2(m); }               // rayon : 25 % de blanc
      *p++ = c;
    }
  }
}
// ombre sur le dessus d'un bloc (sol, marches, caisses) : au pixel pres, par colonnes de 2 px (une seule tuile du dessus par colonne)
void dShadeTile(int x, int y, int tx, int ty) {
  uint16_t *b = fb.getBuffer();
  for (int k = 0; k < 8; k++) {
    if (!dShadeAt(tx * TILE + 2 * k, ty * TILE - 2)) continue;
    int xx = x + 2 * k; if (xx < 0 || xx + 1 >= DVW) continue;
    for (int r = 0; r < 16; r++) {
      int yy = y + r; if (yy < 0 || yy >= DSH) continue;
      uint16_t *p = b + yy * SCREEN_W + xx;
      for (int j = 0; j < 2; j++) { uint16_t c = p[j]; p[j] = (uint16_t)(((c & 0xF7DE) >> 1) + ((c & 0xE79C) >> 2)); }
    }
  }
}
// tuiles 16x16 dessinees une fois (puis copiees par lignes) : sol (dessus / interieur, 2 variantes de grain) et bloc (caisse / rondin)
void dBuildSprites(int theme) {
  static GFXcanvas16 tc(16, 16);
  for (int v = 0; v < 6; v++) {
    bool top = v == 0 || v == 1 || v == 4;
    if (theme == 0) {
      const uint16_t sandHi = RGB565(252, 226, 140), sandA = RGB565(238, 200, 118), sandB = RGB565(222, 178, 92), sandD = RGB565(196, 148, 66);
      const uint16_t W1 = RGB565(168, 105, 60), W2 = RGB565(110, 68, 34);
      if (v == 3) {
        tc.fillRect(0, 0, 16, 16, W1); tc.drawRect(0, 0, 16, 16, W2); tc.drawRect(2, 2, 12, 12, W2);
        tc.drawLine(2, 2, 13, 13, W2); tc.drawLine(13, 2, 2, 13, W2); tc.drawLine(3, 2, 13, 12, W2); tc.drawLine(12, 2, 2, 12, W2);
      } else {
        tc.fillScreen(top ? sandA : sandB);                                          // sable : rides douces (periode 16 = raccord entre tuiles), sans cadre
        for (int x = 0; x < 16; x++) { int w = (int)lroundf(1.6f * sinf((x + v * 3) * 0.3927f)); for (int y = 3; y < 16; y += 5) tc.drawPixel(x, y + w, top ? sandB : sandD); }
        if (top) tc.drawFastHLine(0, 0, 16, sandHi);
        int h = v * 11 + 3; tc.drawPixel(3 + h % 9, 5 + h % 7, sandD); tc.drawPixel(4 + (h * 3) % 8, 9 + (h * 5) % 5, sandHi);
      }
    } else if (theme == 1) {
      const uint16_t soil = RGB565(92, 62, 36), soilD = RGB565(58, 38, 22), grass = RGB565(66, 170, 62), grassL = RGB565(120, 210, 90), grassD = RGB565(38, 110, 44);
      if (v == 3) {                                       // rondin : ecorce orange, anneaux plus fonces (comme la planche)
        const uint16_t o1 = RGB565(206, 112, 52), o2 = RGB565(236, 150, 82), o3 = RGB565(132, 66, 30);
        tc.fillScreen(o3); tc.fillRoundRect(0, 1, 16, 14, 4, o1); tc.drawFastHLine(2, 2, 12, o2); tc.drawFastHLine(2, 13, 12, o3);
        tc.fillRect(6, 1, 3, 14, o3); tc.drawFastVLine(5, 3, 10, o2); tc.drawPixel(2, 6, o3); tc.drawPixel(12, 9, o3); tc.drawPixel(13, 5, o2);
      } else {
        tc.fillScreen(soilD); tc.fillRoundRect(0, 0, 16, 16, 2, soil);
        int h = v * 13 + 5; tc.drawPixel(2 + h % 11, 6 + h % 8, soilD); tc.drawPixel(3 + (h * 3) % 10, 9 + (h * 5) % 6, RGB565(130, 118, 104)); tc.drawPixel(8 + h % 5, 12, soilD);
        if (top) {
          tc.fillRect(0, 0, 16, 5, grass); tc.drawFastHLine(0, 0, 16, grassL); tc.drawFastHLine(0, 4, 16, grassD);
          for (int i = 0; i < 4; i++) tc.drawPixel(1 + i * 4 + (v & 1) * 2, 5 + (i & 1), grass);
        }
      }
    } else if (theme == 2) {                              // glace : dessus enneige, corps de glace, bloc = cube de glace
      const uint16_t sn = RGB565(244, 250, 255), snS = RGB565(200, 224, 246), ice = RGB565(146, 198, 236), ice2 = RGB565(124, 180, 224), crack = RGB565(214, 238, 255), bd = RGB565(88, 140, 196);
      tc.fillScreen(bd);
      if (v == 3) { tc.fillRoundRect(0, 0, 16, 16, 3, RGB565(172, 220, 248)); tc.drawRoundRect(0, 0, 16, 16, 3, bd); tc.fillRect(2, 2, 5, 2, sn); tc.drawLine(2, 3, 2, 8, sn); tc.drawLine(13, 13, 9, 13, RGB565(220, 240, 255)); }
      else {
        tc.fillRoundRect(0, 0, 16, 16, 2, (v & 1) ? ice : ice2);
        int h = v * 7 + 3; tc.drawLine(2 + h % 6, 6 + h % 4, 8 + h % 5, 12, crack); tc.drawPixel(11, 7 + h % 4, crack);
        if (top) { tc.fillRect(0, 0, 16, 5, sn); tc.drawFastHLine(0, 4, 16, snS); tc.drawPixel(3 + (v & 3), 5, sn); tc.drawPixel(10 - (v & 1) * 3, 5, sn); }
      }
    } else if (theme == 3) {                              // donjon : dalles de pierre, lave (1 = surface, 5 = profondeur), bloc = obsidienne a veine ardente
      const uint16_t st = RGB565(70, 62, 76), stD = RGB565(36, 30, 42), stL = RGB565(98, 88, 106), ember = RGB565(255, 110, 30);
      if (v == 1 || v == 5) {
        tc.fillScreen(v == 1 ? RGB565(235, 90, 20) : RGB565(212, 64, 18));
        if (v == 1) { tc.fillRect(0, 0, 16, 2, RGB565(255, 236, 130)); tc.fillRect(0, 2, 16, 4, RGB565(255, 170, 40)); }
        int h = v * 5 + 1; tc.fillCircle(3 + h % 9, 9 + h % 5, 2, RGB565(190, 50, 16)); tc.drawPixel(12 - h % 6, 11, RGB565(255, 160, 40)); tc.drawPixel(7, 4 + h % 3, RGB565(255, 200, 80));
      } else if (v == 3) { tc.fillScreen(stD); tc.fillRoundRect(1, 1, 14, 14, 2, RGB565(80, 60, 72)); tc.drawFastHLine(2, 8, 12, ember); tc.drawFastHLine(2, 7, 12, RGB565(150, 60, 30)); }
      else {
        tc.fillScreen(stD); tc.fillRect(0, 0, 16, 7, st); tc.fillRect(0, 8, 16, 8, top ? st : RGB565(56, 48, 62));
        tc.drawFastVLine(8 - (v & 1) * 4, 0, 7, stD); tc.drawFastVLine(3 + (v & 1) * 6, 8, 8, stD);
        if (top) { tc.drawFastHLine(0, 0, 16, stL); if (v == 0) tc.drawLine(4, 3, 7, 5, ember); }
      }
    } else {                                              // exterieur : herbe sur terre, bloc = caisse en bois (planche)
      const uint16_t soil = RGB565(112, 78, 46), soilD = RGB565(78, 52, 32), grass = RGB565(96, 196, 72), grassL = RGB565(146, 226, 100), grassD = RGB565(52, 130, 50);
      if (v == 3) { const uint16_t w1 = RGB565(218, 132, 76), w2 = RGB565(142, 78, 44); tc.fillRect(0, 0, 16, 16, w1); tc.drawRect(0, 0, 16, 16, w2); tc.drawRect(2, 2, 12, 12, w2); tc.drawLine(2, 2, 13, 13, w2); tc.drawLine(13, 2, 2, 13, w2); }
      else {
        tc.fillScreen(soilD); tc.fillRoundRect(0, 0, 16, 16, 2, soil);
        int h = v * 13 + 5; tc.drawPixel(2 + h % 11, 6 + h % 8, soilD); tc.drawPixel(3 + (h * 3) % 10, 9 + (h * 5) % 6, RGB565(148, 132, 112));
        if (top) { tc.fillRect(0, 0, 16, 5, grass); tc.drawFastHLine(0, 0, 16, grassL); tc.drawFastHLine(0, 4, 16, grassD); for (int i = 0; i < 4; i++) tc.drawPixel(1 + i * 4 + (v & 1) * 2, 5 + (i & 1), grass); }
      }
    }
    if (theme == 4 && dBuild < 2) tc.fillScreen(v == 3 ? RGB565(150, 158, 172) : RGB565(112, 120, 136));   // niveaux 1-2 : aplats unis
    memcpy(dTileSpr[v], tc.getBuffer(), 512);
  }
}
static inline void dSpr(const uint16_t *s, int x, int y) {      // copie une tuile 16x16 dans la bande (avec rognage)
  if (x >= DVW || x <= -16 || y >= DSH || y <= -16) return;
  int r0 = y < 0 ? -y : 0, r1 = y + 16 > DSH ? DSH - y : 16, c0 = x < 0 ? -x : 0, c1 = x + 16 > DVW ? DVW - x : 16;
  uint16_t *b = fb.getBuffer();
  for (int r = r0; r < r1; r++) memcpy(b + (y + r) * SCREEN_W + x + c0, s + r * 16 + c0, (c1 - c0) * 2);
}
// agrandit la bande logique (256x96 dans fb) en 320x120 sur place (de bas en haut : une ligne n'est ecrasee qu'apres avoir servi) puis l'envoie.
// Rapport 4 -> 5 : 4 pixels sources donnent 5 pixels (s0 s0 s1 s2 s3) ; la 2e ligne de chaque groupe de 5 copie la 1re.
static inline void dScaleRow(uint16_t *d, const uint16_t *s) {
  for (int g = 63; g >= 0; g--) {
    const uint16_t *a = s + g * 4; uint16_t *o = d + g * 5;
    uint16_t s0 = a[0], s1 = a[1], s2 = a[2], s3 = a[3];
    o[4] = s3; o[3] = s2; o[2] = s1; o[1] = s0; o[0] = s0;
  }
}
// ---- envoi DMA : on prend le SPI2 a Arduino pendant les niveaux (rendu des qu'un menu dessine : voir MeatTFT::startWrite). Chaque bande
// est envoyee par DMA pendant que le CPU dessine la suivante (au lieu d'attendre ~13 ms par bande). ----
static inline uint16_t dSw(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }   // l'ecran veut l'octet de poids fort d'abord
static inline void dScaleRowSw(uint16_t *d, const uint16_t *s) {                   // 4 -> 5 pixels, octets echanges, vers un autre tampon
  for (int g = 0; g < 64; g++) {
    const uint16_t *a = s + g * 4; uint16_t *o = d + g * 5;
    uint16_t s0 = dSw(a[0]);
    o[0] = s0; o[1] = s0; o[2] = dSw(a[1]); o[3] = dSw(a[2]); o[4] = dSw(a[3]);
  }
}
static void dTx(const void *p, int n, int dc) {
  spi_transaction_t t; memset(&t, 0, sizeof t); t.length = n * 8; t.tx_buffer = p;
  gpio_set_level((gpio_num_t)TFT_DC, dc); spi_device_polling_transmit(dDev, &t);
}
void dWaitDma() {
  if (!dDmaBusy) return;
  for (int i = 0; i < 4; i++) { spi_transaction_t *r; spi_device_get_trans_result(dDev, &r, portMAX_DELAY); }
  dDmaBusy = false; digitalWrite(TFT_CS, HIGH);
}
bool dSpiStart() {
  if (dDev) return true;
  SPI.end();
  spi_bus_config_t bc = {}; bc.mosi_io_num = TFT_MOSI; bc.miso_io_num = -1; bc.sclk_io_num = TFT_SCLK; bc.quadwp_io_num = -1; bc.quadhd_io_num = -1; bc.max_transfer_sz = SCREEN_W * 30 * 2;   // le materiel limite un transfert a 32 Ko : une bande = 4 transferts de 30 lignes
  bool ok = spi_bus_initialize(SPI2_HOST, &bc, SPI_DMA_CH_AUTO) == ESP_OK;
  if (ok) {
    spi_device_interface_config_t dc = {}; dc.mode = 0; dc.clock_speed_hz = 80000000; dc.spics_io_num = -1; dc.queue_size = 4;   // CS gere a la main (comme Adafruit)
    ok = spi_bus_add_device(SPI2_HOST, &dc, &dDev) == ESP_OK;
    if (!ok) { spi_bus_free(SPI2_HOST); dDev = nullptr; }
  }
  if (ok && !dOb) { dOb = (uint16_t *)heap_caps_malloc(SCREEN_W * STRIP_H * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT); if (!dOb) { spi_bus_remove_device(dDev); dDev = nullptr; spi_bus_free(SPI2_HOST); ok = false; } }
  if (!ok) { SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); }
  dDmaBusy = false;
  return ok;
}
void dSpiStop() {
  if (!dDev) return;
  dWaitDma(); digitalWrite(TFT_CS, HIGH);
  spi_bus_remove_device(dDev); dDev = nullptr; spi_bus_free(SPI2_HOST);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  dDma = false;
}
void dSendStrip(int yScreen) {                         // CASET / RASET / RAMWR puis la bande en 4 transferts DMA (dOb pret, octets echanges)
  int x0 = tft.ox(), x1 = x0 + SCREEN_W - 1, y0 = tft.oy() + yScreen, y1 = y0 + STRIP_H - 1;
  uint8_t ca[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 }, ra[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 };
  uint8_t c;
  digitalWrite(TFT_CS, LOW);
  c = 0x2A; dTx(&c, 1, 0); dTx(ca, 4, 1);
  c = 0x2B; dTx(&c, 1, 0); dTx(ra, 4, 1);
  c = 0x2C; dTx(&c, 1, 0);
  static spi_transaction_t tr[4];
  gpio_set_level((gpio_num_t)TFT_DC, 1);
  for (int i = 0; i < 4; i++) { memset(&tr[i], 0, sizeof tr[i]); tr[i].length = SCREEN_W * 30 * 16; tr[i].tx_buffer = dOb + i * SCREEN_W * 30; spi_device_queue_trans(dDev, &tr[i], portMAX_DELAY); }
  dDmaBusy = true;
}
// bande 320x120 telle quelle (jeu principal, mini-jeux) : octets echanges dans dOb, puis DMA ; pendant l'envoi le CPU dessine la bande suivante.
// Repli sur l'envoi Adafruit si la memoire manque ou si le wifi tourne (2 joueurs).
void dFreeOb() { if (dOb && !dDev) { heap_caps_free(dOb); dOb = nullptr; } }   // 77 Ko rendus au tas (le wifi en a besoin)
void dBlitPlain(int yScreen) {
  if (!dDev && !dNoDma && !espOn && !dSpiStart()) dNoDma = true;
  if (!dDev) { tft.drawRGBBitmap(0, yScreen, fb.getBuffer(), SCREEN_W, STRIP_H); return; }
  dWaitDma();
  const uint32_t *src = (const uint32_t *)fb.getBuffer(); uint32_t *dst = (uint32_t *)dOb;
  for (int i = 0; i < SCREEN_W * STRIP_H / 2; i++) { uint32_t v = src[i]; dst[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu); }
  dSendStrip(yScreen);
}
void dBlit(int yScreen) {                              // bande logique 256x96 -> ecran 320x120 (zoom x1.25)
  uint16_t *b = fb.getBuffer();
  if (!dDma) {                                         // repli : mise a l'echelle sur place + envoi bloquant Adafruit
    for (int y = STRIP_H - 1; y >= 0;) {
      uint16_t *dst = b + y * SCREEN_W;
      dScaleRow(dst, b + (y * 4 / 5) * SCREEN_W);
      if (y % 5 == 1) { memcpy(b + (y - 1) * SCREEN_W, dst, SCREEN_W * 2); y -= 2; } else y--;
    }
    tft.drawRGBBitmap(0, yScreen, b, SCREEN_W, STRIP_H);
    return;
  }
  BT(5, dWaitDma());                                   // la bande precedente doit etre partie avant de reecrire le tampon de sortie
  BT(3, {
    for (int y = 0; y < STRIP_H; y++) {
      uint16_t *dst = dOb + y * SCREEN_W;
      if (y % 5 == 1) memcpy(dst, dst - SCREEN_W, SCREEN_W * 2); else dScaleRowSw(dst, b + (y * 4 / 5) * SCREEN_W);
    }
  });
  BT(4, dSendStrip(yScreen));
}
// ciel / dunes (desert) : remplissage par mots de 32 bits (les fillRect d'Adafruit sont lents sur un canevas)
void dSky(int sTop) {
  uint16_t *b = fb.getBuffer();
  static const uint16_t skyC[3][4] = {                 // 3 ambiances selon la variante : jour, couchant, midi pale
    { RGB565(232, 207, 168), RGB565(240, 218, 170), RGB565(246, 226, 176), RGB565(222, 190, 146) },
    { RGB565(236, 192, 148), RGB565(244, 202, 148), RGB565(250, 212, 152), RGB565(214, 164, 124) },
    { RGB565(240, 224, 190), RGB565(246, 232, 200), RGB565(250, 240, 214), RGB565(230, 208, 170) } };
  const uint16_t *k = skyC[dVar % 3], c1 = k[0], c2 = k[1], c3 = k[2], dune = k[3];
  for (int row = 0; row < DSH; row++) {
    int y = sTop + row; uint16_t c = y < 40 ? c3 : y < 80 ? c2 : c1;
    uint32_t v = (uint32_t)c | ((uint32_t)c << 16), *p = (uint32_t *)(b + row * SCREEN_W);
    for (int q = 0; q < DVW / 2; q++) p[q] = v;
  }
  fb.fillCircle(208, 36 - sTop, 24, RGB565(244, 222, 150)); fb.fillCircle(208, 36 - sTop, 16, RGB565(255, 238, 150));
  int dsh = (int)dcamX * 3 / 10, dyo = (int)dcamY * 4 / 10;
  uint32_t dv = (uint32_t)dune | ((uint32_t)dune << 16);
  for (int x = 0; x < DVW; x += 4) {
    int y0 = 150 - dyo - dDune[x + dsh] - sTop; if (y0 < 0) y0 = 0;
    for (int row = y0; row < DSH; row++) { uint32_t *p = (uint32_t *)(b + row * SCREEN_W + x); p[0] = dv; p[1] = dv; }
  }
}
// fond de jungle : degrade vert profond, troncs lointains et grandes feuilles qui defilent plus lentement
void dSkyJ(int sTop) {
  uint16_t *b = fb.getBuffer();
  int camX = (int)dcamX;
  for (int row = 0; row < DSH; row++) {
    int y = sTop + row; uint16_t c = dMix(RGB565(18, 48, 34), RGB565(58, 118, 70), y * 256 / DVH);
    uint32_t v = (uint32_t)c | ((uint32_t)c << 16), *p = (uint32_t *)(b + row * SCREEN_W);
    for (int q = 0; q < DVW / 2; q++) p[q] = v;
  }
  int off1 = (camX * 3 / 10) % 56;
  for (int k = -1; k < DVW / 56 + 2; k++) {              // troncs lointains
    int x = k * 56 - off1 + ((k * 13) & 7);
    fb.fillRect(x, 0, 9, DSH, RGB565(22, 62, 40)); fb.fillRect(x + 6, 0, 3, DSH, RGB565(16, 46, 30));
    fb.fillCircle(x + 4, 4 - sTop, 15, RGB565(26, 78, 48));
  }
  int off2 = (camX * 5 / 10) % 70;
  for (int k = -1; k < DVW / 70 + 2; k++) {              // grandes feuilles du plan moyen
    int lx = k * 70 - off2 + ((k * 29) & 15), ly = 26 + ((k * 37) & 63) - sTop, dirx = (k & 1) ? 1 : -1;
    dLeaf(lx, ly, lx + dirx * 34, ly - 14, 8, RGB565(34, 120, 60), RGB565(80, 170, 90));
    dLeaf(lx + 20, ly + 40, lx + 20 - dirx * 30, ly + 28, 7, RGB565(30, 104, 54), RGB565(70, 150, 82));
  }
}
void dsVine(int i, int cx, int cy) {                     // liane : corde verte, feuilles, noeud d'ancrage
  DVine &v = dVines[i];
  float t = millis() / 1000.0f, th = (dVineIdx == i) ? dTheta : 0.06f * sinf(t * 1.6f + i), L = (dVineIdx == i) ? dVineL : (float)v.len;
  float s = sinf(th), c = cosf(th);
  int ax = v.ax - cx, ay = v.ay - cy, ex = ax + (int)(L * s), ey = ay + (int)(L * c);
  const uint16_t R1 = RGB565(84, 140, 48), R2 = RGB565(46, 88, 34), LF = RGB565(60, 178, 78), LV = RGB565(130, 220, 110);
  fb.drawLine(ax - 1, ay, ex - 1, ey, R2); fb.drawLine(ax, ay, ex, ey, R1); fb.drawLine(ax + 1, ay, ex + 1, ey, R1);
  for (int j = 1; j <= 3; j++) {
    float f = j * 0.27f; int lx = ax + (int)(L * f * s), ly = ay + (int)(L * f * c); int d = (j & 1) ? 1 : -1;
    dLeaf(lx, ly, lx + d * 11, ly + 5, 3, LF, LV);
  }
  fb.fillCircle(ax, ay, 3, RGB565(120, 76, 40)); dLeaf(ax, ay, ax - 9, ay - 6, 3, LF, LV); dLeaf(ax, ay, ax + 9, ay - 6, 3, LF, LV);
  dLeaf(ex, ey, ex - 5, ey + 12, 3, LF, LV); dLeaf(ex, ey, ex + 5, ey + 12, 3, LF, LV);
}
// ---- dessin des mondes glace / donjon de feu / exterieur (primitives) ----
void dBlitIdx(const uint8_t *idx, const uint16_t *pal, int w, int h, int x0, int y0) {   // sprite indexe (0 = transparent), rogne a la bande
  uint16_t *b = fb.getBuffer();
  for (int r = 0; r < h; r++) {
    int yy = y0 + r; if (yy < 0 || yy >= DSH) continue;
    const uint8_t *row = idx + r * w; uint16_t *dst = b + yy * SCREEN_W;
    int c0 = x0 < 0 ? -x0 : 0, c1 = x0 + w > DVW ? DVW - x0 : w;
    for (int c = c0; c < c1; c++) { int v = row[c]; if (v) dst[x0 + c] = pal[v]; }
  }
}
void dsTreeSpr(int k, int bx, int by) {                  // arbre de la planche : sprite indexe (0 = transparent), pied du tronc en (bx, by)
  const uint8_t *idx; const uint16_t *pal; int w, h, ax;
  if (k == 0) { idx = TREE0_IDX; pal = TREE0_PAL; w = TREE0_W; h = TREE0_H; ax = TREE0_AX; }
  else if (k == 1) { idx = TREE1_IDX; pal = TREE1_PAL; w = TREE1_W; h = TREE1_H; ax = TREE1_AX; }
  else { idx = TREE2_IDX; pal = TREE2_PAL; w = TREE2_W; h = TREE2_H; ax = TREE2_AX; }
  int x0 = bx - ax, y0 = by - h + 1;
  uint16_t *b = fb.getBuffer();
  for (int r = 0; r < h; r++) {
    int yy = y0 + r; if (yy < 0 || yy >= DSH) continue;
    const uint8_t *row = idx + r * w; uint16_t *dst = b + yy * SCREEN_W;
    for (int c = 0; c < w; c++) { int v = row[c], xx = x0 + c; if (v && xx >= 0 && xx < DVW) dst[xx] = pal[v]; }
  }
}
void dsOak(int bx, int by) {                             // chene : gros tronc, houppier en boules vertes (planche exterieur)
  const uint16_t T1 = RGB565(132, 78, 54), T2 = RGB565(86, 50, 38), G1 = RGB565(52, 128, 40), G2 = RGB565(78, 166, 52), G3 = RGB565(106, 196, 70);
  fb.fillTriangle(bx - 13, by, bx + 13, by, bx, by - 22, T1);
  fb.fillRect(bx - 7, by - 54, 14, 36, T1); fb.drawFastVLine(bx - 7, by - 54, 36, T2); fb.drawFastVLine(bx + 6, by - 54, 36, T2); fb.drawFastVLine(bx - 1, by - 50, 28, T2);
  fb.fillCircle(bx - 22, by - 62, 15, G1); fb.fillCircle(bx + 21, by - 64, 15, G1); fb.fillCircle(bx - 12, by - 48, 12, G1); fb.fillCircle(bx + 14, by - 50, 12, G1);
  fb.fillCircle(bx, by - 76, 19, G2); fb.fillCircle(bx - 14, by - 66, 13, G2); fb.fillCircle(bx + 12, by - 66, 14, G2); fb.fillCircle(bx, by - 58, 14, G1);
  fb.fillCircle(bx - 6, by - 82, 8, G3); fb.fillCircle(bx + 10, by - 72, 7, G3); fb.fillCircle(bx - 20, by - 66, 6, G3); fb.fillCircle(bx + 24, by - 68, 6, G3);
}
void dsBirch(int bx, int by) {                           // bouleau : tronc blanc, feuillage retombant
  const uint16_t W = RGB565(230, 226, 214), M = RGB565(72, 68, 62), G1 = RGB565(46, 128, 52), G2 = RGB565(70, 160, 66);
  fb.fillRect(bx - 4, by - 64, 8, 64, W); fb.drawFastVLine(bx + 3, by - 64, 64, RGB565(190, 186, 174));
  for (int i = 0; i < 6; i++) fb.drawFastHLine(bx - 3 + (i & 1), by - 8 - i * 10, 3, M);
  fb.fillCircle(bx - 15, by - 66, 12, G1); fb.fillCircle(bx + 15, by - 66, 12, G1); fb.fillCircle(bx, by - 76, 15, G2); fb.fillCircle(bx, by - 64, 13, G1);
  for (int i = -2; i <= 2; i++) { fb.drawFastVLine(bx + i * 8, by - 60, 10 + (i & 1) * 5, G1); fb.drawFastVLine(bx + i * 8 + 1, by - 60, 8 + (i & 1) * 5, G2); }
}
void dsPine(int bx, int by) {                            // sapin enneige
  const uint16_t T = RGB565(94, 62, 42), G = RGB565(40, 110, 86), S = RGB565(244, 250, 255);
  fb.fillRect(bx - 3, by - 16, 7, 16, T);
  for (int i = 0; i < 4; i++) {
    int w = 20 - i * 4, y = by - 12 - i * 18;
    fb.fillTriangle(bx - w, y, bx + w, y, bx, y - 26, G);
    fb.fillTriangle(bx - w / 2 - 2, y - 12, bx + w / 2 + 2, y - 12, bx, y - 26, S);
    fb.drawFastHLine(bx - w + 2, y - 1, w * 2 - 4, S);
  }
}
void dsBrazier(int bx, int by) {                         // brasero du donjon : pilier de pierre et flamme
  float t = millis() / 1000.0f;
  fb.fillRect(bx - 5, by - 30, 10, 30, RGB565(72, 62, 80)); fb.drawFastVLine(bx - 5, by - 30, 30, RGB565(36, 30, 44));
  fb.fillTriangle(bx - 12, by - 40, bx + 12, by - 40, bx, by - 28, RGB565(96, 84, 104));
  int sw = (int)(sinf(t * 9) * 2);
  fb.fillTriangle(bx - 8, by - 40, bx + 8, by - 40, bx + sw, by - 66, RGB565(236, 70, 24));
  fb.fillTriangle(bx - 5, by - 40, bx + 5, by - 40, bx + sw, by - 58, RGB565(255, 160, 40));
  fb.fillTriangle(bx - 2, by - 40, bx + 2, by - 40, bx + sw, by - 50, RGB565(255, 235, 140));
}
void dsHouse(int x, int by) {                            // maison (planche exterieur)
  fb.fillRect(x, by - 26, 42, 26, RGB565(236, 218, 186)); fb.drawRect(x, by - 26, 42, 26, RGB565(150, 110, 80));
  fb.fillTriangle(x - 5, by - 26, x + 47, by - 26, x + 21, by - 50, RGB565(200, 62, 60)); fb.drawLine(x - 5, by - 26, x + 21, by - 50, RGB565(140, 36, 40));
  fb.fillRect(x + 31, by - 52, 5, 12, RGB565(150, 150, 160));
  fb.fillRect(x + 5, by - 20, 9, 9, RGB565(150, 200, 236)); fb.drawRect(x + 5, by - 20, 9, 9, RGB565(120, 80, 56));
  fb.fillRect(x + 28, by - 20, 9, 9, RGB565(150, 200, 236)); fb.drawRect(x + 28, by - 20, 9, 9, RGB565(120, 80, 56));
  fb.fillRect(x + 17, by - 16, 9, 16, RGB565(130, 80, 50));
}
void dsIceSpike(int x, int by, char k) {                 // pics de glace (memes zones que les cactus)
  const uint16_t B = RGB565(138, 200, 244), L = RGB565(226, 246, 255), D = RGB565(70, 120, 186);
  auto cr = [&](int bx, int w, int h) { fb.fillTriangle(bx, by, bx + w, by, bx + w / 2, by - h, B); fb.fillTriangle(bx + w / 2, by - h, bx + w / 2, by, bx + w, by, D); fb.drawLine(bx + 1, by - 1, bx + w / 2, by - h + 1, L); };
  if (k == 'c') { cr(x + 1, 6, 8); cr(x + 5, 7, 10); cr(x + 10, 5, 7); }
  else if (k == 'k') { cr(x + 3, 10, 30); cr(x - 1, 6, 14); cr(x + 12, 6, 12); }
  else { cr(x + 1, 7, 24); cr(x + 8, 8, 28); }
}
void dsFlameJet(int x, int by, char k) {                 // jets de flamme du donjon (memes zones que les cactus)
  float t = millis() / 1000.0f;
  int h = k == 'k' ? 28 : k == 'c' ? 9 : 24, n = k == 'm' ? 2 : 1;
  fb.fillRect(x + 2, by - 3, 12, 3, RGB565(60, 50, 66));
  for (int i = 0; i < n; i++) {
    int bx = x + (n == 2 ? 4 + i * 8 : 8), hh = h - i * 4, sw = (int)(sinf(t * 11 + x + i) * 2);
    fb.fillTriangle(bx - 5, by - 3, bx + 5, by - 3, bx + sw, by - hh, RGB565(226, 60, 22));
    fb.fillTriangle(bx - 3, by - 3, bx + 3, by - 3, bx + sw, by - hh + 4, RGB565(255, 150, 36));
    fb.fillTriangle(bx - 1, by - 3, bx + 1, by - 3, bx + sw, by - hh / 2, RGB565(255, 232, 130));
  }
}
void dsTrapObj(int x, int by, bool up, bool revealed) {  // pic qui surgit (up) / pic cache (revealed) : meme dessin, contour pour le cache
  if (!up && !revealed) return;
  fb.fillTriangle(x + 3, by, x + 13, by, x + 8, by - 14, COL_SPK); fb.drawTriangle(x + 3, by, x + 13, by, x + 8, by - 14, COL_MEATD);
}
void dsDecor2(int x, int by, char ch) {                  // decor des mondes glace (2), donjon (3), exterieur (4)
  float t = millis() / 1000.0f;
  const uint16_t SN = RGB565(244, 250, 255), SNS = RGB565(196, 220, 244);
  if (dTheme == 2) {
    switch (ch) {
      case 'g': fb.fillCircle(x + 5, by - 3, 4, SN); fb.fillCircle(x + 11, by - 3, 5, SN); fb.drawFastHLine(x + 2, by - 1, 12, SNS); break;
      case 'r': fb.fillRoundRect(x, by - 11, 16, 11, 5, RGB565(170, 206, 236)); fb.fillRoundRect(x + 2, by - 11, 9, 5, 3, SN); break;
      case 'u': fb.fillCircle(x + 5, by - 5, 5, SN); fb.fillCircle(x + 11, by - 6, 6, SN); fb.fillRect(x + 4, by - 4, 8, 4, RGB565(70, 130, 110)); fb.drawPixel(x + 7, by - 8, RGB565(220, 60, 70)); break;
      case 'b': fb.fillTriangle(x + 2, by, x + 6, by, x + 4, by - 9, RGB565(150, 206, 244)); fb.fillTriangle(x + 8, by, x + 13, by, x + 10, by - 6, RGB565(150, 206, 244)); break;
      case 'F': fb.fillTriangle(x + 1, by, x + 7, by, x + 4, by - 16, RGB565(138, 200, 244)); fb.fillTriangle(x + 6, by, x + 13, by, x + 10, by - 22, RGB565(110, 176, 232)); fb.fillTriangle(x + 11, by, x + 16, by, x + 14, by - 10, RGB565(168, 220, 250)); break;
      case 'W': {                                         // feu de camp : bûches + flamme (zone de chaleur, point de reprise)
        fb.fillRect(x + 2, by - 4, 12, 3, RGB565(110, 70, 40)); fb.drawLine(x + 3, by - 1, x + 13, by - 5, RGB565(90, 54, 30));
        int sw = (int)(sinf(t * 10 + x) * 2);
        fb.fillTriangle(x + 3, by - 4, x + 13, by - 4, x + 8 + sw, by - 22, RGB565(232, 66, 24)); fb.fillTriangle(x + 5, by - 4, x + 11, by - 4, x + 8 + sw, by - 17, RGB565(255, 160, 40));
        fb.fillTriangle(x + 7, by - 4, x + 9, by - 4, x + 8 + sw, by - 11, RGB565(255, 236, 150)); break; }
      default: break;
    }
  } else if (dTheme == 3) {
    switch (ch) {
      case 'r': fb.fillRoundRect(x + 1, by - 8, 14, 8, 3, RGB565(78, 70, 86)); fb.fillRoundRect(x + 4, by - 11, 8, 5, 2, RGB565(96, 88, 106)); break;
      case 's': fb.fillRoundRect(x + 2, by - 7, 12, 7, 3, RGB565(214, 206, 190)); fb.fillRect(x + 4, by - 5, 2, 2, RGB565(30, 20, 20)); fb.fillRect(x + 9, by - 5, 2, 2, RGB565(30, 20, 20)); break;
      case 'b': fb.drawLine(x + 2, by - 1, x + 12, by - 5, RGB565(214, 206, 190)); fb.drawLine(x + 3, by - 5, x + 13, by - 1, RGB565(196, 188, 172)); fb.fillCircle(x + 3, by - 2, 2, RGB565(214, 206, 190)); break;
      case 'g': for (int i = 0; i < 4; i++) fb.drawPixel(x + 2 + i * 4, by - 2 - (int)(fabsf(sinf(t * 3 + i)) * 6), RGB565(255, 150, 40)); break;
      case 'f': {                                         // torche murale : source de chaleur
        fb.fillRect(x + 6, by - 20, 4, 16, RGB565(86, 62, 44)); fb.fillRect(x + 4, by - 22, 8, 4, RGB565(60, 50, 56));
        int sw = (int)(sinf(t * 9 + x) * 2);
        fb.fillTriangle(x + 4, by - 22, x + 12, by - 22, x + 8 + sw, by - 38, RGB565(232, 66, 24)); fb.fillTriangle(x + 6, by - 22, x + 10, by - 22, x + 8 + sw, by - 33, RGB565(255, 170, 44)); break; }
      default: break;
    }
  } else {
    switch (ch) {
      case 'g': for (int i = 0; i < 5; i++) fb.drawLine(x + 3 + i * 3, by, x + 2 + i * 3 + (i & 1) * 2, by - 6 + (i % 3), RGB565(70, 160, 56)); break;
      case 'u': fb.fillCircle(x + 5, by - 5, 5, RGB565(58, 140, 46)); fb.fillCircle(x + 11, by - 6, 6, RGB565(78, 168, 54)); fb.fillCircle(x + 8, by - 9, 4, RGB565(104, 194, 70)); break;
      case 'r': fb.fillRoundRect(x - 2, by - 12, 20, 12, 5, RGB565(206, 222, 238)); fb.fillRoundRect(x + 2, by - 14, 11, 7, 3, RGB565(236, 244, 252)); fb.drawRoundRect(x - 2, by - 12, 20, 12, 5, RGB565(146, 168, 196)); break;
      case 'o': fb.fillRoundRect(x + 2, by - 13, 12, 13, 3, RGB565(168, 104, 58)); fb.fillRect(x + 2, by - 10, 12, 2, RGB565(86, 58, 40)); fb.fillRect(x + 2, by - 4, 12, 2, RGB565(86, 58, 40)); fb.drawRoundRect(x + 2, by - 13, 12, 13, 3, RGB565(110, 70, 40)); break;
      case 'e': fb.fillRect(x + 1, by - 14, 3, 14, RGB565(176, 112, 64)); fb.fillRect(x + 12, by - 14, 3, 14, RGB565(176, 112, 64)); fb.fillRect(x, by - 11, 16, 3, RGB565(196, 130, 76)); fb.fillRect(x, by - 5, 16, 3, RGB565(196, 130, 76)); break;
      case 'N': dsHouse(x - 8, by); break;
      default: break;
    }
  }
}
void dsGuy2(int cx, int by, bool right, bool raised) {   // lanceurs des mondes glace (yeti), donjon (diablotin), exterieur (gobelin)
  int s = right ? 1 : -1;
  if (dTheme == 2) {
    const uint16_t F = RGB565(232, 242, 252), FD = RGB565(160, 190, 220), FACE = RGB565(90, 110, 150);
    fb.fillRoundRect(cx - 6, by - 11, 12, 11, 5, F); fb.fillCircle(cx, by - 14, 6, F); fb.fillRoundRect(cx - 4, by - 16, 8, 5, 2, FACE);
    fb.fillRect(cx - 3, by - 15, 2, 2, COL_TXT); fb.fillRect(cx + 1, by - 15, 2, 2, COL_TXT); fb.drawRoundRect(cx - 6, by - 11, 12, 11, 5, FD);
    if (raised) { fb.fillRect(cx + s * 5 - 1, by - 20, 3, 8, F); fb.fillCircle(cx + s * 5, by - 22, 3, RGB565(255, 255, 255)); } else fb.fillRect(cx + s * 5 - 1, by - 8, 3, 6, F);
  } else if (dTheme == 3) {
    const uint16_t R = RGB565(196, 52, 40), RD = RGB565(110, 28, 30);
    fb.fillRoundRect(cx - 5, by - 10, 10, 10, 4, R); fb.fillCircle(cx, by - 13, 5, R);
    fb.fillTriangle(cx - 5, by - 16, cx - 2, by - 16, cx - 4, by - 22, RD); fb.fillTriangle(cx + 2, by - 16, cx + 5, by - 16, cx + 4, by - 22, RD);
    fb.fillRect(cx - 3, by - 14, 2, 2, RGB565(255, 230, 60)); fb.fillRect(cx + 1, by - 14, 2, 2, RGB565(255, 230, 60));
    fb.drawLine(cx - s * 5, by - 4, cx - s * 10, by - 10, RD);
    if (raised) { fb.fillRect(cx + s * 4, by - 19, 3, 7, R); fb.fillCircle(cx + s * 5, by - 21, 3, RGB565(255, 160, 40)); } else fb.fillRect(cx + s * 4, by - 8, 3, 5, R);
  } else {
    const uint16_t G = RGB565(112, 176, 72), GD = RGB565(64, 112, 44);
    fb.fillRoundRect(cx - 5, by - 9, 10, 9, 3, RGB565(120, 80, 50)); fb.fillCircle(cx, by - 12, 5, G);
    fb.fillTriangle(cx - 5, by - 13, cx - 10, by - 15, cx - 5, by - 10, G); fb.fillTriangle(cx + 5, by - 13, cx + 10, by - 15, cx + 5, by - 10, G);
    fb.fillRect(cx - 5, by - 18, 10, 4, RGB565(140, 70, 50)); fb.fillRect(cx - 3, by - 13, 2, 2, RGB565(30, 20, 20)); fb.fillRect(cx + 1, by - 13, 2, 2, RGB565(30, 20, 20)); fb.drawFastHLine(cx - 2, by - 9, 4, GD);
    if (raised) { fb.fillRect(cx + s * 4, by - 18, 3, 7, G); fb.fillCircle(cx + s * 5, by - 20, 2, RGB565(120, 110, 100)); } else fb.fillRect(cx + s * 4, by - 8, 3, 5, G);
  }
}
// fonds : exterieur (ciel, nuages, collines), glace (ciel pale, montagnes enneigees, neige qui tombe), donjon (briques, arches, chaines)
const uint8_t MOSB[10] = { 0, 24, 24, 12, 8, 8, 4, 2, 1, 1 };   // taille des "pixels" du decor exterieur selon le niveau (diviseurs de 120 : pas de raccord entre bandes)
void dMosaic(int x0, int y0, int x1, int y1, int B, int sTop) {   // pixelise un rectangle de la bande (grille calee sur l'ecran)
  if (B <= 1) return;
  uint16_t *b = fb.getBuffer();
  x0 = x0 >= 0 ? x0 / B * B : 0; x1 = min(DVW, x1); y0 = max(0, (y0 + sTop) / B * B - sTop); y1 = min(DSH, y1);
  for (int by = y0; by < y1; by += B) for (int bx = x0; bx < x1; bx += B) {
    int ry1 = min(by + B, DSH), rx1 = min(bx + B, DVW);
    uint16_t c = b[min(by + B / 2, ry1 - 1) * SCREEN_W + min(bx + B / 2, rx1 - 1)];
    for (int y = by; y < ry1; y++) { uint16_t *p = b + y * SCREEN_W; for (int x = bx; x < rx1; x++) p[x] = c; }
  }
}
void dSkyO(int sTop) {
  uint16_t *b = fb.getBuffer(); int camX = (int)dcamX, camY = (int)dcamY;
  for (int row = 0; row < DSH; row++) {
    int y = sTop + row; uint16_t c = dBuild < 2 ? 0 : dMix(RGB565(110, 184, 236), RGB565(196, 230, 246), y * 256 / DVH);   // 1-2 : fond noir
    uint32_t v = (uint32_t)c | ((uint32_t)c << 16), *p = (uint32_t *)(b + row * SCREEN_W);
    for (int q = 0; q < DVW / 2; q++) p[q] = v;
  }
  if (dBuild == 1) {                                      // niveau 2 : quelques gros pixels de decor sur le noir
    static const uint16_t PC[4] = { RGB565(70, 160, 70), RGB565(90, 170, 230), RGB565(230, 230, 240), RGB565(240, 200, 70) };
    for (int i = 0; i < 18; i++) {
      uint32_t q = gHash(i * 977 + 13); int x = (((int)(q % 900) - camX * 3 / 10) % 900 + 900) % 900 - 60, y = (12 + (int)((q >> 12) % (DVH - 70))) / 8 * 8 - sTop;
      fb.fillRect(x / 8 * 8, y, 8, 8, PC[(q >> 20) & 3]);
    }
  }
  if (dBuild < 2) return;                                 // 3+ : ciel, nuages, collines, en gros pixels de moins en moins gros
  int offC = (camX * 15 / 100) % 120;
  for (int k = -1; k < DVW / 120 + 2; k++) {              // nuages
    int cx = k * 120 - offC + ((k * 23) & 31), cy = 22 + ((k * 41) & 31) - sTop;
    fb.fillRoundRect(cx, cy, 48, 12, 6, RGB565(250, 252, 255)); fb.fillCircle(cx + 14, cy, 9, RGB565(250, 252, 255)); fb.fillCircle(cx + 30, cy + 1, 7, RGB565(250, 252, 255));
  }
  int off = (camX * 3 / 10) % HILL_W, base = DVH + 8 - (camY - ((DGY + 1) * TILE - DVH) + 48) * 3 / 10 - sTop;    // collines de la planche : sans raccord, defilent a 30 %
  if (base < DSH) { int fy = max(0, base); uint16_t fc = HILL_PAL[HILL_IDX[(HILL_H - 1) * HILL_W]]; fb.fillRect(0, fy, DVW, DSH - fy, fc); }
  for (int x0 = -off; x0 < DVW; x0 += HILL_W) dBlitIdx(HILL_IDX, HILL_PAL, HILL_W, HILL_H, x0, base - HILL_H);
  dMosaic(0, 0, DVW, DSH, MOSB[min(dBuild, 9)], sTop);
}
void dSkyI(int sTop) {
  uint16_t *b = fb.getBuffer(); int camX = (int)dcamX, camY = (int)dcamY;
  for (int row = 0; row < DSH; row++) {
    int y = sTop + row; uint16_t c = dMix(RGB565(150, 196, 232), RGB565(224, 238, 250), y * 256 / DVH);
    uint32_t v = (uint32_t)c | ((uint32_t)c << 16), *p = (uint32_t *)(b + row * SCREEN_W);
    for (int q = 0; q < DVW / 2; q++) p[q] = v;
  }
  int off = (camX * 25 / 100) % MTN_W, base = DVH + 4 - (camY - ((DGY + 1) * TILE - DVH) + 48) * 3 / 10 - sTop;      // montagnes de la planche : se repetent sans raccord, defilent a 25 %
  if (base < DSH) { int fy = max(0, base); uint16_t fc = MTN_PAL[MTN_IDX[(MTN_H - 1) * MTN_W]]; fb.fillRect(0, fy, DVW, DSH - fy, fc); }   // sous le sprite : meme teinte
  for (int x0 = -off; x0 < DVW; x0 += MTN_W) dBlitIdx(MTN_IDX, MTN_PAL, MTN_W, MTN_H, x0, base - MTN_H);
  unsigned long m = millis();
  for (int i = 0; i < 26; i++) {                           // neige qui tombe (position fixe a l'ecran)
    int x = (i * 53 + (int)(m / (60 + (i % 3) * 20)) + (int)(sinf(m / 700.0f + i) * 5)) % DVW, y = (i * 29 + (int)(m / (22 + (i % 4) * 6))) % DVH - sTop;
    if (y >= 0 && y < DSH) fb.fillRect(x, y, 2, 2, RGB565(255, 255, 255));
  }
}
void dSkyF(int sTop) {
  uint16_t *b = fb.getBuffer(); int camX = (int)dcamX;
  for (int row = 0; row < DSH; row++) {
    int y = sTop + row; uint16_t c = dMix(RGB565(22, 10, 16), RGB565(72, 26, 22), y * 256 / DVH);
    uint32_t v = (uint32_t)c | ((uint32_t)c << 16), *p = (uint32_t *)(b + row * SCREEN_W);
    for (int q = 0; q < DVW / 2; q++) p[q] = v;
  }
  const uint16_t M = RGB565(14, 6, 10);
  int off = (camX * 5 / 10) % 28;
  for (int yy = 8; yy < DVH; yy += 14) {                   // joints de briques (parallaxe)
    int y = yy - sTop; if (y < 0 || y >= DSH) continue;
    fb.drawFastHLine(0, y, DVW, M);
    int shift = ((yy / 14) & 1) * 14, hh = min(14, DSH - y);
    for (int x = -((off + shift) % 28); x < DVW; x += 28) fb.drawFastVLine(x, y, hh, M);
  }
  int off2 = (camX * 6 / 10) % 112;
  for (int k = -1; k < DVW / 112 + 2; k++) {               // arches sombres
    int ax = k * 112 - off2 + 30;
    fb.fillRoundRect(ax, 44 - sTop, 40, 130, 20, M); fb.drawRoundRect(ax - 1, 43 - sTop, 42, 131, 20, RGB565(52, 22, 22));
    fb.fillRoundRect(ax + 6, 96 - sTop, 28, 70, 14, RGB565(40, 14, 12));
  }
  { int off = (camX * 35 / 100) % LAVA_W, base = DVH + 6 - ((int)dcamY - ((DGY + 1) * TILE - DVH) + 48) * 3 / 10 - sTop;      // roches a lave de la planche
    if (base < DSH) { int fy = max(0, base); uint16_t fc = LAVA_PAL[LAVA_IDX[(LAVA_H - 1) * LAVA_W]]; fb.fillRect(0, fy, DVW, DSH - fy, fc); }
    for (int x0 = -off; x0 < DVW; x0 += LAVA_W) dBlitIdx(LAVA_IDX, LAVA_PAL, LAVA_W, LAVA_H, x0, base - LAVA_H); }
  for (int k = 0; k < DVW / 64 + 2; k++) {                 // chaines
    int x = k * 64 - (camX * 7 / 10) % 64 + 20;
    for (int y = 0; y < DVH; y += 6) { int yy = y - sTop; if (yy >= 0 && yy < DSH) fb.fillRect(x, yy, 2, 3, RGB565(84, 76, 88)); }
  }
}
void gCorrupt(int x, int y, int w, int h, uint32_t seed) {   // decale / recolore quelques lignes d'un rectangle de la bande (glitch)
  uint16_t *b = fb.getBuffer(); uint32_t fr = millis() / 90;
  if (x < 2) { w -= 2 - x; x = 2; }
  if (x + w > DVW - 2) w = DVW - 2 - x;
  if (w <= 2) return;
  for (int r = max(0, y); r < min(DSH, y + h); r++) {
    uint32_t q = gHash(seed * 977 + (r - y) / 2 * 31 + fr * 7919);
    if ((q & 7) > 1) continue;
    uint16_t *p = b + r * SCREEN_W + x;
    if (q & 8) memmove(p + 2, p, (w - 2) * 2); else memmove(p, p + 2, (w - 2) * 2);
    if (q & 16) for (int i = 0; i < w; i++) { uint16_t c = p[i]; p[i] = (uint16_t)((c << 11) | (c & 0x07E0) | (c >> 11)); }
  }
}
void gZonePass(int sTop, int zx, int x1, int thr) {     // zone corrompue [zx, x1) : morceaux d'ecran (cases de 32x16 du monde) decales, couleurs inversees, parasites
  if (x1 > DVW) x1 = DVW;
  if (zx >= x1 - 8) return;
  uint16_t *b = fb.getBuffer(); uint32_t fr = millis() / 70; int wy = (int)dcamY + sTop, wx = (int)dcamX;
  for (int bc = (wx + zx) >> 5; bc <= (wx + x1 - 1) >> 5; bc++)
    for (int br = wy >> 4; br <= (wy + DSH - 1) >> 4; br++) {
      uint32_t h = gHash(bc * 7349 + br * 131 + fr * 7919);
      if ((int)(h & 1023) >= thr) continue;
      int c0 = max(zx, bc * 32 - wx + (int)((h >> 14) & 7)), c1 = min(x1, c0 + 12 + (int)((h >> 17) & 15));   // morceau de largeur 12..27
      int r0 = max(0, br * 16 - wy + (int)((h >> 21) & 3)), r1 = min(DSH, br * 16 - wy + 10 + (int)((h >> 23) & 7));
      if (c1 - c0 < 6) continue;
      int mode = (h >> 10) & 3;
      for (int row = r0; row < r1; row++) {
        uint16_t *p = b + row * SCREEN_W; uint32_t q = gHash(h + row);
        int sh = (int)(q & 7) - 3;
        if (mode == 0 && sh) { if (sh > 0) memmove(p + c0 + sh, p + c0, (c1 - c0 - sh) * 2); else memmove(p + c0, p + c0 - sh, (c1 - c0 + sh) * 2); }
        else if (mode == 1) for (int x = c0; x < c1; x++) { uint16_t c = p[x]; p[x] = (uint16_t)((c << 11) | (c & 0x07E0) | (c >> 11)); }
        else if (mode == 2) for (int x = c0; x < c1; x++) p[x] = ~p[x];
        else for (int x = c0; x < c1 - 1; x += 4) { q = q * 1103515245u + 12345u; if ((q >> 28) < 3) p[x] = p[x + 1] = (q >> 16) & 1 ? 0xF81F : 0x07FF; }
      }
    }
}
void dsGlitchGuy(int cx, int by, bool right, bool raised) {   // ennemi glitch : amas de pixels qui clignotent, yeux rouges
  static const uint16_t GC[4] = { RGB565(255, 40, 200), RGB565(40, 240, 255), RGB565(120, 255, 60), RGB565(250, 250, 250) };
  uint32_t f = millis() / 90 + cx * 7;
  for (int j = 0; j < 4; j++) for (int i = 0; i < 3; i++) {
    uint32_t h = gHash(i + j * 3 + f * 13); if ((h & 7) == 0) continue;
    int ox = ((h >> 3) & 3) == 0 ? (((h >> 5) & 1) ? 2 : -2) : 0;
    fb.fillRect(cx - 6 + i * 4 + ox, by - 16 + j * 4, 4, 4, GC[(h >> 6) & 3]);
  }
  fb.fillRect(cx - 5, by - 13, 10, 5, RGB565(10, 8, 18));
  int e = right ? 1 : -1;
  fb.fillRect(cx - 3 + e, by - 12, 2, 3, RGB565(255, 40, 40)); fb.fillRect(cx + 1 + e, by - 12, 2, 3, RGB565(255, 40, 40));
  if (raised) fb.fillRect(cx + e * 7 - 1, by - 21, 3, 3, GC[f & 3]);
}
void sayAt(int cx, int y, const char *t, bool gl) {     // bulle rognee a l'ecran ; gl = voix de glitch (fond noir, texte dedouble, plus gros s'il tient)
  if (!gl) { int w = textW(fb, 1, t) + 10; drawBubble(constrain(cx, w / 2 + 2, DVW - w / 2 - 2), y, t); return; }
  int sz = textW(fb, 2, t) + 12 <= DVW - 4 ? 2 : 1, w = textW(fb, sz, t) + 12, bh = sz == 2 ? 22 : 13;
  cx = constrain(cx, w / 2 + 2, DVW - w / 2 - 2); y -= bh - 13;
  int j = (millis() / 60) & 1;
  fb.fillRect(cx - w / 2, y, w, bh, RGB565(16, 8, 24)); fb.drawRect(cx - w / 2 + j, y, w, bh, RGB565(255, 40, 200));
  fb.setTextColor(RGB565(40, 240, 255)); gfxPrintAt(fb, sz, cx - w / 2 + 7, y + 2, t);
  fb.setTextColor(RGB565(255, 60, 210)); gfxPrintAt(fb, sz, cx - w / 2 + 6 - j, y + 2, t);
}
void dBlitScale(int yScreen) {                         // bande logique DVW x DSH -> 320x120 (agrandissement quelconque, plus proche voisin)
  static int16_t xm[SCREEN_W]; static int xmW = 0;
  if (xmW != DVW) { for (int x = 0; x < SCREEN_W; x++) xm[x] = x * DVW / SCREEN_W; xmW = DVW; }
  uint16_t *b = fb.getBuffer();
  if (!dDma) {                                         // repli : sur place, de bas en haut et de droite a gauche (la source est toujours avant la destination)
    for (int y = STRIP_H - 1; y >= 0; y--) { uint16_t *d = b + y * SCREEN_W, *sr = b + (y * DSH / STRIP_H) * SCREEN_W; for (int x = SCREEN_W - 1; x >= 0; x--) d[x] = sr[xm[x]]; }
    tft.drawRGBBitmap(0, yScreen, b, SCREEN_W, STRIP_H); return;
  }
  dWaitDma();
  for (int y = 0; y < STRIP_H; y++) { uint16_t *d = dOb + y * SCREEN_W; const uint16_t *sr = b + (y * DSH / STRIP_H) * SCREEN_W; for (int x = 0; x < SCREEN_W; x++) d[x] = dSw(sr[xm[x]]); }
  dSendStrip(yScreen);
}
void dScaleInPlace() {                                 // agrandit la bande logique dans le tampon lui-meme (de bas en haut, de droite a gauche)
  static int16_t xm[SCREEN_W]; static int xmW = 0;
  if (xmW != DVW) { for (int x = 0; x < SCREEN_W; x++) xm[x] = x * DVW / SCREEN_W; xmW = DVW; }
  uint16_t *b = fb.getBuffer();
  for (int y = STRIP_H - 1; y >= 0; y--) { uint16_t *d = b + y * SCREEN_W, *sr = b + (y * DSH / STRIP_H) * SCREEN_W; for (int x = SCREEN_W - 1; x >= 0; x--) d[x] = sr[xm[x]]; }
}
void dBlitAny(int sTop) { if (DVW == SCREEN_W) dBlitPlain(sTop); else if (DVW == 256) dBlit(sTop * 5 / 4); else dBlitScale(sTop * STRIP_H / DSH); }
void sprDraw(Adafruit_GFX &g, const PAnim &A, const uint8_t *pix, const uint16_t *pal, int frame, int fx, int fy, bool right, int k, uint16_t solid = 0) {   // sprite 4 bits (PAnim), pieds en (fx, fy), echelle k ; solid != 0 : silhouette d'une couleur
  frame = ((frame % A.n) + A.n) % A.n;
  const uint8_t *px = pix + A.off + frame * ((A.w * A.h + 1) / 2);
  bool mir = right != (A.right != 0); int ax = mir ? A.w - 1 - A.ax : A.ax;
  for (int y = 0; y < A.h; y++) for (int x = 0; x < A.w; x++) {
    int i = y * A.w + x, v = (px[i >> 1] >> ((i & 1) ? 0 : 4)) & 15;
    if (v) g.fillRect(fx + ((mir ? A.w - 1 - x : x) - ax) * k, fy + (y - A.ay) * k, k, k, solid ? solid : pal[v]);
  }
}
void dShadeBox(int x0, int y0, int x1, int y1) {        // fond gris transparent (melange ~45 %) sous l'avatar et le sorcier pendant une question
  const uint16_t G = RGB565(120, 124, 136); uint16_t *b = fb.getBuffer();
  for (int y = max(0, y0); y < min(DSH, y1); y++) for (int x = max(0, x0); x < min(DVW, x1); x++) {
    uint16_t &p = b[y * SCREEN_W + x]; p = (y == y0 || y == y1 - 1 || x == x0 || x == x1 - 1) ? dMix(p, RGB565(220, 224, 236), 150) : dMix(p, G, 115); }
}
void dsWizard(int cx, int by, int px, int py) {          // le sorcier de la question (dQW), tourne vers le joueur (px, py) ; bonne reponse = il lance un sort sur lui
  int ai = WIZ_IDLE(dQW), atk = WIZ_ATK(dQW); unsigned long t0 = dQMs;     // idle en boucle pendant la question ; bonne reponse = son attaque, une fois, puis idle
  if (dQPh == 1 && quizOk) { unsigned long e = millis() - dQAMs, d = (unsigned long)WIZ_ANIM[atk].n * WIZ_MS[atk]; if (e < d) { ai = atk; t0 = dQAMs; } else t0 = dQAMs + d; }
  const PAnim &A = WIZ_ANIM[ai];
  sprDraw(fb, A, WIZ_PIX, WIZ_PAL[A.ch], (millis() - t0) / WIZ_MS[ai], cx, by, px > cx, 1);
  if (dQPh != 1 || !quizOk) return;
  static const uint16_t FX[6] = { 0, RGB565(150, 255, 170), RGB565(190, 90, 255), RGB565(255, 60, 90), RGB565(60, 240, 210), RGB565(255, 220, 80) };   // couleur du sort de chaque sorcier
  float t = (millis() - dQAMs) / 1000.0f - WIZ_ANIM[atk].n * WIZ_MS[atk] * 0.0006f; uint16_t c = FX[dQW], w = RGB565(255, 255, 255);   // le sort part aux 60 % de l'attaque
  if (t < 0) return;
  int sx = cx + (px > cx ? 10 : -10), sy = by - A.ay * 2 / 3;
  if (t < 0.7f) for (int i = 0; i < 6; i++) {             // l'orbe file du sorcier au joueur en ondulant, avec sa traînee
    float u = t / 0.7f - i * 0.05f; if (u < 0) break;
    int x = sx + (int)((px - sx) * u), y = sy + (int)((py - sy) * u - sinf(u * 3.1416f) * 22);
    int z = i ? 4 - i / 2 : 6; fb.fillRect(x - z / 2, y - z / 2, z, z, i ? c : w);
  } else if (t < 1.6f) {                                  // la transformation : anneau d'etincelles qui se resserre sur le joueur
    float r = 26 * (1.6f - t);
    for (int i = 0; i < 12; i++) { float a = i * 0.5236f + t * 6; fb.fillRect(px + (int)(cosf(a) * r) - 1, py + (int)(sinf(a) * r) - 1, 3, 3, (i & 1) ? c : w); }
    if (((millis() / 60) & 1)) fb.drawCircle(px, py, 12 - (int)(r / 3), c);
  }
}
void worldQuestionPanel(int ys) {                        // panneau de question, dessine APRES l'agrandissement (pleine resolution, police Chunky15) ; ys = haut de la bande a l'ecran
  const Quiz &q = QUIZZES[quizPick];
  const uint16_t VI = RGB565(110, 70, 220);
  int px = 4, pw = SCREEN_W - 8, py = 4 - ys, a0 = q.q2[0] ? 50 : 28;
  int ph = dQPh == 0 ? a0 + 3 * 25 + 4 : 84;
  fb.fillRoundRect(px, py, pw, ph, 8, COL_TXT); fb.drawRoundRect(px, py, pw, ph, 8, VI);
  int tx = constrain(((int)dQX - (int)dcamX) * SCREEN_W / DVW, px + 12, px + pw - 12);
  fb.fillTriangle(tx - 7, py + ph - 1, tx + 7, py + ph - 1, tx, py + ph + 12, COL_TXT);
  if (dQPh == 0) {
    fb.setTextColor(RGB565(80, 40, 170)); gfxPrintAt(fb, 2, px + 6, py + 4, q.q1); if (q.q2[0]) gfxPrintAt(fb, 2, px + 6, py + 26, q.q2);
    for (int i = 0; i < 3; i++) {
      int oi = (i + quizRot) % 3, yy = py + a0 + i * 25;
      if (i == quizChoice) fb.fillRoundRect(px + 3, yy - 2, pw - 6, 24, 6, VI);
      fb.setTextColor(i == quizChoice ? COL_TXT : COL_BG); gfxPrintAt(fb, 2, px + 10, yy, q.opt[oi]);
    }
  } else {
    char b[48];
    if (quizOk && evoUp) {                               // stade gagne : on le fete
      bool bl = (millis() / 200) & 1;
      fb.drawRoundRect(px + 1, py + 1, pw - 2, ph - 2, 7, bl ? RGB565(255, 210, 60) : VI); fb.drawRoundRect(px + 2, py + 2, pw - 4, ph - 4, 6, bl ? RGB565(255, 210, 60) : VI);
      fb.setTextColor(bl ? RGB565(230, 140, 20) : RGB565(20, 120, 50)); snprintf(b, sizeof(b), "EVOLUTION %d !", evo); gfxPrintAt(fb, 2, px + 8, py + 6, b);
      fb.setTextColor(COL_BG); snprintf(b, sizeof(b), "Tu deviens : %s", evoName(evo)); gfxPrintAt(fb, textW(fb, 2, b) > pw - 16 ? 1 : 2, px + 8, py + 32, b); }
    else if (quizOk) { fb.setTextColor(RGB565(20, 120, 50)); gfxPrintAt(fb, 2, px + 8, py + 6, "Bravo ! +3 pieces");
      fb.setTextColor(COL_BG); int r = EVO_STEP - evoPts; snprintf(b, sizeof(b), "Encore %d pour evoluer", r); gfxPrintAt(fb, 2, px + 8, py + 32, b); }
    else { fb.setTextColor(RGB565(170, 30, 30)); gfxPrintAt(fb, 2, px + 8, py + 6, "Rate ! C'etait :");
      fb.setTextColor(COL_BG); gfxPrintAt(fb, 2, px + 8, py + 32, q.opt[q.correct]); }
    { char e[24]; snprintf(e, sizeof(e), "EVOLUTION %d/%d", evo, EVO_MAX);   // numero d'evolution + barre des 4 bonnes reponses
      int bx = px + pw - 8 - 4 * 13, n = evoUp ? EVO_STEP : evoPts;
      for (int i = 0; i < EVO_STEP; i++) { fb.fillRect(bx + i * 13, py + 56, 11, 7, i < n ? RGB565(255, 200, 40) : RGB565(210, 210, 225)); fb.drawRect(bx + i * 13, py + 56, 11, 7, VI); }
      fb.setTextColor(RGB565(110, 70, 220)); gfxPrintAt(fb, 1, px + pw - 8 - textW(fb, 1, e), py + 68, e); }
    fb.setTextColor(RGB565(90, 100, 120)); gfxPrintAt(fb, 1, px + 8, py + 68, "SAUT = continuer");
  }
}
void drawWorld() {
  const uint16_t GOLD = RGB565(255, 205, 60), GOLDD = RGB565(190, 130, 20);
  int cx = (int)dcamX;
  float tm = millis() / 1000.0f;
  for (int sTop = 0; sTop < DVH; sTop += DSH) {
#ifdef DESERT_BENCH
    tq = micros();
#endif
    int cy = (int)dcamY + sTop;
    if (dTheme == 0) dSky(sTop); else if (dTheme == 1) dSkyJ(sTop); else if (dTheme == 2) dSkyI(sTop); else if (dTheme == 3) dSkyF(sTop); else dSkyO(sTop);   // un fond par monde
#ifdef DESERT_BENCH
    bT[6] += micros() - tq; tq = micros();
#endif
    if (dTheme == 0) BT(2, dShadePass(sTop));              // rayons en biais / ombres sur le fond
    int c0 = max(0, cx / TILE - 1), c1 = min(DW - 1, (cx + DVW) / TILE + 1);
    int r0 = max(0, cy / TILE - 1), r1 = min(DH - 1, (cy + DSH) / TILE + 1);
    for (int ty = r0; ty <= r1; ty++) for (int tx = c0; tx <= c1; tx++) {   // blocs
      char ch = dg[ty][tx]; int x = tx * TILE - cx, y = ty * TILE - cy;
      const uint16_t (*TS)[256] = dTileSpr; int svT = dTheme;
      if (tMix) { dTheme = colTheme(tx); if (dTheme != tA) TS = dTileSprB; }
      if (ch == 'S' || ch == 'h') {                        // h = faux sol (trou cache) : meme dessin, pointille de pres
        bool top = ty == 0 || (dg[ty - 1][tx] != 'S' && dg[ty - 1][tx] != 'h');
        int g = (tx * 7 + ty * 13) & 1, v = top ? (g ? 4 : 0) : (dTheme == 3 ? 2 : (g ? 5 : 2));
        dSpr(TS[v], x, y);
        if (top && dTheme == 0 && ch == 'S') dShadeTile(x, y, tx, ty);   // ombre au sol, au pixel pres
        if (ch == 'h' && ty == DGY && abs((int)dpx + 6 - (tx * TILE + 8)) < 52) for (int d = 0; d < TILE; d += 2) { fb.drawPixel(x + d, y, COL_MEATD); fb.drawPixel(x, y + d, COL_MEATD); fb.drawPixel(x + TILE - 1, y + d, COL_MEATD); }
      } else if (ch == 'X') dSpr(TS[3], x, y);
      else if (ch == 'L') { dSpr(TS[ty == DGY ? 1 : 5], x, y); if (ty == DGY) fb.drawFastHLine(x + ((int)(tm * 22) + tx * 5) % 12, y + 1, 4, RGB565(255, 240, 150)); }
      else if (ch == 'Q') { fb.fillRect(x, y, TILE, TILE, RGB565(10, 6, 16)); uint32_t h = gHash(tx * 31 + ty * 7 + (millis() / 80) * 977); for (int k = 0; k < 4; k++, h >>= 7) fb.fillRect(x + (h & 3) * 4, y + ((h >> 2) & 3) * 4, 4, 4, (h & 32) ? 0xF81F : 0x07FF); }
      else if (ch == 'z') {                               // plateforme glitch : pleine, puis clignote, puis absente (contour pointille)
        int ph = zPh(ty);
        if (ph < 2400 || (ph < 3000 && ((millis() / 70) & 1))) { fb.fillRect(x, y, TILE, TILE, RGB565(40, 12, 56)); fb.drawRect(x, y, TILE, TILE, 0xF81F); uint32_t h = gHash(tx * 17 + ty * 5 + (millis() / 80) * 977); for (int k = 0; k < 3; k++, h >>= 7) fb.fillRect(x + 2 + (h & 3) * 3, y + 2 + ((h >> 2) & 3) * 3, 4, 4, (h & 32) ? 0x07FF : RGB565(250, 250, 250)); }
        else for (int d = 0; d < TILE; d += 4) { fb.drawPixel(x + d, y, 0xF81F); fb.drawPixel(x + d, y + TILE - 1, 0xF81F); }
      }
      else if (ch == 'Z') { fb.fillRect(x, y, TILE, TILE, RGB565(40, 12, 56)); fb.drawRect(x, y, TILE, TILE, 0xF81F); uint32_t h = gHash(tx * 17 + ty * 5 + (millis() / 80) * 977); for (int k = 0; k < 3; k++, h >>= 7) fb.fillRect(x + 2 + (h & 3) * 3, y + 2 + ((h >> 2) & 3) * 3, 4, 4, (h & 32) ? 0x07FF : RGB565(250, 250, 250)); }
      dTheme = svT;
    }
    if (dHumpOn) dHumpDraw(cx, cy);
    for (int i = 0; i < nDr; i++) if (dDr[i].st == 1) dSpr(dTileSpr[3], dDr[i].tx * TILE - cx, (int)dDr[i].y - cy);   // brique en chute
    for (int i = 0; i < 4; i++) if (gH[i].on && !gH[i].open && ((millis() / 90) & 1)) {   // alerte : la case clignote avant de s'ouvrir / de surgir
      int x = gH[i].c * TILE - cx, y = (gH[i].up ? DGY - 1 : DGY) * TILE - cy;
      fb.drawRect(x, y, TILE, TILE, 0xF81F); fb.drawRect(x + 1, y + 1, TILE - 2, TILE - 2, 0x07FF);
      if (gH[i].up && (dVar >= 4 || dTheme != 4)) { fb.drawRect(x, y - TILE, TILE, TILE, 0xF81F); }
    }
#ifdef DESERT_BENCH
    bT[7] += micros() - tq; tq = micros();
#endif
    for (int ty = r0; ty <= min(DH - 1, r1 + 7); ty++) for (int tx = max(0, c0 - 4); tx <= min(DW - 1, c1 + 4); tx++) {   // decor, cactus, arbres (r1+7 : un arbre dont le pied est sous la bande y monte)
      char ch = dg[ty][tx]; if (ch == '.' || ch == 'S' || ch == 'X' || ch == 'h' || ch == 'L' || ch == 'Q' || ch == 'Z' || ch == 'z' || ch == 'q') continue;
      int x = tx * TILE - cx, by = (ty + 1) * TILE - cy;
      if (dHumpOn && ty == DGY - 1 && (ch == 'g' || ch == 'u' || ch == 'b' || ch == 'r' || ch == 's')) by -= dHump[tx];   // decor pose sur la dune
      int svT = dTheme; if (tMix) dTheme = colTheme(tx);
      if (ch == 'T') { if (dTheme == 0) dsTree(x + 8, by); else if (dTheme == 1) dsJTree(x + 8, by); else if (dTheme == 2) dsPine(x + 8, by); else if (dTheme == 3) dsBrazier(x + 8, by); else { dsTreeSpr(tx % 3, x + 8, by); dMosaic(x - 24, by - 104, x + 40, by, MOSB[min(dBuild, 9)], sTop); } }
      else if (ch == 'k' || ch == 'c' || ch == 'm') { if (dTheme == 0) dsCactus(x, by, ch); else if (dTheme == 2) dsIceSpike(x, by, ch); else if (dTheme == 3) dsFlameJet(x, by, ch); else dsThorn(x, by, ch); }
      else if (ch == 'v') { bool up = false; for (int i = 0; i < nDTraps; i++) if (dTraps[i].tx == tx && dTraps[i].st == 1) up = true; fb.fillRect(x + 2, by - 3, 12, 3, RGB565(60, 54, 62)); fb.drawRect(x + 2, by - 3, 12, 3, RGB565(30, 26, 32)); dsTrapObj(x, by - 3, up, false); }
      else if (ch == 'y') { for (int i = 0; i < nDTraps; i++) if (dTraps[i].tx == tx && dTraps[i].st == 1) dsTrapObj(x, by, true, false); }   // pic cache : invisible tant qu'il n'est pas sorti
      else if (ch == 'x') dsTrapObj(x, by, false, abs((int)dpx + 6 - (tx * TILE + 8)) < 52 && abs((int)dpy - (ty * TILE - 8)) < 52);
      else if (ch == 'H') { int bob = (int)(sinf(tm * 4 + tx) * 2), hx = x + 8, hy = by - 8 + bob; const uint16_t R = RGB565(235, 50, 70);   // coeur
        fb.fillCircle(hx - 3, hy - 2, 4, R); fb.fillCircle(hx + 3, hy - 2, 4, R); fb.fillTriangle(hx - 7, hy, hx + 7, hy, hx, hy + 8, R);
        fb.drawPixel(hx - 4, hy - 4, COL_TXT); fb.drawPixel(hx - 5, hy - 3, COL_TXT); if ((int)(tm * 3 + tx) % 4 == 0) fb.drawFastHLine(hx - 10, hy - 8, 4, COL_TXT); }
      else if (ch == 'U') {                                 // etoile bleue scintillante (question)
        int bob = (int)(sinf(tm * 3 + tx) * 2), rr = 8 + (int)(sinf(tm * 7) * 1.5f), sx = x + 8, sy = by - 9 + bob;
        const uint16_t B1 = RGB565(70, 150, 255), B2 = RGB565(150, 210, 255);
        fb.fillTriangle(sx, sy - rr, sx - rr, sy + rr / 2, sx + rr, sy + rr / 2, B1); fb.fillTriangle(sx, sy + rr, sx - rr, sy - rr / 2, sx + rr, sy - rr / 2, B1);
        fb.fillTriangle(sx, sy - rr + 3, sx - rr / 2, sy + 1, sx + rr / 2, sy + 1, B2); fb.fillCircle(sx, sy, 2, COL_TXT);
        int ph = (int)(tm * 10 + tx * 3) % 14;                // eclats qui scintillent autour
        if (ph < 4) { int l = 3 + ph; fb.drawFastHLine(sx - rr - l, sy, l, COL_TXT); fb.drawFastHLine(sx + rr + 1, sy, l, COL_TXT); fb.drawFastVLine(sx, sy - rr - l, l, COL_TXT); }
        if ((int)(tm * 6 + tx) % 5 == 0) fb.drawPixel(sx + rr - 1, sy - rr + 1, COL_TXT);
        if ((int)(tm * 5 + tx * 7) % 7 == 0) fb.drawPixel(sx - rr + 2, sy + rr - 2, B2); }
      else if (ch == 'O' || ch == 'K') { int bob = (int)(sinf(tm * 4 + tx) * 1.5f); fb.fillCircle(x + 8, by - 8 + bob, 5, GOLD); fb.drawCircle(x + 8, by - 8 + bob, 5, GOLDD); fb.drawFastVLine(x + 8, by - 11 + bob, 6, GOLDD); fb.drawPixel(x + 6, by - 10 + bob, COL_TXT);
        if (ch == 'K' && (millis() + tx * 97) % 1700 < 120) { fb.fillRect(x + 3, by - 12 + bob, 4, 4, 0xF81F); fb.fillRect(x + 9, by - 7 + bob, 4, 4, 0x07FF); } }   // le faux scintille en glitch, rarement
      else if (ch == '*' || ch == 'Y') { int bob = (int)(sinf(tm * 3 + tx) * 2), rr = 6 + (int)(sinf(tm * 6) * 1); int sx = x + 8, sy = by - 8 + bob;
        fb.fillTriangle(sx, sy - rr, sx - rr, sy + rr / 2, sx + rr, sy + rr / 2, RGB565(255, 230, 70)); fb.fillTriangle(sx, sy + rr, sx - rr, sy - rr / 2, sx + rr, sy - rr / 2, RGB565(255, 230, 70)); fb.fillCircle(sx, sy, 2, COL_TXT);
        if (ch == 'Y' && (millis() + tx * 97) % 1900 < 120) { fb.fillRect(sx - 6, sy - 2, 4, 4, 0xF81F); fb.fillRect(sx + 3, sy - 5, 4, 4, 0x07FF); } }
      else dsDecor(x, by, ch);
      if (ch == 'P' && dChkX == tx * TILE + 2) fb.fillRect(x + 8, by - 28, 6, 4, RGB565(60, 190, 80));   // panneau actif : petit fanion
      dTheme = svT;
    }
    if (dTheme == 1 || (tCol >= 0 && tB == 1)) for (int i = 0; i < nDVines; i++) if (abs(dVines[i].ax - cx - DVW / 2) < DVW / 2 + 60) dsVine(i, cx, cy);   // lianes
#ifdef DESERT_BENCH
    bT[8] += micros() - tq; tq = micros();
#endif
    for (int i = 0; i < nDEn; i++) if (den[i].alive) {     // petits lanceurs : des glitchs (deguises en monstre du monde hors du couloir)
      int ex = (int)den[i].x - cx, ey = (int)den[i].y - cy, svT = dTheme;
      if (tMix) dTheme = colTheme((int)den[i].x / TILE);
      if (den[i].gl || dTheme == 4) dsGlitchGuy(ex, ey, den[i].right, den[i].cd < 0.4f);
      else if (dTheme == 0) { dsBallCactus(ex, ey, den[i].right, i); gCorrupt(ex - 11, ey - 30, 22, 30, i); }
      else { dsGuy(ex, ey, den[i].right, den[i].cd < 0.4f); gCorrupt(ex - 11, ey - 24, 22, 24, i); }
      dTheme = svT;
    }
    for (int k = 0; k < 8; k++) if (dpr[k].on) {
      int px = (int)dpr[k].x - cx, py = (int)dpr[k].y - cy;
      if (dpr[k].gl) {                                     // bloc corrompu : pointilles jusqu'au sol pour prevenir
        for (int yy = py + 8; yy < DSH && yy < DGY * TILE - cy; yy += 6) fb.drawPixel(px, yy, 0xF81F);
        uint32_t h = gHash(k * 17 + millis() / 70); fb.fillRect(px - 5, py - 5, 10, 10, (h & 1) ? 0xF81F : 0x07FF); fb.fillRect(px - 5 + (h >> 1 & 3), py - 3, 6, 3, RGB565(250, 250, 250));
      } else if (dTheme == 0 || dTheme == 4) { fb.fillCircle(px, py, 3, RGB565(120, 105, 90)); fb.drawCircle(px, py, 3, RGB565(70, 60, 50)); }
      else if (dTheme == 2) { fb.fillCircle(px, py, 4, RGB565(250, 252, 255)); fb.drawCircle(px, py, 4, RGB565(150, 180, 214)); }
      else if (dTheme == 3) { fb.fillCircle(px, py, 4, RGB565(255, 140, 30)); fb.fillCircle(px, py, 2, RGB565(255, 236, 130)); fb.drawPixel(px + ((millis() / 50) & 1 ? 3 : -3), py - 3, RGB565(255, 90, 20)); }
      else { fb.fillCircle(px, py, 4, RGB565(96, 62, 32)); fb.drawCircle(px, py, 4, RGB565(52, 34, 18)); fb.drawPixel(px - 1, py - 1, RGB565(190, 150, 100)); }
    }
    for (int i = 0; i < 16; i++) if (dpt[i].life > 0) { int z = dpt[i].s ? max(2, (int)(dpt[i].s * dpt[i].life / 0.45f + 0.5f)) : 2; fb.fillRect((int)dpt[i].x - cx, (int)dpt[i].y - cy, z, z, dpt[i].c); }
    gZonePass(sTop, gZone0 * TILE - cx, DVW, 8 + glitchLvl / 2);                              // fin de niveau corrompue
    if (tCol >= 0) gZonePass(sTop, max(0, (tCol - 3) * TILE - cx), (tCol + 3) * TILE - cx, 260);    // couture entre les deux mondes
    for (int i = 0; i < nPatch; i++) gZonePass(sTop, max(0, (pC0[i] - 1) * TILE - cx), (pC1[i] + 2) * TILE - cx, 80);   // incrustations
    if (gTransT > 0) gZonePass(sTop, 0, DVW, 150 + (int)(gTransT * 400));                          // le jeu se recompile
    if (dQ) {                                            // question : fond gris transparent derriere l'avatar et le sorcier
      const PAnim &A = WIZ_ANIM[WIZ_IDLE(dQW)];
      int wx = (int)dQX - cx, wy = (int)dQY - cy, ax = (int)dpx - cx, ay = (int)dpy - cy, hw = max((int)A.ax, A.w - A.ax);
      dShadeBox(min(ax - 12, wx - hw - 4), min(ay - 26, wy - A.ay - 4), max(ax + (int)PW + 12, wx + hw + 4), max(ay + (int)PH, wy) + 3);
    }
    // joueur
    if ((dPhase != 2 && (dInvT <= 0 || ((millis() / 80) & 1))) || (dPhase == 2 && ((millis() / 90) & 1))) {
      int x0 = (int)dpx - cx, y0 = (int)dpy - cy;
      dsFlame(x0 + 6, y0 + (int)PH, (dTheme == 0 || dTheme == 2 || dTheme == 3) ? dHeat / 100.0f : 0.0f);   // la flamme est l'avatar dans tous les mondes
    }
    for (int ty = DGY; ty <= DGY + 1; ty++) if (ty * TILE + TILE > cy && ty * TILE < cy + DSH)   // sable mouvant : par-dessus l'avatar (il s'enfonce)
      for (int tx = c0; tx <= c1; tx++) if (dg[ty][tx] == 'q') dsQuick(tx * TILE - cx, ty * TILE - cy, tx, ty == DGY, tm);
    if (dPopT > 0) {                                     // points du mat : "+N" qui monte pres du drapeau
      char fp[12]; snprintf(fp, sizeof(fp), "+%d", dPopPts);
      int fw = textW(fb, 2, fp), fx = min(dFlX * TILE + 18 - cx, SCREEN_W - 4 - fw), fy = (int)(dPopY - cy - (1.8f - dPopT) * 22) - 8;
      fb.setTextColor(COL_BG); gfxPrintAt(fb, 2, fx + 1, fy + 1, fp);
      fb.setTextColor(COL_STAR); gfxPrintAt(fb, 2, fx, fy, fp);
    }
    if (dPhase == 2 && dDeathMsg) sayAt((int)dpx - cx + 6, (int)dpy - cy - 26, dDeathMsg, false);   // c'est l'avatar qui parle
    else if (dPhase == 0 && gAvT > 0) sayAt((int)dpx - cx + 6, (int)dpy - cy - 26, gStartTxt, false);
    if (gSayT > 0 && gSayE >= 0 && den[gSayE].alive) {   // bulle du glitch en haut de l'ecran (ne cache pas le jeu), pointilles jusqu'a lui
      int ex = (int)den[gSayE].x - cx, ey = (int)den[gSayE].y - cy - 20;
      for (int yy = 50 - sTop; yy < ey; yy += 4) if (yy >= 0 && yy < DSH && ex >= 0 && ex < DVW) fb.drawPixel(ex, yy, RGB565(255, 40, 200));
      sayAt(ex, 36 - sTop, gSayTxt, true);
    }
    if (dQ) {                                            // question : sorcier dans la scene, scene agrandie, puis panneau en pleine resolution
      dsWizard((int)dQX - (int)dcamX, (int)dQY - (int)dcamY - sTop, (int)(dpx + PW / 2) - (int)dcamX, (int)(dpy + PH / 2) - (int)dcamY - sTop);
      dScaleInPlace(); int ys = sTop * STRIP_H / DSH; worldQuestionPanel(ys); dBlitPlain(ys); continue;
    }
    // jauge de chaleur (desert), pieces, etoiles, temps
    if (dTheme == 0 || dTheme == 2 || dTheme == 3) {
      uint16_t hc = dHeat < 40 ? RGB565(120, 200, 90) : dHeat < 70 ? RGB565(240, 200, 60) : RGB565(230, 60, 40);
      fb.fillRect(4, 3 - sTop, 78, 11, RGB565(30, 20, 14)); fb.drawRect(5, 4 - sTop, 72, 9, COL_TXT);
      if (dTheme == 2) hc = dMix(RGB565(120, 190, 255), RGB565(255, 255, 255), (int)(dHeat * 2));
      fb.fillRect(6, 5 - sTop, (int)(70 * dHeat / 100), 7, hc);
      fb.setTextSize(1); fb.setTextColor(dTheme == 0 ? (dShaded ? RGB565(60, 90, 170) : RGB565(190, 70, 20)) : dTheme == 2 ? (dWarm ? RGB565(255, 170, 60) : RGB565(120, 190, 255)) : (dShaded ? RGB565(120, 190, 255) : RGB565(255, 120, 60)));
      fb.setCursor(90, 5 - sTop); fb.print(dTheme == 0 ? (dShaded ? "OMBRE" : "SOLEIL") : dTheme == 2 ? (dWarm ? "AU CHAUD" : "FROID") : (dShaded ? "FRAIS" : "CHAUD !"));
    } else if (dTheme == 1) { fb.setTextSize(1); fb.setTextColor(RGB565(200, 240, 190)); fb.setCursor(6, 5 - sTop); fb.print(dVineIdx >= 0 ? "SAUT = LACHER" : "SAUTE SUR LES LIANES"); }
    char tb[16]; unsigned long el = millis() - dStartMs; snprintf(tb, sizeof(tb), "%lu s", el / 1000);
    fb.setTextSize(1); fb.setTextColor(COL_TXT); fb.setCursor(DVW - 6 * (int)strlen(tb) - 6, 5 - sTop); fb.print(tb);
    if (millis() - dStartMs < 8000) { fb.setTextColor(dTheme == 0 ? RGB565(120, 90, 60) : RGB565(170, 220, 160)); fb.setCursor(DVW / 2 - 3 * 25, DVH - 10 - sTop); fb.print("P2+P1 maintenus = quitter"); }
    fb.fillCircle(10, 24 - sTop, 4, GOLD); fb.drawCircle(10, 24 - sTop, 4, GOLDD);
    fb.setTextColor(dTheme == 0 ? RGB565(90, 60, 20) : RGB565(250, 230, 150)); fb.setCursor(18, 21 - sTop); fb.print(dCoinsGot);
    fb.fillTriangle(50, 19 - sTop, 46, 26 - sTop, 54, 26 - sTop, RGB565(255, 230, 70)); fb.fillTriangle(50, 27 - sTop, 46, 20 - sTop, 54, 20 - sTop, RGB565(255, 230, 70));
    fb.setCursor(60, 21 - sTop); fb.print(dStars); fb.print("/3");
    for (int i = 0; i < 2; i++) {                        // vies du niveau
      int hx = 86 + i * 12, hy = 20 - sTop; uint16_t hc = i < dLives ? RGB565(235, 50, 70) : RGB565(70, 70, 90);
      fb.fillCircle(hx + 2, hy + 2, 2, hc); fb.fillCircle(hx + 6, hy + 2, 2, hc); fb.fillTriangle(hx, hy + 3, hx + 8, hy + 3, hx + 4, hy + 8, hc);
    }
    { char gb[16]; snprintf(gb, sizeof(gb), "BUGS %d%%", glitchLvl); fb.setTextColor((millis() / 400) & 1 ? RGB565(255, 60, 210) : RGB565(60, 230, 255)); fb.setCursor(DVW - 6 * (int)strlen(gb) - 6, 21 - sTop); fb.print(gb); }
    if ((dTheme == 0 || dTheme == 2 || dTheme == 3) && dHeat > 70 && ((millis() / 220) & 1)) { uint16_t vc = dTheme == 2 ? RGB565(120, 190, 255) : RGB565(230, 60, 40); fb.drawRect(0, -sTop, DVW, DVH, vc); fb.drawRect(1, 1 - sTop, DVW - 2, DVH - 2, vc); }
#ifdef DESERT_BENCH
    bT[9] += micros() - tq;
#endif
    dBlitAny(sTop);
  }
}


// ================= ECHO (mini-jeu solo ; remplace "Ombres" ; d'apres assets/darkEcho1.jpg et assets/drakEcho.gif) =================
// Tout est noir. Seuls restent visibles tes PAS (blancs) et ceux des ennemis (rouges). Chaque pas lance une ONDE : des rayons qui partent dans tous les sens,
// s'arretent sur les murs : ce sont les rayons, arretes net contre eux, qui dessinent les murs (pas de rebond, pas de points). Ondes blanches (joueur), rouges (ennemis). La SORTIE (zone au sol) n'est visible, en JAUNE, qu'a travers les traits de TES ondes ; des PUITS (zones de chute) ne le sont, en ROUGE, que de la meme facon.
// Les ondes avancent a 125 px/s. Plus tu vas vite (le bouton tenu accelere), plus les pas sont rapproches et forts : un ennemi t'entend si l'onde (portee 56..400 px, reduite par les murs entre vous)
// l'atteint. Il file alors vers l'endroit du bruit, cherche 2,5 s, et t'oublie si tu t'es tu et eloigne. Les pas des ennemis font aussi des ondes, plus faibles.
// But : aller du point A (en haut a gauche) a la sortie. SAUT = AVANCER (tenu, on accelere) ; GAUCHE / DROITE = tourner de 90 degres ; G + D 1 s = quitter.
// ONDE DE CHOC : SAUT deux fois (appuis courts) puis un 3e appui LONG ; plus il dure, plus l'onde est grande et bruyante ; elle part au relachement.
// TOUTES les ondes (pas du joueur, pas des ennemis, onde de choc) sont faites pareil : des rayons continus qui partent de la source dans tous les sens, rebondissent
// sur les murs et s'estompent en 4 s, d'abord du cote de la source. Un ennemi n'entend que si un rayon du joueur arrive jusqu'a lui (delai de propagation compris).
#define EC_W 17                                          // labyrinthe : jusqu'a 17 x 13 cases (cellules de 5 x 4 au niveau 1 a 8 x 6), cases de 48 px = couloirs 3 fois plus larges ; l'ecran suit le joueur
#define EC_H 13
#define EC_T 48
#define EC_N 12                                          // niveaux
#define EC_MAXE 4
#define EC_STRIDE 22.0f                                  // distance entre deux pas du joueur (px)
#define EC_SEGS 2000                                     // pool partage de morceaux de rayon (file circulaire : une nouvelle onde n'arrete JAMAIS les anciennes)
#define EC_MAXW 40                                       // ondes en meme temps
#define EC_E 500                                         // duree de vie d'une onde : 500 px a 125 px/s = 4 s
#define EC_PITS 6
uint8_t ecG[EC_H][EC_W];                                 // 0 libre, 1 mur
int ecLvl, ecPh, ecTW, ecTH, ecHead, ecJTaps; float ecT, ecQuit, ecCamX, ecCamY, ecJH, ecJGap; bool ecChg, ecJPrev, ecPL, ecPR; int ecChain, ecPS; float ecSGap, ecLastLd;   // ecPh : 0 jeu, 1 attrape / chute, 2 sortie, 3 fin ; ecHead : cap 0 E, 1 S, 2 O, 3 N ; ecJH : SAUT tenu (s, < 0 : a relacher d'abord) ; ecChg : charge de l'onde de choc ; ecChain : pas qui se suivent (1, 2, 3) ; ecPS : pas faits pendant l'appui ; ecSGap : temps depuis le dernier pas ; ecLastLd : bruit du dernier pas
float ecX, ecY, ecV, ecAcc, ecLoud; int ecSide, ecExX, ecExY, ecNP, ecPitX[EC_PITS], ecPitY[EC_PITS]; float ecDX = 1, ecDY = 0; const char *ecMsg = "REPERE !";   // joueur : position, vitesse, distance depuis le dernier pas, bruit affiche, sortie, puits (centres), cap
EcWave ecW[EC_MAXW]; int ecWO, ecWN, ecSHead;            // file des ondes : les plus vieilles sortent les premieres (toutes vivent 4 s)
int ecSegsN = 0;                                         // taille reelle du pool (<= EC_SEGS : le plus grand bloc libre, la memoire peut etre fragmentee)
EcSeg *ecS = nullptr;                                    // ~32 Ko : alloue en entrant dans Echo, libere en sortant (la RAM est tres juste)
struct EcFp { float x, y, dx, dy; uint32_t ms; bool en; } ecF[28]; int ecFi;      // empreintes de pas
struct EcEn { float x, y, tm, acc, sp; uint8_t st; bool pend; int8_t gx, gy, htx, hty; uint32_t hearMs; uint8_t dist[EC_W * EC_H]; } ecE[EC_MAXE]; int ecNE;   // st : 0 ronde, 1 alerte (va au bruit), 2 cherche ; pend : a entendu une onde qui n'est pas encore arrivee
static int16_t ecCos[96], ecSin[96];
static inline uint16_t ecCol(int b, int k) {             // eclat 0..255 -> couleur : blanc / jaune (sortie) / rouge (ennemis)
  b = constrain(b, 0, 255);
  return k == 1 ? RGB565(b, b * 225 / 255, b * 30 / 255) : k == 2 ? RGB565(b, b * 40 / 255, b * 40 / 255) : RGB565(b, b, b);
}
static inline int ecCell(int px, int py) {               // 0 libre, 1 mur (ou hors du labyrinthe), 2 sortie
  if (px < 0 || py < 0) return 1;
  int tx = px / EC_T, ty = py / EC_T;
  return (tx >= ecTW || ty >= ecTH) ? 1 : ecG[ty][tx];
}
bool ecBlocked(float x, float y, float m) { return ecCell((int)(x - m), (int)(y - m)) == 1 || ecCell((int)(x + m), (int)(y - m)) == 1 || ecCell((int)(x - m), (int)(y + m)) == 1 || ecCell((int)(x + m), (int)(y + m)) == 1; }
void ecBfs(uint8_t *dist, int tx, int ty) {              // distances (en cases) depuis (tx, ty)
  static uint8_t q[EC_W * EC_H]; int qh = 0, qt = 0;
  memset(dist, 255, EC_W * EC_H); dist[ty * EC_W + tx] = 0; q[qt++] = ty * EC_W + tx;
  static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  while (qh < qt) {
    int v = q[qh++], x = v % EC_W, y = v / EC_W;
    for (int k = 0; k < 4; k++) {
      int nx = x + D[k][0], ny = y + D[k][1];
      if (nx < 0 || ny < 0 || nx >= ecTW || ny >= ecTH || ecG[ny][nx] == 1 || dist[ny * EC_W + nx] != 255) continue;
      dist[ny * EC_W + nx] = dist[v] + 1; q[qt++] = ny * EC_W + nx;
    }
  }
}
float ecFootX, ecFootY;                                  // position du pied qui vient de se poser : l'onde du pas en part
void ecFoot(float x, float y, float dx, float dy, bool en) {   // une empreinte (decalee a gauche / a droite du trajet)
  ecSide ^= 1;
  EcFp &f = ecF[ecFi]; ecFi = (ecFi + 1) % 28;
  float s = ecSide ? 2.5f : -2.5f; f = { x - dy * s, y + dx * s, dx, dy, millis(), en }; ecFootX = f.x + dx * 2; ecFootY = f.y + dy * 2;   // l'onde part du contact : la plante du pied (centre du rond de l'empreinte)
}
void ecHearWave(const EcWave &w, float sx, float sy) {   // un rayon du joueur passe-t-il pres d'un ennemi ? (tolerance = ecart entre rayons) -> il entendra quand la tete du rayon l'atteint
  for (int i = 0; i < ecNE; i++) {
    EcEn &e = ecE[i]; float best = 1e9f;
    for (int j = 0; j < w.ns; j++) {
      const EcSeg &g = ecS[w.start + j]; if (g.in < 35) continue;
      float vx = g.x1 - g.x0, vy = g.y1 - g.y0, L2 = vx * vx + vy * vy; if (L2 < 1) continue;
      float t = ((e.x - g.x0) * vx + (e.y - g.y0) * vy) / L2; t = t < 0 ? 0 : t > 1 ? 1 : t;
      float px = g.x0 + vx * t - e.x, py = g.y0 + vy * t - e.y, s = g.t0 + t * (g.t1 - g.t0), tol = 14 + s * 3.2f / w.nr;
      if (px * px + py * py < tol * tol && s < best) best = s;
    }
    if (best < 1e8f) { uint32_t at = w.ms + (uint32_t)(best * 16); if (!e.pend || at < e.hearMs) { e.pend = true; e.hearMs = at; e.htx = (int)sx / EC_T; e.hty = (int)sy / EC_T; } }
  }
}
int ecSegAlloc(int need) {                               // place contigue dans la file circulaire des morceaux (-1 : plein)
  if (ecWN == 0) { ecSHead = 0; return 0; }
  int tail = ecW[ecWO].start;                            // debut de la plus vieille onde encore vivante
  if (ecSHead > tail) { if (ecSegsN - ecSHead >= need) return ecSHead; if (tail > need) { ecSHead = 0; return 0; } return -1; }   // fin du pool, sinon retour au debut
  return (tail - ecSHead > need) ? ecSHead : -1;         // la place est entre la tete et la plus vieille onde
}
void ecEmit(float fx, float fy, int loud, bool enemy) {  // onde : rayons dans tous les sens qui rebondissent sur les murs ; loud 40 / 150 / 255 = 1er / 2e / 3e pas (et suivants) ; > 255 = onde de choc (taille = loud - 256)
  if (!ecS || ecWN >= EC_MAXW) return;
  int nr, budget, in0, maxSeg, off = random(0, 96);
  if (enemy) { nr = 8; budget = 70 + loud / 2; in0 = 70 + loud / 4; maxSeg = 4; }   // onde d'ennemi : petite (105 px en ronde, 145 px en alerte)
  else if (loud > 255) { int sz = min(255, loud - 256); nr = 24 + sz * 48 / 255; budget = 180 + sz * 320 / 255; in0 = 170 + sz * 85 / 255; maxSeg = 3; }   // onde de choc : 24 a 72 rayons, 180 a 500 px
  else { int q = loud * loud; nr = 5 + q * 19 / 65025; budget = 55 + q * 195 / 65025; in0 = 80 + loud * 175 / 255; maxSeg = 6; }                                           // 1 pas : 5 rayons, 60 px, pale ; 2 pas : 15 rayons, 120 px ; 3 pas et suivants : 24 rayons, 250 px (les ondes avancent 2 fois moins vite : 62 px/s)
  int st = ecSegAlloc(nr * maxSeg); if (st < 0) return;
  EcWave &w = ecW[(ecWO + ecWN) % EC_MAXW]; ecWN++; w.en = enemy; w.nr = nr; w.start = st; w.ns = 0; w.ms = millis(); w.life = (uint16_t)(budget + 125);   // duree de vie en unites de 16 ms : le temps de parcourir le rayon (62 px/s) + 2 s d'effacement
  EcSeg *seg = ecS + st;
  for (int r = 0; r < nr; r++) {
    int rs = w.ns, di = (r * 96 / nr + off) % 96, dx = ecCos[di], dy = ecSin[di];   // 1/256 px par pas de 4 px (vecteur de 1/1024 -> 4 px = 1/256 * 1024)
    int x = (int)(fx * 256), y = (int)(fy * 256), sx = x, sy = y, d = 0, ds = 0, refl = 0, in = in0, segs = 0;
    for (;;) {
      int nx = x + dx, ny = y + dy, c = ecCell(nx >> 8, ny >> 8);
      bool end = false, hit = false;
      if (c == 0) { x = nx; y = ny; d += 4; if (d >= budget) end = true; } else hit = true;
      if (!end && !hit) continue;
      if (d > ds) { seg[w.ns++] = { (int16_t)(sx >> 8), (int16_t)(sy >> 8), (int16_t)(x >> 8), (int16_t)(y >> 8), (uint16_t)ds, (uint16_t)d, 0, (uint8_t)in }; segs++; }
      if (end || ++refl > 6 || segs >= maxSeg) break;                  // fin de portee ou trop de rebonds
      bool xw = ecCell(nx >> 8, y >> 8) != 0, yw = ecCell(x >> 8, ny >> 8) != 0;   // quelle face a-t-on touchee ?
      if (xw && !yw) dx = -dx; else if (yw && !xw) dy = -dy; else { dx = -dx; dy = -dy; }
      in = in * 3 / 4; sx = x; sy = y; ds = d;                         // le rayon repart, un peu moins fort
    }
    for (int k = rs; k < w.ns; k++) seg[k].dr = d;                     // longueur totale du rayon : le front d'effacement va de 0 a dr en 4 s
  }
  ecSHead = st + w.ns;
  if (!enemy) ecHearWave(w, fx, fy);
}
void ecCam(float dt, bool snap) {                        // la camera suit le joueur (labyrinthe plus grand que l'ecran)
  float tx = constrain(ecX - SCREEN_W / 2, 0.0f, (float)max(0, ecTW * EC_T - SCREEN_W)), ty = constrain(ecY - SCREEN_H / 2, 0.0f, (float)max(0, ecTH * EC_T - SCREEN_H));
  if (snap) { ecCamX = tx; ecCamY = ty; } else { ecCamX += (tx - ecCamX) * min(1.0f, dt * 5); ecCamY += (ty - ecCamY) * min(1.0f, dt * 5); }
}
void ecLoad() {
  for (int i = 0; i < 96; i++) { float a = i * 6.2831853f / 96; ecCos[i] = (int16_t)(cosf(a) * 1024); ecSin[i] = (int16_t)(sinf(a) * 1024); }
  int cw = min(8, 5 + ecLvl / 3), ch = min(6, 4 + ecLvl / 4);   // cellules : 5 x 4 au niveau 1, 8 x 6 a la fin
  ecTW = 2 * cw + 1; ecTH = 2 * ch + 1;
  randomSeed(777 + ecLvl * 31);                          // labyrinthe fixe par niveau
  memset(ecG, 1, sizeof(ecG));
  bool vis[6][8] = {}; int stk[48], sp = 0; vis[0][0] = true; ecG[1][1] = 0; stk[sp++] = 0;
  while (sp > 0) {                                       // labyrinthe parfait (parcours en profondeur)
    int cur = stk[sp - 1], cx = cur % cw, cy = cur / cw, o[4], n = 0;
    if (cx > 0 && !vis[cy][cx - 1]) o[n++] = 0; if (cx < cw - 1 && !vis[cy][cx + 1]) o[n++] = 1; if (cy > 0 && !vis[cy - 1][cx]) o[n++] = 2; if (cy < ch - 1 && !vis[cy + 1][cx]) o[n++] = 3;
    if (!n) { sp--; continue; }
    int d = o[random(0, n)], nx = cx + (d == 1) - (d == 0), ny = cy + (d == 3) - (d == 2);
    ecG[2 * cy + 1 + (ny - cy)][2 * cx + 1 + (nx - cx)] = 0; ecG[2 * ny + 1][2 * nx + 1] = 0; vis[ny][nx] = true; stk[sp++] = ny * cw + nx;
  }
  for (int k = 0, extra = min(18, 3 + ecLvl); k < 200 && extra > 0; k++) {     // quelques murs perces : des boucles pour pouvoir fuir
    int tx = random(1, ecTW - 1), ty = random(1, ecTH - 1);
    if ((tx & 1) == (ty & 1) || ecG[ty][tx] != 1) continue;
    bool ok = (tx & 1) ? (ecG[ty - 1][tx] == 0 && ecG[ty + 1][tx] == 0) : (ecG[ty][tx - 1] == 0 && ecG[ty][tx + 1] == 0);
    if (ok) { ecG[ty][tx] = 0; extra--; }
  }
  static uint8_t dist[EC_W * EC_H]; ecBfs(dist, 1, 1);   // sortie : la case la plus eloignee du depart
  int best = 0, bx = 1, by = 1;
  for (int y = 1; y < ecTH; y += 2) for (int x = 1; x < ecTW; x += 2) if (dist[y * EC_W + x] != 255 && dist[y * EC_W + x] > best) { best = dist[y * EC_W + x]; bx = x; by = y; }
  ecExX = bx * EC_T + EC_T / 2; ecExY = by * EC_T + EC_T / 2;
  ecNP = 0;                                              // puits (zones de chute, invisibles hors des ondes) : sur des cases hors du plus court chemin depart -> sortie
  { static uint8_t dE[EC_W * EC_H]; static bool onP[EC_W * EC_H]; ecBfs(dE, bx, by); memset(onP, 0, sizeof(onP));
    static const int8_t D4[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int cx = 1, cy = 1; onP[cy * EC_W + cx] = true;
    while (dE[cy * EC_W + cx] > 0 && dE[cy * EC_W + cx] != 255) {
      for (int k = 0; k < 4; k++) { int nx = cx + D4[k][0], ny = cy + D4[k][1]; if (nx >= 0 && ny >= 0 && nx < ecTW && ny < ecTH && dE[ny * EC_W + nx] == dE[cy * EC_W + cx] - 1) { cx = nx; cy = ny; break; } }
      onP[cy * EC_W + cx] = true;
    }
    int want = min(EC_PITS, 2 + ecLvl / 2);
    for (int t = 0; t < 300 && ecNP < want; t++) {
      int x = random(0, ecTW), y = random(0, ecTH);
      if (ecG[y][x] != 0 || onP[y * EC_W + x] || dist[y * EC_W + x] == 255 || dist[y * EC_W + x] < 4 || (x == bx && y == by)) continue;
      int px = x * EC_T + EC_T / 2, py = y * EC_T + EC_T / 2; bool dup = false; for (int i = 0; i < ecNP; i++) if (ecPitX[i] == px && ecPitY[i] == py) dup = true;
      if (!dup) { ecPitX[ecNP] = px; ecPitY[ecNP] = py; ecNP++; }
    }
  }
  randomSeed(micros());                                  // la suite (ennemis) est imprevisible
  ecX = EC_T + EC_T / 2; ecY = EC_T + EC_T / 2; ecV = 0; ecAcc = 0; ecLoud = 0; ecPh = 0; ecT = 0; ecDX = 1; ecDY = 0;
  ecWN = 0; ecWO = 0; ecSHead = 0;
  for (int i = 0; i < 28; i++) ecF[i].ms = 0;
  ecNE = min(EC_MAXE, 1 + ecLvl / 3);
  int farD = min(10, best / 2);
  for (int i = 0; i < ecNE; i++) {                       // ennemis : cases loin du depart
    EcEn &e = ecE[i]; int tx = ecTW / 2, ty = ecTH / 2;
    for (int t = 0; t < 80; t++) { int x = random(0, ecTW), y = random(0, ecTH); if (ecG[y][x] == 0 && dist[y * EC_W + x] != 255 && dist[y * EC_W + x] >= farD) { tx = x; ty = y; break; } }
    e.x = tx * EC_T + EC_T / 2; e.y = ty * EC_T + EC_T / 2; e.gx = tx; e.gy = ty; e.st = 0; e.tm = 0; e.acc = 0; e.sp = 0; e.pend = false;
    memset(e.dist, 0, sizeof(e.dist));                   // dist = 0 partout : "arrive" -> choisira une cible de ronde
  }
  ecHead = 0; ecChain = 0; ecSGap = 9; ecPS = 0; ecLastLd = 0.1f; ecJTaps = 0; ecJGap = 1; ecChg = false; ecJPrev = ecPL = ecPR = true; ecJH = -1;   // les boutons tenus au lancement doivent d'abord etre relaches
  ecCam(0, true); ecEmit(ecX, ecY, 30, false);           // un premier pas, discret (les deux pieds du depart restent affiches tant qu'on ne marche pas)
}
void ecFree() { if (ecS) { free(ecS); ecS = nullptr; } ecWN = 0; }
void startShadow() {
  stopEspNow();                                          // solo : le Wi-Fi (laisse actif apres une partie a deux) prend 39 Ko, il manquerait pour le pool de 32 Ko
  if (!ecS) {                                            // pool : 2000 morceaux (32 Ko) si possible, sinon le plus grand bloc libre (moins d'ondes a la fois)
    ecSegsN = min(EC_SEGS, (int)((ESP.getMaxAllocHeap() - 1024) / sizeof(EcSeg)));
    if (ecSegsN >= 600) ecS = (EcSeg *)calloc(ecSegsN, sizeof(EcSeg));
  }
  if (!ecS) {                                            // pas assez de memoire : on le dit, puis retour au menu
    brickFill(0, 0, SCREEN_W, SCREEN_H); tft.setTextSize(2); tft.setTextColor(COL_STAR); ctext(96, "MEMOIRE INSUFFISANTE");
    tft.setTextSize(1); tft.setTextColor(COL_TXT); ctextf(130, "Echo demande au moins 10 Ko d'un seul bloc (libre : %u)", (unsigned)ESP.getMaxAllocHeap()); delay(2500);
    state = ST_MODE; drawMode(); return;
  }
  ecWN = 0; ecWO = 0; ecLvl = 0; ecQuit = 0; ecLoad(); state = ST_SHADOW;
}
void ecFx(float dt) {                                    // les ondes vivent 4 s et meurent dans l'ordre ou elles sont nees
  uint32_t now = millis();
  while (ecWN > 0 && ((now - ecW[ecWO].ms) >> 4) > ecW[ecWO].life) { ecWO = (ecWO + 1) % EC_MAXW; ecWN--; }
}
void ecDraw() {
  uint32_t now = millis(); int ox = (int)ecCamX, oy0 = (int)ecCamY;
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {
    const int oy = oy0 + sTop;                           // monde -> bande : (x - ox, y - oy)
    fb.fillRect(0, 0, SCREEN_W, STRIP_H, 0);
    for (int wk = 0; wk < ecWN; wk++) {                  // ondes : traits continus de 2 px ; un front d'effacement part de la source et atteint le bout du rayon a 4 s (bord adouci sur 90 px) ; fondu general sur la derniere seconde
      const EcWave &w = ecW[(ecWO + wk) % EC_MAXW]; int a = (int)((now - w.ms) >> 4), col = w.en ? 2 : 0, fade = constrain((w.life - a) * 255 / 63, 0, 255); if (a > w.life) continue;   // 1 px = 16 ms : 62 px/s
      for (int j = 0; j < w.ns; j++) { const EcSeg &s = ecS[w.start + j]; if (a < s.t0) continue;
        int pe = (int)s.dr * a / w.life, u0 = max((int)s.t0, pe), u1 = min(a, (int)s.t1); if (u1 <= u0) continue;   // partie visible : entre le front d'effacement et la tete
        int x0 = s.x0 - ox, y0 = s.y0 - oy, x1 = s.x1 - ox, y1 = s.y1 - oy;
        if (max(x0, x1) < 0 || min(x0, x1) > SCREEN_W || max(y0, y1) < 0 || min(y0, y1) > STRIP_H) continue;
        int len = s.t1 - s.t0, ramp = 90; bool zone = false;
        if (!w.en) {                                     // ce morceau passe-t-il pres de la sortie ou d'un puits ? (ils ne se voient qu'a travers les traits des ondes du joueur)
          int zx0 = min(s.x0, s.x1) - 24, zx1 = max(s.x0, s.x1) + 24, zy0 = min(s.y0, s.y1) - 24, zy1 = max(s.y0, s.y1) + 24;
          zone = ecExX >= zx0 && ecExX <= zx1 && ecExY >= zy0 && ecExY <= zy1;
          for (int q = 0; q < ecNP && !zone; q++) zone = ecPitX[q] >= zx0 && ecPitX[q] <= zx1 && ecPitY[q] >= zy0 && ecPitY[q] <= zy1;
        }
        int step = (zone || u0 < pe + ramp) ? 16 : (u1 - u0);   // pres du front d'effacement ou d'une zone : petits troncons (eclat / couleur qui changent) ; sinon un seul trait
        for (int v = u0; v < u1; v += step) {
          int v1 = min(u1, v + step), vm = (v + v1) / 2, b = s.in * constrain((vm - pe) * 255 / ramp, 0, 255) / 255 * fade / 255;
          if (b < 12) continue;
          int k = col;
          if (zone) {
            int wx = s.x0 + (s.x1 - s.x0) * (vm - s.t0) / len, wy = s.y0 + (s.y1 - s.y0) * (vm - s.t0) / len;
            if (abs(wx - ecExX) < 22 && abs(wy - ecExY) < 22) k = 1;                                                    // zone de sortie : jaune
            else for (int q = 0; q < ecNP; q++) if (abs(wx - ecPitX[q]) < 24 && abs(wy - ecPitY[q]) < 24) { k = 2; break; }   // puits : rouge
          }
          int ax = x0 + (x1 - x0) * (v - s.t0) / len, ay = y0 + (y1 - y0) * (v - s.t0) / len, bx = x0 + (x1 - x0) * (v1 - s.t0) / len, by = y0 + (y1 - y0) * (v1 - s.t0) / len; uint16_t cc = ecCol(b, k);
          fb.drawLine(ax, ay, bx, by, cc);
          if (abs(bx - ax) >= abs(by - ay)) fb.drawLine(ax, ay + 1, bx, by + 1, cc); else fb.drawLine(ax + 1, ay, bx + 1, by, cc);
        } } }
    for (int i = 0; i < 28; i++) { EcFp &f = ecF[i]; if (!f.ms) continue; uint32_t age = now - f.ms, life = f.en ? 1500 : 2600; if (age > life) continue;
      int b = 255 * (int)(life - age) / (int)life, x = (int)f.x - ox, y = (int)f.y - oy; if (y < -6 || y > STRIP_H + 6 || x < -6 || x > SCREEN_W + 6) continue; uint16_t c = ecCol(b, f.en ? 2 : 0);
      fb.fillCircle((int)(x + f.dx * 2), y + (int)(f.dy * 2), 2, c); fb.fillCircle((int)(x - f.dx * 3), y - (int)(f.dy * 3), 1, c); }   // plante + talon
    if (ecChg && ecJH > 0.1f) { float c = min(1.0f, ecJH / 2.15f); int r = 6 + (int)(18 * c); fb.drawCircle((int)ecX - ox, (int)ecY - oy, r, ecCol(c >= 1.0f ? ((now / 120) & 1 ? 255 : 140) : 40 + (int)(110 * c), 0)); }   // charge de l'onde de choc autour des pieds
    if (ecV == 0) for (int sd = -1; sd <= 1; sd += 2) {  // a l'arret : on voit toujours nos deux pieds, cote a cote, tournes dans le cap
      float px = ecX - ecDY * 4 * sd - ox, py = ecY + ecDX * 4 * sd - oy; uint16_t c = ecCol(235, 0);
      fb.fillCircle((int)(px + ecDX * 2), (int)(py + ecDY * 2), 2, c); fb.fillCircle((int)(px - ecDX * 3), (int)(py - ecDY * 3), 1, c); }
    if (ecPh == 1) {                                     // attrape / chute : fondu rouge sur tout l'ecran
      int tt = constrain((int)(300 * (1.3f - ecT) / 1.3f), 0, 230); const uint16_t RD = RGB565(190, 8, 8); uint16_t *bp = fb.getBuffer();
      if (tt > 0) for (int i = 0; i < SCREEN_W * STRIP_H; i++) bp[i] = dMix(bp[i], RD, tt);
    }
    fb.setTextSize(1); fb.setTextColor(RGB565(90, 90, 100));
    char t[24]; snprintf(t, sizeof(t), "ECHO  %d/%d", ecLvl + 1, EC_N); gfxPrintAt(fb, 1, 8, 8 - sTop, t);
    fb.drawRect(256, 8 - sTop, 50, 7, RGB565(70, 70, 80)); int nb = (int)(ecLoud * 48);        // jauge de bruit : blanc -> rouge
    fb.fillRect(257, 9 - sTop, nb, 5, RGB565(120 + (int)(ecLoud * 135), 120 - (int)(ecLoud * 80), 120 - (int)(ecLoud * 80)));
    if (ecPh > 0) {
      const char *m = ecPh == 1 ? ecMsg : ecPh == 2 ? "SORTIE !" : "BRAVO ! 12 niveaux";
      int tw = textW(fb, 2, m); fb.fillRect(SCREEN_W / 2 - tw / 2 - 8, 104 - sTop, tw + 16, 30, 0);
      fb.setTextColor(ecPh == 1 ? RGB565(255, 90, 90) : RGB565(255, 220, 40)); gfxPrintAt(fb, 2, SCREEN_W / 2 - tw / 2, 108 - sTop, m);
    }
    dBlitPlain(sTop);
  }
}
void ecEnemies(float dt) {
  for (int i = 0; i < ecNE; i++) {
    EcEn &e = ecE[i];
    if (e.pend && millis() >= e.hearMs) { e.pend = false; ecBfs(e.dist, e.htx, e.hty); e.st = 1; e.tm = 0; }   // l'onde du joueur vient d'arriver jusqu'a lui : alerte, il file vers l'endroit du bruit
    if (e.st == 1) { e.tm += dt; if (e.tm > 7.0f) { e.st = 2; e.tm = 1.5f; } }      // plus de bruit depuis 7 s : il n'entend plus rien, il cherche puis oublie
    float sp = e.st == 1 ? 52.0f : e.st == 0 ? 20.0f : 0.0f; e.sp = sp;
    if (e.st == 2) { e.tm -= dt; if (e.tm <= 0) { e.st = 0; memset(e.dist, 0, sizeof(e.dist)); } continue; }   // cherche sur place, puis oublie
    float tx = e.gx * EC_T + EC_T / 2, ty = e.gy * EC_T + EC_T / 2, dx = tx - e.x, dy = ty - e.y, d = sqrtf(dx * dx + dy * dy);
    if (d < 1.5f) {                                      // arrive au centre d'une case : on choisit la suivante
      uint8_t cd = e.dist[e.gy * EC_W + e.gx];
      if (cd == 0) {
        if (e.st == 1) { e.st = 2; e.tm = 2.5f; }        // arrive la ou il a entendu : il cherche
        else { int rx = e.gx, ry = e.gy; for (int t = 0; t < 40; t++) { int x = random(0, ecTW), y = random(0, ecTH); if (ecG[y][x] == 0 && (abs(x - e.gx) + abs(y - e.gy)) >= 4) { rx = x; ry = y; break; } } ecBfs(e.dist, rx, ry); }   // ronde : nouvelle destination au hasard
      } else {
        static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (int k = 0; k < 4; k++) { int nx = e.gx + D[k][0], ny = e.gy + D[k][1]; if (nx >= 0 && ny >= 0 && nx < ecTW && ny < ecTH && e.dist[ny * EC_W + nx] == cd - 1) { e.gx = nx; e.gy = ny; break; } }
      }
    } else {
      float step = sp * dt; if (step > d) step = d;
      e.x += dx / d * step; e.y += dy / d * step; e.acc += step;
      if (e.acc >= 14.0f) { e.acc -= 14.0f; ecFoot(e.x, e.y, dx / d, dy / d, true); ecEmit(ecFootX, ecFootY, e.st == 1 ? 150 : 70, true); }   // pas d'ennemi : empreinte rouge + onde plus faible
    }
  }
}
int ecStepLoud() {                                       // 1er pas : petite onde (40), 2e : moyenne (150), 3e et suivants : le maximum des pas (255) ; la serie continue tant que les pas se suivent a moins de 1,3 s
  ecChain = ecSGap < 1.3f ? min(ecChain + 1, 3) : 1; ecSGap = 0;
  return ecChain == 1 ? 40 : ecChain == 2 ? 150 : 255;
}
void ecTapStep() {                                       // un appui court = un petit pas : avance de 10 px, empreinte + onde faible
  for (int k = 0; k < 5; k++) { float nx = ecX + ecDX * 2, ny = ecY + ecDY * 2; if (ecBlocked(nx, ny, 4)) break; ecX = nx; ecY = ny; }
  int ld = ecStepLoud(); ecLastLd = ld / 255.0f; ecFoot(ecX, ecY, ecDX, ecDY, false); ecEmit(ecFootX, ecFootY, ld, false);
}
bool shadowStep(bool bl, bool br, bool bj, bool je) {    // false = on quitte
  static unsigned long last = 0; unsigned long now = millis();
  float dt = (now - last) / 1000.0f; last = now; if (dt <= 0 || dt > 0.1f) dt = 0.03f;
  if (bl && br) { ecQuit += dt; if (ecQuit > 1.0f) { ecFree(); return false; } } else ecQuit = 0;
  ecFx(dt);
  if (ecPh > 0) {                                        // message : repere (on recommence) / sortie (niveau suivant) / fin
    ecT -= dt;
    if (ecT <= 0) { if (ecPh == 1) ecLoad(); else if (ecPh == 2) { ecLvl++; ecLoad(); } else if (je) { ecFree(); return false; } }
    ecJPrev = bj; ecPL = bl; ecPR = br; ecLoud = 0; ecCam(dt, false); ecDraw(); return true;
  }
  if (bl && !ecPL) ecHead = (ecHead + 3) & 3;            // GAUCHE / DROITE : tourner de 90 degres (le cap oriente les pieds)
  if (br && !ecPR) ecHead = (ecHead + 1) & 3;
  ecPL = bl; ecPR = br;
  static const int8_t HX[4] = { 1, 0, -1, 0 }, HY[4] = { 0, 1, 0, -1 }; ecDX = HX[ecHead]; ecDY = HY[ecHead];
  bool press = bj && !ecJPrev, rel = !bj && ecJPrev; ecJPrev = bj;
  if (ecJH < 0) { if (!bj) ecJH = 0; }                   // le bouton tenu au lancement doit d'abord etre relache
  else {
    if (press) { ecJH = 0.0001f; ecPS = 0; ecChg = ecJTaps >= 2 && ecJGap < 0.45f; }   // 3e appui juste apres 2 appuis courts : on charge l'onde de choc au lieu d'avancer
    if (bj) ecJH += dt; else ecJGap += dt;
    if (rel) {
      if (ecChg) { int sz = constrain((int)((ecJH - 0.15f) / 2.0f * 255), 0, 255); ecEmit(ecX, ecY, 256 + sz, false); ecLoud = 0.4f + 0.6f * sz / 255.0f; ecJTaps = 0; ecChg = false; }   // plus c'est long, plus l'onde est grande
      else if (ecJH < 0.25f) { if (ecPS == 0) ecTapStep(); ecJTaps = ecJGap < 0.45f ? ecJTaps + 1 : 1; }     // appui court : compte pour le double appui
      else ecJTaps = 0;
      ecJH = 0; ecJGap = 0;
    }
  }
  bool mv = bj && ecJH >= 0 && !ecChg;                   // SAUT tenu = avancer dans le cap, en accelerant : 22 px/s (discret) -> 95 px/s (bruyant)
  if (mv) {
    if (ecV < 22) ecV = 22; ecV = min(95.0f, ecV + 55 * dt);
    float nx = ecX + ecDX * ecV * dt, ny = ecY + ecDY * ecV * dt;
    if (!ecBlocked(nx, ny, 4)) { ecAcc += ecV * dt; ecX = nx; ecY = ny; } else ecV = 22;
  } else ecV = 0;
  ecSGap += dt;
  while (ecAcc >= EC_STRIDE) {                           // un pas : empreinte blanche + onde ; 1 pas = petite onde, 2 = moyenne, 3 et suivants = le maximum des pas
    ecAcc -= EC_STRIDE; ecPS++; int ld = ecStepLoud(); ecLastLd = ld / 255.0f;
    ecFoot(ecX, ecY, ecDX, ecDY, false); ecEmit(ecFootX, ecFootY, ld, false);
  }
  ecLoud += ((mv ? ecLastLd : 0) - ecLoud) * min(1.0f, dt * 8);
  ecEnemies(dt);
  for (int i = 0; i < ecNE; i++) if (fabsf(ecE[i].x - ecX) < 10 && fabsf(ecE[i].y - ecY) < 10) { ecPh = 1; ecT = 1.3f; ecMsg = "REPERE !"; }   // un ennemi te touche : fondu rouge
  for (int i = 0; i < ecNP && ecPh == 0; i++) if (fabsf(ecX - ecPitX[i]) < 20 && fabsf(ecY - ecPitY[i]) < 20) { ecPh = 1; ecT = 1.3f; ecMsg = "CHUTE !"; }   // un puits : on tombe
  if (ecPh == 0 && fabsf(ecX - ecExX) < 20 && fabsf(ecY - ecExY) < 20) { if (ecLvl < EC_N - 1) { ecPh = 2; ecT = 1.2f; } else { ecPh = 3; ecT = 0.8f; } }
  ecCam(dt, false); ecDraw();
  return true;
}

// ================= SAMOURAI (banc d'essai de fluidite d'un personnage avant de l'integrer a un jeu : 2 niveaux) =================
// Le joueur est, a tour de role (change a chaque entree dans le mode), l'AVATAR A CAPE (assets/avatarCape.png) ou le SAMOURAI (assets/samourai5.gif) ;
// les fantomes sont ceux de assets/perso.gif. Niveau 1 : crepuscule (assets/DecorSamurai1.png), niveau 2 : foret brumeuse (assets/DecorSamurai2.png),
// chacun avec un fond en parallaxe a 3 plans de detail. Un seul bouton d'action : SAUT fait sauter... sauf si un ennemi est a portee : coup d'epee.
// Sensation de la flamme du jeu principal : acceleration au sol 2300 / en l'air 1850, saut plus haut tant que le bouton est tenu, delai de grace
// apres un bord (0,08 s), saut memorise (0,10 s), ecrasement a l'atterrissage, poussiere, camera qui anticipe.
// GAUCHE / DROITE = marcher ; GAUCHE + DROITE 1 s = quitter. 3 coeurs ; tomber dans un trou = recommencer le niveau.
void pDraw(Adafruit_GFX &g, int anim, int frame, int fx, int fy, bool right, int k, uint16_t solid = 0) {   // sprite de perso.gif, pieds en (fx, fy), echelle k ; solid != 0 : silhouette d'une couleur (touche)
  const PAnim &A = PERSO_ANIM[anim]; sprDraw(g, A, PERSO_PIX, PERSO_PAL[A.ch], frame, fx, fy, right, k, solid);
}
// ---- moteur d'avatar fluide : graphe de poses + images intermediaires (cape), ressorts, dessin mis a l'echelle ligne par ligne ----
static int capeNext(int from, int to, int &dir) {         // premiere arete a prendre pour aller de 'from' a 'to' (plus court chemin) ; -1 si deja arrive
  if (from == to) return -1;
  int d[6]; for (int i = 0; i < 6; i++) d[i] = 99; d[to] = 0;
  for (int it = 0; it < 6; it++) for (int e = 0; e < CAPE_NE; e++) {
    int a = CAPE_EDGE[e][0], b = CAPE_EDGE[e][1];
    if (d[a] + 1 < d[b]) d[b] = d[a] + 1; if (d[b] + 1 < d[a]) d[a] = d[b] + 1;
  }
  for (int e = 0; e < CAPE_NE; e++) {
    int a = CAPE_EDGE[e][0], b = CAPE_EDGE[e][1];
    if (a == from && d[b] == d[from] - 1) { dir = 1; return e; }
    if (b == from && d[a] == d[from] - 1) { dir = -1; return e; }
  }
  return -1;
}
void capeDyn(CapeAv &a, float dt, float vx, bool right, float off0) {   // ressorts communs : retournement, inclinaison, ecrasement / etirement (off0 = etirement vise, + = plus haut)
  a.face += ((right ? 1.0f : -1.0f) - a.face) * min(1.0f, dt * 14);                      // retournement : la largeur passe par ~0
  float lt = constrain(vx / 120.0f, -1.0f, 1.0f) * 0.16f;                                  // inclinaison dans le sens de la marche (cisaillement)
  a.vlean += ((lt - a.lean) * 220 - a.vlean * 22) * dt; a.lean += a.vlean * dt;
  a.voff += ((off0 - a.off) * 260 - a.voff * 20) * dt; a.off += a.voff * dt;
}
void capeStep(CapeAv &a, int target, float edur, float dt, float vx, bool right, float off0) {   // edur : duree d'une arete (s)
  a.target = target;
  float adv = dt / edur;
  while (adv > 0) {
    if (!a.moving) { int e = capeNext(a.node, target, a.dir); if (e < 0) break; a.edge = e; a.p = 0; a.moving = true; }
    float need = 1 - a.p;
    if (adv < need) { a.p += adv; adv = 0; }
    else { adv -= need; a.node = CAPE_EDGE[a.edge][a.dir > 0 ? 1 : 0]; a.moving = false; a.p = 0; }
  }
  capeDyn(a, dt, vx, right, off0);
}
static int capeFrame(const CapeAv &a) {
  if (!a.moving) return a.node;
  int k = (int)(a.p * (CAPE_M + 1) + 0.5f), f = CAPE_EDGE[a.edge][a.dir > 0 ? 0 : 1], t = CAPE_EDGE[a.edge][a.dir > 0 ? 1 : 0];
  if (k <= 0) return f; if (k >= CAPE_M + 1) return t;
  return CAPE_EDGEF[a.edge] + (a.dir > 0 ? k : CAPE_M + 1 - k) - 1;                            // images intermediaires (CAPE_EDGEF : 1re image de l'arete, numerotee apres les 6 poses)
}
// dessin generique d'une image 4 bits (px = ses octets, pal : 0 = transparent), pieds en (fx, fy), echelle k, sxs = signe/largeur relatifs (< 0 : miroir), off = etirement, lean = cisaillement
static const uint8_t BAY[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };   // trame de Bayer 4x4 : fondu entre deux images (50 images/s pour une animation a 10-14 images/s)
// px2 / blend (0..16) : mele une 2e image (pixel de la 2e quand la trame < blend) ; sparse : 1 = un pixel sur 2, 2 = un sur 4 (images fantomes de la trainee)
void fxDraw(Adafruit_GFX &g, const uint8_t *px, const uint16_t *pal, int W, int H, int AX, int AY, int fx, int fy, float k, float sxs, float off, float lean, const uint8_t *px2 = nullptr, int blend = 0, int sparse = 0) {
  float sy = k * (1 + off), sx = k * sxs * (1 - off * 0.6f);
  if (fabsf(sx) < 0.08f) sx = sx < 0 ? -0.08f : 0.08f;
  int y0 = max(0, fy - (int)(AY * sy) - 1), y1 = min((int)g.height() - 1, fy + (int)((H - AY) * sy) + 1);
  float asx = fabsf(sx), l = sx > 0 ? AX * asx : (W - AX) * asx, r = sx > 0 ? (W - AX) * asx : AX * asx;
  const int step = (int)(65536.0f / sx);
  for (int y = y0; y <= y1; y++) {
    int syi = (int)floorf(AY + (y - fy + 0.5f) / sy); if (syi < 0 || syi >= H) continue;
    float sh = lean * (fy - y);
    int xa = (int)(fx + sh - l) - 1, xb = (int)(fx + sh + r) + 1;
    int cur = (int)((AX + (xa + 0.5f - fx - sh) / sx) * 65536.0f);
    int run = xa; uint8_t rc = 0;
    for (int x = xa; x <= xb + 1; x++, cur += step) {
      uint8_t c = 0;
      if (x <= xb) { int si = cur >> 16; if (si >= 0 && si < W) { int i = syi * W + si; const uint8_t *q = (px2 && blend > BAY[((y & 3) << 2) | (x & 3)]) ? px2 : px; c = (q[i >> 1] >> ((i & 1) ? 0 : 4)) & 15;
        if (sparse && ((sparse == 1 && ((x + y) & 1)) || (sparse == 2 && ((x | y) & 1)))) c = 0; } }
      if (c != rc || x > xb) { if (rc) g.drawFastHLine(run, y, x - run, pal[rc]); run = x; rc = c; }
    }
  }
}
void capeDraw(Adafruit_GFX &g, const CapeAv &a, int fx, int fy, float k) {
  fxDraw(g, CAPE_PIX + capeFrame(a) * ((CAPE_W * CAPE_H + 1) / 2), CAPE_PAL, CAPE_W, CAPE_H, CAPE_AX, CAPE_AY, fx, fy, k, a.face, a.off, a.lean);
}
void smDrawImg(Adafruit_GFX &g, const SmImg &m, int fx, int fy, float off, float lean, int sparse) { fxDraw(g, m.px, m.pal, m.W, m.H, m.AX, m.AY, fx, fy, m.k, m.sxs, off, lean, m.px2, m.blend, sparse); }
// ---- niveaux ----
struct SmLv { int16_t w, h; uint8_t ground, np, npl, ne; int16_t sx, sy, ex, ey; int16_t pit[4][2]; int16_t plat[32][4]; int16_t en[8][4]; int16_t mace[2][3]; int16_t box[6][4]; };
// largeur, hauteur du monde, sol (0 = grotte sans sol), nb de fosses / toits / fantomes, depart (pieds), sortie (0 = bord droit), fosses (x0, x1), toits (x0, y0, x1, y1 : pente si y0 != y1),
// fantomes (x, zone x0, zone x1, y), balanciers a pointes (ancre x, y, longueur), blocs pleins (x0, y0, x1, y1 : on ne les traverse pas lateralement). 3 etages dans les niveaux "toits" : sol 200, toits ~150 et ~100 ; sauts de 104 px de haut.
static const SmLv SM_LV[4] = {
  { 1494, 1944, 0, 0, 30, 7, 705, 216, 510, 1923, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },              // niveau 1 : la grotte, decor = assets/niveau1.png (coordonnees du croquis x 3)
    { { 570, 216, 786, 216 }, { 786, 216, 840, 252 }, { 840, 273, 1134, 273 }, { 1050, 186, 1305, 186 }, { 1350, 159, 1464, 159 }, { 816, 498, 1098, 498 }, { 1224, 492, 1395, 492 }, { 1050, 627, 1440, 627 }, { 840, 753, 1320, 753 }, { 1254, 867, 1440, 867 }, { 954, 924, 1191, 924 }, { 894, 1020, 1221, 1020 }, { 930, 1140, 1440, 1140 }, { 639, 855, 711, 930 }, { 711, 930, 771, 999 }, { 771, 999, 840, 1071 }, { 840, 1071, 939, 1137 }, { 210, 849, 636, 849 }, { 120, 975, 435, 975 }, { 405, 1074, 636, 1074 }, { 120, 1176, 555, 1176 }, { 135, 1299, 675, 1299 }, { 204, 1410, 324, 1410 }, { 360, 1503, 510, 1503 }, { 576, 1422, 744, 1422 }, { 120, 1554, 219, 1554 }, { 639, 1566, 750, 1566 }, { 294, 1665, 420, 1665 }, { 540, 1653, 666, 1653 }, { 165, 1923, 705, 1923 } },
    { { 990, 855, 1080, 495 }, { 1260, 1080, 1410, 627 }, { 1050, 900, 1290, 753 }, { 1200, 990, 1410, 1140 }, { 420, 270, 600, 849 }, { 360, 180, 540, 1176 }, { 450, 210, 645, 1299 } },
    { { 267, 999, 108 }, { 0, 0, 0 } },
    { { 450, 210, 813, 756 }, { 1386, 258, 1560, 534 }, { 0, 852, 117, 1944 }, { 750, 1290, 900, 1944 }, { 1452, 180, 1560, 1200 }, { 0, 0, 0, 0 } } },
  { 1100, 240, 1, 3, 8, 6, 40, 200, 0, 0, { { 330, 380 }, { 690, 740 }, { 900, 950 }, { 0, 0 } },              // niveau 1 : crepuscule
    { { 210, 200, 300, 150 }, { 300, 150, 420, 150 }, { 450, 100, 540, 100 }, { 540, 100, 640, 150 }, { 640, 150, 700, 150 }, { 770, 200, 860, 130 }, { 860, 130, 940, 130 }, { 940, 130, 1000, 200 } },
    { { 120, 60, 190, 200 }, { 360, 320, 410, 150 }, { 495, 462, 535, 100 }, { 560, 420, 670, 200 }, { 890, 868, 935, 130 }, { 1045, 1010, 1080, 200 } }, { { 0, 0, 0 }, { 0, 0, 0 } } },
  { 1200, 240, 1, 3, 12, 7, 40, 200, 0, 0, { { 270, 320 }, { 540, 600 }, { 860, 920 }, { 0, 0 } },             // niveau 2 : toits de nuit
    { { 120, 200, 190, 160 }, { 190, 160, 270, 160 }, { 290, 110, 380, 110 }, { 380, 110, 460, 160 }, { 460, 160, 560, 160 }, { 620, 200, 700, 150 }, { 700, 150, 760, 150 }, { 760, 150, 840, 90 },
      { 840, 90, 930, 90 }, { 930, 90, 1010, 140 }, { 1010, 140, 1060, 140 }, { 1060, 140, 1130, 200 } },
    { { 230, 195, 265, 160 }, { 335, 298, 376, 110 }, { 440, 330, 535, 200 }, { 510, 470, 555, 160 }, { 730, 705, 755, 150 }, { 885, 865, 925, 90 }, { 1035, 1015, 1055, 140 } }, { { 0, 0, 0 }, { 0, 0, 0 } } },
  { 1250, 240, 1, 3, 11, 7, 40, 200, 0, 0, { { 260, 310 }, { 560, 614 }, { 880, 934 }, { 0, 0 } },             // niveau 4 : foret brumeuse + boss
    { { 150, 200, 230, 160 }, { 230, 160, 310, 160 }, { 330, 120, 420, 120 }, { 420, 120, 500, 170 }, { 500, 170, 620, 170 }, { 640, 200, 720, 140 }, { 720, 140, 800, 140 }, { 800, 140, 860, 100 },
      { 860, 100, 930, 100 }, { 930, 100, 1010, 170 }, { 1010, 170, 1040, 170 } },
    { { 100, 50, 200, 200 }, { 270, 240, 305, 160 }, { 375, 335, 415, 120 }, { 470, 380, 545, 200 }, { 560, 510, 615, 170 }, { 760, 726, 795, 140 }, { 895, 866, 925, 100 } }, { { 0, 0, 0 }, { 0, 0, 0 } } },
};
static const uint8_t SM_TH[4] = { 3, 0, 2, 1 };           // decor de chaque niveau : 3 grotte (image), 0 crepuscule, 2 nuit, 1 brume
#define SM_MIST (SM_TH[smLevel] == 1)
#define SM_NIGHT (SM_TH[smLevel] == 2)
#define SM_CAVE (SM_TH[smLevel] == 3)
#define SM_GY 200                                         // hauteur du sol
#define SM_W (smL->w)
const SmLv *smL = &SM_LV[0]; int smLevel, smAvKind;       // niveau (0..3) ; avatar : 0 cape, 1 samourai
float smX, smY, smVX, smVY, smAtk, smInv, smCam, smCamY, smMsgT, smQuit, smJBuf, smCoy, smDustT, smA5t, smPrevVY, smAtkPrev; bool smRight, smGround; int smHp, smPh, smKills, smA5, smA5F;   // smPh : 0 jeu, 1 vaincu, 2 niveau fini
CapeAv smAv;
struct SmEn { float x, vx, y, hitT, atk; bool alive; int16_t x0, x1; } smE[8]; int smNE;   // atk : attaque en cours (s)
struct SmPt { float x, y, vx, vy, life; uint16_t c; uint8_t s; } smP[24];                    // poussiere
void smPuff(float x, float y, int n, float spd) {
  uint16_t c = SM_MIST ? RGB565(150, 168, 168) : SM_NIGHT ? RGB565(150, 160, 220) : RGB565(240, 200, 150);
  for (int i = 0; i < 24 && n > 0; i++) if (smP[i].life <= 0) { smP[i] = { x, y, random(-100, 101) / 100.0f * spd, -random(20, 100) / 100.0f * spd * 0.7f, 0.45f, c, (uint8_t)(2 + random(0, 2)) }; n--; }
}
bool smPit(float x) { for (int i = 0; i < smL->np; i++) if (x > smL->pit[i][0] && x < smL->pit[i][1]) return true; return false; }
float smFloor(float x, float yFeet) {                     // surface sous x : sol, ou toit (pente comprise) le plus haut dont on est au-dessus (a 6 px pres : on monte les pentes)
  float best = smL->ground && !smPit(x) ? SM_GY : 30000;   // 30000 = pas de sol (la grotte descend sous 999)
  for (int i = 0; i < smL->npl; i++) {
    const int16_t *p = smL->plat[i];
    if (x > p[0] && x < p[2]) { float fy = p[1] + (p[3] - p[1]) * (x - p[0]) / (p[2] - p[0]); if (yFeet <= fy + 6 && fy < best) best = fy; }
  }
  for (int i = 0; i < 6; i++) { const int16_t *bx = smL->box[i]; if (bx[2] && x > bx[0] && x < bx[2] && yFeet <= bx[1] + 6 && bx[1] < best) best = bx[1]; }   // le dessus d'un bloc plein est un sol
  return best;
}
struct SmBoss { bool on, alive, right; float x, hp, hitT, t, cd; int st; } smBoss;   // petit boss de fin (niveau 2) : st 0 garde, 1 elan, 2 coup, 3 repos
#define SM_BOSS_HP 6
void smBossStep(float dt) {                                // garde (approche), elan 0,45 s (il se voit venir), coup 0,56 s (degats entre 0,14 et 0,42 s), repos
  SmBoss &b = smBoss; if (!b.on || !b.alive) return;
  if (b.hitT > 0) b.hitT -= dt;
  float dx = smX - b.x; if (b.st != 2) b.right = dx > 0;
  float fwd = b.right ? 1.0f : -1.0f;
  auto hurt = [&](float push) { smHp--; smInv = 1.2f; smVX = fwd * push; smVY = -260; smGround = false; if (smHp <= 0) { smPh = 1; smMsgT = 1.4f; } };
  if (b.st == 0) {
    b.cd -= dt;
    if (fabsf(dx) < 300 && fabsf(dx) > 70) b.x += fwd * 38.0f * dt;
    b.x = constrain(b.x, 1060.0f, (float)(SM_W - 30));
    if (fabsf(dx) < 90 && b.cd <= 0) { b.st = 1; b.t = 0; }
  } else if (b.st == 1) { b.t += dt; if (b.t >= 0.45f) { b.st = 2; b.t = 0; } }
  else if (b.st == 2) {
    float t0 = b.t; b.t += dt; b.x = constrain(b.x + fwd * 40.0f * dt, 1060.0f, (float)(SM_W - 30));   // fente en avant
    if (t0 < 0.42f && b.t >= 0.14f && smInv <= 0) { float dd = (smX - b.x) * fwd; if (dd > -14 && dd < 80 && fabsf(smY - SM_GY) < 44) hurt(240); }
    if (b.t >= 0.56f) { b.st = 3; b.t = 0; }
  } else { b.t += dt; if (b.t >= 0.5f) { b.st = 0; b.cd = 1.3f; } }
  if (smInv <= 0 && fabsf(dx) < 12 && fabsf(smY - SM_GY) < 40) hurt(200);                       // ne pas rester dedans
}
SmImg smImg; struct SmTr { SmImg im; float x, y, off, lean; bool on; } smTr[3]; float smTrT;   // trainee : 3 images fantomes (clairsemees) quand on va vite
float smA5B; int smA5G;                                    // fondu : 2e image et part (0..16)
void smAvImage() {                                         // image courante (pose + fondu) pour smDraw
  if (smAvKind == 0) { smImg = { CAPE_PIX + capeFrame(smAv) * ((CAPE_W * CAPE_H + 1) / 2), nullptr, 0, CAPE_PAL, CAPE_W, CAPE_H, CAPE_AX, CAPE_AY, smAv.face, 1.1f }; return; }
  const PAnim &A = SAM5_ANIM[smA5 < 0 ? S5_IDLE : smA5]; int sz = (A.w * A.h + 1) / 2;
  smImg = { SAM5_PIX + A.off + smA5F * sz, SAM5_PIX + A.off + smA5G * sz, (int)(smA5B * 16), SAM5_PAL, A.w, A.h, A.ax, A.ay, smA5 == S5_JUMP ? -smAv.face : smAv.face, 1.0f };   // la piste du saut regarde de l'autre cote dans la planche
}
void smAvatarStep(float dt) {                              // pose / animation selon l'etat du jeu ; ressorts d'ecrasement
  static bool wasG = true;
  float off0 = 0;
  if (smAvKind == 0) {                                     // avatar a cape : graphe de poses
    int tgt = 0; float ed = 0.09f;
    if (smPh == 1 || smInv > 1.0f) tgt = 5;                // vaincu / touche : en boule
    else if (smAtk > 0) { tgt = 3; ed = 0.06f; }
    else if (!smGround) { tgt = smVY < 0 ? 2 : 4; off0 = 0.10f; }
    else if (fabsf(smVX) > 10) {
      int c = smAv.target == 4 ? 4 : 1;
      if (!smAv.moving && smAv.node == c) c = c == 1 ? 4 : 1;   // marche : A <-> B
      tgt = c; ed = max(0.07f, 0.16f - fabsf(smVX) / 120.0f * 0.08f);
    }
    else off0 = 0.02f * sinf(millis() * 0.0075f);          // respiration
    capeStep(smAv, tgt, ed, dt, smVX, smRight, off0);
  } else {                                                 // samourai : animations de la planche, fondu entre images successives
    int an, n; float ft;
    if (smPh == 1 || smInv > 1.0f) { an = S5_HURT; n = 9; ft = min(8.0f, (smA5 == S5_HURT ? smA5t : 0) / 0.07f); }
    else if (smAtk > 0) { an = S5_ATK; n = 11; ft = (0.36f - smAtk) / 0.36f * 10.0f; }
    else if (!smGround) { an = S5_JUMP; n = 5; ft = constrain((smVY + 300.0f) / 150.0f, 0.0f, 3.99f); off0 = 0.06f; }   // image choisie par la vitesse verticale, en continu
    else if (fabsf(smVX) > 15) { an = S5_RUN; n = 16; ft = smA5t / 0.07f; }
    else { an = S5_IDLE; n = 16; ft = smA5t / 0.1f; }
    if (an != smA5) { smA5 = an; smA5t = 0; if (an == S5_HURT) ft = 0; }
    smA5t += dt * (an == S5_RUN ? 0.6f + 0.8f * min(1.0f, fabsf(smVX) / 120.0f) : 1.0f);
    bool loop = an == S5_RUN || an == S5_IDLE; int fi = (int)ft;
    smA5F = loop ? fi % n : min(fi, n - 1); smA5G = loop ? (fi + 1) % n : min(fi + 1, n - 1); smA5B = ft - fi;
    capeDyn(smAv, dt, smVX, smRight, off0);
  }
  if (wasG && !smGround && smVY < 0) { smAv.off = -0.14f; smAv.voff = 3.5f; smPuff(smX, smY, 4, 40); }   // decollage : s'accroupit puis se detend (ressort)
  if (!wasG && smGround) { smAv.off = -min(0.32f, 0.08f + smPrevVY / 1400.0f); smAv.voff = 0; smPuff(smX, smY - 1, 5, 55); }   // atterrissage : ecrasement selon la vitesse de chute
  if (smAtk > 0 && smAtkPrev <= 0) { smAv.off = -0.2f; smAv.voff = 0; }                                         // coup d'epee : ressort
  wasG = smGround; smPrevVY = smVY; smAtkPrev = smAtk;
  for (int i = 0; i < 24; i++) if (smP[i].life > 0) { SmPt &p = smP[i]; p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vy += 420 * dt; }
  if (smGround && fabsf(smVX) > 70 && smPh == 0 && (smDustT -= dt) <= 0) { smDustT = 0.07f; smPuff(smX - (smVX > 0 ? 6 : -6), smY - 1, 1, 22); }   // poussiere de course
  smAvImage();
  if ((smTrT -= dt) <= 0) {                                // trainee : une image fantome toutes les 35 ms quand on va vite
    smTrT = 0.035f; smTr[2] = smTr[1]; smTr[1] = smTr[0];
    smTr[0] = { smImg, smX, smY, smAv.off, smAv.lean, smPh == 0 && (fabsf(smVX) > 85 || fabsf(smVY) > 220) };
  }
}
void smLoad() {
  smL = &SM_LV[smLevel];
  smAv = CapeAv(); smAv.face = 1; for (int i = 0; i < 3; i++) smTr[i].on = false; smA5 = -1; smA5t = 0; smA5F = 0; smJBuf = smCoy = smDustT = 0; smPrevVY = smAtkPrev = 0;
  for (int i = 0; i < 24; i++) smP[i].life = 0;
  smX = smL->sx; smY = smL->sy; smVX = smVY = 0; smAtk = 0; smInv = 0; smCam = constrain(smX - SCREEN_W / 3, 0.0f, (float)(smL->w - SCREEN_W)); smCamY = constrain(smY - 150, 0.0f, (float)(smL->h - SCREEN_H)); smRight = true; smGround = true; smHp = 3; smPh = 0; smKills = 0;
  smNE = smL->ne;
  smBoss = SmBoss(); if (smLevel == 3) { smBoss.on = smBoss.alive = true; smBoss.x = 1170; smBoss.hp = SM_BOSS_HP; smBoss.cd = 1.5f; }
  for (int i = 0; i < smNE; i++) { const int16_t *e = smL->en[i]; smE[i] = { (float)e[0], (i & 1) ? 30.0f : -30.0f, (float)e[3], 0, 0, true, e[1], e[2] }; }
}
void startSamurai() { static int nextKind = 1; smAvKind = nextKind; nextKind ^= 1; smQuit = 0; smLevel = 0; smLoad(); state = ST_SAMU; }
// ---- decor : 3 plans en parallaxe (loin / milieu / proche, de plus en plus detailles) ----
void smRows(int sTop, uint16_t top, uint16_t bot, int yEnd) {          // ciel : degrade vertical jusqu'a yEnd puis uni
  uint16_t *b = fb.getBuffer();
  for (int row = 0; row < STRIP_H; row++) {
    int y = sTop + row; uint16_t c = y < yEnd ? dMix(top, bot, y * 256 / yEnd) : bot;
    uint32_t v = c | ((uint32_t)c << 16), *q = (uint32_t *)(b + row * SCREEN_W); for (int i = 0; i < SCREEN_W / 2; i++) q[i] = v;
  }
}
void smPagoda(int x, int by, int tiers, int w0, int th, uint16_t col) {   // pagode : etages (corps + toit a pointes relevees) et fleche ; x = centre, by = pied (coordonnees de la bande)
  if (by - tiers * th - th > STRIP_H || by + 4 < 0) return;
  int y = by;
  for (int t = 0; t < tiers; t++) {
    int tw = w0 - t * (w0 / (tiers + 2)), top = y - th;
    fb.fillRect(x - tw / 2 + 3, top + 5, tw - 6, th - 4, col);
    fb.fillTriangle(x - tw / 2 - 7, top + 7, x + tw / 2 + 7, top + 7, x, top - th / 3, col);
    fb.fillTriangle(x - tw / 2 - 9, top + 2, x - tw / 2 - 2, top + 7, x - tw / 2 + 6, top + 7, col);
    fb.fillTriangle(x + tw / 2 + 9, top + 2, x + tw / 2 + 2, top + 7, x + tw / 2 - 6, top + 7, col);
    y = top;
  }
  fb.fillRect(x - 1, y - th / 2 - 6, 3, th / 2 + 4, col);
}
void smHouse(int x, int by, int w, int h, uint16_t col, uint16_t win) {   // maison a toit courbe, fenetres allumees
  if (by - h - 14 > STRIP_H || by + 2 < 0) return;
  fb.fillRect(x, by - h, w, h, col);
  fb.fillTriangle(x - 8, by - h + 2, x + w + 8, by - h + 2, x + w / 2, by - h - 14, col);
  fb.fillTriangle(x - 12, by - h - 4, x - 6, by - h + 2, x + 4, by - h + 2, col);
  fb.fillTriangle(x + w + 12, by - h - 4, x + w + 6, by - h + 2, x + w - 4, by - h + 2, col);
  if (win) for (int i = 0; i < 3; i++) fb.fillRect(x + 10 + i * (w - 20) / 3, by - h + 10, 4, 8, win);
}
void smPine(int x, int by, int h, int cw, uint16_t col) {              // pin : tronc et tetes horizontales etagees
  if (by - h - 6 > STRIP_H || by + 2 < 0) return;
  fb.fillRect(x - 3, by - h, 7, h, col);
  for (int i = 0; i < 4; i++) { int yy = by - h + 6 + i * (h / 5), ww = cw - i * 4 + (i & 1) * 5; fb.fillRoundRect(x - ww, yy, ww * 2, 7, 3, col); fb.fillRoundRect(x - ww / 2, yy - 4, ww, 6, 3, col); }
  fb.fillRoundRect(x - cw / 3, by - h - 6, cw * 2 / 3, 8, 3, col);
}
void smTorii(int x, int by, int w, int h, uint16_t col) {              // portique
  if (by - h - 12 > STRIP_H || by + 2 < 0) return;
  fb.fillRect(x - w / 2, by - h, 6, h, col); fb.fillRect(x + w / 2 - 6, by - h, 6, h, col);
  fb.fillRect(x - w / 2 - 10, by - h - 4, w + 20, 6, col);
  fb.fillTriangle(x - w / 2 - 14, by - h - 10, x - w / 2 - 6, by - h - 4, x - w / 2 + 4, by - h - 4, col);
  fb.fillTriangle(x + w / 2 + 14, by - h - 10, x + w / 2 + 6, by - h - 4, x + w / 2 - 4, by - h - 4, col);
  fb.fillRect(x - w / 2, by - h + 14, w, 4, col); fb.fillRect(x - 3, by - h + 2, 6, 12, col);
}
void smLantern(int x, int by, uint16_t col, uint16_t glow) {           // lanterne de pierre : pied, fut, boite eclairee, toit
  if (by - 36 > STRIP_H || by + 2 < 0) return;
  fb.fillRect(x - 6, by - 4, 12, 4, col); fb.fillRect(x - 2, by - 14, 4, 10, col);
  fb.fillRect(x - 6, by - 24, 12, 10, col); fb.fillRect(x - 2, by - 21, 4, 4, glow);
  fb.fillTriangle(x - 10, by - 24, x + 10, by - 24, x, by - 33, col);
}
static inline int smMod(int a, int p) { a %= p; return a < 0 ? a + p : a; }
void smBgDusk(int sTop, int cam, float t, int v) {                    // niveaux 1 (crepuscule, v = 0) et 2 (nuit bleue, v = 1) : lune, pagodes, maisons, toits ; poussiere doree / lucioles
  const uint16_t skyT = v ? RGB565(12, 16, 46) : RGB565(112, 76, 120), skyB = v ? RGB565(112, 88, 156) : RGB565(238, 150, 100);
  const uint16_t farC = v ? RGB565(74, 72, 132) : RGB565(134, 92, 128), midC = v ? RGB565(48, 48, 100) : RGB565(96, 60, 106), nearC = v ? RGB565(26, 26, 62) : RGB565(46, 28, 60), roofC = v ? RGB565(18, 18, 46) : RGB565(34, 20, 46);
  const uint16_t winC = v ? RGB565(255, 214, 130) : RGB565(250, 196, 120), lampC = v ? RGB565(255, 170, 100) : RGB565(255, 196, 120);
  smRows(sTop, skyT, skyB, 170);
  int mx = (v ? 120 : 215) - cam / 40, my = (v ? 74 : 92) - sTop, mr = v ? 40 : 62;
  fb.fillCircle(mx, my, mr + 18, v ? RGB565(96, 90, 160) : RGB565(222, 140, 104)); fb.fillCircle(mx, my, mr + 8, v ? RGB565(140, 138, 204) : RGB565(245, 178, 124)); fb.fillCircle(mx, my, mr, v ? RGB565(236, 240, 255) : RGB565(255, 226, 178));   // lune et halo
  if (v) fb.fillCircle(mx + 12, my - 6, mr - 6, RGB565(214, 222, 250));
  for (int k = -1; k < 3; k++) {                                       // plan 1 (loin) : silhouettes claires, pagodes sans detail
    int o = smMod(cam / 12, 420), x = k * 420 - o;
    smPagoda(x + 130, 196 - sTop, 5, 150, 28, farC); smPagoda(x + 330, 196 - sTop, 3, 92, 24, farC);
  }
  for (int k = -1; k < 3; k++) {                                       // plan 2 (milieu) : maisons a toit courbe et fenetres allumees
    int o = smMod(cam / 5, 320), x = k * 320 - o;
    smHouse(x + 20, 204 - sTop, 112, 34, midC, winC); smPagoda(x + 220, 204 - sTop, 2, 62, 28, midC);
  }
  for (int k = -1; k < 4; k++) {                                       // plan 3 (proche) : toits sombres, lanternes allumees, balustrade
    int o = smMod(cam * 2 / 5, 230), x = k * 230 - o;
    int by = 206 - sTop;
    if (by - 60 < STRIP_H && by + 4 > 0) {
      fb.fillRect(x + 10, by - 40, 120, 40, nearC);
      fb.fillTriangle(x - 6, by - 38, x + 146, by - 38, x + 70, by - 62, roofC);
      fb.fillTriangle(x - 14, by - 48, x - 4, by - 38, x + 10, by - 38, roofC); fb.fillTriangle(x + 154, by - 48, x + 144, by - 38, x + 130, by - 38, roofC);
      for (int i = 0; i < 9; i++) fb.fillRect(x + 150 + i * 8, by - 16, 3, 16, roofC); fb.fillRect(x + 148, by - 18, 76, 3, roofC);
      fb.fillCircle(x + 36, by - 22, 6, v ? RGB565(120, 70, 90) : RGB565(150, 90, 70)); fb.fillCircle(x + 36, by - 22, 3, lampC); fb.fillRect(x + 35, by - 34, 2, 6, roofC);
      if (v) for (int i = 0; i < 3; i++) fb.fillRect(x + 60 + i * 20, by - 30, 5, 9, winC);
    }
  }
  for (int g = 0; g < 3; g++) {                                        // poussiere doree / lucioles : 3 profondeurs
    int rr = 1 + g;
    for (int i = 0; i < 9; i++) {
      int x = smMod(i * 97 + g * 41 + (int)(t * (5 + g * 3)) - cam * (g + 1) / 8, SCREEN_W + 20) - 10, y = smMod(i * 53 + g * 29 - (int)(t * (3 + g * 2)), 190) + 8 - sTop;
      if (y < -4 || y > STRIP_H + 4) continue;
      fb.fillCircle(x, y, rr, (i + (int)(t * 2)) & 1 ? (v ? RGB565(210, 226, 255) : RGB565(255, 214, 140)) : (v ? RGB565(255, 236, 160) : RGB565(240, 170, 100)));
    }
  }
}
void smBgMist(int sTop, int cam, float t) {                            // niveau 2 : foret brumeuse, pins, portiques, lanternes de pierre
  smRows(sTop, RGB565(214, 222, 220), RGB565(248, 250, 247), 150);
  for (int k = -1; k < 3; k++) {                                       // plan 1 (loin) : pins tres pales
    int o = smMod(cam / 16, 300), x = k * 300 - o;
    smPine(x + 60, 200 - sTop, 160, 30, RGB565(204, 212, 212)); smPine(x + 190, 200 - sTop, 126, 26, RGB565(210, 217, 216)); smPine(x + 270, 200 - sTop, 150, 28, RGB565(200, 208, 208));
  }
  for (int k = -1; k < 3; k++) {                                       // plan 2 (milieu) : pins gris, lanternes de pierre, portique lointain
    int o = smMod(cam / 5, 260), x = k * 260 - o;
    smPine(x + 40, 204 - sTop, 168, 34, RGB565(142, 156, 156));
    smLantern(x + 120, 206 - sTop, RGB565(122, 138, 138), RGB565(190, 236, 236)); smLantern(x + 150, 206 - sTop, RGB565(122, 138, 138), RGB565(190, 236, 236));
    if (k & 1) smTorii(x + 200, 206 - sTop, 56, 62, RGB565(124, 138, 138));
  }
  for (int k = -1; k < 3; k++) {                                       // plan 3 (proche) : grands troncs sombres, buissons, portique noir, lanterne a lueur
    int o = smMod(cam * 2 / 5, 340), x = k * 340 - o;
    int by = 208 - sTop;
    if (by + 4 > 0) {
      fb.fillRect(x + 20, -10, 16, by + 10, RGB565(34, 46, 48));
      for (int i = 0; i < 3; i++) { int yy = 14 + i * 26 - sTop, ww = 40 - i * 6; fb.fillRoundRect(x + 28 - ww, yy, ww * 2, 8, 4, RGB565(34, 46, 48)); fb.fillRoundRect(x + 28 - ww / 2, yy - 5, ww, 8, 4, RGB565(34, 46, 48)); }
      fb.fillCircle(x + 84, by - 10, 14, RGB565(30, 40, 42)); fb.fillCircle(x + 100, by - 6, 10, RGB565(30, 40, 42)); fb.fillRect(x + 90, by - 4, 6, 4, RGB565(60, 50, 44));
      smLantern(x + 150, by, RGB565(38, 50, 52), RGB565(170, 244, 240));
      if (k & 1) smTorii(x + 250, by, 70, 92, RGB565(24, 32, 34));
    }
  }
  { int o = smMod(cam * 7 / 10, 640);                                  // branches du premier plan, en haut : arcs noirs epais
    for (int k = 0; k < 2; k++) { int x = k * 640 - o;
      for (int w = 0; w < 4; w++) { fb.drawLine(x + 330, -sTop + w, x + 450, 22 - sTop + w, RGB565(16, 22, 24)); fb.drawLine(x + 450, 22 - sTop + w, x + 600, 6 - sTop + w, RGB565(16, 22, 24)); }
      fb.drawLine(x + 470, 18 - sTop, x + 500, 44 - sTop, RGB565(16, 22, 24)); fb.drawLine(x + 540, 12 - sTop, x + 580, 40 - sTop, RGB565(16, 22, 24)); fb.drawLine(x + 400, 20 - sTop, x + 380, 46 - sTop, RGB565(16, 22, 24)); }
  }
  for (int i = 0; i < 7; i++) {                                        // brume : lueurs pales qui derivent
    int x = smMod(i * 131 + (int)(t * 6) - cam / 6, SCREEN_W + 40) - 20, y = 120 + (i * 37) % 70 - sTop;
    if (y > -6 && y < STRIP_H + 6) fb.fillRoundRect(x, y, 34, 5, 2, RGB565(238, 242, 240));
  }
}
void smMacePos(int i, float &ax, float &ay, float &bx, float &by) {   // balancier : chaine accrochee en (ax, ay), boule au bout qui oscille de +-0,77 rad (44 degres, comme le croquis)
  const int16_t *m = smL->mace[i]; float th = 0.77f * sinf(millis() * 0.0016f + i * 1.3f);
  ax = m[0]; ay = m[1]; bx = ax + m[2] * sinf(th); by = ay + m[2] * cosf(th);
}
void smBgImg(int sTop, int cam, int camY) {              // niveau 1 (grotte) : le decor de assets/niveau1.png a 300 % (chaque pixel de l'image = 3 x 3 pixels d'ecran)
  uint16_t *b = fb.getBuffer(); int lastY = -1;
  for (int row = 0; row < STRIP_H; row++) {
    uint16_t *o = b + row * SCREEN_W; int y = min(NIV1_H - 1, (sTop + camY + row) / 3);
    if (y == lastY) { memcpy(o, o - SCREEN_W, SCREEN_W * 2); continue; }                // meme ligne de l'image : on recopie la ligne du dessus
    lastY = y; const uint8_t *src = NIV1_PIX + y * (NIV1_W / 2);
    int sx = cam / 3, ph = cam - sx * 3;
    for (int x = 0; x < SCREEN_W; sx++, ph = 0) { uint8_t v = src[sx >> 1]; uint16_t c = NIV1_PAL[(sx & 1) ? (v & 15) : (v >> 4)]; for (int n = 3 - ph; n > 0 && x < SCREEN_W; n--) o[x++] = c; }
  }
}
void smDraw() {
  uint16_t *b = fb.getBuffer(); int cam = (int)smCam, camY = (int)smCamY; float t = millis() / 1000.0f;
  const uint16_t groundC = SM_CAVE ? RGB565(12, 14, 16) : SM_MIST ? RGB565(18, 24, 26) : SM_NIGHT ? RGB565(10, 12, 28) : RGB565(22, 14, 30), pitC = SM_MIST ? RGB565(120, 134, 134) : SM_NIGHT ? RGB565(34, 36, 84) : RGB565(54, 34, 70),
                 hiC = SM_CAVE ? RGB565(122, 134, 138) : SM_MIST ? RGB565(84, 112, 114) : SM_NIGHT ? RGB565(176, 194, 255) : RGB565(255, 220, 170);
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {
    const int wy = sTop + camY;                           // ordonnee du monde de la 1re ligne de la bande (camera verticale : grotte)
    if (SM_CAVE) smBgImg(sTop, cam, camY); else if (SM_MIST) smBgMist(sTop, cam, t); else smBgDusk(sTop, cam, t, SM_NIGHT ? 1 : 0);
    for (int x = 0; smL->ground && x < SCREEN_W; x++) {                  // sol (trous) : bord en ecailles de tuiles au niveau 1, pierre unie au niveau 2
      int wx = x + cam; int y0 = SM_GY - wy;
      if (smPit(wx)) { if (y0 < STRIP_H) fb.drawFastVLine(x, max(0, y0), STRIP_H - max(0, y0), pitC); continue; }
      static const int8_t SC[8] = { 2, 1, 0, 0, 0, 0, 1, 2 }; if (!SM_MIST) y0 += SC[wx & 7];
      if (y0 >= STRIP_H) continue;
      fb.drawFastVLine(x, max(0, y0), STRIP_H - max(0, y0), groundC);
      if (y0 >= 0) b[y0 * SCREEN_W + x] = hiC;
    }
    if (SM_MIST) for (int k = cam / 30 - 1; k <= (cam + SCREEN_W) / 30; k++) {   // paves ovales gris-bleu
      uint32_t h = gHash(k * 7 + 3); int wxs = k * 30 + (int)(h & 15), sy = SM_GY + 9 + (int)((h >> 4) % 22) - wy, w = 9 + (int)((h >> 9) % 14);
      if (!smPit(wxs + w / 2) && !smPit(wxs) && sy > -6 && sy < STRIP_H) fb.fillRoundRect(wxs - cam, sy, w, 5, 2, RGB565(30, 58, 60));
    }
    for (int i = 0; !SM_CAVE && i < smL->npl; i++) {      // plateformes (dans la grotte elles sont dans l'image) : toit a pointes relevees (niveau 1) / corniche de pierre (niveau 2)
      const int16_t *p = smL->plat[i]; int x0 = p[0] - cam, y0 = p[1] - wy, x1 = p[2] - cam, y1 = p[3] - wy, th = SM_CAVE ? 5 : SM_MIST ? 9 : 7;
      if (x1 < -14 || x0 > SCREEN_W + 14 || min(y0, y1) > STRIP_H + 8 || max(y0, y1) < -20) continue;
      fb.fillTriangle(x0, y0, x1, y1, x1, y1 + th, groundC); fb.fillTriangle(x0, y0, x1, y1 + th, x0, y0 + th, groundC);   // toit : bande inclinee
      fb.drawLine(x0, y0, x1, y1, hiC);
      if (SM_CAVE) { if (p[1] == p[3]) for (int cx2 = x0 + 8; cx2 < x1 - 4; cx2 += 26) for (int cy2 = y0 - 16; cy2 < y0; cy2 += 4) fb.drawPixel(cx2, cy2, hiC); }   // chaines pendues (decor)
      else if (!SM_MIST) {                                 // pointes relevees aux deux bouts, lanterne suspendue
        fb.fillTriangle(x0 - 12, y0 - 6, x0 + 2, y0, x0 + 2, y0 + th, groundC); fb.fillTriangle(x1 + 12, y1 - 6, x1 - 2, y1, x1 - 2, y1 + th, groundC);
        if (p[1] == p[3]) { fb.fillCircle((x0 + x1) / 2, y0 + 15, 3, RGB565(255, 196, 120)); fb.drawFastVLine((x0 + x1) / 2, y0 + th, 6, groundC); }
      } else if (p[1] == p[3]) { fb.fillRect(x0 + 6, y0 + th, 4, 6, groundC); fb.fillRect(x1 - 10, y0 + th, 4, 6, groundC); }
    }
    for (int i = 0; i < 2; i++) if (smL->mace[i][0]) {      // balanciers a pointes : chaine, boule hérissee
      float ax, ay, bx, by; smMacePos(i, ax, ay, bx, by);
      int x0 = (int)ax - cam, y0 = (int)ay - wy, x1 = (int)bx - cam, y1 = (int)by - wy;
      for (int w = -1; w <= 1; w++) fb.drawLine(x0 + w, y0, x1 + w, y1, w ? RGB565(78, 88, 92) : RGB565(150, 162, 166));
      fb.fillCircle(x1, y1, 15, RGB565(22, 26, 28)); fb.drawCircle(x1, y1, 15, RGB565(96, 106, 110));
      for (int k = 0; k < 8; k++) { float a = k * 0.785f + millis() * 0.002f; fb.fillTriangle(x1 + (int)(cosf(a - 0.3f) * 14), y1 + (int)(sinf(a - 0.3f) * 14), x1 + (int)(cosf(a + 0.3f) * 14), y1 + (int)(sinf(a + 0.3f) * 14), x1 + (int)(cosf(a) * 27), y1 + (int)(sinf(a) * 27), RGB565(170, 180, 182)); }
    }
    { int gx = (smL->ex ? smL->ex : SM_W - 50) - cam, gy = (smL->ex ? smL->ey : SM_GY) - wy;   // sortie : porte lumineuse (niveaux 1-3) / portique (niveau 4)
      if (SM_CAVE) {
        fb.fillRect(gx - 16, gy - 46, 32, 46, RGB565(18, 22, 24)); fb.fillRoundRect(gx - 10, gy - 40, 20, 40, 3, RGB565(214, 232, 232)); fb.fillRoundRect(gx - 6, gy - 36, 12, 36, 2, RGB565(248, 255, 255));
        fb.drawFastHLine(gx - 18, gy - 46, 36, RGB565(122, 134, 138));
      } else if (!SM_MIST) {
        fb.fillRect(gx - 44, gy - 90, 88, 90, RGB565(26, 16, 38)); fb.fillTriangle(gx - 54, gy - 88, gx + 54, gy - 88, gx, gy - 112, RGB565(26, 16, 38));
        fb.fillRoundRect(gx - 14, gy - 48, 28, 48, 3, RGB565(255, 214, 160)); fb.fillRoundRect(gx - 10, gy - 44, 20, 44, 2, RGB565(255, 244, 224));
        fb.fillCircle(gx - 32, gy - 60, 4, RGB565(255, 196, 120)); fb.fillCircle(gx + 32, gy - 60, 4, RGB565(255, 196, 120));
      } else { smTorii(gx, gy, 64, 86, RGB565(14, 20, 22)); smLantern(gx - 54, gy, RGB565(24, 34, 36), RGB565(170, 244, 240)); }
    }
    int fr = (int)(millis() / 70);                         // les animations de perso.gif tournent a 70 ms l'image
    for (int i = 0; i < smNE; i++) {
      SmEn &e = smE[i]; bool fr2 = e.x < smX;
      if (e.alive) { if (e.atk > 0) pDraw(fb, E_ATK, 3 + (int)((0.5f - e.atk) / 0.06f), (int)(e.x + 0.5f) - cam, (int)(e.y + 0.5f) - wy, fr2, 2); else pDraw(fb, E_WALK, fr + i * 7, (int)(e.x + 0.5f) - cam, (int)(e.y + 0.5f) - wy, e.vx > 0, 2); }
      else if (e.hitT > 0) pDraw(fb, E_DIE, (int)((0.3f - e.hitT) / 0.1f), (int)(e.x + 0.5f) - cam, (int)(e.y + 0.5f) - wy, fr2, 2, e.hitT > 0.2f ? RGB565(255, 255, 255) : 0);
    }
    if (smBoss.on && smBoss.alive && (smBoss.hitT <= 0 || ((int)(t * 20) & 1)) && (int)smBoss.x - cam > -70 && (int)smBoss.x - cam < SCREEN_W + 70) {   // le boss
      int f = smBoss.st == 1 ? min(2, (int)(smBoss.t / 0.15f)) : smBoss.st == 2 ? 3 + min(7, (int)(smBoss.t / 0.07f)) : 11 + (int)(millis() / 110) % 7;
      fxDraw(fb, BOSS_PIX + f * ((BOSS_W * BOSS_H + 1) / 2), BOSS_PAL, BOSS_W, BOSS_H, BOSS_AX, BOSS_AY, (int)(smBoss.x + 0.5f) - cam, SM_GY - wy, 1.0f, smBoss.right ? -1.0f : 1.0f, 0, 0);
    }
    for (int i = 0; i < 24; i++) if (smP[i].life > 0) { int z = smP[i].s * smP[i].life / 0.45f + 0.5f; if (z < 1) z = 1; fb.fillRect((int)smP[i].x - cam, (int)smP[i].y - wy, z, z, smP[i].c); }
    bool show = smInv <= 0 || ((int)(t * 12) & 1);
    if (show) {
      int px = (int)(smX + 0.5f) - cam, py = (int)(smY + 0.5f) - wy;                      // positions arrondies : le pixel ne depend plus de la camera tronquee
      for (int i = 2; i >= 0; i--) if (smTr[i].on) smDrawImg(fb, smTr[i].im, (int)(smTr[i].x + 0.5f) - cam, (int)(smTr[i].y + 0.5f) - wy, smTr[i].off, smTr[i].lean, i == 0 ? 1 : 2);   // images fantomes
      smDrawImg(fb, smImg, px, py, smAv.off, smAv.lean, 0);                                 // l'avatar (cape ou samourai)
      if (smAvKind == 0 && smAtk > 0) {                                                     // arc de l'epee de l'avatar a cape : balaye l'avant puis s'eteint
        float u = (0.36f - smAtk) / 0.36f, dr = smRight ? 1.0f : -1.0f; int cx0 = px + (int)(dr * 4), cy0 = py - 24;
        for (int j = 0; j < 7; j++) {
          float a0 = -1.3f + u * 2.0f + j * 0.11f, a1 = a0 + 0.11f; int r0 = 26 + j;
          fb.drawLine(cx0 + (int)(dr * r0 * cosf(a0)), cy0 + (int)(r0 * sinf(a0)), cx0 + (int)(dr * r0 * cosf(a1)), cy0 + (int)(r0 * sinf(a1)), j < 4 ? RGB565(255, 240, 200) : RGB565(255, 180, 90));
        }
      }
    }
    for (int i = 0; i < 3; i++) { int hx = 8 + i * 14, hy = 8 - sTop; uint16_t hc = i < smHp ? RGB565(235, 50, 70) : RGB565(80, 70, 90);
      fb.fillCircle(hx + 3, hy + 3, 3, hc); fb.fillCircle(hx + 8, hy + 3, 3, hc); fb.fillTriangle(hx, hy + 4, hx + 11, hy + 4, hx + 5, hy + 10, hc); }
    fb.setTextColor(SM_MIST ? RGB565(40, 50, 52) : RGB565(255, 240, 220)); char tb[40]; snprintf(tb, sizeof(tb), "NIV %d  %s  %d/%d", smLevel + 1, smAvKind ? "SAMOURAI" : "CAPE", smKills, smNE); gfxPrintAt(fb, 1, SCREEN_W - 8 - textW(fb, 1, tb), 8 - sTop, tb);
    if (smBoss.on && smBoss.alive) {
      fb.drawRect(SCREEN_W / 2 - 42, 20 - sTop, 84, 8, RGB565(255, 240, 220)); fb.fillRect(SCREEN_W / 2 - 40, 22 - sTop, (int)(80 * smBoss.hp / SM_BOSS_HP), 4, RGB565(220, 40, 50));
      fb.setTextColor(SM_MIST ? RGB565(40, 50, 52) : RGB565(255, 240, 220)); gfxPrintAt(fb, 1, SCREEN_W / 2 - 42 - 30, 20 - sTop, "BOSS");
    }
    if (smPh > 0) {
      const char *m = smPh == 1 ? "VAINCU..." : (smLevel < 3 ? "NIVEAU SUIVANT" : "VICTOIRE !"); int tw = textW(fb, 2, m);
      fb.fillRect(SCREEN_W / 2 - tw / 2 - 10, 96 - sTop, tw + 20, 32, RGB565(30, 20, 30)); fb.setTextColor(smPh == 1 ? RGB565(255, 90, 90) : RGB565(255, 220, 120)); gfxPrintAt(fb, 2, SCREEN_W / 2 - tw / 2, 101 - sTop, m);
    }
    dBlitPlain(sTop);
  }
}
bool samuraiStep(bool bl, bool br, bool bj, bool je) {   // false = on quitte
  static unsigned long last = 0; unsigned long now = millis();
  float dt = (now - last) / 1000.0f; last = now; if (dt <= 0 || dt > 0.1f) dt = 0.03f;
  if (bl && br) { smQuit += dt; if (smQuit > 1.0f) return false; } else smQuit = 0;
  { static float skipT = 0; if (bl && bj && !br) skipT += dt; else skipT = 0;   // essai : GAUCHE + SAUT tenus 0,7 s = niveau suivant
    if (skipT > 0.7f) { skipT = 0; smLevel = (smLevel + 1) % 4; smLoad(); } }
  if (smPh > 0) {
    smMsgT -= dt;
    if (smMsgT <= 0) { if (smPh == 1) smLoad(); else if (smLevel < 3) { smLevel++; smLoad(); } else if (je) return false; }
    smAvatarStep(dt); smDraw(); return true;
  }
  for (int i = 0; i < smNE; i++) if (smE[i].hitT > 0) smE[i].hitT -= dt;
  if (smInv > 0) smInv -= dt;
  const bool stun = smInv > 1.0f;                         // juste apres un coup recu : on glisse, sans controle
  int dir = (br ? 1 : 0) - (bl ? 1 : 0);
  if (dir && !(bl && br) && !stun) smRight = dir > 0;
  if (smGround) smCoy = 0.08f; else if (smCoy > 0) smCoy -= dt;   // delai de grace apres un bord
  if (smJBuf > 0) smJBuf -= dt;                                   // saut memorise
  if (je && smAtk <= 0) {                                 // un seul bouton : epee si un ennemi est a portee, sinon saut
    int near = -1; float best = 52;
    for (int i = 0; i < smNE; i++) if (smE[i].alive && fabsf(smE[i].y - smY) < 34 && fabsf(smE[i].x - smX) < best) { best = fabsf(smE[i].x - smX); near = i; }
    if (near >= 0) { smAtk = 0.36f; smRight = smE[near].x > smX; }
    else smJBuf = 0.10f;
  }
  if (smJBuf > 0 && smCoy > 0 && smAtk <= 0) { smVY = -540; smGround = false; smCoy = 0; smJBuf = 0; }   // apogee ~104 px
  if (!stun) {                                            // accelerations de la flamme du jeu principal : 2300 au sol, 1850 en l'air
    float target = smAtk > 0 && smGround ? 0 : dir * 120.0f, ax = (smGround ? 2300.0f : 1850.0f) * dt;
    if (smVX < target) smVX = min(target, smVX + ax); else if (smVX > target) smVX = max(target, smVX - ax);
  }
  if (smAtk > 0) {
    float a0 = smAtk; smAtk -= dt;
    if (a0 > 0.08f && smAtk <= 0.28f) for (int i = 0; i < smNE; i++) {   // la lame frappe pendant le coup
      SmEn &e = smE[i]; float dx = (e.x - smX) * (smRight ? 1 : -1);
      if (e.alive && dx > -6 && dx < 50 && fabsf(e.y - smY) < 34) { e.alive = false; e.hitT = 0.3f; smKills++; smPuff(e.x, e.y - 12, 6, 70); }
    }
    if (a0 > 0.08f && smAtk <= 0.28f && smBoss.on && smBoss.alive && smBoss.hitT <= 0) {   // le sabre touche le boss (6 coups)
      float dx = (smBoss.x - smX) * (smRight ? 1 : -1);
      if (dx > -6 && dx < 56 && fabsf(SM_GY - smY) < 44) {
        smBoss.hp--; smBoss.hitT = 0.45f; smBoss.x = constrain(smBoss.x + (smRight ? 16 : -16), 1060.0f, (float)(SM_W - 30)); smPuff(smBoss.x, SM_GY - 20, 8, 80);
        if (smBoss.hp <= 0) { smBoss.alive = false; smPuff(smBoss.x, SM_GY - 20, 24, 120); smPh = 2; smMsgT = 1.6f; }
      }
    }
  }
  float xmax = smBoss.on && smBoss.alive ? SM_W - 80.0f : SM_W - 10.0f;   // le boss garde la sortie
  float nx = smX + smVX * dt; if (nx < 10) nx = 10; if (nx > xmax) nx = xmax;
  for (int i = 0; i < 6; i++) { const int16_t *bx = smL->box[i]; if (bx[2] && nx + 6 > bx[0] && nx - 6 < bx[2] && smY - 2 > bx[1] && smY - 30 < bx[3]) { nx = smX; smVX = 0; } }   // blocs pleins (roche de la grotte)
  smX = nx;
  float g = 1400.0f; if (smVY < 0 && !bj) g *= 1.7f; else if (smVY > 0) g *= 1.12f;   // saut plus haut tant que le bouton est tenu
  smVY += g * dt; if (smVY > 540) smVY = 540;
  float ny = smY + smVY * dt, fl = smFloor(smX, smY);
  bool snap = smGround && smVY >= 0 && fl < 29000 && fl - smY < 8;   // on reste colle au toit en descendant une pente
  if (smVY >= 0 && (ny >= fl || snap) && smY <= fl + 6) { ny = fl; smVY = 0; smGround = true; } else smGround = false;
  smY = ny;
  if (smY > smL->h + 40) { smPh = 1; smMsgT = 1.4f; }    // tombe dans un trou / dans le vide de la grotte
  for (int i = 0; i < smNE; i++) {                        // les fantomes font leur ronde et foncent sur le joueur s'il approche
    SmEn &e = smE[i]; if (!e.alive) continue;
    if (e.atk > 0) {                                      // le fantome frappe (sa lame part vers le joueur)
      float a0 = e.atk; e.atk -= dt;
      if (a0 > 0.25f && e.atk <= 0.25f && smInv <= 0 && fabsf(e.x - smX) < 40 && fabsf(e.y - smY) < 30) {
        smHp--; smInv = 1.2f; smVX = (smX < e.x ? -1 : 1) * 200; smVY = -240; smGround = false;
        if (smHp <= 0) { smPh = 1; smMsgT = 1.4f; }
      }
      continue;
    }
    if (fabsf(e.x - smX) < 120 && fabsf(e.y - smY) < 40) e.vx = (smX > e.x ? 1 : -1) * 42.0f;
    if (fabsf(e.x - smX) < 34 && fabsf(e.y - smY) < 30 && random(0, 100) < 2) { e.atk = 0.5f; continue; }   // a portee : il attaque parfois
    e.x += e.vx * dt; if (e.x < e.x0) { e.x = e.x0; e.vx = fabsf(e.vx); } if (e.x > e.x1) { e.x = e.x1; e.vx = -fabsf(e.vx); }
    if (smInv <= 0 && fabsf(e.x - smX) < 14 && fabsf(e.y - smY) < 26) {
      smHp--; smInv = 1.2f; smVX = (smX < e.x ? -1 : 1) * 200; smVY = -240; smGround = false;
      if (smHp <= 0) { smPh = 1; smMsgT = 1.4f; }
    }
  }
  for (int i = 0; i < 2; i++) if (smL->mace[i][0] && smInv <= 0 && smPh == 0) {   // balancier : un coup si on touche la boule
    float ax, ay, bx, by; smMacePos(i, ax, ay, bx, by);
    if (fabsf(bx - smX) < 24 && fabsf(by - (smY - 20)) < 30) { smHp--; smInv = 1.2f; smVX = (smX < bx ? -1 : 1) * 200; smVY = -240; smGround = false; if (smHp <= 0) { smPh = 1; smMsgT = 1.4f; } }
  }
  smBossStep(dt);
  bool atExit = smL->ex ? (fabsf(smX - smL->ex) < 22 && fabsf(smY - smL->ey) < 40) : smX > SM_W - 60;
  if (atExit && !(smBoss.on && smBoss.alive) && smPh == 0) { smPh = 2; smMsgT = smLevel < 3 ? 1.0f : 0.8f; }
  smCam += ((smX + smVX * 0.3f - SCREEN_W / 3) - smCam) * min(1.0f, dt * 6); smCam = constrain(smCam, 0.0f, (float)(SM_W - SCREEN_W));   // la camera anticipe le mouvement
  smCamY += ((smY - SCREEN_H * 0.62f + smVY * 0.12f) - smCamY) * min(1.0f, dt * 5); smCamY = constrain(smCamY, 0.0f, (float)(smL->h - SCREEN_H));   // verticale : grotte
  smAvatarStep(dt);
  smDraw();
  return true;
}

// 2 joueurs : l'adversaire ne repond plus ou a quitte -> message, puis retour a l'accueil (ESP-NOW reste actif)
void peerLost(const char *why) {
  if (state == ST_DRIFT) { driftFree(); tft.setSPISpeed(40000000); }
  pongTwoP = false; raceMode = false; twoP = false; vPeer = false; vPongQuit = false; vStart = false; mpFrozen = false; mpN = 0;
  DPRINTF("[2J] retour menu : %s (dernier paquet il y a %lu ms)\n", why, (unsigned long)(millis() - vLastRx));
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextSize(2); tft.setTextColor(COL_STAR); ctext(84, why);
  { char b[40]; if (timingFinalScore(b, sizeof b)) { tft.setTextSize(2); tft.setTextColor(COL_TXT); ctext(118, b); } }   // timing a deux : score final
  tft.setTextSize(1); tft.setTextColor(COL_TXT); ctext(150, "Retour au menu");
  delay(2500); homeChoice = 0; state = ST_HOME; drawHome(); prevJump = true;
}
void twoPQuit() { for (int k = 0; k < 3; k++) { espSend(M_PQUIT, 0, 0, 0, 0); delay(5); } twoP = false; vPeer = false; mpFrozen = false; mpN = 0; homeChoice = 0; state = ST_HOME; drawHome(); }
// liste des joueurs envoyee par l'hote : ids actifs (hote en x2, puis x, y, t) ; elle fait foi (elle remet aussi ceux qu'on croyait partis)
void mpApplyRoster() {
  vRoster = false;
  const uint16_t ids[4] = { vRosterIds[2], vRosterIds[0], vRosterIds[1], vRosterIds[3] }; const int n = min((int)vRosterN, 4); const uint8_t cols = vRosterCol;
  bool me = false; for (int q = 0; q < n; q++) if (ids[q] == mpMe) me = true;
  if (!me) return;                                         // je n'y suis pas (encore) : l'hote m'admettra en m'entendant
  const uint32_t now = millis();
  for (int k = 0; k < mpN; k++) {
    bool in = false; for (int q = 0; q < n; q++) if (ids[q] == mpId[k]) in = true;
    if (in && !(mpIn[k] && mpOn[k])) mpRx[k] = now;        // (re)venu : on repart de maintenant
    mpIn[k] = mpOn[k] = in; if (in) mpQuit[k] = false;
  }
  for (int q = 0; q < n; q++) {                            // couleurs donnees par l'hote
    const uint8_t c = (cols >> (q * 2)) & 3;
    if (ids[q] == mpMe) myCol = c; else { const int k = mpFind(ids[q]); if (k >= 0) mpCol[k] = c; }
  }
  if (!mpFrozen) { mpFrozen = true; isHost = false; if (state == ST_LOBBY) drawLobby(); }
  mpHostId = ids[0]; mpAddPeers();
}
void mpSendRoster(int reps) {
  uint16_t ids[4] = { mpMe, 0, 0, 0 }; uint8_t cols = myCol; int n = 1;
  for (int k = 0; k < mpN && n < 4; k++) if (mpIn[k] && mpOn[k]) { cols |= (mpCol[k] & 3) << (n * 2); ids[n++] = mpId[k]; }
  for (int q = 0; q < reps; q++) {
    Msg m{}; m.type = M_ROSTER; m.lvl = (uint8_t)n; m.x = (int16_t)ids[1]; m.y = (int16_t)ids[2]; m.t = ids[3]; m.x2 = (int16_t)ids[0]; m.b1 = cols; mpSendMsg(m);
    if (reps > 1) delay(4);
  }
}
void mpResendGame(int k) {                                 // le jeu en cours, au seul joueur qu'on vient d'admettre
  for (int q = 0; q < 3; q++) {
    if (state == ST_TIMING) mpSendTo(k, M_START, TIMING_SENTINEL, (int16_t)timingTarget(), 0, 0, 0);
    else if (state == ST_DRIFT) { mpSendTo(k, M_START, DRIFT_SENTINEL, 0, 0, 0, 0); mpSendTo(k, M_DSEL, (uint8_t)driftTrack(), 0, 0, 0, 0); }
    else if ((state == ST_PLAY || state == ST_COUNTDOWN) && curLevel == CUSTOM_IDX) mpSendTo(k, M_START, BRIQUE_SENTINEL, 0, 0, raceCode, 0);
    else if (state == ST_PLAY || state == ST_COUNTDOWN) mpSendTo(k, M_START, (uint8_t)curLevel, 0, 0, 0, 0);
    delay(5);
  }
}
void mpHostKeep() {                                        // hote : admet les consoles qui arrivent ou reviennent, rediffuse la liste toutes les 2 s
  static uint32_t lastRoster = 0; const uint32_t now = millis(); int adm = -1;
  for (int k = 0; k < mpN; k++) {
    if (mpIn[k] && mpOn[k]) continue;
    const uint32_t r = mpRx[k], h = mpHelloMs[k];
    const bool alive = (int32_t)(now - r) < 1500, wants = alive && (!mpQuit[k] || (h && (int32_t)(now - h) < 1000));
    if (wants && mpActive() + 1 < 4 && state != ST_PONG) {   // mpFin : ne bloque pas la relance du drift ; couleur : la premiere libre
      bool used[4] = { false, false, false, false }; used[myCol & 3] = true; for (int q = 0; q < mpN; q++) if (mpIn[q] && mpOn[q]) used[mpCol[q] & 3] = true;
      int c = 0; while (c < 3 && used[c]) c++; mpCol[k] = c;
      mpIn[k] = mpOn[k] = true; mpQuit[k] = false; mpFin[k] = true; mpTmOk[k] = false; adm = k; break;
    }
  }
  if (adm >= 0 || now - lastRoster > 2000) { lastRoster = now; mpAddPeers(); mpSendRoster(adm >= 0 ? 3 : 1); }
  if (adm >= 0) { DPRINTF("[MP] %s admis (en cours de partie)\n", MP_NAME[mpCol[adm] & 3]); mpResendGame(adm); }
}
void mpReset() {                                           // entree dans le salon multijoueur : on oublie tout
  twoP = true; vPeer = false; vStart = false; vFin = false; vPos = false; vRoster = false; vFinWho = -1; mpFrozen = false; mpN = 0; vSessionMs = 0; isHost = false; lobbyInMs = millis();
  myHelloCount = 0; lastHello = 0; lobbyWasPeer = false;
}

// BRIQUE : casse-brique (rows = 7 lignes de 10 : '.', '1'-'3' solidite, 'w' acier) ou invaders (4 lignes de 8 : '.', 'E')
const char* bqMiniParse(const char* s, int jeu) {
  const int H = jeu == 1 ? 7 : 4, W = jeu == 1 ? 10 : 8; const char* ok = jeu == 1 ? ".123w" : ".E";
  const char* p = strstr(s, "\"rows\""); if (!p) return "pas de rows";
  p = strchr(p, '['); if (!p) return "rows mal forme";
  p++;
  char tmp[7][11]; memset(tmp, '.', sizeof tmp); int h = 0, n = 0;
  while (true) {
    while (*p == ' ' || *p == ',') p++;
    if (*p == ']') break;
    if (*p != '"' || h >= H) return "rows mal forme";
    p++;
    int c = 0;
    for (; *p && *p != '"'; p++, c++) { if (c >= W || !strchr(ok, *p)) return "brique inconnue"; tmp[h][c] = *p; n += *p != '.' && *p != 'w'; }
    if (*p != '"') return "ligne non fermee";
    p++; h++;
  }
  if (!n) return jeu == 1 ? "aucune brique a casser" : "aucun ennemi";
  memcpy(bqMiniRows, tmp, sizeof tmp); bqMiniJeu = jeu;
  const char* q = strstr(s, "\"params\""); if (!q) q = s;
  bqMiniVit = bqNum(q, "\"vitesse\":", 0.6f, 1.6f); bqMiniVies = (int)bqNum0(q, "\"vies\":", 0, 9);
  bqMiniVagues = max(1, (int)bqNum0(q, "\"vagues\":", 0, 9)); bqMiniDiff = (int)bqNum0(q, "\"difficulte\":", 0, 3); if (!bqMiniDiff) bqMiniDiff = 2;
  bqMiniBonus = !strstr(q, "\"bonus\":false"); bqMiniFond = bqHex(q, "\"ciel\":");
  jsonStr(s, "titre", bqTitre, sizeof bqTitre); jsonStr(s, "message", bqMsg, sizeof bqMsg);
  return nullptr;
}
// lance le dernier jeu decode (briqueParse) : niveau de plateforme, casse-brique ou invaders
void bqStart() {
  if (!bqMiniJeu) { startLevel(CUSTOM_IDX, true); return; }
  bqMini = bqMiniJeu;
  if (bqMini == 1) startBreakout(); else startInvaders();
}

// BRIQUE : jeu de cote : 15 ou 30 lignes de 20 a 96 cases, '.#SG*^ET'
const char* bqCoteParse(const char* s) {
  bqMiniJeu = 0;
  const char* p = strstr(s, "\"rows\""); if (!p) return "pas de rows";
  p = strchr(p, '['); if (!p) return "rows mal forme";
  p++;
  static char tmp[BQC_HMAX][97]; int h = 0, w = 0, nS = 0, nG = 0;
  while (true) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
    if (*p == ']') break;
    if (*p != '"') return "rows mal forme";
    if (h >= BQC_HMAX) return "niveau trop haut";
    p++; int c = 0;
    for (; *p && *p != '"'; p++, c++) {
      if (c >= 96) return "niveau trop large";
      if (!strchr(".#SG*^ETH", *p)) return "brique inconnue";
      nS += *p == 'S'; nG += *p == 'G'; tmp[h][c] = *p;
    }
    if (*p != '"') return "ligne non fermee";
    p++; tmp[h][c] = 0; w = max(w, c); h++;
  }
  if (h < 15 || w < 20) return "niveau trop petit (15 x 20 au moins)";
  if (nS != 1 || nG != 1) return "il faut 1 depart S et 1 arrivee G";
  for (int r = 0; r < h; r++) { int c = strlen(tmp[r]); while (c < w) tmp[r][c++] = '.'; tmp[r][w] = 0; }
  memcpy(bqCRows, tmp, sizeof tmp); bqCW = w; bqCH = h;
  bqParams(s);
  bqCote = true; customH = h;                       // customH : le resultat (BRIQUE FIN) repart vers la page
  return nullptr;
}
// BRIQUE : decode "rows" dans une ligne JSON. Renvoie nullptr si OK, sinon la raison (renvoyee a la page web).
const char* briqueParse(const char* s) {
  char jeu[10]; jsonStr(s, "jeu", jeu, sizeof jeu);
  bqCote = bqIso = bqFiche = false;
  if (!strcmp(jeu, "fiche")) return bqFicheParse(s);
  if (!strcmp(jeu, "cote")) return bqCoteParse(s);
  if (!strcmp(jeu, "iso")) return bqIsoParse(s);
  if (!strcmp(jeu, "briques") || !strcmp(jeu, "invaders")) return bqMiniParse(s, jeu[0] == 'b' ? 1 : 2);
  bqMiniJeu = 0;
  const char* p = strstr(s, "\"rows\""); if (!p) return "pas de rows";
  p = strchr(p, '['); if (!p) return "rows mal forme";
  p++;
  int h = 0, nS = 0, nG = 0;
  char (*tmp)[COLS + 1] = bqTmp;
  while (true) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
    if (*p == ']') break;
    if (*p != '"') return "rows mal forme";
    if (h >= WROWS) return "niveau trop haut";
    p++;
    int c = 0;
    for (; *p && *p != '"'; p++, c++) {
      if (c >= COLS) return "ligne trop longue";
      if (!strchr(".#SG*R^WCIEH", *p)) return "brique inconnue";
      nS += *p == 'S'; nG += *p == 'G';
      tmp[h][c] = *p;
    }
    if (*p != '"') return "ligne non fermee";
    p++;
    while (c < COLS) tmp[h][c++] = '.';
    tmp[h][COLS] = 0; h++;
  }
  if (h < 3) return "niveau trop petit";
  if (nS != 1 || nG != 1) return "il faut 1 depart S et 1 arrivee G";
  memcpy(customRows, bqTmp, sizeof bqTmp);
  for (int r = 0; r < WROWS; r++) customPtr[r] = customRows[r];
  customH = h;
  customDJ = strstr(s, "\"doubleSaut\":true") != nullptr;
  bqParams(s);
  return nullptr;
}

// ---- BRIQUE "iso" : vue isometrique (cases en losange, avec des hauteurs). G/D = tourner, P3 = sauter d'une case ----
// rows : '.' trou, '#' sol, 'S' depart, 'G' arrivee, '*' etoile, '^' piques, 'P' piques qui sortent 1 s sur 2,
// 'H' pic cache (sort 1,2 s quand on arrive a cote, puis rentre jusqu'a ce qu'on s'eloigne) ; haut : un chiffre 0-4 par case
// On monte d'une marche a la fois, on descend de partout. Trou ou piques = retour au depart.
char isoR[ISO_N][ISO_N + 1], isoC[ISO_N][ISO_N]; uint8_t isoHt[ISO_N][ISO_N]; int isoW = 0, isoH = 0, isoMH = 0, isoNS = 0;
int isoX, isoY, isoSX, isoSY, isoFX, isoFY, isoDir, isoDir0, isoStars;   // isoF = case de depart du saut ; isoDir : 0 haut-droite, 1 bas-droite, 2 bas-gauche, 3 haut-gauche
uint32_t isoHUp[ISO_N][ISO_N];                     // pic cache : millis() de sa sortie, 0 = arme
float isoT = 1, isoMort = 0, isoHold = 0, isoInfoT = 0; bool isoChute = false; const char* isoInfo = ""; uint32_t isoLast = 0;
const int8_t ISO_DX[4] = { 0, 1, 0, -1 }, ISO_DY[4] = { -1, 0, 1, 0 };
const char* bqIsoParse(const char* s) {
  bqMiniJeu = 0;
  static char tmp[ISO_N][ISO_N + 1]; static uint8_t ht[ISO_N][ISO_N]; memset(ht, 0, sizeof ht);
  int h = 0, w = 0, nS = 0, nG = 0;
  const char* p = strstr(s, "\"rows\""); if (!p) return "pas de rows";
  p = strchr(p, '['); if (!p) return "rows mal forme";
  p++;
  while (true) {
    while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
    if (*p == ']') break;
    if (*p != '"') return "rows mal forme";
    if (h >= ISO_N) return "niveau trop grand (12 x 12 au plus)";
    p++; int c = 0;
    for (; *p && *p != '"'; p++, c++) {
      if (c >= ISO_N) return "niveau trop grand (12 x 12 au plus)";
      if (!strchr(".#SG*^PH", *p)) return "brique inconnue";
      nS += *p == 'S'; nG += *p == 'G'; tmp[h][c] = *p;
    }
    if (*p != '"') return "ligne non fermee";
    p++; tmp[h][c] = 0; w = max(w, c); h++;
  }
  if (h < 3 || w < 3) return "niveau trop petit (3 x 3 au moins)";
  if (nS != 1 || nG != 1) return "il faut 1 depart S et 1 arrivee G";
  p = strstr(s, "\"haut\""); if (p) p = strchr(p, '[');   // hauteurs (facultatif, 0 sinon)
  for (int r = 0; p && r < ISO_N; r++) {
    p++; while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') break;
    for (int c = 0; *++p && *p != '"'; c++) {
      if (*p < '0' || *p > '4') return "hauteur : 0 a 4";
      if (c < ISO_N) ht[r][c] = *p - '0';
    }
    if (*p != '"') return "haut mal forme";
  }
  for (int r = 0; r < h; r++) { int c = strlen(tmp[r]); while (c < w) tmp[r][c++] = '.'; tmp[r][w] = 0; }
  memcpy(isoR, tmp, sizeof tmp); memcpy(isoHt, ht, sizeof ht); isoW = w; isoH = h;
  bqParams(s);
  bqIso = true; customH = h;                        // customH : le resultat (BRIQUE FIN) repart vers la page
  return nullptr;
}
void isoRespawn() { isoX = isoFX = isoSX; isoY = isoFY = isoSY; isoT = 1; isoMort = 0; isoDir = isoDir0; memset(isoHUp, 0, sizeof isoHUp); }
void startBqIso() {
  curLevel = CUSTOM_IDX; deaths = 0; bqTimeout = false; isoStars = 0; isoNS = 0; isoMH = 0; isoHold = 0; isoInfoT = 0;
  for (int r = 0; r < isoH; r++) for (int c = 0; c < isoW; c++) {
    char ch = isoR[r][c]; isoC[r][c] = ch; isoNS += ch == '*';
    if (ch == 'S') { isoSX = c; isoSY = r; }
    if (ch == 'G') { isoFX = c; isoFY = r; }
    if (ch != '.') isoMH = max(isoMH, (int)isoHt[r][c]);
  }
  const int dx = isoFX - isoSX, dy = isoFY - isoSY;   // au depart, le heros regarde vers l'arrivee
  isoDir0 = abs(dx) >= abs(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
  isoRespawn(); levelStartMs = isoLast = millis();
  state = ST_ISO;
}
bool isoPiq(int x, int y) { char ch = isoC[y][x]; return ch == '^' || (ch == 'P' && (millis() / 1000) & 1) || (ch == 'H' && isoHUp[y][x] && millis() - isoHUp[y][x] < 1200); }
void isoCaches() {                                   // le heros vient d'atterrir : les pics caches d'a cote sortent, les autres se rearment
  for (int r = 0; r < isoH; r++) for (int c = 0; c < isoW; c++) if (isoC[r][c] == 'H') {
    if (abs(r - isoY) + abs(c - isoX) != 1) isoHUp[r][c] = 0;
    else if (!isoHUp[r][c]) isoHUp[r][c] = millis() | 1;
  }
}
void isoDie(bool chute) { snd(SND_DIE); deaths++; isoMort = 0.8f; isoChute = chute; }
uint16_t isoCol(long rgb, int pct) { return RGB565(min(255L, (rgb >> 16) * pct / 100), min(255L, ((rgb >> 8) & 255) * pct / 100), min(255L, (rgb & 255) * pct / 100)); }
void isoDraw() {
  const int n = isoW + isoH, hw = min(16, 300 / n), hh = hw / 2, zh = hh;
  const int ox = SCREEN_W / 2 - (isoW - isoH) * hw / 2, oy = 18 + isoMH * zh + (214 - n * hh - zh - isoMH * zh) / 2;
  const long sol = bqSol >= 0 ? bqSol : 0x5CB45AL;
  const uint16_t ciel = bqCiel >= 0 ? RGB565(bqCiel >> 16, (bqCiel >> 8) & 255, bqCiel & 255) : RGB565(28, 32, 58);
  const bool idle = isoT >= 1 && isoMort <= 0;
  const int tx = isoX + ISO_DX[isoDir], ty = isoY + ISO_DY[isoDir];
  // heros : en arc entre la case de depart et la case d'arrivee ; il tombe dans un trou, clignote sur des piques
  const float t = isoT; const int fh = isoHt[isoFY][isoFX], th = isoC[isoY][isoX] == '.' ? 0 : isoHt[isoY][isoX];
  const int hx = ox + (int)(((isoFX - isoFY) + ((isoX - isoY) - (isoFX - isoFY)) * t) * hw);
  int hy = oy + hh + (int)(((isoFX + isoFY) + ((isoX + isoY) - (isoFX + isoFY)) * t) * hh - (fh + (th - fh) * t) * zh - sinf(t * 3.14159f) * 10);
  if (isoMort > 0 && isoChute) hy += (int)((0.8f - isoMort) * 160);
  const int hs = max(isoFX + isoFY, isoX + isoY);  // heros dessine apres cette diagonale : les cases de devant le cachent
  const bool hVis = !(isoMort > 0 && !isoChute && ((millis() / 80) & 1));
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {
    fb.fillRect(0, 0, SCREEN_W, STRIP_H, ciel);
    for (int s = 0; s <= n - 2; s++) {
      for (int r = max(0, s - isoW + 1); r <= min(s, isoH - 1); r++) {
        const int c = s - r; const char ch = isoC[r][c]; if (ch == '.') continue;
        const int h = isoHt[r][c], x = ox + (c - r) * hw, y = oy + s * hh - h * zh - sTop, dz = (h + 1) * zh;
        if (y > STRIP_H || y + 2 * hh + dz < -24) continue;
        const uint16_t cl = isoCol(sol, 62 + h * 6), cr = isoCol(sol, 45 + h * 6), ct = isoCol(sol, 85 + h * 10);
        fb.fillTriangle(x - hw, y + hh, x, y + 2 * hh, x, y + 2 * hh + dz, cl); fb.fillTriangle(x - hw, y + hh, x - hw, y + hh + dz, x, y + 2 * hh + dz, cl);
        fb.fillTriangle(x + hw, y + hh, x, y + 2 * hh, x, y + 2 * hh + dz, cr); fb.fillTriangle(x + hw, y + hh, x + hw, y + hh + dz, x, y + 2 * hh + dz, cr);
        fb.fillTriangle(x - hw, y + hh, x, y, x + hw, y + hh, ct); fb.fillTriangle(x - hw, y + hh, x, y + 2 * hh, x + hw, y + hh, ct);
        const int cy = y + hh;
        if (ch == '*') { int b = cy - 7 + (int)(sinf(millis() / 200.0f + c) * 2); fb.fillTriangle(x - 4, b + 2, x + 4, b + 2, x, b - 5, COL_STAR); fb.fillTriangle(x - 4, b - 2, x + 4, b - 2, x, b + 5, COL_STAR); }
        else if (ch == 'G') { fb.drawFastVLine(x, cy - 20, 20, COL_TXT); fb.fillTriangle(x + 1, cy - 20, x + 1, cy - 10, x + 11, cy - 15, COL_GOAL); }
        else if (ch == '^' || ch == 'P' || ch == 'H') {
          if (isoPiq(c, r)) for (int k = -1; k <= 1; k++) fb.fillTriangle(x + k * 5 - 2, cy + 2, x + k * 5 + 2, cy + 2, x + k * 5, cy - 6, RGB565(220, 220, 230));
          else if (ch == 'P') for (int k = -1; k <= 1; k++) fb.fillRect(x + k * 5 - 1, cy, 3, 2, RGB565(40, 40, 50));   // 'H' rentre : invisible
        }
        if (idle && c == tx && r == ty) { fb.drawLine(x - hw, y + hh, x, y, COL_TXT); fb.drawLine(x, y, x + hw, y + hh, COL_TXT); fb.drawLine(x + hw, y + hh, x, y + 2 * hh, COL_TXT); fb.drawLine(x, y + 2 * hh, x - hw, y + hh, COL_TXT); }   // case visee
      }
      if (s == hs && hVis) {
        const int y = hy - sTop, sd = ISO_DX[isoDir] - ISO_DY[isoDir];
        fb.fillRoundRect(hx - 5, y - 14, 10, 13, 3, isoMort > 0 && !isoChute ? COL_SPLAT : COL_MEAT);
        if (isoDir == 1 || isoDir == 2) { fb.fillRect(hx + sd * 2 - 3, y - 11, 2, 3, COL_TXT); fb.fillRect(hx + sd * 2 + 1, y - 11, 2, 3, COL_TXT); }   // de face : les yeux ; de dos : rien
      }
    }
    fb.setTextSize(1); fb.setTextColor(COL_TXT);
    fb.setCursor(4, 4 - sTop); fb.print(bqTitre[0] ? bqTitre : "ISO");
    char b[32]; snprintf(b, sizeof b, "* %d/%d  morts %d", isoStars, isoNS, (int)deaths); fb.setCursor(SCREEN_W - 6 * (int)strlen(b) - 4, 4 - sTop); fb.print(b);
    if (bqChrono) { long r = bqChrono - (long)(millis() - levelStartMs) / 1000; snprintf(b, sizeof b, "%lds", max(0L, r)); fb.setCursor(SCREEN_W / 2 - 6, 4 - sTop); fb.print(b); }
    if (isoInfoT > 0) { fb.setTextColor(COL_STAR); fb.setCursor(SCREEN_W / 2 - (int)strlen(isoInfo) * 3, 18 - sTop); fb.print(isoInfo); }
    fb.setTextColor(RGB565(150, 160, 180)); fb.setCursor(14, 230 - sTop); fb.print("G/D = tourner   P3 = sauter (maintenir = quitter)");
    dBlitPlain(sTop);
  }
}
#ifdef ISO_BENCH                                     // test sans les mains (compilation de test seulement : -DISO_BENCH) : direction du 1er pas d'un plus court chemin vers G
int isoAuto() {
  static int8_t from[ISO_N * ISO_N]; static uint8_t q[ISO_N * ISO_N]; memset(from, -1, sizeof from);
  int n = 0, i = 0, g = -1; q[n++] = isoY * ISO_N + isoX; from[q[0]] = 4;
  while (i < n && g < 0) {
    const int y = q[i] / ISO_N, x = q[i] % ISO_N; i++;
    for (int d = 0; d < 4; d++) {
      const int Y = y + ISO_DY[d], X = x + ISO_DX[d];
      if (Y < 0 || X < 0 || Y >= isoH || X >= isoW || from[Y * ISO_N + X] >= 0 || isoC[Y][X] == '.' || isoC[Y][X] == '^' || isoHt[Y][X] > isoHt[y][x] + 1) continue;
      from[Y * ISO_N + X] = d; q[n++] = Y * ISO_N + X; if (isoC[Y][X] == 'G') g = Y * ISO_N + X;
    }
  }
  if (g < 0) return -1;
  while (true) {
    const int d = from[g], p = (g / ISO_N - ISO_DY[d]) * ISO_N + g % ISO_N - ISO_DX[d];
    if (from[p] == 4) return d;
    g = p;
  }
}
#endif
// une image du jeu iso ; false = quitter vers l'accueil
bool isoStep(bool bj, bool eL, bool eR, bool eJ) {
  const uint32_t now = millis(); float dt = (now - isoLast) / 1000.0f; isoLast = now; if (dt > 0.1f) dt = 0.1f;
  isoHold = bj ? isoHold + dt : 0; if (isoHold > 1.5f) return false;
  if (bqChrono && now - levelStartMs > (unsigned long)bqChrono * 1000) {   // temps ecoule : comme les autres jeux
    snd(SND_DIE); deaths++; deadChoice = 0; bqTimeout = true; dSpiStop(); tft.setSPISpeed(40000000); state = ST_DEAD; drawDead(); return true;
  }
  if (isoInfoT > 0) isoInfoT -= dt;
  if (isoMort > 0) {
    isoMort -= dt;
    if (isoMort <= 0) {
      if (bqVies && (int)deaths >= bqVies) { deadChoice = 0; dSpiStop(); tft.setSPISpeed(40000000); state = ST_DEAD; drawDead(); return true; }
      isoRespawn();
    }
  } else if (isoT < 1) {
    isoT += dt / 0.22f;
    if (isoT >= 1) {                                 // atterrissage
      isoT = 1; char &ch = isoC[isoY][isoX];
      isoCaches();
      if (ch == '.') isoDie(true);
      else if (isoPiq(isoX, isoY)) isoDie(false);
      else if (ch == '*') { ch = '#'; isoStars++; snd(SND_STAR); }
      else if (ch == 'G') {
        if (bqTout && isoStars < isoNS) { isoInfo = "Prends toutes les etoiles !"; isoInfoT = 2; }
        else {
          dSpiStop(); tft.setSPISpeed(40000000);
          if (bqNQ) bqStartQuiz(isoStars); else finishLevel(isoStars);   // question de cours avant la victoire, comme le jeu de cote
          return true;
        }
      }
    }
  } else {
#ifdef ISO_BENCH
    { const int d = isoAuto(); eL = false; eR = d >= 0 && d != isoDir; eJ = d == isoDir; }
#endif
    if (eL) isoDir = (isoDir + 3) & 3;
    if (eR) isoDir = (isoDir + 1) & 3;
    if (eJ) {
      const int nx = isoX + ISO_DX[isoDir], ny = isoY + ISO_DY[isoDir];
      if (nx < 0 || ny < 0 || nx >= isoW || ny >= isoH) { isoInfo = "Le bord !"; isoInfoT = 1; }
      else if (isoC[ny][nx] != '.' && isoHt[ny][nx] > isoHt[isoY][isoX] + 1) { isoInfo = "Trop haut ! Une marche a la fois"; isoInfoT = 1.5f; }
      else { isoFX = isoX; isoFY = isoY; isoX = nx; isoY = ny; isoT = 0; snd(SND_JUMP); }
    } else if (isoPiq(isoX, isoY)) isoDie(false);    // les piques sortent sous les pieds
  }
  isoDraw();
  return true;
}

// ---- BRIQUE "fiche" : le jeu decrit par l'enfant (brique/FICHE-JEU.md), joue dans le moteur de plateforme ----
// Les lettres de la grille sont traduites en briques du moteur ('#','I','C','R','^','*','G','E','H') ; ce qui bouge devient un objet fob[].
// Ce qui n'est pas ecrit n'existe pas : sans "boutons", le heros ne bouge pas ; sans "saut", il ne saute pas ; sans "gravite", rien ne tombe.
static const char* jFin(const char* v) {             // fin d'une valeur JSON (objet, liste, texte ou nombre)
  if (*v == '"') { v++; while (*v && *v != '"') v++; return *v ? v + 1 : v; }
  if (*v != '{' && *v != '[') { while (*v && *v != ',' && *v != '}' && *v != ']') v++; return v; }
  int d = 0;
  for (; *v; v++) {
    if (*v == '"') { v++; while (*v && *v != '"') v++; if (!*v) return v; continue; }
    if (*v == '{' || *v == '[') d++;
    else if ((*v == '}' || *v == ']') && --d == 0) return v + 1;
  }
  return v;
}
static const char* jVal(const char* p, const char* fin, const char* cle) {   // valeur de "cle" dans [p, fin), au premier niveau seulement
  const int l = strlen(cle);
  if (*p == '{') p++;
  while (p < fin) {
    while (p < fin && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r')) p++;
    if (p >= fin || *p != '"') return nullptr;
    const char* k = p + 1; const char* ke = jFin(p);
    p = ke; while (p < fin && (*p == ' ' || *p == ':')) p++;
    if (ke - k - 1 == l && !strncmp(k, cle, l)) return p;
    p = jFin(p);
  }
  return nullptr;
}
static float jNum(const char* p, const char* fin, const char* cle, float def, float lo, float hi) {
  const char* v = jVal(p, fin, cle); if (!v) return def;
  float x = (*v == 't') ? 1 : (*v == 'f') ? 0 : atof(v); return x < lo ? lo : x > hi ? hi : x;
}
// "cle":"mot" ou "cle":{"mot":valeur} -> indice du mot dans mots (-1 si absent) ; arg = la valeur qui suit le mot (nullptr sinon)
static int jMotArg(const char* p, const char* fin, const char* cle, const char* const* mots, int n, const char*& arg) {
  arg = nullptr; const char* v = jVal(p, fin, cle); if (!v) return -1;
  if (*v == '{') { v++; while (*v == ' ') v++; }
  if (*v != '"') return -1;
  for (int i = 0; i < n; i++) {
    const int l = strlen(mots[i]); if (strncmp(v + 1, mots[i], l) || v[1 + l] != '"') continue;
    const char* a = v + 2 + l; while (*a == ' ') a++;
    if (*a == ':') { a++; while (*a == ' ') a++; arg = a; }
    return i;
  }
  return -1;
}
static int jMot(const char* p, const char* fin, const char* cle, const char* const* mots, int n) { const char* a; return jMotArg(p, fin, cle, mots, n, a); }
static const char* const F_ACT[] = { "rien", "aller_gauche", "aller_droite", "sauter", "aller_haut", "aller_bas", "tourner_gauche", "tourner_droite", "avancer_case", "tirer" };
static const char* const F_FORME[] = { "pic", "scie", "bloc", "ennemi", "projectile" };
static const char* const F_MVT[] = { "fixe", "aller_retour", "patrouille", "boucle", "tomber", "suivre" };
static const char* const F_CONTACT[] = { "perd", "rebond", "rien", "ramasse", "vie_moins" };
static const char* const F_DECL[] = { "toujours", "proche", "touche" };
static const char* const F_QUAND[] = { "touche", "ramasse_tout", "ecrase_tout", "temps", "points" };
static const char* const F_ALORS[] = { "gagne", "perd", "ouvre", "ferme", "vie_plus", "message", "vitesse" };
static const char* const F_BUT[] = { "arriver", "ramasser_tout", "ecraser_tout", "survivre", "regles" };
static const char* const F_BORD[] = { "mur", "vide", "boucle" };
static const char* const F_MODELE[] = { "plateforme", "cote", "iso" };
// modele "cote" / "iso" : la grille est traduite dans l'alphabet de ces jeux, puis decodee par bqCoteParse / bqIsoParse.
// Ces moteurs n'ont ni objets qui bougent, ni regles : ce qu'ils ne savent pas faire est remplace (la page le dit avant l'envoi).
static const char* ficheAutre(const char* s, int modele, const char* tr) {
  const size_t n = strlen(s); char* c = (char*)malloc(n + 1); if (!c) return "memoire pleine";
  memcpy(c, s, n + 1);
  char* p = strstr(c, "\"rows\""); p = p ? strchr(p, '[') : nullptr;
  const char* e = p ? jFin(p) : nullptr; const char* err = nullptr;
  for (bool dans = false; p && p < e && !err; p++) {
    if (*p == '"') { dans = !dans; continue; }
    if (!dans) continue;
    const unsigned char L = *p;
    if (L >= 128 || !tr[L]) { static char m[24]; snprintf(m, sizeof m, "lettre %c inconnue", L); err = m; }
    else *p = tr[L];
  }
  if (!err) err = modele == 1 ? bqCoteParse(c) : bqIsoParse(c);
  free(c);
  return err;
}
const char* bqFicheParse(const char* s) {
  bqMiniJeu = 0;
  const char* fin = s + strlen(s);
  const int modele = max(0, jMot(s, fin, "modele", F_MODELE, 3));
  // grille (plateforme : 20 cases de large ; bqTmp garde les lettres de l'enfant)
  const char* p = jVal(s, fin, "rows"); if (!p || *p != '[') return "pas de rows";
  char (*tmp)[COLS + 1] = bqTmp; int h = 0, nS = 0;
  if (modele == 0) {
    for (p++;;) {
      while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
      if (*p == ']') break;
      if (*p != '"') return "rows mal forme";
      if (h >= WROWS) return "niveau trop haut";
      int c = 0;
      for (p++; *p && *p != '"'; p++, c++) { if (c >= COLS) return "ligne trop longue (20 cases)"; tmp[h][c] = *p; nS += *p == 'S'; }
      if (*p != '"') return "ligne non fermee";
      p++; while (c < COLS) tmp[h][c++] = '.'; tmp[h][COLS] = 0; h++;
    }
    if (h < 3) return "niveau trop petit";
    if (nS != 1) return "il faut 1 depart S";
  }
  // briques : lettre -> brique du moteur
  char* tr = fTr; memset(fTr, 0, sizeof fTr); tr['.'] = '.'; tr['S'] = 'S';
  float gr = 0, rebond = 0;
  fBord = 1; fCamFixe = false;
  if (const char* m = jVal(s, fin, "monde")) {
    const char* me = jFin(m);
    gr = jNum(m, me, "gravite", 0, 0, 200);
    int k = jMot(m, me, "bord", F_BORD, 3); fBord = k < 0 ? 1 : k;
    const char* cm = jVal(m, me, "camera"); fCamFixe = cm && !strncmp(cm, "\"fixe\"", 6);
  }
  if (const char* b = jVal(s, fin, "briques")) {
    const char* be = jFin(b);
    for (const char* q = b + 1; q < be;) {
      while (q < be && (*q == ' ' || *q == ',' || *q == '\n')) q++;
      if (q >= be || *q != '"') break;
      const unsigned char L = q[1]; if (q[2] != '"' || L >= 128 || L == 'S') return "brique : une seule lettre (pas S)";
      q += 3; while (*q == ' ' || *q == ':') q++;
      const char* v = q; const char* ve = jFin(v); q = ve;
      char o = '.';
      if (jNum(v, ve, "arrivee", 0, 0, 1)) o = 'G';
      else if (jNum(v, ve, "ramasse", 0, 0, 1)) o = '*';
      else if (jNum(v, ve, "mortel", 0, 0, 1)) o = '^';
      else if (jNum(v, ve, "solide", 0, 0, 1)) {
        const float rb = jNum(v, ve, "rebond", 0, 0, 20);
        if (rb > 0) { o = 'R'; rebond = rb; }
        else if (jNum(v, ve, "disparait", 0, 0, 10) > 0) o = 'C';
        else if (jNum(v, ve, "glisse", 0, 0, 1) >= 0.5f) o = 'I';
        else o = '#';
      }
      if (modele == 1) o = o == 'C' ? 'T' : (o == 'R' || o == 'I') ? '#' : o;   // de cote : bloc qui disparait = trou cache 'T'
      if (modele == 2) o = (o == 'C' || o == 'R' || o == 'I') ? '#' : o;      // iso : pas de case vide = un trou
      tr[L] = o;
    }
  }
  // objets
  static struct FDef { uint8_t forme, mvt, contact, dessus, cache, n; int8_t pts[4][2]; float duree, pause, proche, vit; } def[FOBJ_MAX];   // une definition par lettre d'objet
  uint8_t iDef[128]; int nDef = 0; fTirV = 0;
  if (const char* ob = jVal(s, fin, "objets")) {
    const char* oe = jFin(ob);
    for (const char* q = ob + 1; q < oe;) {
      while (q < oe && (*q == ' ' || *q == ',' || *q == '\n')) q++;
      if (q >= oe || *q != '"') break;
      const unsigned char L = q[1]; if (q[2] != '"' || L < 32 || L >= 128 || L == 'S') return "objet : une seule lettre (pas S)";
      q += 3; while (*q == ' ' || *q == ':') q++;
      const char* v = q; const char* ve = jFin(v); q = ve;
      if (nDef >= FOBJ_MAX) return "trop de sortes d'objets (16 au plus)";
      iDef[L] = nDef; FDef &d = def[nDef++]; tr[L] = 'o';
      int k = jMot(v, ve, "forme", F_FORME, 5); d.forme = k < 0 ? 2 : k;
      k = jMot(v, ve, "mouvement", F_MVT, 6); d.mvt = k < 0 ? 0 : k;
      k = jMot(v, ve, "contact", F_CONTACT, 5); d.contact = k < 0 ? 0 : k;
      const char* ds = jVal(v, ve, "dessus"); d.dessus = ds && !strncmp(ds, "\"ecrase\"", 8);
      d.cache = jNum(v, ve, "cache", 0, 0, 1) > 0;
      k = jMot(v, ve, "declencheur", F_DECL, 3);
      d.proche = k == 1 ? jNum(v, ve, "distance", 3, 1, 20) * TILE : k == 2 ? 1.2f * TILE : 0;   // touche = il faut arriver dessus
      d.duree = jNum(v, ve, "duree", 1, 0.1f, 30); d.pause = jNum(v, ve, "pause", 0, 0, 30);
      d.vit = jNum(v, ve, "vitesse", d.forme == 4 ? 10 : d.mvt == 5 ? 3 : 2, 0.5f, 30) * TILE;
      if (d.forme == 4) fTirV = d.vit;
      d.n = 0;
      if (const char* pt = jVal(v, ve, "points")) {
        const char* pe = jFin(pt);
        for (const char* r = pt + 1; r < pe && d.n < 4; r++) if (*r == '[') {
          char* e; long x = strtol(r + 1, &e, 10); while (*e == ' ' || *e == ',') e++; long y = strtol(e, &e, 10);
          d.pts[d.n][0] = constrain(x, -20, 20); d.pts[d.n][1] = constrain(y, -40, 40); d.n++; r = e;
        }
      }
      if (!d.n) { d.pts[0][0] = d.pts[0][1] = 0; d.n = 1; }
      if (modele) {                                    // de cote / iso : ce que le moteur sait faire, le reste devient du sol (iso) ou du vide
        const bool pic = d.forme == 0, fixe = d.mvt == 0;
        tr[L] = pic && d.proche ? 'H' : pic && fixe ? '^' : pic ? (modele == 2 ? 'P' : '^')
              : d.mvt == 2 && modele == 1 ? 'E' : d.forme == 2 && fixe ? '#' : modele == 2 ? '#' : '.';
      }
    }
  }
  // regles "quand ... alors ..." : {"quand":"ramasse_tout","alors":"gagne"}, {"quand":{"touche":"K"},"alors":{"ouvre":"P"}}
  nFreg = 0;
  if (const char* rg = jVal(s, fin, "regles")) if (*rg == '[') {
    const char* re = jFin(rg);
    for (const char* q = rg + 1; q < re && nFreg < FREG_MAX;) {
      while (q < re && *q != '{' && *q != ']') q++;
      if (q >= re || *q != '{') break;
      const char* ve = jFin(q); FReg &g = freg[nFreg]; const char* a;
      int k = jMotArg(q, ve, "quand", F_QUAND, 5, a); if (k < 0) return "regle : quand ?";
      g.quand = k; g.qL = a && *a == '"' ? a[1] : 0; g.qv = a ? atof(a) : 0;
      k = jMotArg(q, ve, "alors", F_ALORS, 7, a); if (k < 0) return "regle : alors ?";
      g.alors = k; g.aL = a && *a == '"' ? a[1] : 0; g.av = a ? atof(a) : 0; g.txt[0] = 0;
      if (k == 5 && a && *a == '"') { int n = 0; for (const char* t = a + 1; *t && *t != '"' && n < 20; t++) g.txt[n++] = *t; g.txt[n] = 0; }
      if ((g.quand == 0 && !g.qL) || ((k == 2 || k == 3) && !g.aL)) return "regle : quelle lettre ?";
      if (k == 6 && g.av <= 0) g.av = 1;
      nFreg++; q = ve;
    }
  }
  { const char* a; int k = jMotArg(s, fin, "but", F_BUT, 5, a); fBut = k < 0 ? 0 : k; fButT = a ? constrain(atof(a), 1, 600) : 30; }
  if (modele) {                                      // de cote / iso
    if (const char* e = ficheAutre(s, modele, tr)) return e;
    nFob = nFreg = 0; fBut = 0;
  } else {
    int nf = 0, nEt = 0;
    for (int r = 0; r < h; r++) {
      for (int c = 0; c < COLS; c++) {
        const unsigned char L = tmp[r][c];
        if (L >= 128 || !tr[L]) { static char e[24]; snprintf(e, sizeof e, "lettre %c inconnue", L); return e; }
        char o = tr[L];
        if (o == '*' && nEt++ >= 3) o = 'O';             // 3 etoiles au plus dans le moteur : les suivantes sont des pieces
        if (o == 'o') {
          const FDef &d = def[iDef[L]];
          if (d.mvt == 2 && d.contact == 0 && d.dessus) o = 'E';   // l'ennemi d'aujourd'hui : patrouille, tue, s'ecrase
          else if (d.forme == 0 && d.proche && d.mvt == 0) o = 'H';        // pic qui sort quand on approche
          else if (d.mvt == 0 && d.forme == 0 && !d.cache) o = '^';
          else if (d.mvt == 0 && d.forme == 2 && d.contact == 2) o = '#';  // bloc immobile qu'on peut toucher : un mur
          else {
            o = '.';
            if (d.forme == 4) { customRows[r][c] = o; continue; }       // projectile : c'est le heros qui le lance
            if (nf >= FOBJ_MAX) return "trop d'objets (16 au plus)";
            FObj &f = fob[nf++];
            f.x = c * TILE + 8; f.y = r * TILE + 8; memcpy(f.pts, d.pts, sizeof f.pts); f.n = d.n; f.mvt = d.mvt;
            f.duree = d.duree; f.pause = d.pause; f.forme = d.forme; f.contact = d.contact; f.dessus = d.dessus;
            f.proche = d.proche; f.cache = d.cache; f.vit = d.vit; f.lettre = L;
          }
        }
        customRows[r][c] = o;
      }
      customRows[r][COLS] = 0;
    }
    nFob = nf;
    for (int r = 0; r < WROWS; r++) customPtr[r] = customRows[r];
    customH = h;
    bqParams(s);                                     // couleurs, titre, message, chrono... (comme la plateforme)
  }
  // heros : boutons, vitesse, elan, saut, murs, vies
  for (int k = 0; k < 3; k++) fBtn[k] = 0;
  float vit = 0, haut = 0, elan = 0.2f; int enAir = 0; fGlisse = fMural = false;
  if (const char* he = jVal(s, fin, "heros")) {
    const char* hf = jFin(he);
    if (const char* bo = jVal(he, hf, "boutons")) {
      const char* be = jFin(bo); const char* const K[3] = { "G", "D", "P3" };
      for (int k = 0; k < 3; k++) { int a = jMot(bo, be, K[k], F_ACT, 10); fBtn[k] = a < 0 ? 0 : a; }
    }
    vit = jNum(he, hf, "vitesse", 0, 0, 20); elan = jNum(he, hf, "elan", 0.2f, 0, 1);
    if (const char* sa = jVal(he, hf, "saut")) { const char* se = jFin(sa); haut = jNum(sa, se, "hauteur", 0, 0, 20); enAir = (int)jNum(sa, se, "en_l_air", 0, 0, 2); }
    if (const char* mu = jVal(he, hf, "mur")) { const char* me = jFin(mu); fGlisse = jNum(mu, me, "glisse", 0, 0, 1) > 0; fMural = jNum(mu, me, "saut_mural", 0, 0, 1) > 0; }
    bqVies = (int)jNum(he, hf, "vies", 0, 0, 9);
  }
  bqGrav = gr * TILE / GRAVITY; bqVit = vit * TILE / RUN / (modele == 1 ? 0.6f : 1);   // en cases : 87,5 cases/s2 et 12,5 cases/s = la plateforme d'aujourd'hui (le jeu de cote court a 60 %)
  fVit0 = bqVit; fElanK = max(0.06f, (1.05f - elan) / 0.85f);   // elan 0,2 = l'acceleration d'aujourd'hui
  fSautK = gr > 0 ? sqrtf(2 * gr * TILE * haut * TILE) / -JUMP_V : 0; // 4,5 cases a 87,5 cases/s2 -> 450 px/s, le saut d'aujourd'hui
  fRebondV = gr > 0 && rebond > 0 ? -sqrtf(2 * gr * TILE * rebond * TILE) : BOUNCE_V;
  customDJ = enAir > 0;
  bqFiche = true;
  return nullptr;
}
// un objet de la fiche qui suit ses points : aller-retour (ou boucle), duree par trajet, pause a chaque point
void fobPos(int i, float &x, float &y) {             // (indice, pas FObj& : le prototype automatique d'Arduino passe avant la struct)
  const FObj &f = fob[i];
  x = f.x + f.pts[0][0] * TILE; y = f.y + f.pts[0][1] * TILE;
  if (f.n < 2 || f.t <= 0) return;
  const int m = f.n - 1; const float seg = f.duree + f.pause;
  const long k = (long)(f.t / seg); const float u = min(1.0f, (f.t - k * seg) / f.duree);
  int a, b;
  if (f.mvt == 3) { a = k % f.n; b = (a + 1) % f.n; }  // boucle : dernier point -> premier
  else { const int j = k % (2 * m); if (j < m) { a = j; b = j + 1; } else { a = 2 * m - j; b = a - 1; } }
  x = f.x + (f.pts[a][0] + (f.pts[b][0] - f.pts[a][0]) * u) * TILE;
  y = f.y + (f.pts[a][1] + (f.pts[b][1] - f.pts[a][1]) * u) * TILE;
}
// au depart et a chaque vie : objets a leur place, briques remises (regles "ouvre"/"ferme", pieces ramassees), regles rearmees
void ficheReset() {
  int nEt = 0; fOLeft = 0;
  for (int r = 0; r < customH; r++) for (int c = 0; c < COLS; c++) {
    const unsigned char L = bqTmp[r][c]; if (L >= 128) continue;
    char o = fTr[L];
    if (o == '*') { if (nEt++ < 3) continue; o = 'O'; }
    if (!o || !strchr(".#ICR^O", o)) continue;
    grid[r][c] = o; fOLeft += o == 'O';
  }
  for (int i = 0; i < nFob; i++) {
    FObj &f = fob[i]; f.t = 0; f.vy = 0; f.on = !f.proche; f.alive = true;
    fobPos(i, f.cx, f.cy);
    if (f.mvt == 2) {                                // patrouille : bornes de la plateforme du dessous ; vy = vitesse signee
      const int c0 = (int)(f.x / TILE), r = (int)(f.y / TILE); int a = c0, b = c0;
      while (a > 0 && solid(a - 1, r + 1) && !solid(a - 1, r)) a--;
      while (b < COLS - 1 && solid(b + 1, r + 1) && !solid(b + 1, r)) b++;
      f.lo = a * TILE + 8; f.hi = b * TILE + 8; f.vy = f.vit;
    }
  }
  for (auto &t : ftir) t.on = false;
  fFait = fTouchK = 0; fPts = 0; bqVit = fVit0; fMsgFin = 0; fTot = false; fT0 = millis();
}
void ficheTouche(char L) { for (int k = 0; k < nFreg; k++) if (freg[k].quand == 0 && freg[k].qL == L) fTouchK |= 1 << k; }
bool ficheAlors(int k) {                             // false = la partie s'arrete (perdu) ; (indice : le prototype automatique d'Arduino passe avant la struct)
  const FReg &g = freg[k];
  switch (g.alors) {
    case 0: flagDone = true; break;                  // gagne : comme l'arrivee (course, question de cours, ecran de fin)
    case 1: addStain(px + PW / 2, py + PH / 2); die(); return false;
    case 2: case 3:
      for (int r = 0; r < customH; r++) for (int c = 0; c < COLS; c++) if (bqTmp[r][c] == g.aL) grid[r][c] = g.alors == 2 ? '.' : '#';
      if (g.alors == 2) for (int i = 0; i < nFob; i++) if (fob[i].lettre == g.aL) fob[i].alive = false;
      snd(SND_COIN); break;
    case 4: if (bqVies) { if (deaths > 0) deaths--; else if (bqVies < 9) bqVies++; } snd(SND_STAR); break;   // ponytail: la vie en plus reste jusqu'au prochain envoi
    case 5: fMsg = g.txt; fMsgFin = millis() + 2500; break;
    case 6: bqVit = constrain(bqVit * g.av, 0.1f, 3.0f); break;
  }
  return true;
}
bool ficheTick(float dt) {                           // objets, projectiles, regles, but ; false = le heros a perdu
  const float hx = px + PW / 2, hy = py + PH / 2;
  if (fBord == 2) { if (hx < 0) px += COLS * TILE; else if (hx >= COLS * TILE) px -= COLS * TILE; if (py > levH * TILE) py = -PH; }   // boucle : on ressort de l'autre cote
  if (fBord == 1 && (hx < -PW || hx > COLS * TILE + PW)) { die(); return false; }
  for (int i = 0; i < nFob; i++) {
    FObj &f = fob[i]; if (!f.alive) continue;
    if (!f.on && fabsf(hx - f.cx) < f.proche && fabsf(hy - f.cy) < f.proche) f.on = true;
    if (!f.on) continue;
    f.t += dt; const float ox = f.cx;
    if (f.mvt == 1 || f.mvt == 3) fobPos(i, f.cx, f.cy);
    else if (f.mvt == 2) { f.cx += f.vy * dt; if (f.cx > f.hi) { f.cx = f.hi; f.vy = -f.vit; } if (f.cx < f.lo) { f.cx = f.lo; f.vy = f.vit; } }
    else if (f.mvt == 4) {                           // tomber : jusqu'au sol, ou hors du niveau
      f.vy = min(f.vy + GRAVITY * dt, (float)MAXFALL); f.cy += f.vy * dt;
      const int r = (int)floorf((f.cy + 8) / TILE);
      if (r >= levH + 2) { f.alive = false; continue; }
      if (solid((int)(f.cx / TILE), r)) { f.cy = r * TILE - 8; f.vy = 0; }
    } else if (f.mvt == 5) {                         // suivre : tout droit vers le heros
      const float dx = hx - f.cx, dy = hy - f.cy, d = sqrtf(dx * dx + dy * dy);
      if (d > 1) { const float k = min(d, f.vit * dt) / d; f.cx += dx * k; f.cy += dy * k; }
    }
    if (f.forme == 2 && f.contact == 2) {            // bloc sans effet : une plateforme qui bouge, on tient dessus
      if (vy >= 0 && px + PW > f.cx - 8 && px < f.cx + 8 && py + PH >= f.cy - 11 && py + PH <= f.cy - 2) { py = f.cy - 8 - PH; vy = 0; onGround = true; px += f.cx - ox; }
      continue;
    }
    if (px + PW <= f.cx - 7 || px >= f.cx + 7 || py + PH <= f.cy - 7 || py >= f.cy + 7) continue;
    ficheTouche(f.lettre);
    if (f.dessus && vy > 60 && py + PH < f.cy - 2) { f.alive = false; fPts++; vy = JUMP_V * 0.7f; onGround = false; snd(SND_COIN); continue; }   // ecrase par le dessus
    if (f.contact == 0) { addStain(hx, hy); die(); return false; }
    if (f.contact == 1) { vy = JUMP_V; onGround = false; snd(SND_BOUNCE); }
    else if (f.contact == 3) { f.alive = false; fPts++; snd(SND_STAR); burstSparks(f.cx, f.cy, COL_STAR, 8); }
    else if (f.contact == 4) { f.alive = false; if (!bqVies || (int)deaths + 1 >= bqVies) { addStain(hx, hy); die(); return false; } deaths++; snd(SND_DIE); }
  }
  for (auto &t : ftir) if (t.on) {                   // projectiles : cassent les ennemis (et ce qu'on ecrase)
    t.x += t.vx * dt;
    if (t.x < -8 || t.x > COLS * TILE + 8 || solid((int)floorf(t.x / TILE), (int)floorf(t.y / TILE))) { t.on = false; continue; }
    for (int i = 0; i < nFob && t.on; i++) { FObj &f = fob[i]; if (f.alive && f.on && (f.forme == 3 || f.dessus) && fabsf(t.x - f.cx) < 9 && fabsf(t.y - f.cy) < 9) { f.alive = false; t.on = false; fPts++; snd(SND_COIN); } }
    for (int i = 0; i < nEnemies && t.on; i++) { Enemy &e = enemies[i]; if (e.active && fabsf(t.x - e.x) < 9 && fabsf(t.y - (e.y - EH / 2)) < 9) { e.active = false; t.on = false; fPts++; addStain(e.x, e.y - 4); snd(SND_COIN); } }
  }
  if (!nFreg && !fBut) return true;
  // ce qui reste a ramasser / a ecraser (regles et but)
  { const int l = (int)floorf(px / TILE), r = (int)floorf((px + PW - 1) / TILE), tp = (int)floorf(py / TILE), bt = (int)floorf((py + PH - 1) / TILE);
    for (int y = tp; y <= bt; y++) for (int x = l; x <= r; x++) if (y >= 0 && y < customH && x >= 0 && x < COLS) ficheTouche(bqTmp[y][x]); }
  int ram = fOLeft, ecr = 0, got = 0;
  for (int i = 0; i < nStarPos; i++) { ram += !starGot[i]; got += starGot[i]; }
  for (int i = 0; i < nFob; i++) if (fob[i].alive) { ram += fob[i].contact == 3; ecr += fob[i].forme == 3 || fob[i].dessus; }
  for (int i = 0; i < nEnemies; i++) ecr += enemies[i].active;
  if (!fTot) { fTot = true; fRam0 = ram; fEcr0 = ecr; }
  const float tps = (millis() - fT0) / 1000.0f;
  for (int k = 0; k < nFreg; k++) {
    if (fFait >> k & 1) continue;
    const FReg &g = freg[k];
    const bool ok = g.quand == 0 ? (fTouchK >> k & 1) : g.quand == 1 ? fRam0 && !ram : g.quand == 2 ? fEcr0 && !ecr : g.quand == 3 ? tps >= g.qv : fPts + got >= g.qv;
    if (!ok) continue;
    fFait |= 1 << k;
    if (!ficheAlors(k)) return false;
  }
  if (fBut == 1 ? fRam0 && !ram : fBut == 2 ? fEcr0 && !ecr : fBut == 3 && tps >= fButT) flagDone = true;   // gagne
  return true;
}
void ficheDraw(int cy) {
  for (int i = 0; i < nFob; i++) {
    const FObj &f = fob[i]; if (!f.alive || (f.cache && !f.on)) continue;
    const int x = (int)f.cx, y = (int)f.cy - cy;
    if (y < -12 || y > STRIP_H + 12) continue;
    if (f.forme == 1) { fb.fillCircle(x, y, 7, COL_SAW); fb.drawCircle(x, y, 7, COL_MEATD); for (int k = 0; k < 6; k++) { float a = sawPhase + k * 1.047f; fb.drawLine(x + cosf(a) * 7, y + sinf(a) * 7, x + cosf(a) * 10, y + sinf(a) * 10, COL_SAW); } fb.fillCircle(x, y, 2, COL_MEATD); }
    else if (f.forme == 0) { fb.fillTriangle(x - 8, y + 8, x, y - 8, x + 8, y + 8, COL_SPK); fb.drawTriangle(x - 8, y + 8, x, y - 8, x + 8, y + 8, COL_MEATD); }
    else if (f.forme == 3) { fb.fillRoundRect(x - 7, y - 4, 14, 11, 4, COL_ENM); fb.fillRect(x - 4, y - 1, 3, 3, COL_TXT); fb.fillRect(x + 1, y - 1, 3, 3, COL_TXT); }
    else if (f.contact == 3) drawStarFancy(fb, x, y, 6, sawPhase * 0.1f, true);   // a ramasser
    else { fb.fillRect(x - 8, y - 8, 16, 16, COL_BLK); fb.drawRect(x - 8, y - 8, 16, 16, COL_BLKT); }
  }
  for (auto &t : ftir) if (t.on) { const int y = (int)t.y - cy; if (y > -4 && y < STRIP_H + 4) fb.fillCircle((int)t.x, y, 3, COL_STAR); }
}
// boutons de la fiche -> actions (fH tenues, fE juste appuyees)
void ficheIn(bool bL, bool bR, bool bJ, bool eL, bool eR, bool eJ) {
  memset(fH, 0, sizeof fH); memset(fE, 0, sizeof fE);
  const bool b[3] = { bL, bR, bJ }, e[3] = { eL, eR, eJ };
  for (int k = 0; k < 3; k++) { fH[fBtn[k]] |= b[k]; fE[fBtn[k]] |= e[k]; }
}
void ficheStep(bool bL, bool bR, bool bJ, bool eL, bool eR, bool eJ) {
  ficheIn(bL, bR, bJ, eL, eR, eJ);
  const int v = fH[5] - fH[4];
  if (v || bqGrav == 0) vy = v * RUN * bqVit;        // haut / bas ; sans gravite on s'arrete en lachant
  if (fE[9]) for (auto &t : ftir) if (!t.on) { t.on = true; t.x = px + PW / 2; t.y = py + PH / 2; t.vx = (playerFacingRight ? 1 : -1) * (fTirV > 0 ? fTirV : 10 * TILE); snd(SND_JUMP); break; }
  playStep(fH[1], fH[2], fH[3], fE[3]);
}
float jK() { return bqFiche && bqOn() ? fSautK : 1.0f; }   // saut de la fiche (1 = celui d'aujourd'hui)

// BRIQUE : lit le port serie sans bloquer ; une ligne complete = un niveau a charger tout de suite.
// ---- BRIQUE : "Mes niveaux" = les 8 derniers niveaux recus, gardes en flash (fichier /bN = la ligne JSON recue telle quelle) ----
// ponytail: 8 emplacements tournants, le plus ancien est ecrase ; un vrai gestionnaire (effacer, trier) si les enfants en veulent plus
#define MYLV_MAX 8
bool fsOk = false;
int myLvN = 0, myLvChoice = 0, myLvSlot[MYLV_MAX];
char myLvTitre[MYLV_MAX][31], myLvPseudo[MYLV_MAX][13], myLvCode[MYLV_MAX][7];
// code de partage d'un jeu publie (6 caracteres, sans 0/1/I/O) <-> 30 bits (champ t du depart en course)
static const char* CODE_AB = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
uint32_t codeNum(const char* c) { uint32_t v = 0; for (int i = 0; i < 6; i++) { const char* p = c[i] ? strchr(CODE_AB, c[i]) : nullptr; if (!p) return 0; v = v * 32 + (p - CODE_AB); } return v; }
void codeTxt(uint32_t v, char* out) { for (int i = 5; i >= 0; i--) { out[i] = CODE_AB[v & 31]; v >>= 5; } out[6] = 0; }

int briqueNext() { File f = LittleFS.open("/bnext", "r"); int n = f ? f.parseInt() : 0; if (f) f.close(); return n; }   // nombre de niveaux recus depuis toujours
String briqueRead(int slot) { char p[8]; snprintf(p, sizeof p, "/b%d", slot); if (!LittleFS.exists(p)) return String(); File f = LittleFS.open(p, "r"); String s = f ? f.readString() : String(); if (f) f.close(); return s; }
void briqueSave(const char* json, const String& ic) {   // fichier = ligne du niveau + ses lignes d'icones
  if (!fsOk) return;
  int n = briqueNext(); const size_t lj = strlen(json);
  char code[7]; jsonStr(json, "code", code, 7);
  if (code[0]) { char pat[16]; snprintf(pat, sizeof pat, "\"code\":\"%s\"", code);   // jeu publie deja dans Mes niveaux : on garde la version presente (identique)
    for (int i = 0; i < MYLV_MAX && i < n; i++) if (strstr(briqueRead(i).c_str(), pat)) return; }
  if (n > 0) { String old = briqueRead((n - 1) % MYLV_MAX);              // meme niveau renvoye : pas de doublon
    if (old.length() == lj + ic.length() && !strncmp(old.c_str(), json, lj) && !strcmp(old.c_str() + lj, ic.c_str())) return; }
  char p[8]; snprintf(p, sizeof p, "/b%d", n % MYLV_MAX);
  File f = LittleFS.open(p, "w"); if (!f) return; f.print(json); f.print(ic); f.close();
  f = LittleFS.open("/bnext", "w"); if (f) { f.print(n + 1); f.close(); }
}
// copie la valeur texte de "cle" (ASCII imprimable seulement : la police n'a pas d'accents)
void jsonStr(const char* s, const char* cle, char* out, int n) {
  char pat[16]; snprintf(pat, sizeof pat, "\"%s\"", cle);
  const char* p = strstr(s, pat); int k = 0;
  if (p) { p += strlen(pat); while (*p == ' ' || *p == ':') p++; if (*p != '"') p = nullptr; else p++; }   // tolere "cle": "valeur"
  if (p) for (; *p && *p != '"' && k < n - 1; p++) if (*p >= 32 && *p < 127) out[k++] = *p;
  out[k] = 0;
}
// ---- BRIQUE : icones choisies sur la page (0 heros, 1 etoile, 2 arrivee) ----
// 16x16, chaque pixel = 3 chiffres hexa "rgb" (4 bits), "---" = transparent. Une ligne {"icone":"h","px":"..."} par icone, avant le niveau.
#define IC_TR 0x0020                      // cle de transparence : jamais produite par la conversion 444 -> 565 (vert 6 bits = g*4 + g/4)
static const char* IC_K = "hegnd";        // heros, etoile, arrivee (goal), ennemi, decor
uint16_t icPx[5][256]; bool icUse[5];
String icWait;                            // lignes "icone" recues, gardees en flash avec le niveau qui suit
bool icOn(int k) { return curLevel == CUSTOM_IDX && icUse[k]; }
bool bqIcon(int k) { return bqMini && icUse[k]; }   // casse-brique / invaders perso
// ---- BRIQUE : questions de cours ecrites par l'animateur, posees a l'arrivee (ASCII : la police n'a pas d'accents) ----
#define BQQ_MAX 10
struct BqQ { char id[8]; char q[81]; char r[3][31]; uint8_t nr, b; };
BqQ bqQ[BQQ_MAX]; int bqNQ = 0, bqQCur = 0, bqQSel = 0, bqQGot = 0; bool bqQRes = false, bqQOk = false; uint32_t bqQT0 = 0;
void qParse(const char* s) {               // toutes les questions contenues dans s (une ligne recue, ou un fichier de Mes niveaux)
  for (const char* p = strstr(s, "\"question\""); p && bqNQ < BQQ_MAX; p = strstr(p + 1, "\"question\"")) {
    const char* fin = strchr(p, '\n'); char l[400]; int n = fin ? min((int)(fin - p), 399) : min((int)strlen(p), 399);
    memcpy(l, p, n); l[n] = 0;               // on ne lit que cette ligne
    BqQ &q = bqQ[bqNQ]; jsonStr(l, "question", q.q, sizeof q.q); jsonStr(l, "id", q.id, sizeof q.id);
    q.nr = 0; const char* a = strstr(l, "\"r\""); a = a ? strchr(a, '[') : nullptr;
    while (a && q.nr < 3) {
      a = strchr(a, '"'); if (!a) break; a++; int k = 0;
      for (; *a && *a != '"' && k < 30; a++) if (*a >= 32 && *a < 127) q.r[q.nr][k++] = *a;
      q.r[q.nr][k] = 0; while (*a && *a != '"') a++; if (*a) a++; q.nr++;
      while (*a == ' ' || *a == ',') a++; if (*a == ']') break;
    }
    const char* bb = strstr(l, "\"b\":"); q.b = bb ? atoi(bb + 4) : 0;
    if (q.q[0] && q.nr >= 2 && q.b < q.nr) bqNQ++;
  }
}
void extrasReset() { memset(icUse, 0, sizeof icUse); bqNQ = 0; }   // icones + questions du niveau perso
void bqWrap(const char* t, int y, int sz, int maxc) {   // texte centre, coupe aux espaces
  char line[40]; int n = strlen(t), i = 0;
  while (i < n) {
    int len = min(maxc, n - i);
    if (i + len < n) { int k = len; while (k > 0 && t[i + k] != ' ') k--; if (k > 0) len = k; }
    memcpy(line, t + i, len); line[len] = 0; ctext(y, line); y += sz * 10;
    i += len; while (t[i] == ' ') i++;
  }
}
void drawBqQuiz() {
  const BqQ &q = bqQ[bqQCur];
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextSize(2);
  if (!bqQRes) {
    tft.setTextColor(COL_STAR); ctext(8, "QUESTION !");
    tft.setTextColor(COL_TXT); bqWrap(q.q, 34, 2, 25);
    for (int i = 0; i < q.nr; i++) {
      const int y = 132 + i * 32; const bool sel = i == bqQSel;
      tft.fillRoundRect(16, y - 7, 288, 28, 6, sel ? COL_MEAT : RGB565(40, 34, 60));
      tft.setTextColor(sel ? COL_BG : COL_TXT); char t[24]; strncpy(t, q.r[i], 23); t[23] = 0; ctext(y, t);
    }
    tft.setTextColor(tft.color565(120, 130, 150)); tft.setTextSize(1); ctext(229, "P2/P1 = choisir   P3 = repondre");
  } else {
    tft.setTextColor(bqQOk ? COL_GOAL : COL_MEAT); tft.setTextSize(3); ctext(50, bqQOk ? "BRAVO !" : "PRESQUE...");
    tft.setTextSize(2); tft.setTextColor(COL_TXT);
    if (bqQOk) ctext(110, "Bonne reponse !"); else { ctext(100, "La bonne reponse :"); tft.setTextColor(COL_STAR); bqWrap(q.r[q.b], 128, 2, 25); }
    tft.setTextColor(tft.color565(120, 130, 150)); tft.setTextSize(1); ctext(229, "P3 = continuer");
  }
}
void bqStartQuiz(int got) {                // arrivee d'un niveau perso qui a des questions : une question avant la victoire
  bqQGot = got; bqQCur = random(bqNQ); bqQSel = 0; bqQRes = false; bqQT0 = millis();
  snd(SND_JUMP); state = ST_BQQ; drawBqQuiz();
}
void bqQuizStep(bool eL, bool eR, bool eJ) {
  const BqQ &q = bqQ[bqQCur];
  if (!bqQRes) {
    const int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
    if (mv) { bqQSel = (bqQSel + mv + q.nr) % q.nr; drawBqQuiz(); }
    if (!eJ) return;
    bqQOk = bqQSel == q.b; bqQRes = true; drawBqQuiz();
    if (Serial) Serial.printf("BRIQUE QUIZ %s %d\n", q.id, bqQOk ? 1 : 0);   // page connectee : stats tout de suite
    else if (fsOk) { File f = LittleFS.open("/bres", "a"); if (f) { if (f.size() < 3000) f.printf("Q %s %d\n", q.id, bqQOk ? 1 : 0); f.close(); } }   // sinon : remontees a la prochaine connexion
    return;
  }
  if (eJ) { levelStartMs += millis() - bqQT0; finishLevel(bqQGot); }   // le temps passe sur la question ne compte pas
}
bool bqFlagOk() {                          // objectif "tout ramasser" : le drapeau n'accepte qu'une fois toutes les etoiles prises
  if (bqOn() && bqFiche && fBut) return false;     // BRIQUE fiche : un autre but que l'arrivee
  if (!bqOn() || !bqTout) return true;
  for (int i = 0; i < nStarPos; i++) if (!starGot[i]) return false;
  return true;
}
const char* icParse(const char* s) {      // toutes les icones contenues dans s (une ligne recue, ou un fichier de Mes niveaux)
  for (const char* p = strstr(s, "\"icone\""); p; p = strstr(p + 1, "\"icone\"")) {
    char k[2]; jsonStr(p, "icone", k, 2);
    const char* w = k[0] ? strchr(IC_K, k[0]) : nullptr; if (!w) return "icone inconnue";
    const char* q = strstr(p, "\"px\""); if (q) q = strchr(q + 4, '"'); if (!q) return "icone sans px";
    q++; uint16_t tmp[256];
    for (int i = 0; i < 256; i++, q += 3) {
      if (q[0] == '-' && q[1] == '-' && q[2] == '-') { tmp[i] = IC_TR; continue; }
      int v[3];
      for (int c = 0; c < 3; c++) { char ch = q[c]; v[c] = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1; if (v[c] < 0) return "icone mal formee"; }
      tmp[i] = ((v[0] * 2 + (v[0] >> 3)) << 11) | ((v[1] * 4 + (v[1] >> 2)) << 5) | (v[2] * 2 + (v[2] >> 3));
    }
    memcpy(icPx[w - IC_K], tmp, sizeof tmp); icUse[w - IC_K] = true;
  }
  return nullptr;
}
void drawIcon2(int k, int x, int y) {      // en double (32x32) : le decor dans le ciel
  for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) { uint16_t c = icPx[k][j * 16 + i]; if (c != IC_TR) fb.fillRect(x + i * 2, y + j * 2, 2, 2, c); }
}
void drawIcon(int k, int x, int y, bool flip) {   // dans la bande fb (coordonnees deja decalees de la camera)
  for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
    uint16_t c = icPx[k][j * 16 + (flip ? 15 - i : i)]; if (c != IC_TR) fb.drawPixel(x + i, y + j, c);
  }
}

void myLvScan() {
  myLvN = 0; if (!fsOk) return;
  int n = briqueNext();
  for (int i = 0; i < MYLV_MAX && i < n; i++) {                        // du plus recent au plus ancien
    int slot = (n - 1 - i) % MYLV_MAX; String s = briqueRead(slot); if (!s.length()) continue;
    myLvSlot[myLvN] = slot;
    jsonStr(s.c_str(), "titre", myLvTitre[myLvN], 31); if (!myLvTitre[myLvN][0]) snprintf(myLvTitre[myLvN], 31, "Niveau %d", n - i);
    jsonStr(s.c_str(), "pseudo", myLvPseudo[myLvN], 13);
    jsonStr(s.c_str(), "code", myLvCode[myLvN], 7);
    myLvN++;
  }
}
void drawMyLv() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_MEAT); tft.setTextSize(3); ctext(10, "MES NIVEAUX");
  tft.setTextSize(2);
  for (int i = 0; i <= myLvN; i++) {                                    // dernier choix = Retour
    int yy = 46 + i * 19;
    char t[16]; if (i == myLvN) strcpy(t, "< Retour"); else { strncpy(t, myLvTitre[i], 15); t[15] = 0; }
    if (i == myLvChoice) { tft.fillRoundRect(30, yy - 3, 260, 19, 4, COL_MEAT); tft.setTextColor(COL_BG); } else tft.setTextColor(COL_TXT);
    ctext(yy, t);
  }
  tft.setTextSize(1); tft.setTextColor(COL_STAR);
  if (!myLvN) { ctext(90, "Aucun niveau pour l'instant."); ctext(106, "Cree-en un sur la page Brique"); ctext(120, "et envoie-le sur la console !"); }
  else if (twoP && myLvChoice < myLvN && !myLvCode[myLvChoice][0]) { tft.setTextColor(COL_MEAT); ctext(214, "Pas publie : publie-le sur Brique pour jouer a plusieurs"); }
  else if (myLvChoice < myLvN && myLvPseudo[myLvChoice][0]) ctextf(214, myLvCode[myLvChoice][0] ? "par %s  -  code %s" : "par %s", myLvPseudo[myLvChoice], myLvCode[myLvChoice]);
  tft.setTextColor(tft.color565(120, 130, 150)); ctext(229, "P2/P1 = choisir   P3 = jouer");
}
void goMyLv() { myLvScan(); if (myLvChoice > myLvN) myLvChoice = 0; state = ST_MYLV; drawMyLv(); }
void myLvStep(bool eL, bool eR, bool eJ) {
  int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
  if (mv) { myLvChoice = (myLvChoice + mv + myLvN + 1) % (myLvN + 1); drawMyLv(); }
  if (!eJ) return;
  if (myLvChoice == myLvN) { if (twoP) { state = ST_MODE; drawMode(); } else { homeChoice = 2; state = ST_HOME; drawHome(); } return; }
  if (twoP && !myLvCode[myLvChoice][0]) return;                         // course : seulement un jeu publie (les autres le retrouvent par son code)
  String s = briqueRead(myLvSlot[myLvChoice]);
  if (!s.length() || briqueParse(s.c_str()) || (twoP && (bqMiniJeu || bqCote || bqIso))) return;   // fichier abime, ou casse-brique / invaders (pas de course a plusieurs) : on reste dans la liste
  extrasReset(); icParse(s.c_str()); qParse(s.c_str());
  if (twoP) {                                                           // hote : depart pour tous, avec le code du jeu
    raceCode = codeNum(myLvCode[myLvChoice]);
    for (int k = 0; k < 6; k++) { espSend(M_START, BRIQUE_SENTINEL, 0, 0, raceCode); delay(8); }
    beginRace(CUSTOM_IDX); return;
  }
  bqStart();
}
// invite : charge le jeu de Mes niveaux qui porte ce code (meme version que l'hote : un code = une version publiee)
bool briqueLoadCode(uint32_t code) {
  if (!fsOk || !code) return false;
  char c[7]; codeTxt(code, c); char pat[16]; snprintf(pat, sizeof pat, "\"code\":\"%s\"", c);
  for (int i = 0; i < MYLV_MAX; i++) {
    String s = briqueRead(i); if (!s.length() || !strstr(s.c_str(), pat) || briqueParse(s.c_str()) || bqMiniJeu || bqCote || bqIso) continue;
    extrasReset(); icParse(s.c_str()); qParse(s.c_str()); raceCode = code; return true;
  }
  return false;
}
void drawMissing(uint32_t code) {
  char c[7]; codeTxt(code, c);
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_MEAT); tft.setTextSize(3); ctext(30, "IL TE MANQUE");
  ctext(62, "CE JEU !");
  tft.setTextColor(COL_TXT); tft.setTextSize(2); ctext(110, "Sur la page Brique,");
  ctext(132, "recupere le code :");
  tft.setTextColor(COL_STAR); tft.setTextSize(4); ctext(162, c);
  tft.setTextColor(tft.color565(120, 130, 150)); tft.setTextSize(1); ctext(222, "P3 = quitter la partie");
}
// resultats de course sur un jeu Brique : gardes en flash, remontes au classement par la page (commande RESULTATS)
void briqueRace() {
  if (curLevel != CUSTOM_IDX || !raceCode || !fsOk) return;
  File f = LittleFS.open("/bres", "a"); if (!f) return;
  if (f.size() < 3000) { char c[7]; codeTxt(raceCode, c); f.printf("%s %u %d %d\n", c, (unsigned)myFinT, raceResult == 1 ? 1 : 0, mpPlayers()); }   // ponytail: ~100 courses max entre deux connexions
  f.close();
}

void briquePoll() {
  static char buf[3600]; static int n = 0;           // ~23 octets par ligne x 80, ou 30 lignes x 99 (jeu de cote), + params
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch != '\n') { if (n < (int)sizeof buf - 1) buf[n++] = ch; continue; }
    buf[n] = 0; bool trop = n >= (int)sizeof buf - 1; n = 0;
    if (!strncmp(buf, "RESULTATS OK", 12)) { LittleFS.remove("/bres"); Serial.println("BRIQUE RESVIDE"); continue; }   // la page les a envoyes au classement
    if (!strncmp(buf, "RESULTATS", 9)) {                                // courses jouees depuis la derniere connexion
      int n = 0; File f = fsOk ? LittleFS.open("/bres", "r") : File();
      while (f && f.available()) { String l = f.readStringUntil('\n'); l.trim(); if (l.length()) { Serial.printf("BRIQUE RES %s\n", l.c_str()); n++; } }
      if (f) f.close(); Serial.printf("BRIQUE RESFIN %d\n", n); continue;
    }
    if (!strncmp(buf, "VERSION", 7)) { Serial.printf("BRIQUE VERSION %s\n", FW_VERSION); continue; }   // la page compare a la version de firmware/manifest.json
    if (!strncmp(buf, "LISTE", 5)) {                                    // ce que contient "Mes niveaux" (du plus recent au plus ancien)
      myLvScan(); for (int i = 0; i < myLvN; i++) Serial.printf("BRIQUE NIV %s|%s\n", myLvTitre[i], myLvPseudo[i]);
      Serial.printf("BRIQUE LISTE %d%s\n", myLvN, fsOk ? "" : " (flash indisponible)"); continue;
    }
    if (!strchr(buf, '{')) continue;                                    // ligne vide ou parasite
    if (strstr(buf, "\"question\"")) {                                 // question de cours : gardee pour le niveau qui suit
      if (!icWait.length()) extrasReset();
      const int avant = bqNQ; if (!trop) qParse(buf);
      if (bqNQ == avant) Serial.println("BRIQUE ERR question mal formee"); else { icWait += '\n'; icWait += buf; Serial.println("BRIQUE OK question"); }
      continue;
    }
    if (strstr(buf, "\"icone\"")) {                                    // icone : appliquee tout de suite, gardee pour le niveau qui suit
      if (!icWait.length()) extrasReset();
      const char* e = trop ? "icone trop grosse" : icParse(buf);
      if (e) Serial.printf("BRIQUE ERR %s\n", e); else { icWait += '\n'; icWait += buf; Serial.println("BRIQUE OK icone"); }
      continue;
    }
    const bool libre = state == ST_SPLASH || state == ST_HOME || state == ST_SELECT || state == ST_END || state == ST_DEAD || state == ST_WIN
                    || (state == ST_PLAY && curLevel >= 50) || (state == ST_DESERT && curLevel == CUSTOM_IDX) || state == ST_ISO;           // pas au milieu d'un monde, d'un mini-jeu ou d'une partie a deux
    const char* err = trop ? "niveau trop gros" : (twoP || raceMode || !(libre || state == ST_MYLV)) ? "console occupee : reviens a l'accueil" : briqueParse(buf);
    if (err) { Serial.printf("BRIQUE ERR %s\n", err); icWait = ""; continue; }
    Serial.printf("BRIQUE OK %d\n", customH);
    if (!icWait.length()) extrasReset();                                // niveau sans icones ni questions : comme d'origine
    briqueSave(buf, icWait); icWait = "";
    dSpiStop(); tft.setSPISpeed(40000000);                              // comme worldQuit() : on peut arriver du menu de fin d'un monde (DMA)
    bqStart();
  }
}

void setup() {
  Serial.setRxBufferSize(4096);   // BRIQUE : un niveau (jusqu a 3,5 Ko : jeu de cote de 4 x 2 ecrans) doit tenir entre deux images
  Serial.begin(115200);
  delay(200);
  fsOk = LittleFS.begin(true);    // BRIQUE : formate la partition au tout premier demarrage (quelques secondes, une seule fois)
  for (int i = 0; i < N_QUIZ; i++) quizLastRot[i] = -1;  // aucune question posee pour l'instant cette session
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   rstReason = "POWERON"; break;
    case ESP_RST_SW:        rstReason = "SW"; break;
    case ESP_RST_PANIC:     rstReason = "PANIC"; break;
    case ESP_RST_INT_WDT:   rstReason = "INT_WDT"; break;
    case ESP_RST_TASK_WDT:  rstReason = "TASK_WDT"; break;
    case ESP_RST_WDT:       rstReason = "WDT"; break;
    case ESP_RST_BROWNOUT:  rstReason = "BROWNOUT"; break;
    case ESP_RST_DEEPSLEEP: rstReason = "DEEPSLEEP"; break;
    default:                rstReason = "AUTRE"; break;
  }
  lastStage = dbgStage; dbgStage = 0;
  // USB CDC natif : n'ecrire que si un hote lit reellement le port, sinon
  // Serial.print peut se bloquer (buffer plein) -> micro-gel periodique.
  DPRINTF("\n[BOOT] MeatRun demarre - reset=%s stage=%u\n", rstReason, lastStage);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_JUMP, INPUT_PULLUP);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setRotation(1);
  tft.setSPISpeed(40000000);

  COL_BG    = tft.color565(24, 26, 40);
  COL_BG2   = tft.color565(34, 38, 58);
  COL_BLK   = tft.color565(70, 80, 100);
  COL_BLKT  = tft.color565(110, 125, 150);
  COL_SPK   = tft.color565(210, 220, 230);
  COL_GOAL  = tft.color565(120, 230, 90);
  COL_MEAT  = tft.color565(255, 70, 120);
  COL_MEATD = tft.color565(150, 20, 60);
  COL_TXT   = tft.color565(235, 235, 245);
  COL_SAW   = tft.color565(205, 210, 220);
  COL_STAR  = tft.color565(255, 210, 60);
  COL_STARD = tft.color565(150, 110, 0);
  COL_ENM   = tft.color565(140, 70, 190);
  COL_ENMD  = tft.color565(70, 30, 110);
  COL_SPLAT  = tft.color565(210, 30, 40);
  COL_SPLATD = tft.color565(140, 12, 22);
  COL_FRAG   = tft.color565(224, 51, 63);   // brique fragile : rouge dedie (independant du joueur)
  COL_BOUNCE  = tft.color565(90, 235, 140);  // ressort : vert "gelatine" dedie
  COL_BOUNCED = tft.color565(40, 170, 95);
  bootIntro();                                     // BRIQUE : logo + chargement, avant de lire la sauvegarde

  // palette de fond/blocs par dizaine (1-10 .. 41-50) -- change tous les 10
  // niveaux, jamais niveau par niveau. Dizaine 1 = valeurs actuelles (aucun
  // changement visuel pour les niveaux deja valides).
  COL_DECADE_BG[0]  = tft.color565(24, 26, 40);    COL_DECADE_BLK[0]  = tft.color565(70, 80, 100);   COL_DECADE_BLKT[0]  = tft.color565(110, 125, 150);
  COL_DECADE_BG[1]  = tft.color565(15, 42, 44);    COL_DECADE_BLK[1]  = tft.color565(60, 100, 98);   COL_DECADE_BLKT[1]  = tft.color565(96, 150, 146);
  COL_DECADE_BG[2]  = tft.color565(42, 20, 20);    COL_DECADE_BLK[2]  = tft.color565(100, 76, 70);   COL_DECADE_BLKT[2]  = tft.color565(146, 112, 100);
  COL_DECADE_BG[3]  = tft.color565(22, 20, 42);    COL_DECADE_BLK[3]  = tft.color565(80, 76, 112);   COL_DECADE_BLKT[3]  = tft.color565(118, 114, 160);
  COL_DECADE_BG[4]  = tft.color565(32, 10, 10);    COL_DECADE_BLK[4]  = tft.color565(102, 62, 66);   COL_DECADE_BLKT[4]  = tft.color565(150, 96, 100);

  // paliers de recompense du joueur (0=aucune serie .. 4=diamant) -- corps ET
  // cape evoluent ensemble ; COL_MEAT/COL_MEATD (UI) ne changent jamais.
  COL_PLAYER_TIER[0]  = COL_MEAT;                        COL_PLAYERD_TIER[0]  = COL_MEATD;
  COL_PLAYER_TIER[1]  = tft.color565(232, 129, 90);       COL_PLAYERD_TIER[1]  = tft.color565(160, 74, 44);    // bronze
  COL_PLAYER_TIER[2]  = tft.color565(240, 106, 140);      COL_PLAYERD_TIER[2]  = tft.color565(176, 58, 92);    // argent
  COL_PLAYER_TIER[3]  = tft.color565(255, 157, 77);       COL_PLAYERD_TIER[3]  = tft.color565(196, 106, 28);   // or
  COL_PLAYER_TIER[4]  = tft.color565(255, 143, 192);      COL_PLAYERD_TIER[4]  = tft.color565(192, 74, 140);   // diamant
  COL_LVLBG = COL_DECADE_BG[0]; COL_LVLBLK = COL_DECADE_BLK[0]; COL_LVLBLKT = COL_DECADE_BLKT[0];

  prefs.begin("meat", false);
  // la progression survit aux televersements : la memoire NVS n'est plus effacee a chaque flash.
  // Elle ne l'est que si SAVE_FORMAT change (a incrementer SEULEMENT si la structure des donnees
  // sauvegardees est modifiee de facon incompatible) -- les nouvelles cles ont des valeurs par defaut.
  { uint8_t fmt = prefs.getUChar("fmt", 0);
    if (fmt != SAVE_FORMAT) { prefs.clear(); prefs.putUChar("fmt", SAVE_FORMAT); }
    // retour utilisateur : chaque nouvelle version flashee repart de zero (seul le niveau 1 est disponible)
    if (prefs.getString("fwver", "") != FW_VERSION) { prefs.clear(); prefs.putUChar("fmt", SAVE_FORMAT); prefs.putString("fwver", FW_VERSION); }
  }
  story.begin("story", false);                       // scenario : jamais efface, meme par une nouvelle version
  glitchLvl = story.getUChar("gl", 60); evo = story.getUChar("evo", 0); evoPts = story.getUChar("evp", 0);
  { String pv = story.getString("ver", "");
    if (pv != FW_VERSION) {
      if (pv.length()) { glitchLvl = max(5, glitchLvl - 15); snprintf(updMsg, sizeof(updMsg), "Mise a jour %s -> %s !|Des glitchs corriges, merci !", pv.c_str(), FW_VERSION); }
      story.putString("ver", FW_VERSION); glitchSave();
    } }
  if (prefs.getBytes("stars", bestStars, NUM_LEVELS) != NUM_LEVELS)
    for (int i = 0; i < NUM_LEVELS; i++) bestStars[i] = 0;
  if (prefs.getBytes("times", bestTime, sizeof(bestTime)) != sizeof(bestTime))
    for (int i = 0; i < NUM_LEVELS; i++) bestTime[i] = 0;
  if (prefs.getBytes("gates", decadeGateCleared, NUM_DECADES) != NUM_DECADES)
    for (int i = 0; i < NUM_DECADES; i++) decadeGateCleared[i] = 0;
  coins         = prefs.getUInt("coins", 0);
  lbLoad();
  speedBoostLv  = prefs.getUChar("boost", 0);
  trailOwned    = prefs.getUInt("trails", 1) | 1; trailSel = prefs.getUChar("trail", 0); if (trailSel >= NTRAIL || !(trailOwned & (1u << trailSel))) trailSel = 0;
  skinOwned     = prefs.getUInt("skins", 1) | 1; skinSel = prefs.getUChar("skin", 0); if (skinSel >= NSKIN || !(skinOwned & (1u << skinSel))) skinSel = 0;
  totalScore    = prefs.getUInt("score", 0);
  totalPlaytime = prefs.getUInt("play", 0);
  totalDeaths   = prefs.getUInt("deaths", 0);
  totalWins     = prefs.getUInt("wins", 0);
  bestCombo     = prefs.getInt("bcombo", 0);
  loadReward();
  combo = 0;

  state = ST_SPLASH;
  selLevel = 0; endChoice = 0;
  drawSplash();
}

// -------- boucle principale : machine a etats --------
void loop() {
  sndUpdate();
  briquePoll();
  if (twoP && espOn && vRoster) { if (!mpFrozen || (!isHost && vRosterFrom == mpHostId)) mpApplyRoster(); else vRoster = false; }   // liste de l'hote (salon, ou mise a jour en cours de partie)
  if (twoP && espOn && vYield) {                            // un hote d'identifiant plus petit tient une partie : je dissous la mienne et je la rejoins depuis le salon
    vYield = false; if (state == ST_DRIFT) { driftFree(); tft.setSPISpeed(40000000); }
    DPRINTF("[MP] autre hote (plus petit) : je cede la place\n");
    mpFrozen = false; isHost = false; raceMode = false; pongTwoP = false; vStart = false; for (int k = 0; k < mpN; k++) mpIn[k] = false;
    lastHello = 0; lobbyInMs = millis(); state = ST_LOBBY; drawLobby();
  }
  if (twoP && espOn && mpFrozen && isHost) mpHostKeep();     // l'hote rediffuse la liste et admet les nouveaux / les revenants
  if (twoP && espOn && mpFrozen) {                          // multijoueur : signal de vie et detection des departs, dans tous les etats
    static unsigned long lastAlive = 0;
    if (millis() - lastAlive > 400) { lastAlive = millis(); espSend(M_ALIVE, 0, 0, 0, 0); }
    static int ls = -1; static uint32_t helloMark[MP_MAX];  // en pleine partie, des HELLO d'un joueur = il est retourne au lobby
    if (state != ls) { ls = state; for (int k = 0; k < MP_MAX; k++) helloMark[k] = mpHello[k]; }
    const bool inGame = state == ST_TIMING || state == ST_DRIFT || state == ST_PLAY || state == ST_COUNTDOWN || state == ST_PONG;
    vPongQuit = false;
    for (int k = 0; k < mpN; k++) {
      if (!(mpIn[k] && mpOn[k])) continue;
      const uint32_t lr = mpRx[k];                          // lu AVANT millis() : un paquet arrive pendant le calcul ne doit pas donner un ecart "negatif" enorme
      const int32_t lim = mpId[k] == mpHostId ? 6000 : 4000;   // tolerance : 4 s sans nouvelles (6 s pour l'hote) ; un joueur retire peut revenir (l'hote le readmet)
      const bool gone = (int32_t)(millis() - lr) > lim, quit = mpQuit[k] || (inGame && mpHello[k] - helloMark[k] > 5);
      if (!gone && !quit) continue;
      mpOn[k] = false;                                      // ce joueur est sorti de la partie
      int nIn = 0; for (int q = 0; q < mpN; q++) if (mpIn[q]) nIn++; const bool two = nIn == 1;   // partie a deux : messages d'avant
      if (mpId[k] == mpHostId) { peerLost(gone ? "HOTE DECONNECTE" : "L'HOTE EST PARTI"); return; }
      if (mpActive() == 0) { peerLost(two ? (gone ? "ADVERSAIRE DECONNECTE" : "ADVERSAIRE PARTI") : "TOUS LES AUTRES SONT PARTIS"); return; }
      DPRINTF("[MP] %s sorti (%s), reste %d adversaire(s)\n", MP_NAME[mpCol[k] & 3], gone ? "deconnecte" : "parti", mpActive());
    }
  }
  static unsigned long hbMs = 0;
  if (millis() - hbMs > 1000) { hbMs = millis(); DPRINTF("[HB] state=%d race=%d heap=%u\n", (int)state, raceMode ? 1 : 0, ESP.getFreeHeap()); }

  bool bL = (digitalRead(BTN_LEFT)  == LOW);
  bool bR = (digitalRead(BTN_RIGHT) == LOW);
  bool bJ = (digitalRead(BTN_JUMP)  == LOW);
#ifdef MINI_BENCH            // banc d'essai des mini-jeux : Pong / Casse-brique / Invaders 8 s chacun, entrees simulees
  { static int ph = -1, n = 0; static unsigned long t0 = 0, last = 0; unsigned long m = millis();
    if (ph < 0 || m - t0 > 8000) { ph = (ph + 1) % 3; t0 = m; n = 0; last = m; if (ph == 0) startPong(); else if (ph == 1) startBreakout(); else startInvaders(); }
    n++;
    if (m - last > 2000) { Serial.printf("[MBENCH] game=%d fps=%.1f dma=%d noDma=%d esp=%d heap=%u\n", ph, n * 1000.0f / (m - last), dDev ? 1 : 0, dNoDma ? 1 : 0, espOn ? 1 : 0, (unsigned)ESP.getFreeHeap()); n = 0; last = m; }
    bL = (m / 700) & 1; bR = !bL; bJ = ph != 0 && (m % 300) < 150; }
#endif
#ifdef SAMU_BENCH            // banc d'essai du Samourai : 2 avatars x 2 niveaux, 10 s chacun, entrees simulees ; images/s sur le port serie
  { static int ph = -1, n = 0; static unsigned long t0 = 0, last = 0; unsigned long m = millis();
    if (ph < 0 || m - t0 > 10000) { ph = (ph + 1) % 8; t0 = m; n = 0; last = m; startSamurai(); smAvKind = ph & 1; smLevel = ph >> 1; smLoad(); }
    n++;
    if (m - last > 2000) { Serial.printf("[SBENCH] avatar=%d niv=%d fps=%.1f x=%.0f\n", smAvKind, smLevel, n * 1000.0f / (m - last), smX); n = 0; last = m; }
    bR = true; bL = false; bJ = (m % 900) < 400; }
#endif
#ifdef LOBBY_BENCH          // entrees simulees : l'hote valide les menus jusqu'a la course, puis les deux joueurs courent a droite en sautant
  { const unsigned long m = millis();
#ifndef LOBBY_BENCH_SLOT
#define LOBBY_BENCH_SLOT 0
#endif
    if (state == ST_PLAY) { bR = true; bL = false; bJ = (m % 900) < 300; }
    else if (state == ST_TIMING) { bL = bR = false; bJ = ((m + (isHost ? 0 : 700)) % 1700) < 80; }
    else if (state == ST_DRIFT) { bL = bR = false; bJ = isHost && (m % 1500) < 100; }   // en course, le pilote automatique de drift.ino conduit
    else if (isHost && vPeer) {
#ifndef LOBBY_BENCH_MINP
#define LOBBY_BENCH_MINP 1
#endif
      int nP = 0; for (int k = 0; k < mpN; k++) { const uint32_t r = mpRx[k]; if ((int32_t)(m - r) < 1500) nP++; }
      if (state == ST_MODE) modeChoice = min(LOBBY_BENCH_SLOT, modeCount() - 1);
      if (mpFrozen || nP >= LOBBY_BENCH_MINP) bJ = (m % 1500) < 100; } }
#endif
#ifdef LEVEL_DUMP          // test : envoie les 50 mondes generes sur le port serie (verification de franchissabilite hors carte)
  { static bool done = false;
    if (!done) { done = true; delay(1500);
      for (int idx = 0; idx < 50; idx++) {
        genWorld(idx);
        Serial.printf("LV %d %d %.1f\n", idx, dTheme, dSpd);
        for (int r = 0; r < dH; r++) { char l[DW + 1]; memcpy(l, dg[r], dWW); l[dWW] = 0; Serial.println(l); }
        for (int i = 0; i < nDVines; i++) Serial.printf("VN %d %d %d\n", dVines[i].ax, dVines[i].ay, dVines[i].len);
        Serial.println("END");
      }
      Serial.println("DONE"); } }
#endif
  bool eL = bL && !prevLeft, eR = bR && !prevRight, eJ = bJ && !prevJump;
  if ((eL || eR || eJ) && state != ST_PLAY && state != ST_DESERT && state != ST_SHADOW && state != ST_SAMU && state != ST_ALTO && state != ST_DRIFT && state != ST_SMASH && state != ST_ISO) snd(SND_CLICK);   // clic dans les menus
#ifdef DESERT_BENCH
  { static int bw = 5; if (state != ST_DESERT) { startWorld(bw); bw = (bw + 10) % 50; } }
#endif
#ifdef ECHO_BENCH           // banc : Wi-Fi allume (comme apres une partie a deux) puis Echo ; etat sur le port serie
  { static int stp = 0; if (stp == 0 && millis() > 3000) { stp = 1; startEspNow(); Serial.printf("[ECHO] wifi on heap=%u max=%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap()); startShadow(); }
    static uint32_t pt = 0; if (stp && millis() - pt > 1000) { pt = millis(); Serial.printf("[ECHO] segs=%d state=%d (ST_SHADOW=%d) esp=%d heap=%u max=%u\n", ecSegsN, (int)state, (int)ST_SHADOW, espOn ? 1 : 0, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap()); } }
#endif
#ifdef LOBBY_BENCH          // banc d'essai multijoueur : entre seul dans le salon a 3 s (et a chaque retour a l'accueil), etat sur le port serie chaque seconde
  { static bool go = false; static unsigned long t0 = 0;
#ifndef LOBBY_BENCH_START_MS
#define LOBBY_BENCH_START_MS 3000
#endif
    if (millis() > LOBBY_BENCH_START_MS && (!go || state == ST_HOME)) { go = true; mpReset(); startEspNow(); state = ST_LOBBY; drawLobby(); }
    if (go && millis() - t0 > 1000) { t0 = millis(); char b[64]; int o = 0;
      for (int k = 0; k < mpN; k++) o += snprintf(b + o, sizeof b - o, " %s:%s%s", MP_NAME[mpCol[k] & 3], mpIn[k] ? "in" : "-", mpOn[k] ? "" : "(parti)");
      Serial.printf("[MP] moi=%s me=%04X host=%d frozen=%d joueurs=%d n=%d%s state=%d rx=%u txerr=%d heap=%u max=%u pile_libre=%u\n", MP_NAME[myCol & 3], mpMe, isHost ? 1 : 0, mpFrozen ? 1 : 0, mpPlayers(), mpN, b, (int)state, (unsigned)espRxN, espTxErr, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (unsigned)uxTaskGetStackHighWaterMark(NULL)); } }
#endif
#ifdef DRIFT_BENCH
  if (state != ST_DRIFT) startDrift();                   // banc d'essai : Drift demarre seul, pilote automatique, fps a la demande (tools/alto_fps.py)
#endif
#ifdef ALTO_BENCH
  if (state != ST_ALTO) startAlto();                     // banc d'essai : Alto demarre seul, pilote automatique, [ALTO] fps= sur le port serie
#endif
#ifdef GAME_BENCH
  if (state != ST_PLAY && state != ST_COUNTDOWN) startLevel(15, true);
#endif

  switch (state) {
    case ST_SPLASH:
      if (eJ || eL || eR) { homeChoice = 0; state = ST_HOME; drawHome(); }
      else mascotTick();
      break;
    case ST_HOME: {
      static float debugHoldT = 0; static bool debugConsumed = false;
      if (bL && bR && bJ) {                     // tenir les 3 boutons 2s = bascule mode debug (niveaux)
        debugHoldT += 0.02f;
        if (debugHoldT > 2.0f && !debugConsumed) { debugUnlockAll = !debugUnlockAll; debugConsumed = true; drawHome(); }
      } else { debugHoldT = 0; debugConsumed = false; }
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      if (mv != 0) { homeChoice = (homeChoice + mv + 3) % 3; drawHome(); }
      if (eJ && !(bL && bR)) {   // le combo 3-boutons (debug) ne doit pas aussi valider le menu
        if (homeChoice == 2) goMyLv();                                   // BRIQUE : les niveaux crees sur la page web
        else if (homeChoice == 0) { stopEspNow(); twoP = false; raceMode = false; modeChoice = 0; state = ST_MODE; drawMode(); }   // 1 joueur : le Wi-Fi d'une partie a deux est coupe (39 Ko rendus aux jeux solo)   // retour : le menu Mode (Niveaux/Pong/Casse-brique/Invaders) est remis
        else {                                      // 2 Joueurs : purge les messages d'une manche precedente
          mpReset(); startEspNow(); state = ST_LOBBY; drawLobby();
        }
      }
      delay(20);
      break;
    }
    case ST_LOBBY: {
      const uint32_t now = millis();
      if (!espOn) {                                          // le Wi-Fi n'a pas demarre (memoire) : on le dit et on reessaie toutes les 2 s
        static uint32_t retry = 0;
        if (now - retry > 2000) { retry = now; startEspNow(); brickFill(0, 0, SCREEN_W, SCREEN_H); tft.setTextSize(2); tft.setTextColor(COL_STAR);
          ctext(90, espOn ? "Wi-Fi pret" : "Wi-Fi indisponible"); tft.setTextSize(1); tft.setTextColor(COL_TXT); ctextf(130, espOn ? "" : "nouvel essai... (memoire libre %u)", (unsigned)ESP.getFreeHeap()); ctext(210, "P3 = annuler");
          if (espOn) drawLobby(); }
        if (eJ) { twoP = false; state = ST_HOME; drawHome(); }
        delay(20); break;
      }
      if (!mpFrozen && now - lastHello > 300) { lastHello = now; myHelloCount++; espSend(M_HELLO, 0, 0, 0, myHelloCount); }
      int nP = 0; bool lowest = true;                       // joueurs presents (vus il y a moins de 1,5 s) ; hote = plus petit identifiant
      for (int k = 0; k < mpN; k++) { const uint32_t r = mpRx[k]; if ((int32_t)(now - r) < 1500) { nP++; if (mpId[k] < mpMe) lowest = false; } }
      const uint32_t ss = vSessionMs; const bool session = ss && (int32_t)(now - ss) < 3000;   // une partie tourne deja : on y sera admis, on n'est pas hote
      if (!mpFrozen) isHost = lowest && nP > 0 && !session && now - lobbyInMs > 1500;   // 1,5 s d'ecoute avant de pouvoir etre hote (une partie se lance peut-etre a cote)
      static int lastNP = -1; static bool lastHost = false, lastFrozen = false;
      if (nP != lastNP || isHost != lastHost || mpFrozen != lastFrozen) { lastNP = nP; lastHost = isHost; lastFrozen = mpFrozen; vPeer = nP > 0; drawLobby(); }
      if (!mpFrozen) {
        if (isHost && nP > 0 && eJ) {                       // l'hote fige la liste (2 a 4 joueurs) et choisit le jeu
          uint16_t ids[4] = { mpMe, 0, 0, 0 }; int n = 1;
          myCol = 0;
          for (int k = 0; k < mpN; k++) { const uint32_t r = mpRx[k]; mpIn[k] = (int32_t)(now - r) < 1500 && n < 4; mpOn[k] = mpIn[k]; if (mpIn[k]) { mpCol[k] = n; ids[n++] = mpId[k]; } }
          mpFrozen = true; mpHostId = mpMe; mpSendRoster(4); mpAddPeers(); vStart = false; modeChoice = 0; state = ST_MODE; drawMode();
        } else if (nP == 0 && eJ) { twoP = false; state = ST_HOME; drawHome(); }   // annuler (ESP-NOW reste actif)
      } else if (vStart) {                                  // partie lancee par l'hote
        vStart = false;
        if (vStartLvl == PONG_SENTINEL) startPong2P(false);
        else if (vStartLvl == TIMING_SENTINEL) startTiming2P(false, vStartX);
        else if (vStartLvl == DRIFT_SENTINEL) startDrift();
        else if (vStartLvl == BRIQUE_SENTINEL) { if (briqueLoadCode(vStartT)) beginRace(CUSTOM_IDX); else drawMissing(vStartT); }   // jeu absent : on dit lequel recuperer
        else beginRace(vStartLvl);
      } else if (eJ) twoPQuit();                            // quitter le salon apres le depart de la liste
      delay(20);
      break;
    }
    case ST_MODE: {
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      int nMode = modeCount();                     // multijoueur : Niveaux, (Pong), Timing, Drift (modeAt)
      if (mv != 0) { int old = modeChoice; modeChoice = (modeChoice + mv + nMode) % nMode; drawModeTile(old, false); drawModeTile(modeChoice, true); }   // seules les 2 vignettes changent : pas de scintillement
      if (eJ) {
        const int md = modeAt(modeChoice);
        if (md == 0) {                      // Niveaux
          if (twoP) selLevel = maxUnlockedIdx();
          state = ST_SELECT; drawSelect();
        } else if (md == 2) {               // Casse-brique (solo)
          startBreakout();
        } else if (md == 3) {               // Invaders (solo)
          startInvaders();
        } else if (md == 4) {                // Ombres (solo)
          startShadow();
        } else if (md == 5) {                // Samourai (solo)
          startSamurai();
        } else if (md == 6) {                // Alto (solo)
          startAlto();
        } else if (md == 7) {                // Timing (a deux : l'hote tire la cible et la donne a l'autre)
          if (twoP) { const int16_t tg = 300 + 50 * random(0, 15); for (int k = 0; k < 6; k++) { espSend(M_START, TIMING_SENTINEL, tg, 0, 0); delay(8); } startTiming2P(true, tg); }
          else startTiming();
        } else if (md == 8) {                // Drift (a deux : l'hote choisit le circuit)
          if (twoP) for (int k = 0; k < 6; k++) { espSend(M_START, DRIFT_SENTINEL, 0, 0, 0); delay(8); }
          startDrift();
        } else if (md == 9) {                // Boutique (solo)
          shopSel = 0; state = ST_SHOP; drawShop();
        } else if (md == 10) {               // Partage de classement (solo)
          lbStart();
        } else if (md == 12) {               // Mes jeux (Brique) : l'hote choisit un jeu publie
          goMyLv();
        } else if (md == 11) {               // Smash (solo, smash.ino)
          startSmash();
        } else if (twoP) {                          // Pong a deux : l'hote demarre pour les deux
          for (int k = 0; k < 6; k++) { espSend(M_START, PONG_SENTINEL, 0, 0, 0); delay(8); }
          startPong2P(true);
        } else startPong();                         // Pong solo (vs IA)
      }
      delay(20);
      break;
    }
    case ST_SELECT: {
      int mu = maxUnlockedIdx();                 // dernier niveau debloque
      if (selLevel > mu) { selLevel = mu; drawSelect(); }
      static float rep = 0; rep -= 0.03;
      int mv = (bR ? 1 : 0) - (bL ? 1 : 0);
      if (mv != 0 && (eL || eR || rep <= 0)) {
        selLevel = (selLevel + mv + (mu + 1)) % (mu + 1);   // reste dans les debloques
        drawSelect(); rep = (eL || eR) ? 0.30 : 0.09;
      } else if (mv == 0) rep = 0;
      if (eJ) {
        if (twoP) {                              // hote : lance la course pour les deux (jamais bloque par un palier)
          for (int k = 0; k < 6; k++) { espSend(M_START, selLevel, 0, 0, 0); delay(8); }
          beginRace(selLevel);
        } else if (gateBlocks(selLevel)) beginCheckpoint(selLevel);   // dizaine suivante : mini-jeu bonus d'abord
        else startLevel(selLevel, true);       // solo
      }
      delay(20);
      break;
    }
    case ST_COUNTDOWN: {
      unsigned long e = millis() - cdStart;
      static int shown = -1;
      int n = 3 - (int)(e / 800);
      if (e >= 3200) { shown = -1; raceMode = true; raceStartMs = millis(); levelStartMs = millis(); lastPosSend = 0; state = ST_PLAY; dbgStage = 1; DPRINTF("[GO] course lvl=%d heap=%u\n", curLevel + 1, ESP.getFreeHeap()); }
      else if (n != shown) { shown = n; drawCountdown(n); }
      delay(10);              // cede la main au WiFi pendant le decompte
      break;
    }
    case ST_RACEEND:
      if (eJ || eL || eR) { twoP = false; raceMode = false; state = ST_HOME; homeChoice = 0; drawHome(); }
      break;
    case ST_DESERT:
#ifdef DESERT_BENCH
      { unsigned long m = millis() % 900; desertStep(false, true, m < 350, m < 25); }   // test : court et saute en boucle
#else
      if (bqFiche && bqOn()) { ficheIn(bL, bR, bJ, eL, eR, eJ); desertStep(fH[1], fH[2], fH[3], fE[3]); }   // BRIQUE fiche de cote
      else desertStep(bL, bR, bJ, eJ);
#endif
      break;
    case ST_PLAY:
#ifdef GAME_BENCH
      { static unsigned long gbL = 0; static int gbN = 0; unsigned long m = millis() % 900;
        playStep(false, true, m < 350, m < 25);
        gbN++;
        if (gbL == 0) gbL = millis();
        if (millis() - gbL > 3000) { Serial.printf("[GBENCH] dma=%d fps=%.1f lvl=%d | decor=%lu tiles=%lu ents=%lu player=%lu hud=%lu blit=%lu us/img\n", dDev ? 1 : 0, gbN * 1000.0f / (millis() - gbL), curLevel + 1, gT[0] / gbN, gT[1] / gbN, gT[2] / gbN, gT[3] / gbN, gT[4] / gbN, gT[5] / gbN); for (int i = 0; i < 6; i++) gT[i] = 0; gbN = 0; gbL = millis(); }
      }
#else
      if (bqFiche && bqOn()) ficheStep(bL, bR, bJ, eL, eR, eJ); else playStep(bL, bR, bJ, eJ);   // BRIQUE fiche : les boutons font ce que dit la fiche
#endif
      if (raceMode) delay(2);   // C3 mono-coeur : cede la main au WiFi (anti-watchdog)
      break;
    case ST_END: {
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      if (mv != 0) { endChoice = (endChoice + mv + 4) % 4; drawEnd(); }
      if (eJ && curLevel == CUSTOM_IDX) {                       // BRIQUE : Rejouer, sinon retour a Mes niveaux (ou a l'accueil)
        if (endChoice == 1) startLevel(curLevel, false); else if (endChoice == 3) { homeChoice = 2; state = ST_HOME; drawHome(); } else goMyLv();
      } else if (eJ) {
        if (endChoice == 0) {                                   // Continuer
          if (curLevel + 1 < NUM_LEVELS) {
            if (curLevel < 50) { int tgt = curLevel + 1; if (gateBlocks(tgt)) beginCheckpoint(tgt); else startLevel(tgt, false); }   // mondes : les questions sont dans les pieces bleues
            else beginQuiz(curLevel, curLevel + 1, 0);
          }
          else { state = ST_WIN; drawWin(); }
        } else if (endChoice == 1) startLevel(curLevel, false);  // Rejouer
        else if (endChoice == 2) { selLevel = curLevel; if (curLevel < 50) { state = ST_SELECT; drawSelect(); } else beginQuiz(curLevel, curLevel, 1); }  // Niveaux
        else { twoP = false; raceMode = false; if (curLevel < 50) { state = ST_HOME; homeChoice = 0; drawHome(); } else beginQuiz(curLevel, curLevel, 2); }  // Accueil
      }
      delay(20);
      break;
    }
    case ST_DEAD: {
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      if (mv != 0) { deadChoice = (deadChoice + mv + 2) % 2; drawDead(); }
      if (eJ) {
        if (deadChoice == 0) startLevel(curLevel, false);              // Rejouer
        else if (curLevel == CUSTOM_IDX) goMyLv();                     // BRIQUE
        else { selLevel = curLevel; beginQuiz(curLevel, curLevel, 1); } // Niveaux (question d'abord : pas d'esquive)
      }
      delay(20);
      break;
    }
    case ST_BQQ:
      bqQuizStep(eL, eR, eJ); delay(20);
      break;
    case ST_MYLV:
      myLvStep(eL, eR, eJ); delay(20);
      break;
    case ST_WIN:
      if (eJ || eL || eR) { state = ST_SELECT; drawSelect(); }
      break;
    case ST_BRICK:
      if (!breakoutStep(bL, bR, bJ, eJ)) { prevJump = true; bqMini = 0; checkpointFinish(); }
      break;
    case ST_INVADERS:
      if (!invadersStep(bL, bR, bJ, eJ)) { prevJump = true; bqMini = 0; checkpointFinish(); }
      break;
    case ST_ISO:
      if (bqFiche && bqOn()) ficheIn(bL, bR, bJ, eL, eR, eJ);   // BRIQUE fiche iso : tourner / avancer d'une case
      if (!(bqFiche && bqOn() ? isoStep(fH[8], fE[6], fE[7], fE[8]) : isoStep(bJ, eL, eR, eJ))) { prevJump = true; dSpiStop(); tft.setSPISpeed(40000000); checkpointFinish(); }
      break;
    case ST_SAMU:
      if (!samuraiStep(bL, bR, bJ, eJ)) { prevJump = true; dSpiStop(); tft.setSPISpeed(40000000); state = ST_MODE; drawMode(); }
      break;
    case ST_SHADOW:
      if (!shadowStep(bL, bR, bJ, eJ)) { prevJump = true; dSpiStop(); tft.setSPISpeed(40000000); state = ST_MODE; drawMode(); }
      break;
    case ST_DRIFT:
      if (!driftStep(bL, bR, bJ, eJ)) { prevJump = true; driftFree(); tft.setSPISpeed(40000000); if (twoP) twoPQuit(); else { state = ST_MODE; drawMode(); } }
      break;
    case ST_TIMING:
      if (!timingStep(bL, bR, bJ, eJ)) { prevJump = true; if (twoP) twoPQuit(); else { state = ST_MODE; drawMode(); } }
      break;
    case ST_SMASH:
      if (!smashStep(bL, bR, bJ, eJ)) { prevJump = true; state = ST_MODE; drawMode(); }
      break;
    case ST_ALTO:
      if (!altoStep(bL, bR, bJ, eJ)) { prevJump = true; altoFree(); dSpiStop(); tft.setSPISpeed(40000000); state = ST_MODE; drawMode(); }
      break;
    case ST_PONG:
      if (eJ) {                                     // P3 = quitter (ou continuer si c'est un palier bonus)
        if (pongTwoP) espSend(M_PQUIT, 0, 0, 0, 0);
        pongTwoP = false; checkpointFinish();
      } else if (pongTwoP && vPongQuit) {            // l'adversaire a quitte le pong reseau
        vPongQuit = false; pongTwoP = false; homeChoice = 0; state = ST_HOME; drawHome();
      } else pongStep(bL, bR);
      break;
    case ST_LB:
      if (eL) { lbSaveNvs(); stopEspNow(); modeChoice = 10; state = ST_MODE; drawMode(); }
      else if (eJ) {                                   // changer mon nom
        for (int i = 0; i < 6; i++) { const char *f = i < (int)strlen(playerName) ? strchr(NAME_SET, playerName[i]) : strchr(NAME_SET, ' '); nameIdx[i] = f ? f - NAME_SET : 0; }
        namePos = 0; state = ST_NAME; drawName();
      } else lbTick();
      delay(20);
      break;
    case ST_NAME: {                                    // P2/P1 = lettre -/+ (maintenu : defile), P3 = lettre suivante
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      if (eL || eR) nameHoldStart = millis();
      if (!mv && bL != bR && millis() - nameHoldStart > 400 && millis() - nameRep > 80) { mv = bR ? 1 : -1; nameRep = millis(); }
      if (mv) { nameIdx[namePos] = (nameIdx[namePos] + mv + NAME_N) % NAME_N; drawName(); }
      else if (eJ) {
        char ch = NAME_SET[nameIdx[namePos]];
        const int SP = (int)(strchr(NAME_SET, ' ') - NAME_SET);
        if (ch == '<') { nameIdx[namePos] = SP; if (namePos > 0) namePos--; drawName(); }
        else if (ch == '>' || namePos == 5) {          // valider : enregistre, diffuse
          if (ch == '>') nameIdx[namePos] = SP;
          int n = 0; for (int i = 0; i < 6; i++) playerName[n++] = NAME_SET[nameIdx[i]];
          while (n > 0 && playerName[n - 1] == ' ') n--;
          playerName[n] = 0;
          if (n == 0) snprintf(playerName, sizeof(playerName), "C3%04X", myId());
          lbMerge(lbSelf()); lbSaveNvs(); lbSentMs = 0; state = ST_LB; drawLb();
        } else { namePos++; drawName(); }
      }
      delay(20);
      break;
    }
    case ST_SHOP: {
      if (debugUnlockAll && coins < 1000) coins = 1000;       // mode debug : de quoi tout essayer
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      if (mv != 0) { shopSel = (shopSel + mv + SHOP_N) % SHOP_N; drawShop(); }
      else if (eJ) {
        if (shopSel == SHOP_BACK) { modeChoice = 9; state = ST_MODE; drawMode(); }          // Retour
        else if (shopSel == 0) {                                                            // palier de vitesse
          if (speedBoostLv < SPEED_BOOST_MAX && coins >= SPEED_BOOST_COST) {
            coins -= SPEED_BOOST_COST; speedBoostLv++;
            prefs.putUInt("coins", coins); prefs.putUChar("boost", speedBoostLv);
            drawShop();
          }
        } else if (shopSel <= AB_N) {                                                       // capacite temporaire
          int a = shopSel - 1;
          if (coins >= AB_COST[a]) { coins -= AB_COST[a]; abilT[a] += AB_SEC; prefs.putUInt("coins", coins); drawShop(); }
        } else if (shopSel > TR0) {                                                         // trainee : acheter puis equiper
          int k = shopSel - TR0;
          if (!(trailOwned & (1u << k)) && coins >= TRAIL_COST[k]) {
            coins -= TRAIL_COST[k]; trailOwned |= 1u << k;
            prefs.putUInt("coins", coins); prefs.putUInt("trails", trailOwned);
          }
          if (trailOwned & (1u << k)) { trailSel = k; prefs.putUChar("trail", trailSel); }
          drawShop();
        } else {                                                                            // skin : acheter puis equiper
          int k = shopSel - SK0;
          if (!(skinOwned & (1u << k)) && coins >= SKIN_COST[k]) {
            coins -= SKIN_COST[k]; skinOwned |= 1u << k;
            prefs.putUInt("coins", coins); prefs.putUInt("skins", skinOwned);
          }
          if (skinOwned & (1u << k)) { skinSel = k; prefs.putUChar("skin", skinSel); }
          drawShop();
        }
      }
      delay(20);
      break;
    }
    case ST_CPINTRO:                                // annonce du mini-jeu bonus (palier dizaine)
      if (eJ) {
        int g = checkpointDecade % 3;                // rotation Pong / Casse-brique / Invaders
        if (g == 0)      startPong();
        else if (g == 1) startBreakout();
        else             startInvaders();
      }
      delay(20);
      break;
    case ST_QUIZ: {
      int mv = (eR ? 1 : 0) - (eL ? 1 : 0);
      if (mv != 0) { quizChoice = (quizChoice + mv + 3) % 3; drawQuizChoices(); }
      if (eJ) {
        int oi = (quizChoice + quizRot) % 3;   // meme rotation que l'affichage (drawQuiz)
        int cat = QUIZZES[quizPick].cat;
        quizOk = (oi == QUIZZES[quizPick].correct);
        if (quizOk) quizDone[quizPick >> 3] |= 1 << (quizPick & 7);
        quizAskedByCat[cat]++;
        quizSkill[cat] = quizOk ? min(5.0f, quizSkill[cat] + 0.4f) : max(1.0f, quizSkill[cat] - 0.6f);   // difficulte de la matiere
        if (quizOk) { quizCorrectByCat[cat]++; quizStreak++; }                        // la cape grandit ou disparait
        else { quizStreak = 0; quizPendingRetry = quizPick; }   // retour : la question ratee doit revenir apres avoir refait le niveau
        quizGain = quizOk ? 10 * QUIZZES[quizPick].diff + min(quizStreak - 1, 5) * 2 : 0;   // points de savoir
        { int lb = knowLvl(knowledge); quizShown = knowledge; knowledge += quizGain; quizLvUp = knowLvl(knowledge) > lb; }
        saveQuizProg();
        saveReward();
        state = ST_QUIZRESULT; drawQuizResult();
      }
      delay(20);
      break;
    }
    case ST_QUIZRESULT:
      if (quizShown < knowledge && !eJ) {               // les points montent a l'ecran
        int lb = knowLvl(quizShown);
        quizShown = min(knowledge, quizShown + (uint32_t)max(1, quizGain / 15));
        if (knowLvl(quizShown) != lb) drawQuizResult(); else drawQuizScore();   // nouvel accessoire -> avatar redessine
        delay(30);
      } else if (eJ) {
        if (quizDest == 1)      { state = ST_SELECT; drawSelect(); }
        else if (quizDest == 2) { state = ST_HOME; homeChoice = 0; drawHome(); }
        else {
          int tgt = quizOk ? quizTargetLevel : quizRetryLevel;
          if (gateBlocks(tgt)) beginCheckpoint(tgt);   // dizaine suivante : mini-jeu bonus d'abord
          else startLevel(tgt, false);
        }
      }
      break;
  }
  prevLeft = bL; prevRight = bR; prevJump = bJ;
}

// -------- une image de jeu --------
// bilan d'un niveau reussi (got = etoiles prises) : score, combo, records, recap, puis menu de fin. Aussi appele par les mondes (31-50).
void finishLevel(int got) {
  lastTimeMs = (int)(millis() - levelStartMs);
  if (curLevel == CUSTOM_IDX && customH && Serial)                  // BRIQUE : resultat renvoye a la page web (Serial faux = pas d'hote USB, on n'ecrit pas)
    Serial.printf("BRIQUE FIN %d %d %d\n", lastTimeMs, (int)deaths, got);   // temps ms, morts, etoiles

  if (deaths == 0) combo++; else combo = 0;   // combo = enchainement sans mourir
  if (combo > bestCombo) bestCombo = combo;

  // poids : temps (1er facteur) > etoiles (2e) > combo (3e)
  scBase  = 100;
  scTime  = max(0, 60 - lastTimeMs / 1000) * 8;       // rapidite, jusqu'a 480
  scStars = got * 100;                                // 100 par etoile
  scCombo = combo * 40;                               // combo
  scPen   = (int)deaths * 10;                         // penalite morts
  lastScore  = max(10, scBase + scTime + scStars + scCombo + scFlag - scPen);
  lastCombo  = combo; lastStars = got;

  lastBestStars = (got > bestStars[curLevel]);
  if (lastBestStars) { bestStars[curLevel] = got; saveStars(); }
  lastBestTime = (bestTime[curLevel] == 0 || (uint32_t)lastTimeMs < bestTime[curLevel]);
  if (lastBestTime) { bestTime[curLevel] = lastTimeMs; saveTimes(); }

  totalScore += lastScore; totalWins++; sessionWins++; saveReward();
  totalDeaths += deaths; totalPlaytime += lastTimeMs / 1000;
  saveStats();

  celebrate();
  recapScore();
  endChoice = 0; state = ST_END; drawEnd();
}

void playStep(bool bLeft, bool br, bool bj, bool jumpEdge) {
  static unsigned long lastMs = 0;
  unsigned long now = millis();
  if (bqOn() && bqChrono && !raceMode && now - levelStartMs > (unsigned long)bqChrono * 1000) {   // BRIQUE : temps ecoule
    snd(SND_DIE); deaths++; combo = 0; deadChoice = 0; bqTimeout = true; state = ST_DEAD; drawDead(); return;
  }
  float dt = (now - lastMs) / 1000.0;
  lastMs = now;
  if (dt <= 0 || dt > 0.045) dt = 0.02;
  updateTrail(dt);

  if (raceMode && vFin && raceResult == 0) {   // l'adversaire a atteint le drapeau en premier
    raceResult = 2; briqueRace(); state = ST_RACEEND; drawRaceEnd(); return;
  }
  DBG(2);
  while (vCoinTail != vCoinHead) {                    // pieces prises par l'adversaire : retirees ici aussi
    int cx = vCoinX[vCoinTail & 7], cy = vCoinY[vCoinTail & 7]; vCoinTail++;
    if (cx >= 0 && cx < COLS && cy >= 0 && cy < levH && grid[cy][cx] == 'O') grid[cy][cx] = '.';
  }
  if (happyT > 0) happyT -= dt;
  for (int i = 0; i < AB_N; i++) if (abilT[i] > 0) abilT[i] -= dt;
  if (abilT[2] <= 0) flying = false;
  if (flagPopT > 0) flagPopT -= dt;
  if (excitedT > 0) excitedT -= dt;
  for (int i = 0; i < 12; i++) { Spark &k = sparks[i]; if (k.life <= 0) continue;
    k.life -= dt; k.x += k.vx * dt; k.y += k.vy * dt; k.vy += 420 * dt; }
  if (flagSlide) {                                   // accroche au mat : glisse vers le bas avec le drapeau
    float base = (goalTY + 1) * TILE;
    px = goalTX * TILE - PW + 3; vx = 0; vy = 0;
    py += 120 * dt;
    if (py + PH >= base) { py = base - PH; flagSlide = false; flagDone = true; }
    else { camY += (camTarget() - camY) * 0.15; drawGame(); return; }
  }

  if (onGround) coyote = 0.08; else if (coyote > 0) coyote -= dt;
  if (jumpEdge) jumpBuf = 0.10; else if (jumpBuf > 0) jumpBuf -= dt;
  if (clingLock > 0) clingLock -= dt;
  if (flyTap > 0) flyTap -= dt;
  if (onGround || wallSliding) airJumped = false;

  if (jumpBuf > 0) {
    if (onGround || coyote > 0) { snd(SND_JUMP); vy = JUMP_V * jK(); onGround = false; coyote = 0; jumpBuf = 0; clingLock = 0.14; sqX = 0.7; sqY = 1.35; }
    else if (wallSliding && (!(bqFiche && bqOn()) || fMural)) { snd(SND_JUMP); sqX = 0.7; sqY = 1.35; excitedT = 0.5f; vy = WJUMP_VY * jK(); vx = -wallDir * WJUMP_VX; jumpLock = JUMPLOCK; wallSliding = false; jumpBuf = 0; clingLock = 0.14; }
    else if (jumpEdge) {                                       // capacites boutique : vol (2 appuis rapproches) / double saut
      if (flying) { flying = false; jumpBuf = 0; }
      else if (abilT[2] > 0 && flyTap > 0) { flying = true; vy = 0; jumpBuf = 0; }
      else if ((abilT[1] > 0 || (curLevel == CUSTOM_IDX && customDJ)) && !airJumped) { snd(SND_JUMP); vy = JUMP_V * jK(); airJumped = true; jumpBuf = 0; sqX = 0.7; sqY = 1.35; }
    }
  }
  if (jumpEdge) flyTap = 0.3f;

  int dir = (br ? 1 : 0) - (bLeft ? 1 : 0);
  if (dir != 0) playerFacingRight = (dir > 0);
  float target = dir * (bqOn() ? RUN * bqVit : RUN + speedBoostLv * 12.0f) * (abilT[0] > 0 ? 1.35f : 1.0f) * (curLevel < 29 ? 0.6f : 1.0f);   // boutique : +12px/s par palier achete (pas en niveau perso : vitesse de la page) ; niveaux 1-29 : 60 % (retour)
  if (jumpLock > 0) jumpLock -= dt;
  else {
    bool onIce = (onGround || coyote > 0) && (tileAt((int)floor((px + 2) / TILE), (int)floor((py + PH + 1) / TILE)) == 'I' || tileAt((int)floor((px + PW - 2) / TILE), (int)floor((py + PH + 1) / TILE)) == 'I');
    float ax = (onIce ? 380.0 : onGround ? 2300.0 : 1850.0) * dt;   // glace : on accelere et on freine tres lentement   // retour "encore plus rapide" -- 1700/1400 -> 2000/1600 -> 2300/1850
    if (bqFiche && bqOn()) ax *= fElanK;             // BRIQUE fiche : elan
    if (vx < target) { vx += ax; if (vx > target) vx = target; }
    else if (vx > target) { vx -= ax; if (vx < target) vx = target; }
  }

  float g = GRAVITY * (bqOn() ? bqGrav : 1.0f);   // niveau perso : gravite choisie sur la page
  if (vy < 0 && !bj) g *= 1.7;
  else if (vy > 0)   g *= 1.12;
  vy += g * dt;
  if (vy > MAXFALL) vy = MAXFALL;
  if (flying) { if (onGround) flying = false; else vy = bj ? -170 : 90; }   // vol : saut maintenu = monte, sinon plane

  // horizontal (+ poussee des briques 'B' : au sol, en appuyant contre elles)
  float nx = px + vx * dt;
  int tp = (int)floor(py / TILE), bt = (int)floor((py + PH - 1) / TILE);
  bool pushing = false;
  if (vx > 0) { int c = (int)floor((nx + PW - 1) / TILE); for (int r = tp; r <= bt; r++) if (solid(c, r)) {
      nx = c * TILE - PW; vx = 0; break; } }
  else if (vx < 0) { int c = (int)floor(nx / TILE); for (int r = tp; r <= bt; r++) if (solid(c, r)) {
      nx = (c + 1) * TILE; vx = 0; break; } }
  px = nx;
  // poussee par contact (la collision ne se declenche qu'une frame sur deux car
  // vx repart de 0 : on teste le contact direct avec la brique)
  if ((onGround || coyote > 0) && dir != 0 && tp == bt) {   // onGround alterne 1 frame sur 2 au sol -> coyote
    int c = (dir > 0) ? (int)floor((px + PW) / TILE) : (int)floor((px - 1) / TILE);
    float gap = (dir > 0) ? c * TILE - (px + PW) : px - (c + 1) * TILE;
    if (gap < 1.5f && tileAt(c, tp) == 'B') {
      pushing = true; pushT += dt;
      if (pushT >= PUSH_DELAY && pbPush(c, tp, dir)) pushT = 0;
    }
  }
  if (!pushing) pushT = 0;
  pushVis = pushing;
  for (int i = 0; i < nPB; i++) {                 // animation de glissement/chute des briques
    PushB &b = pbs[i];
    float sx = 150 * dt, sy = 420 * dt;
    b.ox = (b.ox > 0) ? max(0.0f, b.ox - sx) : min(0.0f, b.ox + sx);
    b.oy = (b.oy > 0) ? max(0.0f, b.oy - sy) : min(0.0f, b.oy + sy);
  }
  if (nPB > 0) pbSettle();                         // une brique qui attendait (joueur dessous) retombe

  // detection de mur par proximite
  wallDir = 0;
  { int rTop = (int)floor(py / TILE), rBot = (int)floor((py + PH - 1) / TILE);
    int rc = (int)floor((px + PW) / TILE);
    if (fabs((px + PW) - rc * TILE) < 1.5f) for (int r = rTop; r <= rBot; r++) if (solid(rc, r)) { wallDir = 1; break; }
    if (wallDir == 0) { int lc = (int)floor((px - 1) / TILE);
      if (fabs(px - (lc + 1) * TILE) < 1.5f) for (int r = rTop; r <= rBot; r++) if (solid(lc, r)) { wallDir = -1; break; } }
  }

  bool pressToward = (wallDir < 0 && bLeft) || (wallDir > 0 && br);
  wallSliding = (!onGround && wallDir != 0 && vy >= 0 && pressToward && clingLock <= 0) && (!(bqFiche && bqOn()) || fGlisse || fMural);
  if (wallSliding) {
    if (!wasSliding) wallTimer = 0;
    wallTimer += dt;
    float cap = WSLIDE_MIN + wallTimer * WSLIP_ACCEL;
    if (vy > cap) vy = cap;
  }
  wasSliding = wallSliding;

  // vertical
  onGround = false;
  float preVy = vy;
  float ny = py + vy * dt;
  int lc = (int)floor(px / TILE), rc = (int)floor((px + PW - 1) / TILE);
  if (vy > 0) { int r = (int)floor((ny + PH - 1) / TILE); for (int c = lc; c <= rc; c++) if (solid(c, r)) { ny = r * TILE - PH; vy = 0; onGround = true; break; } }
  else if (vy < 0) { int r = (int)floor(ny / TILE); for (int c = lc; c <= rc; c++) if (solid(c, r)) { ny = (r + 1) * TILE; vy = 0; break; } }
  py = ny;
  if (onGround || wallSliding) fallPeakY = py; else if (py < fallPeakY) fallPeakY = py;
  if (onGround && preVy > 220) { float k = min(1.0f, (preVy - 220) / 300.0f); sqX = 1.15f + 0.25f * k; sqY = 0.85f - 0.25f * k; }  // atterrissage : ecrase
  { float e = min(1.0f, dt * 10); sqX += (1 - sqX) * e; sqY += (1 - sqY) * e; }            // retour a la forme normale

  // --- ressort 'R' (dizaine 1, niveaux "squishy") : rebond vertical fort ---
  if (onGround) {
    int fr = (int)floor((py + PH) / TILE);
    if (fr >= 0 && fr < levH)
      for (int c = (int)floor(px / TILE); c <= (int)floor((px + PW - 1) / TILE); c++)
        if (grid[fr][c] == 'R') { snd(SND_BOUNCE); vy = bqFiche && bqOn() ? fRebondV : BOUNCE_V; excitedT = 1.0f; onGround = false; sqX = 1.6f; sqY = 0.4f; break; }
  }

  // --- plateformes fragiles : declenche au contact + minuterie d'effondrement ---
  if (onGround) {
    int fr = (int)floor((py + PH) / TILE);
    if (fr >= 0 && fr < levH)
      for (int c = (int)floor(px / TILE); c <= (int)floor((px + PW - 1) / TILE); c++)
        if (grid[fr][c] == 'C') { int ci = crumbAt(c, fr); if (ci >= 0 && crumbs[ci].state == 0) { crumbs[ci].state = 1; crumbs[ci].t = 0; } }
  }
  for (int i = 0; i < nCrumbs; i++) if (crumbs[i].state == 1) { crumbs[i].t += dt; if (crumbs[i].t > CRUMB_DELAY) crumbs[i].state = 2; }

  // scies
  sawPhase += dt * 12.0;
  for (int i = 0; i < nSaws; i++) {
    Saw &s = saws[i]; if (!s.active) continue;
    s.cur += s.dir * s.spd * dt;
    if (s.cur > s.hi) { s.cur = s.hi; s.dir = -1; }
    if (s.cur < s.lo) { s.cur = s.lo; s.dir = 1; }
    float sxx = sawX(s), syy = sawY(s);
    float cx = sxx; if (cx < px) cx = px; if (cx > px + PW) cx = px + PW;
    float cy = syy; if (cy < py) cy = py; if (cy > py + PH) cy = py + PH;
    float dx = sxx - cx, dy = syy - cy;
    if (dx * dx + dy * dy < s.r * s.r) { addStain(px + PW / 2, py + PH / 2); die(); return; }
  }
  if (bqFiche && bqOn() && !ficheTick(dt)) return;     // BRIQUE fiche : objets qui bougent, regles, but

  if (popT > 0) popT -= dt;
  // ennemis : patrouille ; tue au contact, ecrasable par le dessus (rebond)
  for (int i = 0; i < nEnemies; i++) {
    Enemy &e = enemies[i]; if (!e.active) continue;
    e.x += e.dir * e.spd * dt;
    if (e.x > e.hi) { e.x = e.hi; e.dir = -1; }
    if (e.x < e.lo) { e.x = e.lo; e.dir = 1; }
    float ex0 = e.x - EW / 2, ey0 = e.y - EH;
    if (px + PW > ex0 + 1 && px < ex0 + EW - 1 && py + PH > ey0 + 1 && py < e.y) {
      if (vy > 60 && py + PH < ey0 + 7) {        // ecrase par le dessus
        e.active = false; vy = JUMP_V * 0.7; onGround = false;
        addStain(e.x, e.y - 4);
        snd(SND_COIN); coins++; levelCoins++; prefs.putUInt("coins", coins); happyT = 0.8f; happyKind = 1; burstSparks(e.x, e.y - 6, COL_STAR, 6);   // recompense : 1 piece
        popX = e.x; popY = e.y - EH; popT = 0.8f;
      } else { addStain(px + PW / 2, py + PH / 2); die(); return; }
    }
  }

  // etoiles (prise par l'un = prise pour les deux en course 2J)
  for (int i = 0; i < nStarPos; i++) {
    if (starGot[i]) continue;
    float cx = starTX[i] * TILE + 8, cy = starTY[i] * TILE + 8;
    if (fabs((px + PW / 2) - cx) < 12 && fabs((py + PH / 2) - cy) < 12) {
      snd(SND_STAR); starGot[i] = true; happyT = 1.0f; happyKind = 2; sqX = 0.75f; sqY = 1.3f; burstSparks(cx, cy, COL_STAR, 10);
      if (raceMode) espSend(M_STAR, curLevel, i, 0, 0);
    }
  }

  // pieces (boutique) : tuile de grille, ramassee comme un bloc qu'on traverse
  { int l = (int)floor(px / TILE), r = (int)floor((px + PW - 1) / TILE);
    int tp = (int)floor(py / TILE), bt = (int)floor((py + PH - 1) / TILE);
    for (int ty = tp; ty <= bt; ty++) for (int tx = l; tx <= r; tx++)
      if (ty >= 0 && ty < levH && tx >= 0 && tx < COLS && grid[ty][tx] == 'O') {
        grid[ty][tx] = '.'; fOLeft--; if (raceMode) espSend(M_COIN, curLevel, tx, ty, 0); snd(SND_COIN); coins++; levelCoins++; prefs.putUInt("coins", coins); happyT = 0.8f; happyKind = 1; sqX = 0.8f; sqY = 1.25f; burstSparks(tx * TILE + 8, ty * TILE + 8, COL_STAR, 7);
      }
  }

  // --- pieges dynamiques (pics surgissants + blocs qui tombent) ---
  for (int i = 0; i < nTraps; i++) {
    Trap &t = traps[i]; if (!t.active) continue;
    if (t.kind == KSPIKE) {
      float dxp = fabs((px + PW / 2) - (t.tx * TILE + 8));
      float dyp = fabs((py + PH / 2) - (t.ty * TILE + 8));
      if (!t.st) { if (dxp < t.trig && dyp < 44) { t.t += dt; if (t.t > t.p2) { t.st = true; t.t = 0; } } else t.t = 0; }
      else { t.t += dt; if (t.t > t.p1) { t.st = false; t.t = 0; } }
      if (t.st && px + PW > t.tx * TILE && px < t.tx * TILE + TILE && py + PH > t.ty * TILE + 3 && py < t.ty * TILE + TILE) {
        addStain(px + PW / 2, py + PH / 2); die(); return;
      }
    } else { // KFALL
      // declenchement individuel : joueur sous le bloc ET a moins de 6 tuiles en dessous
      // (avant : n'importe ou en dessous -> tous les blocs alignes tombaient en meme temps)
      float below = py - (t.y + TILE);
      if (!t.st && !t.st2 && fabs((px + PW / 2) - (t.tx * TILE + 8)) < t.trig && (py + PH) > t.y && below < 6 * TILE) t.st = true;
      if (t.st) {
        t.vy += 1500 * dt; t.y += t.vy * dt;
        int br = (int)floor((t.y + TILE) / TILE);
        if (br >= levH || solid(t.tx, br)) { t.y = br * TILE - TILE; t.st = false; t.st2 = true; }
        if (px + PW > t.tx * TILE && px < t.tx * TILE + TILE && py + PH > t.y && py < t.y + TILE) {
          addStain(px + PW / 2, py + PH / 2); die(); return;
        }
      }
    }
  }

  // mort
  if (overlapsType('^') || overlapsType('X')) { addStain(px + PW / 2, py + PH / 2); die(); return; }  // X = pic invisible
  if (py > levH * TILE + 8) { die(); return; }

  // --- boss : tirs, laser (P1+P2), vie ---
  if (bossOn && bossHp > 0) {
    bossT += dt; if (bossHit > 0) bossHit -= dt;
    float bc = bossCY(), ly = py + PH / 2;
    laserOn = bLeft && br && laserE > 0;
    if (laserOn) {
      laserE -= dt; if (laserE <= 0) { laserE = 0; laserCd = 20; }
      if (playerFacingRight && fabs(ly - bc) < 18) {
        bossHp -= dt; bossHit = 0.1f;
        if (bossHp <= 0) { bossHp = 0; happyT = 3; happyKind = 2; burstSparks(296, (int)bc, COL_STAR, 12); }
      }
    } else if (laserCd > 0) { laserCd -= dt; if (laserCd <= 0) { laserCd = 0; laserE = 3; } }
    bossShot -= dt;
    if (bossShot <= 0) {
      bossShot = max(0.7f, 1.6f - (curLevel + 1) / 1000.0f);
      for (int i = 0; i < 4; i++) if (!bbul[i].a) { bbul[i].a = true; bbul[i].x = 286; bbul[i].y = bc; break; }
    }
    for (int i = 0; i < 4; i++) {
      BBul &b = bbul[i]; if (!b.a) continue;
      b.x -= min(200.0f, 120.0f + (curLevel + 1) / 10.0f) * dt;
      if (b.x < -8) { b.a = false; continue; }
      if (laserOn && playerFacingRight && b.x > px && fabs(b.y - ly) < 8) { b.a = false; continue; }   // le laser detruit les tirs
      if (px + PW > b.x - 4 && px < b.x + 4 && py + PH > b.y - 4 && py < b.y + 4) { addStain(px + PW / 2, py + PH / 2); die(); return; }
    }
  } else laserOn = false;

  // --- drapeau : fuit quand on approche (leurres), puis se laisse attraper ---
  if (goalFlash > 0) goalFlash -= dt;
  {
    float gx = goalTX * TILE + 8, gy = goalTY * TILE + 8;
    float dxg = (px + PW / 2) - gx, dyg = (py + PH / 2) - gy;
    float gBase = (goalTY + 1) * TILE, gTop = gBase - min((float)POLE_H, gBase);
    bool poleHit = !(bossOn && bossHp > 0) && bqFlagOk() && (px + PW > goalTX * TILE + 1 && px < goalTX * TILE + 7 && py + PH > gTop && py < gBase);
    bool touch = flagDone || (raceMode && poleHit);   // course 2J : arrivee immediate, sans glissade
    flagDone = false;
    if (gpIdx < gpN - 1) {
      if (dxg * dxg + dyg * dyg < 32 * 32) { gpIdx++; goalTX = gpX[gpIdx]; goalTY = gpY[gpIdx]; goalFlash = 0.45; }
    } else if (!touch && poleHit) {                // accroche le mat : bonus selon la hauteur, puis glissade
      float fr = (gBase - (py + PH / 2)) / (gBase - gTop);
      fr = max(0.0f, min(1.0f, fr));
      scFlag = (int)(fr * 300);
      flagSlide = true; happyT = 4; happyKind = 2; vx = vy = 0;
      flagPopT = 1.8f; flagPopY = py; flagPopPts = scFlag; burstSparks(goalTX * TILE + 4, py + PH / 2, COL_STAR, 12);
    } else if (touch) {
    if (raceMode) {                       // course 2J : premier au drapeau gagne
      myFinT = millis() - raceStartMs;
      raceResult = vFin ? 2 : 1;          // si l'adversaire a deja fini -> perdu
      espSend(M_FIN, curLevel, 0, 0, myFinT);
      briqueRace(); state = ST_RACEEND; drawRaceEnd(); return;
    }
    int got = 0; for (int i = 0; i < nStarPos; i++) if (starGot[i]) got++;
    if (bqOn() && bqNQ) { bqStartQuiz(got); return; }   // BRIQUE : question de cours avant la victoire
    finishLevel(got); return;
    }
  }

  // course 2J : diffuse ma position (fantome chez l'adversaire)
  if (raceMode && now - lastPosSend > (uint32_t)(mpActive() > 1 ? 60 : 40)) { lastPosSend = now; espSend(M_POS, curLevel, (int16_t)px, (int16_t)py, 0); }  // ~25 Hz : marge large pour ESP-NOW (paquet de 10 octets)

  // camera + rendu
  camY += (camTarget() - camY) * 0.15;
  DBG(3);
  drawGame();
}

// ================= RECAP DE NIVEAU =================
// Recapitulatif anime : le score monte par etapes -- TEMPS (1er facteur),
// ETOILES (2e), COMBO (3e), puis penalite de morts, pieces et total.
// Un appui sur un bouton = tout s'affiche immediatement, sans animation.
static bool recapFast;
static bool recapSkip() {
  return digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_JUMP) == LOW;
}
static void rdelay(int ms) {                    // pause d'animation, interrompue par un appui
  for (int t = 0; t < ms && !recapFast; t += 10) { if (recapSkip()) recapFast = true; else delay(10); }
}
static void recapNum(int x, int y, int sz, uint16_t col, int v, bool sign) {
  brickFill(x, y, 110, sz * 8);
  tft.setTextSize(sz); tft.setTextColor(col);
  ltextf(x, y, "%s%d", sign ? (v < 0 ? "-" : "+") : "", v < 0 ? -v : v);
}
static void recapCount(int x, int y, int sz, uint16_t col, int target, bool sign) {
  int step = max(1, abs(target) / 24), dir = target < 0 ? -1 : 1;
  for (int v = 0; abs(v) < abs(target) && !recapFast; v += step * dir) {
    recapNum(x, y, sz, col, v, sign);
    rdelay(22);
  }
  recapNum(x, y, sz, col, target, sign);
}
void recapScore() {
  const uint16_t dim = tft.color565(150, 160, 180);
  const int Y0 = 28, DY = 24;                   // rangees : Base, Temps, Etoiles, Combo, Morts, Pieces
  brickFill(0, 0, SCREEN_W, SCREEN_H);          // fond de briques
  tft.setTextColor(COL_GOAL); tft.setTextSize(2);
  ctextf(6, "RECAP NIVEAU %d", curLevel + 1);
  while (recapSkip()) delay(10);              // attendre le relachement du bouton de fin de niveau
  delay(150);
  recapFast = false;

  int run = 0;                                // total courant
  auto total = [&](int v) {
    brickFill(190, 205, 130, 28);
    tft.setTextSize(3); tft.setTextColor(COL_STAR); ltextf(190, 208, "%d", v);
  };
  auto label = [&](int row, const char* t) {
    tft.setTextSize(2); tft.setTextColor(COL_TXT); ltextAt(10, Y0 + row * DY, t);
  };
  tft.setTextSize(1); tft.setTextColor(dim); ltextAt(10, 212, "TOTAL"); total(0);

  // base
  label(0, "Base");
  recapNum(190, Y0, 2, COL_TXT, scBase, true); run = scBase; total(run);
  rdelay(250);

  // 1) TEMPS
  label(1, "Temps");
  tft.setTextColor(lastBestTime ? COL_STAR : dim); tft.setTextSize(1);
  ltextf(96, Y0 + DY + 4, "%d.%ds%s", lastTimeMs / 1000, (lastTimeMs / 100) % 10, lastBestTime ? " RECORD" : "");
  recapCount(190, Y0 + DY, 2, COL_GOAL, scTime, true);
  run += scTime; total(run);
  rdelay(300);

  // 2) ETOILES
  label(2, "Etoiles");
  for (int i = 0; i < 3; i++) {
    int cx = 124 + i * 28, cy = Y0 + 2 * DY + 6;
    brickFill(cx - 15, cy - 15, 30, 30);
    drawStarFancy(tft, cx, cy, 8, 1.0 + i * 1.7, i < lastStars);
    if (i < lastStars) {
      run += 100; recapNum(190, Y0 + 2 * DY, 2, COL_STAR, (i + 1) * 100, true); total(run);
      rdelay(480);
    }
  }
  if (lastStars == 0) recapNum(190, Y0 + 2 * DY, 2, COL_STAR, 0, true);
  rdelay(250);

  // 3) COMBO
  label(3, "Combo");
  tft.setTextColor(dim); tft.setTextSize(1); ltextf(96, Y0 + 3 * DY + 4, "x%d", lastCombo);
  recapCount(190, Y0 + 3 * DY, 2, COL_MEAT, scCombo, true);
  run += scCombo; total(run);
  rdelay(300);

  // 4) drapeau : bonus de hauteur d'accroche (solo)
  if (scFlag > 0) {
    label(4, "Drapeau");
    tft.setTextColor(dim); tft.setTextSize(1); ltextf(110, Y0 + 4 * DY + 4, "%d%% haut", scFlag / 3);
    recapCount(190, Y0 + 4 * DY, 2, COL_STAR, scFlag, true);
    run += scFlag; total(run);
    rdelay(300);
  }
  // 5) morts
  if (scPen > 0) {
    label(5, "Morts");
    tft.setTextColor(dim); tft.setTextSize(1); ltextf(96, Y0 + 5 * DY + 4, "%u x10", deaths);
    recapCount(190, Y0 + 5 * DY, 2, tft.color565(255, 90, 90), -scPen, true);
    run -= scPen; total(max(10, run));
    rdelay(300);
  }

  // 5) pieces ramassees (hors score) : ce niveau + cumul
  label(6, "Pieces");
  tft.setTextSize(2); tft.setTextColor(tft.color565(255, 205, 60)); ltextf(190, Y0 + 6 * DY, "+%d", levelCoins);
  tft.setTextSize(1); tft.setTextColor(dim); ltextf(246, Y0 + 6 * DY + 4, "(total %u)", (unsigned)coins);
  rdelay(300);

  total(lastScore);                            // total final (plancher 10)
  { static bool pl = false; static uint32_t pm0 = 0;                  // temps passe sur la console : cette seance (depuis l allumage) + cumul (NVS "playm", minutes)
    if (!pl) { pm0 = prefs.getUInt("playm", 0); pl = true; }
    uint32_t sm = millis() / 60000, tm = pm0 + sm; prefs.putUInt("playm", tm);
    tft.setTextSize(1); tft.setTextColor(dim);
    ltextf(10, 224, "Console : %uh%02u  (total %uh%02u)", (unsigned)(sm / 60), (unsigned)(sm % 60), (unsigned)(tm / 60), (unsigned)(tm % 60)); }
  tft.setTextSize(1); tft.setTextColor(dim); ctext(194, "P3 = continuer");
  while (recapSkip()) delay(10);               // relacher l'eventuel appui "passer"
  delay(200);
  while (!recapSkip()) delay(10);              // attendre un appui pour passer au menu
  while (recapSkip()) delay(10);               // puis le relachement (evite de valider le menu)
}

// ================= CELEBRATION =================
void celebrate() {
  snd(SND_WIN);
  const int NP = 64;
  static float cx[NP], cy[NP], vx[NP], vy[NP]; static uint16_t cc[NP];
  uint16_t pal[6] = { COL_MEAT, COL_STAR, COL_GOAL, tft.color565(80,160,255),
                      tft.color565(255,140,40), COL_TXT };
  for (int i = 0; i < NP; i++) {
    cx[i] = 160; cy[i] = 120;
    float a = (random(0, 628)) / 100.0;
    float sp = random(60, 220);
    vx[i] = cos(a) * sp; vy[i] = sin(a) * sp - 120;
    cc[i] = pal[random(0, 6)];
  }
  unsigned long t0 = millis();
  while (millis() - t0 < 1100) {
    float dt = 0.03;
    for (int i = 0; i < NP; i++) { vy[i] += 380 * dt; cx[i] += vx[i] * dt; cy[i] += vy[i] * dt; }
    for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {   // rendu en 2 bandes
      fb.fillRect(0, 0, SCREEN_W, STRIP_H, COL_BG2);
      fb.setTextColor(COL_GOAL); fb.setTextSize(4);
      fb.setCursor(70, 60 - sTop); fb.print("REUSSI !");
      if (lastCombo > 1) {
        fb.setTextColor(COL_STAR); fb.setTextSize(3);
        int wx = 160 - (7 + (lastCombo >= 10 ? 1 : 0)) * 9;
        fb.setCursor(wx, 120 - sTop); fb.print("COMBO x"); fb.print(lastCombo);
      }
      fb.setTextColor(COL_TXT); fb.setTextSize(2);
      fb.setCursor(96, 160 - sTop); fb.print("+"); fb.print(lastScore); fb.print(" pts");
      for (int i = 0; i < NP; i++)
        if (cy[i] < SCREEN_H + 6) fb.fillRect((int)cx[i], (int)cy[i] - sTop, 4, 4, cc[i]);
      dBlitPlain(sTop);
    }
    delay(16);
  }
}

// ================= RENDU =================
void drawStarShape(int cx, int cy, int r, uint16_t c, uint16_t o) {
  fb.fillTriangle(cx, cy - r, cx - r, cy + r, cx + r, cy + r, c);
  fb.fillTriangle(cx, cy + r, cx - r, cy - r, cx + r, cy - r, c);
}

// etoile "realiste" : 5 branches facettees (reflet / ombre), contour, brillance
// au centre, halo qui pulse et eclats qui scintillent.
void drawStarFancy(Adafruit_GFX &d, int cx, int cy, int r, float ph, bool lit) {
  const uint16_t cHi = lit ? 0xFF90 : 0x4A69, cLo = lit ? 0xD4A1 : 0x2965, cMid = lit ? COL_STAR : 0x3A07, cOut = lit ? COL_STARD : 0x1082;
  // halo pulsant
  float pul = 0.5 + 0.5 * sin(ph * 1.3);
  if (lit) d.drawCircle(cx, cy, r + 3 + (int)(pul * 2), blendCol(COL_STARD, COL_BG, 0.55));
  if (lit) d.drawCircle(cx, cy, r + 5 + (int)(pul * 2), blendCol(COL_STARD, COL_BG, 0.8));
  float ox[5], oy[5], ix[5], iy[5];
  float rot = sin(ph * 0.5) * 0.12;                          // leger balancement
  for (int i = 0; i < 5; i++) {
    float a = -1.5708 + i * 1.2566 + rot;
    ox[i] = cx + cos(a) * r;        oy[i] = cy + sin(a) * r;
    ix[i] = cx + cos(a + 0.6283) * r * 0.45; iy[i] = cy + sin(a + 0.6283) * r * 0.45;
  }
  for (int i = 0; i < 5; i++) {
    int j = (i + 4) % 5;                                     // inner precedent (a gauche de la pointe)
    // facette gauche (claire) et droite (sombre) de chaque branche
    d.fillTriangle(cx, cy, (int)ox[i], (int)oy[i], (int)ix[j], (int)iy[j], (i == 0 || i == 4) ? cHi : cMid);
    d.fillTriangle(cx, cy, (int)ox[i], (int)oy[i], (int)ix[i], (int)iy[i], (i == 2 || i == 3) ? cLo : cMid);
  }
  for (int i = 0; i < 5; i++) {                              // contour
    d.drawLine((int)ox[i], (int)oy[i], (int)ix[i], (int)iy[i], cOut);
    d.drawLine((int)ix[i], (int)iy[i], (int)ox[(i + 1) % 5], (int)oy[(i + 1) % 5], cOut);
  }
  if (lit) d.fillCircle(cx - 1, cy - 1, 1, 0xFFFF);                  // brillance
  // eclat qui scintille
  float tw = sin(ph * 2.2);
  if (lit && tw > 0.2) {
    int L = (int)(tw * 4) + 1, sx = cx + r - 1, sy = cy - r + 1;
    d.drawFastHLine(sx - L, sy, 2 * L + 1, 0xFFFF);
    d.drawFastVLine(sx, sy - L, 2 * L + 1, 0xFFFF);
  }
}

// melange deux couleurs RGB565 (t=0 -> a, t=1 -> b) -- sert a simuler une
// opacite sur GFXcanvas16, qui n'a pas de canal alpha.
uint16_t blendCol(uint16_t a, uint16_t b, float t) {
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  int r = ar + (int)((br - ar) * t), g = ag + (int)((bg - ag) * t), bl = ab + (int)((bb - ab) * t);
  if (r < 0) r = 0; if (r > 31) r = 31; if (g < 0) g = 0; if (g > 63) g = 63; if (bl < 0) bl = 0; if (bl > 31) bl = 31;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// decor de montagnes en arriere-plan (dizaine 1, niveaux "squishy") -- purement
// decoratif (aucune collision), dessine avant les tuiles pour rester derriere.
// Position fixe a l'ecran (pas de parallax) : suffisamment loin pour ne pas
// paraitre "colle a la camera", et beaucoup plus simple/robuste a caler juste
// sous les niveaux qui montent haut (jusqu'a levH~33).
// Diversifie par niveau (retour : "plus diversifie") : 3 teintes qui cernent
// en fonction du niveau, largeur/hauteur des sommets qui varient aussi.
// Opacite ~20% (retour) : les teintes sont melangees au fond du niveau avec
// blendCol() plutot qu'appliquees pleines.
// decor de chateau fort : muraille crenelee + tours (toit pointu ou creneaux), fenetres, porte
void drawCastleDecor() {
  uint16_t wall  = blendCol(COL_LVLBG, tft.color565(70, 76, 100), 0.30f);
  uint16_t tower = blendCol(COL_LVLBG, tft.color565(90, 96, 124), 0.34f);
  uint16_t dark  = blendCol(COL_LVLBG, tft.color565(10, 10, 20), 0.55f);
  uint16_t roof  = blendCol(COL_LVLBG, tft.color565(160, 70, 80), 0.32f);
  const int baseY = 112, wh = 30, wTop = baseY - wh;
  fb.fillRect(0, wTop, SCREEN_W, wh + 8, wall);
  for (int x = 0; x < SCREEN_W; x += 12) fb.fillRect(x + 2, wTop - 5, 7, 5, wall);          // creneaux de la muraille
  int gx = 120 + (curLevel * 37) % 80;                                                      // porte voutee
  fb.fillRect(gx, baseY - 18, 16, 18 + 8, dark); fb.fillCircle(gx + 8, baseY - 18, 8, dark);
  for (int i = 0; i < 3; i++) {
    int tw = 24 + ((i + curLevel) % 3) * 4, th = 46 + ((i * 17 + curLevel) % 3) * 10;
    int tx = 14 + i * 104 + (curLevel * 13 + i * 29) % 40;
    int top = baseY - th;
    fb.fillRect(tx, top, tw, th + 8, tower);
    if ((i + curLevel) & 1) {                                                               // toit pointu + fanion
      fb.fillTriangle(tx - 3, top, tx + tw / 2, top - 22, tx + tw + 3, top, roof);
      fb.drawFastVLine(tx + tw / 2, top - 32, 10, tower);
      fb.fillTriangle(tx + tw / 2, top - 32, tx + tw / 2 + 9, top - 29, tx + tw / 2, top - 26, roof);
    } else {
      for (int cx2 = tx; cx2 < tx + tw - 3; cx2 += 8) fb.fillRect(cx2, top - 5, 5, 5, tower);   // creneaux
    }
    fb.fillRect(tx + tw / 2 - 2, top + 12, 4, 9, dark); fb.fillCircle(tx + tw / 2, top + 12, 2, dark);   // fenetre arquee
    if (th > 56) { fb.fillRect(tx + tw / 2 - 2, top + 28, 4, 8, dark); fb.fillCircle(tx + tw / 2, top + 28, 2, dark); }
  }
}

void drawMountainDecor() {
  static const uint8_t pal[3][6] = {
    { 46, 54, 76,   64, 76, 104 },   // bleu-gris nocturne
    { 58, 44, 78,   82, 64, 108 },   // violet crepuscule
    { 40, 58, 56,   58, 82, 78  },   // vert-ardoise
  };
  int variant = curLevel % 3;
  uint16_t back  = blendCol(COL_LVLBG, tft.color565(pal[variant][0], pal[variant][1], pal[variant][2]), 0.20f);
  uint16_t front = blendCol(COL_LVLBG, tft.color565(pal[variant][3], pal[variant][4], pal[variant][5]), 0.30f);
  uint16_t snow  = blendCol(COL_LVLBG, tft.color565(205, 215, 232), 0.35f);
  int peakWBase = 36 + (curLevel % 4) * 6;    // silhouette differente par niveau
  int shift = curLevel * 5;
  for (int band = 0; band < 2; band++) {
    int baseY = 98 + band * 12;
    uint16_t col = band == 0 ? back : front;
    int peakW = peakWBase + band * 4;
    int off = band * 22 + shift;
    for (int x = -peakW + (off % peakW); x < SCREEN_W + peakW; x += peakW) {
      int h = 24 + band * 16 + (((x / peakW) + curLevel) & 3) * 7;
      int apexY = baseY - h;
      fb.fillTriangle(x, baseY, x + peakW / 2, apexY, x + peakW, baseY, col);
      if (band == 1)
        fb.fillTriangle(x + peakW / 2 - 5, apexY + 10, x + peakW / 2, apexY, x + peakW / 2 + 5, apexY + 10, snow);
    }
  }
}

// "visible au dernier moment" : un obstacle fantome se revele quand le joueur
// arrive a moins de `range` px (centre a centre) -- jamais totalement invisible.
bool nearPlayer(int tx, int ty, float range) {
  return fabs((px + PW / 2) - (tx * TILE + 8)) < range && fabs((py + PH / 2) - (ty * TILE + 8)) < range;
}

// bulle de bd au-dessus de la tete (cx = centre x ecran, y = haut de la bulle)
void drawBubble(int cx, int y, const char* t) {
  int w = textW(fb, 1, t) + 10;
  fb.fillRoundRect(cx - w / 2, y, w, 13, 4, COL_TXT);
  fb.fillTriangle(cx - 3, y + 12, cx + 3, y + 12, cx, y + 16, COL_TXT);
  fb.setTextColor(COL_BG); gfxPrintAt(fb, 1, cx - w / 2 + 5, y + 2, t);
}

void drawGame() {
  DBG(4);
  for (int sTop = 0; sTop < SCREEN_H; sTop += STRIP_H) {   // rendu en 2 bandes (buffer reduit)
#ifdef GAME_BENCH
  gq = micros();
#endif
  int cy = (int)camY + sTop;
  if (bqOn() && (bqTheme >= 0 || bqCiel >= 0)) {                        // niveau perso : ciel du theme (ou de la couleur choisie), en degrade sur tout le niveau
    uint8_t c0[3], c1[3]; const int H = levH * TILE;
    if (bqCiel >= 0) { c0[0] = bqCiel >> 16; c0[1] = bqCiel >> 8; c0[2] = bqCiel; for (int k = 0; k < 3; k++) c1[k] = c0[k] + (255 - c0[k]) * 11 / 20; }   // vers le blanc en bas
    else { memcpy(c0, BQ_THEMES[bqTheme].c0, 3); memcpy(c1, BQ_THEMES[bqTheme].c1, 3); }
    for (int y = 0; y < STRIP_H; y++) fb.drawFastHLine(0, y, SCREEN_W, bqMix(c0, c1, constrain(cy + y, 0, H), H));
    if (icOn(4)) for (int wy = 48; wy < H; wy += 112) {                 // BRIQUE : decor choisi sur la page, en grand dans le ciel
      const int sy = wy - cy; if (sy > -32 && sy < STRIP_H) drawIcon2(4, ((wy / 112) & 1) ? 214 : 74, sy);
    }
  } else {
  fb.fillRect(0, 0, SCREEN_W, STRIP_H, COL_LVLBG);
  if (curLevel < 10 || (curLevel / 10) % 2 == 0) drawMountainDecor();   // dizaines paires : montagnes
  else drawCastleDecor();                                                // dizaines impaires (11-20, 31-40...) : chateau fort
  }

#ifdef GAME_BENCH
  gT[0] += micros() - gq; gq = micros();
#endif
  int r0 = cy / TILE - 1, r1 = (cy + STRIP_H) / TILE + 1;
  if (r0 < 0) r0 = 0; if (r1 > levH) r1 = levH;
  for (int ty = r0; ty < r1; ty++) {
    for (int tx = 0; tx < COLS; tx++) {
      char c = grid[ty][tx];
      int x = tx * TILE, y = ty * TILE - cy;
      if (c == 'H' && nearPlayer(tx, ty, 52)) {   // trou fantome : se revele de pres (contour pointille, vide dedans)
        for (int d = 0; d < TILE; d += 4) { fb.drawPixel(x + d, y, COL_LVLBLKT); fb.drawPixel(x, y + d, COL_LVLBLKT); fb.drawPixel(x + TILE - 1, y + d, COL_LVLBLKT); }
      } else if (c == 'X') {                        // pic fantome : invisible de loin, visible de pres
        if (nearPlayer(tx, ty, 52)) {
          fb.fillTriangle(x, y + TILE, x + TILE / 2, y, x + TILE, y + TILE, COL_SPK);
          fb.drawTriangle(x, y + TILE, x + TILE / 2, y, x + TILE, y + TILE, COL_MEATD);
        }
      } else if (c == '#' && bqOn() && (bqTheme >= 0 || bqSol >= 0)) {   // niveau perso : sol du theme ou de la couleur choisie (comme l'apercu web)
        if (bqSol >= 0) { const int r = bqSol >> 16 & 255, g = bqSol >> 8 & 255, b = bqSol & 255;
          fb.fillRect(x, y, TILE, TILE, RGB565(r, g, b)); fb.fillRect(x, y, TILE, 3, RGB565(r * 3 / 5, g * 3 / 5, b * 3 / 5)); }
        else { const BqTheme &t = BQ_THEMES[bqTheme];
          fb.fillRect(x, y, TILE, TILE, RGB565(t.sol[0], t.sol[1], t.sol[2])); fb.fillRect(x, y, TILE, 3, RGB565(t.bord[0], t.bord[1], t.bord[2])); }
      } else if (c == '#' || c == 'H') {   // H de loin : rendu identique a '#'
        int pi = platIdx[ty][tx]; if (pi > 7) pi = 0;     // cube du ton de sa plateforme
        fb.fillRect(x, y, TILE, TILE, toneF[pi]);
        fb.fillRect(x + 3, y + 3, TILE - 6, TILE - 6, toneM[pi]);
        fb.drawFastHLine(x, y, TILE, toneL[pi]);          fb.drawFastVLine(x, y, TILE, toneL[pi]);
        fb.drawFastHLine(x, y + TILE - 1, TILE, toneD[pi]); fb.drawFastVLine(x + TILE - 1, y, TILE, toneD[pi]);
      } else if (c == 'I') {                     // plaque glissante : glace bleu clair avec reflets
        fb.fillRect(x, y, TILE, TILE, RGB565(150, 210, 245));
        fb.fillRect(x + 2, y + 2, TILE - 4, TILE - 4, RGB565(195, 235, 255));
        fb.drawFastHLine(x, y, TILE, COL_TXT); fb.drawFastVLine(x, y, TILE, COL_TXT);
        fb.drawFastHLine(x, y + TILE - 1, TILE, RGB565(90, 150, 210)); fb.drawFastVLine(x + TILE - 1, y, TILE, RGB565(90, 150, 210));
        fb.drawLine(x + 3, y + 10, x + 8, y + 4, COL_TXT);
      } else if (c == 'C') {                     // plateforme fragile -- toujours rouge (COL_FRAG), reconnaissable d'emblee
        int ci = crumbAt(tx, ty); int st = (ci < 0) ? 0 : crumbs[ci].state;
        if (st != 2) {
          uint16_t col = COL_FRAG;
          if (st == 1) { bool bl = ((int)(crumbs[ci].t * 12)) & 1; col = bl ? COL_SPLAT : COL_FRAG; }
          fb.fillRect(x, y, TILE, TILE, col);
          fb.drawRect(x, y, TILE, TILE, COL_MEATD);
          fb.drawLine(x + 3, y + 2, x + 6, y + TILE - 3, COL_MEATD);   // fissures
          fb.drawLine(x + 11, y + 2, x + 8, y + TILE - 4, COL_MEATD);
        }
      } else if (c == '^') {
        fb.fillTriangle(x, y + TILE, x + TILE / 2, y, x + TILE, y + TILE, COL_SPK);
        fb.drawTriangle(x, y + TILE, x + TILE / 2, y, x + TILE, y + TILE, COL_MEATD);
      } else if (c == 'R') {                    // ressort/tremplin "squishy"
        fb.fillRect(x, y, TILE, TILE, COL_LVLBLK);
        fb.fillRoundRect(x + 1, y + 6, TILE - 2, TILE - 7, 4, COL_BOUNCE);
        fb.drawFastHLine(x + 3, y + 8, TILE - 6, COL_BOUNCED);
      } else if (c == 'G') {
        fb.drawFastVLine(x + 3, y, TILE, COL_TXT);
        fb.fillTriangle(x + 4, y + 1, x + 4, y + 8, x + 14, y + 4, COL_GOAL);
      } else if (c == 'O') {                      // piece (boutique) -- petite croix dessus
        fb.fillCircle(x + 8, y + 8, 6, tft.color565(255, 205, 60));
        fb.drawCircle(x + 8, y + 8, 6, tft.color565(170, 120, 20));
        fb.drawFastHLine(x + 5, y + 8, 6, tft.color565(170, 120, 20));
        fb.drawFastVLine(x + 8, y + 5, 6, tft.color565(170, 120, 20));
      }
    }
  }

#ifdef GAME_BENCH
  gT[1] += micros() - gq; gq = micros();
#endif
  // eclaboussures (restent sur le niveau)
  int ns = nStain < MSTAIN ? nStain : MSTAIN;
  for (int i = 0; i < ns; i++) {
    int x = (int)stainX[i], y = (int)stainY[i] - cy;
    if (y < -8 || y > SCREEN_H + 8) continue;
    int h = (int)(stainX[i] * 3 + stainY[i]);
    fb.fillCircle(x, y, 4, COL_SPLAT);
    fb.fillCircle(x - 4 + (h & 1), y + 2, 2, COL_SPLATD);
    fb.fillCircle(x + 5, y - 1 + (h & 2), 2, COL_SPLAT);
    fb.fillCircle(x + 2, y + 5, 3, COL_SPLAT);
    fb.fillCircle(x - 5, y - 3, 2, COL_SPLATD);
    fb.fillCircle(x + 6 - (h & 3), y + 4, 1, COL_SPLAT);
  }

  // etoiles
  for (int i = 0; i < nStarPos; i++) {
    if (starGot[i]) continue;
    int sx = starTX[i] * TILE + 8, sy = starTY[i] * TILE + 8 - cy;
    if (sy < -10 || sy > SCREEN_H + 10) continue;
    const int bob = (int)(sin(sawPhase * 0.35 + i * 2.1) * 2.5);
    if (icOn(1)) drawIcon(1, sx - 8, sy - 8 + bob, false);                // BRIQUE : etoile choisie sur la page
    else drawStarFancy(fb, sx, sy + bob, 7, sawPhase + i * 1.7, true);
  }

  if (flagPopT > 0) {                                         // points du mat : "+N" qui monte pres du drapeau
    char fp[12]; snprintf(fp, sizeof(fp), "+%d", flagPopPts);
    int fw = textW(fb, 2, fp), fx = min(goalTX * TILE + 18, SCREEN_W - 4 - fw), fy = (int)(flagPopY - cy - (1.8f - flagPopT) * 22) - 8;
    fb.setTextColor(COL_BG); gfxPrintAt(fb, 2, fx + 1, fy + 1, fp);
    fb.setTextColor(COL_STAR); gfxPrintAt(fb, 2, fx, fy, fp);
  }
  if (popT > 0) {                                             // "+1" piece qui monte
    fb.setTextSize(1); fb.setTextColor(COL_STAR);
    gfxPrintAt(fb, 1, (int)popX - 6, (int)(popY - cy - (0.8f - popT) * 24), "+1");
  }

  // ennemis
  for (int i = 0; i < nEnemies; i++) {
    Enemy &e = enemies[i]; if (!e.active) continue;
    int ex = (int)e.x, ey = (int)e.y - cy;
    if (ey < -14 || ey > SCREEN_H + 14) continue;
    if (icOn(3)) { drawIcon(3, ex - 8, ey - 16, e.dir < 0); continue; }   // BRIQUE : ennemi choisi sur la page
    int fo = ((int)(sawPhase * 0.6 + i) & 1) ? 1 : 0;           // pas alternes
    fb.fillRect(ex - 5 + fo, ey - 2, 3, 2, COL_ENMD);
    fb.fillRect(ex + 2 - fo, ey - 2, 3, 2, COL_ENMD);
    fb.fillRoundRect(ex - 6, ey - 10, 12, 9, 4, COL_ENM);
    fb.drawRoundRect(ex - 6, ey - 10, 12, 9, 4, COL_ENMD);
    fb.drawLine(ex - 3, ey - 9, ex + 1, ey - 9, COL_TXT);        // reflet
    int d = e.dir > 0 ? 1 : -1;
    fb.fillRect(ex - 4 + (d > 0), ey - 7, 3, 3, COL_TXT);
    fb.fillRect(ex + 1 + (d > 0), ey - 7, 3, 3, COL_TXT);
    fb.drawPixel(ex - 3 + d + (d > 0), ey - 6, COL_BLK);
    fb.drawPixel(ex + 2 + d + (d > 0), ey - 6, COL_BLK);
    fb.drawLine(ex - 5, ey - 9 + (d > 0 ? 0 : 1), ex - 1, ey - 8 + (d > 0 ? 1 : 0), COL_ENMD);   // sourcils fronces
    fb.drawLine(ex + 5, ey - 9 + (d > 0 ? 1 : 0), ex + 1, ey - 8 + (d > 0 ? 0 : 1), COL_ENMD);
  }

  // scies
  for (int i = 0; i < nSaws; i++) {
    Saw &s = saws[i]; if (!s.active) continue;
    int sx = (int)sawX(s), sy = (int)sawY(s) - cy, rr = (int)s.r;
    if (sy < -14 || sy > SCREEN_H + 14) continue;
    fb.fillCircle(sx, sy, rr, COL_SAW);
    fb.drawCircle(sx, sy, rr, COL_MEATD);
    for (int k = 0; k < 8; k++) {
      float a = sawPhase + k * 0.7854;
      fb.drawLine(sx + cos(a) * rr, sy + sin(a) * rr, sx + cos(a) * (rr + 3), sy + sin(a) * (rr + 3), COL_SAW);
    }
    fb.fillCircle(sx, sy, 2, COL_MEATD);
  }
  if (bqFiche && bqOn()) ficheDraw(cy);

  // pieges dynamiques
  for (int i = 0; i < nTraps; i++) {
    Trap &t = traps[i]; if (!t.active) continue;
    if (t.kind == KSPIKE) {
      int x = t.tx * TILE, y = t.ty * TILE - cy;
      if (t.st) {
        fb.fillTriangle(x, y + TILE, x + TILE / 2, y, x + TILE, y + TILE, COL_SPK);
        fb.drawTriangle(x, y + TILE, x + TILE / 2, y, x + TILE, y + TILE, COL_MEATD);
      } else if (!t.hidden || nearPlayer(t.tx, t.ty, 56)) {   // fantome : base visible seulement de pres
        fb.fillRect(x + 3, y + TILE - 3, TILE - 6, 3, COL_BLKT);   // base (telegraphe) -- absent si "fantome"
      }
    } else {
      int x = t.tx * TILE, y = (int)t.y - cy;
      if (t.hidden && !t.st && !t.st2) {  // bloc fantome : juste un contour, et seulement quand on s'en approche
        if (fabs((px + PW / 2) - (t.tx * TILE + 8)) < 60 && py > t.y) fb.drawRect(x + 1, y + 1, TILE - 2, TILE - 2, COL_MEATD);
        continue;
      }
      uint16_t c = t.st ? COL_MEAT : COL_BLKT;
      fb.fillRect(x + 1, y + 1, TILE - 2, TILE - 2, c);
      fb.drawRect(x + 1, y + 1, TILE - 2, TILE - 2, COL_MEATD);
      fb.fillRect(x + 2, y + TILE - 4, TILE - 4, 3, COL_MEATD);    // dents dessous
    }
  }

  // briques a pousser (caisse avec croix)
  for (int i = 0; i < nPB; i++) {
    PushB &b = pbs[i];
    int x = b.tx * TILE + (int)b.ox, y = b.ty * TILE + (int)b.oy - cy;
    if (y < -TILE || y > STRIP_H + TILE) continue;
    fb.fillRect(x, y, TILE, TILE, COL_STARD);
    fb.drawRect(x, y, TILE, TILE, COL_MEATD);
    fb.drawRect(x + 2, y + 2, TILE - 4, TILE - 4, COL_MEATD);
    fb.drawLine(x + 2, y + 2, x + TILE - 3, y + TILE - 3, COL_MEATD);
    fb.drawLine(x + TILE - 3, y + 2, x + 2, y + TILE - 3, COL_MEATD);
  }

  if (bossOn) {                                      // boss, tirs, laser, jauges
    if (bossHp > 0) {
      int byc = (int)bossCY() - cy;
      uint16_t bc = bossHit > 0 ? COL_TXT : RGB565(150, 40, 170);
      fb.fillRoundRect(288, byc - 16, 30, 32, 6, bc);
      fb.drawRoundRect(288, byc - 16, 30, 32, 6, COL_MEATD);
      fb.fillRect(292, byc - 8, 8, 6, COL_TXT); fb.fillRect(306, byc - 8, 8, 6, COL_TXT);
      fb.fillRect(292, byc - 6, 3, 3, COL_MEATD); fb.fillRect(306, byc - 6, 3, 3, COL_MEATD);
      fb.fillRect(294, byc + 6, 20, 4, COL_MEATD);
      for (int i = 0; i < 4; i++) if (bbul[i].a) { fb.fillCircle((int)bbul[i].x, (int)bbul[i].y - cy, 4, COL_STAR); fb.drawCircle((int)bbul[i].x, (int)bbul[i].y - cy, 4, COL_MEATD); }
      if (laserOn) {
        int x0 = playerFacingRight ? (int)px + PW : 0, ln = playerFacingRight ? SCREEN_W - x0 : (int)px, ly = (int)py + PH / 2 - cy;
        fb.fillRect(x0, ly - 2, ln, 5, RGB565(255, 60, 60)); fb.drawFastHLine(x0, ly, ln, COL_TXT);
      }
      fb.drawRect(SCREEN_W - 84, 3 - sTop, 80, 6, COL_TXT); fb.fillRect(SCREEN_W - 83, 4 - sTop, (int)(78 * bossHp / bossMaxHp), 4, RGB565(220, 40, 40));
    }
    fb.drawRect(3, 3 - sTop, 62, 6, COL_TXT);        // jauge laser : energie, ou recharge pendant les 20 s
    if (laserCd > 0) fb.fillRect(4, 4 - sTop, (int)(60 * (1 - laserCd / 20.0f)), 4, RGB565(90, 120, 200));
    else             fb.fillRect(4, 4 - sTop, (int)(60 * laserE / 3.0f), 4, RGB565(255, 220, 60));
    if (bossHp > 0 && bossT < 8) { fb.setTextColor(COL_TXT); fb.setTextSize(1); fb.setCursor(4, 14 - sTop); fb.print("P1+P2 = LASER"); }
  }

  // drapeau (entite, peut se deplacer) : un mat haut, drapeau qui descend avec le joueur pendant la glissade
  { int gx = goalTX * TILE, base = (goalTY + 1) * TILE, top = base - min(POLE_H, base);
    uint16_t fc = (goalFlash > 0) ? COL_TXT : COL_GOAL;
    fb.fillRect(gx + 3, top - cy, 2, base - top, COL_TXT);
    fb.fillCircle(gx + 4, top - cy - 1, 2, COL_STAR);
    int fy = flagSlide ? (int)max((float)top + 2, min(py - 2, (float)base - 12)) : top + 2;
    if (icOn(2)) drawIcon(2, gx + 5, fy - cy - 4, false);                 // BRIQUE : drapeau choisi sur la page
    else fb.fillTriangle(gx + 5, fy - cy, gx + 5, fy - cy + 8, gx + 15, fy - cy + 4, fc);
  }

  DBG(5);
  // fantomes des adversaires (course multijoueur), couleur du numero de joueur
  if (raceMode) for (int k = 0; k < mpN; k++) {
    if (!(mpIn[k] && mpOn[k] && mpPos[k])) continue;
    const int gx = mpX[k], gy = mpY[k] - cy; const uint16_t c = MP_COL[mpCol[k] & 3];
    fb.drawRoundRect(gx, gy, (int)PW, (int)PH, 3, c); fb.drawRoundRect(gx + 1, gy + 1, (int)PW - 2, (int)PH - 2, 3, c);
  }
  if (raceMode) { const int mx = (int)px + (int)PW / 2, my = (int)py - cy - 6; fb.fillTriangle(mx - 5, my - 6, mx + 5, my - 6, mx, my, MP_COL[myCol & 3]); }   // ma couleur, au-dessus de moi
  DBG(6);

#ifdef GAME_BENCH
  gT[2] += micros() - gq; gq = micros();
#endif
  // joueur
  // recompense : palier 0..4 derive de rewardPower() (bonnes reponses + niveaux reussis) --
  // cumulatif, evolue au fil de la session. Le corps ET la
  // trainee se teintent ensemble -- COL_MEAT/COL_MEATD (UI) ne changent jamais.
  // BRIQUE : dans un jeu de la page (seul ou en course), pas d'avatar de progression (palier, skin, trainee) : tout le monde part egal
  int tier = bqOn() ? 0 : computePlayerTier();
  uint16_t bodyCol  = COL_PLAYER_TIER[tier];
  uint16_t bodyColD = COL_PLAYERD_TIER[tier];
  if (skinSel > 0 && !bqOn()) { bodyCol = skinColor(skinSel); bodyColD = SKIN_COLD[skinSel]; }   // skin achete en boutique

  // trainee de particules derriere le joueur (voir updateTrail()) : chaque
  // particule s'efface en retrecissant avec son age -- pas de canal alpha
  // sur GFXcanvas16, on simule le fondu par la taille.
  int ttier = bqOn() ? 0 : effTier();
  if (ttier > 0) {
    uint16_t trailCols[NUM_PLAYER_TIERS - 1] = { tft.color565(180, 120, 60), tft.color565(210, 210, 220),
                                                  COL_STAR, tft.color565(120, 230, 255) };   // bronze/argent/or/diamant
    uint16_t tc = trailCols[ttier - 1];
    for (int i = 0; i < nTrail; i++) {
      float f = 1.0f - trailAge[i] / trailLife[i];   // 1 = fraiche .. 0 = sur le point de disparaitre
      if (f <= 0) continue;
      // rendu pixelise : particules calees sur une grille de 4 px, gros pixel de 4 puis
      // de 2 px en fin de vie, et couleur assombrie de moitie sur la deuxieme moitie de vie
      int sz = (f > 0.5f) ? 4 : 2;
      uint16_t base = tc;
      if (ttier >= 4) { const uint16_t rb[5] = { 0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF }; base = rb[trailK[i] % 5]; }   // Diamant : arc-en-ciel
      else if (ttier == 3 && trailK[i] == 0) base = COL_TXT;   // Or uniquement : quelques points blancs brillants -- Argent reste un style "argent pur" distinct, sans melange
      if (trailSel > 0) { const uint16_t rb[5] = { 0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF }; base = (trailSel == NTRAIL - 1) ? rb[trailK[i] % 5] : TRAIL_COL[trailSel]; }   // trainee achetee
      uint16_t c = (f > 0.5f) ? base : (uint16_t)((base >> 1) & 0x7BEF);
      int sx = ((int)trailX[i] / 4) * 4, sy = (((int)trailY[i] - cy) / 4) * 4;
      if (sy < -10 || sy > SCREEN_H + 10) continue;
      fb.fillRect(sx + (4 - sz) / 2, sy + (4 - sz) / 2, sz, sz, c);
    }
  }
  // corps deforme (squash & stretch), ancre aux pieds
  if (icOn(0)) drawIcon(0, (int)px + PW / 2 - 8, (int)(py + PH) - cy - 16, !playerFacingRight);   // BRIQUE : heros choisi sur la page (ancre aux pieds)
  else { float fw = PW * sqX, fh = PH * sqY; int lean = 0;
    if (pushVis) { fw *= 0.85f; fh *= 1.06f; lean = playerFacingRight ? 2 : -2; }   // pousse : compresse + penche
    int w = (int)(fw + 0.5f), h = (int)(fh + 0.5f);
    int bx = (int)(px + PW / 2) + lean, by = (int)(py + PH) - cy;
    int x0 = bx - w / 2, y0 = by - h;
    if (bqOn() && bqHero >= 0) { bodyCol = BQ_HEROS[bqHero].col; bodyColD = blendCol(bodyCol, 0x0000, 0.45f); }   // niveau perso : heros choisi sur la page
    skinBody(fb, bqOn() ? 0 : skinSel, x0, y0, w, h, 3, bodyCol, bodyColD);
    int ey = y0 + h * 4 / 14, eh = max(2, h * 3 / 14);
    int ex1 = x0 + w * 3 / 12, ex2 = x0 + w * 7 / 12;
    if (pushVis) { ex1 += lean / 2; ex2 += lean / 2; }
    // mimiques : joie (piece/etoile) > peur (grande chute) > inquiet (scie/ennemi proche) > effort (accroche) > normal
    bool scared = !onGround && !wallSliding && vy > 200 && (py - fallPeakY) > 5 * TILE;
    int threatDir = 0; bool threat = false;
    { float pcx = px + PW / 2, pcy = py + PH / 2;
      for (int i = 0; i < nSaws && !threat; i++) { Saw &sw = saws[i]; if (!sw.active) continue;
        float dx = sawX(sw) - pcx, dy = sawY(sw) - pcy;
        if (sqrtf(dx * dx + dy * dy) - sw.r < 40) { threat = true; threatDir = dx > 0 ? 1 : -1; } }
      for (int i = 0; i < nEnemies && !threat; i++) { Enemy &en = enemies[i]; if (!en.active) continue;
        float dx = en.x - pcx, dy = en.y - EH / 2 - pcy;
        if (sqrtf(dx * dx + dy * dy) < 44) { threat = true; threatDir = dx > 0 ? 1 : -1; } } }
    int my = y0 + h * 10 / 14;
    const uint16_t blush = RGB565(255, 150, 170);
    if (happyT > 0) {                                  // joie : yeux selon ce qui vient d'etre pris + grand sourire + joues
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        if (happyKind == 2)      { fb.drawFastHLine(ex - 1, ey + 1, 4, COL_TXT); fb.drawFastVLine(ex + 1, ey - 1, 4, COL_TXT); }   // etincelles
        else if (happyKind == 1) { fb.fillCircle(ex + 1, ey + 1, 2, COL_STAR); fb.drawPixel(ex + 1, ey + 1, bodyColD); }          // yeux-pieces
        else { fb.drawLine(ex - 1, ey + 2, ex, ey, bodyColD); fb.drawLine(ex, ey, ex + 1, ey + 2, bodyColD); } }
      fb.fillRect(x0 + w / 4, my, w / 2, 3, bodyColD);              // grande bouche ouverte
      fb.drawPixel(x0 + w / 4 - 1, my - 1, bodyColD); fb.drawPixel(x0 + w * 3 / 4, my - 1, bodyColD);
      fb.fillRect(x0 + 1, my - 1, 2, 2, blush); fb.fillRect(x0 + w - 3, my - 1, 2, 2, blush);
    } else if (excitedT > 0 && !scared) {              // elan (ressort / saut mural) : yeux ^ ^, bouche ouverte + langue, bulle
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        fb.drawLine(ex - 1, ey + 2, ex, ey, bodyColD); fb.drawLine(ex, ey, ex + 1, ey + 2, bodyColD); }
      fb.fillRect(x0 + w / 4, my - 1, w / 2, 4, bodyColD);
      fb.fillRect(x0 + w / 2 - 1, my + 2, 3, 1, RGB565(255, 90, 110));
      drawBubble(bx, y0 - 17, ((int)(excitedT * 6) & 1) ? "YAHOO!" : "WOUHOU");
    } else if (scared) {                               // yeux ecarquilles + bouche ouverte + bulle
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        fb.fillRect(ex - 1, ey - 1, 4, eh + 2, COL_TXT); fb.fillRect(ex, ey + eh / 2, 2, 2, bodyColD); }
      fb.fillRect(x0 + w / 2 - 1, my, 3, 3, bodyColD);
      drawBubble(bx, y0 - 17, "AH!");
    } else if (threat) {                               // yeux ronds qui fixent le danger + goutte de sueur
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        fb.fillRect(ex - 1, ey, 4, eh + 1, COL_TXT); fb.fillRect(ex + (threatDir > 0 ? 1 : 0), ey + eh / 2, 2, 2, bodyColD); }
      fb.drawFastHLine(x0 + w / 3, my, w / 3, bodyColD);
      fb.fillCircle(x0 + w + 1, y0 + 2, 1, 0x07FF);
    } else if (wallSliding) {                          // yeux plisses : effort
      fb.fillRect(ex1 - 1, ey + eh / 2, 4, 1, bodyColD); fb.fillRect(ex2 - 1, ey + eh / 2, 4, 1, bodyColD);
      fb.drawFastHLine(x0 + w / 4, my, w / 2, bodyColD);
    } else {
      fb.fillRect(ex1, ey, 2, eh, bodyColD);
      fb.fillRect(ex2, ey, 2, eh, bodyColD);
    }
    if (excitedT > 0 && vy < -100)                      // lignes de vitesse sous les pieds
      for (int k = -1; k <= 1; k++) fb.drawFastVLine(bx + k * 4, by + 2, k ? 6 : 11, COL_TXT);
  }

  // etincelles (prise d'etoile / piece)
  for (int i = 0; i < 12; i++) { Spark &k = sparks[i]; if (k.life <= 0) continue;
    int sz = k.life > 0.25f ? 3 : 1;
    fb.fillRect((int)k.x - sz / 2, (int)k.y - cy - sz / 2, sz, sz, k.c); }

#ifdef GAME_BENCH
  gT[3] += micros() - gq; gq = micros();
#endif
  // HUD (uniquement visible dans la bande du haut)
  if (sTop == 0) {
  fb.setTextColor(COL_TXT); fb.setTextSize(1);
  { char hb[24];
    if (bqOn() && bqChrono) { int r = bqChrono - (int)((millis() - levelStartMs) / 1000); snprintf(hb, sizeof(hb), "Temps: %d", r < 0 ? 0 : r); fb.setTextColor(r <= 5 ? COL_MEAT : COL_TXT); }
    else if (bqOn()) snprintf(hb, sizeof(hb), "%.20s", bqFiche && millis() < fMsgFin ? fMsg : bqTitre[0] ? bqTitre : "Mon jeu");   // BRIQUE fiche : message d'une regle
    else snprintf(hb, sizeof(hb), "NIV %d/%d", curLevel + 1, NUM_LEVELS);
    gfxPrintAt(fb, 1, 4, 2 - sTop, hb); fb.setTextColor(COL_TXT);
    for (int i = 0; i < nStarPos; i++) drawStarFancy(fb, 138 + i * 18, 8 - sTop, 4, 1.0f + i * 1.7f, starGot[i]);   // mini-etoiles du niveau
    if (bqOn() && bqVies) snprintf(hb, sizeof(hb), "Vies: %d", bqVies - (int)deaths); else snprintf(hb, sizeof(hb), "Morts: %u", deaths);
    gfxPrintAt(fb, 1, 316 - textW(fb, 1, hb), 2 - sTop, hb); } }

#ifdef GAME_BENCH
  gT[4] += micros() - gq; gq = micros();
#endif
  DBG(8);
  dBlitPlain(sTop);
#ifdef GAME_BENCH
  gT[5] += micros() - gq;
#endif
  }
  DBG(9);
}

// -------- ecrans (dessin direct tft) --------
void drawStars3(int x, int y, int got, int sz) {
  for (int i = 0; i < 3; i++)
    drawStarFancy(tft, x + i * (sz * 2 + 8) + sz, y + sz, sz - 1, 1.0 + i * 1.7, i < got);
}

// petit "bouton" illustre avec une fleche (0=gauche, 1=droite, 2=haut) + son
// nom, pour que la fonction de chaque touche physique se comprenne sans lire
// (retour : "on ne comprend rien").
void drawButtonGlyph(int cx, int cy, uint16_t col, int dir, const char* label) {
  tft.fillRoundRect(cx - 24, cy - 24, 48, 48, 10, col);
  tft.drawRoundRect(cx - 24, cy - 24, 48, 48, 10, tft.color565(255, 255, 255));
  uint16_t dark = tft.color565(20, 20, 28);
  if (dir == 0)      tft.fillTriangle(cx + 10, cy - 12, cx + 10, cy + 12, cx - 10, cy, dark);        // fleche gauche
  else if (dir == 1) tft.fillTriangle(cx - 10, cy - 12, cx - 10, cy + 12, cx + 10, cy, dark);        // fleche droite
  else               tft.fillTriangle(cx - 12, cy + 10, cx + 12, cy + 10, cx, cy - 10, dark);        // fleche haut
  tft.setTextColor(COL_TXT); tft.setTextSize(1);
  int lw = strlen(label) * 6;
  tft.setCursor(cx - lw / 2, cy + 32); tft.print(label);
}

// --- mascotte (la flamme) sur l'ecran titre : bouge, change d'humeur et commente la facon de jouer ---
int  mascotMood, mascotLast = -1;     // 0 neutre 1 content 2 narquois 3 inquiet 4 choque
unsigned long mascotNextMs, mascotAnimMs;
unsigned long mascotMoodSince;        // ms : debut de l'humeur courante (animations d'entree)
char mascotTxt[84];
unsigned long introMs; int introStep = -1;     // intro de l'ecran titre : GAUCHE, DROITE, SAUT (1,2 s chacun)
bool mascotGl;                        // la bulle courante est dite par un glitch (sinon par la flamme)
FastCanvas gcv(108, 116);             // tampon du sorcier de l'ecran titre (wizard1)
// echanges glitch -> flamme : le glitch veut figer le jeu, la flamme invite le joueur a le modifier lui-meme
const char *const DLG[][2] = {
  { "Ce jeu est fini.|Personne ne le changera.", "Faux ! Ouvre le code|et change-le toi-meme !" },
  { "Tu ne sais pas coder.", "Change une couleur :|c'est deja coder !" },
  { "Les niveaux sont figes.|Pour toujours.", "Invente ton niveau|et ajoute-le au jeu !" },
  { "%s ? Obsolete.", "Alors fais la version|suivante toi-meme !" },
  { "Le quiz est nul.", "Ecris tes questions|dans questions.txt !" },
  { "Cette flamme est moche.", "Redessine-moi !|Change mes couleurs." },
  { "Je vais tout casser.", "Chaque bug corrige|te rend plus faible." },
  { "Trop dur, ce jeu.|Abandonne.", "Change la gravite|dans le code. Facile !" },
  { "Aucun joueur ne|modifiera ce jeu.", "Toi, si. Flashe ta|version, on verra !" },
  { "Je suis un bug.|Indestructible.", "Trouve-moi dans le|code et efface-moi !" },
};
const int NDLG = sizeof(DLG) / sizeof(DLG[0]);

void mascotPick() {
  static int pend = -1;
  if (pend >= 0) {                                     // la flamme repond au glitch
    strncpy(mascotTxt, DLG[pend][1], sizeof(mascotTxt) - 1); mascotGl = false; mascotMood = 1; mascotMoodSince = millis(); pend = -1; return;
  }
  if (!updMsg[0] && random(0, 2) == 0) {               // un glitch prend la parole
    pend = random(0, NDLG); snprintf(mascotTxt, sizeof(mascotTxt), DLG[pend][0], FW_VERSION); mascotGl = true; mascotMood = 3; mascotMoodSince = millis(); return;
  }
  mascotGl = false;
  int lvDone = 0, stars = 0;
  for (int i = 0; i < NUM_LEVELS; i++) { if (bestTime[i] > 0) lvDone++; stars += bestStars[i]; }
  int qa = 0, qc = 0;
  for (int i = 0; i < NCAT; i++) { qa += quizAskedByCat[i]; qc += quizCorrectByCat[i]; }
  static char cand[24][84]; static int moodc[24]; int n = 0;
#define MADD(m, ...) do { if (n < 24) { snprintf(cand[n], 84, __VA_ARGS__); moodc[n] = (m); n++; } } while (0)
  if (totalWins == 0 && totalDeaths == 0) { MADD(1, "Salut ! Je suis la flamme du jeu.|Aide-moi a le terminer !"); MADD(2, "Des glitchs veulent casser ce jeu.|On ne va pas les laisser faire."); }
  if (totalDeaths >= 5 && totalDeaths > totalWins * 3) { MADD(3, "%u morts pour %u victoires...|Je suis en miettes, la !", (unsigned)totalDeaths, (unsigned)totalWins); MADD(4, "Aie ! Menage-moi un peu,|je ne suis pas en acier."); }
  if (totalWins >= 3 && totalDeaths * 2 < totalWins) MADD(1, "Si peu de morts ! Tu me traites|avec respect, j'adore.");
  if (bestCombo >= 3) MADD(1, "Un combo de %d ! Tu m'enchaines|sans me laisser mourir.", bestCombo);
  if (bestCombo <= 1 && totalWins >= 4) MADD(2, "Aucun combo ? Essaie sans mourir,|juste pour voir.");
  if (lvDone >= 3 && stars < lvDone * 3 / 2) MADD(2, "Tu snobes les etoiles...|Elles brillent pourtant !");
  if (lvDone >= 3 && stars >= lvDone * 5 / 2) MADD(1, "Toutes ces etoiles ! Un vrai|collectionneur, toi.");
  if (coins >= 20) MADD(1, "%u pieces en poche ! Un tour a|la boutique ?", (unsigned)coins);
  if (coins == 0 && totalWins >= 3) MADD(2, "Zero piece ? Elles se cachent dans|les niveaux, ouvre l'oeil.");
  if (speedBoostLv == 0 && coins >= SPEED_BOOST_COST) MADD(2, "Un palier de vitesse ? Ca change|tout, tu verras.");
  if (skinSel == 0 && coins >= 40) MADD(2, "Un nouveau look pour moi ?|La boutique en propose !");
  if (totalPlaytime >= 600) MADD(3, "Deja %u min de jeu... Pense a|boire un peu d'eau !", (unsigned)(totalPlaytime / 60));
  if (qa >= 4 && qc * 2 < qa) MADD(4, "Le quiz te donne du fil a retordre ?|Courage, ca vient.");
  if (qa >= 4 && qc * 4 >= qa * 3) MADD(1, "Bravo au quiz, t'as un vrai|cerveau de cuisinier !");
  if (totalScore >= 1000) MADD(1, "%u points au compteur !|Je suis fier de toi.", (unsigned)totalScore);
  MADD(glitchLvl > 60 ? 3 : 1, "Glitchs : %d%%.|Chaque niveau fini les repousse.", glitchLvl);
  MADD(2, "Version %s, compilee le %s.|La prochaine sera encore mieux.", FW_VERSION, __DATE__);
  MADD(0, "Ce jeu est a toi :|change-le, ameliore-le !");
  MADD(1, "Une idee ? Code-la,|flashe-la, joue-la !");
  MADD(2, "Chaque mise a jour|repousse les glitchs.");
  if (evo < EVO_MAX) MADD(1, "Les etoiles bleues ?|4 bonnes reponses = j'evolue !"); else MADD(1, "Robot final !|Merci, je suis au max !");
  if (glitchLvl >= 75) MADD(4, "Le jeu se desagrege...|Vite, gagne des niveaux !");
  if (glitchLvl <= 25) MADD(1, "Presque plus de bugs !|Ce jeu devient vraiment bien.");
  if (updMsg[0]) { n = 0; MADD(1, "%s", updMsg); updMsg[0] = 0; }   // une nouvelle version vient d'etre flashee : on le dit d'abord
#undef MADD
  int k = random(n); if (k == mascotLast && n > 1) k = (k + 1) % n;
  mascotLast = k; mascotMood = moodc[k]; mascotMoodSince = millis(); strncpy(mascotTxt, cand[k], sizeof(mascotTxt) - 1);
}

void mascotDrawBubble() {                              // texte en grand (taille 2) s'il tient, sinon en petit
  tft.fillRect(0, 44, SCREEN_W, 58, COL_BG);
  const uint16_t GBG = RGB565(16, 8, 24);
  if (mascotGl) { tft.fillRoundRect(8, 44, 304, 44, 8, GBG); tft.drawRoundRect(8, 44, 304, 44, 8, 0xF81F); tft.fillTriangle(250, 87, 268, 87, 262, 100, GBG); }
  else { tft.fillRoundRect(8, 44, 304, 44, 8, COL_TXT); tft.fillTriangle(150, 87, 170, 87, 160, 94, COL_TXT); }
  char b[84]; strncpy(b, mascotTxt, sizeof(b) - 1); b[sizeof(b) - 1] = 0;
  char *l2 = strchr(b, '|'); if (l2) *l2++ = 0;
  int sz = textW(tft, 2, b) <= 292 && (!l2 || textW(tft, 2, l2) <= 292) ? 2 : 1;
  tft.setTextSize(sz);
  int y1 = l2 ? (sz == 2 ? 48 : 54) : (sz == 2 ? 58 : 62), y2 = sz == 2 ? 67 : 68;
  for (int pass = mascotGl ? 0 : 1; pass < 2; pass++) {   // glitch : texte dedouble cyan / magenta
    int dx = mascotGl && pass == 0 ? 1 : 0;
    tft.setTextColor(mascotGl ? (pass == 0 ? 0x07FF : 0xF81F) : COL_BG);
    ctextAt(SCREEN_W / 2 + dx, y1, b); if (l2) ctextAt(SCREEN_W / 2 + dx, y2, l2);
  }
  tft.setTextSize(1);
}
static const uint8_t TITLE_WIZ[6] = { 0, 1, 3, 5, 7, 9 };   // animations (WIZ_ANIM) des 6 sorciers de l'ecran titre : wizard1 puis les idle des 5 sorciers de question
int titleWiz = 0, titleWizCnt = 0;
void glitchTitleDraw(unsigned long now) {              // le sorcier de l'ecran titre (change regulierement), a droite, tourne vers la flamme
  const int wi = TITLE_WIZ[titleWiz]; const PAnim &A = WIZ_ANIM[wi];
  int axe = A.right ? A.w - 1 - A.ax : A.ax, fx = (108 - A.w) / 2 + axe, fy = min(110, 115 - (A.h - 1 - A.ay));   // centre dans le tampon, pieds en bas
  gcv.fillScreen(COL_BG);
  sprDraw(gcv, A, WIZ_PIX, WIZ_PAL[A.ch], now / WIZ_MS[wi], fx, fy, false, 1);
  tft.drawRGBBitmap(212, 90, gcv.getBuffer(), 108, 116);
  if (mascotGl) tft.fillTriangle(250, 87, 268, 87, 262, 100, RGB565(16, 8, 24));   // queue de sa bulle, par-dessus le tampon
}

static int mk(int v) { return (int)(v * 1.5f + 0.5f); }         // echelle de la mascotte (visage dessine pour 36x40, affiche en x1.5)

FastCanvas mc(104, 108);   // tampon de la mascotte (ecran titre), ~22 Ko

void mascotDraw(unsigned long now) {
  const int cx = 52, feet = 100;      // coordonnees locales du tampon mc (affiche en (108, 90))
  float t = now / 1000.0f, tm = (now - mascotMoodSince) / 1000.0f;
  float sx = 1, sy = 1, dx = 0, hop = 0;
  switch (mascotMood) {
    case 1: {                                                    // content : saute avec etirement en l'air, ecrasement a l'atterrissage
      float u = fmodf(tm * 2.0f, 1.0f);
      hop = 4 * u * (1 - u) * 24;
      if (u < 0.10f || u > 0.90f) { sx = 1.16f; sy = 0.80f; } else { sx = 0.92f; sy = 1.12f; }
      dx = sinf(tm * 4.0f) * 2;
      break; }
    case 2:                                                      // narquois : se balance lentement, bombe le torse
      dx = sinf(t * 1.3f) * 7; sx = 1.04f + 0.03f * sinf(t * 2.6f); sy = 1.04f + 0.05f * sinf(t * 2.6f + 1);
      break;
    case 3:                                                      // inquiet : tremble, se tasse
      dx = sinf(t * 45.0f) * 2.0f; sy = 0.93f + 0.02f * sinf(t * 9); sx = 1.05f;
      break;
    case 4:                                                      // choque : sursaute (etirement) puis vibre
      if (tm < 0.35f) { float r = 1 - tm / 0.35f; hop = 20 * r; sy = 1 + 0.25f * r; sx = 1 - 0.12f * r; }
      dx = sinf(t * 30.0f) * 1.2f;
      break;
    default:                                                     // neutre : respire, se dandine
      sx = 1 + 0.04f * sinf(t * 3.0f); sy = 1 - 0.05f * sinf(t * 3.0f); dx = sinf(t * 0.9f) * 4;
  }
  if (now - introMs < 3600) {                                   // intro : va a gauche, a droite, puis saute (en meme temps que le bouton)
    float it = (now - introMs) / 1200.0f, fr = it - (int)it; sx = sy = 1; hop = 0; dx = 0;
    if (it < 1) dx = -24 * sinf(fr * 3.1416f);
    else if (it < 2) dx = 24 * sinf(fr * 3.1416f);
    else { hop = 4 * fr * (1 - fr) * 30; if (fr < 0.1f || fr > 0.9f) { sx = 1.16f; sy = 0.8f; } else { sx = 0.9f; sy = 1.12f; } }
  }
  int tPose = mascotMood == 1 && hop > 3 ? 2 : mascotMood == 4 && tm < 0.35f ? 3 : 0; bool tRight = true;   // pose du bonhomme
  if (now - introMs < 3600) { float it = (now - introMs) / 1200.0f; if (it < 2) { tPose = 1; tRight = it >= 1; } else tPose = hop > 3 ? 2 : 4; }
  int w = (int)(mk(36) * sx), h = (int)(mk(40) * sy), hp = (int)hop;
  mc.fillScreen(COL_BG);                // dessin hors ecran puis un seul envoi : plus de scintillement
  int shw = max(14, mk(30) - hp);                                // ombre qui retrecit quand il saute
  mc.fillRect(cx - shw / 2 + (int)dx, feet + 1, shw, 3, tft.color565(10, 10, 16));
  int tier = computePlayerTier();                       // la flamme prend la couleur du palier atteint par le joueur, a l'instant ou elle s'affiche
  uint16_t c = COL_PLAYER_TIER[tier], d = COL_PLAYERD_TIER[tier];
  int x0 = cx - w / 2 + (int)dx, y0 = feet - h - hp;    // (x0, y0, w, h) : boite du visage, dans le coeur clair de la flamme
  if (flameStage() == 4) drawChibi(mc, cx + (int)dx, feet - hp, 3, tPose, (int)(now / 110), tRight, t);
  else { int fs = flameStage(); float es = min(1.1f, evoScale()), wr = (fs == 0 ? 26 : 27 * es) * sx, k = 27 * sx / 6.5f;   // pixels de 16 a l'accueil
    flameStaged(mc, x0 + w / 2, feet - hp, (fs == 0 ? 84 : 92 * es) * sy, wr, (sinf(t * 9) * 2.2f + sinf(t * 5.3f + 1) * 1.4f) * k - dx * 0.5f, c, dMix(c, RGB565(255, 255, 255), 100), dMix(c, RGB565(255, 255, 255), 215), t, true, false); }
  if (flameStage() == 3) {                                       // flamme : visage expressif
  int ey = y0 + h * 3 / 10, ex1 = x0 + w / 4 - 3, ex2 = x0 + w * 5 / 8 - 1, my = y0 + h * 7 / 10, mx = x0 + w / 2;
  bool blink = (now % 3300) < 130;
  uint16_t blush = tft.color565(255, 150, 170);
  switch (mascotMood) {
    case 1:                                                      // content : yeux ^ ^, grand sourire, joues
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        mc.drawLine(ex, ey + mk(6), ex + mk(3), ey + 1, d); mc.drawLine(ex + mk(3), ey + 1, ex + mk(6), ey + mk(6), d);
        mc.drawLine(ex, ey + mk(6) + 1, ex + mk(3), ey + 2, d); mc.drawLine(ex + mk(3), ey + 2, ex + mk(6), ey + mk(6) + 1, d); }
      mc.fillRect(mx - mk(6), my, mk(12), mk(4), d); mc.fillRect(mx - mk(8), my - 2, 2, 3, d); mc.fillRect(mx + mk(7), my - 2, 2, 3, d);
      mc.fillRect(x0 + 4, my - 3, mk(4), mk(3), blush); mc.fillRect(x0 + w - 4 - mk(4), my - 3, mk(4), mk(3), blush);
      break;
    case 2:                                                      // narquois : paupieres mi-closes, sourcil leve, sourire en coin
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1; mc.fillRect(ex, ey + mk(4), mk(6), mk(5), d); mc.fillRect(ex - 1, ey + mk(3), mk(8), 2, d); }
      mc.drawLine(ex1, ey - 3, ex1 + mk(6), ey - 1, d); mc.drawLine(ex1, ey - 2, ex1 + mk(6), ey, d);
      mc.fillRect(mx - mk(6), my + 1, mk(11), 2, d); mc.fillRect(mx + mk(5), my - 1, 2, 3, d); mc.fillRect(mx + mk(6), my - 3, 2, 3, d);
      break;
    case 3:                                                      // inquiet : yeux blancs qui louchent, sueur, bouche tremblante
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        mc.fillRect(ex - 1, ey, mk(8), mk(10), COL_TXT); mc.fillRect(ex + mk(2) + (int)(sinf(t * 2) * 2), ey + mk(4), mk(3), mk(3), d); }
      mc.fillCircle(x0 + w - 3, y0 + 8 + (int)(fmodf(t * 14, 12)), 3, 0x07FF);
      for (int q = 0; q < 5; q++) { mc.drawLine(mx - mk(7) + q * mk(3), my + (q & 1) * 2, mx - mk(4) + q * mk(3), my + ((q + 1) & 1) * 2, d); }
      break;
    case 4:                                                      // choque : grands yeux, bouche en O
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        mc.fillRect(ex - 1, ey - 2, mk(8), mk(12), COL_TXT); mc.fillRect(ex + mk(2), ey + mk(4), mk(3), mk(3), d); }
      mc.fillRoundRect(mx - mk(4), my - 2, mk(8), mk(8), 3, d);
      break;
    default:                                                     // neutre
      for (int k = 0; k < 2; k++) { int ex = k ? ex2 : ex1;
        if (blink) mc.fillRect(ex, ey + mk(4), mk(5), 2, d); else mc.fillRect(ex, ey, mk(5), mk(9), d); }
      mc.fillRect(mx - mk(4), my + 1, mk(8), 2, d);
  }
  } else if (flameStage() == 0) {                                // avatar en pixels : deux yeux sur la rangee du haut
    int n = evoBlocks(), bh = (int)(84 * sy * 0.19f + 0.5f), top = feet - hp - pixRows(n) * bh + 1, mx = x0 + w / 2, e = n <= 3 ? 3 : 4;
    bool bl = (now % 3300) < 130;
    mc.fillRect(mx - 3 - e, top + bh / 3, e, bl ? 1 : e + 1, d); mc.fillRect(mx + 3, top + bh / 3, e, bl ? 1 : e + 1, d);
  }
  { uint16_t *b = mc.getBuffer(); const uint16_t SHC = tft.color565(10, 10, 16), MK = 0x0001;   // contour sombre autour du personnage
    auto solid = [&](int x, int y) { uint16_t v = b[y * 104 + x]; return v != COL_BG && v != SHC && v != MK; };
    for (int y = 0; y < 108; y++) for (int x = 0; x < 104; x++) if (b[y * 104 + x] == COL_BG && ((x > 0 && solid(x - 1, y)) || (x < 103 && solid(x + 1, y)) || (y > 0 && solid(x, y - 1)) || (y < 107 && solid(x, y + 1)))) b[y * 104 + x] = MK;
    for (int i = 0; i < 104 * 108; i++) if (b[i] == MK) b[i] = RGB565(200, 204, 222); }
  { // glitchs sur la flamme : lignes decalees en x + carres colores (comme les ennemis) ; rafales regulieres, plus forts si le jeu est bugge
    static const uint16_t GC[4] = { RGB565(255, 40, 200), RGB565(40, 240, 255), RGB565(120, 255, 60), RGB565(250, 250, 250) };
    uint16_t *b = mc.getBuffer(); uint32_t fr = now / 80; bool burst = (now % 1700) < 260;
    int thr = (burst ? 220 : 15) + glitchLvl;
    for (int r = 0; r < 108; r += 3) {
      uint32_t h = gHash(r * 131 + fr * 7919); if ((int)(h & 1023) >= thr) continue;
      int sh = (int)((h >> 12) & 7) - 3; if (!sh) sh = 4;
      for (int rr = r; rr < r + 3 && rr < 108; rr++) { uint16_t *p = b + rr * 104; if (sh > 0) memmove(p + sh, p, (104 - sh) * 2); else memmove(p, p - sh, (104 + sh) * 2); }
    }
    int nsq = (burst ? 7 : 2) + glitchLvl / 30;
    for (int k = 0; k < nsq; k++) {
      uint32_t h = gHash(k * 977 + fr * 31 + 7); if (!burst && (h & 3)) continue;
      int x = 22 + (h >> 4) % 56, y = 14 + (h >> 11) % 86, n = 1 + ((h >> 20) & 3);
      for (int i = 0; i < n; i++) mc.fillRect(x + (i & 1) * 4, y + (i >> 1) * 4, 4, 4, GC[(h >> (22 + i * 2)) & 3]);
    }
  }
  tft.drawRGBBitmap(108, 90, mc.getBuffer(), 104, 108);
  if (!mascotGl) tft.fillTriangle(150, 87, 170, 87, 160, 94, COL_TXT);   // queue de la bulle (recouverte en bas par le tampon)
  glitchTitleDraw(now);
}

void drawIntroButton(int i) {                          // un gros bouton + son nom, centre en bas (i = 3 : on efface)
  tft.fillRect(0, 212, SCREEN_W, 28, COL_BG);
  if (i > 2) return;
  const char* lab[3] = { "GAUCHE", "DROITE", "SAUT" };
  uint16_t col[3] = { tft.color565(90, 200, 255), tft.color565(255, 160, 60), tft.color565(120, 230, 90) }, dark = tft.color565(20, 20, 28);
  int tw = textW(tft, 2, lab[i]), x = (SCREEN_W - (26 + 8 + tw)) / 2, cy = 226, bx = x + 13;
  tft.fillRoundRect(x, cy - 13, 26, 26, 6, col[i]);
  if (i == 0)      tft.fillTriangle(bx + 6, cy - 8, bx + 6, cy + 8, bx - 7, cy, dark);
  else if (i == 1) tft.fillTriangle(bx - 6, cy - 8, bx - 6, cy + 8, bx + 7, cy, dark);
  else             tft.fillTriangle(bx - 8, cy + 6, bx + 8, cy + 6, bx, cy - 7, dark);
  tft.setTextSize(2); tft.setTextColor(col[i]); ltextAt(x + 34, cy - 8, lab[i]); tft.setTextSize(1);
}
void mascotTick() {
  unsigned long now = millis();
  { int st = now - introMs < 3600 ? (int)((now - introMs) / 1200) : 3;
    if (st != introStep) { introStep = st; drawIntroButton(st); } }
  if (now >= mascotNextMs) { if (++titleWizCnt % 2 == 0) titleWiz = (titleWiz + 1 + random(0, 5)) % 6; mascotPick(); mascotDrawBubble(); mascotNextMs = now + 5500; mascotAnimMs = 0; }   // mascotte redessinee tout de suite
  if (now - mascotAnimMs > 70) { mascotAnimMs = now; mascotDraw(now); }
}

void drawEvoInfo() {                                   // progression de l'avatar (coin gauche de l'ecran titre) : stade, barre globale, bonnes reponses vers le prochain stade
  const uint16_t dim = tft.color565(150, 160, 180), VI = tft.color565(110, 70, 220);
  tft.setTextSize(1); tft.setTextColor(dim); ltextAt(4, 106, "AVATAR");
  tft.setTextSize(2); tft.setTextColor(COL_STAR); ltextf(4, 117, "%d/%d", evo, EVO_MAX);
  tft.setTextSize(1); tft.setTextColor(COL_TXT);
  char nm[28]; strncpy(nm, evoName(evo), sizeof(nm) - 1); nm[sizeof(nm) - 1] = 0;
  char *l2 = nullptr;
  if (textW(tft, 1, nm) > 100) { l2 = strrchr(nm, ' '); if (l2) *l2++ = 0; }
  ltextAt(4, 138, nm); if (l2) ltextAt(4, 149, l2);
  tft.drawRect(4, 162, 96, 8, dim); tft.fillRect(5, 163, evo * 94 / EVO_MAX, 6, COL_MEAT);
  for (int i = 0; i < EVO_STEP; i++) { tft.drawRect(4 + i * 24, 174, 22, 8, VI); if (i < evoPts) tft.fillRect(5 + i * 24, 175, 20, 6, COL_STAR); }
  tft.setTextColor(dim);
  if (evo < EVO_MAX) { ltextf(4, 185, "Encore %d pour", EVO_STEP - evoPts); ltextAt(4, 195, "evoluer"); } else ltextAt(4, 185, "Stade max !");
}

// intro au demarrage (~2,4 s, un bouton = passer) : les lettres de BRIQUE tombent comme des briques, rond de chargement au centre
bool bootBtn() { return digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_JUMP) == LOW; }
void bootIntro() {
  const char* T = "BRIQUE";
  const uint16_t C[6] = { RGB565(255, 90, 90), RGB565(255, 160, 60), RGB565(255, 220, 70), RGB565(120, 230, 90), RGB565(90, 200, 255), RGB565(200, 120, 255) };
  const int Y = 56, SY = 168, R = 18;                   // ligne d'arrivee des lettres, centre du rond de chargement
  tft.fillScreen(COL_BG); tft.setTextSize(5);
  int adv[6], tot = 0;
  for (int i = 0; i < 6; i++) { char b[2] = { T[i], 0 }; adv[i] = textW(tft, 5, b); tot += adv[i] + 4; }
  int py[6] = { -99, -99, -99, -99, -99, -99 }, lastA = -1; bool sous = false;
  const uint32_t t0 = millis();
  for (uint32_t t = 0; t < 2400 && !(t > 300 && bootBtn()); t = millis() - t0) {
    int x = (SCREEN_W - tot) / 2;
    for (int i = 0; i < 6; i++) {                       // une lettre toutes les 120 ms : chute acceleree en 300 ms, puis petit rebond
      const float k = constrain(((int)t - i * 120) / 300.0f, 0.0f, 1.0f), b = ((int)t - i * 120 - 300) / 150.0f;
      int y = k < 1 ? -50 + (int)((Y + 50) * k * k) : (b < 1 ? Y - (int)(6 * sinf(b * 3.14159f)) : Y);
      if (k > 0 && y != py[i]) {
        tft.fillRect(x - 2, max(0, py[i] - 2), adv[i] + 6, 58, COL_BG);
        char s[2] = { T[i], 0 };
        if (y > -45) { tft.setTextSize(5); tft.setTextColor(RGB565(10, 8, 16)); ltextAt(x + 3, y + 3, s); tft.setTextColor(C[i]); ltextAt(x, y, s); }
        py[i] = y;
      }
      x += adv[i] + 4;
    }
    if (t > 1300 && !sous) { tft.setTextSize(1); tft.setTextColor(RGB565(150, 150, 170)); ctext(124, "la console que tu as soudee"); sous = true; }
    const int a = (t / 90) % 8;                         // rond de chargement : 8 points, le plus clair tourne
    if (a != lastA) {
      for (int k = 0; k < 8; k++) {
        const int d = (a - k + 8) % 8, cx = SCREEN_W / 2 + (int)(R * cosf(k * 0.7854f)), cy = SY + (int)(R * sinf(k * 0.7854f));
        tft.fillCircle(cx, cy, 4, COL_BG);
        tft.fillCircle(cx, cy, d == 0 ? 4 : 3, d == 0 ? COL_TXT : d < 3 ? RGB565(150, 150, 170) : RGB565(60, 60, 80));
      }
      lastA = a;
    }
    delay(16);
  }
  while (bootBtn()) delay(10);                          // le bouton qui a passe l'intro ne valide pas l'ecran suivant
}

void drawSplash() {
  tft.fillScreen(COL_BG);
  titleWiz = random(0, 6);                              // un sorcier different a chaque retour a l'ecran titre
  drawEvoInfo();

  // titre multicolore avec une legere ondulation lettre par lettre + ombre
  // portee -- plus accrocheur qu'un titre uni (retour).
  const char* title = "PIXEL RUN";
  uint16_t titleCols[8] = {
    tft.color565(255, 90, 90), tft.color565(255, 160, 60), tft.color565(255, 220, 70), COL_BG,
    tft.color565(120, 230, 90), tft.color565(90, 200, 255), tft.color565(150, 120, 255), tft.color565(255, 120, 200)
  };
  tft.setTextSize(4);
  int adv[12], totalW = 0, gap = 3;               // largeur de chaque lettre (police Chunky) pour centrer le titre
  for (int i = 0; title[i]; i++) { char bb[2] = { title[i], 0 }; adv[i] = (title[i] == ' ') ? 14 : textW(tft, 4, bb); totalW += adv[i] + gap; }
  int x = (SCREEN_W - totalW) / 2;
  int baseY = 6;
  for (int i = 0; title[i]; i++) {
    if (title[i] != ' ') {
      int wobble = (i % 2 == 0) ? 0 : -6;
      char buf[2] = { title[i], 0 };
      tft.setTextColor(tft.color565(10, 8, 16));   // ombre portee
      ltextAt(x + 2, baseY + wobble + 2, buf);
      tft.setTextColor(titleCols[i % 8]);
      ltextAt(x, baseY + wobble, buf);
    }
    x += adv[i] + gap;
  }

  tft.setTextColor(tft.color565(130, 140, 160)); tft.setTextSize(1);
  ctextf(204, "%s  -  appuie sur un bouton", FW_VERSION);

  introMs = millis(); introStep = -1;                   // intro : les 3 boutons apparaissent un par un pendant que la flamme fait le geste
  mascotNextMs = introMs + 3700; mascotAnimMs = 0; mascotMood = 0;   // puis la flamme (ou un glitch) prend la parole
}

// petite miniature du niveau (coin haut-droit du menu de selection). Regenere
// la grille via genLevel() -- deterministe et sans effet de bord : le vrai
// niveau sera regenere de toute facon par loadLevelData() quand on jouera.
// 'X' (pic invisible) n'est pas dessine, 'H' (trou invisible) rendu comme un
// bloc normal : la miniature ne spoile pas les niveaux "obstacles fantomes".
void drawLevelPreview(int n) {
  if (n < 20) {                                  // niveaux 1-20 : le vrai niveau (genere comme en jeu), a l'echelle dans le cadre
    const int px0 = 256, py0 = 18, BW = 40, BH = 60;
    genWorld(n);
    static const uint16_t GND[5] = { RGB565(226, 162, 32), RGB565(66, 170, 62), RGB565(236, 244, 252), RGB565(90, 80, 96), RGB565(96, 196, 72) };
    static const uint16_t SKY[5] = { RGB565(232, 207, 168), RGB565(30, 90, 56), RGB565(170, 210, 240), RGB565(48, 20, 24), RGB565(130, 200, 240) };
    int rows = DGY + 2; float sc = min(BW / (float)dWW, BH / (float)rows);
    int w = (int)(dWW * sc), h = (int)(rows * sc), ox = px0 + (BW - w) / 2, oy = py0 + (BH - h) / 2;
    tft.drawRect(px0 - 2, py0 - 2, BW + 4, BH + 4, tft.color565(80, 90, 110));
    tft.fillRect(px0, py0, BW, BH, COL_BG);
    for (int c = 0; c < dWW; c++) {
      int th = colTheme(c), x0 = ox + (int)(c * sc), x1 = ox + (int)((c + 1) * sc); if (x1 <= x0) x1 = x0 + 1;
      tft.fillRect(x0, oy, x1 - x0, h, th == 4 && dBuild < 3 ? 0 : SKY[th]);
      for (int r = 0; r < rows; r++) {
        char ch = dg[r][c]; uint16_t col;
        if (ch == 'S') col = th == 4 && dBuild < 2 ? RGB565(112, 120, 136) : GND[th];
        else if (ch == 'X') col = th == 4 && dBuild < 2 ? RGB565(150, 158, 172) : RGB565(218, 132, 76);
        else if (ch == 'L') col = RGB565(255, 110, 30);
        else if (ch == 'z') col = RGB565(255, 40, 200);
        else if (ch == '*' || ch == 'Y') col = RGB565(255, 230, 70);
        else if (ch == 'U') col = RGB565(70, 150, 255);   // la fausse etoile ressemble a une vraie
        else if (ch == 'G') col = RGB565(230, 50, 50);
        else if (ch == 'k' || ch == 'c' || ch == 'm' || ch == 'v') col = RGB565(40, 40, 50);
        else if (ch == 'T') col = th == 3 ? RGB565(255, 150, 40) : th == 2 ? RGB565(40, 110, 86) : RGB565(52, 128, 40);
        else continue;                               // pics caches et briques qui tombent : pas montres (il faut les decouvrir)
        int y0 = oy + (int)(r * sc), y1 = oy + (int)((r + 1) * sc); if (y1 <= y0) y1 = y0 + 1;
        tft.fillRect(x0, y0, x1 - x0, y1 - y0, col);
      }
    }
    tft.setTextSize(1); tft.setTextColor(COL_TXT); ltextAt(px0 + 2, py0 + 64, n % 10 == 9 ? "PASSAGE" : n < 10 ? "DEHORS" : "DONJON");
    return;
  }
  if (n < 50) {                                  // mondes larges (1-50) : pas de miniature, une vignette par monde
    const int px0 = 256, py0 = 18; int th = n < 10 ? 4 : n < 20 ? 3 : n < 30 ? 2 : n < 40 ? 0 : 1;
    static const char* const nm[5] = { "DESERT", "JUNGLE", "GLACE", "DONJON", "DEHORS" };
    uint16_t bg[5] = { tft.color565(232, 207, 168), tft.color565(30, 90, 56), tft.color565(170, 210, 240), tft.color565(48, 20, 24), tft.color565(130, 200, 240) };
    tft.drawRect(px0 - 2, py0 - 2, 44, 64, tft.color565(80, 90, 110));
    tft.fillRect(px0, py0, 40, 60, bg[th]);
    if (th == 0) { tft.fillCircle(px0 + 28, py0 + 12, 7, tft.color565(255, 238, 150)); tft.fillRect(px0, py0 + 48, 40, 12, tft.color565(226, 162, 32)); tft.fillRect(px0 + 8, py0 + 30, 5, 18, tft.color565(62, 142, 58)); }
    else if (th == 1) { tft.drawLine(px0 + 12, py0, px0 + 12, py0 + 36, tft.color565(120, 190, 80)); tft.fillCircle(px0 + 12, py0 + 38, 3, tft.color565(255, 205, 60)); tft.fillTriangle(px0 + 20, py0 + 52, px0 + 36, py0 + 30, px0 + 38, py0 + 52, tft.color565(70, 190, 90)); tft.fillRect(px0, py0 + 54, 40, 6, tft.color565(92, 62, 36)); }
    else if (th == 2) { tft.fillTriangle(px0 + 4, py0 + 52, px0 + 20, py0 + 20, px0 + 36, py0 + 52, tft.color565(240, 247, 255)); tft.fillRect(px0, py0 + 52, 40, 8, tft.color565(150, 200, 236)); }
    else if (th == 3) { tft.fillRect(px0, py0 + 50, 40, 10, tft.color565(235, 90, 20)); tft.fillTriangle(px0 + 14, py0 + 50, px0 + 26, py0 + 50, px0 + 20, py0 + 26, tft.color565(255, 170, 40)); tft.fillRect(px0 + 4, py0 + 8, 8, 14, tft.color565(20, 8, 12)); }
    else { tft.fillCircle(px0 + 14, py0 + 34, 12, tft.color565(70, 160, 70)); tft.fillRect(px0 + 12, py0 + 40, 4, 12, tft.color565(130, 78, 54)); tft.fillRect(px0, py0 + 52, 40, 8, tft.color565(96, 196, 72)); }
    tft.setTextSize(1); tft.setTextColor(COL_TXT); ltextAt(px0 + 2, py0 + 64, nm[th]);
    return;
  }
  setLevelPalette(n);   // meme mecanisme que genLevel() : temporaire, sans effet sur le niveau reellement charge
  genLevel(n);
  const int cell = 2, px0 = 256, py0 = 18;
  tft.drawRect(px0 - 2, py0 - 2, COLS * cell + 4, 30 * cell + 4, tft.color565(80, 90, 110));
  tft.fillRect(px0, py0, COLS * cell, 30 * cell, COL_LVLBG);
  for (int r = 0; r < levH; r++) {
    for (int c = 0; c < COLS; c++) {
      char ch = grid[r][c];
      uint16_t col;
      if (ch == '#' || ch == 'H') col = COL_LVLBLK;
      else if (ch == 'C')         col = COL_FRAG;
      else if (ch == 'I')         col = tft.color565(170, 220, 250);
      else if (ch == 'B')         col = COL_STARD;
      else if (ch == 'R')         col = COL_BOUNCE;
      else if (ch == 'O')         col = tft.color565(255, 205, 60);
      else if (ch == '^')         col = COL_SPK;
      else if (ch == 'S')         col = COL_GOAL;
      else if (ch == 'G')         col = COL_STAR;
      else continue;
      // niveaux plus hauts que 30 lignes (dizaine 11-20) : ecrases verticalement dans le cadre
      int y0 = py0 + (levH > 30 ? r * 60 / levH : r * cell);
      int y1 = py0 + (levH > 30 ? (r + 1) * 60 / levH : (r + 1) * cell);
      tft.fillRect(px0 + c * cell, y0, cell, max(1, y1 - y0), col);
    }
  }
}

void drawSelect() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_TXT); tft.setTextSize(2);
  ctext(16, "CHOISIR NIVEAU");
  drawLevelPreview(selLevel);

  // niveau selectionne (gros)
  tft.setTextColor(COL_MEAT); tft.setTextSize(6);
  ctextf(70, "%d", selLevel + 1);

  // fleches
  tft.setTextColor(COL_TXT); tft.setTextSize(3);
  ltextAt(14, 84, "<");
  rtextAt(306, 84, ">");

  // etoiles best
  drawStars3(122, 134, bestStars[selLevel], 10);

  // etat : reussi (avec meilleur temps) ou nouveau a debloquer
  bool done = bestTime[selLevel] > 0;
  if (done) {
    tft.setTextSize(2); tft.setTextColor(COL_GOAL); ctext(158, "REUSSI");
    tft.setTextSize(1); tft.setTextColor(COL_TXT);
    ctextf(176, "%u.%us", (unsigned)(bestTime[selLevel] / 1000), (unsigned)((bestTime[selLevel] / 100) % 10));
  } else if (gateBlocks(selLevel)) {
    tft.setTextSize(1); tft.setTextColor(COL_STAR); ctext(164, "MINI-JEU BONUS D'ABORD");
  } else {
    tft.setTextSize(2); tft.setTextColor(COL_STAR); ctext(158, "NOUVEAU");
  }

  tft.setTextColor(COL_TXT); tft.setTextSize(1);
  ctext(190, "P2/P1 = changer   P3 = jouer");

  // stats globales (futur classement)
  tft.setTextColor(COL_STAR);
  ctextf(204, "Score: %u   Best combo: %d", (unsigned)totalScore, bestCombo);
  tft.setTextColor(tft.color565(150,160,180));
  ctextf(218, "Reussis: %u   Morts: %u   Jeu: %us", (unsigned)totalWins, (unsigned)totalDeaths, (unsigned)totalPlaytime);
}

void drawEnd() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_GOAL); tft.setTextSize(3);
  if (bqOn()) ctext(12, bqTitre[0] ? bqTitre : "BRAVO !"); else ctextf(12, "NIVEAU %d", curLevel + 1);
  drawStars3(118, 46, lastStars, 11);

  tft.setTextSize(1);
  tft.setTextColor(lastBestTime ? COL_STAR : COL_TXT);
  ctextf(78, "Temps: %d.%ds%s", lastTimeMs / 1000, (lastTimeMs / 100) % 10, lastBestTime ? "  RECORD!" : "");
  tft.setTextColor(lastCombo > 1 ? COL_STAR : COL_TXT);
  if (lastCombo > 1) ctextf(92, "Morts: %u   COMBO x%d", deaths, lastCombo);
  else               ctextf(92, "Morts: %u   Combo: 0", deaths);
  tft.setTextColor(COL_TXT);
  ctextf(106, "Score +%d", lastScore);
  // globaux
  tft.setTextColor(tft.color565(150,160,180));
  if (bqOn() && bqMsg[0]) { tft.setTextColor(COL_STAR); ctext(122, bqMsg); }   // BRIQUE : message de victoire choisi sur la page
  else ctextf(122, "Total: %u pts   Best combo: %d", (unsigned)totalScore, bestCombo);

  // grille 2x2 : Continuer/Rejouer (haut), Niveaux/Accueil (bas, "Accueil" = rechoisir 1/2 joueurs)
  const char* opt[4] = { "Continuer", "Rejouer", "Niveaux", "Accueil" };
  tft.setTextSize(2);
  for (int i = 0; i < 4; i++) {
    int cx = 86 + (i % 2) * 148, y = (i < 2) ? 146 : 180;
    if (i == endChoice) { tft.fillRoundRect(cx - 70, y - 4, 140, 26, 4, COL_MEAT); tft.setTextColor(COL_BG); }
    else tft.setTextColor(COL_TXT);
    ctextAt(cx, y, opt[i]);
  }
  tft.setTextColor(tft.color565(120,130,150)); tft.setTextSize(1);
  ctext(214, "P2/P1 = choisir   P3 = valider");
}

void drawDead() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_MEAT); tft.setTextSize(5);
  ctext(46, "PERDU");
  tft.setTextColor(COL_TXT); tft.setTextSize(1);
  if (bqTimeout) { tft.setTextColor(COL_STAR); tft.setTextSize(2); ctext(108, "TEMPS ECOULE !"); tft.setTextSize(1); }
  else if (bqOn()) ctext(110, bqTitre[0] ? bqTitre : "Mon jeu");
  else ctextf(110, "Niveau %d", curLevel + 1);
  const char* opt[2] = { "Rejouer", "Niveaux" };
  tft.setTextSize(2);
  int y = 160;
  for (int i = 0; i < 2; i++) {
    int cx = 90 + i * 140;
    if (i == deadChoice) { tft.fillRoundRect(cx - 65, y - 6, 130, 30, 5, COL_MEAT); tft.setTextColor(tft.color565(34, 10, 14)); }
    else tft.setTextColor(COL_TXT);
    ctextAt(cx, y, opt[i]);
  }
  tft.setTextColor(tft.color565(150, 120, 130)); tft.setTextSize(1);
  ctext(214, "P2/P1 = choisir   P3 = valider");
}

void drawHome() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_MEAT); tft.setTextSize(4);
  ctext(28, "PIXEL RUN");
  // mon avatar : il reprend la progression (palier de couleur, skin, accessoires de savoir) -- jamais utilise dans les jeux Brique (bqOn)
  tft.fillRoundRect(8, 74, 96, 128, 8, RGB565(26, 22, 34)); tft.drawRoundRect(8, 74, 96, 128, 8, RGB565(70, 70, 90));
  quizShown = knowledge; quizAvatar(56, 128, false, 1);
  int etoiles = 0; for (int i = 0; i < NUM_LEVELS; i++) etoiles += bestStars[i];
  char ln[16]; tft.setTextSize(1);
  tft.setTextColor(COL_TXT);  ctextAt(56, 140, playerName[0] ? playerName : "Joueur");
  tft.setTextColor(COL_STAR); ctextAt(56, 154, KN_NAME[knowLvl(knowledge)]);
  tft.setTextColor(RGB565(170, 170, 190));
  snprintf(ln, sizeof ln, "Niveau %d", maxUnlockedIdx() + 1); ctextAt(56, 168, ln);
  snprintf(ln, sizeof ln, "%d etoiles", etoiles); ctextAt(56, 180, ln);
  snprintf(ln, sizeof ln, "%lu pieces", (unsigned long)coins); ctextAt(56, 192, ln);
  const char* opt[3] = { "1 Joueur", "2 a 4 Joueurs", "Mes niveaux" };
  tft.setTextSize(2);
  for (int i = 0; i < 3; i++) {
    int yy = 90 + i * 40;
    if (i == homeChoice) { tft.fillRoundRect(114, yy - 9, 198, 32, 6, COL_MEAT); tft.setTextColor(COL_BG); }
    else tft.setTextColor(COL_TXT);
    ctextAt(213, yy, opt[i]);
  }
  tft.setTextColor(tft.color565(120, 130, 150)); tft.setTextSize(1);
  ctext(208, FW_VERSION);
  ctext(229, "P2/P1 = choisir   P3 = valider");
  if (debugUnlockAll) { tft.setTextColor(COL_STAR); ctext(8, "DEBUG : tous niveaux"); }
}

void drawLobby() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_TXT); tft.setTextSize(3);
  ctext(18, "MULTIJOUEUR");
  const uint32_t now = millis(); int nP = 0;
  for (int k = 0; k < mpN; k++) { const uint32_t r = mpRx[k]; if (mpFrozen ? (mpIn[k] && mpOn[k]) : (int32_t)(now - r) < 1500) nP++; }
  tft.setTextSize(2);
  if (nP == 0) {
    tft.setTextColor(COL_STAR); ctext(90, "Recherche...");
    tft.setTextColor(COL_TXT); tft.setTextSize(1);
    ctext(140, "Lance 'Multijoueur' sur les autres consoles (4 au plus)");
    ctext(210, "P3 = annuler");
    return;
  }
  if (mpFrozen) {                                          // partie lancee : ma couleur, en grand, et celles des autres
    tft.fillCircle(SCREEN_W / 2, 66, 16, MP_COL[myCol & 3]); tft.drawCircle(SCREEN_W / 2, 66, 17, COL_TXT);
    tft.setTextColor(MP_COL[myCol & 3]); ctextf(90, "Tu es %s", MP_NAME[myCol & 3]);
    int i = 0; for (int k = 0; k < mpN; k++) if (mpIn[k] && mpOn[k]) { tft.fillCircle(SCREEN_W / 2 - 40 + i * 40, 122, 7, MP_COL[mpCol[k] & 3]); i++; }
  } else {
    for (int i = 0; i <= nP && i < 4; i++) { const int cx = SCREEN_W / 2 + (i * 2 - nP) * 30; tft.fillCircle(cx, 72, 12, COL_TXT); tft.drawCircle(cx, 72, 13, COL_MEAT); }   // un rond par joueur (couleurs donnees au depart)
    tft.setTextColor(COL_GOAL); ctextf(98, "%d joueurs", nP + 1);
  }
  tft.setTextColor(COL_TXT);
  { const uint32_t ss = vSessionMs; if (!mpFrozen && ss && (int32_t)(now - ss) < 3000) { tft.setTextColor(COL_STAR); ctext(138, "Partie en cours : connexion..."); return; } }
  if (mpFrozen) ctext(146, isHost ? "Tu choisis le jeu" : "L'hote choisit le jeu...");
  else if (isHost) { ctext(134, "Tu es l'hote"); tft.setTextColor(COL_STAR); ctext(166, "P3 = commencer"); tft.setTextSize(1); tft.setTextColor(COL_TXT); ctext(196, "(d'autres consoles peuvent encore rejoindre)"); }
  else ctext(138, "Attente de l'hote...");
}

LbEnt lbSelf() {                                       // ma ligne du classement, avec la version incrementee si nom/avatar ont change
  LbEnt e = {}; e.id = myId(); e.score = totalScore;
  memset(e.name, ' ', 6); memcpy(e.name, playerName, min((int)strlen(playerName), 6));
  e.skin = skinSel; e.lv = knowLvl(knowledge);
  for (int i = 0; i < lbN; i++) if (lbTab[i].id == e.id) {
    e.rev = lbTab[i].rev;
    if (memcmp(lbTab[i].name, e.name, 6) || lbTab[i].skin != e.skin || lbTab[i].lv != e.lv) e.rev++;
  }
  return e;
}
void lbAvatar(int cx, int feet, int skin, int lv) {    // mini avatar d'un joueur : son skin (forme + couleur) et ses accessoires de savoir
  if (skin >= NSKIN) skin = 0;
  const int w = 12, h = 14; int x0 = cx - w / 2, y0 = feet - h;
  uint16_t c = skin ? skinColor(skin) : COL_MEAT, d = skin ? SKIN_COLD[skin] : COL_MEATD;
  skinBody(tft, skin, x0, y0, w, h, 3, c, d);
  tft.fillRect(x0 + 3, y0 + 4, 2, 3, d); tft.fillRect(x0 + 7, y0 + 4, 2, 3, d);
  tft.drawFastHLine(x0 + 4, y0 + 10, 4, d);
  if (lv >= 1) tft.drawFastHLine(x0 + 1, y0 + 5, w - 2, COL_TXT);                     // lunettes
  if (lv >= 3) tft.fillRect(x0 - 1, y0 - 2, w + 2, 2, RGB565(15, 15, 25));            // toque
  if (lv >= 4) tft.fillCircle(cx, y0 - 5, 1, COL_STAR);                               // etoile du genie
}
void drawName() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_STAR); tft.setTextSize(3);
  ctext(16, "TON NOM");
  for (int i = 0; i < 6; i++) {
    int x = 37 + i * 42;
    tft.fillRoundRect(x, 70, 36, 50, 6, i == namePos ? COL_MEAT : COL_TXT);
    char b[2] = { NAME_SET[nameIdx[i]], 0 };
    tft.setTextSize(4); tft.setTextColor(COL_BG); ctextAt(x + 18, 82, b);
  }
  tft.setTextSize(1); tft.setTextColor(tft.color565(150, 160, 180));
  ctext(140, "P2 / P1 = changer la lettre (maintenu = defile)");
  ctext(154, "P3 = lettre suivante");
  ctext(172, "< = revenir en arriere     > = valider maintenant");
  lbAvatar(160, 214, skinSel, knowLvl(knowledge));
}
void drawLb() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_STAR); tft.setTextSize(3);
  ctext(6, "CLASSEMENT");
  for (int i = 0; i < lbN && i < 8; i++) {
    int y = 40 + i * 21;
    bool me = lbTab[i].id == myId();
    if (me) tft.fillRoundRect(8, y - 3, 304, 20, 4, RGB565(70, 70, 100));
    tft.setTextSize(2);
    tft.setTextColor(i == 0 ? COL_STAR : COL_TXT);
    ltextf(14, y, "%d", i + 1);
    lbAvatar(58, y + 16, lbTab[i].skin, lbTab[i].lv);
    char nm[7]; memcpy(nm, lbTab[i].name, 6); nm[6] = 0;
    for (int k = 5; k >= 0 && nm[k] == ' '; k--) nm[k] = 0;
    if (!nm[0]) snprintf(nm, sizeof(nm), "C3%04X", lbTab[i].id);
    ltextAt(76, y, nm);
    char sc[12]; snprintf(sc, sizeof(sc), "%lu", (unsigned long)lbTab[i].score);
    rtextAt(306, y, sc);
  }
  tft.setTextSize(1); tft.setTextColor(tft.color565(120, 130, 150));
  ctextf(210, "Partage actif : %d console(s)", lbN);
  ctext(224, "P2 = retour     P3 = changer mon nom");
}
void lbStart() {
  startEspNow();
  if (!playerName[0]) snprintf(playerName, sizeof(playerName), "C3%04X", myId());   // nom par defaut
  lbMerge(lbSelf());
  lbSentMs = 0; lbDirty = false; state = ST_LB; drawLb();
}
void lbTick() {
  if (vLbRx) { for (int i = 0; i < lbRx.n; i++) lbMerge(lbRx.e[i]); vLbRx = false; }
  unsigned long now = millis();
  if (now - lbSentMs > 1500) {                   // diffuse toute la table (gossip)
    lbSentMs = now; lbMerge(lbSelf());
    LbMsg m; m.type = M_LB; m.n = lbN; memcpy(m.e, lbTab, lbN * sizeof(LbEnt));
    esp_now_send(BCAST, (uint8_t *)&m, 2 + lbN * sizeof(LbEnt));
  }
  if (lbDirty) { lbDirty = false; drawLb(); }
  if (lbSave && now - lbSavedMs > 8000) lbSaveNvs();   // limite l'usure de la flash
}
// menu "Que jouer ?" : 11 vignettes (4 x 3) avec une icone ; finit toujours par Boutique et Classement. En 2 joueurs : Niveaux et Pong.
void drawModeIcon(int i, int cx, int cy) {
  const uint16_t G = RGB565(255, 205, 60), W = COL_TXT;
  switch (i) {
    case 0: tft.fillRect(cx - 18, cy + 8, 14, 6, RGB565(96, 196, 72)); tft.fillRect(cx + 2, cy - 2, 16, 6, RGB565(96, 196, 72));   // niveaux : plateformes + flamme
      tft.fillTriangle(cx - 16, cy + 8, cx - 6, cy + 8, cx - 11, cy - 8, RGB565(40, 100, 230)); tft.fillTriangle(cx - 13, cy + 8, cx - 9, cy + 8, cx - 11, cy - 1, RGB565(150, 210, 255));
      tft.fillCircle(cx + 10, cy - 12, 4, G); break;
    case 1: tft.fillRect(cx - 18, cy - 12, 4, 16, W); tft.fillRect(cx + 14, cy - 4, 4, 16, W); tft.fillCircle(cx + 2, cy - 2, 3, G);   // pong
      for (int y = -14; y < 16; y += 6) tft.drawFastVLine(cx, cy + y, 3, RGB565(120, 130, 150)); break;
    case 2: for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) tft.fillRect(cx - 18 + c * 9, cy - 14 + r * 6, 8, 5, r == 0 ? RGB565(230, 70, 70) : r == 1 ? RGB565(240, 180, 60) : RGB565(90, 200, 120));   // casse-brique
      tft.fillCircle(cx + 4, cy + 6, 2, W); tft.fillRect(cx - 8, cy + 12, 16, 3, W); break;
    case 3: { static const char *A[6] = { "..X....X..", "...X..X...", "..XXXXXX..", ".XX.XX.XX.", "XXXXXXXXXX", "X.X....X.X" };   // invaders
      for (int r = 0; r < 6; r++) for (int c = 0; c < 10; c++) if (A[r][c] == 'X') tft.fillRect(cx - 20 + c * 4, cy - 12 + r * 4, 4, 4, RGB565(120, 230, 90)); } break;
    case 4: for (int k = 0; k < 14; k++) { float a = k * 0.449f; int rr = 13 + (k % 3) * 4;                                        // echo : onde de rayons, une sortie jaune, pas blancs et rouges
        tft.drawLine(cx - 8 + (int)(cosf(a) * 4), cy + (int)(sinf(a) * 4), cx - 8 + (int)(cosf(a) * rr), cy + (int)(sinf(a) * rr), k == 2 ? RGB565(255, 220, 40) : RGB565(110 + (k % 3) * 50, 110 + (k % 3) * 50, 110 + (k % 3) * 50)); }
      tft.fillCircle(cx - 8, cy, 2, W); tft.fillCircle(cx + 14, cy + 12, 2, RGB565(220, 40, 40)); tft.fillCircle(cx + 10, cy + 13, 1, RGB565(220, 40, 40)); break;
    case 5: pDraw(tft, P_IDLE, 0, cx, cy + 18, true, 2); break;                                                                        // samourai
    case 6: tft.fillCircle(cx + 9, cy - 6, 10, RGB565(255, 200, 170)); tft.fillTriangle(cx - 26, cy - 4, cx + 26, cy + 18, cx - 26, cy + 18, RGB565(90, 40, 70));   // alto : soleil couchant, pente, glisseur
      tft.drawLine(cx - 13, cy + 1, cx - 1, cy + 6, RGB565(30, 16, 30)); tft.drawLine(cx - 7, cy + 3, cx - 6, cy - 6, RGB565(30, 16, 30)); tft.fillCircle(cx - 6, cy - 8, 2, RGB565(30, 16, 30)); break;
    case 7: tft.drawCircle(cx, cy + 2, 15, W); tft.drawCircle(cx, cy + 2, 14, W); tft.fillRect(cx - 3, cy - 17, 6, 4, W);   // timing : chronometre
      tft.drawLine(cx, cy + 2, cx, cy - 9, RGB565(255, 200, 40)); tft.drawLine(cx, cy + 2, cx + 7, cy + 6, W); tft.fillCircle(cx, cy + 2, 2, RGB565(255, 200, 40)); break;
    case 8: { static const int8_t P[7][2] = { { 0, 0 }, { 0, 15 }, { 4, 11 }, { 7, 17 }, { 9, 16 }, { 6, 10 }, { 11, 10 } };   // drift : pointeur + bords de piste
      tft.drawLine(cx - 22, cy + 14, cx - 4, cy - 14, RGB565(80, 230, 255)); tft.drawLine(cx + 2, cy + 16, cx + 22, cy - 12, RGB565(80, 230, 255));
      for (int i = 0; i < 7; i++) tft.drawLine(cx - 5 + P[i][0], cy - 8 + P[i][1], cx - 5 + P[(i + 1) % 7][0], cy - 8 + P[(i + 1) % 7][1], W);
      int X[7], Y[7]; for (int i = 0; i < 7; i++) { X[i] = cx - 5 + P[i][0]; Y[i] = cy - 8 + P[i][1]; }
      tft.fillTriangle(X[0], Y[0], X[1], Y[1], X[2], Y[2], W); tft.fillTriangle(X[0], Y[0], X[2], Y[2], X[5], Y[5], W); tft.fillTriangle(X[2], Y[2], X[3], Y[3], X[4], Y[4], W); tft.fillTriangle(X[2], Y[2], X[4], Y[4], X[5], Y[5], W); tft.fillTriangle(X[0], Y[0], X[5], Y[5], X[6], Y[6], W); } break;
    case 9: tft.fillRoundRect(cx - 14, cy - 6, 28, 22, 5, RGB565(150, 100, 60)); tft.drawRoundRect(cx - 8, cy - 14, 16, 12, 5, RGB565(150, 100, 60));   // boutique : sac + piece
      tft.fillCircle(cx + 12, cy + 12, 7, G); tft.drawCircle(cx + 12, cy + 12, 7, RGB565(190, 130, 20)); tft.drawFastVLine(cx + 12, cy + 8, 9, RGB565(190, 130, 20)); break;
    case 11: tft.fillRect(cx - 2, cy - 18, 4, 26, W); tft.fillRect(cx - 9, cy + 6, 18, 4, G); tft.fillRect(cx - 2, cy + 10, 4, 8, RGB565(150, 100, 60));   // smash : epee
      tft.fillRoundRect(cx + 6, cy - 6, 14, 18, 4, RGB565(60, 120, 230)); tft.drawRoundRect(cx + 6, cy - 6, 14, 18, 4, G); break;   // + bouclier
    case 12: for (int r = 0; r < 3; r++) for (int c = 0; c < 3 - (r & 1); c++) tft.fillRect(cx - 18 + c * 13 + (r & 1) * 6, cy - 12 + r * 9, 11, 7, r == 1 ? G : RGB565(167, 139, 250));   // mes jeux : briques
      break;
    default: tft.fillRoundRect(cx - 11, cy - 16, 22, 16, 4, G); tft.fillRect(cx - 2, cy, 4, 8, G); tft.fillRect(cx - 9, cy + 8, 18, 5, G);   // classement : coupe
      tft.drawCircle(cx - 13, cy - 10, 4, G); tft.drawCircle(cx + 13, cy - 10, 4, G); tft.fillCircle(cx, cy - 9, 3, RGB565(255, 245, 200));
  }
}
int modeCount() { return !twoP ? 12 : mpPlayers() > 2 ? 4 : 5; }
int modeAt(int slot) {                                     // multijoueur : Niveaux, Pong (a deux seulement), Timing, Drift
  static const int8_t M2[5] = { 0, 1, 7, 8, 12 }, M3[4] = { 0, 7, 8, 12 };   // 12 = Mes jeux (Brique)
  return !twoP ? slot : mpPlayers() > 2 ? M3[slot] : M2[slot];
}
void drawModeTile(int slot, bool sel) {
  const int i = modeAt(slot);
  static const char *const L[13] = { "Niveaux", "Pong", "Briques", "Invaders", "Echo", "Samourai", "Alto", "Timing", "Drift", "Boutique", "Scores", "Smash", "Mes jeux" };
  int x = 8 + (slot % 4) * 77, y = 44 + (slot / 4) * 61, w = 73, h = 57;
  tft.fillRoundRect(x, y, w, h, 8, sel ? RGB565(70, 44, 52) : RGB565(26, 22, 34));
  tft.drawRoundRect(x, y, w, h, 8, sel ? COL_MEAT : RGB565(70, 70, 90));
  if (sel) { tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 7, COL_MEAT); tft.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 6, COL_MEAT); }
  drawModeIcon(i, x + w / 2, y + 24);
  tft.setTextSize(1); tft.setTextColor(sel ? COL_TXT : RGB565(170, 170, 190)); ctextAt(x + w / 2, y + h - 14, L[i]);
}
void drawMode() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_TXT); tft.setTextSize(3);
  ctext(12, "QUE JOUER ?");
  int n = modeCount();
  if (modeChoice >= n) modeChoice = 0;
  for (int i = 0; i < n; i++) drawModeTile(i, i == modeChoice);
  tft.setTextColor(tft.color565(120, 130, 150)); tft.setTextSize(1);
  ctext(229, "P2/P1 = choisir   P3 = valider");
}

// boutique : carrousel (P2/P1 = changer d'article, P3 = acheter/equiper) -- palier de
// vitesse, puis les skins, puis Retour. Tout s'achete avec les pieces (tuile 'O' / ennemis).
// icone d'un article de la boutique, centree en (cx, cy), r = rayon approximatif
// item 0 = eclair (vitesse), 1..NSKIN = personnage dans le skin, NSKIN+1 = fleche retour
void drawShopIcon(int cx, int cy, int item, int r) {
  if (item == 0) {
    tft.fillTriangle(cx + r / 3, cy - r, cx - r * 2 / 3, cy + r / 6, cx + r / 6, cy + r / 6, COL_STAR);
    tft.fillTriangle(cx - r / 3, cy + r, cx + r * 2 / 3, cy - r / 6, cx - r / 6, cy - r / 6, COL_STAR);
    tft.drawLine(cx + r / 3, cy - r, cx - r * 2 / 3, cy + r / 6, COL_STARD);
    tft.drawLine(cx - r / 3, cy + r, cx + r * 2 / 3, cy - r / 6, COL_STARD);
  } else if (item == SHOP_BACK) {
    tft.fillTriangle(cx - r, cy, cx - r / 6, cy - r * 2 / 3, cx - r / 6, cy + r * 2 / 3, COL_TXT);
    tft.fillRect(cx - r / 6, cy - r / 4, r * 5 / 4, r / 2, COL_TXT);
  } else if (item <= AB_N) {                         // capacites : chevrons / double fleche / ailes
    if (item == 1) {
      for (int j = 0; j < 2; j++) tft.fillTriangle(cx - r + j * r, cy - r, cx - r + j * r, cy + r, cx + j * r, cy, COL_STAR);
    } else if (item == 2) {
      for (int j = 0; j < 2; j++) tft.fillTriangle(cx, cy - r + j * r, cx - r * 2 / 3, cy + j * r, cx + r * 2 / 3, cy + j * r, COL_GOAL);
    } else {
      tft.fillTriangle(cx - r, cy - r / 2, cx - r / 5, cy - r / 6, cx - r / 5, cy + r * 2 / 3, COL_TXT);
      tft.fillTriangle(cx + r, cy - r / 2, cx + r / 5, cy - r / 6, cx + r / 5, cy + r * 2 / 3, COL_TXT);
      tft.fillRect(cx - r / 6, cy - r / 4, r / 3 + 1, r, COL_MEAT);
    }
  } else if (item > TR0) {                           // trainee : 3 carres qui s'estompent
    int k = item - TR0;
    uint16_t c = (k == NTRAIL - 1) ? skinColor(7) : TRAIL_COL[k];
    for (int j = 0; j < 3; j++) { int sz = max(2, r / 2 - j * 2); tft.fillRect(cx + r - j * (r * 2 / 3) - sz, cy - sz / 2, sz, sz, c); }
  } else {
    int k = item - SK0;
    uint16_t c = k ? skinColor(k) : COL_MEAT, d = k ? SKIN_COLD[k] : COL_MEATD;
    int w = r * 3 / 2 + 2, h = r * 2 - 2, rd = max(2, r / 3);
    skinBody(tft, k, cx - w / 2, cy - h / 2, w, h, rd, c, d);
    int ew = max(2, r / 4), eh = max(3, r / 2);
    tft.fillRect(cx - w / 4 - ew / 2, cy - h / 6, ew, eh, d);
    tft.fillRect(cx + w / 4 - ew / 2, cy - h / 6, ew, eh, d);
  }
}

// boutique : bandeau d'icones (tous les articles) + grande icone de l'article choisi.
// P2/P1 = changer d'article, P3 = acheter / equiper / quitter.
void drawShop() {
  const uint16_t dim = tft.color565(150, 160, 180), bad = tft.color565(255, 90, 90);
  const int N = SHOP_N;
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  // un seul article par page, en grand : P2/P1 = article precedent / suivant
  tft.setTextSize(2); tft.setTextColor(COL_STAR); ltextAt(10, 4, "BOUTIQUE");
  { char b[24]; snprintf(b, sizeof(b), "%u pieces", (unsigned)coins); tft.setTextColor(COL_TXT); rtextAt(SCREEN_W - 10, 4, b); }

  const char* cat = shopSel == 0 ? "AMELIORATION" : shopSel <= AB_N ? "CAPACITE TEMPORAIRE" : shopSel <= TR0 ? "SKIN (forme + couleur)" : shopSel < N - 1 ? "TRAINEE" : "";
  tft.setTextSize(1); tft.setTextColor(dim);
  ctextf(30, "%s   %d / %d", cat, shopSel + 1, N);

  tft.fillTriangle(32, 98, 32, 126, 12, 112, COL_TXT);                   // fleches : article precedent / suivant
  tft.fillTriangle(288, 98, 288, 126, 308, 112, COL_TXT);
  drawShopIcon(160, 112, shopSel, 38);

  tft.setTextSize(3); tft.setTextColor(COL_TXT);
  if (shopSel == 0)            ctext(156, "Vitesse");
  else if (shopSel == N - 1)   ctext(156, "Retour");
  else if (shopSel <= AB_N)    ctext(156, AB_NAME[shopSel - 1]);
  else if (shopSel > TR0)      ctext(156, TRAIL_NAME[shopSel - TR0]);
  else                         ctext(156, SKIN_NAME[shopSel - SK0]);

  if (shopSel == 0) {
    tft.setTextSize(2); tft.setTextColor(COL_TXT); ctextf(188, "Palier %d / %d", (int)speedBoostLv, SPEED_BOOST_MAX);
    tft.setTextSize(1); tft.setTextColor(speedBoostLv >= SPEED_BOOST_MAX ? COL_GOAL : coins >= SPEED_BOOST_COST ? COL_STAR : bad);
    if (speedBoostLv >= SPEED_BOOST_MAX) ctext(212, "Palier max atteint !");
    else ctextf(212, "Prochain : %d pieces - P3 = acheter", SPEED_BOOST_COST);
  } else if (shopSel == N - 1) {
    tft.setTextSize(2); tft.setTextColor(dim); ctext(188, "P3 = quitter");
  } else if (shopSel <= AB_N) {
    int a = shopSel - 1, t = (int)abilT[a];
    tft.setTextSize(2);
    if (t > 0) { tft.setTextColor(COL_GOAL); ctextf(188, "Reste %d:%02d", t / 60, t % 60); }
    else       { tft.setTextColor(dim);      ctext(188, "Inactif"); }
    tft.setTextSize(1); tft.setTextColor(dim); ctext(208, AB_DESC[a]);
    tft.setTextColor(coins >= AB_COST[a] ? COL_STAR : bad);
    ctextf(218, "+5 min : %d pieces - P3 = acheter", (int)AB_COST[a]);
  } else if (shopSel > TR0) {
    int k = shopSel - TR0;
    tft.setTextSize(2);
    if (trailSel == k)               { tft.setTextColor(COL_GOAL); ctext(188, "EQUIPEE"); }
    else if (trailOwned & (1u << k)) { tft.setTextColor(COL_TXT);  ctext(188, "Possedee"); tft.setTextSize(1); tft.setTextColor(dim); ctext(212, "P3 = equiper"); }
    else {
      tft.setTextColor(coins >= TRAIL_COST[k] ? COL_STAR : bad); ctextf(188, "Prix : %d pieces", (int)TRAIL_COST[k]);
      tft.setTextSize(1); ctext(212, "P3 = acheter");
    }
  } else {
    int k = shopSel - SK0;
    tft.setTextSize(2);
    if (skinSel == k)               { tft.setTextColor(COL_GOAL); ctext(188, "EQUIPE"); }
    else if (skinOwned & (1u << k)) { tft.setTextColor(COL_TXT);  ctext(188, "Possede"); tft.setTextSize(1); tft.setTextColor(dim); ctext(212, "P3 = equiper"); }
    else {
      tft.setTextColor(coins >= SKIN_COST[k] ? COL_STAR : bad); ctextf(188, "Prix : %d pieces", (int)SKIN_COST[k]);
      tft.setTextSize(1); ctext(212, "P3 = acheter");
    }
  }
  tft.setTextSize(1); tft.setTextColor(dim);
  ctext(229, "P2 / P1 = parcourir     P3 = valider");
}
// jeux, avance multi-jeux") -- une dizaine de niveaux finie, un mini-jeu en
// rotation Pong/Casse-brique/Invaders avant de debloquer la suivante.
void drawCheckpointIntro() {
  const char* names[3] = { "PONG", "CASSE-BRIQUE", "INVADERS" };
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_GOAL); tft.setTextSize(3);
  ctext(32, "DIZAINE"); ctext(62, "TERMINEE !");
  tft.setTextColor(COL_TXT); tft.setTextSize(2);
  ctext(106, "Mini-jeu bonus :");
  tft.setTextColor(COL_STAR); tft.setTextSize(3);
  ctext(132, names[checkpointDecade % 3]);
  tft.setTextColor(tft.color565(150, 160, 180)); tft.setTextSize(1);
  ctext(190, "P3 pour jouer");
  ctextf(204, "puis dizaine %d debloquee", checkpointDecade + 2);
}

void drawCountdown(int n) {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_TXT); tft.setTextSize(2);
  ctextf(18, "Niveau %d", curLevel + 1);
  tft.setTextColor(COL_MEAT);
  if (n > 0) { tft.setTextSize(12); ctextf(70, "%d", n); }
  else { tft.setTextSize(8); ctext(90, "GO!"); }
}

void drawRaceEnd() {
  bool win = (raceResult == 1);
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(win ? COL_GOAL : COL_MEAT); tft.setTextSize(5);
  ctext(60, win ? "GAGNE!" : "PERDU");
  tft.setTextColor(COL_TXT); tft.setTextSize(2);
  if (!win && vFinWho >= 0) { tft.setTextColor(MP_COL[mpCol[vFinWho] & 3]); ctextf(118, "%s a gagne", MP_NAME[mpCol[vFinWho] & 3]); tft.setTextColor(COL_TXT); }
  if (twoP) { tft.setTextColor(MP_COL[myCol & 3]); tft.setTextSize(1); ctextf(190, "tu es %s", MP_NAME[myCol & 3]); }
  if (myFinT > 0) ctextf(150, "Ton temps: %u.%us", (unsigned)(myFinT / 1000), (unsigned)((myFinT / 100) % 10));
  else            ctext(150, "Ton temps: --");
  tft.setTextColor(tft.color565(150, 150, 170)); tft.setTextSize(1);
  ctext(212, "Bouton = menu");
}

void drawWin() {
  brickFill(0, 0, SCREEN_W, SCREEN_H);
  tft.setTextColor(COL_GOAL); tft.setTextSize(4);
  ctext(60, "100/100 !");
  tft.setTextColor(COL_TXT); tft.setTextSize(2);
  ctext(150, "Bouton = menu");
}
