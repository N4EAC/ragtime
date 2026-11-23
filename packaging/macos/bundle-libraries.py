#!/usr/bin/env python3
"""Copy and relocate native dependencies without requiring Qt deployment tools."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

app = Path(sys.argv[1]).resolve()
frameworks = app / "Contents/Frameworks"
frameworks.mkdir(exist_ok=True)
queue = [(p, p) for folder in (app / "Contents/MacOS", frameworks)
         for p in folder.iterdir() if p.is_file()]
visited = set()

def output(*args):
    return subprocess.check_output(args, text=True)

while queue:
    binary, original = queue.pop(0)
    if binary in visited:
        continue
    visited.add(binary)
    binary.chmod(binary.stat().st_mode | 0o200)
    dependencies = output("otool", "-L", str(binary)).splitlines()[1:]
    ids = output("otool", "-D", str(binary)).splitlines()[1:]
    if ids:
        subprocess.run(["install_name_tool", "-id", "@rpath/" + binary.name, str(binary)], check=True)
    for line in dependencies:
        dependency = line.strip().split(" (", 1)[0]
        if dependency.startswith(("/System/Library/", "/usr/lib/")) or dependency in ids:
            continue
        if dependency.startswith("/"):
            source = Path(dependency).resolve()
        else:
            name = Path(dependency).name
            candidates = [original.parent / name, frameworks / name, Path("/opt/homebrew/lib") / name]
            source = next((p.resolve() for p in candidates if p.exists()), None)
            if source is None:
                raise RuntimeError(f"Cannot resolve {dependency} in {binary}")
        target = frameworks / source.name
        if not target.exists():
            shutil.copy2(source, target)
            queue.append((target, source))
        replacement = "@executable_path/../Frameworks/" + target.name
        if replacement != dependency:
            subprocess.run(["install_name_tool", "-change", dependency, replacement, str(binary)], check=True)
    loads = output("otool", "-l", str(binary)).splitlines()
    for i, line in enumerate(loads):
        if line.strip() == "cmd LC_RPATH":
            path = loads[i + 2].strip().split(" ", 1)[1].split(" (offset", 1)[0]
            if path.startswith("/"):
                subprocess.run(["install_name_tool", "-delete_rpath", path, str(binary)], check=True)
