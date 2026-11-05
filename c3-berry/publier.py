# Compile c3-berry et le depose dans ../brique/firmware-berry/ : la page Brique (mode script) le propose en "Installer" (esp-web-tools).
# Memes 4 morceaux et adresses que le firmware principal (publier_firmware_brique.py) : la partition des fichiers (jeux gardes) n'est pas effacee.
# Ensuite : git add brique/firmware-berry + commit + push -> Render le sert.
import glob, json, os, re, shutil, subprocess

ICI = os.path.dirname(os.path.abspath(__file__))
CLI = r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
FQBN = "esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app"
BUILD = os.path.join(os.environ.get("TEMP", ICI), "c3-berry-build")   # hors Dropbox : il verrouille build/ pendant la synchro
DEST = os.path.join(ICI, "..", "brique", "firmware-berry")
version = re.search(r'#define BERRY_VERSION "([^"]+)"', open(os.path.join(ICI, "c3-berry.ino"), encoding="utf-8").read()).group(1)
env = dict(os.environ, ARDUINO_DIRECTORIES_USER=os.path.join(ICI, "..", "c3-tft-meatboy", "libs"))
subprocess.run([CLI, "compile", "--fqbn", FQBN, "--build-path", BUILD, "c3-berry.ino"], cwd=ICI, env=env, check=True)
boot_app0 = sorted(glob.glob(os.path.join(os.environ["LOCALAPPDATA"], "Arduino15", "packages", "esp32", "hardware", "esp32", "*", "tools", "partitions", "boot_app0.bin")))[-1]
parts = [("bootloader.bin", os.path.join(BUILD, "c3-berry.ino.bootloader.bin"), 0x0),
         ("partitions.bin", os.path.join(BUILD, "c3-berry.ino.partitions.bin"), 0x8000),
         ("boot_app0.bin", boot_app0, 0xE000),
         ("console.bin", os.path.join(BUILD, "c3-berry.ino.bin"), 0x10000)]
os.makedirs(DEST, exist_ok=True)
for nom, src, _ in parts:
    shutil.copyfile(src, os.path.join(DEST, nom))
json.dump({"name": "Console Brique (script)", "version": version, "new_install_prompt_erase": False,
           "builds": [{"chipFamily": "ESP32-C3", "parts": [{"path": nom, "offset": off} for nom, _, off in parts]}]},
          open(os.path.join(DEST, "manifest.json"), "w"), indent=1)
print(f"c3-berry {version} pret dans brique/firmware-berry/ : commit + push pour le publier.")
