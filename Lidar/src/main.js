const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { LidarA2 } = require('./lidar');

let mainWindow;
let vizWindow;
let lidar;

function broadcast(channel, data) {
  [mainWindow, vizWindow].forEach(win => {
    if (win && !win.isDestroyed()) win.webContents.send(channel, data);
  });
}

function createWindows() {
  mainWindow = new BrowserWindow({
    width: 760, height: 740,
    backgroundColor: '#080810',
    title: 'RPLiDAR A2 — Connexion',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  mainWindow.loadFile(path.join(__dirname, 'renderer/index.html'));
  mainWindow.on('closed', () => { mainWindow = null; });

  vizWindow = new BrowserWindow({
    width: 1280, height: 900,
    backgroundColor: '#000508',
    title: 'RPLiDAR A2 — Visualiseur',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  vizWindow.loadFile(path.join(__dirname, 'renderer/visualizer.html'));
  vizWindow.on('closed', () => { vizWindow = null; });
}

app.whenReady().then(createWindows);
app.on('window-all-closed', () => {
  if (lidar) lidar.stop().catch(() => {});
  if (process.platform !== 'darwin') app.quit();
});

// ─── IPC : lister les ports ──────────────────────────────────────────────────
ipcMain.handle('lidar:list-ports', async () => LidarA2.listPorts());

// ─── IPC : connexion ─────────────────────────────────────────────────────────
ipcMain.handle('lidar:connect', async (_event, { portPath, baudRate }) => {
  try {
    // Nettoyer une instance précédente si elle existe
    if (lidar) {
      try { await lidar.stop(); } catch (_) {}
      lidar = null;
    }

    lidar = new LidarA2(portPath, baudRate || 115200);
    lidar.on('scan',   (pts) => broadcast('lidar:scan', pts));
    lidar.on('info',   (i)   => broadcast('lidar:info', i));
    lidar.on('motor',  (m)   => broadcast('lidar:motor', m));
    lidar.on('error',  (err) => broadcast('lidar:error', err.message || err));
    lidar.on('health', (h)   => broadcast('lidar:health', h));

    // Ouvrir le port
    await lidar.connect();

    // Envoyer STOP d'abord pour réinitialiser l'état du capteur
    // (important si COM3 était déjà actif, ex. après un script de test)
    try {
      await lidar._sendCommand(0x25); // CMD STOP
      console.log('[main] STOP envoyé — attente reset capteur...');
    } catch (_) {}
    await new Promise(r => setTimeout(r, 500));

    // Maintenant GET_INFO (attend la vraie réponse)
    const info = await lidar.getInfo();
    console.log('[main] getInfo OK:', info);

    // Puis GET_HEALTH (attend la vraie réponse)
    const health = await lidar.getHealth();
    console.log('[main] getHealth OK:', health);

    return { success: true };
  } catch (err) {
    console.error('[main] connect erreur:', err.message);
    // Nettoyer en cas d'échec
    if (lidar) {
      try { lidar.port?.close(() => {}); } catch (_) {}
      lidar = null;
    }
    return { success: false, error: err.message };
  }
});

// ─── IPC : démarrer le moteur ────────────────────────────────────────────────
ipcMain.handle('lidar:start-motor', async (_event, pwm) => {
  try {
    if (!lidar) return { success: false, error: 'Non connecté' };
    console.log(`[main] lidar:start-motor pwm=${pwm}`);
    await lidar.startMotor(pwm || 600);
    return { success: true };
  } catch (err) {
    console.error('[main] start-motor erreur:', err.message);
    return { success: false, error: err.message };
  }
});

// ─── IPC : démarrer le scan ──────────────────────────────────────────────────
ipcMain.handle('lidar:start-scan', async () => {
  try {
    if (!lidar) return { success: false, error: 'Non connecté' };
    await lidar.startScan();
    return { success: true };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ─── IPC : arrêter le scan ───────────────────────────────────────────────────
ipcMain.handle('lidar:stop-scan', async () => {
  try {
    if (!lidar) return { success: false, error: 'Non connecté' };
    lidar._scanning = false;
    lidar.state = 'IDLE';
    await lidar._sendCommand(0x25); // CMD STOP
    return { success: true };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ─── IPC : déconnecter tout ──────────────────────────────────────────────────
ipcMain.handle('lidar:disconnect', async () => {
  try {
    if (lidar) {
      await lidar.stop();
      lidar = null;
    }
    broadcast('lidar:motor', { running: false, pwm: 0 });
    return { success: true };
  } catch (err) {
    lidar = null;
    return { success: false, error: err.message };
  }
});

// ─── IPC : ouvrir visualiseur ────────────────────────────────────────────────
ipcMain.handle('lidar:open-viz', async () => {
  if (!vizWindow || vizWindow.isDestroyed()) {
    vizWindow = new BrowserWindow({
      width: 1280, height: 900,
      backgroundColor: '#000508',
      title: 'RPLiDAR A2 — Visualiseur',
      webPreferences: {
        preload: path.join(__dirname, 'preload.js'),
        contextIsolation: true,
        nodeIntegration: false,
      },
    });
    vizWindow.loadFile(path.join(__dirname, 'renderer/visualizer.html'));
    vizWindow.on('closed', () => { vizWindow = null; });
  } else {
    vizWindow.focus();
  }
  return { success: true };
});
