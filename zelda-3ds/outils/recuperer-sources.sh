#!/bin/bash
# Récupère les sources nécessaires à la fabrication dans zelda-3ds/externe (non enregistré dans git) :
#  - Dusklight (le jeu reconstruit), Aurora (sa couche GameCube) et borealis (ses outils) ;
#  - SDL 3 (entrées, son, fichiers et fils d'exécution : SDL fonctionne sur 3DS) ;
#  - les petites bibliothèques utilisées par le jeu : fmt, nlohmann/json, cxxopts, Dear ImGui,
#    PicoSHA2, miniz, xxHash, et les en-têtes de Tracy (désactivé) et de RmlUi (menus du PC,
#    non affichés sur la 3DS).
# Chaque source est figée à une version précise ; les correctifs pour la 3DS (correctifs/*.patch)
# sont appliqués après le téléchargement.
set -euo pipefail

ICI=$(cd "$(dirname "$0")/.." && pwd)
EXT="$ICI/externe"
mkdir -p "$EXT"

# Versions utilisées (mises à jour à la main, après vérification)
DUSKLIGHT_COMMIT=77705f8bb95ef7b7dca4675b149c4bcb6a48da34
AURORA_COMMIT=3227d76c60e1e782ca576610bce61c9e7744d8be     # sous-module de Dusklight
BOREALIS_COMMIT=4ac5e7052a8c49a122f8d57f626b5c75c5ca6968   # sous-module de Dusklight
SDL3_COMMIT=8e37db5e797b6167f3a00d697d816a684bd259c7       # release-3.4.10 (celle d'Aurora)
FMT_COMMIT=407c905e45ad75fc29bf0f9bb7c5c2fd3475976f        # 12.1.0
JSON_COMMIT=55f93686c01528224f448c19128836e7df245f72       # v3.12.0
CXXOPTS_COMMIT=44380e5a44706ab7347f400698c703eb2a196202    # v3.3.1
IMGUI_COMMIT=4806a1924ff6181180bf5e4b8b79ab4394118875      # v1.91.9b-docking
PICOSHA2_COMMIT=161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29   # v1.0.1
TRACY_COMMIT=30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c      # v0.14.1
XXHASH_COMMIT=e626a72bc2321cd320e953a0ccf1584cad60f363     # v0.8.3
RMLUI_COMMIT=f00a0fee38391b2f927114e11bea18dc0a7dba1e      # celle d'Aurora (en-têtes seulement)
MINIZ_URL=https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
MINIZ_SHA256=ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5

recuperer() {
  local url=$1 dossier=$2 commit=$3 correctif=${4:-}
  if [ -d "$dossier/.git" ] && [ "$(git -C "$dossier" rev-parse HEAD 2>/dev/null)" = "$commit" ]; then
    # Déjà présent : on remet les fichiers d'origine (les correctifs sont réappliqués plus bas)
    git -C "$dossier" reset -q --hard
    git -C "$dossier" clean -qfd
    echo "déjà présent : $dossier"
  else
    rm -rf "$dossier"
    mkdir -p "$dossier"
    git -C "$dossier" init -q
    git -C "$dossier" remote add origin "$url"
    git -C "$dossier" fetch -q --depth 1 origin "$commit"
    git -C "$dossier" -c advice.detachedHead=false checkout -q FETCH_HEAD
    echo "récupéré : $dossier ($commit)"
  fi
  # Correctifs pour la 3DS (processeur 32 bits, pas de fil de travail GX, etc.)
  if [ -n "$correctif" ] && [ -s "$correctif" ]; then
    git -C "$dossier" apply --whitespace=nowarn "$correctif"
    echo "  correctif appliqué : $(basename "$correctif")"
  fi
}

recuperer https://github.com/TwilitRealm/dusklight.git "$EXT/dusklight" "$DUSKLIGHT_COMMIT" "$ICI/correctifs/dusklight.patch"
recuperer https://github.com/encounter/aurora.git "$EXT/dusklight/extern/aurora" "$AURORA_COMMIT" "$ICI/correctifs/aurora.patch"
recuperer https://github.com/encounter/borealis.git "$EXT/dusklight/extern/borealis" "$BOREALIS_COMMIT" "$ICI/correctifs/borealis.patch"
recuperer https://github.com/libsdl-org/SDL.git "$EXT/sdl3" "$SDL3_COMMIT" "$ICI/correctifs/sdl3.patch"
recuperer https://github.com/fmtlib/fmt.git "$EXT/fmt" "$FMT_COMMIT"
recuperer https://github.com/nlohmann/json.git "$EXT/json" "$JSON_COMMIT"
recuperer https://github.com/jarro2783/cxxopts.git "$EXT/cxxopts" "$CXXOPTS_COMMIT"
recuperer https://github.com/ocornut/imgui.git "$EXT/imgui" "$IMGUI_COMMIT"
recuperer https://github.com/okdshin/PicoSHA2.git "$EXT/picosha2" "$PICOSHA2_COMMIT"
recuperer https://github.com/wolfpld/tracy.git "$EXT/tracy" "$TRACY_COMMIT"
recuperer https://github.com/Cyan4973/xxHash.git "$EXT/xxhash" "$XXHASH_COMMIT"
recuperer https://github.com/encounter/RmlUi.git "$EXT/rmlui" "$RMLUI_COMMIT"

# miniz : version « amalgamée » (miniz.c + miniz.h) publiée avec chaque version
if [ ! -f "$EXT/miniz/miniz.c" ]; then
  mkdir -p "$EXT/miniz"
  curl -sSL -o "$EXT/miniz/miniz.zip" "$MINIZ_URL"
  echo "$MINIZ_SHA256  $EXT/miniz/miniz.zip" | sha256sum -c --quiet
  python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2], ['miniz.c', 'miniz.h', 'LICENSE'])" \
    "$EXT/miniz/miniz.zip" "$EXT/miniz"
  echo "récupéré : $EXT/miniz (3.0.2)"
else
  echo "déjà présent : $EXT/miniz"
fi
