# Câblage

Branchement broche par broche de la station, avec les points d'attention
(tensions, résistances). Les numéros de GPIO de ce document et ceux de
[`include/pins.h`](../include/pins.h) doivent toujours être identiques.

> Rempli à partir de l'étape 1 (capteurs et moteur en local).

## Carte

ESP32 DevKit V1, 30 broches (module ESP32-WROOM-32).

## Règles valables pour tout le montage

- **Logique en 3,3 V.** Les GPIO de l'ESP32 ne tolèrent pas le 5 V : aucun
  signal à 5 V ne doit arriver directement sur une broche.
- **Capteurs analogiques sur ADC1** (GPIO 32 à 39). L'ADC2 est inutilisable
  quand le Wi-Fi est actif.
- **GPIO 34 à 39 : entrées uniquement**, sans résistance de tirage interne.

## Étape 0

Aucun câblage. Le programme de test utilise la LED bleue soudée sur la carte.

| Élément | GPIO | Remarque |
|---------|------|----------|
| LED intégrée | 2 | Active à l'état haut |

## Étape 1 et suivantes

_À venir : capteurs (LM35, photorésistance, flamme), moteur 28BYJ-48 + ULN2003,
LED, buzzer._
