// ============================================================
//  ESP2 — MATRICE LED 8×32 · Dual ESP32 Color Train
//  Studio EYA — 2026-06-07
//
//  MÉCANIQUE :
//   • Un train (3 colonnes colorées) arrive de la droite, avance vers la gauche
//   • Le joueur envoie des pixels colorés depuis la gauche (un par appui bouton)
//   • Les pixels joueur avancent vers la droite
//   • Si un pixel joueur rencontre la prochaine colonne attendue du train
//     et que la couleur correspond → la colonne est "validée" (flash blanc)
//   • Valider les 3 colonnes dans l'ordre → SCORE + explosion arc-en-ciel
//   • Mauvaise couleur → flash rouge, -1 vie, train détruit
//   • Train qui dépasse la gauche sans être validé → -1 vie
// ============================================================

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>

// ── Paramètres matrice ─────────────────────────────────────
#define LED_PIN      5
#define MATRIX_W     32      // largeur (colonnes)
#define MATRIX_H     8       // hauteur (lignes)
#define NUM_LEDS     (MATRIX_W * MATRIX_H)
#define BRIGHTNESS   60
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

// ── Paramètres jeu ─────────────────────────────────────────
#define TRAIN_COLS   3       // nombre de colonnes du train (= longueur séquence)
#define TRAIN_SPEED  0.04f   // colonnes/ms au départ
#define TRAIN_SPEED_MAX 0.15f
#define TRAIN_INTERVAL   3500  // ms entre deux trains (décroît avec score)
#define PIXEL_SPEED  0.06f   // colonnes/ms pour les pixels joueur

// ── Couleurs ───────────────────────────────────────────────
const CRGB GAME_COLORS[] = { CRGB::Red, CRGB::Green, CRGB::Blue };
const char* COLOR_NAMES[]= { "ROUGE", "VERT", "BLEU" };

// ── Mapping matrice serpentin ──────────────────────────────
// WS2812B 8×32 câblé en serpentin vertical :
// colonne 0 = LEDs 0–7 (bas→haut), colonne 1 = LEDs 8–15 (haut→bas), etc.
int xyToIndex(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return -1;
  if (x % 2 == 0) return x * MATRIX_H + y;
  else             return x * MATRIX_H + (MATRIX_H - 1 - y);
}

// ── Tableau LED ────────────────────────────────────────────
CRGB leds[NUM_LEDS];

// ── Structure message ESP-NOW ──────────────────────────────
typedef struct {
  uint8_t  buttons;
  uint8_t  gameEvent;
  uint32_t msgId;
} GameMsg;

// ── Train ──────────────────────────────────────────────────
struct Train {
  float    pos;              // position de la tête (colonne, float), part de MATRIX_W-1 → 0
  uint8_t  seq[TRAIN_COLS];  // séquence de couleurs (indices 0/1/2)
  bool     validated[TRAIN_COLS]; // quelles colonnes ont été validées
  int      nextExpected;     // prochain index à valider (0, 1 ou 2)
  bool     active;
};

// ── Pixel joueur ───────────────────────────────────────────
struct PlayerPixel {
  float   pos;        // position colonne (va vers la droite)
  uint8_t colorIdx;
  bool    active;
};

#define MAX_PLAYER_PIXELS 6

Train        train;
PlayerPixel  playerPixels[MAX_PLAYER_PIXELS];
float        trainSpeed    = TRAIN_SPEED;
uint32_t     trainInterval = TRAIN_INTERVAL;
uint8_t      score         = 0;
uint8_t      lives         = 3;
uint32_t     lastTrainTime = 0;

// ── ESP-NOW ────────────────────────────────────────────────
volatile bool     newData    = false;
volatile uint8_t  recvBtns   = 0;
volatile uint8_t  recvEvent  = 0;
volatile uint32_t recvMsgId  = 0;
volatile uint8_t  senderMAC[6];
uint8_t           lastButtons = 0;
uint32_t          totalReceived = 0;
uint32_t          lastHeartbeat = 0;

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len == sizeof(GameMsg)) {
    const GameMsg *msg = (const GameMsg *)data;
    recvBtns  = msg->buttons;
    recvEvent = msg->gameEvent;
    recvMsgId = msg->msgId;
    memcpy((void*)senderMAC, mac, 6);
    newData = true;
    totalReceived++;
  } else {
    Serial.printf("[ESP-NOW] ✗ Taille inattendue : %d (attendu %d)\n", len, sizeof(GameMsg));
  }
}

// ────────────────────────────────────────────────────────────
//  Utilitaires affichage
// ────────────────────────────────────────────────────────────

// Dessine une colonne entière d'une couleur
void drawColumn(int col, CRGB color) {
  if (col < 0 || col >= MATRIX_W) return;
  for (int y = 0; y < MATRIX_H; y++) {
    int idx = xyToIndex(col, y);
    if (idx >= 0) leds[idx] = color;
  }
}

// Dessine une colonne avec atténuation
void drawColumnScaled(int col, CRGB color, uint8_t scale) {
  if (col < 0 || col >= MATRIX_W) return;
  CRGB c = color;
  c.nscale8(scale);
  for (int y = 0; y < MATRIX_H; y++) {
    int idx = xyToIndex(col, y);
    if (idx >= 0) leds[idx] = c;
  }
}

// Dessine un pixel unique (milieu de la colonne, hauteur centrée)
void drawPixelPlayer(int col, CRGB color) {
  if (col < 0 || col >= MATRIX_W) return;
  // Pixel joueur : 3 pixels centrés verticalement
  int mid = MATRIX_H / 2;
  for (int dy = -1; dy <= 1; dy++) {
    int y = mid + dy;
    if (y >= 0 && y < MATRIX_H) {
      int idx = xyToIndex(col, y);
      if (idx >= 0) leds[idx] = color;
    }
  }
}

void showExplosionRainbow() {
  for (int t = 0; t < 4; t++) {
    for (int x = 0; x < MATRIX_W; x++) {
      CRGB c = CHSV((x * 8 + t * 30) & 0xFF, 255, 200);
      drawColumn(x, c);
    }
    FastLED.show(); delay(60);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show(); delay(40);
  }
}

void showValidationFlash(int col, CRGB color) {
  // Flash blanc sur la colonne validée
  for (int f = 0; f < 2; f++) {
    drawColumn(col, CRGB::White);
    FastLED.show(); delay(50);
    drawColumn(col, color);
    FastLED.show(); delay(40);
  }
}

void showFail() {
  for (int i = 0; i < 2; i++) {
    fill_solid(leds, NUM_LEDS, CRGB(50, 0, 0));
    FastLED.show(); delay(120);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show(); delay(80);
  }
}

void showGameOver() {
  // Balayage rouge de droite à gauche
  for (int wave = 0; wave < 3; wave++) {
    for (int x = MATRIX_W - 1; x >= 0; x--) {
      drawColumn(x, CRGB::Red);
      FastLED.show(); delay(15);
    }
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show(); delay(150);
  }
}

void showLivesIndicator() {
  // Affiche les vies en bas à gauche (3 petits points)
  for (int i = 0; i < 3; i++) {
    CRGB c = (i < lives) ? CRGB(0, 40, 0) : CRGB(20, 0, 0);
    int idx = xyToIndex(i, 0);
    if (idx >= 0) leds[idx] = c;
  }
}

// ────────────────────────────────────────────────────────────
//  Logique jeu
// ────────────────────────────────────────────────────────────

void spawnTrain() {
  train.pos          = (float)(MATRIX_W - 1);  // part de la droite
  train.nextExpected = 0;
  train.active       = true;
  for (int i = 0; i < TRAIN_COLS; i++) {
    train.seq[i]       = random(3);
    train.validated[i] = false;
  }
  Serial.printf("[JEU] Nouveau train → %s %s %s\n",
                COLOR_NAMES[train.seq[0]],
                COLOR_NAMES[train.seq[1]],
                COLOR_NAMES[train.seq[2]]);
}

void spawnPlayerPixel(uint8_t colorIdx) {
  for (int i = 0; i < MAX_PLAYER_PIXELS; i++) {
    if (!playerPixels[i].active) {
      playerPixels[i].pos      = 0.0f;  // part de la gauche
      playerPixels[i].colorIdx = colorIdx;
      playerPixels[i].active   = true;
      Serial.printf("[JEU] Pixel joueur %s lancé\n", COLOR_NAMES[colorIdx]);
      return;
    }
  }
  Serial.println("[JEU] Trop de pixels actifs — appui ignoré");
}

void resetGame() {
  score        = 0;
  lives        = 3;
  trainSpeed   = TRAIN_SPEED;
  trainInterval = TRAIN_INTERVAL;
  train.active = false;
  for (int i = 0; i < MAX_PLAYER_PIXELS; i++) playerPixels[i].active = false;
  lastTrainTime = millis();
}

// ────────────────────────────────────────────────────────────
//  SETUP
// ────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("  ESP2 — Matrice LED 8×32");
  Serial.println("========================================");

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  Serial.printf("[LED] Matrice %d×%d = %d LEDs, GPIO %d\n", MATRIX_W, MATRIX_H, NUM_LEDS, LED_PIN);

  // Test visuel : colonnes R→G→B de gauche à droite
  Serial.println("[LED] Test visuel...");
  for (int c = 0; c < 3; c++) {
    for (int x = 0; x < MATRIX_W; x++) {
      drawColumn(x, GAME_COLORS[c]);
      FastLED.show();
      delay(20);
    }
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
  }
  Serial.println("[LED] Test OK");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.print("[WiFi] Mon adresse MAC : ");
  Serial.println(WiFi.macAddress());
  Serial.println("       → Copie cette MAC dans ESP2_MAC[] de l'ESP1 !");

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] ✗ Init échouée — redémarre");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onReceive);
  Serial.println("[ESP-NOW] ✓ Initialisé — en attente de l'ESP1...");

  randomSeed(esp_random());
  resetGame();
  spawnTrain();

  lastHeartbeat = millis();
  Serial.println("========================================\n");
}

// ────────────────────────────────────────────────────────────
//  LOOP
// ────────────────────────────────────────────────────────────

void loop() {
  uint32_t now = millis();

  // ── Heartbeat log toutes les 5s ───────────────────────────
  if (now - lastHeartbeat >= 5000) {
    lastHeartbeat = now;
    Serial.printf("[STAT] reçus=%lu | score=%d | vies=%d | train=%s (pos=%.1f) | next=%s\n",
                  totalReceived, score, lives,
                  train.active ? "actif" : "inactif",
                  train.pos,
                  train.active ? COLOR_NAMES[train.seq[train.nextExpected]] : "-");
    if (totalReceived == 0)
      Serial.println("[STAT] ⚠ Aucun message ESP1 — vérifie la MAC");
  }

  // ── Lecture message ESP-NOW ───────────────────────────────
  if (newData) {
    newData = false;
    uint8_t btns  = recvBtns;
    uint8_t event = recvEvent;

    if (event == 1) {
      // ESP1 envoie event=1 uniquement au moment de l'appui physique.
      // On spawn directement sur le bitmask reçu, sans logique de front montant.
      if (btns & (1 << 0)) spawnPlayerPixel(0);   // ROUGE
      if (btns & (1 << 1)) spawnPlayerPixel(1);   // VERT
      if (btns & (1 << 2)) spawnPlayerPixel(2);   // BLEU
    }
    // heartbeat (event=0) : ignoré côté jeu
  }

  // ── Avancer pixels joueur & collision ────────────────────
  static uint32_t lastUpdate = 0;
  uint32_t delta = now - lastUpdate;
  if (delta == 0) goto render;
  lastUpdate = now;

  for (int i = 0; i < MAX_PLAYER_PIXELS; i++) {
    if (!playerPixels[i].active) continue;
    playerPixels[i].pos += PIXEL_SPEED * delta;

    if (!train.active) {
      // Pas de train : pixel sort par la droite sans effet
      if (playerPixels[i].pos >= MATRIX_W) playerPixels[i].active = false;
      continue;
    }

    // ── Détection collision pixel joueur ↔ front du train ──
    int pCol = (int)playerPixels[i].pos;
    int tHead = (int)train.pos;  // colonne de tête du train (la plus à gauche)

    // La tête du train est à train.pos, les colonnes suivantes sont à +1, +2
    // On cherche si le pixel atteint la prochaine colonne attendue du train
    int targetCol = tHead + train.nextExpected;  // colonne à valider

    if (pCol >= targetCol) {
      uint8_t pColor = playerPixels[i].colorIdx;
      uint8_t needed = train.seq[train.nextExpected];
      playerPixels[i].active = false;

      if (pColor == needed) {
        // ✓ Bonne couleur !
        train.validated[train.nextExpected] = true;
        Serial.printf("[JEU] ✓ Colonne %d validée (%s)\n",
                      train.nextExpected, COLOR_NAMES[needed]);
        showValidationFlash(targetCol, GAME_COLORS[needed]);
        train.nextExpected++;

        if (train.nextExpected >= TRAIN_COLS) {
          // 🎉 Toutes les colonnes validées !
          score++;
          Serial.printf("[JEU] ✓✓✓ Train complet ! Score=%d\n", score);
          showExplosionRainbow();
          train.active = false;
          if (trainInterval > 1200) trainInterval -= 150;
          if (trainSpeed < TRAIN_SPEED_MAX) trainSpeed += 0.005f;
          for (int j = 0; j < MAX_PLAYER_PIXELS; j++) playerPixels[j].active = false;
        }
      } else {
        // ✗ Mauvaise couleur
        lives--;
        Serial.printf("[JEU] ✗ Mauvaise couleur ! Attendu=%s Reçu=%s Vies=%d\n",
                      COLOR_NAMES[needed], COLOR_NAMES[pColor], lives);
        showFail();
        train.active = false;
        for (int j = 0; j < MAX_PLAYER_PIXELS; j++) playerPixels[j].active = false;

        if (lives == 0) {
          Serial.println("[JEU] ✗ GAME OVER");
          showGameOver();
          resetGame();
          spawnTrain();
        }
      }
    }

    // Pixel dépasse la droite sans collision
    if (playerPixels[i].pos >= MATRIX_W) playerPixels[i].active = false;
  }

  // ── Avancer le train ──────────────────────────────────────
  if (train.active && lives > 0) {
    train.pos -= trainSpeed * delta;

    // Train sort par la gauche → vie perdue
    if (train.pos < -TRAIN_COLS) {
      lives--;
      Serial.printf("[JEU] Train manqué ! Vies=%d\n", lives);
      train.active = false;
      for (int j = 0; j < MAX_PLAYER_PIXELS; j++) playerPixels[j].active = false;
      showFail();
      if (lives == 0) {
        Serial.println("[JEU] ✗ GAME OVER");
        showGameOver();
        resetGame();
        spawnTrain();
      }
      lastTrainTime = now;
    }
  }

  // ── Spawn nouveau train ───────────────────────────────────
  if (!train.active && lives > 0 && (now - lastTrainTime >= trainInterval)) {
    spawnTrain();
    lastTrainTime = now;
  }

  // ── RENDU ─────────────────────────────────────────────────
  render:
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Fond : indication zone centrale (très atténuée)
  for (int x = MATRIX_W / 2 - 2; x <= MATRIX_W / 2 + 2; x++) {
    drawColumnScaled(x, CRGB(0, 0, 20), 255);
  }

  // Train
  if (train.active) {
    for (int c = 0; c < TRAIN_COLS; c++) {
      int col = (int)train.pos + c;
      if (train.validated[c]) {
        // Colonne validée : pulse blanc très atténué
        drawColumnScaled(col, CRGB::White, 30);
      } else {
        // Atténuation de queue → tête
        uint8_t scale = (c == TRAIN_COLS - 1) ? 255 : (c == TRAIN_COLS - 2) ? 160 : 80;
        drawColumnScaled(col, GAME_COLORS[train.seq[c]], scale);
      }
    }
  }

  // Pixels joueur
  for (int i = 0; i < MAX_PLAYER_PIXELS; i++) {
    if (!playerPixels[i].active) continue;
    int col = (int)playerPixels[i].pos;
    drawPixelPlayer(col, GAME_COLORS[playerPixels[i].colorIdx]);
  }

  // Indicateur vies
  showLivesIndicator();

  FastLED.show();
  delay(8);
}
