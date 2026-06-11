# RPLiDAR A2 — Document de contexte de développement
**Studio EYA · Mai 2026**

---

## Objectif du projet

Développer une application desktop **Electron + Node.js** pour communiquer avec le capteur lidar **SLAMTEC RPLiDAR A2** branché en USB, dans le cadre d'une **installation interactive**. L'app doit permettre de connecter le capteur, démarrer le moteur, lancer le scan, et visualiser le nuage de points en temps réel.

---

## Stack technique

| Composant | Choix |
|---|---|
| Runtime | Node.js + Electron |
| Communication série | `serialport` v12 |
| Visualisation | Three.js r128 |
| Architecture | Main process (série + IPC) + Renderer (UI + WebGL) |
| Sécurité | `contextIsolation: true`, preload bridge |

---

## Matériel

- **Capteur** : SLAMTEC RPLiDAR A2 (modèle A2M8)
- **Câble** : câble USB officiel SLAMTEC avec bouton switch baud rate
- **Adaptateur USB-Série** : CP2102 (driver CP210x Windows)
- **Port** : COM17 (réassigné depuis COM3, qui était en conflit avec un port Bluetooth virtuel)
- **Baud rate** : **256 000** (switch physique sur le câble → position A2M8)
- **OS** : Windows

---

## Protocole RPLiDAR A2 — résumé

### Commandes binaires

| Commande | Hex | Payload | Description |
|---|---|---|---|
| STOP | `0xA5 0x25` | — | Arrêter le scan |
| RESET | `0xA5 0x40` | — | Reset hardware |
| SCAN | `0xA5 0x20` | — | Démarrer le scan standard |
| GET_INFO | `0xA5 0x50` | — | Infos firmware/matériel |
| GET_HEALTH | `0xA5 0x52` | — | État de santé |
| SET_MOTOR_PWM | `0xA5 0xF0` | 2 bytes LE + checksum | Vitesse moteur (0–1023) |

### Construction SET_MOTOR_PWM
```js
const lo = pwm & 0xFF, hi = (pwm >> 8) & 0xFF;
let cs = 0; cs ^= 0xA5; cs ^= 0xF0; cs ^= 0x02; cs ^= lo; cs ^= hi;
const pkt = Buffer.from([0xA5, 0xF0, 0x02, lo, hi, cs]);
```

### Format descripteur de réponse (7 bytes)
```
[0xA5][0x5A][size32 LE (4 bytes)][data_type]
size32 bits 31-30 = send_mode (0=single, 1=multi)
size32 bits 29-0  = data_length
```

### Format paquet scan (5 bytes par point)
```
Byte 0 : [start_flag b7][inv_start_flag b6][quality b5-0]
Byte 1 : [angle_lsb b7-1][check_bit b0]
Byte 2 : angle_msb
Bytes 3-4 : distance little-endian (unité = 0.25 mm)

angle    = ((byte1 >> 1) | (byte2 << 7)) / 64.0   → degrés
distance = uint16LE(bytes 3-4) / 4.0              → mm
```

### Identifiant appareil confirmé
```
Modèle    : 0x2C
Firmware  : 1.32
Hardware  : 6
Série     : B8ECED93C0EA98C9A5E698F22D324669
```

---

## Architecture de l'application

```
lidar-app/
├── package.json
└── src/
    ├── main.js          ← Main process : IPC handlers, broadcast
    ├── lidar.js         ← Classe LidarA2 : parsing, moteur, scan
    ├── preload.js       ← Bridge contextIsolation main ↔ renderer
    └── renderer/
        ├── index.html   ← Interface connexion (3 étapes)
        └── visualizer.html ← Visualiseur Three.js
```

### Flux IPC

```
Renderer → ipcRenderer.invoke('lidar:list-ports')
Renderer → ipcRenderer.invoke('lidar:connect', { portPath, baudRate })
Renderer → ipcRenderer.invoke('lidar:start-motor', pwm)
Renderer → ipcRenderer.invoke('lidar:start-scan')
Renderer → ipcRenderer.invoke('lidar:stop-scan')
Renderer → ipcRenderer.invoke('lidar:disconnect')

Main → webContents.send('lidar:scan',   points[])
Main → webContents.send('lidar:info',   info)
Main → webContents.send('lidar:motor',  { running, pwm })
Main → webContents.send('lidar:error',  message)
Main → webContents.send('lidar:health', { status, errorCode })
```

### Broadcast double fenêtre
Le main process diffuse tous les events lidar simultanément vers la fenêtre de connexion (`index.html`) et le visualiseur (`visualizer.html`).

---

## Flux de démarrage — 3 étapes obligatoires

```
① CONNECTER
   → SerialPort.open(COM3, 256000)
   → RTS = true (init immédiate à l'ouverture)
   → GET_INFO  (27 bytes de réponse)
   → GET_HEALTH (10 bytes de réponse)

② DÉMARRER LE MOTEUR  ← étape critique
   → port.set({ dtr: false, rts: true })  ← active le moteur (signal RTS)
   → attendre 300 ms
   → SET_MOTOR_PWM(600)
   → attendre 1000 ms (montée en vitesse)

③ LANCER LE SCAN
   → CMD SCAN (0xA5 0x20)
   → parser le flux de paquets 5 bytes en continu
   → émettre 'scan' à chaque tour complet (~10 Hz)
```

---

## ⚠️ Mise à jour (juin 2026) — état actuel FONCTIONNEL

L'app fonctionne de bout en bout (connexion, moteur, scan, visualiseur 3D avec points
correctement placés). Trois bugs supplémentaires ont été trouvés et corrigés après les
versions v1–v11c ci-dessous :

### 1. Port COM3 en conflit (matériel/Windows, pas un bug de code)
Le CP210x du lidar (USB\VID_10C4&PID_EA60\...) partageait COM3 avec un port Bluetooth
virtuel "Lien série sur Bluetooth standard". Le port Bluetooth s'ouvrait sans erreur
mais ne renvoyait jamais de données → connexion "réussie" mais GET_INFO bloqué indéfiniment.
**Solution** : Gestionnaire de périphériques → CP210x → Propriétés → Paramètres du port →
Avancé → réassigner sur un port libre (ex. COM17), puis débrancher/rebrancher l'USB.
**→ Le lidar est maintenant sur COM17 (256000 baud, switch câble sur position 256000).**

### 2. La technique close/reopen pour RTS ne fonctionnait plus
Sur COM17, le close/reopen du port (solution v10 ci-dessous) ne faisait plus tourner
le moteur. Remplacé par un `port.set({ dtr: false, rts: true })` **explicite** dans
`startMotor()` — confirmé par `lidar-a2-diag-v2.js` (RTS seul → 39082 bytes reçus).
`stopMotor()` repasse `rts: false`.

### 3. Aucun point affiché dans le visualiseur (bug de parsing le plus subtil)
Dans `_parseScanPacket()` et dans la resynchronisation de `_process()` (état READ_SCAN),
les bits `start_flag` / `inverted_start_flag` / `quality` du byte 0 étaient extraits en
MSB-first (`(pkt[0] >> 7) & 1`, etc.) alors qu'ils sont en réalité **LSB-first** :

```js
const startFlag         = pkt[0] & 0x01;
const invertedStartFlag = (pkt[0] >> 1) & 0x01;
const quality           = (pkt[0] >> 2) & 0x3F;
```

Avec l'ancien ordre, `startFlag === invertedStartFlag` était presque toujours vrai même
sur un flux parfaitement aligné (stride 5 octets, offset 0, vérifié à 100% par capture
brute), donc **tous les paquets étaient silencieusement rejetés** → 0 point émis, malgré
un moteur qui tournait et des données qui arrivaient bien sur le port.

Egalement ajouté : resynchronisation octet par octet dans `_process()` (READ_SCAN) si
`startFlag === invertedStartFlag` ou `checkBit !== 1`, et détection de tour complet par
retour arrière de l'angle (`angle < this._lastAngle - 180`) en plus du bit start_flag.

**Résultat validé** : ~9-10 tours/s, ~150-300 points/tour, distances cohérentes (±0.5-5m).

---

## Problème principal résolu : le moteur ne démarrait pas (historique v1-v11c)

### Chronologie du diagnostic

**Symptôme initial** : le moteur ne tournait pas malgré l'envoi de `SET_MOTOR_PWM` et `RTS = true`.

**Script de diagnostic v1** (`lidar-a2-diag.js`) : a confirmé que le baud rate 256 000 est correct (GET_INFO répond avec 27 bytes), mais CMD SCAN ne reçoit que 7 bytes (le descripteur uniquement, sans données de scan → moteur inactif).

**Script de diagnostic v2** (`lidar-a2-diag-v2.js`) : teste DTR seul, RTS seul, DTR+RTS séparément. Résultats :
- DTR seul → **4 bytes** → moteur mort
- RTS seul → **39 087 bytes** → ✓ moteur actif
- DTR + RTS → **39 082 bytes** → ✓ moteur actif

**Conclusion du diagnostic** : RTS est le signal correct. Mais dans l'app Electron, `RTS = true` ne fait rien.

### Cause racine identifiée

Le driver **CP210x sur Windows** remet `RTS = false` silencieusement après les échanges `GET_INFO` et `GET_HEALTH`. `port.set({ rts: true })` s'exécute sans erreur mais n'a aucun effet physique car le driver l'écrase lors du prochain échange.

**Preuve** : le script de diagnostic ferme et rouvre le port entre chaque test (`port.close()` → `port.open()`). À chaque réouverture, le driver CP210x initialise `RTS = true` par défaut — c'est ce comportement d'init qui fait tourner le moteur, pas l'appel explicite `port.set({ rts: true })`.

### Solution

Fermer et rouvrir le port dans `startMotor()` :

```js
async startMotor(pwm = 600) {
  // 1. Fermer le port (état CP210x remis à zéro)
  await new Promise(res => this.port.close(res));
  await new Promise(r => setTimeout(r, 200));

  // 2. Rouvrir → driver CP210x initialise RTS=true automatiquement
  await new Promise((res, rej) => this.port.open(err => err ? rej(err) : res()));
  await new Promise(r => setTimeout(r, 300));

  // 3. SET_MOTOR_PWM pour fixer la vitesse
  const lo = pwm & 0xFF, hi = (pwm >> 8) & 0xFF;
  let cs = 0; cs ^= 0xA5; cs ^= 0xF0; cs ^= 0x02; cs ^= lo; cs ^= hi;
  const pkt = Buffer.from([0xA5, 0xF0, 0x02, lo, hi, cs]);
  await new Promise(res => this.port.write(pkt, res));

  // 4. Laisser le moteur monter en vitesse
  await new Promise(r => setTimeout(r, 1000));
}
```

---

## Erreurs à éviter

| Erreur | Conséquence | Correction |
|---|---|---|
| Envoyer `CMD STOP` avant `startMotor` | Bloque `SET_MOTOR_PWM` | Ne pas envoyer STOP avant le moteur |
| Utiliser DTR au lieu de RTS | Moteur mort | Signal moteur = RTS sur ce câble |
| Baud rate 115 200 avec switch sur 256 000 | GET_INFO ne répond pas | Aligner switch physique et config logicielle |
| Lancer CMD SCAN sans moteur tournant | Descripteur reçu (7 bytes) mais pas de données | Toujours démarrer moteur avant scan |
| Extraire start_flag/inv_start_flag/quality en MSB-first (`>>7`, `>>6`, `&0x3F`) | Tous les paquets scan rejetés silencieusement, 0 point émis | Ces bits sont LSB-first : `b&1`, `(b>>1)&1`, `(b>>2)&0x3F` |
| Deux périphériques série mappés sur le même port COM (ex. CP210x + Bluetooth virtuel) | Le port "s'ouvre" mais ne répond jamais (GET_INFO bloqué) | Vérifier `Get-PnpDevice`, réassigner le CP210x sur un port libre via Gestionnaire de périphériques |

---

## Visualiseur Three.js

- **Buffer WebGL** : `THREE.Points` + `BufferGeometry`, 4096 points max
- **Couleur par distance** :
  - `< 1 m` → cyan `#00ffe7`
  - `1–3 m` → vert `#00ff88`
  - `3–6 m` → vert-jaune `#88ff00`
  - `6–10 m` → amber `#ffaa00`
  - `> 10 m` → rouge-orange `#ff4400`
- **Traîne** : 8 tours précédents en buffer circulaire, opacité décroissante
- **Sweep line** : ligne animée qui suit l'angle du dernier point reçu
- **Cercles** : 1 m, 3 m, 5 m, 10 m affichés en repères
- **Contrôles** : TOP (vue dessus) / 3D (orbite) / TRAÎNE / GRILLE / PAUSE

---

## Versions livrées

| Version | Contenu |
|---|---|
| v1 | Connexion série + parsing protocole binaire SLAMTEC |
| v2 | Visualiseur Three.js + double fenêtre |
| v3 | Flux 3 étapes UI (connecter / moteur / scan) |
| v4 | Sélecteur baud rate 115 200 / 256 000 dans l'interface |
| v5–v9 | Itérations diagnostic moteur (RTS, DTR, séquences) |
| v10 | Close/reopen port dans startMotor (fonctionnait sur COM3, plus sur COM17) |
| v11 | **Solution actuelle, validée bout en bout** : `port.set({dtr:false, rts:true})` explicite pour le moteur, correction de l'ordre des bits (LSB-first) du paquet scan + resynchronisation octet par octet, port réassigné sur COM17 |

---

## Prochaine étape prévue

**Étape 3** : détection de présences et zones interactives
- Zones polygonales configurables
- Tracking de personnes (clustering de points)
- Événements déclenchables pour l'installation (OSC, WebSocket, callbacks)
