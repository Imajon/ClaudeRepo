const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('lidar', {
  listPorts:  ()      => ipcRenderer.invoke('lidar:list-ports'),
  connect:    (port, baud) => ipcRenderer.invoke('lidar:connect', { portPath: port, baudRate: baud }),
  disconnect: ()      => ipcRenderer.invoke('lidar:disconnect'),
  startMotor: (pwm)   => ipcRenderer.invoke('lidar:start-motor', pwm),
  startScan:  ()      => ipcRenderer.invoke('lidar:start-scan'),
  stopScan:   ()      => ipcRenderer.invoke('lidar:stop-scan'),
  openViz:    ()      => ipcRenderer.invoke('lidar:open-viz'),

  onScan:   (cb) => ipcRenderer.on('lidar:scan',  (_e, d) => cb(d)),
  onInfo:   (cb) => ipcRenderer.on('lidar:info',  (_e, d) => cb(d)),
  onMotor:  (cb) => ipcRenderer.on('lidar:motor', (_e, d) => cb(d)),
  onError:  (cb) => ipcRenderer.on('lidar:error', (_e, m) => cb(m)),
  onHealth: (cb) => ipcRenderer.on('lidar:health',(_e, d) => cb(d)),

  offAll: () => {
    ['lidar:scan','lidar:info','lidar:motor','lidar:error','lidar:health']
      .forEach(ch => ipcRenderer.removeAllListeners(ch));
  },
});
