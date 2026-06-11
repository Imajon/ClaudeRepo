// ============================================================
//  TEST RÉACTIVITÉ — ESP1 (BOUTONS)
//  But : mesurer le temps aller-retour (RTT) ESP-NOW pour
//  chaque appui bouton, et détecter les messages perdus.
//
//  Fonctionnement :
//   - À chaque appui (debounce identique au jeu), on envoie
//     un message PRESS avec un msgId + timestamp (millis()).
//   - ESP2 répond immédiatement par un ACK contenant le même
//     msgId/timestamp.
//   - Ici on calcule le RTT = millis() - timestamp à la
//     réception de l'ACK.
//   - Si aucun ACK ne revient avant ACK_TIMEOUT_MS, le message
//     est compté comme PERDU.
//
//  → Renseigne ESP2_MAC[] avec la MAC affichée par
//    test_esp2_matrix.ino au démarrage.
// ============================================================

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ── À renseigner après avoir flashé ESP2 et relevé sa MAC ──
uint8_t ESP2_MAC[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

#define PIN_RED    12
#define PIN_GREEN  13
#define PIN_BLUE   14

#define DEBOUNCE_MS    30    // identique au jeu
#define ACK_TIMEOUT_MS 200   // au-delà → message considéré perdu

typedef struct {
  uint8_t  type;       // 0 = PRESS, 1 = ACK
  uint8_t  color;      // 0=ROUGE 1=VERT 2=BLEU
  uint32_t msgId;
  uint32_t timestamp;  // millis() de l'émetteur d'origine (ESP1)
} TestMsg;

struct Button {
  uint8_t  pin;
  uint8_t  colorIdx;
  bool     lastRaw;
  bool     state;
  uint32_t lastChange;
};

Button buttons[3] = {
  { PIN_RED,   0, false, false, 0 },
  { PIN_GREEN, 1, false, false, 0 },
  { PIN_BLUE,  2, false, false, 0 },
};

const char* COLOR_NAMES[] = { "ROUGE", "VERT ", "BLEU " };

uint32_t msgIdCounter = 0;

// Suivi de l'unique message en attente d'ACK
bool     waitingAck     = false;
uint32_t pendingMsgId   = 0;
uint32_t pendingSentMs  = 0;
uint8_t  pendingColor   = 0;

// Statistiques
uint32_t totalSent = 0;
uint32_t totalAck  = 0;
uint32_t totalLost = 0;
uint32_t rttMin = 0xFFFFFFFF, rttMax = 0, rttSum = 0;

void onSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("[ESP-NOW] ✗ Échec d'envoi (couche radio)");
  }
}

void sendPress(uint8_t colorIdx) {
  // Si un message précédent attend toujours son ACK, on le compte perdu
  if (waitingAck) {
    totalLost++;
    Serial.printf("[TIMEOUT] msg#%lu (%s) — pas d'ACK reçu\n",
                   pendingMsgId, COLOR_NAMES[pendingColor]);
  }

  TestMsg msg;
  msg.type      = 0; // PRESS
  msg.color     = colorIdx;
  msg.msgId     = ++msgIdCounter;
  msg.timestamp = millis();

  pendingMsgId  = msg.msgId;
  pendingSentMs = msg.timestamp;
  pendingColor  = colorIdx;
  waitingAck    = true;
  totalSent++;

  esp_now_send(ESP2_MAC, (uint8_t *)&msg, sizeof(msg));
  Serial.printf("[SEND]  msg#%-4lu %s  (t=%lu ms)\n",
                 msg.msgId, COLOR_NAMES[colorIdx], msg.timestamp);
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(TestMsg)) return;
  const TestMsg *msg = (const TestMsg *)data;
  if (msg->type != 1) return; // on n'attend que des ACK

  uint32_t now = millis();

  if (waitingAck && msg->msgId == pendingMsgId) {
    uint32_t rtt = now - msg->timestamp;
    waitingAck = false;
    totalAck++;
    if (rtt < rttMin) rttMin = rtt;
    if (rtt > rttMax) rttMax = rtt;
    rttSum += rtt;

    Serial.printf("[ACK]   msg#%-4lu %s  RTT=%4lu ms   | total: envoyés=%lu ack=%lu perdus=%lu | RTT min/avg/max = %lu/%lu/%lu ms\n",
                   msg->msgId, COLOR_NAMES[msg->color], rtt,
                   totalSent, totalAck, totalLost,
                   rttMin, rttSum / totalAck, rttMax);
  } else {
    // ACK tardif (arrivé après le timeout/un nouvel envoi)
    Serial.printf("[ACK]   msg#%-4lu (tardif, ignoré)\n", msg->msgId);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("  TEST RÉACTIVITÉ — ESP1 (Boutons)");
  Serial.println("========================================");

  for (int i = 0; i < 3; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].lastRaw = digitalRead(buttons[i].pin) == LOW;
    buttons[i].state   = buttons[i].lastRaw;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.print("[WiFi] MAC ESP1 : ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] ✗ Init échouée");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, ESP2_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.println("[ESP-NOW] ✓ Peer ESP2 enregistré");
  else
    Serial.println("[ESP-NOW] ✗ Impossible d'enregistrer ESP2");

  Serial.println("Appuie sur les boutons pour tester (Rouge/Vert/Bleu).");
  Serial.println("========================================\n");
}

void loop() {
  uint32_t now = millis();

  for (int i = 0; i < 3; i++) {
    bool raw = (digitalRead(buttons[i].pin) == LOW);

    if (raw != buttons[i].lastRaw) {
      buttons[i].lastRaw    = raw;
      buttons[i].lastChange = now;
    }

    if ((now - buttons[i].lastChange) >= DEBOUNCE_MS) {
      bool prevState = buttons[i].state;
      buttons[i].state = raw;

      if (buttons[i].state && !prevState) {
        sendPress(buttons[i].colorIdx);
      }
    }
  }

  // Timeout : si on attend un ACK depuis trop longtemps, on le signale
  if (waitingAck && (now - pendingSentMs) > ACK_TIMEOUT_MS) {
    totalLost++;
    Serial.printf("[TIMEOUT] msg#%lu (%s) — pas d'ACK après %d ms | total: envoyés=%lu ack=%lu perdus=%lu\n",
                   pendingMsgId, COLOR_NAMES[pendingColor], ACK_TIMEOUT_MS,
                   totalSent, totalAck, totalLost);
    waitingAck = false;
  }
}
