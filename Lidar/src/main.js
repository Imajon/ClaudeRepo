const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const fs   = require('fs');
const { LidarA2 }  = require('./lidar');
const { OSCSender } = require('./osc');

let mainWindow, vizWindow, lidar;

// ─── Persistance fichier (userData, survit aux crashs) ───────────────────────
function settingsPath() {
  return path.join(app.getPath('userData'), 'lidar-settings.json');
}
function loadSettingsFromDisk() {
  try {
    const p = settingsPath();
    if (fs.existsSync(p)) return JSON.parse(fs.readFileSync(p, 'utf8'));
  } catch (e) {}
  return null;
}
function saveSettingsToDisk(settings) {
  try { fs.writeFileSync(settingsPath(), JSON.stringify(settings, null, 2), 'utf8'); }
  catch (e) { console.error('[main] saveSettings error:', e.message); }
}

// ─── OSC ─────────────────────────────────────────────────────────────────────
const oscSender = new OSCSender();
let oscConfig   = { enabled: false, host: '127.0.0.1', port: 9000 };

function sendBlobsOSC(blobs) {
  if (!oscConfig.enabled || !blobs.length) return;
  const { host, port } = oscConfig;
  blobs.forEach(b => {
    oscSender.send(host, port, '/lidar/blob', [
      { type: 'i', value: b.id },
      { type: 'f', value: b.x },
      { type: 'f', value: b.z },
      { type: 'f', value: b.radius },
    ]);
  });
  oscSender.send(host, port, '/lidar/blobs/count', [{ type: 'i', value: blobs.length }]);
}

// ─── Broadcast vers les deux fenêtres ────────────────────────────────────────
function broadcast(channel, data) {
  [mainWindow, vizWindow].forEach(win => {
    if (win && !win.isDestroyed()) win.webContents.send(channel, data);
  });
}

// ─── Création des fenêtres ───────────────────────────────────────────────────
function createWindows() {
  mainWindow = new BrowserWindow({
    width: 760, height: 740,
    backgroundColor: '#080810',
    title: 'RPLiDAR A2 — Connexion',
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false },
  });
  mainWindow.loadFile(path.join(__dirname, 'renderer/index.html'));
  mainWindow.on('closed', () => { mainWindow = null; });

  vizWindow = new BrowserWindow({
    width: 1280, height: 900,
    backgroundColor: '#000508',
    title: 'RPLiDAR A2 — Visualiseur',
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false },
  });
  vizWindow.loadFile(path.join(__dirname, 'renderer/visualizer.html'));
  vizWindow.on('closed', () => { vizWindow = null; });
}

app.whenReady().then(createWindows);
app.on('window-all-closed', () => {
  if (lidar) lidar.stop().catch(() => {});
  oscSender.close();
  if (process.platform !== 'darwin') app.quit();
});

// ─── IPC : persistance settings (survit aux crashs) ──────────────────────────
ipcMain.handle('lidar:save-settings', (_event, settings) => {
  saveSettingsToDisk(settings);
  // Synchroniser oscConfig si la config OSC est dans les settings
  if (settings && settings.osc) {
    oscConfig = { ...oscConfig, ...settings.osc };
  }
  return { success: true };
});

ipcMain.handle('lidar:load-settings', () => {
  return { success: true, settings: loadSettingsFromDisk() };
});

// ─── IPC : configuration OSC ─────────────────────────────────────────────────
ipcMain.handle('lidar:osc-config', (_event, cfg) => {
  oscConfig = { enabled: !!(cfg && cfg.enabled), host: (cfg && cfg.host) || '127.0.0.1', port: (cfg && Number(cfg.port)) || 9000 };
  return { success: true, config: oscConfig };
});

// ─── IPC : blobs → OSC ───────────────────────────────────────────────────────
ipcMain.on('lidar:blobs', (_event, blobs) => {
  sendBlobsOSC(Array.isArray(blobs) ? blobs : []);
});

// ─── IPC : lister les ports ──────────────────────────────────────────────────
ipcMain.handle('lidar:list-ports', async () => LidarA2.listPorts());

// ─── IPC : connexion ─────────────────────────────────────────────────────────
ipcMain.handle('lidar:connect', async (_event, { portPath, baudRate }) => {
  try {
    if (lidar) { try { await lidar.stop(); } catch (_) {} lidar = null; }
    lidar = new LidarA2(portPath, baudRate || 115200);
    lidar.on('scan',   pts => broadcast('lidar:scan', pts));
    lidar.on('info',   i   => broadcast('lidar:info', i));
    lidar.on('motor',  m   => broadcast('lidar:motor', m));
    lidar.on('error',  err => broadcast('lidar:error', err.message || err));
    lidar.on('health', h   => broadcast('lidar:health', h));
    await lidar.connect();
    try { await lidar._sendCommand(0x25); } catch (_) {}
    await new Promise(r => setTimeout(r, 500));
    const info = await lidar.getInfo();
    const health = await lidar.getHealth();
    console.log('[main] connect OK', info, health);
    return { success: true };
  } catch (err) {
    console.error('[main] connect error:', err.message);
    if (lidar) { try { lidar.port?.close(() => {}); } catch (_) {} lidar = null; }
    return { success: false, error: err.message };
  }
});

// ─── IPC : moteur ────────────────────────────────────────────────────────────
ipcMain.handle('lidar:start-motor', async (_event, pwm) => {
  try {
    if (!lidar) return { success: false, error: 'Non connecté' };
    await lidar.startMotor(pwm || 600);
    return { success: true };
  } catch (err) { return { success: false, error: err.message }; }
});

// ─── IPC : scan ──────────────────────────────────────────────────────────────
ipcMain.handle('lidar:start-scan', async () => {
  try {
    if (!lidar) return { success: false, error: 'Non connecté' };
    await lidar.startScan();
    return { success: true };
  } catch (err) { return { success: false, error: err.message }; }
});

ipcMain.handle('lidar:stop-scan', async () => {
  try {
    if (!lidar) return { success: false, error: 'Non connecté' };
    lidar._scanning = false; lidar.state = 'IDLE';
    await lidar._sendCommand(0x25);
    return { success: true };
  } catch (err) { return { success: false, error: err.message }; }
});

// ─── IPC : déconnexion ───────────────────────────────────────────────────────
ipcMain.handle('lidar:disconnect', async () => {
  try {
    if (lidar) { await lidar.stop(); lidar = null; }
    broadcast('lidar:motor', { running: false, pwm: 0 });
    return { success: true };
  } catch (err) { lidar = null; return { success: false, error: err.message }; }
});

// ─── IPC : ouvrir visualiseur ────────────────────────────────────────────────
ipcMain.handle('lidar:open-viz', async () => {
  if (!vizWindow || vizWindow.isDestroyed()) {
    vizWindow = new BrowserWindow({
      width: 1280, height: 900, backgroundColor: '#000508', title: 'RPLiDAR A2 — Visualiseur',
      webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false },
    });
    vizWindow.loadFile(path.join(__dirname, 'renderer/visualizer.html'));
    vizWindow.on('closed', () => { vizWindow = null; });
  } else { vizWindow.focus(); }
  return { success: true };
});
