"""Fallos de despliegue reales, en copias aisladas; nunca modifica recursos originales."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
smoke_option = sys.argv[2] if len(sys.argv) > 2 else "--lighting-smoke"
if smoke_option not in ("--lighting-smoke", "--interaction-smoke"):
    raise ValueError("Expected lighting or interaction smoke")
scratch = Path(tempfile.mkdtemp(prefix="phase3-resources-", dir=binary.parent.parent))
for case, diagnostic in (("missing-skybox", "Cannot open shader:"),
                         ("corrupt-model", "GLSL compile failed:"),
                         ("missing-model", "Cannot open model:")):
    target = scratch / case
    target.mkdir()
    app = target / binary.name
    shutil.copy2(binary, app)
    shutil.copytree(binary.parent / "shaders", target / "shaders",
                    ignore=shutil.ignore_patterns("skybox.frag") if case == "missing-skybox" else None)
    shutil.copytree(binary.parent / "assets", target / "assets",
                    ignore=shutil.ignore_patterns("*.glb") if case == "missing-model" else None)
    if case == "corrupt-model":
        (target / "shaders/model.frag").write_text("#version 330 core\ninvalid shader for failure test\n")
    command = [str(app), smoke_option, str(target / "captures")]
    result = subprocess.run(command, cwd="/tmp", text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=20)
    print("$", " ".join(command), "(cwd=/tmp)")
    print(result.stdout, end="")
    print("exit=", result.returncode)
    if result.returncode != 1 or diagnostic not in result.stdout or "shutdown stub" not in result.stdout:
        raise RuntimeError(f"{case}: expected diagnostic and module cleanup missing")
print("PASS: missing skybox shader, corrupt model shader, missing GLB; cwd=/tmp, isolated copies")
