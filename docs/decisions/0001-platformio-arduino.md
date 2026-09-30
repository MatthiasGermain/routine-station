# 0001 : PlatformIO et le framework Arduino

- **Date** : 2026-09-30
- **Étape** : 0

## Contexte

Le firmware tourne sur un ESP32 DevKit V1. Il doit lire des capteurs simples,
piloter un moteur pas-à-pas, réagir à une flamme en temps réel, puis parler à un
broker MQTT en TLS. Le dépôt est aussi un projet portfolio : n'importe qui doit
pouvoir le cloner et obtenir la même compilation, sans réglage manuel.

## Décision

Le firmware est écrit en C++ avec le **framework Arduino**, et le projet est
géré par **PlatformIO**. La version de la plateforme est figée dans
`platformio.ini` (`espressif32@6.12.0`, qui embarque le cœur Arduino-ESP32
2.0.17).

## Alternatives écartées

- **Arduino IDE** : la carte, la version du cœur et les bibliothèques se règlent
  à la main dans l'interface et ne sont pas décrites dans le dépôt. Avec
  PlatformIO, tout tient dans `platformio.ini`, versionné avec le code.
- **ESP-IDF seul** (le SDK officiel d'Espressif) : plus de contrôle, mais
  beaucoup plus de code à écrire pour des capteurs et un moteur que les
  fonctions Arduino gèrent en quelques lignes. Le framework Arduino pour ESP32
  repose lui-même sur ESP-IDF et FreeRTOS : les tâches et les priorités
  nécessaires à l'alarme temps réel restent accessibles.
- **Laisser la version de la plateforme libre** : la compilation pourrait
  changer de comportement le jour où une nouvelle version sort.

## Conséquences

- Une seule commande (`pio run`) compile le projet sur n'importe quelle machine.
- Le code reste lisible par quelqu'un qui connaît Arduino, tout en donnant accès
  à FreeRTOS pour l'étape 2.
- La plateforme 6.12.0 n'est pas la plus récente (la branche 7.x existe). Monter
  de version sera un changement volontaire, fait à part et testé, pas un effet
  de bord.
