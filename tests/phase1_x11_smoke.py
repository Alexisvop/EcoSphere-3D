"""Entrada real por XTEST en la ventana propia; requiere xdotool, no pertenece a CTest."""
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
APP_LOG = ROOT / "build/phase1-x11-app.log"
env = os.environ.copy()
env.pop("WAYLAND_DISPLAY", None)
env["XDG_SESSION_TYPE"] = "x11"


def run(*args, check=True):
    print("$", shlex.join(args), flush=True)
    result = subprocess.run(args, env=env, text=True, capture_output=True, timeout=5)
    if result.returncode:
        print(f"exit={result.returncode}: {result.stdout} {result.stderr}", flush=True)
        if check:
            result.check_returncode()
    return result


def app_text():
    return APP_LOG.read_text()


def state():
    matches = re.findall(r"Camera state: position=([^ ]+) yaw=([^ ]+) pitch=([^\n]+)", app_text())
    if not matches:
        raise RuntimeError("No camera state received from application")
    position, yaw, pitch = matches[-1]
    return tuple(map(float, position.split(","))), float(yaw), float(pitch)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


with APP_LOG.open("w") as output:
    process = subprocess.Popen([str(ROOT / "build/linux-dev/ecosphere"), "--module", "aquabio"],
                               cwd=ROOT, env=env, stdout=output, stderr=output)
    try:
        window = None
        for _ in range(100):
            found = run("xdotool", "search", "--pid", str(process.pid), "--name",
                        "^EcoSphere Phase 3 - aquabio$", check=False)
            if found.returncode == 0:
                window = found.stdout.splitlines()[0]
                break
            require(process.poll() is None, "Application exited before window appeared")
            time.sleep(0.05)
        require(window is not None, "No EcoSphere X11 window")

        def focus():
            run("xdotool", "windowfocus", "--sync", window)
            require(run("xdotool", "getwindowfocus").stdout.strip() == window,
                    "Refusing input: EcoSphere is not focused")

        def key(name, seconds=0):
            focus()
            before = app_text().count("Camera state:")
            if seconds:
                run("xdotool", "keydown", name)
                try:
                    time.sleep(seconds)
                finally:
                    run("xdotool", "keyup", name)
            else:
                run("xdotool", "key", name)
            for _ in range(40):
                if app_text().count("Camera state:") > before:
                    return state()
                time.sleep(0.025)
            raise RuntimeError("Camera state not received after key release")

        time.sleep(0.3)
        start = key("F1")
        walked = key("w", 0.25)
        require(math.dist(start[0], walked[0]) > 0.1, "First-person W did not move")
        require(abs(walked[0][1] - 1.7) < 0.001, "First-person height changed")
        backed = key("s", 0.15)
        require(backed[0][2] > walked[0][2] + 0.1, "First-person S did not move back")
        left = key("a", 0.15)
        require(left[0][0] < backed[0][0] - 0.1, "First-person A did not strafe left")
        right = key("d", 0.15)
        require(right[0][0] > left[0][0] + 0.1, "First-person D did not strafe right")
        turned = key("Right", 0.25)
        require(turned[1] > walked[1] + 1, "Arrow yaw did not rotate")
        pitched = key("Up", 0.4)
        require(pitched[2] > turned[2] + 1, "Arrow pitch did not rotate")
        grounded = key("e", 0.2)
        require(math.dist(grounded[0], pitched[0]) < 0.001, "First-person must ignore E")
        free = key("F2")
        raised = key("e", 0.25)
        require(raised[0][1] > free[0][1] + 0.1, "Free E did not ascend")
        lowered = key("q", 0.25)
        require(lowered[0][1] < raised[0][1] - 0.1, "Free Q did not descend")
        flown = key("w", 0.25)
        require(flown[0][1] > lowered[0][1] + 0.02, "Free W must follow upward pitch")
        free_turn = key("Left", 0.2)
        require(free_turn[1] < flown[1] - 1, "Free camera rotation missing")
        restored = key("F1")
        require(abs(restored[0][1] - 1.7) < 0.001, "F1 must restore observer height")
        print("PASS: real WASD/arrows, F1/F2, first-person height, free ascent/descent/rotation", flush=True)

        for name, field in (("1", "direct"), ("2", "directional"), ("3", "point"),
                            ("4", "reflection"), ("5", "skybox"), ("6", "regression")):
            key(name)
            latest = re.findall(r"Lighting state: ([^\n]+)", app_text())[-1]
            require(f"{field}={1 if name == '6' else 0}" in latest, f"Toggle {field} failed")
            key(name)
            latest = re.findall(r"Lighting state: ([^\n]+)", app_text())[-1]
            require(f"{field}={0 if name == '6' else 1}" in latest, f"Restore {field} failed")
        print("PASS: lighting/reflection/skybox/regression keys 1..6 and restoration", flush=True)

        # El compositor puede ignorar resize/minimización. Registrar solo cambios efectivos.
        if shutil.which("hyprctl"):
            clients = json.loads(run("hyprctl", "-j", "clients").stdout)
            own = [client for client in clients if client.get("pid") == process.pid]
            require(len(own) == 1, "Cannot identify unique EcoSphere compositor window")
            raw_address = own[0]["address"]
            require(re.fullmatch(r"0x[0-9a-fA-F]+", raw_address) is not None, "Invalid window address")
            address = "address:" + raw_address
            # Sintaxis Lua del compositor local; selector exclusivo de nuestro PID.
            run("hyprctl", "dispatch",
                f'hl.dsp.window.float({{window = "{address}", action = "set"}})', check=False)
        sizes = re.findall(r"Framebuffer: (\d+x\d+)", app_text())
        run("xdotool", "windowsize", window, "900", "600")
        time.sleep(0.4)
        run("xdotool", "windowsize", window, "640", "800")
        time.sleep(0.4)
        actual_sizes = re.findall(r"Framebuffer: (\d+x\d+)", app_text())
        resized = "900x600" in actual_sizes[len(sizes):] and "640x800" in actual_sizes[len(sizes):]
        print("PASS: framebuffer resize 900x600 -> 640x800" if resized else
              "NO PROBADO: compositor did not apply both framebuffer sizes", flush=True)

        minimized_before = app_text().count("Minimized: yes")
        minimize = run("xdotool", "windowminimize", window, check=False)
        print("windowminimize exit:", minimize.returncode, minimize.stderr.strip(), flush=True)
        time.sleep(0.4)
        minimized = app_text().count("Minimized: yes") > minimized_before
        run("xdotool", "windowmap", window)
        focus()
        time.sleep(0.3)
        print("PASS: real minimize/restore" if minimized and "Minimized: no" in app_text() else
              "NO PROBADO: compositor did not emit real iconify/restore", flush=True)

        focus()
        run("xdotool", "key", "Escape")
        result = process.wait(timeout=10)
        require(result == 0, f"Unexpected application exit: {result}")
        require("ESC: close requested" in app_text() and "shutdown stub" in app_text() and
                "Clean exit" in app_text(), "ESC or cleanup not observed")
        print("PASS: ESC, module shutdown, clean exit", flush=True)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        output.flush()
        print("--- Actual application log ---\n" + app_text(), flush=True)
