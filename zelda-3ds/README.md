# Crépuscule — Twilight Princess (Dusklight) sur une vraie New 3DS

Ce dossier contient le portage pour **New 3DS / New 3DS XL** (console modifiée avec Luma3DS)
du projet [Dusklight](https://github.com/TwilitRealm/dusklight), la version PC de
*The Legend of Zelda: Twilight Princess* reconstruite à partir de la décompilation du jeu.

Aucun fichier du jeu n'est fourni : il faut **votre propre copie** du disque GameCube
(fichier `.iso` ou `.ciso` que vous avez extrait vous-même).

## Contenu

| Dossier | Rôle |
|---|---|
| `test/` | Petit programme de test : vérifie la fabrication, l'émulateur et la console |
| `cia/` | Réglages et images du paquet CIA (icône, bannière, son) |
| `outils/` | Scripts : essai automatique dans l'émulateur Azahar |

La recette `.github/workflows/zelda-3ds.yml` fabrique les fichiers `.3dsx` et `.cia`
(onglet **Actions** → **Zelda 3DS**), puis les essaie dans l'émulateur Azahar.
