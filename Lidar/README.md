# RPLiDAR A2 — Electron App · Studio EYA

Application Electron pour communiquer avec le capteur SLAMTEC RPLiDAR A2 via port série USB.

## Installation

```bash
npm install
```

> ⚠️ Sur Linux, `serialport` nécessite `build-essential` et `libudev-dev` :
> ```bash
> sudo apt install build-essential libudev-dev
> ```

## Lancement

```bash
npm start
```

## Structure du projet

```
lidar-app/
├── package.json
└── src/
    ├── main.js          # Main process Electron (IPC + SerialPort)
    ├── lidar.js         # Classe LidarA2 — parsing protocole binaire SLAMTEC
    ├── preload.js       # Bridge sécurisé main ↔ renderer
    └── renderer/
        └── index.html   # Interface de diagnostic et connexion
```

## Protocole RPLiDAR A2

Le A2 communique à **115 200 baud** via un protocole binaire :

| Commande      | Hex    | Description              |
|---------------|--------|--------------------------|
| STOP          | `0x25` | Arrêter le scan          |
| SCAN          | `0x20` | Démarrer le scan         |
| GET_INFO      | `0x50` | Infos firmware/matériel  |
| GET_HEALTH    | `0x52` | État de santé du capteur |
| RESET         | `0x40` | Reset hardware           |

### Format paquet scan (5 bytes)

```
Byte 0: [start_flag][inv_start_flag][quality x6]
Byte 1: [angle_lsb x7][check_bit]
Byte 2: angle_msb
Bytes 3-4: distance (little-endian, unité = 0.25 mm)
```

### Événements émis par `LidarA2`

```js
lidar.on('scan',   (points) => { /* points = [{ angle, distance, quality, x, y }] */ });
lidar.on('info',   (info)   => { /* firmware, modèle, numéro de série */ });
lidar.on('health', (health) => { /* status, errorCode */ });
lidar.on('error',  (err)    => { /* objet Error */ });
```

## Étapes suivantes

- **Étape 2** : Visualisation temps réel (Canvas 2D / WebGL)
- **Étape 3** : Détection de présences et zones interactives
