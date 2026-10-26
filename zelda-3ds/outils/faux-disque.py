#!/usr/bin/env python3
"""Fabrique une image de disque GameCube factice (quelques Ko, aucune donnée du jeu).

Utilisation :
    python3 faux-disque.py sortie.iso

L'image a la structure d'un disque GameCube (en-tête avec l'identifiant GZ2P01, bi2,
chargeur d'application, programme principal vide, table des fichiers vide) mais ne contient aucun
fichier. Elle sert à l'essai automatique dans l'émulateur : le jeu l'ouvre, initialise tout
(mémoire, fils, son, rendu) puis échoue en cherchant ses premiers fichiers. On voit ainsi jusqu'où
le démarrage fonctionne sans utiliser de données protégées.
"""
import struct
import sys

DEBUT_DOL = 0x3000
DEBUT_FST = 0x3100


def fabriquer():
    disque = bytearray(0x3200)

    # En-tête (boot.bin) : identifiant, numéro de disque, version, code magique GameCube, nom
    disque[0x00:0x06] = b"GZ2P01"
    struct.pack_into(">I", disque, 0x1C, 0xC2339F3D)
    nom = b"DISQUE FACTICE POUR ESSAI (AUCUNE DONNEE)"
    disque[0x20:0x20 + len(nom)] = nom
    # Programme principal, table des fichiers et sa taille
    fst = bytearray()
    fst += struct.pack(">BBHII", 1, 0, 0, 0, 1)  # racine : dossier, nom vide, 1 entrée
    fst += b"\0"  # table des noms
    while len(fst) % 4:
        fst += b"\0"
    struct.pack_into(">IIII", disque, 0x420, DEBUT_DOL, DEBUT_FST, len(fst), len(fst))

    # bi2.bin (0x440..0x2440) : laissé à zéro

    # Chargeur d'application : date, point d'entrée, taille, taille de fin
    disque[0x2440:0x2450] = b"2026/10/27\0\0\0\0\0\0"
    struct.pack_into(">III", disque, 0x2450, 0x81200000, 0x20, 0)

    # Programme principal (DOL) : en-tête sans aucune section
    # (0x3000..0x3100 : zéros)

    disque[DEBUT_FST:DEBUT_FST + len(fst)] = fst
    return bytes(disque)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    with open(sys.argv[1], "wb") as f:
        f.write(fabriquer())
    return 0


if __name__ == "__main__":
    sys.exit(main())
