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
    -I"${ESPUI}/themes/terminal_aesthetic/renderers"
    -I"${COMPONENTS}/protocol/include"
    -I"${COMPONENTS}/state/include"
    -I"${COMPONENTS}/networking/include"
    -I../../main
)

# board_lcd4.c needs the managed Waveshare BSP/LVGL headers and wifi_prov.c
# needs ESP-IDF Wi-Fi/HTTP/NVS headers; both are verified by the real build.
mapfile -t files < <(find "${COMPONENTS}" ../../main -name '*.c' \
    ! -path '*/main/board_lcd4.c' \
    ! -path '*/components/networking/wifi_prov.c' \
    ! -path '*/components/networking/net_client.c' | sort)
for file in "${files[@]}"; do
    echo "[syntax] ${file}"
    "${CC}" "${CFLAGS[@]}" "${file}"
done

echo "[syntax] OK: ${#files[@]} file(s)"
