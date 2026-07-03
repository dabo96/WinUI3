#!/usr/bin/env bash
# Regenera el atlas MSDF de TEXTO (Segoe UI) -> assets/fonts/atlas.{json,png}
#
# NO afecta a los iconos (Lucide va por FreeType on-demand, camino aparte).
#
# Requisitos:
#   - msdf-atlas-gen en PATH, o exportar MSDF_ATLAS_GEN=/ruta/a/msdf-atlas-gen.exe
#     (binario prebuilt: https://github.com/Chlumsky/msdf-atlas-gen/releases)
#   - Segoe UI: por defecto C:\Windows\Fonts\segoeui.ttf (override con SEGOE_TTF=...)
#
# IMPORTANTE - preservar el aspecto del texto ya existente:
#   Los parametros deben COINCIDIR con el header del atlas.json actual:
#     type msdf | size 64 | pxrange 12 (distanceRange) | 2048x2048 | yorigin bottom
#   Con esos valores los glifos que ya existian quedan con layout identico (mismo advance
#   y planeBounds); solo se anaden los nuevos. NOTA: el shader usa pxRange=4.0 hardcodeado
#   (OpenGLBackend.cpp / VulkanBackend.cpp) aunque el atlas diga 12 -> es un desajuste
#   preexistente que se ve bien; mantener 12 aqui replica el aspecto actual. No lo cambies
#   sin ajustar tambien el shader, o cambiaria el look de TODO el texto.
set -euo pipefail
cd "$(dirname "$0")/.."   # raiz del repo

MAG="${MSDF_ATLAS_GEN:-msdf-atlas-gen}"
SEGOE="${SEGOE_TTF:-/c/Windows/Fonts/segoeui.ttf}"
CHARSET="tools/text_atlas_charset.txt"
OUT_JSON="assets/fonts/atlas.json"
OUT_PNG="assets/fonts/atlas.png"

command -v "$MAG" >/dev/null 2>&1 || [ -x "$MAG" ] || { echo "ERROR: no encuentro msdf-atlas-gen (PATH o \$MSDF_ATLAS_GEN)"; exit 1; }
[ -f "$SEGOE" ] || { echo "ERROR: no encuentro Segoe UI en $SEGOE (define \$SEGOE_TTF)"; exit 1; }

# El charset efectivo = ultima linea no vacia y no-comentario del fichero.
# Se escribe a un fichero temporal real: msdf-atlas-gen.exe (binario nativo Windows) NO
# entiende la sustitucion de proceso <(...) de Bash (/dev/fd/NN).
CHARS="$(grep -v '^[[:space:]]*#' "$CHARSET" | grep -v '^[[:space:]]*$' | tail -1)"
echo "Charset: $CHARS"
TMP_CHARSET="$(mktemp)"
trap 'rm -f "$TMP_CHARSET"' EXIT
printf '%s\n' "$CHARS" > "$TMP_CHARSET"

"$MAG" -font "$SEGOE" -charset "$TMP_CHARSET" \
  -type msdf -size 64 -pxrange 12 -dimensions 2048 2048 -yorigin bottom \
  -imageout "$OUT_PNG" -json "$OUT_JSON"

echo "OK -> $OUT_PNG / $OUT_JSON"
echo "Glifos: $(grep -o '\"unicode\"' "$OUT_JSON" | wc -l)"
