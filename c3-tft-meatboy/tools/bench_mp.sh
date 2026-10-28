# usage : bench_mp.sh SLOT SECONDES [DROP_MS]   (COM12 simule un joueur de plus)
SLOT=$1; SEC=$2; DROP=${3:-0}
cd "C:/Users/info/Dropbox/Dossier_AntiFaktory/2026/ClaudeRepo/c3-tft-meatboy"
CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"; FQ=esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app; export ARDUINO_DIRECTORIES_USER="$(pwd -W)/libs"
DEF="#define LOBBY_BENCH\n#define LOBBY_BENCH_SLOT $SLOT\n#define LOBBY_BENCH_MINP 2\n"
python make_proto_ino.py >/dev/null && sed -i "s|^#define FW_VERSION|$DEF#define FW_VERSION|; s|^#define DEBUG_SERIAL 0|#define DEBUG_SERIAL 1|" proto-0-4/proto-0-4.ino
D="$TEMP/lobbymain"; rm -rf "$D"; mkdir -p "$D"; cp c3-tft-meatboy.ino "$D/lobbymain.ino"; cp alto.ino drift.ino timing.ino *.h "$D/"
sed -i "s|^#define FW_VERSION|$DEF#define MP_VIRT 1\n#define MP_VIRT_DROP_MS $DROP\n#define FW_VERSION|; s|^#define DEBUG_SERIAL 0|#define DEBUG_SERIAL 1|" "$D/lobbymain.ino"
"$CLI" compile --fqbn $FQ --build-path "$(cygpath -w $TEMP)/lobbymain_build" "$(cygpath -w $D)" > "$TEMP/lm.log" 2>&1; grep -E " error" "$TEMP/lm.log" | head -5; grep -q "Sketch uses" "$TEMP/lm.log" || exit 1
"$CLI" compile --fqbn $FQ --build-path "$(cygpath -w $TEMP)/proto_bench" proto-0-4 > "$TEMP/lp.log" 2>&1; grep -E " error" "$TEMP/lp.log" | head -5; grep -q "Sketch uses" "$TEMP/lp.log" || exit 1
"$CLI" upload -p COM12 --fqbn $FQ --build-path "$(cygpath -w $TEMP)/lobbymain_build" "$(cygpath -w $D)" 2>&1 | tail -1
"$CLI" upload -p COM15 --fqbn $FQ --build-path "$(cygpath -w $TEMP)/proto_bench" proto-0-4 2>&1 | tail -1
sleep 1
python -c "
import serial,time,threading
def rd(port):
  s=serial.Serial(); s.port=port; s.baudrate=115200; s.timeout=0.5; s.dtr=False; s.rts=False; s.open()
  t=time.time(); last={}
  while time.time()-t<$SEC:
    l=s.readline().decode(errors='ignore').strip()
    tag=l[:4]
    if tag in ('[MP]','[T2]','[D2]','[2J]') or 'Guru' in l or 'panic' in l.lower():
      ts=int(time.time()-t)
      if tag=='[2J]' or 'Guru' in l or 'sorti' in l or ts-last.get(tag,-99)>=3: last[tag]=ts; print(port, ts, l[:200], flush=True)
th=[threading.Thread(target=rd,args=(p,)) for p in ('COM12','COM15')]
[x.start() for x in th]; [x.join() for x in th]
"
