"""Every #include "x.h" in a download bundle must resolve inside that bundle."""
import os, re, sys
root = sys.argv[1].rstrip("/") + "/"
BASE = ["EthologyRobot","Robot","CogServo","CogAnaDigi","CogProximity","CogLight","CogCollision"]
eth  = [f"{b}.{e}" for b in BASE for e in ("h","cpp")]
disp = ["PAWConfig.h","CogDisplay.h","CogDisplay.cpp"]
ble  = ["CogBluetooth.h","CogBluetooth.cpp","ethology_ble_robot.ino"]
ok = True
for name, files in [("hierarchy", eth+disp), ("receiver", eth+disp+ble)]:
    have = set(files)
    for f in files:
        d = "firmware/ethology/" if f in eth else "firmware/display/" if f in disp else "firmware/ble/"
        try:
            src = open(root + d + f).read()
        except FileNotFoundError:
            print(f"      {name}: {f} missing from the repo"); ok = False; continue
        for inc in re.findall(r'#include\s+"([^"]+)"', src):
            if inc not in have:
                print(f"      {name}: {f} includes {inc}, which the bundle does not carry")
                ok = False
sys.exit(0 if ok else 1)
