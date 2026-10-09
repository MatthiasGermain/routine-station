# 0003 : l'alarme flamme dans une tâche FreeRTOS

- **Date** : 2026-10-08
- **Étape** : 2
- **Statut** : toujours en vigueur. Depuis la
  [décision 0008](0008-arret-urgence-tactile.md), la tâche est réveillée par
  le module tactile au lieu de lire le capteur de flamme

## Contexte

Quand une flamme apparaît, la LED, le buzzer et l'arrêt du moteur doivent
réagir en quelques millisecondes, **quoi que fasse le reste du programme**.
Or, à partir de l'étape 3, `loop()` gérera le Wi-Fi et MQTT : une reconnexion
au broker en TLS peut la bloquer une ou deux secondes.

Le capteur de flamme du kit est un récepteur infrarouge nu, sans comparateur ni
sortie numérique : il ne peut que se lire en analogique, il ne peut pas
déclencher d'interruption.

## Décision

L'alarme est une **tâche FreeRTOS** dédiée (`src/alarm.cpp`) :

- priorité 10, bien au-dessus de `loop()` (priorité 1) : dès qu'elle doit
  tourner, elle interrompt `loop()`, où qu'elle en soit ;
- épinglée sur le cœur 1, comme `loop()`, loin de la pile Wi-Fi qui tourne sur
  le cœur 0 ;
- réveillée toutes les 2 ms par `vTaskDelayUntil()`, qui garde une période
  régulière ;
- surveillée par le chien de garde des tâches : si elle cesse de tourner pendant
  5 s, la carte redémarre au lieu de rester sans protection ;
- elle n'écrit jamais sur le port série, qui peut bloquer : elle envoie ses
  événements dans une file FreeRTOS, que `loop()` vide et affiche.

Le module moteur protège son état par une section critique (spinlock) : la
tâche d'alarme peut l'arrêter même au milieu d'un pas lancé par `loop()`.

## Alternatives écartées

- **Vérifier la flamme dans `loop()`** : la réaction dépendrait de la durée du
  tour de boucle, donc du réseau à partir de l'étape 3. C'est exactement ce que
  l'étape 2 veut éviter, et ce que mesure l'expérience du journal
  (`SIMULATED_STALL_MS`).
- **Interruption sur une broche** : impossible, le capteur n'a pas de sortie
  numérique. Il faudrait un comparateur, absent du kit.
- **Interruption d'un minuteur matériel** : une routine d'interruption doit être
  très courte et ne peut pas appeler le pilote de l'ADC, qui utilise des
  verrous. Elle ne pourrait que réveiller une tâche, ce qui revient à la
  solution retenue, en plus compliqué.
- **Tâche sur le cœur 0** : elle tournerait vraiment en parallèle de `loop()`,
  mais partagerait le cœur avec la pile Wi-Fi, de priorité plus haute, qui
  pourrait la retarder.

## Conséquences

- Le temps de réaction ne dépend plus de `loop()`. Il est mesuré par le
  firmware lui-même : la tâche horodate à la microseconde le premier
  échantillon au-dessus du seuil et l'activation des sorties.
- Deux tâches touchent maintenant au moteur et à l'ADC : le moteur passe par un
  spinlock, et le pilote de l'ADC de l'ESP32 a son propre verrou. Le capteur de
  flamme n'est lu que par la tâche d'alarme ; la photorésistance est lue par
  les deux (la tâche d'alarme s'en sert pour reconnaître la lumière du jour).
- La lecture toutes les 2 ms coûte un peu de temps processeur (quelques
  dizaines de microsecondes par lecture), négligeable ici.
