#!/usr/bin/env bash
# Regenerate include/core/EmbeddedShadersVulkan.h from the GLSL sources here.
# Requires glslc (ships with the Vulkan SDK). Run from anywhere.
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

GLSLC="${GLSLC:-glslc}"
if ! command -v "$GLSLC" >/dev/null 2>&1; then
    if [ -n "$VULKAN_SDK" ] && [ -x "$VULKAN_SDK/Bin/glslc.exe" ]; then
        GLSLC="$VULKAN_SDK/Bin/glslc.exe"
    else
        echo "error: glslc not found (set GLSLC or VULKAN_SDK)" >&2
        exit 1
    fi
fi

OUT="$DIR/../../include/core/EmbeddedShadersVulkan.h"

compile() { "$GLSLC" -O "$1" -o "$2" -mfmt=c; }

compile ui.vert    ui.vert.spv.txt
compile basic.frag basic.frag.spv.txt
compile text.frag  text.frag.spv.txt
compile msdf.frag  msdf.frag.spv.txt
compile image.frag image.frag.spv.txt

{
cat <<'HDR'
#pragma once
#include <cstdint>
#include <cstddef>

// AUTO-GENERATED — do not edit by hand.
// Source GLSL lives in tools/shaders/*.{vert,frag}. Regenerate with:
//   tools/shaders/compile.sh   (invokes glslc -O -mfmt=c)
// SPIR-V words are little-endian uint32; pass byte size = count * 4 to
// VkShaderModuleCreateInfo.codeSize.

namespace FluentUI {
namespace ShadersVK {

HDR

emit() {
  echo "static const uint32_t $1[] ="
  cat "$2"
  echo ";"
  echo "static const size_t $1Size = sizeof($1);"
  echo ""
}

emit UI_Vert    ui.vert.spv.txt
emit Basic_Frag basic.frag.spv.txt
emit Text_Frag  text.frag.spv.txt
emit MSDF_Frag  msdf.frag.spv.txt
emit Image_Frag image.frag.spv.txt

cat <<'FTR'
} // namespace ShadersVK
} // namespace FluentUI
FTR
} > "$OUT"

rm -f ./*.spv.txt
echo "Generated $OUT"
