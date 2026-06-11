const { SerialPort } = require('serialport');
const EventEmitter = require('events');

const SYNC_BYTE    = 0xA5;
const SYNC_BYTE2   = 0x5A;

const CMD = {
  STOP:          0x25,
  RESET:         0x40,
  SCAN:          0x20,
  GET_INFO:      0x50,
  GET_HEALTH:    0x52,
  SET_MOTOR_PWM: 0xF0,
};

const MOTOR_PWM_DEFAULT = 600;
const DESCRIPTOR_SIZE   = 7;
const SCAN_PACKET_SIZE  = 5;

const PARSER_STATE = {
  IDLE:            'IDLE',
  WAIT_DESCRIPTOR: 'WAIT_DESCRIPTOR',
  READ_DESCRIPTOR: 'READ_DESCRIPTOR',
  READ_RESPONSE:   'READ_RESPONSE',
  READ_SCAN:       'READ_SCAN',
};

class LidarA2 extends EventEmitter {
  constructor(portPath, baudRate = 115200) {
    super();
    this.portPath  = portPath;
    this.baudRate  = baudRate;
    this._motorPwm = 0;
    this.port = null;
    this.buffer = Buffer.alloc(0);
    this.state = PARSER_STATE.IDLE;
    this.descriptor = null;
    this.pendingCommand = null;
    this._scanning = false;
    this._scanPoints = [];
    this._lastAngle = -1;
    this._totalRx = 0; // compteur debug
  }

  static async listPorts() {
    const ports = await SerialPort.list();
    return ports.map(p => ({
      path: p.path,
      manufacturer: p.manufacturer || '',
      serialNumber: p.serialNumber || '',
      vendorId: p.vendorId || '',
      productId: p.productId || '',
    }));
  }

  connect() {
    return new Promise((resolve, reject) => {
      this.port = new SerialPort({
        path: this.portPath,
        baudRate: this.baudRate,
        autoOpen: false,
      });

      this.port.open((err) => {
        if (err) return reject(new Error(`Impossible d'ouvrir ${this.portPath}: ${err.message}`));
        console.log(`[LidarA2] Port ouvert sur ${this.portPath} @ ${this.baudRate}`);
        this._attachDataListener();
        resolve();
      });
    });
  }

  _attachDataListener() {
    this.port.removeAllListeners('data');
    this.port.removeAllListeners('error');
    this.port.on('data',  (data) => this._onData(data));
    this.port.on('error', (err)  => this.emit('error', err));
    console.log('[LidarA2] Listener data attaché');
  }

  // ─── GET_INFO — approche simple comme le script diag ────────────────────
  // On écoute les bytes bruts pendant 1s après l'envoi de la commande,
  // puis on parse manuellement le buffer résultant.
  getInfo() {
    return new Promise((resolve, reject) => {
      let rxBuf = Buffer.alloc(0);
      let settled = false;

      const rawListener = (chunk) => {
        rxBuf = Buffer.concat([rxBuf, chunk]);
        console.log(`[LidarA2] getInfo RX ${chunk.length} bytes, total=${rxBuf.length}, hex=${[...chunk].map(b=>b.toString(16).padStart(2,'0')).join(' ')}`);
      };

      const done = () => {
        if (settled) return;
        settled = true;
        this.port.removeListener('data', rawListener);

        console.log(`[LidarA2] getInfo total reçu: ${rxBuf.length} bytes`);

        if (rxBuf.length < 27) {
          reject(new Error(`GET_INFO: seulement ${rxBuf.length} bytes reçus (attendu 27)`));
          return;
        }

        // Trouver le descripteur A5 5A dans le buffer
        let offset = -1;
        for (let i = 0; i < rxBuf.length - 1; i++) {
          if (rxBuf[i] === 0xA5 && rxBuf[i+1] === 0x5A) { offset = i; break; }
        }
        if (offset === -1) {
          reject(new Error(`GET_INFO: descripteur A5 5A introuvable dans ${[...rxBuf].map(b=>b.toString(16).padStart(2,'0')).join(' ')}`));
          return;
        }

        const data = rxBuf.slice(offset + 7); // sauter les 7 bytes de descripteur
        if (data.length < 20) {
          reject(new Error(`GET_INFO: données trop courtes (${data.length} bytes après descripteur)`));
          return;
        }

        const info = {
          model:         data[0],
          firmwareMinor: data[1],
          firmwareMajor: data[2],
          hardware:      data[3],
          serialNumber:  data.slice(4, 20).toString('hex').toUpperCase(),
        };
        console.log('[LidarA2] Info:', info);
        this.emit('info', info);
        resolve(info);
      };

      // Attacher le listener AVANT d'envoyer la commande
      this.port.on('data', rawListener);

      this._sendCommand(CMD.GET_INFO).then(() => {
        // Attendre 1s que les bytes arrivent (comme le script diag attend 500ms)
        setTimeout(done, 1000);
      }).catch(err => {
        settled = true;
        this.port.removeListener('data', rawListener);
        reject(err);
      });
    });
  }

  // ─── GET_HEALTH — même approche simple ──────────────────────────────────
  getHealth() {
    return new Promise((resolve, reject) => {
      let rxBuf = Buffer.alloc(0);
      let settled = false;

      const rawListener = (chunk) => {
        rxBuf = Buffer.concat([rxBuf, chunk]);
        console.log(`[LidarA2] getHealth RX ${chunk.length} bytes, total=${rxBuf.length}`);
      };

      const done = () => {
        if (settled) return;
        settled = true;
        this.port.removeListener('data', rawListener);

        console.log(`[LidarA2] getHealth total reçu: ${rxBuf.length} bytes`);

        if (rxBuf.length < 10) {
          // Health pas critique — on résout quand même avec un état par défaut
          console.warn(`[LidarA2] getHealth: seulement ${rxBuf.length} bytes, on continue`);
          resolve({ status: 'Unknown', errorCode: 0 });
          return;
        }

        let offset = -1;
        for (let i = 0; i < rxBuf.length - 1; i++) {
          if (rxBuf[i] === 0xA5 && rxBuf[i+1] === 0x5A) { offset = i; break; }
        }

        if (offset === -1 || rxBuf.length < offset + 10) {
          resolve({ status: 'Unknown', errorCode: 0 });
          return;
        }

        const data = rxBuf.slice(offset + 7);
        const statusCode = data[0];
        const statusMap  = { 0: 'Good', 1: 'Warning', 2: 'Error' };
        const health = {
          status:    statusMap[statusCode] || 'Unknown',
          errorCode: data.readUInt16LE(1),
        };
        console.log('[LidarA2] Health:', health);
        this.emit('health', health);
        resolve(health);
      };

      this.port.on('data', rawListener);

      this._sendCommand(CMD.GET_HEALTH).then(() => {
        setTimeout(done, 800);
      }).catch(err => {
        settled = true;
        this.port.removeListener('data', rawListener);
        reject(err);
      });
    });
  }

  async startMotor(pwm = MOTOR_PWM_DEFAULT) {
    this._motorPwm = pwm;
    console.log(`[LidarA2] startMotor — activation RTS`);

    this._scanning = false;
    this.state = PARSER_STATE.IDLE;
    this.buffer = Buffer.alloc(0);

    await new Promise((res, rej) => {
      this.port.set({ dtr: false, rts: true }, err => err ? rej(err) : res());
    });
    console.log('[LidarA2] RTS=true');

    await new Promise(r => setTimeout(r, 300));

    const lo = pwm & 0xFF, hi = (pwm >> 8) & 0xFF;
    let cs = 0; cs ^= 0xA5; cs ^= 0xF0; cs ^= 0x02; cs ^= lo; cs ^= hi;
    const pkt = Buffer.from([0xA5, 0xF0, 0x02, lo, hi, cs]);
    await new Promise(res => this.port.write(pkt, (err) => {
      if (err) console.warn('[LidarA2] SET_MOTOR_PWM err:', err.message);
      else     console.log(`[LidarA2] SET_MOTOR_PWM(${pwm}) envoyé`);
      res();
    }));

    await new Promise(r => setTimeout(r, 1000));

    console.log(`[LidarA2] Moteur démarré (PWM=${pwm})`);
    this.emit('motor', { running: true, pwm, baudRate: this.baudRate });
  }

  async stopMotor() {
    try {
      await this._sendCommand(CMD.SET_MOTOR_PWM, [0x00, 0x00]);
    } catch (_) {}
    await new Promise(r => setTimeout(r, 100));
    if (this.port && this.port.isOpen) {
      await new Promise(res => this.port.set({ rts: false }, () => res()));
    }
    this._motorPwm = 0;
    console.log('[LidarA2] Moteur arrêté');
    this.emit('motor', { running: false, pwm: 0 });
  }

  _sendCommand(cmd, payload = null) {
    let packet;
    if (payload && payload.length > 0) {
      const size = payload.length;
      let checksum = 0;
      checksum ^= SYNC_BYTE;
      checksum ^= cmd;
      checksum ^= size;
      for (const b of payload) checksum ^= b;
      packet = Buffer.from([SYNC_BYTE, cmd, size, ...payload, checksum]);
    } else {
      packet = Buffer.from([SYNC_BYTE, cmd]);
    }

    return new Promise((resolve, reject) => {
      if (!this.port || !this.port.isOpen) {
        return reject(new Error('Port non ouvert'));
      }
      console.log(`[LidarA2] TX: ${[...packet].map(b=>b.toString(16).padStart(2,'0')).join(' ')}`);
      this.port.write(packet, (err) => {
        if (err) return reject(err);
        resolve();
      });
    });
  }

  async startScan() {
    // Reprendre le parser d'état pour le scan continu
    this.pendingCommand = CMD.SCAN;
    this.state = PARSER_STATE.WAIT_DESCRIPTOR;
    this.buffer = Buffer.alloc(0);
    this._scanning = true;
    this._scanPoints = [];
    this._lastAngle = -1;

    // Réattacher le listener qui alimente le parser
    this._attachDataListener();

    await this._sendCommand(CMD.SCAN);
    console.log('[LidarA2] Scan démarré');
  }

  async stop() {
    this._scanning = false;
    this.state = PARSER_STATE.IDLE;
    if (this.port && this.port.isOpen) {
      try { await this._sendCommand(CMD.STOP); } catch (_) {}
      await new Promise(r => setTimeout(r, 100));
      await this.stopMotor();
      await new Promise(r => setTimeout(r, 100));
      return new Promise((resolve) => {
        this.port.close(() => {
          console.log('[LidarA2] Port fermé');
          resolve();
        });
      });
    }
  }

  _onData(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    this._process();
  }

  _process() {
    while (this.buffer.length > 0) {
      switch (this.state) {

        case PARSER_STATE.IDLE: {
          const idx = this.buffer.indexOf(SYNC_BYTE);
          if (idx === -1) { this.buffer = Buffer.alloc(0); return; }
          this.buffer = this.buffer.slice(idx);
          this.state = PARSER_STATE.WAIT_DESCRIPTOR;
          break;
        }

        case PARSER_STATE.WAIT_DESCRIPTOR:
          if (this.buffer.length < DESCRIPTOR_SIZE) return;
          this.state = PARSER_STATE.READ_DESCRIPTOR;
          break;

        case PARSER_STATE.READ_DESCRIPTOR: {
          if (this.buffer.length < DESCRIPTOR_SIZE) return;

          if (this.buffer[0] !== SYNC_BYTE || this.buffer[1] !== SYNC_BYTE2) {
            this.buffer = this.buffer.slice(1);
            this.state = PARSER_STATE.WAIT_DESCRIPTOR;
            break;
          }

          const size32   = this.buffer.readUInt32LE(2);
          const sendMode = (size32 >> 30) & 0x03;
          const dataLen  = size32 & 0x3FFFFFFF;
          const dataType = this.buffer[6];

          this.descriptor = { sendMode, dataLen, dataType };
          this.buffer = this.buffer.slice(DESCRIPTOR_SIZE);

          console.log(`[LidarA2] Descripteur: type=0x${dataType.toString(16)}, len=${dataLen}, mode=${sendMode}`);

          this.state = (sendMode === 0)
            ? PARSER_STATE.READ_RESPONSE
            : PARSER_STATE.READ_SCAN;
          break;
        }

        case PARSER_STATE.READ_RESPONSE: {
          if (!this.descriptor) { this.state = PARSER_STATE.IDLE; break; }
          if (this.buffer.length < this.descriptor.dataLen) return;

          const respData = this.buffer.slice(0, this.descriptor.dataLen);
          this.buffer = this.buffer.slice(this.descriptor.dataLen);

          this._handleResponse(this.descriptor.dataType, respData);

          this.state = this._scanning
            ? PARSER_STATE.WAIT_DESCRIPTOR
            : PARSER_STATE.IDLE;
          break;
        }

        case PARSER_STATE.READ_SCAN: {
          if (this.buffer.length < SCAN_PACKET_SIZE) return;
          // Vérifier l'alignement avant de consommer 5 octets : si invalide,
          // décaler d'1 octet pour resynchroniser sur le flux.
          const startFlag = this.buffer[0] & 0x01;
          const invStart  = (this.buffer[0] >> 1) & 0x01;
          const checkBit  = this.buffer[1] & 0x01;
          if (startFlag === invStart || checkBit !== 1) {
            this.buffer = this.buffer.slice(1);
            break;
          }
          const pkt = this.buffer.slice(0, SCAN_PACKET_SIZE);
          this.buffer = this.buffer.slice(SCAN_PACKET_SIZE);
          this._parseScanPacket(pkt);
          break;
        }

        default:
          this.state = PARSER_STATE.IDLE;
          break;
      }
    }
  }

  _handleResponse(dataType, data) {
    switch (dataType) {
      case 0x04: {
        const info = {
          model:         data[0],
          firmwareMinor: data[1],
          firmwareMajor: data[2],
          hardware:      data[3],
          serialNumber:  data.slice(4, 20).toString('hex').toUpperCase(),
        };
        console.log('[LidarA2] Info:', info);
        this.emit('info', info);
        break;
      }
      case 0x06: {
        const statusCode = data[0];
        const statusMap  = { 0: 'Good', 1: 'Warning', 2: 'Error' };
        const health = {
          status:    statusMap[statusCode] || 'Unknown',
          errorCode: data.readUInt16LE(1),
        };
        console.log('[LidarA2] Health:', health);
        this.emit('health', health);
        break;
      }
      default:
        console.log(`[LidarA2] Réponse inconnue type=0x${dataType.toString(16)}`);
    }
  }

  _parseScanPacket(pkt) {
    const startFlag         = pkt[0] & 0x01;
    const invertedStartFlag = (pkt[0] >> 1) & 0x01;
    const quality           = (pkt[0] >> 2) & 0x3F;
    const checkBit          = pkt[1] & 0x01;
    const angleRaw          = ((pkt[1] >> 1) | (pkt[2] << 7));
    const distanceRaw       = pkt.readUInt16LE(3);

    if (startFlag === invertedStartFlag) return;
    if (checkBit !== 1) return;

    const angle    = angleRaw / 64.0;
    const distance = distanceRaw / 4.0;

    const point = {
      angle, distance, quality,
      x: distance * Math.cos((angle * Math.PI) / 180),
      y: distance * Math.sin((angle * Math.PI) / 180),
    };

    // Détection de tour complet par retour arrière de l'angle (plus robuste
    // que le bit start_flag, qui peut être manqué/dupliqué sur certains paquets).
    if (this._lastAngle >= 0 && angle < this._lastAngle - 180) {
      if (this._scanPoints.length > 0) {
        this.emit('scan', [...this._scanPoints]);
      }
      this._scanPoints = [];
    }

    if (distance > 0 && quality > 0) {
      this._scanPoints.push(point);
    }

    this._lastAngle = angle;
  }
}

module.exports = { LidarA2 };
