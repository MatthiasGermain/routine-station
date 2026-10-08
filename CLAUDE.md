# CLAUDE.md — routine-station

Mémo de contexte et de règles de travail, à relire à chaque session.

## Le projet

Projet portfolio de Matthias (jeune ingénieur logiciel, web, embarqué) : une
**station de supervision connectée**. Le repo doit être agréable à lire pour un
recruteur : on comprend le projet en 30 secondes et on peut suivre sa
construction étape par étape.

Un ESP32 lit des capteurs, pilote un moteur et envoie ses mesures à un broker
MQTT dans le cloud (HiveMQ Cloud, offre gratuite). Une section « Station » de la
page `/routine` du site de Matthias affiche les mesures en direct et permet
d'envoyer des ordres.

Le site est **dans un autre repo**, `portfolio` (Astro + TypeScript, hébergé sur
Vercel). La page `/routine` y existe déjà : c'est le tableau de bord personnel
de Matthias (planning de la journée, tâches Notion en cours). La station y
ajoute une section, elle ne remplace rien.

- **Mesures** : température (LM35), lumière (photorésistance + 10 kΩ), flamme
  (récepteur infrarouge nu à 2 pattes + 10 kΩ : pas de module, **pas de sortie
  numérique**, donc pas d'interruption matérielle possible, le seuil se fait en
  logiciel)
- **Actions** : moteur pas-à-pas 28BYJ-48 via driver ULN2003, LED, buzzer
- **Ordres depuis le web** : faire tourner / arrêter le moteur, déclencher /
  couper l'alarme, régler des seuils
- **Temps réel** : si une flamme est détectée, alarme + arrêt moteur immédiats,
  même si le Wi-Fi ou le broker sont indisponibles. **La sécurité locale ne
  dépend jamais du réseau.**
- **Sécurité** :
  - le broker a deux accès : lecture seule (utilisé par la page, visible par
    tous) et commande (utilisé uniquement par une route API Vercel côté serveur)
  - la route API vérifie que Matthias est connecté (Auth.js ou équivalent pour
    Astro, à choisir à l'étape 5 ; connexion GitHub limitée à son compte) et
    valide chaque commande (type autorisé, valeurs dans les bornes)
  - l'ESP32 valide aussi chaque commande reçue et se connecte au broker en TLS

## Contraintes

- **Matériel** : uniquement ce que Matthias possède déjà, rien à acheter. Un kit
  Arduino UNO (UNO, capteurs, moteur + ULN2003, LED, buzzer actif, LCD 1602,
  boutons, résistances, breadboards, module d'alimentation 3,3 V/5 V pour
  breadboard) et deux ESP32. Pour l'instant **un seul ESP32** ; le
  second et l'UNO sont des évolutions possibles plus tard.
- **Carte** : ESP32 DevKit V1, 30 broches (ESP32-WROOM-32), board PlatformIO
  `esp32doit-devkit-v1`. LED intégrée sur GPIO2.
- **Firmware** : C++ avec le framework Arduino, sous PlatformIO.
- **ADC** : les capteurs analogiques vont sur des broches **ADC1** (GPIO 32 à
  39). ADC2 est inutilisable quand le Wi-Fi est actif.
- **Logique 3,3 V** : les GPIO de l'ESP32 ne tolèrent pas le 5 V. À vérifier
  pour chaque capteur du kit UNO avant de proposer un câblage.
- **Secrets** : aucun mot de passe Wi-Fi, identifiant broker ou certificat
  commité. Fichier exemple commité (`include/secrets.example.h`, à partir de
  l'étape 3), vrai fichier `include/secrets.h` dans le `.gitignore`.

## Les étapes

| # | Étape | Tag |
|---|-------|-----|
| 0 | Mise en place : structure du repo, PlatformIO, README squelette, conventions | `v0.0-setup` |
| 1 | Capteurs et moteur en local, résultats dans le moniteur série | `v0.1-sensors` |
| 2 | Alarme flamme temps réel : tâche FreeRTOS haute priorité qui surveille le capteur, temps de réaction mesuré et documenté | `v0.2-alarm` |
| 3 | Connexion au broker cloud : Wi-Fi, MQTT en TLS, reconnexion automatique, publication des mesures, réception des commandes | `v0.3-mqtt` |
| 4 | Section « Station » de la page `/routine` : affichage en direct (dans le repo `portfolio`) | `v0.4-live-page` |
| 5 | Commandes depuis le web : route API protégée par login (dans le repo `portfolio`) | `v0.5-commands` |
| 6 | Finition : README complet, schéma de câblage, GIF/vidéo de démo, bilan | `v1.0` |

L'étape en cours se lit dans la feuille de route du [README](README.md#roadmap)
et dans le dernier fichier de `docs/journal/`.

Les étapes 4 et 5 se codent dans `portfolio`, mais leur tag est posé **ici**,
sur le commit qui met à jour la feuille de route, le journal de l'étape et, si
besoin, `docs/protocol.md`. Le journal renvoie vers les commits du site.

## Organisation du repo

```
README.md / README.fr.md   README bilingue (anglais principal, français en miroir)
platformio.ini             configuration de la carte et de la chaîne de compilation
include/pins.h             toutes les broches au même endroit
src/                       un module par responsabilité, à plat :
                           sensors, motor, alarm, capture, network (+ main.cpp)
docs/protocol.md           contrat ESP32 <-> web (topics MQTT, JSON), référence
                           commune avec le repo du site
docs/wiring.md             câblage broche par broche, points d'attention
docs/journal/              un fichier court par étape
docs/decisions/            une note courte par choix important
docs/assets/               photos, captures, GIF
```

## Conventions

- **Langues** :
  - code en anglais : identifiants, commentaires, messages série
  - commits, tags et issues en anglais
  - `README.md` en anglais, `README.fr.md` en français, à garder synchronisés
  - `docs/` et ce fichier en français
- **Code** : simple, lisible et commenté. La clarté avant l'astuce. Un module
  par responsabilité (capteurs, moteur, alarme, réseau). Toute broche passe par
  `include/pins.h`, aucune valeur de GPIO en dur ailleurs.
- **Journal** (`docs/journal/NN-titre.md`) : objectif, ce qui a été fait,
  difficultés rencontrées, résultat, captures.
- **Décisions** (`docs/decisions/NNNN-titre.md`) : contexte → décision →
  alternatives écartées → conséquences. Gabarit dans `0000-modele.md`.
- **Git** : commits conventionnels (`feat:`, `fix:`, `docs:`, `chore:`…), un tag
  par étape terminée, une issue GitHub par étape.

## Règles de travail

- **Une étape à la fois.** Ne rien coder qui appartient à une étape suivante.
- **Avant de coder une étape** : expliquer simplement ce qu'on va faire et
  pourquoi, proposer le plan, attendre la validation.
- **Câblage** : donner le branchement exact broche par broche et attendre la
  confirmation que c'est câblé avant d'écrire le code qui l'utilise.
- **À la fin de chaque étape** :
  - mettre à jour la feuille de route dans les deux README et créer le fichier
    de journal de l'étape
  - proposer les messages de commit et le tag
  - expliquer comment vérifier que tout marche
- Si quelque chose est ambigu ou risqué, poser la question au lieu de supposer.
- **Git** : Claude ne lance jamais `git commit` ni `git push`. Il signale quand
  un commit est pertinent, dit ce qu'il regrouperait et propose le message.
  C'est Matthias qui commite, tague et pousse. Les commandes en lecture
  (`status`, `diff`, `log`) sont permises.

## Commandes utiles

PlatformIO est installé par l'extension VSCode et n'est pas dans le `PATH` :

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run                 # compiler
& $pio run -t upload       # téléverser (carte branchée en USB)
& $pio device monitor      # moniteur série, 115200 bauds
& $pio device monitor -f log2file   # idem, enregistré dans logs/ (ignoré par git)
```

Touches dans le moniteur série (firmware depuis l'étape 2) :

- `c` : enregistre 5 s de mesures brutes (flamme, lumière, état de l'alarme,
  toutes les 2 ms) puis les affiche en CSV. Lancer le moniteur avec
  `-f log2file` pour que Claude puisse lire le fichier dans `logs/`.
- `s` : active ou coupe l'expérience « `loop()` bloquée 500 ms à chaque tour ».
