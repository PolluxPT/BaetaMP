#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create an installable release from explicit public assets and matching source."""
from pathlib import Path
import argparse, hashlib, re, stat, struct, zipfile
ROOT=Path(__file__).resolve().parent.parent
VERSION=(ROOT/"VERSION").read_text().strip()
def public_files():
    fixed=["LICENSE","Makefile","VERSION","README.md","README_EN.md","CHANGELOG.md","CONTRIBUTING.md","THIRD_PARTY_NOTICES.md",".gitignore",".gitattributes",
        "APPS/BaetaMP.sh","APPS/Imgs/BaetaMP.png","APPS/BaetaMP/bin/baetamp","APPS/BaetaMP/fonts/DejaVuSans.ttf","APPS/BaetaMP/fonts/LICENSE.txt",
        "APPS/BaetaMP/Filmes/Sample_720p.mp4","APPS/BaetaMP/Filmes/Sample_720p.srt","APPS/BaetaMP/Filmes/README.txt","APPS/BaetaMP/IPTV/README.txt"]
    files=[ROOT/name for name in fixed]
    for name in ["src","tests","tools","docs","licenses",".github"]:
        files.extend(p for p in (ROOT/name).rglob("*") if p.is_file() and "__pycache__" not in p.parts and p.suffix in {".c",".h",".py",".sh",".md",".txt",".yml",".yaml",".png"})
    return sorted(set(files),key=lambda p:p.relative_to(ROOT).as_posix())
def main():
    parser=argparse.ArgumentParser();parser.add_argument("--output-dir",type=Path,default=ROOT/"dist");args=parser.parse_args()
    assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+",VERSION),"Invalid VERSION"
    binary=ROOT/"APPS/BaetaMP/bin/baetamp";data=binary.read_bytes()
    assert data[:4]==b"\x7fELF" and len(data)>20 and data[4]==2 and struct.unpack_from("<H",data,18)[0]==183,"Build the AArch64 program first: make arm64"
    args.output_dir.mkdir(parents=True,exist_ok=True);output=args.output_dir/("BaetaMP-v"+VERSION+"-RGDSPlus.zip")
    files=public_files()
    for p in files:
        assert p.is_file() and not p.is_symlink(),str(p)
        assert p.stat().st_size<25*1024*1024,str(p)+" exceeds browser upload size"
    with zipfile.ZipFile(output,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in files:
            name=p.relative_to(ROOT).as_posix();info=zipfile.ZipInfo(name,(2026,10,19,12,0,0));info.create_system=3
            mode=0o755 if p.suffix==".sh" or name=="APPS/BaetaMP/bin/baetamp" else 0o644
            info.external_attr=(stat.S_IFREG|mode)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,p.read_bytes())
    digest=hashlib.sha256(output.read_bytes()).hexdigest()
    (args.output_dir/"SHA256SUMS.txt").write_text(digest+"  "+output.name+"\n")
    print(output);print("SHA256",digest)
if __name__=="__main__":main()
