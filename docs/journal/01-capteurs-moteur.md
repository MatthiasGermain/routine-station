# Étape 1 : capteurs et moteur en local

- **Date** : 2026-10-08
- **Tag** : `v0.1-sensors`

## Objectif

Faire fonctionner chaque composant seul, sans réseau : lire la température, la
lumière et le capteur de flamme, faire tourner le moteur, et tout afficher dans
le moniteur série. Valider le matériel et le câblage avant d'y ajouter l'alarme
puis le Wi-Fi : si quelque chose casse plus tard, ce ne sera pas le montage.

## Ce qui a été fait

- **Câblage complet** sur breadboard, détaillé dans
  [`docs/wiring.md`](../wiring.md) : 5 V pris sur VIN, 3,3 V en fils directs
  (pas de rail, pour ne jamais brancher un capteur sur le 5 V par erreur),
  capteurs sur l'ADC1 (GPIO 34, 35 et 32), moteur sur GPIO 19, 18, 17 et 16.
- **`src/sensors`** : chaque mesure est la moyenne de 16 échantillons, convertie
  en millivolts avec la calibration d'usine de la puce
  (`analogReadMilliVolts()`). Le LM35 est lu avec une atténuation de 0 dB : son
  signal est faible (250 mV à 25 °C), et c'est dans cette plage que l'ADC est le
  plus précis.
- **`src/motor`** : un pilote en demi-pas écrit pour le projet, qui ne bloque
  jamais le programme ([décision 0002](../decisions/0002-pilote-moteur-maison.md)).
- **`src/main.cpp`** : une boucle sans `delay()`. Le moteur fait un tour, une
  pause, un tour dans l'autre sens ; une ligne de mesures s'affiche chaque
  seconde.

## Difficultés rencontrées

- **Le capteur de flamme n'est pas un module.** Le kit fournit un récepteur
  infrarouge nu, en forme de LED noire à 2 pattes, sans comparateur ni sortie
  numérique. Il est monté en pont diviseur avec une résistance de 10 kΩ et lu en
  analogique. Conséquence pour l'étape 2 : pas d'interruption matérielle
  possible, le seuil de détection sera logiciel.
- **Les rails de la grande breadboard sont coupés au milieu.** Sans les deux
  ponts ajoutés, la moitié basse du montage n'aurait été ni alimentée ni reliée
  à la masse.
- **Le moteur vibrait sans tourner.** Une première hypothèse, deux fils croisés
  dans une paire, s'est révélée fausse. Le bon indice est venu des LED A à D de
  la carte ULN2003 : pendant les pauses, le programme met les 4 entrées à 0,
  pourtant la LED C restait allumée. Le fil de IN3 était branché sur D5, juste à
  côté de TX2. D5 n'étant pas pilotée par le programme, elle restait à l'état
  haut : une bobine toujours alimentée, une autre jamais, et le moteur ne
  pouvait que vibrer. Le diagnostic par les LED est maintenant noté dans
  `docs/wiring.md`.

## Résultat

Compilation réussie :

```
PLATFORM: Espressif 32 (6.12.0) > DOIT ESP32 DEVKIT V1
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
RAM:   [=         ]   6.6% (used 21664 bytes from 327680 bytes)
Flash: [==        ]  21.2% (used 278125 bytes from 1310720 bytes)
```

Vérification sur la carte :

- [x] le moteur fait un tour en 8 secondes environ, marque une pause, puis fait
  un tour dans l'autre sens ; les LED A à D sont éteintes pendant la pause
- [x] les trois mesures affichées sont cohérentes

Les valeurs du capteur de flamme (au repos, devant une télécommande infrarouge,
devant un briquet) seront relevées à l'étape 2 : ce sont elles qui fixeront le
seuil de l'alarme.

## Captures

_À ajouter dans `docs/assets/` : une photo du montage vue de dessus, un extrait
du moniteur série, un GIF du moteur qui tourne._
