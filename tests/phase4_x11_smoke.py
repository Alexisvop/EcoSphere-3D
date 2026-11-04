"""Entrada XTEST real: solo envía teclas a la ventana EcoSphere propia con PID/foco comprobados."""
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "build/linux-dev/ecosphere"
APP_LOG = ROOT / "build/phase4-x11-app.log"
env = os.environ.copy()
env.pop("WAYLAND_DISPLAY", None)
env["XDG_SESSION_TYPE"] = "x11"


def run(*args, check=True):
    result = subprocess.run(args, env=env, capture_output=True, text=True, timeout=5)
    print("$", shlex.join(args), "exit=", result.returncode, flush=True)
    if result.returncode and check:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def text():
    return APP_LOG.read_text()


def state():
    lines = [line for line in text().splitlines() if line.startswith("Interaction state:")]
    require(bool(lines), "Missing interaction telemetry")
    return dict(re.findall(r"(\w+)=([^ ]+)", lines[-1]))


with APP_LOG.open("w") as output:
    process = subprocess.Popen([str(BINARY), "--interaction", "--module", "aquabio"],
                               cwd=ROOT, env=env, stdout=output, stderr=output)
    try:
        window = None
        for _ in range(100):
            found = run("xdotool", "search", "--pid", str(process.pid), "--name",
                        "^EcoSphere Phase 4 - aquabio$", check=False)
            if found.returncode == 0:
                window = found.stdout.splitlines()[0]
                break
            require(process.poll() is None, "Application exited before window appeared")
            time.sleep(0.05)
        require(window is not None, "No EcoSphere X11 window")

        def focus():
            run("xdotool", "windowfocus", "--sync", window)
            # X11 confirma el foco antes de que GLFW descarte su primer paso al recuperarlo.
            time.sleep(0.15)
            require(run("xdotool", "getwindowfocus").stdout.strip() == window, "Refusing input without own window focus")
            require(run("xdotool", "getwindowpid", window).stdout.strip() == str(process.pid), "PID changed")

        def key(name, seconds=0):
            focus()
            before = text().count("Interaction state:")
            if seconds:
                run("xdotool", "keydown", name)
                try:
                    time.sleep(seconds)
                finally:
                    # Volver a comprobar foco antes de enviar también la liberación.
                    focus()
                    run("xdotool", "keyup", name)
                return key("F1")
            run("xdotool", "key", name)
            for _ in range(80):
                if text().count("Interaction state:") > before:
                    return state()
                time.sleep(0.025)
            raise RuntimeError("No interaction state after key")

        time.sleep(0.3)
        require(key("p")["playing"] == "0", "P pause")
        require(key("Tab")["selected"] == "simple-rotation", "Tab selection")
        moved = key("l", 0.25)
        require(float(moved["offset"].split(",")[0]) > 0.15, "Translation with L")
        rotated = key("x", 0.25)
        require(abs(float(rotated["quaternion"].split(",")[2])) > 0.03, "Rotation with X")
        scaled = key("v", 0.25)
        require(float(scaled["scale"]) > 1.05, "Scale with V")
        taken = key("g")
        require(taken["held"] == "1", "G take")
        followed = key("w", 0.25)
        require(math.dist(tuple(map(float,taken["world"].split(","))),
                          tuple(map(float,followed["world"].split(",")))) > 0.15, "Held object follows WASD camera")
        require(key("g")["held"] == "0", "G drop")
        require(key("BackSpace")["selected"] == "none", "Backspace deselect")
        require(key("space")["selected"] == "simple-rotation", "Space ray picks object at camera center")
        for _ in range(7):
            current = key("Tab")
        require(current["selected"] == "imported", "Imported instance selectable")
        require(float(key("j", 0.2)["offset"].split(",")[0]) < -0.1, "Imported instance manipulable")
        resumed = key("p")
        require(resumed["playing"] == "1", "P resumes")
        time.sleep(0.25)
        paused = key("p")
        require(float(paused["time"]) > float(resumed["time"]) + 0.1, "Animation advances by real time")
        time.sleep(0.2)
        require(state()["time"] == key("F1")["time"], "Paused animation stays fixed")
        restarted = key("r")
        require(restarted["playing"] == "1" and float(restarted["time"]) < 0.1, "R restarts playback")
        focus()
        run("xdotool", "key", "Escape")
        require(process.wait(timeout=10) == 0, "ESC clean exit")
        require("shutdown stub" in text() and "Clean exit" in text(), "Lifecycle cleanup")
        print("PHASE 4 XTEST PASS: Tab/ray/deselect, TRS, take/follow/drop, GLB, pause/resume/restart, ESC", flush=True)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
