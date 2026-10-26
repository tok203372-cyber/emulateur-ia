#!/bin/bash
# Lance un programme 3DS dans l'émulateur Azahar sur un écran virtuel (Xvfb),
# prend des captures d'écran et récupère les journaux.
#
# Utilisation :
#   essai-azahar.sh <AppRun d'Azahar> <fichier.3dsx|-> <fichier.cia|-> <dossier de sortie> [id du titre] [secondes] [journal]
#   - id du titre : 8 chiffres hexadécimaux de la fin de l'identifiant (ex. 0C7E3000)
#   - journal : chemin du journal écrit par le programme sur la carte SD (ex. zelda-3ds/test-journal.txt)
set -u

APPRUN=$1
FICHIER_3DSX=$2
FICHIER_CIA=$3
SORTIE=$4
TITRE=${5:-}
DUREE=${6:-30}
JOURNAL_SD=${7:-zelda-3ds/test-journal.txt}

mkdir -p "$SORTIE"
SORTIE=$(cd "$SORTIE" && pwd)

export HOME=$(mktemp -d)
DONNEES=$HOME/.local/share/azahar-emu

# Écran virtuel
NUMERO=$((90 + RANDOM % 100))
Xvfb ":$NUMERO" -screen 0 1280x960x24 > /dev/null 2>&1 &
PID_XVFB=$!
export DISPLAY=":$NUMERO"
sleep 2

lancer() {
    local quoi=$1 nom=$2
    rm -f "$DONNEES/log/azahar_log.txt"
    setsid "$APPRUN" "$quoi" > "$SORTIE/sortie-$nom.txt" 2>&1 &
    local pid=$!
    local pas=$((DUREE / 3))
    for i in 1 2 3; do
        sleep "$pas"
        import -window root "$SORTIE/ecran-$nom-$i.png" 2> /dev/null || true
    done
    kill -9 -- "-$pid" 2> /dev/null || kill -9 "$pid" 2> /dev/null || true
    wait "$pid" 2> /dev/null || true
    sleep 1
    cp "$DONNEES/log/azahar_log.txt" "$SORTIE/azahar-$nom.txt" 2> /dev/null || true
    if [ -f "$DONNEES/sdmc/$JOURNAL_SD" ]; then
        cp "$DONNEES/sdmc/$JOURNAL_SD" "$SORTIE/journal-$nom.txt"
        rm -f "$DONNEES/sdmc/$JOURNAL_SD"
    else
        echo "(aucun journal écrit sur la carte SD)" > "$SORTIE/journal-$nom.txt"
    fi
}

if [ "$FICHIER_3DSX" != "-" ]; then
    lancer "$FICHIER_3DSX" 3dsx
fi

if [ "$FICHIER_CIA" != "-" ] && [ -n "$TITRE" ]; then
    "$APPRUN" -i "$FICHIER_CIA" > "$SORTIE/installation-cia.txt" 2>&1 || true
    titre_bas=$(echo "$TITRE" | tr 'A-F' 'a-f')
    APP=$(find "$DONNEES" -path "*title/00040000/$titre_bas/content/*.app" | head -1)
    if [ -n "$APP" ]; then
        lancer "$APP" cia
    else
        echo "CIA non installé (voir installation-cia.txt)" > "$SORTIE/journal-cia.txt"
    fi
fi

kill "$PID_XVFB" 2> /dev/null || true
rm -rf "$HOME"
ls -la "$SORTIE"
