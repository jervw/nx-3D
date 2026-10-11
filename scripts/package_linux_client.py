#!/usr/bin/env python3
"""Bundle the Linux client, assets, and non-glibc shared libraries."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

from package_linux_server import ROOT, dependencies


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    version = (ROOT / "VERSION.txt").read_text().strip()
    executable = args.build_dir / "client" / "nx_3D_client"
    nexilis = args.build_dir / "nexilis" / "libnexilis.so"
    if not executable.is_file() or not nexilis.is_file():
        raise FileNotFoundError("Build the client and Nexilis before packaging")

    with tempfile.TemporaryDirectory(prefix="nx3d-linux-client-") as temp:
        package = Path(temp) / "nx-3D-client"
        package.mkdir()
        shutil.copy2(executable, package / executable.name)
        shutil.copy2(nexilis, package / nexilis.name)
        shutil.copytree(ROOT / "assets", package / "assets")
        shutil.copytree(ROOT / "spear" / "engine" / "assets", package / "spear" / "assets")
        shutil.copytree(ROOT / "spear" / "engine" / "shaders", package / "spear" / "shaders")
        queue = [executable, nexilis]
        copied = {nexilis.name}
        while queue:
            for name, source in dependencies(queue.pop()).items():
                if name not in copied:
                    shutil.copy2(source, package / name)
                    copied.add(name)
                    queue.append(source)
        for name in (executable.name, *copied):
            subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(package / name)], check=True)
        launcher = package / "run.sh"
        launcher.write_text('#!/usr/bin/env bash\nset -e\ncd "$(dirname "$0")"\nexec ./nx_3D_client "$@"\n')
        launcher.chmod(0o755)
        archive = ROOT / "dist" / f"nx-3D-client-v{version}-linux-x64.tar.gz"
        archive.parent.mkdir(exist_ok=True)
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(package, arcname=package.name)
        print(f"Created {archive}")


if __name__ == "__main__":
    main()
