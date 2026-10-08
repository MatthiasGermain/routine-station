# 0007 : le moteur dans sa propre tâche FreeRTOS

- **Date** : 2026-10-08
- **Étape** : 3

## Contexte

Jusqu'à l'étape 2, `loop()` faisait avancer le moteur d'un demi-pas toutes les
2 ms ([décision 0002](0002-pilote-moteur-maison.md)). À l'étape 3, `loop()`
gère aussi la connexion au broker. Sur le Wi-Fi de la station, faible
(−74 dBm), les reconnexions sont fréquentes, et chacune bloque `loop()` de
quelques secondes à près de 30 s. Pendant ce temps, le moteur se figeait,
bobines alimentées.

## Décision

Le moteur avance dans **sa propre tâche FreeRTOS**, réveillée toutes les 2 ms
par `vTaskDelayUntil()`, de priorité 5 : au-dessus de `loop()` (1), en dessous
de l'alarme (10), sur le cœur 1 avec les deux. `loop()` ne fait plus que
démarrer et arrêter le moteur sur commande.

## Alternatives écartées

- **Le réseau dans sa propre tâche** plutôt que le moteur : PubSubClient n'est
  pas prévu pour être appelé depuis plusieurs tâches, il aurait fallu faire
  passer chaque publication par une file. Beaucoup plus de changements pour le
  même résultat.
- **esp-mqtt**, le client MQTT d'ESP-IDF qui tourne dans sa propre tâche : voir
  la [décision 0006](0006-pubsubclient.md).
- **Interruption d'un minuteur matériel** pour chaque pas : possible, mais une
  routine d'interruption impose des contraintes (pas de verrou bloquant, code
  en mémoire rapide) qu'une tâche n'a pas, pour aucun gain visible à 2 ms.

## Conséquences

- Le moteur tourne régulièrement, même pendant une reconnexion ou l'expérience
  de la `loop()` bloquée (touche `s`).
- Trois tâches se partagent maintenant le cœur 1, par ordre de priorité :
  l'alarme, le moteur, `loop()`. L'état du moteur était déjà protégé par un
  spinlock depuis l'étape 2 : rien à changer de ce côté.
- La période des pas suit le tic de FreeRTOS (1 ms) : 2 ms exactement, au lieu
  de « au moins 2 ms » quand `loop()` prenait du retard.
