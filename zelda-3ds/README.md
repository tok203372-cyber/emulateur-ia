# Crépuscule — Twilight Princess (Dusklight) sur une vraie New 3DS

Ce dossier contient le portage pour **New 3DS / New 3DS XL** (console modifiée avec Luma3DS)
du projet [Dusklight](https://github.com/TwilitRealm/dusklight), la version PC de
*The Legend of Zelda: Twilight Princess* reconstruite à partir de la décompilation du jeu.

Aucun fichier du jeu n'est fourni : il faut **votre propre copie** du disque GameCube
(fichier `.iso`, `.gcz` ou `.ciso` que vous avez extrait vous-même), à placer dans le dossier
`zelda-3ds` de la carte SD.

## Comment ça marche

Le code du jeu (Dusklight) est compilé tel quel pour le processeur de la 3DS ; seules les
parties propres au PC sont remplacées :

- **Affichage** : le jeu dessine avec l'API graphique de la GameCube (GX). Le moteur `gx3ds`
  traduit ces commandes pour la puce graphique de la 3DS (PICA200) : combineurs de textures,
  sommets, textures, éclairage. Image en 400x240 (format large) sur l'écran du haut.
- **Manette, son, fichiers, fils d'exécution** : SDL 3, qui fonctionne sur 3DS.
- **Disque** : lecteur d'images GameCube (ISO, CISO, GCZ) qui remplace la bibliothèque « nod ».
- **Écran du bas** : journal (messages du jeu, erreurs).

## Contenu

| Dossier | Rôle |
|---|---|
| `moteur/` | Moteur graphique GX → PICA200 (`gx3ds`) et versions 3DS de fichiers d'Aurora |
| `demo-gx/` | Démonstration du moteur (quatre petites scènes) |
| `jeu/` | Fabrication du jeu complet ; `plateforme/` : code propre à la 3DS ; `compat/` : en-têtes de compatibilité |
| `correctifs/` | Petites modifications du code de Dusklight et d'Aurora pour la 3DS |
| `commun/` | En-tête inclus partout (types entiers identiques au PC) |
| `test/` | Petit programme de test : vérifie la fabrication, l'émulateur et la console |
| `cia/` | Réglages et images du paquet CIA (icône, bannière, son) |
| `outils/` | Récupération des sources externes, essai automatique dans l'émulateur Azahar |

La recette `.github/workflows/zelda-3ds.yml` récupère les sources (versions figées), applique
les correctifs, fabrique les fichiers `.3dsx` et `.cia` (onglet **Actions** → **Zelda 3DS**),
puis les essaie dans l'émulateur Azahar.
