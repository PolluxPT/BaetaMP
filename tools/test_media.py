#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import os, shutil, subprocess, tempfile
root=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="baetamp-media-") as temp:
    app=Path(temp);(app/"config/retomar").mkdir(parents=True)
    for name in ("pt","en","external"):(app/(name+".srt")).write_text("1\n00:00:00,100 --> 00:00:07,000\n"+name+" subtitles\n")
    subprocess.run(["ffmpeg","-loglevel","error","-y","-i",str(root/"APPS/BaetaMP/Filmes/Sample_720p.mp4"),"-i",str(app/"pt.srt"),"-i",str(app/"en.srt"),"-map","0:v","-map","0:a","-map","0:a","-map","1","-map","2","-c:v","copy","-c:a","copy","-c:s","srt","-metadata:s:a:0","language=por","-metadata:s:a:1","language=eng","-metadata:s:a:1","title=English commentary","-metadata:s:s:0","language=por","-metadata:s:s:1","language=eng","-metadata:s:s:1","title=SDH",str(app/"film.mkv")],check=True)
    env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0")
    subprocess.run([str(root/"build/tracks-integration-native"),str(app),str(app/"film.mkv"),str(app/"external.srt")],env=env,check=True,timeout=25)
