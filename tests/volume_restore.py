#!/usr/bin/env python3
"""Exercise actual launcher capture/cleanup against disposable hardware nodes."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parent.parent
launcher = (root / "APPS/BaetaMP.sh").read_text()
section = launcher.split("saved_volume=''")[1].split('(\n    trap')[0]
section = "saved_volume=''" + section
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    for initial in ("0", "4", "10", "invalid", None):
        for mode in ("normal", "error", "term", "restart"):
            code = section
            nodes = {
                "/sys/class/anbernic_misc/openbor_volume": tmp / "volume",
                "/sys/class/anbernic_misc/tpctrl": tmp / "touch",
                "/sys/class/backlight/backlight1/brightness": tmp / "upper",
                "/sys/class/backlight/backlight/brightness": tmp / "lower",
            }
            for path, node in nodes.items():
                code = code.replace(path, str(node))
                node.write_text("50\n")
            vol = tmp / "volume"
            if initial is None:
                vol.unlink()
            else:
                vol.write_text(initial + "\n")
            code += f"\nprintf '8\\n' > '{vol}'\n"
            if mode == "restart":
                code += f'restore\n[ "$(cat {vol})" = 8 ] || exit 88\n'
            code += {"normal": "exit 0", "error": "exit 7", "term": "kill -TERM $$", "restart": "exit 0"}[mode]
            result = subprocess.run(["bash", "-u", "-c", code], timeout=5)
            assert result.returncode == {"normal": 0, "error": 7, "term": 143, "restart": 0}[mode]
            assert vol.read_text().strip() == (initial if initial and initial.isdigit() else "8"), (initial, mode)
print("Console volume: normal/error/signal exits, internal restart, zero, invalid/missing snapshot PASS")
