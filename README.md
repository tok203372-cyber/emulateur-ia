# Arcadia — l'émulateur intelligent

Arcadia réunit tous vos émulateurs dans une seule application Android, avec une intelligence
artificielle qui fonctionne entièrement sur le téléphone.

## Ce que fait Arcadia

- **Une seule bibliothèque pour toutes les consoles** : NES, Super Nintendo, Game Boy (Color/Advance),
  Nintendo 64, Nintendo DS, 3DS, PlayStation, PSP, Mega Drive, Master System, Game Gear, PC Engine,
  Neo Geo Pocket, WonderSwan, Atari, arcade, DOS… directement dans Arcadia.
- **Encore plus de consoles** (onglet « Plus ») : GameCube et Wii (Dolphin), PlayStation 2 (ARMSX2,
  NetherSX2), Dreamcast (Flycast, Redream), Saturn, 3DS (Azahar), Wii U (Cemu), Switch, PS3 —
  Arcadia range ces jeux, affiche les jaquettes GameCube/Wii et les lance dans le bon émulateur.
- **Changer de jeu ou de console en pleine partie** : la partie est sauvegardée automatiquement,
  puis le jeu choisi démarre avec le bon émulateur.
- **Manettes** : les touches sont configurées automatiquement pour chaque console.
- **IA locale** :
  - traduction en direct des textes du jeu (japonais, anglais… vers le français), posée sur l'image ;
  - lecture de l'écran à voix haute ;
  - commandes vocales (« sauvegarde », « traduis l'écran », « change de jeu pour Zelda »…) ;
  - assistant de jeu qui voit votre écran (application **Arcadia IA**) ;
  - « Que jouer ce soir ? » : l'IA choisit parmi vos jeux selon votre envie ;
  - « Présenter ce jeu » depuis la bibliothèque.

Aucun jeu n'est fourni : utilisez uniquement vos propres copies de jeux.

## Les deux applications

| Dossier | Application | Rôle |
|---|---|---|
| `lemuroid-app` (et modules voisins) | **Arcadia** | L'émulateur, les menus, la traduction, la voix |
| `arcadia-ia` | **Arcadia IA** | Le modèle de langage (LiteRT-LM) : assistant et recommandations |

La recette `.github/workflows/arcadia.yml` fabrique les deux applications ; le résultat se trouve
dans l'onglet **Actions**, artefact **Arcadia-APK**.

## Origine et licence

Arcadia est une version modifiée de [Lemuroid](https://github.com/Swordfish90/Lemuroid)
(Swordfish90, licence GPL-3.0), basée sur la révision 3625bc1. Les ajouts d'Arcadia sont surtout
dans `lemuroid-app/src/main/java/com/swordfish/lemuroid/app/arcadia/` et dans `arcadia-ia/`.
L'ensemble est distribué sous la même licence **GPL-3.0** (voir `COPYING`).

Bibliothèques principales : LibretroDroid et les cœurs libretro, ML Kit (reconnaissance de texte,
traduction, identification de langue), LiteRT-LM (Google AI Edge). Les modèles d'IA (Qwen 2.5,
Phi-4 mini, Gemma 3n) sont téléchargés séparément et gardent leurs propres licences.
