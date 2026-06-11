from pyrplidar import PyRPlidar
import time

lidar = PyRPlidar()
lidar.connect(port="COM3", baudrate=256000, timeout=3)  # ← 256000 !
time.sleep(0.1)

info = lidar.get_info()
print("info :", info)

health = lidar.get_health()
print("health :", health)

samplerate = lidar.get_samplerate()
print("samplerate :", samplerate)

lidar.disconnect()