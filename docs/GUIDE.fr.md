# ❄️ Guide d'utilisation — Snow on Desktop

[English version](GUIDE.md)

## Sommaire
1. [Installer](#1-installer)
2. [Lancer](#2-lancer)
3. [Le menu ❄](#3-le-menu-)
4. [Jouer avec la neige](#4-jouer-avec-la-neige)
5. [Lancer au démarrage du Mac](#5-lancer-au-démarrage-du-mac)
6. [Quitter, mettre à jour, désinstaller](#6-quitter-mettre-à-jour-désinstaller)
7. [Problèmes fréquents](#7-problèmes-fréquents)
8. [Pour les développeurs](#8-pour-les-développeurs)

---

## 1. Installer

### Option A : télécharger l'appli (le plus simple)
1. Va dans les [Releases](https://github.com/Firmirsa/SnowingDesktop/releases/latest) et télécharge `SnowOnDesktop-x.y.z-macOS.zip`.
2. Double-clique sur le zip, puis glisse **Snow on Desktop.app** dans le dossier **Applications**.
3. **Premier lancement** : l'appli n'est pas signée par Apple (ça coûte 99 $/an), donc macOS se méfie.
   - Fais **clic droit sur l'appli → Ouvrir → Ouvrir**.
   - Si macOS bloque encore : **Réglages Système → Confidentialité et sécurité**, tout en bas, clique **Ouvrir quand même**.
   - Ou dans le Terminal :
     ```bash
     xattr -dr com.apple.quarantine "/Applications/Snow on Desktop.app"
     ```

Il suffit de faire ça une fois.

### Option B : compiler soi-même
Il faut seulement les outils développeur d'Apple (une fois) :
```bash
xcode-select --install
```
Puis :
```bash
git clone https://github.com/Firmirsa/SnowingDesktop.git
cd SnowingDesktop
make run
```
`make` télécharge et compile tout seul la bibliothèque graphique (raylib). Le premier build prend environ 30 secondes, les suivants 3 secondes.

**Configuration requise** : macOS 11 Big Sur ou plus récent, Mac Apple Silicon (M1, M2…) ou Intel.

---

## 2. Lancer

- **Appli téléchargée** : ouvre **Snow on Desktop** depuis le dossier Applications, le Launchpad ou Spotlight (`⌘ Espace` puis tape « Snow »).
- **Depuis le code** : `make run` dans le dossier du projet.

Ensuite :
- un **flocon ❄ apparaît dans la barre de menus**, en haut à droite à côté de l'heure ;
- la neige commence à tomber et s'accumule sur tes fenêtres ;
- **il n'y a pas d'icône dans le Dock** : c'est normal, tout se pilote depuis le ❄.

L'appli ne demande **aucune autorisation**. Elle lit seulement la position des fenêtres, jamais leur contenu ni leur titre.

---

## 3. Le menu ❄

Clique sur le **❄ dans la barre de menus**. Le menu est en français si ton Mac est en français, sinon en anglais.

| Menu | Ce que ça fait |
|---|---|
| **Neige légère** | Quelques flocons, ambiance calme |
| **Neige** | Chute normale (réglage par défaut) |
| **Tempête de neige** | Beaucoup de neige, ça s'accumule vite |
| **Arrêter la neige** | Plus aucun flocon ne tombe, la neige déjà posée reste. Clique à nouveau pour relancer |
| **Faire fondre la neige** | Toute la neige accumulée (et les bonhommes) fond en quelques secondes |
| **Masquer** | Cache tout (neige qui tombe et neige posée) sans quitter. Clique à nouveau pour réafficher |
| **Lancer au démarrage** | L'appli démarre toute seule quand tu allumes le Mac (macOS 13 ou plus récent) |
| **Quitter Snow on Desktop** | Ferme l'appli (`⌘Q` quand le menu est ouvert) |

Une coche ✓ indique le réglage actif.

---

## 4. Jouer avec la neige

| Action | Ce qui se passe |
|---|---|
| Laisser neiger | La neige s'empile sur le haut de chaque fenêtre, déborde sur les coins arrondis et forme une congère en bas de l'écran |
| **Déplacer une fenêtre** doucement | La neige reste dessus et voyage avec elle |
| **Secouer une fenêtre** ou l'arrêter d'un coup | La neige garde son élan et s'envole |
| **Fermer une fenêtre** | Sa neige s'effrite et tombe sur les fenêtres en dessous |
| **Passer la souris** vite dans un tas de neige | Tu la balaies, elle part en gerbe |
| **Bouger la souris** dans les flocons qui tombent | Le courant d'air les pousse |
| Mettre une fenêtre par-dessus une autre | La neige cachée derrière reste invisible et réapparaît quand tu découvres la fenêtre |
| Changer de bureau (Spaces) | La neige t'attend, elle ne tombe pas |
| Laisser tranquille un moment | Quand beaucoup de neige s'est accumulée et reste là (environ 1 min 30), un **bonhomme de neige ☃️** s'y construit en une minute : classique, Père Noël 🎅 ou cool 😎 (un par fenêtre, jusqu'à trois en bas de l'écran) |
| Secouer ou fermer la fenêtre, ou passer la souris dans un bonhomme | Il se brise |
| Les tas deviennent très pleins | De petites **avalanches** glissent par les côtés de temps en temps |

La neige **ne bloque jamais tes clics** : tu cliques à travers comme si elle n'existait pas.

💡 **Astuce pour une vidéo** : mets **Tempête de neige** 1 à 2 minutes, puis secoue une fenêtre.

---

## 5. Lancer au démarrage du Mac

Menu ❄ → **Lancer au démarrage**, une coche apparaît. Pour désactiver, clique à nouveau.

Tu peux aussi le gérer dans **Réglages Système → Général → Ouverture**.

> Disponible sur macOS 13 Ventura et plus récent. Ça marche mieux avec l'appli installée dans le dossier Applications.

---

## 6. Quitter, mettre à jour, désinstaller

**Quitter** : menu ❄ → **Quitter Snow on Desktop**.

**Mettre à jour**
- Appli téléchargée : quitte l'appli, télécharge la nouvelle version dans les Releases et remplace l'ancienne dans Applications.
- Depuis le code :
  ```bash
  git pull && make run
  ```
  Quitte d'abord l'ancienne version, sinon deux neiges se superposent.

**Désinstaller**
1. Menu ❄ → décoche **Lancer au démarrage** si c'est activé.
2. Menu ❄ → **Quitter**.
3. Mets **Snow on Desktop** (dans Applications) à la corbeille.

L'appli ne laisse aucun autre fichier sur ton Mac.

---

## 7. Problèmes fréquents

**Je ne vois pas le ❄ dans la barre de menus**
Sur les MacBook avec encoche, macOS cache les icônes quand il y en a trop : elles passent derrière l'encoche. Retire des icônes de la barre en maintenant `⌘` et en les glissant vers le bas, ou utilise une appli comme Ice ou Bartender.

**« Snow on Desktop ne peut pas être ouvert car le développeur ne peut pas être vérifié »**
Voir [Premier lancement](#option-a--télécharger-lappli-le-plus-simple) : clic droit → Ouvrir.

**Il y a deux neiges en même temps**
L'appli a été lancée deux fois. Quitte-les toutes depuis le ❄, ou dans le Terminal :
```bash
killall SnowOnDesktop
```

**De la neige flotte à côté des petites icônes de Stage Manager**
Problème connu : macOS y place des fenêtres invisibles. Ce n'est pas gênant, et c'est sur la liste à corriger.

**La neige n'apparaît que sur un écran**
Normal pour l'instant : seul l'écran principal (celui avec la barre de menus) est géré. Le multi-écran est prévu.

**`make` échoue**
- Installe les outils développeur : `xcode-select --install`.
- Regarde la fin de `build/build.log`, ou relance avec l'affichage brut : `make V=1`.
- Pour tout reconstruire à zéro : `make distclean && make`.

**Mon Mac chauffe**
Passe en **Neige légère** ou **Arrêter la neige**. Et ouvre une issue sur GitHub en indiquant ton modèle de Mac.

---

## 8. Pour les développeurs

| Commande | Effet |
|---|---|
| `make` | Compile `build/Snow on Desktop.app` |
| `make run` | Compile et lance |
| `make zip` | Crée le zip à publier dans une Release |
| `make demo` | Démo sur un faux bureau (marche aussi sur Linux) |
| `./build/snow-demo --record demo.mp4` | Enregistre la vidéo de démo (nécessite `ffmpeg`) |
| `make V=1` | Affiche les commandes brutes du compilateur |
| `make clean` | Supprime `build/` |
| `make distclean` | Supprime aussi raylib (`vendor/`) |

Organisation du code : voir la section *How it works* du [README](../README.md#-how-it-works).

**Publier une version** : crée un tag, et GitHub Actions compile l'appli et crée la Release tout seul.
```bash
git tag v0.2.0 && git push origin v0.2.0
```
