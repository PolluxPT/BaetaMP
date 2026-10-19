#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import hashlib, subprocess, tempfile, zipfile
root=Path(__file__).resolve().parent.parent
config=root/"APPS/BaetaMP/config";config.mkdir(parents=True,exist_ok=True)
marker=config/"public-package-test.private";assert not marker.exists();marker.write_text("PRIVATE_PACKAGE_TEST_MARKER")
try:
    with tempfile.TemporaryDirectory(prefix="baetamp-package-") as temp:
        subprocess.run(["python3",str(root/"tools/package_release.py"),"--output-dir",temp],check=True)
        package=next(Path(temp).glob("*.zip"));first=package.read_bytes()
        with zipfile.ZipFile(package) as z:
            assert z.testzip() is None
            names=z.namelist();assert "APPS/Imgs/BaetaMP.png" in names
            assert not any("/config/" in name or "__pycache__" in name or "/build/" in name for name in names)
            assert z.read("APPS/Imgs/BaetaMP.png")== (root/"APPS/Imgs/BaetaMP.png").read_bytes()
            assert z.read("src/player.c")== (root/"src/player.c").read_bytes()
            assert z.read("APPS/BaetaMP/bin/baetamp")== (root/"APPS/BaetaMP/bin/baetamp").read_bytes()
            assert (z.getinfo("APPS/BaetaMP/bin/baetamp").external_attr>>16)&0o111
            assert (z.getinfo("APPS/BaetaMP.sh").external_attr>>16)&0o111
        checksum=(Path(temp)/"SHA256SUMS.txt").read_text().split()[0];assert checksum==hashlib.sha256(first).hexdigest()
        subprocess.run(["python3",str(root/"tools/package_release.py"),"--output-dir",temp],check=True);assert package.read_bytes()==first
finally:
    marker.unlink()
    if not any(config.iterdir()):config.rmdir()
print("Release package: exact source/binary/logo, executable modes, no private config, checksum, reproducible ZIP PASS")
