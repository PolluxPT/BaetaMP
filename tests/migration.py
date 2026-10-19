#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Use disposable card trees; launcher exits before hardware access on x86."""
from pathlib import Path
import subprocess, tempfile, shutil, platform
root=Path(__file__).resolve().parent.parent
if platform.machine() not in ("x86_64", "amd64"):
    raise SystemExit("Run the launcher migration test on an x86-64 host, not on the console.")
with tempfile.TemporaryDirectory(prefix="baetamp-migration-") as temp:
    apps=Path(temp)/"APPS";apps.mkdir();script=apps/"BaetaMP.sh";shutil.copy2(root/"APPS/BaetaMP.sh",script)
    new=apps/"BaetaMP"
    for directory in ("config/retomar","Filmes","IPTV","bin"):(new/directory).mkdir(parents=True)
    (new/"bin/baetamp").write_bytes(b"new program")
    (new/"Filmes/conflict.mp4").write_bytes(b"current")
    for name in ("BaetaMediaPlayer","MediaPlayer_RGDSPlus"):
        old=apps/name
        for directory in ("config/retomar","Filmes","IPTV","bin"):(old/directory).mkdir(parents=True)
        (old/"config/retomar"/name).write_text("start=12\n")
        (old/"Filmes"/(name+".mp4")).write_bytes(b"user film")
        (old/"Filmes/conflict.mp4").write_bytes(name.encode())
        (old/"IPTV"/(name+".m3u")).write_text("#EXTM3U\n")
        (old/"bin/mediaplayer").write_bytes(b"old program")
    (apps/"BaetaMediaPlayer/config/settings.txt").write_text("0 36 65 1 1\n")
    (apps/"BaetaMediaPlayer/config/favoritos.txt").write_text("Channel\thttps://example.org/live\n")
    (apps/"BaetaMediaPlayer.sh").write_text("# BaetaMediaPlayer RGDS_RESTART_AFTER_VIDEO\n")
    (apps/"MediaPlayer.sh").write_text("# MediaPlayer_RGDSPlus RGDS_RESTART_AFTER_VIDEO\n")
    (apps/"Unrelated.sh").write_text("# keep this app\n")
    (apps/"Imgs").mkdir();(apps/"Imgs/BaetaMediaPlayer.png").write_bytes(b"old icon")
    def launch():
        result=subprocess.run(["bash",str(script)],check=False)
        assert result.returncode==1
    launch()
    assert (new/"bin/baetamp").read_bytes()==b"new program"
    assert (new/"Filmes/conflict.mp4").read_bytes()==b"current"
    assert (new/"config/settings.txt").read_text()=="0 36 65 1 1\n"
    assert (new/"config/favoritos.txt").read_text().startswith("Channel\t")
    for name in ("BaetaMediaPlayer","MediaPlayer_RGDSPlus"):
        assert not (apps/name).exists()
        assert (new/"Filmes"/(name+".mp4")).read_bytes()==b"user film"
        assert (new/"IPTV"/(name+".m3u")).exists()
        assert (new/"config/retomar"/name).read_text()=="start=12\n"
        assert (new/"config"/("migracao-"+name)/"Filmes/conflict.mp4").read_bytes()==name.encode()
    assert not (apps/"BaetaMediaPlayer.sh").exists() and not (apps/"MediaPlayer.sh").exists()
    assert (apps/"Unrelated.sh").exists()
    backups=list((new/"config").glob("migracao-*"));launch();assert list((new/"config").glob("migracao-*"))==backups
    shutil.rmtree(new);launch();assert (new/"config/retomar").is_dir() and not list((new/"config").glob("migracao-*"))
print("Migration: both legacy names, conflicts, movies, playlists, config, resume, launchers, fresh/repeat install PASS")
