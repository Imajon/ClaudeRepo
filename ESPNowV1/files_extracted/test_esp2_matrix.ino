// ============================================================
//  TEST RÉACTIVITÉ — ESP2 (MATRICE)
//  But : recevoir les messages PRESS d'ESP1 et répondre
//  immédiatement par un ACK (même msgId/timestamp), pour
//  permettre à ESP1 de mesurer le RTT ESP-NOW.
//
//  Affiche aussi en série :
//   - chaque couleur reçue
//   - le délai écoulé depuis le message précédent (pour repérer
//     des paquets qui arrivent "groupés" ou en rafale)
//   - un compteur total + alerte si rien n'est reçu
//
//  → Renseigne ESP1_MAC[] avec la MAC affichée par
//    test_esp1_buttons.ino au démarrage.
//  → Cet ESP2 affiche sa propre MAC : copie-la dans ESP2_MAC[]
//    de test_esp1_buttons.ino.
// ============================================================

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ── À renseigner après avoir flashé ESP1 et relevé sa MAC ──
uint8_t ESP1_MAC[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };

typedef struct {
  uint8_t  type;       // 0 = PRESS, 1 = ACK
  uint8_t  color;      // 0=ROUGE 1=VERT 2=BLEU
  uint32_t msgId;
  uint32_t timestamp;  // millis() de l'émetteur d'origine (ESP1)
} TestMsg;

const char* COLOR_NAMES[] = { "ROUGE", "VERT ", "BLEU " };

uint32_t totalReceived  = 0;
uint32_t lastRecvMs     = 0;
uint32_t lastHeartbeat  = 0;

void onSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("[ESP-NOW] ✗ Échec d'envoi ACK (couche radio)");
  }
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(TestMsg)) {
    Serial.printf("[ESP-NOW] ✗ Taille inattendue : %d (attendu %d)\n", len, (int)sizeof(TestMsg));
    return;
  }
  const TestMsg *msg = (const TestMsg *)data;
  if (msg->type != 0) return; // on n'attend que des PRESS

  uint32_t now = millis();
  uint32_t gap = (totalReceived == 0) ? 0 : (now - lastRecvMs);
  lastRecvMs = now;
  totalReceived++;

  Serial.printf("[RECV]  msg#%-4lu %s  (écart depuis précédent = %4lu ms) | total reçus=%lu\n",
                 msg->msgId, COLOR_NAMES[msg->color], gap, totalReceived);

  // Renvoie immédiatement l'ACK avec le même msgId/timestamp
  TestMsg ack;
  ack.type      = 1; // ACK
  ack.color     = msg->color;
  ack.msgId     = msg->msgId;
  ack.timestamp = msg->timestamp;
  esp_now_send(ESP1_MAC, (uint8_t *)&ack, sizeof(ack));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("  TEST RÉACTIVITÉ — ESP2 (Matrice)");
  Serial.println("========================================");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.print("[WiFi] MAC ESP2 : ");
  Serial.println(WiFi.macAddress());
  Serial.println("       → Copie cette MAC dans ESP2_MAC[] de test_esp1_buttons.ino");

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] ✗ Init échouée — redémarre");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, ESP1_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK)
    Serial.println("[ESP-NOW] ✓ Peer ESP1 enregistré");
  else
    Serial.println("[ESP-NOW] ✗ Impossible d'enregistrer ESP1");

  Serial.println("En attente des appuis bouton...");
  Serial.println("========================================\n");

  lastHeartbeat = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - lastHeartbeat >= 5000) {
    lastHeartbeat = now;
    Serial.printf("[STAT] total reçus = %lu\n", totalReceived);
    if (totalReceived == 0)
      Serial.println("[STAT] ⚠ Aucun message reçu — vérifie ESP1_MAC[] et que ESP1 tourne");
  }
}
