#!/bin/bash
# Récupère les sources nécessaires à la fabrication dans zelda-3ds/externe (non enregistré dans git) :
#  - Dusklight (le jeu reconstruit) et Aurora (sa couche GameCube), à une version précise ;
#  - fmt (mise en forme du texte) et les en-têtes de Tracy (désactivé).
set -euo pipefail

ICI=$(cd "$(dirname "$0")/.." && pwd)
EXT="$ICI/externe"
mkdir -p "$EXT"

# Versions utilisées (mises à jour à la main, après vérification)
DUSKLIGHT_COMMIT=77705f8bb95ef7b7dca4675b149c4bcb6a48da34
AURORA_COMMIT=3227d76c60e1e782ca576610bce61c9e7744d8be
FMT_COMMIT=407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
TRACY_COMMIT=30997d5ca6bb632cc10807a1da8a6d3de0aeeb3c

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
recuperer https://github.com/fmtlib/fmt.git "$EXT/fmt" "$FMT_COMMIT"
recuperer https://github.com/wolfpld/tracy.git "$EXT/tracy" "$TRACY_COMMIT"
