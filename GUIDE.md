# Guide d'installation d'Arcadia (Galaxy S25 Ultra)

Arcadia, ce sont **deux applications** :

| Fichier | Application | À quoi elle sert |
|---|---|---|
| `Arcadia.apk` | **Arcadia** | L'émulateur : toutes les consoles, le menu en jeu, la traduction, la voix |
| `Arcadia-IA.apk` | **Arcadia IA** | Le « cerveau » : l'assistant qui répond à vos questions et conseille des jeux |

Arcadia marche seule. Arcadia IA ajoute l'assistant et les recommandations.

---

## Étape 1 — Récupérer les applications sur GitHub

1. Ouvrez **https://github.com/tok203372-cyber/emulateur-ia/actions**
2. Cliquez sur la dernière ligne **« Compiler Arcadia »** qui a une **coche verte**.
3. Tout en bas, dans **Artifacts**, cliquez sur **Arcadia-APK** : un fichier zip se télécharge.
4. Décompressez-le dans :
   `C:\Users\jerom\OneDrive\Documents\pour code\arcadia`
   Vous obtenez `Arcadia.apk`, `Arcadia-IA.apk` et `LISEZMOI.txt`.

> Si adb affiche plus tard « failed to stat », copiez le dossier hors de OneDrive (par exemple `C:\arcadia`) et utilisez ce chemin.

## Étape 2 — Installer sur le téléphone (adb)

1. Branchez le S25 Ultra au PC en USB (débogage USB déjà activé).
2. Dans le terminal d'Android Studio :

```
cd C:\Users\jerom\AppData\Local\Android\Sdk\platform-tools
.\adb devices
.\adb -s R5CXC3M1SPR install -r "C:\Users\jerom\OneDrive\Documents\pour code\arcadia\Arcadia.apk"
.\adb -s R5CXC3M1SPR install -r "C:\Users\jerom\OneDrive\Documents\pour code\arcadia\Arcadia-IA.apk"
```

Chaque commande doit afficher **Success**. Deux icônes apparaissent : **Arcadia** (manette) et **Arcadia IA** (étincelles).

## Étape 3 — Premier lancement d'Arcadia

1. Ouvrez **Arcadia** et autorisez les **notifications** (nécessaires pendant les parties).
2. Sur l'accueil, choisissez le **dossier de vos jeux** (vos propres copies). Arcadia trouve tout seul la console de chaque jeu.
3. Au premier lancement d'un jeu de chaque console, Arcadia télécharge l'émulateur de cette console (une seule fois, Internet nécessaire).
4. Certaines consoles ont besoin du **BIOS** de votre console : Paramètres (roue dentée) → BIOS indique lesquels.

## Étape 4 — La manette

1. Appairez la manette dans les réglages Bluetooth du téléphone.
2. C'est tout : Arcadia règle les touches automatiquement pour chaque console.
3. **Ouvrir le menu en jeu** : bouton central (Home) de la manette, ou **L3 + R3** (les deux sticks enfoncés), ou le bouton menu à l'écran.
4. Pour changer une touche : Paramètres → Manettes.

## Étape 5 — GameCube, Wii, PS2… (onglet « Plus »)

1. Dans Arcadia, onglet **Plus**.
2. GameCube / Wii : touchez **Installer Dolphin** (Play Store), puis ouvrez Dolphin **une fois** :
   - Paramètres → Entrées GameCube → Manette GameCube 1 → **Manette standard** ;
   - dans ses réglages, choisissez votre manette puis touchez **Par défaut**.
3. Revenez dans Arcadia → **Choisir le dossier des jeux** (vos fichiers `.rvz` / `.iso`).
4. Les jaquettes s'affichent : touchez un jeu, Dolphin le démarre directement.
5. PS2 : installez **ARMSX2** (bouton dans la carte PS2) et ajoutez le BIOS de votre PS2 dans ARMSX2.

## Étape 6 — Traduction et voix (déjà dans Arcadia)

1. Onglet **IA** → « Traduire vers : **Français** ».
2. « Langues des jeux » : touchez **Préparer** pour **Japonais** et **Anglais** (≈ 30 Mo chacun, en Wi-Fi). Ensuite, plus besoin d'Internet.
3. Pendant une partie, ouvrez le menu :
   - **Traduction en direct (IA)** : les textes du jeu sont remplacés par leur traduction ;
   - **Lire l'écran à voix haute** ;
   - **Commande vocale** : autorisez le micro la première fois, puis dites par exemple
     « sauvegarde », « charge la sauvegarde », « traduis l'écran », « change de jeu pour Mario Kart ».

## Étape 7 — L'assistant (Arcadia IA)

1. Ouvrez **Arcadia IA**.
2. Choisissez un modèle et touchez **Télécharger** (en Wi-Fi) :
   - **Phi-4 mini** (3,9 Go) : le plus malin, conseillé sur le S25 Ultra ;
   - **Qwen 2.5** (1,6 Go) : plus léger et plus rapide.
3. Attendez 100 %, puis touchez **Préparer** (10 à 20 secondes la première fois) et **Discuter**.
4. Depuis Arcadia :
   - onglet **IA** → **Discuter avec l'IA** ou **Que jouer ce soir ?** ;
   - appui long sur un jeu → **Présenter ce jeu (IA)** ;
   - menu en jeu → **Assistant de jeu IA** : l'assistant reçoit le texte de votre écran.
5. *Facultatif* — une IA qui **voit vraiment l'image** : créez un compte Hugging Face, acceptez la licence
   de **Gemma 3n**, téléchargez `gemma-3n-E4B-it-int4.litertlm`, puis dans Arcadia IA → **Importer un fichier**.

## Étape 8 — Changer de jeu ou de console en pleine partie

Menu en jeu → **Changer de jeu ou de console** → choisissez un jeu (ou dites « change de jeu pour Luigi's Mansion »).
La partie est sauvegardée, puis le nouveau jeu démarre avec le bon émulateur, même si c'est Dolphin.

---

## Bon à savoir

- Une petite IA sur téléphone peut se tromper sur des détails précis d'un jeu : vérifiez si c'est important.
- La traduction est automatique (qualité proche d'un traducteur hors ligne) ; certaines polices de jeux rétro sont mal lues.
- Arcadia lance Dolphin, ARMSX2… mais ne peut pas ajouter son menu à l'intérieur de ces émulateurs : pour revenir, utilisez Retour ou les applications récentes.
- Aucun jeu ni BIOS n'est fourni : utilisez uniquement vos propres copies.

## Dépannage

- **« Installez l'application Arcadia IA »** : installez `Arcadia-IA.apk` (étape 2).
- **La traduction n'affiche rien** : il faut du texte à l'écran, et la langue doit être préparée (étape 6).
- **La commande vocale ne comprend pas sans Internet** : installez la reconnaissance vocale hors ligne en français
  (Paramètres du téléphone → recherchez « reconnaissance vocale hors ligne »).
- **Dolphin s'ouvre sans lancer le jeu** : mettez Dolphin à jour depuis le Play Store, puis réessayez.
- **L'IA est lente** : prenez Qwen 2.5 et laissez « Utiliser la puce graphique » activé.
- **Journal à m'envoyer** :
  `.\adb -s R5CXC3M1SPR logcat -s Arcadia ArcadiaIA > "C:\Users\jerom\OneDrive\Documents\pour code\journal-arcadia.txt"`
