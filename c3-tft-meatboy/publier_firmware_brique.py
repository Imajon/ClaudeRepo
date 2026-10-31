# Compile le firmware et le depose dans ../brique/firmware/ : la page Brique le propose en "Mettre a jour ma console" (esp-web-tools).
# 4 morceaux aux adresses de l'upload Arduino, PAS le .merged.bin (4 Mo, il effacerait aussi "Mes niveaux").
# NB : la progression repart quand meme de zero, voulu : le firmware efface la NVS quand FW_VERSION change (setup).
# Ensuite : git add brique/firmware + commit + push -> Render sert la nouvelle version a tout le monde.
import glob, json, os, re, shutil, subprocess, sys

ICI = os.path.dirname(os.path.abspath(__file__))
CLI = r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
FQBN = "esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app"
BUILD = os.path.join(ICI, "build-brique")
DEST = os.path.join(ICI, "..", "brique", "firmware")

# version = FW_VERSION du sketch (ex. v54.4) : l'incrementer avant de publier, sinon la page ne propose rien aux consoles deja a jour
with open(os.path.join(ICI, "c3-tft-meatboy.ino"), encoding="utf-8", errors="replace") as f:
    version = re.search(r'#define FW_VERSION "([^"]+)"', f.read()).group(1)
ancien = os.path.join(DEST, "manifest.json")
if os.path.exists(ancien) and json.load(open(ancien)).get("version") == version:
    print(f"Attention : {version} est deja publiee. Incremente FW_VERSION dans c3-tft-meatboy.ino.")

for gen in ("gen_quiz.py", "gen_hand_levels.py"):
    subprocess.run([sys.executable, os.path.join(ICI, gen)], cwd=ICI, check=True)
env = dict(os.environ, ARDUINO_DIRECTORIES_USER=os.path.join(ICI, "libs"))
subprocess.run([CLI, "compile", "--fqbn", FQBN, "--output-dir", BUILD, "c3-tft-meatboy.ino"], cwd=ICI, env=env, check=True)

boot_app0 = sorted(glob.glob(os.path.join(os.environ["LOCALAPPDATA"], "Arduino15", "packages", "esp32", "hardware", "esp32", "*", "tools", "partitions", "boot_app0.bin")))[-1]
parts = [("bootloader.bin", os.path.join(BUILD, "c3-tft-meatboy.ino.bootloader.bin"), 0x0),
         ("partitions.bin", os.path.join(BUILD, "c3-tft-meatboy.ino.partitions.bin"), 0x8000),
         ("boot_app0.bin", boot_app0, 0xE000),
         ("console.bin", os.path.join(BUILD, "c3-tft-meatboy.ino.bin"), 0x10000)]
os.makedirs(DEST, exist_ok=True)
for nom, src, _ in parts:
    shutil.copyfile(src, os.path.join(DEST, nom))
manifest = {"name": "Console Brique", "version": version, "new_install_prompt_erase": False,
            "builds": [{"chipFamily": "ESP32-C3", "parts": [{"path": nom, "offset": off} for nom, _, off in parts]}]}
with open(os.path.join(DEST, "manifest.json"), "w") as f:
    json.dump(manifest, f, indent=1)
print(f"\nVersion {version} prete dans brique/firmware/ : commit + push pour la publier.")
