#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Build the experimental fm1-base port from exact dependency revisions."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

if "--legacy" in sys.argv:
    raise SystemExit("The legacy platform is retired; only fm1-base targets are available.")

ROOT=Path(__file__).resolve().parents[1]
REV="6c0b47e4bfc56fd3cd5b34463210d0e7e0ce0431"
BSP_REV="0f08fcaa6610b7cabe8cfdcc5c50d6facdae0e7f"
source=ROOT/"build/deps"/("fm1-base-"+REV[:12])
work=ROOT/"build/fm1-base-platform"
def run(args,**kw):subprocess.run([str(x) for x in args],check=True,**kw)
if not source.exists():
    source.parent.mkdir(parents=True,exist_ok=True)
    run(["git","clone","--no-checkout","https://github.com/charlesvestal/fm1-base.git",source])
    run(["git","-C",source,"checkout","--detach",REV])
    run(["git","-C",source,"submodule","update","--init","--recursive"])
if subprocess.check_output(["git","-C",str(source),"rev-parse","HEAD"],text=True).strip()!=REV:
    raise SystemExit("fm1-base audit checkout is not at the pinned revision")
if subprocess.check_output(["git","-C",str(source/"bsp"),"rev-parse","HEAD"],text=True).strip()!=BSP_REV:
    raise SystemExit("felucca-bsp is not at the pinned revision")
if subprocess.check_output(["git","-C",str(source),"status","--porcelain"],text=True).strip():
    raise SystemExit("Pinned dependency has local changes; preserve them and use a clean checkout")
if work.exists():
    shutil.rmtree(work)  # disposable build overlay only; never the dependency checkout
shutil.copytree(source,work,ignore=shutil.ignore_patterns(".git","build","__pycache__"))
# The overlay is kept in the repository; dependency sources remain untouched.
shutil.copyfile(ROOT/"platform/app.ld",work/"device/app.ld")
from base_overlay import apply
apply(work)
from base_bootloader import prepare
prepare(work, ROOT/"platform/build/bootloader")
# Asset generation is independent of the retired firmware compiler and packager.
from generate_assets import generate
generate()
env=os.environ.copy()
env.setdefault("JIELI_TOOLCHAIN",str(Path.home()/".jieli/toolchain"))
env.setdefault("AC79_SDK",str(Path.home()/"fw-AC79_AIoT_SDK"))
run([sys.executable,work/"tools/build.py",ROOT/"platform",*sys.argv[1:]],env=env,cwd=ROOT)

if "--host" not in sys.argv and "--web" not in sys.argv:
    stem="saadet-usb" if "--no-ble" in sys.argv else "saadet"
    if "--release" in sys.argv:
        stem+="-"+sys.argv[sys.argv.index("--release")+1]
    run([sys.executable,ROOT/"tools/check_base.py",ROOT/"platform/build"/(stem+".fwsc")])
