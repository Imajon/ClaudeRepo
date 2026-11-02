// c3-berry : preuve de faisabilite « le jeu est un script ecrit par l'IA » (Brique, mode expert), sur la console c3-tft-meatboy.
// La console fait tourner un script Berry recu par le cable : pas de compilation, le firmware reste intact.
// Le C fait ce qui doit etre rapide : dessin en 4 bandes de 60 lignes, boutons, son, reseau (ESP-NOW). Le script decrit le jeu :
//   debut()      une fois au lancement (facultatif)
//   maj(dt)      la logique, dt en secondes
//   dessine()    les appels de dessin ; enregistres une fois, rejoues pour chaque bande de l'ecran
// Envoi : ligne "SCRIPT <octets>" puis le script ; reponse "OK ..." ou "ERR ...". Garde dans /jeu.be, relance au demarrage (sauf apres un plantage).
// Au demarrage : logo, puis le menu (Jouer, Classement, Mes stats, A propos). G + D tenus 1 s pendant un jeu = retour au menu.
// Mesures sur le port serie si on les demande ("STAT 1").
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "src/berry/src/berry.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "op.h"                                  // struct Op (avant les prototypes automatiques d'Arduino)

#define BERRY_VERSION "b2"                       // la page Brique la demande ("VERSION") ; a incrementer a chaque firmware publie
#define TFT_SCLK 4
#define TFT_MOSI 6
#define TFT_CS   7
#define TFT_DC   5
#define TFT_RST  10
#define SND_PIN  9
SET_LOOP_TASK_STACK_SIZE(16 * 1024);             // 8 Ko par defaut : debordement mesure en chargeant drift.be (l'analyseur de Berry est recursif)
const int BTN[3] = { 2, 1, 3 };                 // G, D, P3 (INPUT_PULLUP : appuye = 0)
#define W  320
#define H  240
#define BH 60                                    // hauteur d'une bande
#define NOPS 1200                                // appels de dessin par image
#define NTXT 3000                                // octets de texte par image

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

// ---- envoi a l'ecran en DMA (comme Alto) : une bande part pendant que le CPU dessine la suivante ----
uint16_t *buf[2];                                // deux bandes : on dessine l'une pendant que l'autre part
spi_device_handle_t dev = nullptr; spi_transaction_t tr[4]; int enVol = 0;
void dmaAttend(int garde) { while (enVol > garde) { spi_transaction_t *r; spi_device_get_trans_result(dev, &r, portMAX_DELAY); enVol--; } }
void cmd(uint8_t c, const uint8_t *d, int n) {
  spi_transaction_t t = {}; t.length = 8; t.tx_buffer = &c; gpio_set_level((gpio_num_t)TFT_DC, 0); spi_device_polling_transmit(dev, &t);
  if (n) { t.length = n * 8; t.tx_buffer = d; gpio_set_level((gpio_num_t)TFT_DC, 1); spi_device_polling_transmit(dev, &t); }
}
bool dmaDebut() {                                // apres tft.init : le bus SPI passe a l'IDF (80 MHz, DMA)
  SPI.end();
  spi_bus_config_t bc = {}; bc.mosi_io_num = TFT_MOSI; bc.miso_io_num = -1; bc.sclk_io_num = TFT_SCLK; bc.quadwp_io_num = -1; bc.quadhd_io_num = -1; bc.max_transfer_sz = W * 30 * 2;
  if (spi_bus_initialize(SPI2_HOST, &bc, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  spi_device_interface_config_t dc = {}; dc.clock_speed_hz = 80000000; dc.spics_io_num = -1; dc.queue_size = 4;
  if (spi_bus_add_device(SPI2_HOST, &dc, &dev) != ESP_OK) { dev = nullptr; return false; }
  pinMode(TFT_CS, OUTPUT); pinMode(TFT_DC, OUTPUT); return true;
}
void imageDebut() {                              // fenetre plein ecran, puis les 4 bandes a la suite
  dmaAttend(0); digitalWrite(TFT_CS, HIGH);
  const uint8_t ca[4] = { 0, 0, (W - 1) >> 8, (W - 1) & 255 }, ra[4] = { 0, 0, (H - 1) >> 8, (H - 1) & 255 };
  digitalWrite(TFT_CS, LOW); cmd(0x2A, ca, 4); cmd(0x2B, ra, 4); cmd(0x2C, nullptr, 0); gpio_set_level((gpio_num_t)TFT_DC, 1);
}
void bandeEnvoie(int k) {                        // 2 transferts de 30 lignes (le materiel limite un transfert a 32 Ko)
  for (int i = 0; i < 2; i++) { spi_transaction_t &t = tr[(k & 1) * 2 + i]; memset(&t, 0, sizeof t); t.length = W * 30 * 16; t.tx_buffer = buf[k & 1] + i * W * 30; spi_device_queue_trans(dev, &t, portMAX_DELAY); enVol++; }
}

// ---- une bande de l'ecran : couleurs deja dans l'ordre des octets de l'ecran (pas de conversion avant l'envoi) ----
static inline uint16_t sw(uint16_t c) { return (uint16_t)((c << 8) | (c >> 8)); }
class Bande : public Adafruit_GFX {
 public:
  uint16_t *b = nullptr;
  Bande() : Adafruit_GFX(W, BH) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override { if ((uint16_t)x < W && (uint16_t)y < BH) b[y * W + x] = c; }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    int x0 = max<int>(x, 0), y0 = max<int>(y, 0), x1 = min<int>(x + w, W), y1 = min<int>(y + h, BH);
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) { uint16_t *p = b + yy * W + x0; for (int n = x1 - x0; n > 0; n--) *p++ = c; }
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override { fillRect(x, y, w, 1, c); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override { fillRect(x, y, 1, h, c); }
  void fillScreen(uint16_t c) override { uint32_t v = c | ((uint32_t)c << 16), *q = (uint32_t *)b; for (int i = 0; i < W * BH / 2; i++) q[i] = v; }
};
Bande g;

// ---- liste des appels de dessin de l'image ----
enum { O_RECT, O_CADRE, O_DISQUE, O_CERCLE, O_LIGNE, O_TRI, O_TEXTE };
Op ops[NOPS]; int nOps = 0; char txt[NTXT]; int nTxt = 0;
uint16_t fondCol = 0; int camX = 0, camY = 0; bool tropOps = false;
Op *ajoute(uint8_t op, int y0, int y1, int col) {
  if (nOps >= NOPS) { tropOps = true; return nullptr; }
  Op &o = ops[nOps++]; o.op = op; o.y0 = y0; o.y1 = y1; o.col = sw((uint16_t)col); return &o;
}
void rejoue(int sTop) {                          // rejoue la liste dans la bande [sTop, sTop + BH)
  g.fillScreen(sw(fondCol));
  for (int i = 0; i < nOps; i++) {
    const Op &o = ops[i]; if (o.y1 < sTop || o.y0 >= sTop + BH) continue;
    const int s = sTop;
    switch (o.op) {
      case O_RECT: g.fillRect(o.a, o.b - s, o.c, o.d, o.col); break;
      case O_CADRE: g.drawRect(o.a, o.b - s, o.c, o.d, o.col); break;
      case O_DISQUE: g.fillCircle(o.a, o.b - s, o.c, o.col); break;
      case O_CERCLE: g.drawCircle(o.a, o.b - s, o.c, o.col); break;
      case O_LIGNE: g.drawLine(o.a, o.b - s, o.c, o.d - s, o.col); break;
      case O_TRI: g.fillTriangle(o.a, o.b - s, o.c, o.d - s, o.e, o.f - s, o.col); break;
      case O_TEXTE: g.setTextSize(o.t); g.setTextColor(o.col); g.setCursor(o.a, o.b - s); g.print(txt + o.c); break;
    }
  }
}

// ---- boutons, son, reseau ----
bool bt[3], btAv[3];

// ---- ecrans de la console (hors jeu) : logo, menu, classement, stats, a propos ----
enum { E_LOGO, E_MENU, E_JEU, E_CLASSEMENT, E_STATS, E_INFO };
int ecran = E_LOGO, choix = 0; uint32_t ecranT = 0;
const char *MENU[4] = { "Jouer", "Classement", "Mes stats", "A propos" };
Preferences prefs;
struct Record { char nom[22]; int32_t best; };   // classement : meilleur score de chaque jeu (par nom)
Record recs[8]; int nRecs = 0, recCourant = -1; bool recSale = false;
uint32_t stRecus = 0, stParties = 0, stSecondes = 0, stErreurs = 0; float stAcc = 0;   // stats gardees dans la console
char nomJeu[22] = "Mon jeu", note[80] = "";
void statsSauve() { prefs.putUInt("recus", stRecus); prefs.putUInt("parties", stParties); prefs.putUInt("secondes", stSecondes); prefs.putUInt("erreurs", stErreurs); }
void recsSauve() { prefs.putBytes("recs", recs, nRecs * sizeof(Record)); prefs.putUChar("nrecs", nRecs); recSale = false; }
void nomDuJeu(const char *src) {                 // 1re ligne "# Nom du jeu" (sinon "Mon jeu")
  snprintf(nomJeu, sizeof nomJeu, "Mon jeu");
  if (src[0] != '#') return;
  const char *a = src + 1; while (*a == ' ') a++;
  int n = 0; while (a[n] && a[n] != '\n' && a[n] != '\r' && n < (int)sizeof nomJeu - 1) n++;
  if (n) { memcpy(nomJeu, a, n); nomJeu[n] = 0; }
}
void txtOp(int x, int y, const char *t, uint16_t c, int taille) {   // texte des ecrans de la console (meme liste de dessin que les jeux)
  int n = strlen(t); if (nTxt + n + 1 > NTXT) return;
  if (Op *o = ajoute(O_TEXTE, y, y + 8 * taille, c)) { o->a = x; o->b = y; o->t = taille; o->c = nTxt; memcpy(txt + nTxt, t, n + 1); nTxt += n + 1; }
}
void rectOp(int x, int y, int w, int h, uint16_t c, bool plein = true) { if (Op *o = ajoute(plein ? O_RECT : O_CADRE, y, y + h, c)) { o->a = x; o->b = y; o->c = w; o->d = h; } }
#define VERT 0x3F10                               // le vert de Brique
#define GRIS 0x9CD3
void puce(int x, int y, int w, int h, int k) {   // le logo : un cadre de puce avec ses pattes (k = pattes deja sorties, animation)
  rectOp(x, y, w, h, VERT, false); rectOp(x + 1, y + 1, w - 2, h - 2, VERT, false);
  for (int i = 0; i < 6 && i < k; i++) { const int px = x + 14 + i * (w - 28) / 5; rectOp(px, y - 8, 3, 7, VERT); rectOp(px, y + h + 1, 3, 7, VERT); }
}
void dessineEcran() {                            // remplit la liste de dessin pour l'ecran courant (hors jeu)
  nOps = nTxt = 0; camX = camY = 0; fondCol = 0x0841; char b[64];
  const uint32_t t = millis() - ecranT;
  if (ecran == E_LOGO) {
    const int k = t / 180;
    puce(76, 70, 168, 64, k); txtOp(88, 86, "BRIQUE", VERT, 4);
    if (t > 900) txtOp(46, 160, "tu decris, l'IA ecrit, la console joue", GRIS, 1);
    txtOp(132, 224, "moteur " BERRY_VERSION, 0x4A69, 1);
    return;
  }
  puce(8, 10, 98, 26, 6); txtOp(16, 16, "BRIQUE", VERT, 2);
  if (ecran == E_MENU) {
    for (int i = 0; i < 4; i++) {
      const int y = 62 + i * 36;
      if (i == choix) rectOp(40, y - 6, 240, 30, VERT);
      txtOp(56, y, MENU[i], i == choix ? 0x0841 : 0xFFFF, 2);
    }
    if (note[0]) txtOp(8, 206, note, 0xFE60, 1);
    txtOp(8, 226, "G / D : choisir    P3 : ouvrir", GRIS, 1);
  } else if (ecran == E_CLASSEMENT) {
    txtOp(120, 18, "Classement", 0xFFFF, 2);
    if (!nRecs) txtOp(16, 70, "Pas encore de score sur cette console.", GRIS, 1);
    int ordre[8]; for (int i = 0; i < nRecs; i++) ordre[i] = i;
    for (int i = 0; i < nRecs; i++) for (int j = i + 1; j < nRecs; j++) if (recs[ordre[j]].best > recs[ordre[i]].best) { int x = ordre[i]; ordre[i] = ordre[j]; ordre[j] = x; }
    for (int i = 0; i < nRecs; i++) {
      const Record &r = recs[ordre[i]];
      snprintf(b, sizeof b, "%d. %s", i + 1, r.nom); txtOp(16, 60 + i * 20, b, i ? 0xFFFF : 0xFE60, 1);
      snprintf(b, sizeof b, "%ld", (long)r.best); txtOp(300 - 6 * strlen(b), 60 + i * 20, b, i ? 0xFFFF : 0xFE60, 1);
    }
    txtOp(8, 226, "P3 : retour", GRIS, 1);
  } else if (ecran == E_STATS) {
    txtOp(120, 18, "Mes stats", 0xFFFF, 2);
    snprintf(b, sizeof b, "Jeux recus de l'IA      %lu", (unsigned long)stRecus); txtOp(16, 64, b, 0xFFFF, 1);
    snprintf(b, sizeof b, "Parties jouees          %lu", (unsigned long)stParties); txtOp(16, 88, b, 0xFFFF, 1);
    snprintf(b, sizeof b, "Temps de jeu            %lu min %02lu s", (unsigned long)(stSecondes / 60), (unsigned long)(stSecondes % 60)); txtOp(16, 112, b, 0xFFFF, 1);
    snprintf(b, sizeof b, "Bugs rencontres         %lu", (unsigned long)stErreurs); txtOp(16, 136, b, 0xFFFF, 1);
    if (stErreurs) txtOp(16, 160, "Chaque bug corrige, c'est un pas de plus.", GRIS, 1);
    txtOp(8, 226, "P3 : retour", GRIS, 1);
  } else if (ecran == E_INFO) {
    const char *L[] = { "Brique : une console a fabriquer et a programmer.", "",
      "Tes jeux sont ecrits par une IA : Mistral", "(modele Codestral), une IA francaise.", "",
      "Tu decris ton jeu avec tes mots, l'IA ecrit le", "programme, ta console le fait tourner.", "",
      "L'IA peut se tromper : teste, repere ce qui ne", "va pas, et demande mieux. C'est toi qui decides.", "",
      "La console ne garde que tes jeux et tes scores." };
    for (int i = 0; i < 12; i++) txtOp(10, 48 + i * 13, L[i], i < 1 ? 0xFE60 : 0xFFFF, 1);
    txtOp(8, 226, "P3 : retour", GRIS, 1);
  }
}
void allerMenu() { ecran = E_MENU; ecranT = millis(); }
uint32_t sonFin = 0;
bool netOn = false; uint8_t myMac[6]; const uint8_t BCAST[6] = { 255, 255, 255, 255, 255, 255 };
struct Msg { uint16_t de; uint8_t n; char d[200]; };
volatile int msgTete = 0, msgQueue = 0; Msg msgs[8];
void onRecv(const esp_now_recv_info_t *info, const uint8_t *d, int n) {
  if (n <= 0 || n > 199) return;
  Msg &m = msgs[msgTete & 7]; m.de = ((uint16_t)info->src_addr[4] << 8) | info->src_addr[5]; m.n = n; memcpy(m.d, d, n); m.d[n] = 0; msgTete++;
  if (msgTete - msgQueue > 8) msgQueue = msgTete - 8;   // les plus anciens sont perdus
}
bool reseau() {                                  // ESP-NOW au minimum (meme reglage que la console) ; allume au premier envoi / reception
  if (netOn) return true;
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.static_rx_buf_num = 4; cfg.dynamic_rx_buf_num = 8; cfg.tx_buf_type = 1; cfg.static_tx_buf_num = 0; cfg.dynamic_tx_buf_num = 8; cfg.cache_tx_buf_num = 0;
  cfg.ampdu_rx_enable = 0; cfg.ampdu_tx_enable = 0; cfg.amsdu_tx_enable = 0; cfg.nvs_enable = 0;
  if (esp_wifi_init(&cfg) != ESP_OK) return false;
  esp_wifi_set_storage(WIFI_STORAGE_RAM); esp_wifi_set_mode(WIFI_MODE_STA);
  if (esp_wifi_start() != ESP_OK) { esp_wifi_deinit(); return false; }
  esp_wifi_set_max_tx_power(60); esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); esp_wifi_get_mac(WIFI_IF_STA, myMac);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t p = {}; memcpy(p.peer_addr, BCAST, 6); p.channel = 1; p.encrypt = false; esp_now_add_peer(&p);
  netOn = true; return true;
}

// ---- Berry : la machine, ses erreurs, les fonctions offertes au script ----
bvm *vm = nullptr; bool jeuOk = false; char erreur[160] = ""; int errN = 0; uint32_t appelDebut = 0;
extern "C" void berry_sortie(const char *s, size_t n) {   // print() et erreurs du script -> port serie (+ message d'erreur)
  Serial.write((const uint8_t *)s, n);
  for (size_t i = 0; i < n && errN < (int)sizeof erreur - 1; i++) erreur[errN++] = s[i];
  erreur[errN] = 0;
}
uint32_t gcT0 = 0, gcUs = 0, gcMax = 0, gcN = 0;   // nettoyages de la memoire de Berry (mesure des saccades)
static void heartbeat(bvm *v, int ev, ...) {     // toutes les 8192 instructions : un appel du script ne doit pas bloquer la console
  if (ev == BE_OBS_GC_START) { gcT0 = micros(); return; }
  if (ev == BE_OBS_GC_END) { uint32_t d = micros() - gcT0; gcUs += d; gcN++; if (d > gcMax) gcMax = d; return; }
  if (ev == BE_OBS_VM_HEARTBEAT && millis() - appelDebut > 500) be_raise(v, "temps_error", "le script tourne trop longtemps (boucle sans fin ?)");
}
static int I(bvm *v, int i, int def = 0) { return be_top(v) >= i ? (int)be_toint(v, i) : def; }
static int f_fond(bvm *v) { fondCol = I(v, 1); be_return_nil(v); }
static int f_rect(bvm *v) { int y = I(v, 2) - camY, h = I(v, 4); if (Op *o = ajoute(O_RECT, y, y + h, I(v, 5, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y; o->c = I(v, 3); o->d = h; } be_return_nil(v); }
static int f_cadre(bvm *v) { int y = I(v, 2) - camY, h = I(v, 4); if (Op *o = ajoute(O_CADRE, y, y + h, I(v, 5, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y; o->c = I(v, 3); o->d = h; } be_return_nil(v); }
static int f_disque(bvm *v) { int y = I(v, 2) - camY, r = I(v, 3); if (Op *o = ajoute(O_DISQUE, y - r, y + r, I(v, 4, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y; o->c = r; } be_return_nil(v); }
static int f_cercle(bvm *v) { int y = I(v, 2) - camY, r = I(v, 3); if (Op *o = ajoute(O_CERCLE, y - r, y + r, I(v, 4, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y; o->c = r; } be_return_nil(v); }
static int f_ligne(bvm *v) { int y1 = I(v, 2) - camY, y2 = I(v, 4) - camY; if (Op *o = ajoute(O_LIGNE, min(y1, y2), max(y1, y2), I(v, 5, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y1; o->c = I(v, 3) - camX; o->d = y2; } be_return_nil(v); }
static int f_tri(bvm *v) {
  int y1 = I(v, 2) - camY, y2 = I(v, 4) - camY, y3 = I(v, 6) - camY;
  if (Op *o = ajoute(O_TRI, min(y1, min(y2, y3)), max(y1, max(y2, y3)), I(v, 7, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y1; o->c = I(v, 3) - camX; o->d = y2; o->e = I(v, 5) - camX; o->f = y3; }
  be_return_nil(v);
}
static int f_texte(bvm *v) {                     // texte(x, y, chaine, couleur, taille)
  const char *s = be_top(v) >= 3 ? be_tostring(v, 3) : ""; int n = strlen(s), t = constrain(I(v, 5, 1), 1, 6), y = I(v, 2) - camY;
  if (nTxt + n + 1 > NTXT) { tropOps = true; be_return_nil(v); }
  if (Op *o = ajoute(O_TEXTE, y, y + 8 * t, I(v, 4, 0xFFFF))) { o->a = I(v, 1) - camX; o->b = y; o->c = nTxt; o->t = t; memcpy(txt + nTxt, s, n + 1); nTxt += n + 1; }
  be_return_nil(v);
}
static int f_rgb(bvm *v) { int r = I(v, 1), gg = I(v, 2), b = I(v, 3); be_pushint(v, ((r & 0xF8) << 8) | ((gg & 0xFC) << 3) | (b >> 3)); be_return(v); }
static int f_camera(bvm *v) { camX = I(v, 1); camY = I(v, 2); be_return_nil(v); }
static int f_bouton(bvm *v) { int k = I(v, 1); be_pushbool(v, k >= 0 && k < 3 && bt[k]); be_return(v); }
static int f_appui(bvm *v) { int k = I(v, 1); be_pushbool(v, k >= 0 && k < 3 && bt[k] && !btAv[k]); be_return(v); }
static int f_lache(bvm *v) { int k = I(v, 1); be_pushbool(v, k >= 0 && k < 3 && !bt[k] && btAv[k]); be_return(v); }
static int f_temps(bvm *v) { be_pushreal(v, millis() / 1000.0f); be_return(v); }
static int f_hasard(bvm *v) { be_pushint(v, random(I(v, 1), I(v, 2) + 1)); be_return(v); }
static int f_son(bvm *v) { int f = I(v, 1), ms = I(v, 2, 80); if (f > 0) tone(SND_PIN, f); else noTone(SND_PIN); sonFin = millis() + ms; be_return_nil(v); }
static int f_envoie(bvm *v) {                    // envoie(chaine) a toutes les consoles autour
  const char *s = be_top(v) >= 1 ? be_tostring(v, 1) : ""; int n = min((int)strlen(s), 199);
  be_pushbool(v, reseau() && esp_now_send(BCAST, (const uint8_t *)s, n) == ESP_OK); be_return(v);
}
static int f_recois(bvm *v) {                    // liste des messages recus depuis le dernier appel : [[de, chaine], ...]
  reseau(); be_newobject(v, "list");             // objet list (+ sa liste interne au-dessus de la pile)
  while (msgQueue != msgTete) {
    const Msg &m = msgs[msgQueue & 7]; msgQueue++;
    be_newobject(v, "list"); be_pushint(v, m.de); be_data_push(v, -2); be_pop(v, 1); be_pushstring(v, m.d); be_data_push(v, -2); be_pop(v, 2);
    be_data_push(v, -2); be_pop(v, 1);
  }
  be_pop(v, 1); be_return(v);
}
static int f_score(bvm *v) {                     // score(n) : le classement garde le meilleur score de ce jeu
  const int32_t n = I(v, 1);
  if (recCourant < 0) {
    for (int i = 0; i < nRecs; i++) if (!strcmp(recs[i].nom, nomJeu)) recCourant = i;
    if (recCourant < 0) { if (nRecs < 8) recCourant = nRecs++; else { recCourant = 0; for (int i = 1; i < 8; i++) if (recs[i].best < recs[recCourant].best) recCourant = i; }
      strcpy(recs[recCourant].nom, nomJeu); recs[recCourant].best = n; recSale = true; }
  }
  if (n > recs[recCourant].best) { recs[recCourant].best = n; recSale = true; }
  be_return_nil(v);
}
static int f_moi(bvm *v) { reseau(); be_pushint(v, ((uint16_t)myMac[4] << 8) | myMac[5]); be_return(v); }

bool appelle(const char *nom, int argc = 0, float a = 0) {   // false = erreur (le jeu s'arrete, le message s'affiche)
  if (!be_getglobal(vm, nom) || !be_isfunction(vm, -1)) { be_pop(vm, 1); return true; }
  if (argc) be_pushreal(vm, a);
  appelDebut = millis();
  if (be_pcall(vm, argc)) { errN = 0; be_dumpexcept(vm); jeuOk = false; stErreurs++; statsSauve(); Serial.printf("ERR %s\n", erreur); return false; }
  be_pop(vm, argc + 1); return true;
}
const char *lance(const char *src, size_t n) {   // nouvelle machine, le script, debut() ; nullptr si tout va bien
  if (vm) be_vm_delete(vm);
  vm = be_vm_new(); be_set_obs_hook(vm, heartbeat); jeuOk = false; errN = 0; erreur[0] = 0;
  nomDuJeu(src); recCourant = -1; ecran = E_JEU; ecranT = millis(); stParties++;
  be_regfunc(vm, "fond", f_fond); be_regfunc(vm, "rect", f_rect); be_regfunc(vm, "cadre", f_cadre); be_regfunc(vm, "disque", f_disque);
  be_regfunc(vm, "cercle", f_cercle); be_regfunc(vm, "ligne", f_ligne); be_regfunc(vm, "tri", f_tri); be_regfunc(vm, "texte", f_texte);
  be_regfunc(vm, "rgb", f_rgb); be_regfunc(vm, "camera", f_camera); be_regfunc(vm, "bouton", f_bouton); be_regfunc(vm, "appui", f_appui);
  be_regfunc(vm, "lache", f_lache); be_regfunc(vm, "temps", f_temps); be_regfunc(vm, "hasard", f_hasard); be_regfunc(vm, "son", f_son);
  be_regfunc(vm, "envoie", f_envoie); be_regfunc(vm, "recois", f_recois); be_regfunc(vm, "moi", f_moi); be_regfunc(vm, "score", f_score);
  const char *K[3] = { "G", "D", "P3" }; for (int k = 0; k < 3; k++) { be_pushint(vm, k); be_setglobal(vm, K[k]); be_pop(vm, 1); }
  appelDebut = millis();
  if (be_loadbuffer(vm, "jeu", src, n) || be_pcall(vm, 0)) { errN = 0; be_dumpexcept(vm); stErreurs++; statsSauve(); return erreur; }
  be_pop(vm, 1);
  jeuOk = true;
  if (!appelle("debut")) return erreur;
  return nullptr;
}

// ---- reception d'un script par le cable ----
bool stats = false;                              // mesures sur le port serie : seulement si on les demande ("STAT 1")
uint8_t btSim = 0;                               // boutons simules par le cable (mesures en jeu)
char ligne[48]; int nLigne = 0; char *recu = nullptr; int attendu = 0, nRecu = 0; uint32_t recuT = 0;
void ecoute() {
  while (Serial.available()) {
    int c = Serial.read();
    if (recu) {
      recu[nRecu++] = c; recuT = millis();
      if (nRecu == attendu) {
        recu[nRecu] = 0;
        File f = LittleFS.open("/jeu.be", "w"); if (f) { f.write((const uint8_t *)recu, nRecu); f.close(); }
        stRecus++;
        const char *e = lance(recu, nRecu);
        if (e) { Serial.printf("ERR %s\n", e); } else Serial.printf("OK %d octets, tas %u\n", nRecu, (unsigned)ESP.getFreeHeap());
        free(recu); recu = nullptr;
      }
      continue;
    }
    if (c == '\n') {
      ligne[nLigne] = 0; nLigne = 0;
      if (!strcmp(ligne, "VERSION")) Serial.println("BERRY VERSION " BERRY_VERSION);
      if (!strncmp(ligne, "STAT ", 5)) stats = ligne[5] == '1';
      if (!strncmp(ligne, "BTN ", 4)) { btSim = 0; for (int k = 0; k < 3 && ligne[4 + k]; k++) btSim |= (ligne[4 + k] == '1') << k; }   // test : boutons tenus G D P3, ex. "BTN 011"
      if (!strncmp(ligne, "SCRIPT ", 7)) {
        attendu = atoi(ligne + 7); nRecu = 0; recuT = millis();
        recu = attendu > 0 && attendu < 60000 ? (char *)malloc(attendu + 1) : nullptr;
        if (!recu) Serial.println("ERR taille ou memoire");
      }
    } else if (nLigne < (int)sizeof ligne - 1) ligne[nLigne++] = c;
  }
  if (recu && millis() - recuT > 3000) { free(recu); recu = nullptr; Serial.println("ERR envoi coupe"); }
}

void setup() {
  Serial.setRxBufferSize(4096);                 // un script arrive d'un coup
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);                      // sans programme qui lit le port USB, ecrire bloquait le jeu (~10 img/s) : on perd le texte au lieu d'attendre
  for (int k = 0; k < 3; k++) pinMode(BTN[k], INPUT_PULLUP);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320); tft.setRotation(1); tft.setSPISpeed(80000000);
  buf[0] = (uint16_t *)heap_caps_malloc(W * BH * 2, MALLOC_CAP_DMA); buf[1] = (uint16_t *)heap_caps_malloc(W * BH * 2, MALLOC_CAP_DMA);
  if (!dmaDebut()) Serial.println("ERR pas de DMA");
  LittleFS.begin(true);
  const esp_reset_reason_t rr = esp_reset_reason();
  const bool plante = rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT;
  if (plante) snprintf(note, sizeof note, "Ton jeu a plante la console. Demande a l'IA de corriger.");
  prefs.begin("berry", false);
  stRecus = prefs.getUInt("recus"); stParties = prefs.getUInt("parties"); stSecondes = prefs.getUInt("secondes"); stErreurs = prefs.getUInt("erreurs");
  nRecs = min((int)prefs.getUChar("nrecs"), 8); if (nRecs) prefs.getBytes("recs", recs, nRecs * sizeof(Record));
  ecran = E_LOGO; ecranT = millis(); tone(SND_PIN, 523); sonFin = millis() + 90;
}

uint32_t tPrecImg = 0, pire = 0, lentes = 0;     // pire image (ms), images de plus de 25 ms
uint32_t tPrec = 0, statT = 0, nImg = 0, usScript = 0, usRendu = 0;
void loop() {
  ecoute();
  if (recu) return;                              // reception d'un script en cours : pas d'image
  if (sonFin && millis() > sonFin) { noTone(SND_PIN); sonFin = 0; }
  for (int k = 0; k < 3; k++) { btAv[k] = bt[k]; bt[k] = !digitalRead(BTN[k]) || (btSim >> k & 1); }
  const uint32_t now = millis(); float dt = (now - tPrec) / 1000.0f; tPrec = now; if (dt > 0.1f) dt = 0.1f;
  uint32_t t0 = micros();
  const bool appuiP3 = bt[2] && !btAv[2], appuiG = bt[0] && !btAv[0], appuiD = bt[1] && !btAv[1];
  static uint32_t gdT = 0;                       // G + D tenus 1 s : quitter le jeu
  if (bt[0] && bt[1]) { if (!gdT) gdT = now; } else gdT = 0;
  if (ecran == E_JEU && gdT && now - gdT > 1000) {
    gdT = 0; if (vm) { be_vm_delete(vm); vm = nullptr; } jeuOk = false; noTone(SND_PIN);
    if (recSale) recsSauve(); statsSauve(); allerMenu();
  }
  g.setTextWrap(false);
  if (ecran == E_LOGO) {
    const uint32_t t = now - ecranT;
    if (t > 180 && t < 200) { tone(SND_PIN, 659); sonFin = now + 90; } else if (t > 360 && t < 380) { tone(SND_PIN, 784); sonFin = now + 90; } else if (t > 540 && t < 560) { tone(SND_PIN, 1047); sonFin = now + 160; }
    if (t > 2200 || (t > 300 && (appuiP3 || appuiG || appuiD))) allerMenu();
    dessineEcran();
  } else if (ecran == E_MENU) {
    if (appuiG) choix = (choix + 3) % 4; if (appuiD) choix = (choix + 1) % 4;
    if (appuiP3) {
      note[0] = 0;
      if (choix == 0) {                          // Jouer : le dernier jeu recu
        File f = LittleFS.open("/jeu.be", "r");
        if (!f) snprintf(note, sizeof note, "Pas encore de jeu : cree-le sur Brique (mode script).");
        else { size_t n = f.size(); char *s = (char *)malloc(n + 1); if (s) { f.read((uint8_t *)s, n); s[n] = 0; lance(s, n); free(s); } f.close(); }
      } else { ecran = choix == 1 ? E_CLASSEMENT : choix == 2 ? E_STATS : E_INFO; ecranT = now; }
    }
    if (ecran != E_JEU) dessineEcran();
  } else if (ecran != E_JEU) {
    if (appuiP3) allerMenu();
    dessineEcran();
  }
  if (ecran == E_JEU) {
    if (jeuOk) {
      appelle("maj", 1, dt);
      nOps = 0; nTxt = 0; camX = camY = 0; tropOps = false;
      if (jeuOk) appelle("dessine");
      stAcc += dt; if (stAcc >= 1) { stAcc -= 1; stSecondes++; if (stSecondes % 60 == 0) { statsSauve(); if (recSale) recsSauve(); } }
    }
    if (!jeuOk) {                                // erreur : on l'affiche (P3 = menu)
      nOps = nTxt = 0; camX = camY = 0; fondCol = 0x2000;
      Op *o = ajoute(O_TEXTE, 0, 240, 0xFFE0); o->a = 6; o->b = 8; o->t = 1; o->c = 0;
      snprintf(txt, NTXT, "%s", erreur[0] ? erreur : "..."); nTxt = strlen(txt) + 1;
      txtOp(8, 226, "P3 : menu", GRIS, 1);
      g.setTextWrap(true);
      if (appuiP3) { if (vm) { be_vm_delete(vm); vm = nullptr; } allerMenu(); }
    }
  }
  uint32_t t1 = micros();
  imageDebut();
  for (int k = 0; k < H / BH; k++) { dmaAttend(2); g.b = buf[k & 1]; rejoue(k * BH); bandeEnvoie(k); }   // la bande k-2 (meme tampon) doit etre partie
  uint32_t t2 = micros();
  usScript += t1 - t0; usRendu += t2 - t1; nImg++;
  { const uint32_t ms = now - tPrecImg; tPrecImg = now; if (ms > pire) pire = ms; if (ms > 25) lentes++; }
  if (now - statT >= 1000) {
    if (stats) Serial.printf("STAT %u img/s  script %u us  rendu %u us  pire %u ms  lentes %u  gc %u x (max %u us)  appels %d%s  tas %u  reseau %d\n", (unsigned)nImg, (unsigned)(usScript / max(1u, (unsigned)nImg)), (unsigned)(usRendu / max(1u, (unsigned)nImg)),
                  (unsigned)pire, (unsigned)lentes, (unsigned)gcN, (unsigned)gcMax, nOps, tropOps ? " (trop !)" : "", (unsigned)ESP.getFreeHeap(), netOn);
    statT = now; nImg = usScript = usRendu = pire = lentes = gcN = gcMax = gcUs = 0;
  }
}
