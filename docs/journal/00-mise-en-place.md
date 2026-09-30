# Étape 0 : mise en place

- **Date** : 2026-09-30
- **Tag** : `v0.0-setup`

## Objectif

Poser les fondations avant de brancher le moindre capteur : un dépôt lisible,
des conventions écrites, et une chaîne de compilation qui fonctionne de bout en
bout (compiler, téléverser, lire le port série).

## Ce qui a été fait

- Structure du dépôt : `src/` et `include/` pour le firmware, `docs/` pour le
  protocole, le câblage, le journal et les décisions.
- Projet PlatformIO pour l'ESP32 DevKit V1 (`esp32doit-devkit-v1`), framework
  Arduino, version de la plateforme figée
  ([décision 0001](../decisions/0001-platformio-arduino.md)).
- Programme de test : la LED intégrée (GPIO2) clignote toutes les 500 ms et son
  état s'affiche sur le port série à 115200 bauds.
- `include/pins.h` : le fichier unique où sont déclarées les broches.
- README squelette en anglais et en français, avec le schéma d'architecture
  cible et la feuille de route.
- `.gitignore` : sorties de compilation, fichiers générés par VS Code, et le
  futur fichier de secrets `include/secrets.h`.
- Conventions de travail écrites dans `CLAUDE.md`.

## Difficultés rencontrées

- **`pio` introuvable dans le terminal.** PlatformIO est installé par
  l'extension VS Code, qui ne l'ajoute pas au `PATH`. Il faut passer par le
  terminal PlatformIO de VS Code, ou appeler
  `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` directement.
- **Quelle version de la plateforme ?** La branche 7.x d'`espressif32` est
  sortie, mais la 6.12.0 était déjà installée et éprouvée. Elle est figée dans
  `platformio.ini` ; la montée de version se fera à part.

## Résultat

Compilation réussie :

```
PLATFORM: Espressif 32 (6.12.0) > DOIT ESP32 DEVKIT V1
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
RAM:   [=         ]   6.6% (used 21464 bytes from 327680 bytes)
Flash: [==        ]  20.5% (used 269169 bytes from 1310720 bytes)
========================= [SUCCESS] Took 29.59 seconds =========================
```

Vérification sur la carte :

- [x] téléversement réussi
- [x] la LED bleue clignote, une demi-seconde allumée, une demi-seconde éteinte
- [x] le moniteur série affiche `LED on` / `LED off` en alternance

## Captures

_À ajouter dans `docs/assets/` : une photo de la carte, LED allumée._
