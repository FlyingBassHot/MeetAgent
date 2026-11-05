#!/usr/bin/env bash
#
# Compile every firmware C file with the host GCC + ESP-IDF stubs.
# This is a syntax/type check, not a substitute for `idf.py build`.
set -euo pipefail
cd "$(dirname "$0")"

CC="${CC:-gcc}"
ESPUI="../../components/espui"
COMPONENTS="../../components"

CFLAGS=(
    -std=c11
    -Wall
    -Wextra
    -Werror
    -fsyntax-only
    -Istubs
    -I"${ESPUI}/theme_api"
    -I"${ESPUI}/core"
    -I"${ESPUI}/runtime"
    -I"${ESPUI}/hal"
    -I"${ESPUI}/themes"
    -I"${COMPONENTS}/protocol/include"
    -I"${COMPONENTS}/state/include"
    -I"${COMPONENTS}/networking/include"
    -I../../main
)

mapfile -t files < <(find "${COMPONENTS}" ../../main -name '*.c' | sort)
for file in "${files[@]}"; do
    echo "[syntax] ${file}"
    "${CC}" "${CFLAGS[@]}" "${file}"
done

echo "[syntax] OK: ${#files[@]} file(s)"
