# 0006 : PubSubClient comme client MQTT

- **Date** : 2026-10-08
- **Étape** : 3

## Contexte

L'ESP32 doit se connecter au broker en MQTT sur TLS, publier ses mesures,
recevoir les commandes, et se reconnecter seul après une coupure du Wi-Fi ou du
broker. Le code doit rester lisible : la reconnexion fait partie de ce que le
projet veut montrer.

## Décision

**PubSubClient** (version 2.8, figée dans `platformio.ini`), au-dessus de
`WiFiClientSecure` pour le TLS. La reconnexion est écrite dans
`src/network.cpp` : tentatives de plus en plus espacées (2 s, 4 s, 8 s...
jusqu'à 60 s), et des limites de temps sur chaque phase de la connexion.

## Alternatives écartées

- **esp-mqtt**, le client MQTT intégré à ESP-IDF : il tourne dans sa propre
  tâche et gère seul la reconnexion, donc `loop()` n'est jamais bloquée. Mais
  son API en C, à base d'événements, est plus verbeuse, et la reconnexion
  devient invisible dans le code du projet.
- **AsyncMqttClient** : asynchrone aussi, mais son support du TLS sur ESP32 est
  incomplet et la bibliothèque n'est plus maintenue.

## Conséquences

- **La connexion au broker bloque `loop()`** : quelques secondes, moins de 30
  au pire (limites de 12 s pour ouvrir la connexion, 10 s pour le TLS, 5 s
  pour la réponse du broker). Par défaut, ces attentes cumulées dépassaient 2
  minutes. C'est exactement le cas que la tâche d'alarme de l'étape 2 sait
  encaisser ; le moteur a été placé dans sa propre tâche pour la même raison
  ([décision 0007](0007-moteur-tache-freertos.md)).
- PubSubClient ne publie qu'en QoS 0 : les mesures et réponses peuvent se
  perdre, ce que le protocole accepte (une mesure perdue est remplacée 5 s plus
  tard). Il s'abonne en QoS 1 pour les commandes, et le testament (*Last Will*)
  part en QoS 1.
- Son tampon par défaut (256 octets, topic compris) est trop petit pour nos
  messages : il est porté à 512 octets.
