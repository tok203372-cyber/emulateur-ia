#!/usr/bin/env python3
"""Retrouve les fonctions citées dans un rapport de plantage du jeu (sdmc:/zelda-3ds/plantage.txt).

Utilisation :
    python3 lire-plantage.py plantage.txt carte.map[.xz]

La carte des adresses de chaque version est sur la branche « cartes-3ds » du dépôt
(fichier <date>-<version>.map.xz ; la version est écrite en tête du rapport). Pour chaque
adresse du rapport (PC, LR, adresses trouvées sur la pile), le script affiche la fonction qui la
contient et le fichier objet d'où elle vient.
"""
import bisect
import lzma
import re
import shutil
import subprocess
import sys


def lire_carte(chemin):
    ouvrir = lzma.open if chemin.endswith(".xz") else open
    with ouvrir(chemin, "rt", encoding="utf-8", errors="replace") as f:
        lignes = f.read().splitlines()

    # Dans la carte de GNU ld, chaque section de code (.text.<fonction> avec
    # -ffunction-sections) est suivie de son adresse, de sa taille et de son fichier objet ;
    # les symboles qu'elle contient suivent, un par ligne (adresse puis nom).
    fonctions = []  # (adresse, taille, nom, objet)
    section = None
    for i, ligne in enumerate(lignes):
        m = re.match(r"^ (\.text\S*)\s*$", ligne)
        if m:
            # nom de section trop long : adresse, taille et objet sur la ligne suivante
            suite = lignes[i + 1] if i + 1 < len(lignes) else ""
            m2 = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$", suite)
            if m2:
                section = (int(m2.group(1), 16), int(m2.group(2), 16), m.group(1), m2.group(3))
                fonctions.append(section)
            continue
        m = re.match(r"^ (\.text\S*)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$", ligne)
        if m:
            section = (int(m.group(2), 16), int(m.group(3), 16), m.group(1), m.group(4))
            fonctions.append(section)
            continue
        m = re.match(r"^\s+0x([0-9a-f]+)\s+([^\s=].*)$", ligne)
        if m and section is not None:
            adresse = int(m.group(1), 16)
            nom = m.group(2).strip()
            debut, taille, _, objet = section
            if debut <= adresse < debut + taille and not nom.startswith("PROVIDE"):
                fonctions.append((adresse, max(1, debut + taille - adresse), nom, objet))
    fonctions = [f for f in fonctions if f[0] != 0]
    fonctions.sort()
    return fonctions


def demeler(noms):
    outil = shutil.which("arm-none-eabi-c++filt") or shutil.which("c++filt")
    if not outil or not noms:
        return noms
    resultat = subprocess.run([outil], input="\n".join(noms), capture_output=True, text=True)
    sortie = resultat.stdout.splitlines()
    return sortie if len(sortie) == len(noms) else noms


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    with open(sys.argv[1], encoding="utf-8", errors="replace") as f:
        rapport = f.read()
    fonctions = lire_carte(sys.argv[2])
    debuts = [f[0] for f in fonctions]

    adresses = []
    for valeur in re.findall(r"\b(?:0x)?([0-9A-Fa-f]{8})\b", rapport):
        v = int(valeur, 16)
        if 0x100000 <= v < 0x4000000 and v not in adresses:
            adresses.append(v)

    trouvees = []
    for a in adresses:
        i = bisect.bisect_right(debuts, a) - 1
        # le symbole le plus proche en dessous qui contient l'adresse
        while i >= 0 and not (fonctions[i][0] <= a < fonctions[i][0] + fonctions[i][1]):
            i -= 1
        if i >= 0:
            debut, _, nom, objet = fonctions[i]
            nom = re.sub(r"^\.text\.", "", nom)
            trouvees.append((a, nom, a - debut, objet))
        else:
            trouvees.append((a, "?", 0, ""))

    noms = demeler([t[1] for t in trouvees])
    for (a, _, decalage, objet), nom in zip(trouvees, noms):
        objet_court = objet.rsplit("/", 1)[-1]
        print(f"{a:08X}  {nom} +0x{decalage:X}  [{objet_court}]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
