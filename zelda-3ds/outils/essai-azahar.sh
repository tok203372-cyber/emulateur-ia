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
    setsid timeout -s KILL $((DUREE + 20)) "$APPRUN" "$quoi" > "$SORTIE/sortie-$nom.txt" 2>&1 &
    local pid=$!
    local pas=$((DUREE / 3))
    for i in 1 2 3; do
        sleep "$pas"
        import -window root "$SORTIE/ecran-$nom-$i.png" 2> /dev/null || true
    done
    kill -9 -- "-$pid" 2> /dev/null || kill -9 "$pid" 2> /dev/null || true
    wait "$pid" 2> /dev/null || true
    sleep 1
    local log="$DONNEES/log/azahar_log.txt"
    if [ -f "$log" ]; then
        # Accès mémoire invalides, comptés par adresse du code (PC) avant de raccourcir le journal
        grep -o "at PC 0x[0-9A-Fa-f]*" "$log" | awk '{print $3}' | sort | uniq -c | sort -rn | head -40 > "$SORTIE/pc-$nom.txt"
        # Journal limité à 600 Ko (début et fin) : il peut atteindre des centaines de Mo
        if [ "$(stat -c %s "$log")" -gt 600000 ]; then
            { head -c 400000 "$log"; printf '\n\n[... journal raccourci : %s octets au total ...]\n\n' "$(stat -c %s "$log")"; tail -c 200000 "$log"; } > "$SORTIE/azahar-$nom.txt"
        else
            cp "$log" "$SORTIE/azahar-$nom.txt"
        fi
    fi
    if [ -f "$DONNEES/sdmc/$JOURNAL_SD" ]; then
        cp "$DONNEES/sdmc/$JOURNAL_SD" "$SORTIE/journal-$nom.txt"
        rm -f "$DONNEES/sdmc/$JOURNAL_SD"
    else
        echo "(aucun journal écrit sur la carte SD)" > "$SORTIE/journal-$nom.txt"
    fi
    # Jeu : rapport de plantage et journaux de Dusklight
    local dossier="$DONNEES/sdmc/zelda-3ds"
    if [ -f "$dossier/plantage.txt" ]; then
        mv "$dossier/plantage.txt" "$SORTIE/plantage-$nom.txt"
    fi
    for f in "$dossier"/*.log; do
        [ -f "$f" ] || continue
        head -c 300000 "$f" > "$SORTIE/dusklight-$nom-$(basename "$f")"
        rm -f "$f"
    done
}

if [ "$FICHIER_3DSX" != "-" ]; then
    lancer "$FICHIER_3DSX" 3dsx
fi

if [ "$FICHIER_CIA" != "-" ] && [ -n "$TITRE" ]; then
    timeout -s KILL 90 "$APPRUN" -i "$FICHIER_CIA" > "$SORTIE/installation-cia.txt" 2>&1 || echo "code de sortie : $?" >> "$SORTIE/installation-cia.txt"
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
