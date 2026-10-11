#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:?Usage: package_windows_client.sh BUILD_DIR}"
version="$(<"$repo_root/VERSION.txt")"
package_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/nx3d-client-win.XXXXXX")"
mkdir -p "$package_dir/assets" "$package_dir/spear/assets" "$package_dir/spear/shaders" "$repo_root/dist"

cp "$build_dir/client/nx_3D_client.exe" "$package_dir/"
cp "$build_dir/nexilis/libnexilis.dll" "$package_dir/"
cp -a "$repo_root/assets/." "$package_dir/assets/"
cp -a "$repo_root/spear/engine/assets/." "$package_dir/spear/assets/"
cp -a "$repo_root/spear/engine/shaders/." "$package_dir/spear/shaders/"

# Follow imports through every bundled DLL. Windows system DLLs are provided
# by the OS; MSYS2 DLLs are copied from the active UCRT64 environment.
queue=("$package_dir/nx_3D_client.exe" "$package_dir/libnexilis.dll")
while ((${#queue[@]})); do
    binary="${queue[0]}"
    queue=("${queue[@]:1}")
    while read -r dll; do
        [[ -n "$dll" ]] || continue
        if [[ -f "$package_dir/$dll" ]]; then
            continue
        fi
        source="$(find "$MINGW_PREFIX/bin" -maxdepth 1 -type f -iname "$dll" -print -quit)"
        if [[ -n "$source" ]]; then
            cp "$source" "$package_dir/$dll"
            queue+=("$package_dir/$dll")
        fi
    done < <(objdump -p "$binary" | awk '/DLL Name:/ {print $3}')
done

cat > "$package_dir/run.bat" <<'BAT'
@echo off
cd /d "%~dp0"
nx_3D_client.exe %*
BAT

file "$package_dir/nx_3D_client.exe" | grep -q 'PE32+ executable'
archive="$repo_root/dist/nx-3D-client-v${version}-windows-x64.zip"
(cd "$package_dir" && zip -qr "$archive" .)
unzip -t "$archive" | tail -1
echo "Created $archive"
