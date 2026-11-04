import os
import subprocess
import time

env = os.environ.copy()
env.pop("WAYLAND_DISPLAY", None)
env["XDG_SESSION_TYPE"] = "x11"
# El resultado temporal vive en build; la auditoría conserva su salida aparte.
with open("build/xwayland-esc.log", "w") as log:
    process = subprocess.Popen(["./build/linux-dev/ecosphere", "--module", "aquabio"], env=env, stdout=log, stderr=log)
    try:
        window = None
        for _ in range(100):
            found = subprocess.run(["xdotool", "search", "--pid", str(process.pid), "--name", "^EcoSphere Phase 3 - aquabio$"], env=env, text=True, capture_output=True)
            if found.returncode == 0:
                window = found.stdout.splitlines()[0]
                break
            if process.poll() is not None:
                raise RuntimeError("Demo exited before X11 window appeared")
            time.sleep(0.05)
        if window is None:
            raise RuntimeError("X11 window not found in 5 seconds")
        subprocess.run(["xdotool", "getwindowgeometry", "--shell", window], env=env, check=True, stdout=log, stderr=log)
        subprocess.run(["xdotool", "windowsize", window, "960", "540"], env=env, check=True, stdout=log, stderr=log)
        time.sleep(0.3)
        subprocess.run(["xdotool", "getwindowgeometry", "--shell", window], env=env, check=True, stdout=log, stderr=log)
        subprocess.run(["xdotool", "windowfocus", "--sync", window], env=env, check=True, timeout=5, stdout=log, stderr=log)
        focused = subprocess.check_output(["xdotool", "getwindowfocus"], env=env, text=True).strip()
        if focused != window:
            raise RuntimeError("Refusing key injection: EcoSphere is not focused")
        subprocess.run(["xdotool", "key", "Escape"], env=env, check=True, stdout=log, stderr=log)
        result = process.wait(timeout=10)
        log.write(f"process exit={result}\n")
        if result:
            raise RuntimeError(f"Unexpected demo exit {result}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
with open("build/xwayland-esc.log") as log:
    text = log.read()
assert "ESC: close requested" in text, text
assert "shutdown stub" in text, text
print(text)
