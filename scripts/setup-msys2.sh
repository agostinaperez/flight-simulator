#!/usr/bin/env bash
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
    echo "Ejecuta este script desde una terminal MSYS2 MINGW64." >&2
    exit 1
fi

packages=()
while IFS= read -r package || [[ -n "$package" ]]; do
    package="${package%$'\r'}"
    [[ -z "$package" ]] && continue
    if ! pacman -Q "$package" >/dev/null 2>&1; then
        packages+=("$package")
    fi
done < dependencies/msys2-packages.txt

if ((${#packages[@]} > 0)); then
    pacman -S --needed --noconfirm "${packages[@]}"
else
    echo "Todas las dependencias MSYS2 del manifiesto ya estan instaladas."
fi
