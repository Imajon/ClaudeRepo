// ============================================================
//  ESP1 — BOUTONS · Dual ESP32 Color Train · Studio EYA
//  Anti-rebond par timestamp, un message par bouton
// ============================================================

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ── À renseigner après avoir flashé ESP2 et relevé sa MAC ──
uint8_t ESP2_MAC[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

#define PIN_RED    12
#define PIN_GREEN  13
#define PIN_BLUE   14

#define DEBOUNCE_MS  30   // ms anti-rebond par bouton

typedef struct {
  uint8_t  buttons;
  uint8_t  gameEvent;
  uint32_t msgId;
} GameMsg;

GameMsg  outMsg;
uint32_t msgId = 0;

// État par bouton
struct Button {
  uint8_t  pin;
  uint8_t  bit;
  bool     lastRaw;      // lecture brute précédente
  bool     state;        // état stable (après debounce)
  uint32_t lastChange;   // timestamp dernier changement brut
};

Button buttons[3] = {
  { PIN_RED,   0, false, false, 0 },
  { PIN_GREEN, 1, false, false, 0 },
  { PIN_BLUE,  2, false, false, 0 },
};

uint32_t lastHB = 0;

void onSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (outMsg.gameEvent == 1)
    Serial.printf("[ESP-NOW] %s msg#%lu\n",
                  status == ESP_NOW_SEND_SUCCESS ? "✓" : "✗", outMsg.msgId);
}

void sendPress(uint8_t bitmask) {
  outMsg.buttons   = bitmask;
  outMsg.gameEvent = 1;
  outMsg.msgId     = ++msgId;
  esp_now_send(ESP2_MAC, (uint8_t *)&outMsg, sizeof(outMsg));
}

void sendHeartbeat(uint8_t state) {
  outMsg.buttons   = state;
  outMsg.gameEvent = 0;
  outMsg.msgId     = ++msgId;
  esp_now_send(ESP2_MAC, (uint8_t *)&outMsg, sizeof(outMsg));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("  ESP1 — Contrôleur Boutons");
  Serial.println("========================================");

  for (int i = 0; i < 3; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].lastRaw = digitalRead(buttons[i].pin) == LOW;
    buttons[i].state   = buttons[i].lastRaw;
  }
  Serial.printf("[GPIO] Boutons : R=%d  G=%d  B=%d\n", PIN_RED, PIN_GREEN, PIN_BLUE);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.print("[WiFi] MAC ESP1 : ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] ✗ Init échouée");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, ESP2_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.println("[ESP-NOW] ✓ Peer ESP2 enregistré");
  else
    Serial.println("[ESP-NOW] ✗ Impossible d'enregistrer ESP2");

  lastHB = millis();
  Serial.println("========================================\n");
}

void loop() {
  uint32_t now = millis();
  uint8_t  stableState = 0;

  for (int i = 0; i < 3; i++) {
    bool raw = (digitalRead(buttons[i].pin) == LOW);

    // Changement brut détecté → on remet le chrono
    if (raw != buttons[i].lastRaw) {
      buttons[i].lastRaw    = raw;
      buttons[i].lastChange = now;
    }

    // Stable depuis DEBOUNCE_MS → on valide
    if ((now - buttons[i].lastChange) >= DEBOUNCE_MS) {
      bool prevState = buttons[i].state;
      buttons[i].state = raw;

      // Front montant = nouvel appui confirmé
      if (buttons[i].state && !prevState) {
        const char* names[] = { "ROUGE", "VERT", "BLEU" };
        Serial.printf("[BTN] Appui → %s\n", names[i]);
        sendPress(1 << buttons[i].bit);   // un message par bouton
      }
    }

    if (buttons[i].state) stableState |= (1 << buttons[i].bit);
  }

  // Heartbeat toutes les 20ms
  if (now - lastHB >= 20) {
    lastHB = now;
    sendHeartbeat(stableState);
  }

  // Pas de delay() — boucle aussi vite que possible pour le debounce
}
