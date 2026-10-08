# Protocole ESP32 ↔ web

Ce document est le contrat entre le firmware de la station et la section
« Station » de la page `/routine` du site (repo `portfolio`) : topics MQTT,
format JSON des mesures et des commandes.
C'est la référence commune aux deux dépôts. Si le firmware et le site ne sont
pas d'accord, c'est ce fichier qui a raison.

## Broker et accès

Broker : **EMQX Serverless**, offre gratuite, région Europe (Francfort)
([décision 0005](decisions/0005-broker-emqx.md)).

| Accès | Port | Utilisé par |
|-------|------|-------------|
| MQTT sur TLS | 8883 | l'ESP32, la route API du site |
| WebSocket sur TLS (`wss://<adresse>:8084/mqtt`) | 8084 | la page, dans le navigateur |

- **Adresse du broker** : dans `include/secrets.h` pour la station, dans les
  variables d'environnement Vercel pour le site. Elle n'est pas un secret au
  sens strict (la page l'exposera dans le navigateur), mais elle n'est pas
  publiée dans le dépôt pour ne pas inviter les tentatives de connexion.
- **TLS obligatoire**, et le certificat du broker est **vérifié** : l'ESP32
  connaît l'autorité racine qui l'a signé, DigiCert Global Root G2, valable
  jusqu'en 2038. Jamais de connexion qui accepterait n'importe quel serveur.

### Les trois utilisateurs

| Utilisateur | Utilisé par | Peut publier | Peut s'abonner | Créé à l'étape |
|-------------|-------------|--------------|----------------|----------------|
| `station` | l'ESP32 | `measurements`, `events`, `status`, `replies` | `commands` | 3 |
| `web-viewer` | la page, dans le navigateur | rien | `measurements`, `events`, `status`, `replies` | 4 |
| `web-command` | la route API, côté serveur | `commands` | rien | 5 |

(Tous les topics sont sous le préfixe `routine/station/`, voir plus bas.)

- Le mot de passe de `web-viewer` finit forcément dans le code JavaScript de la
  page, donc il faut le considérer comme **public** : c'est pour ça que cet
  utilisateur ne peut rien publier.
- Le mot de passe de `web-command` ne quitte jamais le serveur Vercel.
- Les droits sont des règles d'autorisation EMQX, en liste blanche : une règle
  « tous les utilisateurs : `#`, publication et abonnement : refusé », puis une
  règle « autorisé » par utilisateur et par topic du tableau.

## Topics

Tous les topics commencent par `routine/station/`.

| Topic | Sens | QoS | Retenu | Quand |
|-------|------|-----|--------|-------|
| `routine/station/measurements` | station → web | 0 | oui | toutes les 5 s |
| `routine/station/events` | station → web | 0 | non | aussitôt qu'un événement se produit |
| `routine/station/status` | station → web | 1 | oui | à la connexion, et par le broker à la déconnexion |
| `routine/station/commands` | web → station | 1 | non | à chaque ordre |
| `routine/station/replies` | station → web | 0 | non | en réponse à chaque commande |

- **QoS 0** : le message est envoyé une fois, sans accusé de réception. Une
  mesure perdue est remplacée 5 s plus tard.
- **QoS 1** : le broker garantit qu'une commande arrive au moins une fois. Elle
  peut arriver deux fois : les commandes sont donc sans effet si on les répète
  (démarrer un moteur déjà démarré ne change rien).
- **Retenu** : le broker garde le dernier message du topic et l'envoie à chaque
  nouvel abonné. Une page qui s'ouvre affiche tout de suite la dernière mesure
  et l'état de la station, sans attendre.

## Mesures (station → web)

Topic `routine/station/measurements`, toutes les 5 s, retenu.

```json
{
  "time": "2026-10-08T19:30:05Z",
  "uptime_s": 1234,
  "temperature_c": 23.6,
  "light_pct": 88,
  "flame_rise_mv": 5,
  "alarm": "off",
  "motor": "stopped",
  "rssi_dbm": -61
}
```

| Champ | Type | Sens |
|-------|------|------|
| `time` | chaîne ISO 8601 UTC, ou `null` | heure de la mesure, obtenue par NTP ; `null` tant que l'heure n'est pas connue |
| `uptime_s` | entier | secondes depuis le démarrage de la station |
| `temperature_c` | nombre, 1 décimale | température du LM35 |
| `light_pct` | entier 0 à 100 | lumière visible, relative (pas des lux) |
| `flame_rise_mv` | entier | hausse d'infrarouge au-dessus du niveau de référence (voir [décision 0004](decisions/0004-flamme-ou-lumiere-du-jour.md)) |
| `alarm` | `"off"`, `"on"`, `"silenced"`, `"test"` | état de l'alarme (voir plus bas) |
| `motor` | `"stopped"`, `"forward"`, `"backward"`, `"locked"` | état du moteur ; `locked` pendant une alarme |
| `rssi_dbm` | entier | force du signal Wi-Fi, utile pour diagnostiquer |

États de l'alarme :

- `off` : pas d'alarme ;
- `on` : flamme détectée, LED et buzzer allumés, moteur bloqué ;
- `silenced` : flamme toujours là, buzzer coupé par une commande, LED allumée et
  moteur toujours bloqué ;
- `test` : test lancé par une commande, LED et buzzer pendant 3 s, moteur
  bloqué.

## Événements (station → web)

Topic `routine/station/events`, envoyés aussitôt.

```json
{ "time": "2026-10-08T19:31:12Z", "type": "alarm_raised", "flame_rise_mv": 423, "reaction_ms": 18.0 }
{ "time": "2026-10-08T19:31:20Z", "type": "alarm_cleared" }
```

| `type` | Champs en plus |
|--------|----------------|
| `alarm_raised` | `flame_rise_mv`, `reaction_ms` (temps de réaction de la tâche d'alarme) |
| `alarm_cleared` | aucun |

L'alarme réagit sur la carte sans attendre le réseau : l'événement part dès que
la connexion le permet, il peut donc arriver après coup.

## Commandes (web → station)

Topic `routine/station/commands`, QoS 1.

```json
{ "id": "k3f9a2", "type": "motor", "action": "start", "direction": "forward" }
{ "id": "k3f9a3", "type": "motor", "action": "stop" }
{ "id": "k3f9a4", "type": "alarm", "action": "test" }
{ "id": "k3f9a5", "type": "alarm", "action": "silence" }
```

| `type` | `action` | Autres champs | Effet |
|--------|----------|---------------|-------|
| `motor` | `start` | `direction` : `"forward"` ou `"backward"` | le moteur tourne en continu jusqu'à `stop` |
| `motor` | `stop` | aucun | le moteur s'arrête, bobines coupées |
| `alarm` | `test` | aucun | LED, buzzer et moteur bloqué pendant 3 s |
| `alarm` | `silence` | aucun | coupe le buzzer d'une alarme en cours |

Une commande envoyée pendant que la station est hors ligne est **perdue** : la
station se connecte sans session persistante, et le broker ne lui garde rien.
C'est voulu : un ordre ne doit pas s'exécuter des minutes plus tard, quand plus
personne ne le surveille.

**Validation par l'ESP32**, même si la route API a déjà validé : tout ce qui
n'est pas prévu est refusé.

- message de 256 octets au plus, JSON valide ;
- `id` obligatoire : chaîne de 1 à 32 caractères, renvoyée dans la réponse ;
- `type`, `action` et `direction` uniquement parmi les valeurs du tableau ;
- aucun champ en plus de ceux prévus pour la commande.

**Règles de sécurité**, appliquées par l'ESP32 :

- `motor start` est refusé pendant une alarme ou un test ;
- `alarm silence` ne coupe que le buzzer : la LED reste allumée et le moteur
  reste bloqué tant que la flamme est là ;
- `alarm test` est refusé pendant une vraie alarme ;
- aucune commande ne peut éteindre une alarme due à une flamme : elle s'arrête
  seule, 3 s après la disparition de la flamme.

## Réponses (station → web)

Topic `routine/station/replies`, une réponse par commande reçue.

```json
{ "id": "k3f9a2", "ok": true }
{ "id": "k3f9a3", "ok": false, "error": "alarm_active" }
```

| `error` | Cause |
|---------|-------|
| `too_long` | message de plus de 256 octets |
| `invalid_json` | JSON illisible |
| `invalid_id` | `id` absent ou invalide (la réponse a alors `"id": null`) |
| `invalid_command` | `type`, `action`, `direction` inconnus, ou champ en trop |
| `alarm_active` | refusé pendant une alarme ou un test |
| `nothing_to_silence` | `silence` alors qu'aucune alarme ne sonne |

## État de la station

Topic `routine/station/status`, retenu, QoS 1.

```json
{ "online": true }
{ "online": false }
```

- À chaque connexion, la station publie `{"online": true}`.
- En se connectant, elle confie aussi au broker un **testament** (*Last Will*) :
  `{"online": false}` sur ce même topic. Si la station disparaît sans prévenir
  (coupure de courant, perte du Wi-Fi), le broker le publie lui-même environ
  45 secondes plus tard, faute de nouvelles d'elle (1,5 fois le *keep-alive*
  de 30 s).
- Une page qui s'ouvre reçoit donc tout de suite le dernier état connu.
